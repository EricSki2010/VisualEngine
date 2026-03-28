#pragma once
#include "screen_menu.h"
#include "menu_ui.h"
#include "placed_blocks.h"
#include "editor_camera.h"
#include "save_system.h"
#include "editor_structure.h"
#include "editor_model.h"
#include "editor_sprite.h"
#include "raylib.h"
#include <string>
#include <vector>

struct LoadScreen {
    std::vector<SaveEntry> allEntries;
    std::vector<SaveEntry> saveEntries; // filtered view
    int selectedSave = -1;
    int scrollOffset = 0;
    bool confirmDelete = false;
    int filterIndex = 0; // 0=All, 1=3D Structure, 2=3D Block, 3=3D Model, 4=3D Sprite
    bool filterDropdownOpen = false;

    const char* filterOptions[5] = { "All", "3D Structure", "3D Block", "3D Model", "3D Sprite" };
    static const int FILTER_COUNT = 5;

    void refresh() {
        allEntries = listSaves();
        applyFilter();
        selectedSave = -1;
        scrollOffset = 0;
        confirmDelete = false;
    }

    void applyFilter() {
        saveEntries.clear();
        for (auto& e : allEntries) {
            if (filterIndex == 0 || e.type == filterOptions[filterIndex])
                saveEntries.push_back(e);
        }
    }

    AppState drawAndUpdate(PlacedBlocks& placedBlocks, EditorCamera& editorCam,
                           StructureEditor& structEditor, ModelEditor& modelEditor,
                           SpriteEditor& spriteEditor,
                           std::string& savePath, std::string& structureName,
                           std::string& structureType, float& autoSaveTimer) {
        AppState nextState = STATE_LOAD;

        if (confirmDelete && IsKeyPressed(KEY_ESCAPE)) confirmDelete = false;

        BeginDrawing();
            ClearBackground({ 15, 15, 20, 255 });

            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();

            int titleW2 = MeasureText("Load Game", 40);
            DrawText("Load Game", (screenW - titleW2) / 2, screenH / 8, 40, WHITE);

            int listW = 500, listX = (screenW - listW) / 2;
            int filterY = screenH / 8 + 50;
            int filterH = 36, filterW = 200;
            int filterX = listX + listW - filterW;
            drawFilterHeader(filterX, filterY, filterW, filterH);

            int itemH = 50, listStartY = filterY + filterH + 12, maxVis = 7;

            drawSaveList(listX, listStartY, listW, itemH, maxVis);

            int btnW2 = 120, btnH2 = 45, btnGap = 12;
            int btnRowY = listStartY + maxVis * (itemH + 4) + 30;
            int totalBtnW = btnW2 * 4 + btnGap * 3;
            int btnStartX = (screenW - totalBtnW) / 2;

            bool hasSelection = selectedSave >= 0 && selectedSave < (int)saveEntries.size();
            bool selectedIsStarred = hasSelection && saveEntries[selectedSave].starred;

            if (drawMenuButton("Back", btnStartX, btnRowY, btnW2, btnH2))
                nextState = STATE_MENU;

            // Star/Unstar button
            const char* starLabel = selectedIsStarred ? "Unstar" : "Star";
            if (drawMenuButton(starLabel, btnStartX + btnW2 + btnGap, btnRowY, btnW2, btnH2, hasSelection)) {
                if (hasSelection) {
                    std::string prevPath = saveEntries[selectedSave].filepath;
                    toggleStarred(prevPath);
                    refresh();
                    for (int i = 0; i < (int)saveEntries.size(); i++) {
                        if (saveEntries[i].filepath == prevPath) { selectedSave = i; break; }
                    }
                }
            }

            // Delete button — disabled if starred
            bool canDelete = hasSelection && !selectedIsStarred;
            if (drawMenuButton("Delete", btnStartX + 2 * (btnW2 + btnGap), btnRowY, btnW2, btnH2, canDelete)) {
                if (canDelete) confirmDelete = true;
            }

            if (drawMenuButton("Load", btnStartX + 3 * (btnW2 + btnGap), btnRowY, btnW2, btnH2, hasSelection)) {
                if (hasSelection) {
                    savePath = saveEntries[selectedSave].filepath;
                    editorCam = EditorCamera();
                    modelEditor.reset();
                    loadStructureInto(savePath, structureName, structureType, placedBlocks, editorCam,
                                      modelEditor.slots, ModelEditor::HOTBAR_SLOTS);
                    autoSaveTimer = 0.0f;
                    structEditor.reset();
                    spriteEditor.reset();
                    if (structureType == "3D Sprite") {
                        spriteEditor.loadSpriteModels(savePath);
                    }
                    nextState = STATE_EDITOR;
                }
            }

            if (confirmDelete) drawDeleteConfirm(screenW, screenH);
            if (filterDropdownOpen && !confirmDelete) drawFilterOptions(filterX, filterY, filterW, filterH);

        EndDrawing();
        return nextState;
    }

private:
    void drawDeleteConfirm(int sW, int sH) {
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int pW = 420, pH = 180, pX = (sW-pW)/2, pY = (sH-pH)/2;
        DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
        DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

        const char* prompt = "Delete this save?";
        int pw = MeasureText(prompt, 24);
        DrawText(prompt, (sW-pw)/2, pY+20, 24, WHITE);

        if (selectedSave >= 0 && selectedSave < (int)saveEntries.size()) {
            const char* name = saveEntries[selectedSave].name.c_str();
            int nw = MeasureText(name, 20);
            DrawText(name, (sW-nw)/2, pY+54, 20, RED);
        }

        int bw = 140, bh = 42, bg = 30;
        int tbw = bw*2+bg, bsx = (sW-tbw)/2, by = pY+pH-bh-24;

        if (drawMenuButton("Cancel", bsx, by, bw, bh)) {
            confirmDelete = false;
        }
        if (drawMenuButton("Delete", bsx+bw+bg, by, bw, bh)) {
            if (selectedSave >= 0 && selectedSave < (int)saveEntries.size()) {
                deleteSave(saveEntries[selectedSave].filepath);
                confirmDelete = false;
                refresh();
            }
        }
    }

