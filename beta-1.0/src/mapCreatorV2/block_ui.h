#pragma once
#include "engine/gameplay/registry.h"
#include "raylib.h"
#include <string>
#include <vector>

// ── Block Thumbnail Renderer ────────────────────────────────────────

static Texture2D renderBlockThumbnail(BlockDef& def, int size) {
    RenderTexture2D rt = LoadRenderTexture(size, size);

    BeginTextureMode(rt);
        ClearBackground({ 30, 30, 35, 255 });

        if (def.formatType == "glb" && def.modelLoaded) {
            BoundingBox bb = GetModelBoundingBox(def.model);
            Vector3 center = {
                (bb.min.x + bb.max.x) * 0.5f * def.modelScaleX,
                (bb.min.y + bb.max.y) * 0.5f * def.modelScaleY,
                (bb.min.z + bb.max.z) * 0.5f * def.modelScaleZ
            };
            float extentX = (bb.max.x - bb.min.x) * def.modelScaleX;
            float extentY = (bb.max.y - bb.min.y) * def.modelScaleY;
            float extentZ = (bb.max.z - bb.min.z) * def.modelScaleZ;
            float maxExtent = extentX;
            if (extentY > maxExtent) maxExtent = extentY;
            if (extentZ > maxExtent) maxExtent = extentZ;
            float camDist = maxExtent * 1.4f;

            Camera3D thumbCam = {};
            thumbCam.position = { center.x + camDist * 0.7f, center.y + camDist * 0.5f, center.z + camDist * 0.7f };
            thumbCam.target   = center;
            thumbCam.up       = { 0.0f, 1.0f, 0.0f };
            thumbCam.fovy     = 40.0f;
            thumbCam.projection = CAMERA_PERSPECTIVE;

            BeginMode3D(thumbCam);
                DrawModelEx(def.model, { 0, 0, 0 },
                    { 0, 1, 0 }, 0.0f,
                    { def.modelScaleX, def.modelScaleY, def.modelScaleZ },
                    WHITE);
            EndMode3D();
        } else {
            Camera3D thumbCam = {};
            thumbCam.position = { 1.2f, 1.0f, 1.2f };
            thumbCam.target   = { 0.0f, 0.0f, 0.0f };
            thumbCam.up       = { 0.0f, 1.0f, 0.0f };
            thumbCam.fovy     = 40.0f;
            thumbCam.projection = CAMERA_PERSPECTIVE;

            BeginMode3D(thumbCam);
                Color col = { def.r, def.g, def.b, def.a };
                DrawCube({ 0, 0, 0 }, 0.8f, 0.8f, 0.8f, col);
                DrawCubeWires({ 0, 0, 0 }, 0.8f, 0.8f, 0.8f, {
                    (unsigned char)(def.r / 2), (unsigned char)(def.g / 2),
                    (unsigned char)(def.b / 2), 255 });
            EndMode3D();
        }
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    Texture2D thumb = LoadTextureFromImage(img);
    UnloadImage(img);
    UnloadRenderTexture(rt);

    return thumb;
}

// ── Shared layout calculation ───────────────────────────────────────
// Eliminates duplicated slot position math across draw/hit-test methods.

struct HotbarLayout {
    int count;
    int totalW;
    int startX;
    int startY;
    int slotSize;
    int slotPad;

    int slotX(int i) const { return startX + i * (slotSize + slotPad); }
    int slotY() const { return startY; }

    int hitTest(Vector2 mouse) const {
        for (int i = 0; i < count; i++) {
            int x = slotX(i);
            int y = slotY();
            if (mouse.x >= x && mouse.x < x + slotSize &&
                mouse.y >= y && mouse.y < y + slotSize) {
                return i;
            }
        }
        return -1;
    }
};

// ── Block Hotbar ────────────────────────────────────────────────────

struct BlockHotbar {
    static const int SLOT_COUNT = 20;
    static const int SLOT_SIZE  = 54;
    static const int SLOT_PAD   = 4;
    static const int THUMB_SIZE = 48;
    static const int RENDER_SIZE = 128; // render at high res, draw scaled down

    struct Slot {
        std::string name;
        Texture2D thumb = { 0 };  // kept for inventory swaps
        bool valid = false;
    };

    Slot slots[SLOT_COUNT] = {};
    int selected = 0;
    int scrollOffset = 0;
    float rotationAngle = 0.0f;
    Registry<BlockDef>* blocksRef = nullptr;
    RenderTexture2D slotRT = { 0 };

    int validCount() const {
        int n = 0;
        for (int i = 0; i < SLOT_COUNT; i++) if (slots[i].valid) n++;
        return n;
    }

    HotbarLayout getLayout() const {
        int count = validCount();
        int totalW = count * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        return {
            count, totalW,
            (GetScreenWidth() - totalW) / 2,
            GetScreenHeight() - SLOT_SIZE - 12,
            SLOT_SIZE, SLOT_PAD
        };
    }

