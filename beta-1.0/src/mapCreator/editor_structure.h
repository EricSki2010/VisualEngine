#pragma once
#include "engine/engine.h"
#include "engine/gameplay/registry.h"
#include "rlgl.h"
#include "gradient_sky.h"
#include "editor_camera.h"
#include "block_ui.h"
#include "placed_blocks.h"
#include "menu_ui.h"
#include "save_system.h"
#include <cmath>
#include <string>
#include <vector>
#include <map>

struct StructureEditor {
    Registry<BlockDef> blocks;
    BlockHotbar hotbar;
    BlockInventory inventory;
    bool initialized = false;
    int gridSize = 150;

    bool escMenuOpen = false;
    bool originConfirmOpen = false;

    // Import state
    bool importOpen = false;
    bool importPreview = false;
    bool importReplaceBlocks = false;
    std::vector<SaveEntry> importEntries;
    int selectedImport = -1;
    int importScrollOffset = 0;
    std::string importX, importY, importZ;
    int importFocusField = 0;
    std::map<Vec3i, std::string> previewBlocks;

    void reset() {
        escMenuOpen = false;
        originConfirmOpen = false;
        importOpen = false;
        importPreview = false;
        previewBlocks.clear();
    }

    // Returns true if editor wants to exit to menu
    bool frame(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
               GradientSky& sky, std::string& savePath, std::string& structureName,
               std::string& structureType, float& autoSaveTimer) {
        bool exitToMenu = false;

        if (!initialized) {
            blocks.tag = "BLOCKS";
            blocks.loadFolder("assets/entities/blocks");
            blocks.autoDetectModels("assets/entities/blockModels");
            blocks.loadModels();
            hotbar.load(blocks);
            inventory.load(blocks, hotbar);
            initialized = true;
        }

        // ── Input ──────────────────────────────────────────────
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (importPreview) { importPreview = false; previewBlocks.clear(); }
            else if (importOpen) { importOpen = false; }
            else if (originConfirmOpen) { originConfirmOpen = false; }
            else { escMenuOpen = !escMenuOpen; }
        }

        if (IsKeyPressed(KEY_O) && !escMenuOpen && !originConfirmOpen && !importOpen && !inventory.open) {
            originConfirmOpen = true;
        }

