#pragma once
#include "engine/engine.h"
#include "engine/gameplay/registry.h"
#include "rlgl.h"
#include "gradient_sky.h"
#include "editor_camera.h"
#include "placed_blocks.h"
#include "menu_ui.h"
#include "save_system.h"
#include "glb_export.h"
#include <cmath>
#include <string>
#include <set>

struct ModelEditor {
    bool escMenuOpen = false;

    // ── Hotbar ─────────────────────────────────────────────
    static const int HOTBAR_SLOTS = 10;
    static const int SLOT_SIZE = 54;
    static const int SLOT_PAD = 4;

    HotbarSlotData slots[HOTBAR_SLOTS] = {};
    int selectedSlot = 0;

    // ── Wireframe toggle ──────────────────────────────────
    bool showWireframe = true;

    // ── Export state ───────────────────────────────────────
    bool exportOpen = false;
    std::string exportName;
    std::string exportMessage; // success/error feedback
    float exportMsgTimer = 0.0f;

    // ── Right-click drag ──────────────────────────────────
    bool rightDragActive = false;
    bool rightDragMoved = false;
    Vector2 rightDragStart = {0, 0};
    int dragLockedFace = -1; // face direction locked on drag start

    struct DragHit { Vec3i offset; int face; };
    std::vector<DragHit> dragHighlights;
    Vector2 lastDragMousePos = {0, 0};

    // ── Color Wheel ────────────────────────────────────────
    bool wheelOpen = false;
    float wheelBrightness = 1.0f;
    Color pickedColor = WHITE;
    bool hasPickedColor = false;

    // Hex/RGBA text input in color wheel
    char hexInput[8] = "#ffffff";   // includes '#'
    char rgbaInput[3][4] = {"255","255","255"}; // R, G, B fields
    int hexCursor = 7;              // cursor position in hex field
    int activeRgbaField = -1;       // which RGB field is being edited (-1 = none, 0-2)
    bool hexFieldActive = false;

    // ── Eyedropper tool ─────────────────────────────────────
    bool eyedropperActive = false;

    static std::string colorToHex(Color c) {
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
        return buf;
    }

    static Color hexToColor(const char* hex) {
        unsigned int r = 0, g = 0, b = 0;
        const char* start = (hex[0] == '#') ? hex + 1 : hex;
        if (sscanf(start, "%02x%02x%02x", &r, &g, &b) == 3)
            return {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
        return WHITE;
    }

    void syncHexFromColor(Color c) {
        snprintf(hexInput, sizeof(hexInput), "#%02x%02x%02x", c.r, c.g, c.b);
        snprintf(rgbaInput[0], 4, "%d", c.r);
        snprintf(rgbaInput[1], 4, "%d", c.g);
        snprintf(rgbaInput[2], 4, "%d", c.b);
    }

    void reset() {
        escMenuOpen = false;
        showWireframe = true;
        rightDragActive = false;
        rightDragMoved = false;
        exportOpen = false;
        exportName.clear();
        exportMessage.clear();
        exportMsgTimer = 0.0f;
        wheelOpen = false;
        hasPickedColor = false;
        eyedropperActive = false;
        hexFieldActive = false;
        activeRgbaField = -1;
        for (int i = 0; i < HOTBAR_SLOTS; i++) slots[i] = {};
        selectedSlot = 0;
    }

    // ────────────────────────────────────────────────────────
    // Private helper methods (extracted from frame())
    // ────────────────────────────────────────────────────────

    void handleInput(float dt) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (exportOpen) { exportOpen = false; }
            else if (wheelOpen) { wheelOpen = false; hexFieldActive = false; activeRgbaField = -1; }
            else { escMenuOpen = !escMenuOpen; }
        }

