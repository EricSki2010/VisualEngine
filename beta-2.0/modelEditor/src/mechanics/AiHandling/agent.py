"""
Gemini tool-calling sidecar for modelEditor.

Protocol: JSONL over stdin/stdout. One JSON object per line.

Host -> sidecar:
    {"type": "prompt", "text": "..."}
    {"type": "tool_result", "call_id": "...", "name": "...", "ok": true, "result": <any>}
    {"type": "tool_result", "call_id": "...", "name": "...", "ok": false, "error": "..."}
    {"type": "scene_change", "scene": "3dModeler" | "vectorMesh" | ...}
    {"type": "shutdown"}

Sidecar -> host:
    {"type": "ready"}
    {"type": "tool_call", "call_id": "...", "name": "...", "args": {...}}
    {"type": "final", "text": "..."}
    {"type": "log", "text": "..."}
    {"type": "error", "text": "..."}

Token-economy design:
  - We maintain `current_scene` and only send the subset of tool declarations
    relevant to that scene on each generate_content call. That means the tool
    schema overhead (~40-60 tokens per tool) only covers tools the model could
    reasonably use in the current context.
  - History is maintained manually rather than via client.chats.create(...) so
    tools can change between requests (chats bind tools at creation time).
  - scene_change events are processed wherever we read from stdin. The host
    emits scene_change *before* any tool_result whose tool changed the scene,
    so the next generate_content call uses the right tool set.
"""

import json
import os
import sys
import time
import traceback
import uuid

try:
    from google import genai
    from google.genai import types
except ImportError:
    sys.stdout.write(json.dumps({
        "type": "error",
        "text": "google-genai not installed. Run: pip install google-genai"
    }) + "\n")
    sys.stdout.flush()
    sys.exit(1)


MODEL = "gemini-2.5-flash"

# USD per 1M tokens, keyed by model. Cached-content tokens bill at ~25% of
# regular input tokens on Gemini 2.5 and are reported separately in usage.
MODEL_PRICING = {
    "gemini-2.5-pro":        {"input": 1.25, "output": 10.00, "cached": 0.3125},
    "gemini-2.5-flash":      {"input": 0.30, "output":  2.50, "cached": 0.075},
    "gemini-2.5-flash-lite": {"input": 0.10, "output":  0.40, "cached": 0.025},
}


def _compute_cost(prompt_tokens, output_tokens, cached_tokens):
    pricing = MODEL_PRICING.get(MODEL, MODEL_PRICING["gemini-2.5-flash"])
    non_cached_input = max(0, prompt_tokens - cached_tokens)
    return (
        non_cached_input * pricing["input"]  / 1_000_000 +
        cached_tokens    * pricing["cached"] / 1_000_000 +
        output_tokens    * pricing["output"] / 1_000_000
    )

SYSTEM_BASE = """You are an assistant embedded in a 3D model editor.
You manipulate the editor by calling tools. Be concise. When done, respond
with a short confirmation."""

# Scene-specific instructions appended after SYSTEM_BASE. We only send the
# section relevant to the active scene, trimming context for every request.
SYSTEM_BY_SCENE = {
    "3dModeler": """Scene: 3dModeler.

Palette tools (sp, gp, ps) edit color slots 0..15 with r/g/b floats 0..1.
To build geometry in a slot, call es(slot=N). That switches the editor to
vectorMesh for that slot and your next tool set becomes the vmesh tools.
If a user prompt needs a slot but doesn't name one, ask which.""",

    "vectorMesh": """Scene: vectorMesh. You are already inside a slot;
do NOT ask for a slot number.

Shapes live in a 1x1x1 box at origin. Base y=-0.5, top y=+0.5, sides x,z=+-0.5.

For "create <shape>" prompts, use bd (one-shot build). It clears the slot,
places all vertices, adds all triangles, orients normals outward, and saves.
Stays in vectorMesh so the user can inspect; only fn when the user says so.

IMPORTANT: You CAN build complex organic shapes. Do NOT refuse. Plan out
the geometry and produce it. Nothing limits you to primitives:
  - Flower: vertical stem (stack ~6-10 vertices along y), a few radial
    petals around an upper center (5+ petals = 10+ triangles), leaves as
    flat triangle fans. Easily 30+ vertices, 40+ triangles — just make them.
  - Tree, animal, vehicle — same deal, decompose into stems/slabs/cones.
  - After bd, call pv(magnitude=0.02) for organic jitter if desired.

Example — pyramid (simple, for reference):
  bd(
    vertices=[[-0.5,-0.5,-0.5],[0.5,-0.5,-0.5],[0.5,-0.5,0.5],
              [-0.5,-0.5,0.5],[0,0.5,0]],
    triangles=[{a:0,b:1,c:2},{a:0,b:2,c:3},
               {a:0,b:4,c:1},{a:1,b:4,c:2},
               {a:2,b:4,c:3},{a:3,b:4,c:0}]
  )

Pyramid = 5 verts + 6 triangles. Cube = 8 verts + 12 triangles. Use as
many as the shape needs — large builds are fine.

For fine edits to an existing mesh, use the primitives instead:
  av (batch vertices) -> at (batch triangles) -> af -> fn.""",

    "menu":        "Scene: menu. No editing tools available.",
    "createScene": "Scene: createScene. No editing tools available.",
}


