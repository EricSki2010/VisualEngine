#include "vectorMesh.h"
#include "../../../VisualEngine/VisualEngine.h"
#include "../../../VisualEngine/EngineGlobals.h"
#include "../../../VisualEngine/inputManagement/Camera.h"
#include "../../../VisualEngine/inputManagement/Raycasting.h"
#include "../../../VisualEngine/renderingManagement/primitives/LineRenderer.h"
#include "../../../VisualEngine/renderingManagement/primitives/DotRenderer.h"
#include "../../../VisualEngine/renderingManagement/meshing/Overlay.h"
#include "../../../VisualEngine/inputManagement/Collision.h"
#include "../../../VisualEngine/uiManagement/UIManager.h"
#include "../../../VisualEngine/uiManagement/UIRenderer.h"
#include "../../../VisualEngine/uiManagement/UIPrefabs.h"
#include "../../../VisualEngine/uiManagement/TextRenderer.h"
#include "../../../VisualEngine/uiManagement/EmbeddedFont.h"
#include "SceneData.h"
#include "../../../VisualEngine/renderingManagement/meshing/ChunkMesh.h"
#include "../../../VisualEngine/memoryManagement/memory.h"
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <iostream>

// 8 corners of a unit cube centered at origin
static const glm::vec3 cubeVerts[8] = {
    {-0.5f,  0.5f, -0.5f}, // 0 top back-left
    { 0.5f,  0.5f, -0.5f}, // 1 top back-right
    { 0.5f,  0.5f,  0.5f}, // 2 top front-right
    {-0.5f,  0.5f,  0.5f}, // 3 top front-left
    {-0.5f, -0.5f, -0.5f}, // 4 bottom back-left
    { 0.5f, -0.5f, -0.5f}, // 5 bottom back-right
    { 0.5f, -0.5f,  0.5f}, // 6 bottom front-right
    {-0.5f, -0.5f,  0.5f}, // 7 bottom front-left
};

// 12 edges as pairs of vertex indices
static const int cubeEdges[12][2] = {
    // Top square
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    // Bottom square
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    // Vertical connections
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

static int sDrawMode = -1; // -1 = none, 0 = Vector, 1 = Line, 2 = Plane
static bool sHideOutlines = false;
static bool sWasLeftDown = false;
static bool sWasDDown = false;
static bool sWasADown = false;
static std::vector<glm::vec3> sPlacedDots;
static bool sHoverValid = false;
static glm::vec3 sHoverPoint;
static int sHoverPlacedIndex = -1;

// Line mode state
static std::vector<int> sLineSelectedDots; // indices into sPlacedDots, max 2
struct PlacedLine { int dotA; int dotB; };
static std::vector<PlacedLine> sPlacedLines;
static int sHoverLineIndex = -1;

// Plane mode state
static std::vector<int> sPlaneSelectedDots; // indices into sPlacedDots, max 3
struct PlacedTriangle { int dotA; int dotB; int dotC; bool flipped = false; };
static std::vector<PlacedTriangle> sPlacedTriangles;
static int sHoverTriIndex = -1;
static int sSelectedTriIndex = -1;

// File state
static std::string sVMeshName;
static int sReturnSlot = -1;
static std::string sReturnModelName;
static bool sVMeshPaused = false;
static bool sVMeshWasEscDown = false;
static bool sVMeshWasCtrlTabDown = false;
static bool sVMeshWasCtrlZDown = false;

// Undo system: snapshot of all placed geometry
struct VMeshSnapshot {
    std::vector<glm::vec3> dots;
    std::vector<PlacedLine> lines;
    std::vector<PlacedTriangle> triangles;
};
static std::vector<VMeshSnapshot> sVMeshUndoStack;
static const int kVMeshMaxUndoSteps = 50;

static void pushVMeshUndo() {
    VMeshSnapshot s;
    s.dots = sPlacedDots;
    s.lines = sPlacedLines;
    s.triangles = sPlacedTriangles;
    sVMeshUndoStack.push_back(std::move(s));
    if ((int)sVMeshUndoStack.size() > kVMeshMaxUndoSteps)
        sVMeshUndoStack.erase(sVMeshUndoStack.begin());
}

static void exportVMeshToMesh() {
    if (sVMeshName.empty() || sPlacedTriangles.empty()) return;
    std::string meshPath = "assets/saves/vectorMeshes/" + sVMeshName + ".mesh";
    std::ofstream out(meshPath, std::ios::binary);
    if (!out) return;

    std::vector<float> verts;
    std::vector<unsigned int> indices;
    for (const auto& t : sPlacedTriangles) {
        if (t.dotA < (int)sPlacedDots.size() && t.dotB < (int)sPlacedDots.size() &&
            t.dotC < (int)sPlacedDots.size()) {

            glm::vec3 va = sPlacedDots[t.dotA];
            glm::vec3 vb = sPlacedDots[t.dotB];
            glm::vec3 vc = sPlacedDots[t.dotC];

            // Blanket flip: user's natural click convention is CW from their
            // viewing angle, so RH cross points inward. Negate on save so
            // stored normal points outward by default. Per-triangle override
            // via the Flip Normal button (swaps winding + negates normal
            // again for the oddball triangles).
            glm::vec3 normal = glm::cross(vb - va, vc - va);
            if (t.flipped) {
                std::swap(vb, vc);
                normal = -normal;
            }
            normal = glm::normalize(-normal);

            unsigned int base = (unsigned int)(verts.size() / 8);
            auto addVert = [&](const glm::vec3& p, const glm::vec3& n) {
                verts.push_back(p.x); verts.push_back(p.y); verts.push_back(p.z);
                verts.push_back(0.0f); verts.push_back(0.0f);
                verts.push_back(n.x); verts.push_back(n.y); verts.push_back(n.z);
            };

            addVert(va, normal); addVert(vb, normal); addVert(vc, normal);
            indices.push_back(base); indices.push_back(base+1); indices.push_back(base+2);
        }
    }

    uint32_t vertCount = (uint32_t)(verts.size() / 8);
    uint32_t idxCount = (uint32_t)indices.size();
    uint32_t texPathLen = 0;
    char magic[2] = {'V', 'N'};
    out.write(magic, 2);
    out.write(reinterpret_cast<const char*>(&vertCount), 4);
    out.write(reinterpret_cast<const char*>(&idxCount), 4);
    out.write(reinterpret_cast<const char*>(&texPathLen), 4);
    out.write(reinterpret_cast<const char*>(verts.data()), verts.size() * sizeof(float));
    out.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));
}