    void drawFilterHeader(int fx, int fy, int fw, int fh) {
        DrawText("Filter:", fx - 60, fy + 8, 18, LIGHTGRAY);
        DrawRectangle(fx, fy, fw, fh, {30,30,40,255});
        DrawRectangleLines(fx, fy, fw, fh, {100,100,120,255});
        DrawText(filterOptions[filterIndex], fx + 8, fy + 8, 20, WHITE);
        DrawText("v", fx + fw - 22, fy + 8, 20, LIGHTGRAY);

        Vector2 m = GetMousePosition();
        if (m.x >= fx && m.x < fx + fw && m.y >= fy && m.y < fy + fh)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) filterDropdownOpen = !filterDropdownOpen;
    }

    void drawFilterOptions(int fx, int fy, int fw, int fh) {
        for (int i = 0; i < FILTER_COUNT; i++) {
            int oy = fy + fh + i * fh;
            Vector2 m = GetMousePosition();
            bool hov = m.x >= fx && m.x < fx + fw && m.y >= oy && m.y < oy + fh;
            DrawRectangle(fx, oy, fw, fh, hov ? Color{55,55,75,250} : Color{40,40,55,250});
            DrawRectangleLines(fx, oy, fw, fh, {100,100,120,255});
            DrawText(filterOptions[i], fx + 8, oy + 8, 20, WHITE);
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                filterIndex = i;
                filterDropdownOpen = false;
                applyFilter();
                selectedSave = -1;
                scrollOffset = 0;
            }
        }
    }

    void drawSaveList(int listX, int listStartY, int listW, int itemH, int maxVis) {
        if (saveEntries.empty()) {
            DrawText("No saved structures found.", listX, listStartY, 20, GRAY);
            return;
        }

        if (!confirmDelete) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                scrollOffset -= (int)wheel;
                if (scrollOffset < 0) scrollOffset = 0;
                int mo = (int)saveEntries.size() - maxVis;
                if (mo < 0) mo = 0;
                if (scrollOffset > mo) scrollOffset = mo;
            }
        }

        for (int i = 0; i < maxVis && (i + scrollOffset) < (int)saveEntries.size(); i++) {
            int idx = i + scrollOffset;
            int y = listStartY + i * (itemH + 4);
            Vector2 m = GetMousePosition();
            bool hov = !confirmDelete && m.x >= listX && m.x < listX + listW && m.y >= y && m.y < y + itemH;
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedSave = idx;

            Color bg = (idx == selectedSave) ? Color{60,60,90,230}
                     : hov ? Color{45,45,60,230} : Color{30,30,40,230};
            DrawRectangle(listX, y, listW, itemH, bg);
            DrawRectangleLines(listX, y, listW, itemH, (idx==selectedSave) ? WHITE : Color{80,80,80,200});

            // Star indicator
            if (saveEntries[idx].starred) {
                DrawText("*", listX + listW - 28, y + 12, 26, GOLD);
            }

            DrawText(saveEntries[idx].name.c_str(), listX+12, y+8, 22, WHITE);
            DrawText(saveEntries[idx].type.c_str(), listX+12, y+30, 14, LIGHTGRAY);
        }

        if ((int)saveEntries.size() > maxVis) {
            int totalH = maxVis * (itemH + 4);
            float ratio = (float)scrollOffset / (float)((int)saveEntries.size() - maxVis);
            int barH = totalH / (int)saveEntries.size() * maxVis;
            if (barH < 20) barH = 20;
            int barY = listStartY + (int)(ratio * (totalH - barH));
            DrawRectangle(listX + listW + 6, barY, 6, barH, {80,80,100,200});
        }
    }
};