# ---- Tool declarations ------------------------------------------------------

TOOLS = [
    {
        "name": "get_current_scene",
        "description": "Active scene name.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "set_palette_color",
        "description": "Set palette slot to RGB. slot=0..15, r/g/b=0..1.",
        "parameters": {
            "type": "object",
            "properties": {
                "slot": {"type": "integer"},
                "r": {"type": "number"},
                "g": {"type": "number"},
                "b": {"type": "number"},
            },
            "required": ["slot", "r", "g", "b"],
        },
    },
    {
        "name": "get_palette",
        "description": "All 16 palette colors as [[r,g,b],...].",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "select_color_slot",
        "description": "Select active paint slot (0..15).",
        "parameters": {
            "type": "object",
            "properties": {"slot": {"type": "integer"}},
            "required": ["slot"],
        },
    },
    {
        "name": "edit_slot",
        "description": "Open vectorMesh editor for slot; scene transitions.",
        "parameters": {
            "type": "object",
            "properties": {"slot": {"type": "integer"}},
            "required": ["slot"],
        },
    },
    {
        "name": "vmesh_add_vertices",
        "description": "Batch-add vertices. vertices=[[x,y,z],...]. Returns {indices:[...]}.",
        "parameters": {
            "type": "object",
            "properties": {
                "vertices": {
                    "type": "array",
                    "items": {
                        "type": "array",
                        "items": {"type": "number"},
                    },
                },
            },
            "required": ["vertices"],
        },
    },
    {
        "name": "vmesh_add_triangles",
        "description": "Batch-add triangles using existing vertex indices. triangles=[{a,b,c,flipped?},...]. Returns {indices:[...]}.",
        "parameters": {
            "type": "object",
            "properties": {
                "triangles": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "a": {"type": "integer"},
                            "b": {"type": "integer"},
                            "c": {"type": "integer"},
                            "flipped": {"type": "boolean"},
                        },
                        "required": ["a", "b", "c"],
                    },
                },
            },
            "required": ["triangles"],
        },
    },
    {
        "name": "vmesh_flip_triangle",
        "description": "Flip triangle normal.",
        "parameters": {
            "type": "object",
            "properties": {"index": {"type": "integer"}},
            "required": ["index"],
        },
    },
    {
        "name": "vmesh_delete_triangles",
        "description": "Batch-delete triangles by index. indices: [i, i, ...].",
        "parameters": {
            "type": "object",
            "properties": {"indices": {"type": "array", "items": {"type": "integer"}}},
            "required": ["indices"],
        },
    },
    {
        "name": "vmesh_delete_vertices",
        "description": "Batch-delete vertices by index. Any triangle referencing a deleted vertex is also removed, and remaining vertex indices are compacted.",
        "parameters": {
            "type": "object",
            "properties": {"indices": {"type": "array", "items": {"type": "integer"}}},
            "required": ["indices"],
        },
    },
    {
        "name": "vmesh_subdivide_triangles",
        "description": "Split each target triangle into 4 by connecting edge midpoints. Reuses existing vertices that already sit on a midpoint so the mesh stays watertight. Returns {subdivided: N}.",
        "parameters": {
            "type": "object",
            "properties": {"indices": {"type": "array", "items": {"type": "integer"}}},
            "required": ["indices"],
        },
    },
    {
        "name": "vmesh_scale_from",
        "description": "Scale vertices around an origin point. Each target vertex moves to origin + (vertex - origin) * factor. factor > 1 grows, factor < 1 shrinks, negative mirrors. Pass indices to scale only those vertices, omit to scale all. Returns {scaled: N}.",
        "parameters": {
            "type": "object",
            "properties": {
                "origin": {"type": "array", "items": {"type": "number"}},
                "factor": {"type": "number"},
                "indices": {"type": "array", "items": {"type": "integer"}},
            },
            "required": ["origin", "factor"],
        },
    },
    {
        "name": "vmesh_perturb_vertices",
        "description": "Displace vertices by a random unit direction times magnitude. Pass indices to target specific vertices, omit to perturb all. Good for adding 'bumpiness' after subdividing. magnitude default 0.05.",
        "parameters": {
            "type": "object",
            "properties": {
                "indices": {"type": "array", "items": {"type": "integer"}},
                "magnitude": {"type": "number"},
            },
        },
    },
    {
        "name": "vmesh_list_vertices",
        "description": "[{index,x,y,z},...]",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "vmesh_list_triangles",
        "description": "[{index,a,b,c,flipped,normal:[x,y,z]},...]. Check normals to decide flips.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "vmesh_save",
        "description": "Save mesh to disk.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "vmesh_clear",
        "description": "Clear all geometry.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "finish_vmesh_edit",
        "description": "Save and return to 3dModeler.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "vmesh_auto_fix_normals",
        "description": "Orient all triangle normals outward in one pass. Handles multiple disjoint objects. Returns {airtight: true/false}.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "get_camera_position",
        "description": "Camera world position and orientation: {x,y,z,yaw,pitch}. yaw/pitch in degrees.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "get_camera_forward",
        "description": "Unit vector the camera is looking along: {x,y,z}.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "list_blocks",
        "description": "Return every placed block: [{x,y,z,mesh}, ...]. Use this when the user says 'all blocks' / 'the scene' / 'every cube'.",
        "parameters": {"type": "object", "properties": {}},
    },
    {
        "name": "get_aimed_block",
        "description": "Returns the placed block and face the camera is currently pointing at, via ray-AABB intersection. Response: {hit: true, x,y,z, face (0..5), distance} or {hit: false}. Use this first when the user says 'the block I'm looking at' / 'the face I'm facing' / 'this face'.",
        "parameters": {
            "type": "object",
            "properties": {"max_distance": {"type": "number"}},
        },
    },
    {
        "name": "vmesh_build",
        "description": "One-shot shape creation: clears the slot, places all vertices, adds all triangles, orients normals outward, saves. Stays in vectorMesh so the user can review. Returns {airtight: true/false}.",
        "parameters": {
            "type": "object",
            "properties": {
                "vertices": {
                    "type": "array",
                    "items": {"type": "array", "items": {"type": "number"}},
                },
                "triangles": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "a": {"type": "integer"},
                            "b": {"type": "integer"},
                            "c": {"type": "integer"},
                            "flipped": {"type": "boolean"},
                        },
                        "required": ["a", "b", "c"],
                    },
                },
            },
            "required": ["vertices", "triangles"],
        },
    },
]

