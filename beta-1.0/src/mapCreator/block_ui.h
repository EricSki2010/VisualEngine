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
            // Compute bounding box to frame the model properly
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

// ── Block Hotbar ────────────────────────────────────────────────────

struct BlockHotbar {
    static const int SLOT_COUNT = 20;
    static const int SLOT_SIZE  = 54;
    static const int SLOT_PAD   = 4;
    static const int THUMB_SIZE = 48;

    struct Slot {
        std::string name;
        Texture2D thumb = { 0 };
        bool valid = false;
    };

    Slot slots[SLOT_COUNT] = {};
    int selected = 0;
    int scrollOffset = 0;

    void load(Registry<BlockDef>& blocks) {
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
        for (int k = 0; k < 10 && k < SLOT_COUNT; k++) {
            int key = (k == 9) ? KEY_ZERO : (KEY_ONE + k);
            if (IsKeyPressed(key)) selected = k;
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            selected -= (int)wheel;
            int count = 0;
            for (int i = 0; i < SLOT_COUNT; i++) if (slots[i].valid) count++;
            if (count > 0) {
                if (selected < 0) selected = count - 1;
                if (selected >= count) selected = 0;
            }
        }
    }

    void draw() {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        int count = 0;
        for (int i = 0; i < SLOT_COUNT; i++) if (slots[i].valid) count++;
        if (count == 0) return;

        int totalW = count * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int startX = (screenW - totalW) / 2;
        int startY = screenH - SLOT_SIZE - 12;

        DrawRectangle(startX - 6, startY - 6, totalW + 12, SLOT_SIZE + 12, { 0, 0, 0, 150 });

        for (int i = 0; i < count; i++) {
            int x = startX + i * (SLOT_SIZE + SLOT_PAD);
            int y = startY;

            Color bg = (i == selected) ? Color{ 80, 80, 120, 200 } : Color{ 40, 40, 50, 200 };
            DrawRectangle(x, y, SLOT_SIZE, SLOT_SIZE, bg);

            if (i == selected) {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, WHITE);
            } else {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, { 80, 80, 80, 200 });
            }

            if (slots[i].valid && slots[i].thumb.id > 0) {
                int pad = (SLOT_SIZE - THUMB_SIZE) / 2;
                DrawTexture(slots[i].thumb, x + pad, y + pad, WHITE);
            }
        }

        if (selected >= 0 && selected < count && slots[selected].valid) {
            const char* name = slots[selected].name.c_str();
            int nameW = MeasureText(name, 16);
            DrawText(name, (screenW - nameW) / 2, startY - 22, 16, WHITE);
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
                int hotSlot = getHotbarSlotAt(mouse, hotbar);
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
    int getSlotAt(Vector2 mouse) {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int rows = ((int)slots.size() + COLS - 1) / COLS;
        int gridW = COLS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int gridH = rows * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int panelW = gridW + 24;
        int panelH = gridH + 48;
        int panelX = (screenW - panelW) / 2;
        int panelY = (screenH - panelH) / 2 - 40;
        int startX = panelX + 12;
        int startY = panelY + 36;

        for (int i = 0; i < (int)slots.size(); i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = startX + col * (SLOT_SIZE + SLOT_PAD);
            int y = startY + row * (SLOT_SIZE + SLOT_PAD);
            if (mouse.x >= x && mouse.x < x + SLOT_SIZE &&
                mouse.y >= y && mouse.y < y + SLOT_SIZE) {
                return i;
            }
        }
        return -1;
    }

    int getHotbarSlotAt(Vector2 mouse, BlockHotbar& hotbar) {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int count = 0;
        for (int i = 0; i < BlockHotbar::SLOT_COUNT; i++)
            if (hotbar.slots[i].valid) count++;
        if (count == 0) return -1;

        int totalW = count * (BlockHotbar::SLOT_SIZE + BlockHotbar::SLOT_PAD) - BlockHotbar::SLOT_PAD;
        int startX = (screenW - totalW) / 2;
        int startY = screenH - BlockHotbar::SLOT_SIZE - 12;

        for (int i = 0; i < count; i++) {
            int x = startX + i * (BlockHotbar::SLOT_SIZE + BlockHotbar::SLOT_PAD);
            int y = startY;
            if (mouse.x >= x && mouse.x < x + BlockHotbar::SLOT_SIZE &&
                mouse.y >= y && mouse.y < y + BlockHotbar::SLOT_SIZE) {
                return i;
            }
        }
        return -1;
    }
};