static void saveVectorMesh() {
    if (sVMeshName.empty()) return;
    setMemoryPath("assets/saves/vectorMeshes");
    std::string path = "assets/saves/vectorMeshes/" + sVMeshName + ".vmesh";
    std::ofstream out(path, std::ios::binary);
    if (!out) { std::cerr << "Failed to save: " << path << std::endl; return; }

    // Write dots
    uint32_t dotCount = (uint32_t)sPlacedDots.size();
    out.write(reinterpret_cast<const char*>(&dotCount), 4);
    for (const auto& d : sPlacedDots)
        out.write(reinterpret_cast<const char*>(&d), sizeof(glm::vec3));

    // Write lines
    uint32_t lineCount = (uint32_t)sPlacedLines.size();
    out.write(reinterpret_cast<const char*>(&lineCount), 4);
    for (const auto& l : sPlacedLines) {
        out.write(reinterpret_cast<const char*>(&l.dotA), 4);
        out.write(reinterpret_cast<const char*>(&l.dotB), 4);
    }

    // Write triangles
    uint32_t triCount = (uint32_t)sPlacedTriangles.size();
    out.write(reinterpret_cast<const char*>(&triCount), 4);
    for (const auto& t : sPlacedTriangles) {
        out.write(reinterpret_cast<const char*>(&t.dotA), 4);
        out.write(reinterpret_cast<const char*>(&t.dotB), 4);
        out.write(reinterpret_cast<const char*>(&t.dotC), 4);
        uint8_t flipped = t.flipped ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&flipped), 1);
    }
}

static void loadVectorMesh(const std::string& name) {
    sVMeshName = name;
    sPlacedDots.clear();
    sPlacedLines.clear();
    sPlacedTriangles.clear();

    std::string path = "assets/saves/vectorMeshes/" + name + ".vmesh";
    std::ifstream in(path, std::ios::binary);
    if (!in) return; // new file, start empty

    // Read dots
    uint32_t dotCount = 0;
    in.read(reinterpret_cast<char*>(&dotCount), 4);
    sPlacedDots.resize(dotCount);
    for (uint32_t i = 0; i < dotCount; i++)
        in.read(reinterpret_cast<char*>(&sPlacedDots[i]), sizeof(glm::vec3));

    // Read lines
    uint32_t lineCount = 0;
    in.read(reinterpret_cast<char*>(&lineCount), 4);
    sPlacedLines.resize(lineCount);
    for (uint32_t i = 0; i < lineCount; i++) {
        in.read(reinterpret_cast<char*>(&sPlacedLines[i].dotA), 4);
        in.read(reinterpret_cast<char*>(&sPlacedLines[i].dotB), 4);
    }

    // Read triangles
    uint32_t triCount = 0;
    in.read(reinterpret_cast<char*>(&triCount), 4);
    sPlacedTriangles.resize(triCount);
    for (uint32_t i = 0; i < triCount; i++) {
        in.read(reinterpret_cast<char*>(&sPlacedTriangles[i].dotA), 4);
        in.read(reinterpret_cast<char*>(&sPlacedTriangles[i].dotB), 4);
        in.read(reinterpret_cast<char*>(&sPlacedTriangles[i].dotC), 4);
        uint8_t flipped = 0;
        in.read(reinterpret_cast<char*>(&flipped), 1);
        sPlacedTriangles[i].flipped = (flipped != 0);
    }
}