        // Export text input
        if (exportOpen) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch <= 125 && (int)exportName.size() < 64)
                    exportName += (char)ch;
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !exportName.empty())
                exportName.pop_back();
        }

        if (exportMsgTimer > 0.0f) exportMsgTimer -= dt;

        if (IsKeyPressed(KEY_E) && !escMenuOpen && !exportOpen && !hexFieldActive && activeRgbaField < 0) {
            wheelOpen = !wheelOpen;
            hasPickedColor = false;
            hexFieldActive = false;
            activeRgbaField = -1;
        }

        // Hotbar slot selection (number keys + scroll)
        if (!escMenuOpen && !wheelOpen && !exportOpen) {
            for (int k = 0; k < 10; k++) {
                int key = (k == 9) ? KEY_ZERO : (KEY_ONE + k);
                if (IsKeyPressed(key)) selectedSlot = k;
            }
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                selectedSlot -= (int)wheel;
                if (selectedSlot < 0) selectedSlot = HOTBAR_SLOTS - 1;
                if (selectedSlot >= HOTBAR_SLOTS) selectedSlot = 0;
            }
        }
    }

    void updateLogic(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
                     std::string& savePath, std::string& structureName,
                     std::string& structureType, float& autoSaveTimer) {
        // Auto-save
        autoSaveTimer += dt;
        if (autoSaveTimer >= 10.0f) {
            saveStructure(savePath, structureName, structureType, placedBlocks, editorCam,
                          slots, HOTBAR_SLOTS);
            autoSaveTimer = 0.0f;
        }

        editorCam.update(dt);

        // Hover raycast
        Ray ray = GetScreenToWorldRay(GetMousePosition(), editorCam.cam);
        placedBlocks.hover = placedBlocks.raycastBlocks(ray);
    }

    void handleBlockRemoval(PlacedBlocks& placedBlocks) {
        if (IsKeyReleased(KEY_X) && !rightDragMoved && placedBlocks.hover.hit) {
            Vec3i offset = placedBlocks.toOffset(placedBlocks.hover.pos);
            if (placedBlocks.hasBlockAt(offset)) {
                placedBlocks.removed.insert(offset);
                placedBlocks.matrix.erase(offset.x, offset.y, offset.z);
                placedBlocks.markDirty();
            }
        }
    }

    void handleEyedropper(PlacedBlocks& placedBlocks) {
        if (!eyedropperActive) return;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && placedBlocks.hover.hit) {
            Vec3i offset = placedBlocks.toOffset(placedBlocks.hover.pos);
            const std::string& blockName = placedBlocks.getBlockName(offset);
            if (!blockName.empty()) {
                Color faces[6];
                PlacedBlocks::decodeFaceColors(blockName, faces);
                int face = PlacedBlocks::faceFromNormal(placedBlocks.hover.normal);
                pickedColor = faces[face];
                hasPickedColor = true;
                eyedropperActive = false;
                syncHexFromColor(pickedColor);
            }
        }
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            eyedropperActive = false;
        }
    }

    void handleOriginPick(EditorCamera& editorCam, PlacedBlocks& placedBlocks) {
        if (placedBlocks.modelDims.x <= 0) return; // only for 3D Model mode
        if (!IsKeyPressed(KEY_O) || !placedBlocks.hover.hit) return;

        // Place origin at the center of the hovered face
        Vec3i hitPos = placedBlocks.hover.pos;
        Vector3 n = placedBlocks.hover.normal;
        Vec3i offset = placedBlocks.toOffset(hitPos);

        // Face center = block center + normal * 0.5
        Vec3i dir = PlacedBlocks::normalToDir(n);
        placedBlocks.modelOriginPos = {
            (float)offset.x + dir.x * 0.5f,
            (float)offset.y + dir.y * 0.5f,
            (float)offset.z + dir.z * 0.5f
        };
    }

    void updateDragHighlights(EditorCamera& editorCam, PlacedBlocks& placedBlocks) {
        if (rightDragActive && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 cur = GetMousePosition();
            float dx = cur.x - rightDragStart.x;
            float dy = cur.y - rightDragStart.y;
            if (sqrtf(dx*dx + dy*dy) > 5.0f) rightDragMoved = true;

            // Skip recomputation if mouse hasn't moved significantly
            float mdx = cur.x - lastDragMousePos.x;
            float mdy = cur.y - lastDragMousePos.y;
            if (sqrtf(mdx*mdx + mdy*mdy) < 2.0f) return;
            lastDragMousePos = cur;

            // Build highlight list while dragging (locked face direction)
            dragHighlights.clear();
            bool xHeld = IsKeyDown(KEY_X);
            bool canPaint = selectedSlot > 0 && slots[selectedSlot].hasColor;
            bool canRestore = selectedSlot == 0 && placedBlocks.isModelMode();
            if (rightDragMoved && dragLockedFace >= 0 && (canPaint || xHeld || canRestore)) {
                float minX = fminf(rightDragStart.x, cur.x);
                float maxX = fmaxf(rightDragStart.x, cur.x);
                float minY = fminf(rightDragStart.y, cur.y);
                float maxY = fmaxf(rightDragStart.y, cur.y);

                const Vec3i* faceDirs = PlacedBlocks::DIRS6;
                Vec3i faceDir = faceDirs[dragLockedFace];

                Vector3 camPos = editorCam.cam.position;
                Vector3 camTarget = editorCam.cam.target;
                Vector3 camFwd = {camTarget.x-camPos.x, camTarget.y-camPos.y, camTarget.z-camPos.z};
                float fwdLen = sqrtf(camFwd.x*camFwd.x + camFwd.y*camFwd.y + camFwd.z*camFwd.z);
                if (fwdLen > 0) { camFwd.x/=fwdLen; camFwd.y/=fwdLen; camFwd.z/=fwdLen; }

                auto surfacePositions = placedBlocks.allWorldPositions();
                for (auto& w : surfacePositions) {
                    Vec3i ph = placedBlocks.toOffset(w);
                    Vec3i neighbor = {ph.x+faceDir.x, ph.y+faceDir.y, ph.z+faceDir.z};
                    if (placedBlocks.hasBlockAt(neighbor)) continue;

                    float bx = (float)w.x, by = (float)w.y, bz = (float)w.z;

                    // Skip blocks behind the camera
                    Vector3 toBlock = {bx-camPos.x, by-camPos.y, bz-camPos.z};
                    if (toBlock.x*camFwd.x + toBlock.y*camFwd.y + toBlock.z*camFwd.z < 0.0f) continue;

                    // Project all 8 corners to get screen bounding box
                    float sMinX = 99999, sMaxX = -99999, sMinY = 99999, sMaxY = -99999;
                    for (int ci = 0; ci < 8; ci++) {
                        Vector3 corner = {
                            bx + ((ci & 1) ? 0.5f : -0.5f),
                            by + ((ci & 2) ? 0.5f : -0.5f),
                            bz + ((ci & 4) ? 0.5f : -0.5f)
                        };
                        Vector2 sp = GetWorldToScreen(corner, editorCam.cam);
                        if (sp.x < sMinX) sMinX = sp.x;
                        if (sp.x > sMaxX) sMaxX = sp.x;
                        if (sp.y < sMinY) sMinY = sp.y;
                        if (sp.y > sMaxY) sMaxY = sp.y;
                    }

                    if (sMaxX >= minX && sMinX <= maxX &&
                        sMaxY >= minY && sMinY <= maxY) {
                        dragHighlights.push_back({ph, dragLockedFace});
                    }
                }
            }
        }
    }

    void handlePaintInput(PlacedBlocks& placedBlocks) {
        // Right-click: start drag tracking
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            rightDragStart = GetMousePosition();
            lastDragMousePos = rightDragStart;
            rightDragActive = true;
            rightDragMoved = false;
            dragLockedFace = placedBlocks.hover.hit ?
                PlacedBlocks::faceFromNormal(placedBlocks.hover.normal) : -1;
        }

        // Right-click release: apply drag-fill or single-click paint
        if (rightDragActive && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            bool xHeld = IsKeyDown(KEY_X);
            if (rightDragMoved && xHeld) {
                // Drag remove: delete all highlighted blocks
                for (auto& dh : dragHighlights) {
                    if (placedBlocks.hasBlockAt(dh.offset)) {
                        placedBlocks.removed.insert(dh.offset);
                        placedBlocks.matrix.erase(dh.offset.x, dh.offset.y, dh.offset.z);
                    }
                }
                placedBlocks.markDirty();
                dragHighlights.clear();
            } else if (rightDragMoved && selectedSlot > 0 && slots[selectedSlot].hasColor) {
                // Drag fill: apply paint to all highlighted blocks
                Color paintColor = slots[selectedSlot].color;
                for (auto& dh : dragHighlights) {
                    Color faces[6];
                    const std::string& existing = placedBlocks.getBlockName(dh.offset);
                    if (!existing.empty()) {
                        PlacedBlocks::decodeFaceColors(existing, faces);
                    } else {
                        Color grey = GRAY;
                        for (int i = 0; i < 6; i++) faces[i] = grey;
                    }
                    faces[dh.face] = paintColor;
                    placedBlocks.setBlock(dh.offset, PlacedBlocks::encodeFaceColors(faces));
                }
                dragHighlights.clear();
            } else if (rightDragMoved && selectedSlot == 0 && placedBlocks.isModelMode()) {
                // Drag restore: restore removed blocks adjacent to highlighted surface
                Vec3i faceDir = PlacedBlocks::DIRS6[dragLockedFace];
                for (auto& dh : dragHighlights) {
                    Vec3i target = {dh.offset.x + faceDir.x, dh.offset.y + faceDir.y, dh.offset.z + faceDir.z};
                    if (placedBlocks.removed.count(target)) {
                        placedBlocks.removed.erase(target);
                    }
                }
                placedBlocks.markDirty();
                dragHighlights.clear();
            } else if (!rightDragMoved) {
                // Single click paint
                if (placedBlocks.hover.hit) {
                    if (selectedSlot == 0) {
                        // Restore a removed block
                        Vec3i dir = placedBlocks.normalToDir(placedBlocks.hover.normal);
                        Vec3i newWorld = {
                            placedBlocks.hover.pos.x + dir.x,
                            placedBlocks.hover.pos.y + dir.y,
                            placedBlocks.hover.pos.z + dir.z
                        };
                        Vec3i newOffset = placedBlocks.toOffset(newWorld);
                        if (placedBlocks.removed.count(newOffset)) {
                            placedBlocks.removed.erase(newOffset);
                            placedBlocks.markDirty();
                        }
                    } else if (slots[selectedSlot].hasColor) {
                        Vec3i hitOffset = placedBlocks.toOffset(placedBlocks.hover.pos);
                        int face = PlacedBlocks::faceFromNormal(placedBlocks.hover.normal);
                        Color paintColor = slots[selectedSlot].color;

                        if (placedBlocks.hasBlockAt(hitOffset)) {
                            Color faces[6];
                            const std::string& existing = placedBlocks.getBlockName(hitOffset);
                            if (!existing.empty()) {
                                PlacedBlocks::decodeFaceColors(existing, faces);
                            } else {
                                Color grey = GRAY;
                                for (int i = 0; i < 6; i++) faces[i] = grey;
                            }
                            faces[face] = paintColor;
                            placedBlocks.setBlock(hitOffset, PlacedBlocks::encodeFaceColors(faces));
                        }
                    }
                }
            }
            rightDragActive = false;
            dragHighlights.clear();
        }

        if (!rightDragActive && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            dragHighlights.clear();
        }
    }

    void drawScene(EditorCamera& editorCam, PlacedBlocks& placedBlocks,
                   Registry<BlockDef>& blocks, GradientSky& sky) {
        sky.draw(editorCam.pitch);

        BeginMode3D(editorCam.cam);
            placedBlocks.draw(blocks, editorCam.cam, showWireframe);
            DrawSphere(editorCam.origin, 0.1f, RED);

            // Draw blue sphere at model origin (3D Model mode)
            if (placedBlocks.modelDims.x > 0) {
                Vec3i wo = placedBlocks.getOrigin();
                Vector3 originWorld = {
                    (float)wo.x + placedBlocks.modelOriginPos.x,
                    (float)wo.y + placedBlocks.modelOriginPos.y,
                    (float)wo.z + placedBlocks.modelOriginPos.z
                };
                DrawSphere(originWorld, 0.12f, BLUE);
            }

            // Draw yellow face outlines on drag-highlighted blocks
            for (auto& dh : dragHighlights) {
                Vec3i w = placedBlocks.toWorld(dh.offset);
                Vector3 faceNormals[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                placedBlocks.drawFaceOutline(w, faceNormals[dh.face]);
            }
        EndMode3D();

        // Coordinates
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f",
                     editorCam.origin.x, editorCam.origin.y, editorCam.origin.z);
            DrawText(buf, 10, 10, 20, GREEN);
        }

        if (placedBlocks.modelDims.x > 0) {
            DrawText("3D Model", 10, 34, 16, LIGHTGRAY);
        } else {
            DrawText("3D Block", 10, 34, 16, LIGHTGRAY);
        }
    }

    void drawHotbar() {
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int totalW = HOTBAR_SLOTS * (SLOT_SIZE + SLOT_PAD) - SLOT_PAD;
        int startX = (screenW - totalW) / 2;
        int startY = screenH - SLOT_SIZE - 12;

        DrawRectangle(startX - 6, startY - 6, totalW + 12, SLOT_SIZE + 12, {0,0,0,150});

        for (int i = 0; i < HOTBAR_SLOTS; i++) {
            int x = startX + i * (SLOT_SIZE + SLOT_PAD);
            int y = startY;

            Color bg = (i == selectedSlot) ? Color{80,80,120,200} : Color{40,40,50,200};
            DrawRectangle(x, y, SLOT_SIZE, SLOT_SIZE, bg);

            if (i == selectedSlot) {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, WHITE);
            } else {
                DrawRectangleLines(x, y, SLOT_SIZE, SLOT_SIZE, {80,80,80,200});
            }

            // Slot content
            int pad = 6;
            int inner = SLOT_SIZE - pad * 2;
            if (i == 0) {
                DrawRectangle(x + pad, y + pad, inner, inner, GRAY);
                DrawRectangleLines(x + pad, y + pad, inner, inner, DARKGRAY);
            } else if (slots[i].hasColor) {
                DrawRectangle(x + pad, y + pad, inner, inner, slots[i].color);
            }
        }

        // Selected slot name
        const char* slotName = (selectedSlot == 0) ? "Grey Block" :
            slots[selectedSlot].hasColor ? "Color Block" : "Empty";
        int nameW = MeasureText(slotName, 16);
        DrawText(slotName, (screenW - nameW) / 2, startY - 22, 16, WHITE);

        // Wireframe toggle slot (right of hotbar)
        {
            int wireX = startX + totalW + 14;
            int wireY = startY;
            Color wireBg = showWireframe ? Color{80,80,120,200} : Color{40,40,50,200};
            DrawRectangle(wireX, wireY, SLOT_SIZE, SLOT_SIZE, wireBg);
            DrawRectangleLines(wireX, wireY, SLOT_SIZE, SLOT_SIZE,
                showWireframe ? WHITE : Color{80,80,80,200});

            // Draw a wireframe cube icon
            int p = 12;
            int s = SLOT_SIZE - p * 2;
            DrawRectangleLines(wireX + p, wireY + p, s, s, showWireframe ? WHITE : GRAY);
            int o = 6;
            DrawLine(wireX+p, wireY+p, wireX+p+o, wireY+p-o, showWireframe ? WHITE : GRAY);
            DrawLine(wireX+p+s, wireY+p, wireX+p+s+o, wireY+p-o, showWireframe ? WHITE : GRAY);
            DrawLine(wireX+p+s, wireY+p+s, wireX+p+s+o, wireY+p+s-o, showWireframe ? WHITE : GRAY);

            // Click to toggle
            Vector2 m = GetMousePosition();
            if (m.x >= wireX && m.x < wireX + SLOT_SIZE &&
                m.y >= wireY && m.y < wireY + SLOT_SIZE) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    showWireframe = !showWireframe;
            }

            // Label
            const char* wLabel = showWireframe ? "Wireframe: ON" : "Wireframe: OFF";
            int wLabelW = MeasureText(wLabel, 14);
            DrawText(wLabel, wireX + SLOT_SIZE/2 - wLabelW/2, wireY - 18, 14, LIGHTGRAY);

            // ── Eyedropper button (right of wireframe) ──
            int eyeX = wireX + SLOT_SIZE + 10;
            int eyeY = wireY;
            Color eyeBg = eyedropperActive ? Color{80,120,80,200} : Color{40,40,50,200};
            DrawRectangle(eyeX, eyeY, SLOT_SIZE, SLOT_SIZE, eyeBg);
            DrawRectangleLines(eyeX, eyeY, SLOT_SIZE, SLOT_SIZE,
                eyedropperActive ? WHITE : Color{80,80,80,200});

            // Draw eyedropper icon (simple dropper shape)
            Color iconCol = eyedropperActive ? WHITE : GRAY;
            int cx = eyeX + SLOT_SIZE/2, cy = eyeY + SLOT_SIZE/2;
            // Dropper body (diagonal line)
            DrawLine(cx - 8, cy + 8, cx + 6, cy - 6, iconCol);
            DrawLine(cx - 7, cy + 8, cx + 7, cy - 6, iconCol);
            // Dropper tip
            DrawCircle(cx - 8, cy + 10, 3, iconCol);
            // Dropper top
            DrawRectangle(cx + 4, cy - 12, 8, 8, iconCol);

            Vector2 m2 = GetMousePosition();
            if (m2.x >= eyeX && m2.x < eyeX + SLOT_SIZE &&
                m2.y >= eyeY && m2.y < eyeY + SLOT_SIZE) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    eyedropperActive = !eyedropperActive;
                    if (eyedropperActive) hasPickedColor = false;
                }
            }

            const char* eLabel = eyedropperActive ? "Dropper: ON" : "Dropper";
            int eLabelW = MeasureText(eLabel, 14);
            DrawText(eLabel, eyeX + SLOT_SIZE/2 - eLabelW/2, eyeY - 18, 14, LIGHTGRAY);
        }

        // Click hotbar slots to assign picked color
        if (hasPickedColor && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            for (int i = 1; i < HOTBAR_SLOTS; i++) {
                int x = startX + i * (SLOT_SIZE + SLOT_PAD);
                int y = startY;
                if (mouse.x >= x && mouse.x < x + SLOT_SIZE &&
                    mouse.y >= y && mouse.y < y + SLOT_SIZE) {
                    slots[i].hasColor = true;
                    slots[i].color = pickedColor;
                    hasPickedColor = false;
                    break;
                }
            }
        }
    }

    void drawColorWheel() {
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int panelW = 400, panelH = 490;
        int panelX = (sW - panelW) / 2, panelY = (sH - panelH) / 2;
        DrawRectangle(panelX, panelY, panelW, panelH, {20,20,28,240});
        DrawRectangleLines(panelX, panelY, panelW, panelH, {80,80,80,200});

        int tw = MeasureText("Color Wheel", 26);
        DrawText("Color Wheel", (sW - tw) / 2, panelY + 12, 26, WHITE);

        int wheelCX = panelX + panelW / 2 - 30;
        int wheelCY = panelY + 210;
        int wheelR = 130;

        // Draw wheel from cached texture
        {
            static Texture2D wheelTex = { 0 };
            static float cachedBrightness = -1.0f;
            static int cachedR = 0;

            if (wheelTex.id == 0 || cachedBrightness != wheelBrightness || cachedR != wheelR) {
                if (wheelTex.id != 0) UnloadTexture(wheelTex);
                int size = wheelR * 2 + 1;
                Image img = GenImageColor(size, size, BLANK);
                for (int py = -wheelR; py <= wheelR; py++) {
                    for (int px = -wheelR; px <= wheelR; px++) {
                        float dist = sqrtf((float)(px*px + py*py));
                        if (dist > wheelR) continue;
                        float hue = atan2f((float)py, (float)px) * RAD2DEG;
                        if (hue < 0) hue += 360.0f;
                        float sat = dist / (float)wheelR;
                        Color c = ColorFromHSV(hue, sat, wheelBrightness);
                        ImageDrawPixel(&img, px + wheelR, py + wheelR, c);
                    }
                }
                wheelTex = LoadTextureFromImage(img);
                UnloadImage(img);
                cachedBrightness = wheelBrightness;
                cachedR = wheelR;
            }

            DrawTexture(wheelTex, wheelCX - wheelR, wheelCY - wheelR, WHITE);
        }

        // Wheel border
        DrawCircleLines(wheelCX, wheelCY, (float)wheelR, {80,80,80,200});

        // Brightness slider (right side)
        int sliderX = wheelCX + wheelR + 30;
        int sliderY = wheelCY - wheelR;
        int sliderW = 24;
        int sliderH = wheelR * 2;

        // Draw gradient bar
        for (int y = 0; y < sliderH; y++) {
            float val = 1.0f - (float)y / (float)sliderH;
            Color sc = ColorFromHSV(0, 0.0f, val);
            DrawRectangle(sliderX, sliderY + y, sliderW, 1, sc);
        }
        DrawRectangleLines(sliderX, sliderY, sliderW, sliderH, {80,80,80,200});

        // Slider handle
        int handleY = sliderY + (int)((1.0f - wheelBrightness) * sliderH);
        DrawRectangle(sliderX - 3, handleY - 3, sliderW + 6, 6, WHITE);

        DrawText("Brightness", sliderX - 10, sliderY + sliderH + 8, 14, LIGHTGRAY);

        // Handle slider drag
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            if (m.x >= sliderX - 5 && m.x < sliderX + sliderW + 5 &&
                m.y >= sliderY && m.y < sliderY + sliderH) {
                wheelBrightness = 1.0f - (m.y - sliderY) / (float)sliderH;
                if (wheelBrightness < 0.0f) wheelBrightness = 0.0f;
                if (wheelBrightness > 1.0f) wheelBrightness = 1.0f;
            }
        }

        // Handle wheel click to pick color
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            float dx = m.x - wheelCX;
            float dy = m.y - wheelCY;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist <= wheelR) {
                float hue = atan2f(dy, dx) * RAD2DEG;
                if (hue < 0) hue += 360.0f;
                float sat = dist / (float)wheelR;
                pickedColor = ColorFromHSV(hue, sat, wheelBrightness);
                hasPickedColor = true;
                syncHexFromColor(pickedColor);
            }
        }

        // ── Hex input field ──────────────────────────────────
        {
            int hexLabelX = panelX + 20;
            int hexFieldY = panelY + panelH - 120;
            DrawText("Hex:", hexLabelX, hexFieldY + 6, 16, LIGHTGRAY);

            int hfX = hexLabelX + 40, hfW = 90, hfH = 28;
            Color hfBg = hexFieldActive ? Color{40,40,60,255} : Color{30,30,40,255};
            DrawRectangle(hfX, hexFieldY, hfW, hfH, hfBg);
            DrawRectangleLines(hfX, hexFieldY, hfW, hfH, hexFieldActive ? WHITE : Color{80,80,80,200});
            DrawText(hexInput, hfX + 4, hexFieldY + 5, 18, WHITE);

            if (hexFieldActive && ((int)(GetTime()*2.0)%2)==0) {
                int cx2 = hfX + 4 + MeasureText(hexInput, 18);
                DrawRectangle(cx2, hexFieldY + 4, 2, 20, WHITE);
            }

            // Click to activate hex field
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 m3 = GetMousePosition();
                if (m3.x >= hfX && m3.x < hfX + hfW && m3.y >= hexFieldY && m3.y < hexFieldY + hfH) {
                    hexFieldActive = true;
                    activeRgbaField = -1;
                } else {
                    hexFieldActive = false;
                }
            }

            // Hex text input
            if (hexFieldActive) {
                int ch = GetCharPressed();
                while (ch > 0) {
                    int len = (int)strlen(hexInput);
                    if (len < 7 && ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || ch == '#')) {
                        hexInput[len] = (char)ch;
                        hexInput[len+1] = '\0';
                    }
                    ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(hexInput) > 0)
                    hexInput[strlen(hexInput)-1] = '\0';
                if (IsKeyPressed(KEY_ENTER) && strlen(hexInput) >= 4) {
                    char padded[8] = "#";
                    const char* src = (hexInput[0] == '#') ? hexInput + 1 : hexInput;
                    int slen = (int)strlen(src);
                    if (slen == 3)
                        snprintf(padded, sizeof(padded), "#%c%c%c%c%c%c", src[0],src[0],src[1],src[1],src[2],src[2]);
                    else
                        snprintf(padded, sizeof(padded), "#%.6s", src);
                    pickedColor = hexToColor(padded);
                    hasPickedColor = true;
                    syncHexFromColor(pickedColor);
                    hexFieldActive = false;
                }
            }

            // ── RGB input fields ─────────────────────────────
            int rgbaY = hexFieldY + hfH + 8;
            const char* rgbLabels[] = {"R:", "G:", "B:"};
            int rgbaFW = 40;

            for (int i = 0; i < 3; i++) {
                int fx = hexLabelX + i * (rgbaFW + 24);
                DrawText(rgbLabels[i], fx, rgbaY + 6, 16, LIGHTGRAY);
                int fieldX = fx + 18;
                Color rfBg = (activeRgbaField == i) ? Color{40,40,60,255} : Color{30,30,40,255};
                DrawRectangle(fieldX, rgbaY, rgbaFW, hfH, rfBg);
                DrawRectangleLines(fieldX, rgbaY, rgbaFW, hfH,
                    (activeRgbaField == i) ? WHITE : Color{80,80,80,200});
                DrawText(rgbaInput[i], fieldX + 3, rgbaY + 5, 16, WHITE);

                if (activeRgbaField == i && ((int)(GetTime()*2.0)%2)==0) {
                    int cx3 = fieldX + 3 + MeasureText(rgbaInput[i], 16);
                    DrawRectangle(cx3, rgbaY + 4, 2, 20, WHITE);
                }

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 m4 = GetMousePosition();
                    if (m4.x >= fieldX && m4.x < fieldX + rgbaFW &&
                        m4.y >= rgbaY && m4.y < rgbaY + hfH) {
                        activeRgbaField = i;
                        hexFieldActive = false;
                    }
                }
            }

            // RGB text input
            if (activeRgbaField >= 0 && activeRgbaField < 3) {
                int ch = GetCharPressed();
                while (ch > 0) {
                    int len = (int)strlen(rgbaInput[activeRgbaField]);
                    if (len < 3 && ch >= '0' && ch <= '9') {
                        rgbaInput[activeRgbaField][len] = (char)ch;
                        rgbaInput[activeRgbaField][len+1] = '\0';
                    }
                    ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && strlen(rgbaInput[activeRgbaField]) > 0)
                    rgbaInput[activeRgbaField][strlen(rgbaInput[activeRgbaField])-1] = '\0';
                if (IsKeyPressed(KEY_ENTER)) {
                    int r = std::atoi(rgbaInput[0]); if (r > 255) r = 255;
                    int g = std::atoi(rgbaInput[1]); if (g > 255) g = 255;
                    int b = std::atoi(rgbaInput[2]); if (b > 255) b = 255;
                    pickedColor = {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                    hasPickedColor = true;
                    syncHexFromColor(pickedColor);
                    activeRgbaField = -1;
                }
                if (IsKeyPressed(KEY_TAB))
                    activeRgbaField = (activeRgbaField + 1) % 3;
            }
        }

        // Preview picked color
        if (hasPickedColor) {
            int prevX = panelX + 20;
            int prevY = panelY + panelH - 50;
            DrawRectangle(prevX, prevY, 36, 36, pickedColor);
            DrawRectangleLines(prevX, prevY, 36, 36, WHITE);
            DrawText("Click a hotbar slot to assign", prevX + 44, prevY + 8, 18, LIGHTGRAY);
        } else {
            DrawText("Click the wheel to pick a color", panelX + 20, panelY + panelH - 40, 16, GRAY);
        }
    }

    void drawExportMessage() {
        int sW2 = GetScreenWidth();
        int mw = MeasureText(exportMessage.c_str(), 22);
        DrawRectangle(sW2/2-mw/2-12, 50, mw+24, 34, {0,0,0,200});
        Color mc = (exportMessage[0] == 'E') ? RED : GREEN; // "Exported..." vs "Error..."
        DrawText(exportMessage.c_str(), sW2/2-mw/2, 56, 22, mc);
    }

    bool drawEscMenu(std::string& savePath, std::string& structureName,
                     std::string& structureType, PlacedBlocks& placedBlocks,
                     EditorCamera& editorCam) {
        bool exitToMenu = false;
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int pW = 340, pH = 220;
        int pX = (sW-pW)/2, pY = (sH-pH)/2;
        DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
        DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

        int ptW = MeasureText("Paused", 30);
        DrawText("Paused", (sW-ptW)/2, pY+20, 30, WHITE);

        int bw = 260, bh = 45, bx = (sW-bw)/2, gap = 16, by = pY+70;

        if (drawMenuButton("Save & Exit", bx, by, bw, bh)) {
            saveStructure(savePath, structureName, structureType, placedBlocks, editorCam,
                          slots, HOTBAR_SLOTS);
            placedBlocks.matrix.clear();
            placedBlocks.placeholders = {{0,0,0}};
            placedBlocks.rebuildPlaceholderSet();
            placedBlocks.markDirty();
            placedBlocks.modelSize = 0;
            placedBlocks.removed.clear();
            reset();
            exitToMenu = true;
        }

        if (drawMenuButton("Export as GLB", bx, by+bh+gap, bw, bh)) {
            escMenuOpen = false;
            exportOpen = true;
            exportName.clear();
        }

        return exitToMenu;
    }

    void drawExportDialog(PlacedBlocks& placedBlocks, const std::string& structureType) {
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int pW = 420, pH = 200;
        int pX = (sW-pW)/2, pY = (sH-pH)/2;
        DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
        DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

        int tw = MeasureText("Export as GLB", 26);
        DrawText("Export as GLB", (sW-tw)/2, pY+16, 26, WHITE);

        // Name field
        int fX = pX+20, fY = pY+60, fW = pW-40, fH = 40;
        DrawText("File name:", fX, fY-22, 18, LIGHTGRAY);
        DrawRectangle(fX, fY, fW, fH, {30,30,40,255});
        DrawRectangleLines(fX, fY, fW, fH, WHITE);
        DrawText(exportName.c_str(), fX+8, fY+10, 20, WHITE);

        if (((int)(GetTime()*2.0)%2)==0) {
            int cx = fX+8+MeasureText(exportName.c_str(), 20);
            DrawRectangle(cx, fY+8, 2, 24, WHITE);
        }

        DrawText("Saves to exports/ folder", fX, fY+fH+4, 14, GRAY);

        // Buttons
        int bw2 = 160, bh2 = 42, bgap = 20;
        int bsx = (sW-(bw2*2+bgap))/2, by2 = pY+pH-bh2-20;

        if (drawMenuButton("Cancel", bsx, by2, bw2, bh2)) {
            exportOpen = false;
        }

        bool canExport = !exportName.empty();
        if (drawMenuButton("Export", bsx+bw2+bgap, by2, bw2, bh2, canExport)) {
            if (canExport) {
                std::string safeName = sanitizeFilename(exportName);
                bool ok = false;
                if (structureType == "3D Model") {
                    // 3D Model exports to AbstractModels/ with no block JSON
                    std::string glbPath = getAbstractModelsDir() + "/" + safeName + ".glb";
                    ok = exportModelAsGLB(glbPath, placedBlocks);
                } else {
                    // 3D Block exports to blockModels/ with block JSON
                    std::string glbPath = getBlockModelsDir() + "/" + safeName + ".glb";
                    std::string glbRelPath = "assets/entities/blockModels/" + safeName + ".glb";
                    ok = exportModelAsGLB(glbPath, placedBlocks);
                    if (ok) createBlockJson(safeName, glbRelPath);
                }
                exportMessage = ok ? ("Exported: " + safeName) : "Error: export failed";
                exportMsgTimer = 3.0f;
                exportOpen = false;
                escMenuOpen = false;
            }
        }
    }

    void drawDragRect() {
        if (rightDragActive && rightDragMoved && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 cur = GetMousePosition();
            float rx = fminf(rightDragStart.x, cur.x);
            float ry = fminf(rightDragStart.y, cur.y);
            float rw = fabsf(cur.x - rightDragStart.x);
            float rh = fabsf(cur.y - rightDragStart.y);
            bool xHeld = IsKeyDown(KEY_X);
            Color fillCol = xHeld ? Color{255,60,60,30} : Color{255,255,255,30};
            Color lineCol = xHeld ? RED : WHITE;
            DrawRectangle((int)rx, (int)ry, (int)rw, (int)rh, fillCol);
            DrawRectangleLines((int)rx, (int)ry, (int)rw, (int)rh, lineCol);
        }
    }

    // ────────────────────────────────────────────────────────
    // Main frame orchestrator
    // ────────────────────────────────────────────────────────

    // Returns true if editor wants to exit to menu
    bool frame(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
               GradientSky& sky, Registry<BlockDef>& blocks,
               std::string& savePath, std::string& structureName,
               std::string& structureType, float& autoSaveTimer) {
        bool exitToMenu = false;

        handleInput(dt);
        if (!escMenuOpen && !wheelOpen && !exportOpen) {
            updateLogic(dt, editorCam, placedBlocks, savePath,
                        structureName, structureType, autoSaveTimer);
            if (eyedropperActive) {
                handleEyedropper(placedBlocks);
            } else {
                handleBlockRemoval(placedBlocks);
                handleOriginPick(editorCam, placedBlocks);
                updateDragHighlights(editorCam, placedBlocks);
                handlePaintInput(placedBlocks);
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);
            drawScene(editorCam, placedBlocks, blocks, sky);
            drawHotbar();
            drawDragRect();
            if (placedBlocks.modelDims.x > 0) {
                DrawText("X: Remove  |  Right-Click: Paint  |  Drag: Fill  |  X+Drag: Remove Area  |  O: Set Origin  |  E: Color Wheel", 10, GetScreenHeight() - 30, 16, GRAY);
            } else {
                DrawText("X: Remove  |  Right-Click: Paint  |  Drag: Fill area  |  X+Drag: Remove Area  |  E: Color Wheel", 10, GetScreenHeight() - 30, 16, GRAY);
            }
            if (eyedropperActive) {
                const char* eyeMsg = "EYEDROPPER: Click a block face to sample its color (Right-click/ESC to cancel)";
                int eyeMsgW = MeasureText(eyeMsg, 18);
                DrawRectangle(GetScreenWidth()/2 - eyeMsgW/2 - 10, 10, eyeMsgW + 20, 30, {0,0,0,200});
                DrawText(eyeMsg, GetScreenWidth()/2 - eyeMsgW/2, 16, 18, YELLOW);
            }
            if (wheelOpen)                              drawColorWheel();
            if (exportMsgTimer > 0.0f && !exportMessage.empty()) drawExportMessage();
            if (escMenuOpen && !exportOpen)              exitToMenu = drawEscMenu(savePath, structureName,
                                                                                  structureType, placedBlocks, editorCam);
            if (exportOpen)                              drawExportDialog(placedBlocks, structureType);
        EndDrawing();

        return exitToMenu;
    }
};