    void load(Registry<BlockDef>& blocks) {
        blocksRef = &blocks;
        if (slotRT.id == 0) slotRT = LoadRenderTexture(RENDER_SIZE, RENDER_SIZE);
        int i = 0;
        for (auto& [name, def] : blocks.entries) {
            if (i >= SLOT_COUNT) break;
            slots[i].name  = name;
            slots[i].thumb = renderBlockThumbnail(def, THUMB_SIZE);
            slots[i].valid = true;
            i++;
        }
    }

    void update() {
        rotationAngle += GetFrameTime() * 30.0f;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;

        for (int k = 0; k < 10 && k < SLOT_COUNT; k++) {
            int key = (k == 9) ? KEY_ZERO : (KEY_ONE + k);
            if (IsKeyPressed(key)) selected = k;
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            int count = validCount();
            selected -= (int)wheel;
            if (count > 0) {
                if (selected < 0) selected = count - 1;
                if (selected >= count) selected = 0;
            }
        }
    }

    void draw() {
        auto layout = getLayout();
        if (layout.count == 0) return;

        DrawRectangle(layout.startX - 6, layout.startY - 6, layout.totalW + 12, SLOT_SIZE + 12, { 0, 0, 0, 150 });

        for (int i = 0; i < layout.count; i++) {
            int x = layout.slotX(i);
            int y = layout.slotY();

            Color bg = (i == selected) ? Color{ 80, 80, 120, 200 } : Color{ 40, 40, 50, 200 };
            DrawRectangle(x, y, SLOT_SIZE, SLOT_SIZE, bg);

            if (i == selected) {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, WHITE);
            } else {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, { 80, 80, 80, 200 });
            }

            if (slots[i].valid) {
                renderSlotBlock(slots[i].name);
                int pad = (SLOT_SIZE - THUMB_SIZE) / 2;
                Rectangle src = { 0, 0, (float)RENDER_SIZE, -(float)RENDER_SIZE };
                Rectangle dst = { (float)(x + pad), (float)(y + pad), (float)THUMB_SIZE, (float)THUMB_SIZE };
                DrawTexturePro(slotRT.texture, src, dst, {0,0}, 0.0f, WHITE);
            }
        }

        if (selected >= 0 && selected < layout.count && slots[selected].valid) {
            const char* name = slots[selected].name.c_str();
            int nameW = MeasureText(name, 16);
            DrawText(name, (GetScreenWidth() - nameW) / 2, layout.startY - 22, 16, WHITE);
        }
    }

    const std::string& getSelectedName() {
        static std::string empty;
        if (selected >= 0 && selected < SLOT_COUNT && slots[selected].valid)
            return slots[selected].name;
        return empty;
    }

    void unload() {
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (slots[i].thumb.id > 0) UnloadTexture(slots[i].thumb);
            slots[i] = {};
        }
        if (slotRT.id != 0) { UnloadRenderTexture(slotRT); slotRT = { 0 }; }
    }

private:
    void renderSlotBlock(const std::string& name) {
        if (!blocksRef) return;
        auto it = blocksRef->entries.find(name);
        if (it == blocksRef->entries.end()) return;
        BlockDef& def = it->second;

        float yawRad = rotationAngle * DEG2RAD;
        float pitchRad = 25.0f * DEG2RAD;
        float camDist = 2.4f;

        Camera3D cam = {};
        cam.position = {
            camDist * cosf(pitchRad) * sinf(yawRad),
            camDist * sinf(pitchRad),
            camDist * cosf(pitchRad) * cosf(yawRad)
        };
        cam.target = { 0.0f, 0.0f, 0.0f };
        cam.up = { 0.0f, 1.0f, 0.0f };
        cam.fovy = 30.0f;
        cam.projection = CAMERA_PERSPECTIVE;

        // GLB models have origin at base-middle, so center the camera on the model's midpoint
        if (def.formatType == "glb" && def.modelLoaded) {
            BoundingBox bb = GetModelBoundingBox(def.model);
            float midY = (bb.min.y + bb.max.y) * 0.5f * def.modelScaleY;
            cam.target = { 0.0f, midY, 0.0f };
            cam.position.y = midY + camDist * sinf(pitchRad);
        }

        BeginTextureMode(slotRT);
            ClearBackground(BLANK);
            BeginMode3D(cam);
                if (def.formatType == "glb" && def.modelLoaded) {
                    DrawModelEx(def.model, { 0, 0, 0 },
                        { 0, 1, 0 }, 0.0f,
                        { def.modelScaleX, def.modelScaleY, def.modelScaleZ },
                        WHITE);
                } else {
                    Color col = { def.r, def.g, def.b, def.a };
                    DrawCube({ 0, 0, 0 }, 1.0f, 1.0f, 1.0f, col);
                    DrawCubeWires({ 0, 0, 0 }, 1.0f, 1.0f, 1.0f, {
                        (unsigned char)(def.r / 2), (unsigned char)(def.g / 2),
                        (unsigned char)(def.b / 2), 255 });
                }
            EndMode3D();
        EndTextureMode();
    }
};