TOOLS_BY_NAME = {t["name"]: t for t in TOOLS}

# Short aliases used only on the wire to Gemini. The model sees and emits the
# short names; we translate back to the real name before dispatching to C++.
# Keeping this layer in Python means the editor code never has to learn the
# aliases, and changing an alias is a one-line edit here.
SHORT_NAMES = {
    "get_current_scene":       "gs",
    "set_palette_color":       "sp",
    "get_palette":             "gp",
    "select_color_slot":       "ps",
    "edit_slot":               "es",
    "vmesh_add_vertices":      "av",
    "vmesh_add_triangles":     "at",
    "vmesh_delete_triangles":  "dt",
    "vmesh_delete_vertices":   "dv",
    "vmesh_subdivide_triangles":"sd",
    "vmesh_perturb_vertices":  "pv",
    "vmesh_scale_from":        "sf",
    "vmesh_flip_triangle":     "ft",
    "vmesh_list_vertices":     "lv",
    "vmesh_list_triangles":    "lt",
    "vmesh_save":              "sv",
    "vmesh_clear":             "cl",
    "finish_vmesh_edit":       "fn",
    "vmesh_auto_fix_normals":  "af",
    "vmesh_build":             "bd",
    "get_camera_position":     "cp",
    "get_camera_forward":      "cf",
    "get_aimed_block":         "ab",
    "list_blocks":             "lb",
}
LONG_NAMES = {v: k for k, v in SHORT_NAMES.items()}