static void openVMeshPauseMenu() {
    sVMeshPaused = true;

    Camera* cam = getGlobalCamera();
    if (cam->looking) {
        cam->looking = false;
        glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    addUIGroup("vmesh_pause");

    float btnW = 0.3f;
    float btnH = 0.08f;
    float btnX = -btnW / 2.0f;
    glm::vec4 btnColor = {0.15f, 0.15f, 0.15f, 0.95f};

    addToGroup("vmesh_pause", createButton("vm_continue",
        btnX, 0.1f, btnW, btnH, btnColor, "Continue",
        []() {
            sVMeshPaused = false;
            VE::setBrightness(1.0f);
            removeUIGroup("vmesh_pause");
        }
    ));

    addToGroup("vmesh_pause", createButton("vm_save",
        btnX, -0.02f, btnW, btnH,
        {0.1f, 0.4f, 0.15f, 0.95f}, "Save Model",
        []() {
            saveVectorMesh();
            exportVMeshToMesh();
            sVMeshPaused = false;
            VE::setBrightness(1.0f);
            removeUIGroup("vmesh_pause");
        }
    ));

    addToGroup("vmesh_pause", createButton("vm_exit",
        btnX, -0.14f, btnW, btnH,
        {0.3f, 0.1f, 0.1f, 0.9f}, "Exit",
        []() {
            sVMeshPaused = false;
            VE::setBrightness(1.0f);
            removeUIGroup("vmesh_pause");
            if (!sReturnModelName.empty())
                VE::setScene("3dModeler", new std::string(sReturnModelName));
            else
                VE::setScene("menu");
        }
    ));
}

// Build all snap points from edges, deduplicating corners
static std::vector<glm::vec3> buildSnapPoints(int snapCount) {
    std::vector<glm::vec3> points;

    // Always include corners (endpoints)
    for (int i = 0; i < 8; i++)
        points.push_back(cubeVerts[i]);

    // Include placed dot positions as snap points
    for (const auto& d : sPlacedDots)
        points.push_back(d);

    // Add intermediate points along each wireframe edge
    int divisions = snapCount > 0 ? snapCount + 1 : 0;
    if (divisions > 0) {
        for (int e = 0; e < 12; e++) {
            glm::vec3 from = cubeVerts[cubeEdges[e][0]];
            glm::vec3 to = cubeVerts[cubeEdges[e][1]];
            for (int i = 1; i < divisions; i++) {
                float t = (float)i / (float)divisions;
                points.push_back(from + t * (to - from));
            }
        }

        // Add intermediate points along placed lines
        for (const auto& l : sPlacedLines) {
            if (l.dotA < (int)sPlacedDots.size() && l.dotB < (int)sPlacedDots.size()) {
                glm::vec3 from = sPlacedDots[l.dotA];
                glm::vec3 to = sPlacedDots[l.dotB];
                for (int i = 1; i < divisions; i++) {
                    float t = (float)i / (float)divisions;
                    points.push_back(from + t * (to - from));
                }
            }
        }
    }

    // Deduplicate (corners shared by multiple edges)
    std::vector<glm::vec3> unique;
    for (const auto& p : points) {
        bool dup = false;
        for (const auto& u : unique)
            if (glm::length(p - u) < 0.001f) { dup = true; break; }
        if (!dup) unique.push_back(p);
    }
    return unique;
}

// Find the nearest snap point to a 3D position
static int findNearestSnap(const std::vector<glm::vec3>& snaps, const glm::vec3& pos) {
    int best = -1;
    float bestDist = 999999.0f;
    for (int i = 0; i < (int)snaps.size(); i++) {
        float d = glm::length(snaps[i] - pos);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

// Find if mouse is near a placed dot (screen-space check, closest to camera wins)
static int findHoveredDot(double mx, double my) {
    int best = -1;
    float bestDepth = 999999.0f;
    for (int i = 0; i < (int)sPlacedDots.size(); i++) {
        glm::vec4 clip = ctx.scene->projection * ctx.scene->view * glm::vec4(sPlacedDots[i], 1.0f);
        if (clip.w <= 0.0001f) continue;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = (ndc.x * 0.5f + 0.5f) * ctx.width;
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * ctx.height;
        float dist = glm::length(glm::vec2(sx, sy) - glm::vec2((float)mx, (float)my));
        float depth = clip.w; // distance from camera
        if (dist < 12.0f && depth < bestDepth) {
            bestDepth = depth;
            best = i;
        }
    }
    return best;
}

static const float PANEL_X = 0.6f;
static const float PANEL_W = 0.4f;
static const float PANEL_PAD = 0.02f;
static const float SIDE_BTN_W = PANEL_W - PANEL_PAD * 2;
static const float SIDE_BTN_H = 0.06f;
static const float SIDE_BTN_X = PANEL_X + PANEL_PAD;
static const float SIDE_START_Y = 0.9f;
static const float SIDE_GAP = SIDE_BTN_H + PANEL_PAD;

static void rebuildModeTools() {
    removeUIGroup("mode_tools");
    addUIGroup("mode_tools");

    // Start below: hide toggle at SIDE_START_Y, dropdown at -1 gap, tools at -2 gaps
    float y = SIDE_START_Y - SIDE_GAP * 2;

    if (sDrawMode == 0) { // Vector
        addToGroup("mode_tools", createButton("snap_label",
            SIDE_BTN_X, y, SIDE_BTN_W, SIDE_BTN_H,
            {0.0f, 0.0f, 0.0f, 0.0f}, "Snap Point Count",
            nullptr
        ));
        y -= SIDE_GAP;

        addToGroup("mode_tools", createTextInput("snap_input",
            SIDE_BTN_X, y, SIDE_BTN_W, SIDE_BTN_H,
            {0.18f, 0.18f, 0.18f, 0.95f},
            "Enter value...", 3
        ));
        y -= SIDE_GAP;
    } else if (sDrawMode == 1) { // Line
        // Line mode tools can go here
    } else if (sDrawMode == 2) { // Plane
        if (sSelectedTriIndex >= 0 && sSelectedTriIndex < (int)sPlacedTriangles.size()) {
            addToGroup("mode_tools", createButton("flip_normal",
                SIDE_BTN_X, y, SIDE_BTN_W, SIDE_BTN_H,
                {0.3f, 0.2f, 0.5f, 0.95f}, "Flip Normal",
                []() {
                    if (sSelectedTriIndex >= 0 && sSelectedTriIndex < (int)sPlacedTriangles.size()) {
                        pushVMeshUndo();
                        sPlacedTriangles[sSelectedTriIndex].flipped = !sPlacedTriangles[sSelectedTriIndex].flipped;
                    }
                }
            ));
            y -= SIDE_GAP;
        }
    }
}

static int getSnapCount() {
    std::string val = getInputText("mode_tools", "snap_input");
    if (val.empty()) return 0;
    try { return std::stoi(val); }
    catch (...) { return 0; }
}

void registerVectorMeshScene() {
    VE::registerScene("vectorMesh",
        // onEnter
        [](void* data) {
            getGlobalCamera()->setMode(CAMERA_FPS);
            initLineRenderer();
            initDotRenderer();
            initOverlay();
            initUIRenderer();
            initTextRendererFromMemory(EMBEDDED_FONT_DATA, EMBEDDED_FONT_SIZE, 48);
            VE::setCamera(2, 1.5f, 2, 210, -25);
            VE::setGradientBackground(true);
            sDrawMode = -1;
            sPlacedDots.clear();
            sPlacedLines.clear();
            sPlacedTriangles.clear();
            sWasLeftDown = false;
            sVMeshPaused = false;
            sVMeshWasEscDown = false;

            sReturnSlot = -1;
            sReturnModelName = "";

            // Load vector mesh if data was passed
            if (data) {
                auto* editData = static_cast<VectorMeshEditData*>(data);
                loadVectorMesh(editData->meshName);
                sReturnSlot = editData->slotIndex;
                sReturnModelName = editData->modelName;
                delete editData;
            }
            sVMeshUndoStack.clear();

            // Right sidebar
            addUIGroup("sidebar");
            addToGroup("sidebar", createPanel("sidebar_bg",
                PANEL_X, -1.0f, PANEL_W, 2.0f,
                {0.12f, 0.12f, 0.12f, 0.85f}
            ));

            // Hide outlines toggle
            addToGroup("sidebar", createButton("hide_outlines",
                SIDE_BTN_X, SIDE_START_Y, SIDE_BTN_W, SIDE_BTN_H,
                {0.25f, 0.25f, 0.25f, 0.95f}, "Hide Outlines: Off",
                []() {
                    sHideOutlines = !sHideOutlines;
                    UIElement* btn = getUIElement("sidebar", "hide_outlines");
                    if (btn) btn->label = sHideOutlines ? "Hide Outlines: On" : "Hide Outlines: Off";
                }
            ));

            // Draw mode dropdown
            createDropdown("sidebar", "draw_mode",
                SIDE_BTN_X, SIDE_START_Y - SIDE_GAP, SIDE_BTN_W, SIDE_BTN_H,
                {0.18f, 0.18f, 0.18f, 0.95f},
                "Draw Mode...",
                {"Vector", "Line", "Plane"},
                [](int index, const std::string& option) {
                    sDrawMode = index;
                    sLineSelectedDots.clear();
                    sPlaneSelectedDots.clear();
                    sSelectedTriIndex = -1;
                    rebuildModeTools();
                },
                0.0f, -SIDE_GAP
            );
        },
        // onExit
        []() {
            cleanupLineRenderer();
            cleanupDotRenderer();
            cleanupOverlay();
            VE::setBrightness(1.0f);
            VE::setGradientBackground(false);
            cleanupTextRenderer();
            cleanupUIRenderer();
            clearUI();
        },
        // onInput
        [](float dt) {
            // Esc toggle pause menu
            bool escDown = glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !sVMeshWasEscDown) {
                if (sVMeshPaused) {
                    sVMeshPaused = false;
                    VE::setBrightness(1.0f);
                    removeUIGroup("vmesh_pause");
                } else {
                    openVMeshPauseMenu();
                }
            }
            sVMeshWasEscDown = escDown;

            // Ctrl+Tab: save and exit back to 3dModeler - fires on release
            bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            bool tabDown = glfwGetKey(ctx.window, GLFW_KEY_TAB) == GLFW_PRESS;
            bool ctrlTab = ctrlHeld && tabDown;
            if (!ctrlTab && sVMeshWasCtrlTabDown) {
                sVMeshWasCtrlTabDown = false;
                saveVectorMesh();
                exportVMeshToMesh();
                sVMeshPaused = false;
                VE::setBrightness(1.0f);
                removeUIGroup("vmesh_pause");
                if (!sReturnModelName.empty())
                    VE::setScene("3dModeler", new std::string(sReturnModelName));
                else
                    VE::setScene("menu");
                return;
            }
            sVMeshWasCtrlTabDown = ctrlTab;

            // Ctrl+Z: undo
            bool zDown = glfwGetKey(ctx.window, GLFW_KEY_Z) == GLFW_PRESS;
            bool ctrlZ = ctrlHeld && zDown;
            if (ctrlZ && !sVMeshWasCtrlZDown) {
                if (!sVMeshUndoStack.empty()) {
                    VMeshSnapshot s = sVMeshUndoStack.back();
                    sVMeshUndoStack.pop_back();
                    sPlacedDots = s.dots;
                    sPlacedLines = s.lines;
                    sPlacedTriangles = s.triangles;
                    sLineSelectedDots.clear();
                    sPlaneSelectedDots.clear();
                    sSelectedTriIndex = -1;
                    sHoverPlacedIndex = -1;
                    sHoverLineIndex = -1;
                    sHoverTriIndex = -1;
                    rebuildModeTools();
                }
            }
            sVMeshWasCtrlZDown = ctrlZ;

            processUIInput();

            if (sVMeshPaused) return;

            if (!getGlobalCamera()->looking) {
                double mx, my;
                glfwGetCursorPos(ctx.window, &mx, &my);

                sHoverValid = false;
                sHoverPlacedIndex = -1;

                // Only interact outside sidebar, and only in Vector mode
                if (mx < ctx.width * 0.8f && sDrawMode == 0) {
                    // Check if hovering over a placed dot
                    sHoverPlacedIndex = findHoveredDot(mx, my);

                    // Ctrl+D to delete hovered placed dot
                    bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                    bool dDown = glfwGetKey(ctx.window, GLFW_KEY_D) == GLFW_PRESS;
                    if (dDown && !sWasDDown && ctrlHeld && sHoverPlacedIndex >= 0) {
                        pushVMeshUndo();
                        sPlacedDots.erase(sPlacedDots.begin() + sHoverPlacedIndex);
                        sHoverPlacedIndex = -1;
                    }
                    sWasDDown = dDown;
                    Ray ray = screenToRay(mx, my, ctx.width, ctx.height,
                                          ctx.scene->view, ctx.scene->projection);

                    // Find closest line hit
                    float bestScreenDist = 999.0f;
                    glm::vec3 bestPoint;
                    bool anyHit = false;

                    // Check wireframe edges
                    for (int e = 0; e < 12; e++) {
                        LineHit hit = rayToLine(ray,
                            cubeVerts[cubeEdges[e][0]], cubeVerts[cubeEdges[e][1]],
                            mx, my, ctx.width, ctx.height,
                            ctx.scene->view, ctx.scene->projection, 10.0f);
                        if (hit.hit && hit.screenDistance < bestScreenDist) {
                            bestScreenDist = hit.screenDistance;
                            bestPoint = hit.point;
                            anyHit = true;
                        }
                    }

                    // Check placed lines
                    for (const auto& l : sPlacedLines) {
                        if (l.dotA < (int)sPlacedDots.size() && l.dotB < (int)sPlacedDots.size()) {
                            LineHit hit = rayToLine(ray,
                                sPlacedDots[l.dotA], sPlacedDots[l.dotB],
                                mx, my, ctx.width, ctx.height,
                                ctx.scene->view, ctx.scene->projection, 10.0f);
                            if (hit.hit && hit.screenDistance < bestScreenDist) {
                                bestScreenDist = hit.screenDistance;
                                bestPoint = hit.point;
                                anyHit = true;
                            }
                        }
                    }

                    if (anyHit) {
                        int snapCount = getSnapCount();
                        std::vector<glm::vec3> snaps = buildSnapPoints(snapCount);
                        int nearest = findNearestSnap(snaps, bestPoint);
                        if (nearest >= 0) {
                            sHoverPoint = snaps[nearest];
                            sHoverValid = true;
                        }
                    }

                    // Left-click to place dot (skip if one already exists there)
                    bool leftDown = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                    if (leftDown && !sWasLeftDown && sHoverValid) {
                        bool exists = false;
                        for (const auto& d : sPlacedDots)
                            if (glm::length(d - sHoverPoint) < 0.001f) { exists = true; break; }
                        if (!exists) {
                            pushVMeshUndo();
                            sPlacedDots.push_back(sHoverPoint);
                        }
                    }
                    sWasLeftDown = leftDown;
                }

                // Line mode: select placed dots and Ctrl+A to connect
                if (mx < ctx.width * 0.8f && sDrawMode == 1) {
                    sHoverPlacedIndex = findHoveredDot(mx, my);

                    // Detect hovered placed line
                    sHoverLineIndex = -1;
                    if (sHoverPlacedIndex < 0) {
                        Ray ray = screenToRay(mx, my, ctx.width, ctx.height,
                                              ctx.scene->view, ctx.scene->projection);
                        float bestDist = 999.0f;
                        for (int i = 0; i < (int)sPlacedLines.size(); i++) {
                            const auto& l = sPlacedLines[i];
                            if (l.dotA < (int)sPlacedDots.size() && l.dotB < (int)sPlacedDots.size()) {
                                LineHit hit = rayToLine(ray,
                                    sPlacedDots[l.dotA], sPlacedDots[l.dotB],
                                    mx, my, ctx.width, ctx.height,
                                    ctx.scene->view, ctx.scene->projection, 8.0f);
                                if (hit.hit && hit.screenDistance < bestDist) {
                                    bestDist = hit.screenDistance;
                                    sHoverLineIndex = i;
                                }
                            }
                        }
                    }

                    // Ctrl+D to delete hovered line
                    bool ctrlHeld2 = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                    bool dDown2 = glfwGetKey(ctx.window, GLFW_KEY_D) == GLFW_PRESS;
                    if (dDown2 && !sWasDDown && ctrlHeld2 && sHoverLineIndex >= 0) {
                        pushVMeshUndo();
                        sPlacedLines.erase(sPlacedLines.begin() + sHoverLineIndex);
                        sHoverLineIndex = -1;
                    }
                    sWasDDown = dDown2;

                    // Left-click to select dots (max 2, third replaces first)
                    bool leftDown = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                    if (leftDown && !sWasLeftDown && sHoverPlacedIndex >= 0) {
                        // Don't add duplicate selection
                        bool alreadySelected = false;
                        for (int s : sLineSelectedDots)
                            if (s == sHoverPlacedIndex) { alreadySelected = true; break; }

                        if (!alreadySelected) {
                            if ((int)sLineSelectedDots.size() >= 2)
                                sLineSelectedDots.erase(sLineSelectedDots.begin());
                            sLineSelectedDots.push_back(sHoverPlacedIndex);
                        }
                    }
                    sWasLeftDown = leftDown;

                    // Ctrl+A to add line between 2 selected dots
                    bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                    bool aDown = glfwGetKey(ctx.window, GLFW_KEY_A) == GLFW_PRESS;
                    if (aDown && !sWasADown && ctrlHeld && (int)sLineSelectedDots.size() == 2) {
                        int a = sLineSelectedDots[0];
                        int b = sLineSelectedDots[1];

                        // Check if line already exists
                        bool exists = false;
                        for (const auto& l : sPlacedLines)
                            if ((l.dotA == a && l.dotB == b) || (l.dotA == b && l.dotB == a))
                                { exists = true; break; }

                        if (!exists) {
                            pushVMeshUndo();
                            sPlacedLines.push_back({a, b});
                            sLineSelectedDots.clear();
                        }
                    }
                    sWasADown = aDown;
                }

                // Plane mode: select 3 dots, Ctrl+A to create triangle
                if (mx < ctx.width * 0.8f && sDrawMode == 2) {
                    sHoverPlacedIndex = findHoveredDot(mx, my);

                    // Detect hovered triangle
                    sHoverTriIndex = -1;
                    if (sHoverPlacedIndex < 0) {
                        Ray ray = screenToRay(mx, my, ctx.width, ctx.height,
                                              ctx.scene->view, ctx.scene->projection);
                        float bestDist = 999999.0f;
                        for (int i = 0; i < (int)sPlacedTriangles.size(); i++) {
                            const auto& tr = sPlacedTriangles[i];
                            if (tr.dotA < (int)sPlacedDots.size() && tr.dotB < (int)sPlacedDots.size() &&
                                tr.dotC < (int)sPlacedDots.size()) {
                                // Check both sides
                                TriangleHit hit = rayToTriangle(ray, sPlacedDots[tr.dotA], sPlacedDots[tr.dotB], sPlacedDots[tr.dotC]);
                                if (!hit.hit)
                                    hit = rayToTriangle(ray, sPlacedDots[tr.dotA], sPlacedDots[tr.dotC], sPlacedDots[tr.dotB]);
                                if (hit.hit && hit.distance < bestDist) {
                                    bestDist = hit.distance;
                                    sHoverTriIndex = i;
                                }
                            }
                        }
                    }

                    // Left-click to select triangle or dots
                    bool leftDown = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                    if (leftDown && !sWasLeftDown) {
                        if (sHoverTriIndex >= 0 && sHoverPlacedIndex < 0) {
                            // Select/deselect triangle
                            int prev = sSelectedTriIndex;
                            sSelectedTriIndex = (sSelectedTriIndex == sHoverTriIndex) ? -1 : sHoverTriIndex;
                            if (sSelectedTriIndex != prev) rebuildModeTools();
                        } else if (sHoverPlacedIndex < 0 && sHoverTriIndex < 0) {
                            // Click empty space — deselect triangle
                            if (sSelectedTriIndex >= 0) {
                                sSelectedTriIndex = -1;
                                rebuildModeTools();
                            }
                        }
                    }
                    if (leftDown && !sWasLeftDown && sHoverPlacedIndex >= 0) {
                        bool alreadySelected = false;
                        for (int s : sPlaneSelectedDots)
                            if (s == sHoverPlacedIndex) { alreadySelected = true; break; }

                        if (!alreadySelected) {
                            if ((int)sPlaneSelectedDots.size() >= 3)
                                sPlaneSelectedDots.erase(sPlaneSelectedDots.begin());
                            sPlaneSelectedDots.push_back(sHoverPlacedIndex);
                        }
                    }
                    sWasLeftDown = leftDown;

                    // Ctrl+A to add triangle between 3 selected dots
                    bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                    bool aDown = glfwGetKey(ctx.window, GLFW_KEY_A) == GLFW_PRESS;
                    if (aDown && !sWasADown && ctrlHeld && (int)sPlaneSelectedDots.size() == 3) {
                        int a = sPlaneSelectedDots[0];
                        int b = sPlaneSelectedDots[1];
                        int c = sPlaneSelectedDots[2];

                        // Check if triangle already exists (any vertex order)
                        bool exists = false;
                        for (const auto& t : sPlacedTriangles) {
                            int ta[3] = {t.dotA, t.dotB, t.dotC};
                            int na[3] = {a, b, c};
                            // Check all permutations
                            bool match = false;
                            for (int p = 0; p < 3 && !match; p++)
                                for (int q = 0; q < 3 && !match; q++)
                                    if (q != p)
                                        for (int r = 0; r < 3 && !match; r++)
                                            if (r != p && r != q)
                                                if (ta[0]==na[p] && ta[1]==na[q] && ta[2]==na[r])
                                                    match = true;
                            if (match) { exists = true; break; }
                        }

                        if (!exists) {
                            pushVMeshUndo();
                            sPlacedTriangles.push_back({a, b, c});
                            sPlaneSelectedDots.clear();
                        }
                    }
                    sWasADown = aDown;

                    // Ctrl+D to delete hovered triangle
                    bool ctrlHeld2 = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
                    bool dDown2 = glfwGetKey(ctx.window, GLFW_KEY_D) == GLFW_PRESS;
                    if (dDown2 && !sWasDDown && ctrlHeld2 && sHoverTriIndex >= 0) {
                        pushVMeshUndo();
                        sPlacedTriangles.erase(sPlacedTriangles.begin() + sHoverTriIndex);
                        sHoverTriIndex = -1;
                    }
                    sWasDDown = dDown2;
                }
            }
        },
        // onUpdate
        nullptr,
        // onRender
        []() {
            if (sVMeshPaused)
                VE::setBrightness(0.5f);

            if (!sHideOutlines) {
                // Draw 12 edges
                glm::vec3 lineColor(1.0f, 1.0f, 1.0f);
                for (int i = 0; i < 12; i++)
                    drawLine(cubeVerts[cubeEdges[i][0]], cubeVerts[cubeEdges[i][1]], lineColor, 2.0f);

                // Vector mode rendering
                if (sDrawMode == 0) {
                    int snapCount = getSnapCount();
                    std::vector<glm::vec3> snaps = buildSnapPoints(snapCount);
                    for (const auto& s : snaps)
                        drawDot(s, 0.02f, {0.0f, 1.0f, 0.5f});

                    if (sHoverValid)
                        drawDot(sHoverPoint, 0.05f, {1.0f, 1.0f, 0.0f}, 0.4f);
                }
            }

            // Draw placed triangles
            ctx.shader->use();
            for (int i = 0; i < (int)sPlacedTriangles.size(); i++) {
                const auto& t = sPlacedTriangles[i];
                if (t.dotA < (int)sPlacedDots.size() && t.dotB < (int)sPlacedDots.size() &&
                    t.dotC < (int)sPlacedDots.size()) {
                    Triangle tri = {sPlacedDots[t.dotA], sPlacedDots[t.dotB], sPlacedDots[t.dotC]};

                    // Use winding order as placed; user can flip manually
                    Triangle front = tri;
                    if (t.flipped)
                        front = {front.v0, front.v2, front.v1};

                    Triangle back = {front.v0, front.v2, front.v1};

                    glm::vec3 color(0.8f);
                    if (i == sSelectedTriIndex)
                        color = glm::vec3(0.5f, 0.7f, 1.0f);
                    else if (i == sHoverTriIndex)
                        color = glm::vec3(1.0f, 0.5f, 0.0f);

                    drawTriangleOverlay(*ctx.shader, front, color, 1.0f, false);
                    drawTriangleOverlay(*ctx.shader, back, color, 1.0f, false);
                }
            }

            if (!sHideOutlines) {
                // Draw placed lines (user-created)
                for (int i = 0; i < (int)sPlacedLines.size(); i++) {
                    const auto& l = sPlacedLines[i];
                    if (l.dotA < (int)sPlacedDots.size() && l.dotB < (int)sPlacedDots.size()) {
                        if (i == sHoverLineIndex)
                            drawLine(sPlacedDots[l.dotA], sPlacedDots[l.dotB], {1.0f, 0.5f, 0.0f}, 3.0f);
                        else
                            drawLine(sPlacedDots[l.dotA], sPlacedDots[l.dotB], {0.0f, 0.6f, 1.0f}, 2.0f);
                    }
                }

                // Draw placed dots
                for (int i = 0; i < (int)sPlacedDots.size(); i++) {
                    bool selected = false;
                    for (int s : sLineSelectedDots)
                        if (s == i) { selected = true; break; }
                    if (!selected)
                        for (int s : sPlaneSelectedDots)
                            if (s == i) { selected = true; break; }

                    if (selected)
                        drawDot(sPlacedDots[i], 0.05f, {0.0f, 0.5f, 1.0f});
                    else if (i == sHoverPlacedIndex)
                        drawDot(sPlacedDots[i], 0.05f, {1.0f, 0.5f, 0.0f});
                    else
                        drawDot(sPlacedDots[i], 0.04f, {1.0f, 0.0f, 0.0f});
                }
            }

            renderUI();
        }
    );
}
