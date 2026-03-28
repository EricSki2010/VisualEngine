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

    struct ColorSlot {
        bool hasColor = false;
        Color color = GRAY;
    };

    ColorSlot slots[HOTBAR_SLOTS] = {};
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

    // ── Color Wheel ────────────────────────────────────────
    bool wheelOpen = false;
    float wheelBrightness = 1.0f;
    Color pickedColor = WHITE;
    bool hasPickedColor = false;

    static std::string colorToHex(Color c) {
        char buf[8];
        snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
        return buf;
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
        for (int i = 0; i < HOTBAR_SLOTS; i++) slots[i] = {};
        selectedSlot = 0;
    }

    // Returns true if editor wants to exit to menu
    bool frame(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
               GradientSky& sky, Registry<BlockDef>& blocks,
               std::string& savePath, std::string& structureName,
               std::string& structureType, float& autoSaveTimer) {
        bool exitToMenu = false;

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (exportOpen) { exportOpen = false; }
            else if (wheelOpen) { wheelOpen = false; }
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

        if (IsKeyPressed(KEY_E) && !escMenuOpen && !exportOpen) {
            wheelOpen = !wheelOpen;
            hasPickedColor = false;
        }

        // Hotbar slot selection (number keys)
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

        // Auto-save
        autoSaveTimer += dt;
        if (autoSaveTimer >= 10.0f) {
            saveStructure(savePath, structureName, structureType, placedBlocks, editorCam);
            autoSaveTimer = 0.0f;
        }

        // ── Update ─────────────────────────────────────────────
        if (!escMenuOpen && !wheelOpen && !exportOpen) {
            editorCam.update(dt);

            // Hover raycast
            {
                Ray ray = GetScreenToWorldRay(GetMousePosition(), editorCam.cam);
                placedBlocks.hover = placedBlocks.raycastBlocks(ray);
            }

            // X to remove a block
            if (IsKeyPressed(KEY_X) && placedBlocks.hover.hit) {
                Vec3i offset = placedBlocks.toOffset(placedBlocks.hover.pos);
                if (placedBlocks.hasBlockAt(offset)) {
                    placedBlocks.removed.insert(offset);
                    placedBlocks.placed.erase(offset);
                }
            }

            // ── Right-click: single paint or drag fill ──────────
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                rightDragStart = GetMousePosition();
                rightDragActive = true;
                rightDragMoved = false;
                dragLockedFace = placedBlocks.hover.hit ?
                    PlacedBlocks::faceFromNormal(placedBlocks.hover.normal) : -1;
            }

            if (rightDragActive && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 cur = GetMousePosition();
                float dx = cur.x - rightDragStart.x;
                float dy = cur.y - rightDragStart.y;
                if (sqrtf(dx*dx + dy*dy) > 5.0f) rightDragMoved = true;

                // Build highlight list while dragging (locked face direction)
                dragHighlights.clear();
                if (rightDragMoved && dragLockedFace >= 0 && selectedSlot > 0 && slots[selectedSlot].hasColor) {
                    float minX = fminf(rightDragStart.x, cur.x);
                    float maxX = fmaxf(rightDragStart.x, cur.x);
                    float minY = fminf(rightDragStart.y, cur.y);
                    float maxY = fmaxf(rightDragStart.y, cur.y);

                    // The neighbor direction for the locked face — blocks with a
                    // neighbor on that side have the face hidden, so skip them.
                    Vec3i faceDirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                    Vec3i faceDir = faceDirs[dragLockedFace];

                    Vector3 camPos = editorCam.cam.position;
                    Vector3 camTarget = editorCam.cam.target;
                    Vector3 camFwd = {camTarget.x-camPos.x, camTarget.y-camPos.y, camTarget.z-camPos.z};
                    float fwdLen = sqrtf(camFwd.x*camFwd.x + camFwd.y*camFwd.y + camFwd.z*camFwd.z);
                    if (fwdLen > 0) { camFwd.x/=fwdLen; camFwd.y/=fwdLen; camFwd.z/=fwdLen; }

                    // Get all surface positions (works for both model and structure mode)
                    auto surfacePositions = placedBlocks.allWorldPositions();
                    for (auto& w : surfacePositions) {
                        Vec3i ph = placedBlocks.toOffset(w);
                        // Skip if this face is covered by a neighbor
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

            if (rightDragActive && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
                if (rightDragMoved && selectedSlot > 0 && slots[selectedSlot].hasColor) {
                    // ── Drag fill: apply paint to all highlighted blocks ──
                    Color paintColor = slots[selectedSlot].color;
                    for (auto& dh : dragHighlights) {
                        Color faces[6];
                        auto it = placedBlocks.placed.find(dh.offset);
                        if (it != placedBlocks.placed.end()) {
                            PlacedBlocks::decodeFaceColors(it->second, faces);
                        } else {
                            Color grey = GRAY;
                            for (int i = 0; i < 6; i++) faces[i] = grey;
                        }
                        faces[dh.face] = paintColor;
                        placedBlocks.placed[dh.offset] = PlacedBlocks::encodeFaceColors(faces);
                    }
                    dragHighlights.clear();
                } else if (!rightDragMoved) {
                    // ── Single click paint ──
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
                            }
                        } else if (slots[selectedSlot].hasColor) {
                            Vec3i hitOffset = placedBlocks.toOffset(placedBlocks.hover.pos);
                            int face = PlacedBlocks::faceFromNormal(placedBlocks.hover.normal);
                            Color paintColor = slots[selectedSlot].color;

                            if (placedBlocks.hasBlockAt(hitOffset)) {
                                Color faces[6];
                                auto it = placedBlocks.placed.find(hitOffset);
                                if (it != placedBlocks.placed.end()) {
                                    PlacedBlocks::decodeFaceColors(it->second, faces);
                                } else {
                                    Color grey = GRAY;
                                    for (int i = 0; i < 6; i++) faces[i] = grey;
                                }
                                faces[face] = paintColor;
                                placedBlocks.placed[hitOffset] = PlacedBlocks::encodeFaceColors(faces);
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

        // ── Draw ───────────────────────────────────────────────
        BeginDrawing();
            ClearBackground(BLACK);
            sky.draw(editorCam.pitch);

            BeginMode3D(editorCam.cam);
                placedBlocks.draw(blocks, editorCam.cam, showWireframe);
                DrawSphere(editorCam.origin, 0.1f, RED);

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

            DrawText("3D Model", 10, 34, 16, LIGHTGRAY);

            // ── Hotbar ─────────────────────────────────────────
            {
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
                        // Grey block slot (always present)
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
                    // inner offset lines to suggest 3D wireframe
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
                }

                // Click hotbar slots to assign picked color
                if (hasPickedColor && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 mouse = GetMousePosition();
                    for (int i = 1; i < HOTBAR_SLOTS; i++) { // skip slot 0
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

            // Drag rectangle
            if (rightDragActive && rightDragMoved && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 cur = GetMousePosition();
                float rx = fminf(rightDragStart.x, cur.x);
                float ry = fminf(rightDragStart.y, cur.y);
                float rw = fabsf(cur.x - rightDragStart.x);
                float rh = fabsf(cur.y - rightDragStart.y);
                DrawRectangle((int)rx, (int)ry, (int)rw, (int)rh, {255,255,255,30});
                DrawRectangleLines((int)rx, (int)ry, (int)rw, (int)rh, WHITE);
            }

            // Controls hint
            DrawText("X: Remove  |  Right-Click: Paint  |  Drag: Fill area  |  E: Color Wheel", 10, GetScreenHeight() - 30, 16, GRAY);

            // ── Color Wheel ────────────────────────────────────
            if (wheelOpen) {
                int sW = GetScreenWidth(), sH = GetScreenHeight();
                DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

                int panelW = 400, panelH = 420;
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
                    // Regenerate wheel texture when brightness changes
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

            // Export success/error message
            if (exportMsgTimer > 0.0f && !exportMessage.empty()) {
                int sW2 = GetScreenWidth();
                int mw = MeasureText(exportMessage.c_str(), 22);
                DrawRectangle(sW2/2-mw/2-12, 50, mw+24, 34, {0,0,0,200});
                Color mc = (exportMessage[0] == 'E') ? RED : GREEN; // "Exported..." vs "Error..."
                DrawText(exportMessage.c_str(), sW2/2-mw/2, 56, 22, mc);
            }

            // ── ESC Menu ───────────────────────────────────────
            if (escMenuOpen && !exportOpen) {
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
                    saveStructure(savePath, structureName, structureType, placedBlocks, editorCam);
                    placedBlocks.placed.clear();
                    placedBlocks.placeholders = {{0,0,0}};
                    placedBlocks.rebuildPlaceholderSet();
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
            }

            // ── Export Overlay ──────────────────────────────────
            if (exportOpen) {
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
                        std::string glbPath = getBlockModelsDir() + "/" + safeName + ".glb";
                        std::string glbRelPath = "assets/entities/blockModels/" + safeName + ".glb";
                        bool ok = exportModelAsGLB(glbPath, placedBlocks);
                        if (ok) {
                            createBlockJson(safeName, glbRelPath);
                            exportMessage = "Exported: " + safeName;
                        } else {
                            exportMessage = "Error: export failed";
                        }
                        exportMsgTimer = 3.0f;
                        exportOpen = false;
                        escMenuOpen = false;
                    }
                }
            }

        EndDrawing();
        return exitToMenu;
    }
};