// ── Block Inventory (overflow panel) ─────────────────────────────────

struct BlockInventory {
    static const int SLOT_SIZE  = 54;
    static const int SLOT_PAD   = 4;
    static const int THUMB_SIZE = 48;
    static const int COLS       = 8;

    struct Slot {
        std::string name;
        Texture2D thumb = { 0 };
    };

    std::vector<Slot> slots;
    bool open = false;
    int highlighted = -1;

    void load(Registry<BlockDef>& blocks, BlockHotbar& hotbar) {
        for (auto& [name, def] : blocks.entries) {
            bool inHotbar = false;
            for (int i = 0; i < BlockHotbar::SLOT_COUNT; i++) {
                if (hotbar.slots[i].valid && hotbar.slots[i].name == name) {
                    inHotbar = true;
                    break;
                }
            }
            if (!inHotbar) {
                Slot s;
                s.name  = name;
                s.thumb = renderBlockThumbnail(def, THUMB_SIZE);
                slots.push_back(s);
            }
        }
    }

    bool update(BlockHotbar& hotbar) {
        if (IsKeyPressed(KEY_E)) {
            open = !open;
            if (!open) highlighted = -1;
        }

        if (!open) return false;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();

            int invSlot = getSlotAt(mouse);
            if (invSlot >= 0) {
                highlighted = (highlighted == invSlot) ? -1 : invSlot;
                return true;
            }

            if (highlighted >= 0) {
                auto layout = hotbar.getLayout();
                int hotSlot = layout.hitTest(mouse);
                if (hotSlot >= 0) {
                    std::swap(slots[highlighted].name, hotbar.slots[hotSlot].name);
                    std::swap(slots[highlighted].thumb, hotbar.slots[hotSlot].thumb);
                    highlighted = -1;
                    return true;
                }
            }
        }

        return false;
    }

    void draw() {
        if (!open) return;

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int rows = ((int)slots.size() + COLS - 1) / COLS;
        int gridW = COLS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int gridH = rows * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;

        int panelW = gridW + 24;
        int panelH = gridH + 48;
        int panelX = (screenW - panelW) / 2;
        int panelY = (screenH - panelH) / 2 - 40;

        DrawRectangle(panelX, panelY, panelW, panelH, { 15, 15, 20, 230 });
        DrawRectangleLines(panelX, panelY, panelW, panelH, { 80, 80, 80, 200 });

        DrawText("Block Inventory", panelX + 12, panelY + 8, 20, WHITE);

        int startX = panelX + 12;
        int startY = panelY + 36;

        for (int i = 0; i < (int)slots.size(); i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = startX + col * (SLOT_SIZE + SLOT_PAD);
            int y = startY + row * (SLOT_SIZE + SLOT_PAD);

            Color bg = (i == highlighted) ? Color{ 100, 80, 40, 220 } : Color{ 40, 40, 50, 200 };
            DrawRectangle(x, y, SLOT_SIZE, SLOT_SIZE, bg);

            if (i == highlighted) {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, GOLD);
            } else {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, { 80, 80, 80, 200 });
            }

            if (slots[i].thumb.id > 0) {
                int pad = (SLOT_SIZE - THUMB_SIZE) / 2;
                DrawTexture(slots[i].thumb, x + pad, y + pad, WHITE);
            }
        }

        if (highlighted >= 0 && highlighted < (int)slots.size()) {
            const char* name = slots[highlighted].name.c_str();
            int nameW = MeasureText(name, 16);
            DrawText(name, (screenW - nameW) / 2, panelY + panelH + 6, 16, GOLD);
        }

        DrawText("Click block, then click hotbar slot to swap  |  TAB to close",
                 panelX, panelY + panelH + 24, 14, GRAY);
    }

    void unload() {
        for (auto& s : slots) {
            if (s.thumb.id > 0) UnloadTexture(s.thumb);
        }
        slots.clear();
    }

private:
    // Returns panel layout info for hit testing
    struct PanelLayout {
        int startX, startY, panelX, panelY, panelW, panelH;
    };

    PanelLayout getPanelLayout() const {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int rows = ((int)slots.size() + COLS - 1) / COLS;
        int gridW = COLS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int gridH = rows * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int panelW = gridW + 24;
        int panelH = gridH + 48;
        int panelX = (screenW - panelW) / 2;
        int panelY = (screenH - panelH) / 2 - 40;
        return { panelX + 12, panelY + 36, panelX, panelY, panelW, panelH };
    }

    int getSlotAt(Vector2 mouse) const {
        auto pl = getPanelLayout();
        for (int i = 0; i < (int)slots.size(); i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = pl.startX + col * (SLOT_SIZE + SLOT_PAD);
            int y = pl.startY + row * (SLOT_SIZE + SLOT_PAD);
            if (mouse.x >= x && mouse.x < x + SLOT_SIZE &&
                mouse.y >= y && mouse.y < y + SLOT_SIZE) {
                return i;
            }
        }
        return -1;
    }
};