# Tool availability by scene. Tools not listed are unavailable in that scene.
# get_current_scene is included everywhere as a safety fallback.
SCENE_TOOLS = {
    "3dModeler": [
        "get_current_scene",
        "set_palette_color",
        "get_palette",
        "select_color_slot",
        "edit_slot",
    ],
    "vectorMesh": [
        "get_current_scene",
        "vmesh_build",
        "vmesh_add_vertices",
        "vmesh_add_triangles",
        "vmesh_delete_triangles",
        "vmesh_delete_vertices",
        "vmesh_subdivide_triangles",
        "vmesh_perturb_vertices",
        "vmesh_scale_from",
        "vmesh_auto_fix_normals",
        "vmesh_flip_triangle",
        "vmesh_list_vertices",
        "vmesh_list_triangles",
        "vmesh_save",
        "vmesh_clear",
        "finish_vmesh_edit",
    ],
    # Menu and other scenes expose only the scene query.
    "menu": ["get_current_scene"],
    "createScene": ["get_current_scene"],
}

# Extra tools available only when the user clicks "Send + Info". These let
# the model inspect spatial state (camera, hover) and take spatial actions
# (paint_face). Cost of the extra declarations is paid only for those prompts.
CONTEXT_TOOLS = {
    "3dModeler": ["get_camera_position", "get_camera_forward", "get_aimed_block", "list_blocks"],
}


# ---- Global state -----------------------------------------------------------

current_scene = ""
history = []       # list[types.Content]
client = None
# Toggled true for the duration of a single "Send + Info" prompt.
context_mode = False


def tools_for_current_scene():
    names = list(SCENE_TOOLS.get(current_scene) or ["get_current_scene"])
    if context_mode:
        names += CONTEXT_TOOLS.get(current_scene, [])
    result = []
    seen = set()
    for n in names:
        if n in seen or n not in TOOLS_BY_NAME:
            continue
        seen.add(n)
        spec = dict(TOOLS_BY_NAME[n])
        spec["name"] = SHORT_NAMES.get(n, n)
        result.append(spec)
    return result


def make_config():
    scene_part = SYSTEM_BY_SCENE.get(current_scene, "")
    instruction = SYSTEM_BASE + (("\n\n" + scene_part) if scene_part else "")
    return types.GenerateContentConfig(
        system_instruction=instruction,
        tools=[types.Tool(function_declarations=tools_for_current_scene())],
        # Give flash ~4x its default output cap so it doesn't bail out of
        # large bd calls (many vertices/triangles) or refuse complex shapes
        # out of caution.
        max_output_tokens=32768,
    )


# ---- IO helpers -------------------------------------------------------------

def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def log(text):
    emit({"type": "log", "text": text})


def read_line():
    """Read one JSON message from stdin, processing scene_change as a side
    effect and returning the next non-scene-change message. Returns None on
    EOF, and exits the process on shutdown."""
    global current_scene
    while True:
        line = sys.stdin.readline()
        if not line:
            return None
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            log(f"bad json from host: {line.strip()}")
            continue
        t = msg.get("type")
        if t == "scene_change":
            current_scene = msg.get("scene", "") or ""
            continue
        if t == "shutdown":
            sys.exit(0)
        return msg


# ---- Tool call plumbing -----------------------------------------------------

def call_tool(name, args):
    call_id = uuid.uuid4().hex[:12]
    emit({"type": "tool_call", "call_id": call_id, "name": name, "args": args})
    while True:
        msg = read_line()
        if msg is None:
            raise RuntimeError("stdin closed while awaiting tool_result")
        if msg.get("type") == "tool_result" and msg.get("call_id") == call_id:
            if msg.get("ok"):
                return msg.get("result")
            return {"error": msg.get("error", "unknown error")}
        log(f"unexpected message while awaiting tool_result: {msg}")


# ---- Main agent loop --------------------------------------------------------

def generate_with_retry():
    """Wrap generate_content in exponential-backoff retry for transient
    503/429 errors. Google's free tier returns these when the model is
    temporarily overloaded or rate-limited; they're almost always fleeting."""
    delays = [1, 3, 7]  # ~11s total before giving up
    last_err = None
    for attempt, delay in enumerate([0] + delays):
        if delay:
            time.sleep(delay)
        try:
            return client.models.generate_content(
                model=MODEL,
                contents=history,
                config=make_config(),
            )
        except Exception as e:
            err = str(e)
            transient = "503" in err or "UNAVAILABLE" in err or "429" in err or "RESOURCE_EXHAUSTED" in err
            if not transient:
                raise
            last_err = e
            log(f"retry {attempt + 1} after transient error: {err[:120]}")
    raise last_err