        if (importOpen && !importPreview) {
            if (IsKeyPressed(KEY_TAB)) importFocusField = (importFocusField + 1) % 3;
            std::string* focused = (importFocusField == 0) ? &importX : (importFocusField == 1) ? &importY : &importZ;
            int ch = GetCharPressed();
            while (ch > 0) {
                if ((ch >= '0' && ch <= '9') || (ch == '-' && focused->empty())) {
                    if ((int)focused->size() < 8) *focused += (char)ch;
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !focused->empty()) focused->pop_back();
        }

        // Auto-save
        autoSaveTimer += dt;
        if (autoSaveTimer >= 10.0f) {
            saveStructure(savePath, structureName, structureType, placedBlocks, editorCam);
            autoSaveTimer = 0.0f;
        }

        // ── Update ─────────────────────────────────────────────
        if (importPreview) {
            editorCam.update(dt);
        } else if (!escMenuOpen && !originConfirmOpen && !importOpen) {
            bool invConsumedClick = inventory.update(hotbar);
            if (!invConsumedClick && !inventory.open) {
                editorCam.update(dt);
            } else {
                float yawRad = editorCam.yaw * DEG2RAD;
                float forwardX = sinf(yawRad), forwardZ = cosf(yawRad);
                float rightX = cosf(yawRad), rightZ = -sinf(yawRad);
                float moveX = 0.0f, moveZ = 0.0f;
                if (IsKeyDown(KEY_W)) { moveX -= forwardX; moveZ -= forwardZ; }
                if (IsKeyDown(KEY_S)) { moveX += forwardX; moveZ += forwardZ; }
                if (IsKeyDown(KEY_D)) { moveX += rightX;   moveZ += rightZ; }
                if (IsKeyDown(KEY_A)) { moveX -= rightX;   moveZ -= rightZ; }
                float len = sqrtf(moveX * moveX + moveZ * moveZ);
                if (len > 0.0f) {
                    editorCam.origin.x += (moveX / len) * editorCam.panSpeed * dt;
                    editorCam.origin.z += (moveZ / len) * editorCam.panSpeed * dt;
                }
                if (IsKeyDown(KEY_LEFT_SHIFT)) editorCam.origin.y -= editorCam.panSpeed * dt;
                if (IsKeyDown(KEY_SPACE))      editorCam.origin.y += editorCam.panSpeed * dt;
                if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) {
                    editorCam.distance += editorCam.zoomSpeed * 10.0f * dt;
                    if (editorCam.distance > editorCam.maxDist) editorCam.distance = editorCam.maxDist;
                }
                if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) {
                    editorCam.distance -= editorCam.zoomSpeed * 10.0f * dt;
                    if (editorCam.distance < editorCam.minDist) editorCam.distance = editorCam.minDist;
                }
                editorCam.updatePosition();
            }
            hotbar.update();
            placedBlocks.update(editorCam.cam, hotbar, inventory.open, editorCam);
        }

        // ── Draw ───────────────────────────────────────────────
        BeginDrawing();
            ClearBackground(BLACK);
            sky.draw(editorCam.pitch);

            BeginMode3D(editorCam.cam);
                rlPushMatrix();
                rlTranslatef(0.5f, 0.0f, 0.5f);
                DrawGrid(gridSize * 2, 1.0f);
                rlPopMatrix();

                placedBlocks.draw(blocks, editorCam.cam);
                DrawCubeWires(editorCam.origin, 1.02f, 1.02f, 1.02f, RED);

                if (importPreview && !previewBlocks.empty()) {
                    for (auto& [offset, blockName] : previewBlocks) {
                        Vec3i w = placedBlocks.toWorld(offset);
                        Vector3 v = { (float)w.x, (float)w.y, (float)w.z };
                        auto bIt = blocks.entries.find(blockName);
                        if (bIt != blocks.entries.end()) {
                            BlockDef& def = bIt->second;
                            if (def.formatType == "glb" && def.modelLoaded) {
                                Vector3 mp = { v.x, v.y - 0.5f, v.z };
                                DrawModelEx(def.model, mp, {0,1,0}, 0.0f,
                                    {def.modelScaleX, def.modelScaleY, def.modelScaleZ}, {255,255,255,140});
                            } else {
                                DrawCube(v, 1.0f, 1.0f, 1.0f, {def.r, def.g, def.b, 140});
                                DrawCubeWires(v, 1.0f, 1.0f, 1.0f, {255,255,100,180});
                            }
                        } else {
                            DrawCube(v, 1.0f, 1.0f, 1.0f, {100,100,255,100});
                            DrawCubeWires(v, 1.0f, 1.0f, 1.0f, {255,255,100,180});
                        }
                    }
                }
            EndMode3D();

            inventory.draw();
            hotbar.draw();

            // Coordinates
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f",
                         editorCam.origin.x, editorCam.origin.y, editorCam.origin.z);
                DrawText(buf, 10, 10, 20, GREEN);
            }

            if (importPreview) {
                const char* t = "IMPORT PREVIEW  -  Press ESC to go back";
                int tw = MeasureText(t, 24);
                int sw = GetScreenWidth();
                DrawRectangle(sw/2 - tw/2 - 12, 40, tw + 24, 36, {0,0,0,180});
                DrawText(t, sw/2 - tw/2, 46, 24, YELLOW);
            }

            // ── ESC Menu Overlay ───────────────────────────────
            if (escMenuOpen) {
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
                if (drawMenuButton("Import Structure", bx, by+bh+gap, bw, bh)) {
                    escMenuOpen = false;
                    importOpen = true;
                    importEntries = listSaves();
                    selectedImport = -1;
                    importScrollOffset = 0;
                    importX.clear(); importY.clear(); importZ.clear();
                    importFocusField = 0;
                }
            }

            // ── Origin Confirm Overlay ─────────────────────────
            if (originConfirmOpen) {
                int sW = GetScreenWidth(), sH = GetScreenHeight();
                DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

                int pW = 420, pH = 180, pX = (sW-pW)/2, pY = (sH-pH)/2;
                DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
                DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

                const char* prompt = "Move the structure origin here?";
                int pw = MeasureText(prompt, 22);
                DrawText(prompt, (sW-pw)/2, pY+24, 22, WHITE);

                int nox = (int)roundf(editorCam.origin.x);
                int noy = (int)roundf(editorCam.origin.y);
                int noz = (int)roundf(editorCam.origin.z);
                char posText[128];
                snprintf(posText, sizeof(posText), "New origin: (%d, %d, %d)", nox, noy, noz);
                int posW = MeasureText(posText, 18);
                DrawText(posText, (sW-posW)/2, pY+56, 18, LIGHTGRAY);

                int bw = 140, bh = 42, bg = 30;
                int tbw = bw*2+bg, bsx = (sW-tbw)/2, by = pY+pH-bh-24;

                if (drawMenuButton("No", bsx, by, bw, bh)) originConfirmOpen = false;
                if (drawMenuButton("Yes", bsx+bw+bg, by, bw, bh)) {
                    Vec3i oldO = placedBlocks.getOrigin();
                    Vec3i newO = {nox, noy, noz};
                    Vec3i delta = {oldO.x-newO.x, oldO.y-newO.y, oldO.z-newO.z};
                    std::map<Vec3i, std::string> remapped;
                    for (auto& [off, bn] : placedBlocks.placed)
                        remapped[{off.x+delta.x, off.y+delta.y, off.z+delta.z}] = bn;
                    placedBlocks.placed = remapped;
                    placedBlocks.placeholders[0] = newO;
                    placedBlocks.rebuildPlaceholderSet();
                    originConfirmOpen = false;
                }
            }

            // ── Import Overlay ─────────────────────────────────
            if (importOpen && !importPreview) {
                int sW = GetScreenWidth(), sH = GetScreenHeight();
                DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

                int pW3 = 520, pH3 = 520, pX3 = (sW-pW3)/2, pY3 = (sH-pH3)/2;
                DrawRectangle(pX3, pY3, pW3, pH3, {20,20,28,240});
                DrawRectangleLines(pX3, pY3, pW3, pH3, {80,80,80,200});

                int itw = MeasureText("Import Structure", 28);
                DrawText("Import Structure", (sW-itw)/2, pY3+16, 28, WHITE);

                int lx = pX3+16, lw = pW3-32, ly = pY3+56, ih = 44, mv = 5;

                if (importEntries.empty()) {
                    DrawText("No saved structures found.", lx, ly, 18, GRAY);
                } else {
                    float wheel = GetMouseWheelMove();
                    if (wheel != 0.0f) {
                        importScrollOffset -= (int)wheel;
                        if (importScrollOffset < 0) importScrollOffset = 0;
                        int mo = (int)importEntries.size() - mv;
                        if (mo < 0) mo = 0;
                        if (importScrollOffset > mo) importScrollOffset = mo;
                    }
                    for (int i = 0; i < mv && (i+importScrollOffset) < (int)importEntries.size(); i++) {
                        int idx = i + importScrollOffset;
                        int iy = ly + i*(ih+3);
                        Vector2 mouse = GetMousePosition();
                        bool hov = mouse.x >= lx && mouse.x < lx+lw && mouse.y >= iy && mouse.y < iy+ih;
                        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedImport = idx;
                        Color ibg = (idx==selectedImport) ? Color{60,60,90,230} : hov ? Color{45,45,60,230} : Color{30,30,40,230};
                        DrawRectangle(lx, iy, lw, ih, ibg);
                        DrawRectangleLines(lx, iy, lw, ih, (idx==selectedImport) ? WHITE : Color{80,80,80,200});
                        DrawText(importEntries[idx].name.c_str(), lx+10, iy+6, 20, WHITE);
                        DrawText(importEntries[idx].type.c_str(), lx+10, iy+26, 12, LIGHTGRAY);
                    }
                }

                int cy = ly + mv*(ih+3) + 20;
                DrawText("Place at position:", lx, cy, 20, LIGHTGRAY);
                cy += 28;

                int labW = 24, gapB = 10, fW = (lw - 3*labW - 2*gapB)/3, fH = 36, fS = labW+fW+gapB;
                const char* labs[] = {"X:","Y:","Z:"};
                std::string* flds[] = {&importX, &importY, &importZ};
                for (int f = 0; f < 3; f++) {
                    int fx = lx + f*fS;
                    DrawText(labs[f], fx, cy+8, 20, LIGHTGRAY);
                    int ix = fx+labW;
                    Color fb = (f==importFocusField) ? WHITE : Color{100,100,120,255};
                    DrawRectangle(ix, cy, fW, fH, {30,30,40,255});
                    DrawRectangleLines(ix, cy, fW, fH, fb);
                    DrawText(flds[f]->c_str(), ix+6, cy+8, 20, WHITE);
                    if (f==importFocusField && ((int)(GetTime()*2.0)%2)==0) {
                        int cx = ix+6+MeasureText(flds[f]->c_str(), 20);
                        DrawRectangle(cx, cy+6, 2, 24, WHITE);
                    }
                    Vector2 mouse = GetMousePosition();
                    if (mouse.x>=ix && mouse.x<ix+fW && mouse.y>=cy && mouse.y<cy+fH)
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) importFocusField = f;
                }
                DrawText("Tab to switch fields", lx, cy+fH+4, 14, GRAY);

                int ty = cy+fH+24;
                { // Replace toggle
                    int bs = 20;
                    DrawRectangle(lx, ty, bs, bs, {30,30,40,255});
                    DrawRectangleLines(lx, ty, bs, bs, {100,100,120,255});
                    if (importReplaceBlocks) DrawRectangle(lx+4, ty+4, bs-8, bs-8, WHITE);
                    DrawText("Replace blocks that are in the way", lx+bs+8, ty+2, 18, LIGHTGRAY);
                    Vector2 m = GetMousePosition();
                    if (m.x>=lx && m.x<lx+bs && m.y>=ty && m.y<ty+bs)
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) importReplaceBlocks = !importReplaceBlocks;
                }

                bool vc = !importX.empty() && !importY.empty() && !importZ.empty()
                        && importX!="-" && importY!="-" && importZ!="-";
                bool ci = selectedImport>=0 && selectedImport<(int)importEntries.size() && vc;

                int bw = 140, bh = 42, bg = 12, tbw = bw*3+bg*2, bsx = (sW-tbw)/2, by = pY3+pH3-bh-20;

                if (drawMenuButton("Cancel", bsx, by, bw, bh)) importOpen = false;

                if (drawMenuButton("Preview", bsx+bw+bg, by, bw, bh, ci)) {
                    if (ci) {
                        int tx=std::atoi(importX.c_str()), ty2=std::atoi(importY.c_str()), tz=std::atoi(importZ.c_str());
                        auto ib = loadImportBlocks(importEntries[selectedImport].filepath);
                        Vec3i bo = placedBlocks.toOffset({tx,ty2,tz});
                        previewBlocks.clear();
                        for (auto& [off, bn] : ib)
                            previewBlocks[{bo.x+off.x, bo.y+off.y, bo.z+off.z}] = bn;
                        importPreview = true;
                    }
                }

                if (drawMenuButton("Import", bsx+2*(bw+bg), by, bw, bh, ci)) {
                    if (ci) {
                        int tx=std::atoi(importX.c_str()), ty2=std::atoi(importY.c_str()), tz=std::atoi(importZ.c_str());
                        auto ib = loadImportBlocks(importEntries[selectedImport].filepath);
                        Vec3i bo = placedBlocks.toOffset({tx,ty2,tz});
                        for (auto& [off, bn] : ib) {
                            Vec3i fo = {bo.x+off.x, bo.y+off.y, bo.z+off.z};
                            if (importReplaceBlocks || placedBlocks.placed.find(fo)==placedBlocks.placed.end())
                                placedBlocks.placed[fo] = bn;
                        }
                        importOpen = false; importPreview = false; previewBlocks.clear();
                    }
                }
            }

        EndDrawing();
        return exitToMenu;
    }
};