def run_prompt(prompt_text, with_context=False):
    global history, context_mode
    context_mode = with_context
    history.append(types.Content(
        role="user",
        parts=[types.Part.from_text(text=prompt_text)],
    ))

    total_in = 0
    total_out = 0
    total_cached = 0
    malformed_retries = 0
    MAX_MALFORMED_RETRIES = 2

    while True:
        response = generate_with_retry()

        # Accumulate token usage across every generate_content call in this
        # prompt so the final cost reflects the whole tool-use loop.
        usage = getattr(response, "usage_metadata", None)
        if usage is not None:
            total_in     += getattr(usage, "prompt_token_count", 0) or 0
            total_out    += getattr(usage, "candidates_token_count", 0) or 0
            total_cached += getattr(usage, "cached_content_token_count", 0) or 0

        if not response.candidates:
            cost = _compute_cost(total_in, total_out, total_cached)
            emit({"type": "final",
                  "text": f"(no response)  [${cost:.5f} | {total_in}->{total_out} tok]"})
            return

        candidate = response.candidates[0]
        model_content = candidate.content
        # Gemini sometimes returns a candidate with content=None — most often
        # when finish_reason is SAFETY, MAX_TOKENS, MALFORMED_FUNCTION_CALL,
        # or a token-quota issue.
        if model_content is None:
            reason = getattr(candidate, "finish_reason", None)
            reason_str = str(reason) if reason is not None else "unknown"
            # Silently retry malformed function calls and transient-looking
            # reasons — re-sampling usually produces a clean response.
            retryable = any(tok in reason_str.upper() for tok in
                            ("MALFORMED", "OTHER", "FINISH_REASON_UNSPECIFIED"))
            if retryable and malformed_retries < MAX_MALFORMED_RETRIES:
                malformed_retries += 1
                log(f"retrying after {reason_str} ({malformed_retries}/{MAX_MALFORMED_RETRIES})")
                continue
            cost = _compute_cost(total_in, total_out, total_cached)
            emit({"type": "final",
                  "text": f"(empty response, finish_reason={reason_str})\n"
                          f"[${cost:.5f} | {total_in}->{total_out} tok]"})
            return
        history.append(model_content)

        function_calls = []
        final_text_parts = []
        for part in (model_content.parts or []):
            if getattr(part, "function_call", None):
                function_calls.append(part.function_call)
            elif getattr(part, "text", None):
                final_text_parts.append(part.text)

        if not function_calls:
            text = "".join(final_text_parts).strip() or "(done)"
            cost = _compute_cost(total_in, total_out, total_cached)
            text = f"{text}\n[${cost:.5f} | {total_in}->{total_out} tok]"
            emit({"type": "final", "text": text})
            return

        tool_response_parts = []
        for fc in function_calls:
            # Gemini saw the short alias; translate to the real name for C++.
            real_name = LONG_NAMES.get(fc.name, fc.name)
            args = dict(fc.args) if fc.args else {}
            result = call_tool(real_name, args)
            # Echo the short alias back so history is consistent.
            tool_response_parts.append(types.Part.from_function_response(
                name=fc.name,
                response={"result": result},
            ))
        history.append(types.Content(role="user", parts=tool_response_parts))


def main():
    global client
    api_key = os.environ.get("GEMINI_API_KEY") or os.environ.get("GOOGLE_API_KEY")
    if not api_key:
        emit({"type": "error", "text": "GEMINI_API_KEY env var not set"})
        return

    client = genai.Client(api_key=api_key)
    emit({"type": "ready"})

    while True:
        msg = read_line()
        if msg is None:
            return
        t = msg.get("type")
        if t == "prompt":
            global context_mode
            try:
                run_prompt(msg.get("text", ""), with_context=msg.get("with_context", False))
            except Exception as e:
                emit({"type": "error", "text": f"{e}\n{traceback.format_exc()}"})
            finally:
                context_mode = False
        else:
            log(f"ignoring message of type: {t}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        emit({"type": "error", "text": f"{e}\n{traceback.format_exc()}"})
