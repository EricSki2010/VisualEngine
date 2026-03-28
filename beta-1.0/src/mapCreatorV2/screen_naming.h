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

struct NamingScreen {
    std::string structureName;
    std::string structureType = "3D Structure";
    bool dropdownOpen = false;
    std::string modelSizeStr;
    std::string modelSizeX, modelSizeY, modelSizeZ;
    int sizeFocusField = -1; // -1=name, 0=single size, 1=X, 2=Y, 3=Z

    void reset() {
        structureName.clear();
        structureType = "3D Structure";
        dropdownOpen = false;
        modelSizeStr.clear();
        modelSizeX.clear(); modelSizeY.clear(); modelSizeZ.clear();
        sizeFocusField = -1;
    }

    AppState drawAndUpdate(PlacedBlocks& placedBlocks, EditorCamera& editorCam,
                           StructureEditor& structEditor, ModelEditor& modelEditor,
                           SpriteEditor& spriteEditor,
                           std::string& savePath, std::string& outName,
                           std::string& outType, float& autoSaveTimer) {
        AppState nextState = STATE_NAMING;

        handleTextInput();

        BeginDrawing();
            ClearBackground({ 15, 15, 20, 255 });

            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();

            const char* title = "New Structure";
            int titleSize = 40;
            int titleW = MeasureText(title, titleSize);
            DrawText(title, (screenW - titleW) / 2, screenH / 5, titleSize, WHITE);

            int fieldW = 400, fieldH = 40;
            int fieldX = (screenW - fieldW) / 2;
            int fieldY = screenH / 3;

            drawNameField(fieldX, fieldY, fieldW, fieldH);
            int dropY = fieldY + fieldH + 40;
            drawTypeHeader(fieldX, dropY, fieldW, fieldH);

            int extraH = 0;
            if (structureType == "3D Block") {
                extraH = fieldH + 40;
                drawSizeField(fieldX, dropY + fieldH + 40, fieldW, fieldH);
            } else if (structureType == "3D Model") {
                extraH = fieldH + 40;
                drawXYZSizeFields(fieldX, dropY + fieldH + 40, fieldW, fieldH);
            }

            nextState = drawButtons(screenW, dropY + fieldH + extraH + 80,
                                    placedBlocks, editorCam, structEditor, modelEditor,
                                    spriteEditor, savePath, outName, outType, autoSaveTimer);

            // Draw dropdown options last so they render on top of size field
            if (dropdownOpen) drawTypeOptions(fieldX, dropY, fieldW, fieldH);

        EndDrawing();
        return nextState;
    }

private:
    void handleTextInput() {
        if (IsKeyPressed(KEY_TAB)) {
            if (structureType == "3D Model") {
                // Cycle: name(-1) -> X(1) -> Y(2) -> Z(3) -> name
                sizeFocusField = (sizeFocusField + 2) % 4 - 1;
            } else if (structureType == "3D Block") {
                sizeFocusField = (sizeFocusField == -1) ? 0 : -1;
            }
        }

        // Get the target string for input
        std::string* target = nullptr;
        bool numOnly = false;
        if (sizeFocusField == -1) {
            target = &structureName;
        } else if (sizeFocusField == 0) {
            target = &modelSizeStr; numOnly = true;
        } else if (sizeFocusField == 1) {
            target = &modelSizeX; numOnly = true;
        } else if (sizeFocusField == 2) {
            target = &modelSizeY; numOnly = true;
        } else if (sizeFocusField == 3) {
            target = &modelSizeZ; numOnly = true;
        }

        if (target) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (numOnly) {
                    if (ch >= '0' && ch <= '9' && (int)target->size() < 4)
                        *target += (char)ch;
                } else {
                    if (ch >= 32 && ch <= 125 && (int)target->size() < 64)
                        *target += (char)ch;
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !target->empty())
                target->pop_back();
        }
    }

    void drawNameField(int fieldX, int fieldY, int fieldW, int fieldH) {
        DrawText("Name:", fieldX, fieldY - 24, 20, LIGHTGRAY);
        bool focused = (sizeFocusField == -1);
        Color nameBorder = focused ? WHITE : Color{ 100, 100, 120, 255 };
        DrawRectangle(fieldX, fieldY, fieldW, fieldH, { 30, 30, 40, 255 });
        DrawRectangleLines(fieldX, fieldY, fieldW, fieldH, nameBorder);
        DrawText(structureName.c_str(), fieldX + 8, fieldY + 10, 20, WHITE);

        if (focused && ((int)(GetTime() * 2.0) % 2) == 0) {
            int cx = fieldX + 8 + MeasureText(structureName.c_str(), 20);
            DrawRectangle(cx, fieldY + 8, 2, 24, WHITE);
        }

        if (!dropdownOpen) {
            Vector2 m = GetMousePosition();
            if (m.x >= fieldX && m.x < fieldX + fieldW && m.y >= fieldY && m.y < fieldY + fieldH)
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) sizeFocusField = -1;
        }
    }

    void drawTypeHeader(int fieldX, int dropY, int fieldW, int fieldH) {
        DrawText("Type:", fieldX, dropY - 24, 20, LIGHTGRAY);
        DrawRectangle(fieldX, dropY, fieldW, fieldH, { 30, 30, 40, 255 });
        DrawRectangleLines(fieldX, dropY, fieldW, fieldH, { 100, 100, 120, 255 });
        DrawText(structureType.c_str(), fieldX + 8, dropY + 10, 20, WHITE);
        DrawText("v", fieldX + fieldW - 28, dropY + 10, 20, LIGHTGRAY);

        Vector2 m = GetMousePosition();
        if (m.x >= fieldX && m.x < fieldX + fieldW && m.y >= dropY && m.y < dropY + fieldH)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dropdownOpen = !dropdownOpen;
    }

    void drawTypeOptions(int fieldX, int dropY, int fieldW, int fieldH) {
        const char* options[] = { "3D Structure", "3D Block", "3D Model", "3D Sprite" };
        for (int oi = 0; oi < 4; oi++) {
            int optY = dropY + fieldH + oi * fieldH;
            Vector2 mo = GetMousePosition();
            bool hov = mo.x >= fieldX && mo.x < fieldX + fieldW && mo.y >= optY && mo.y < optY + fieldH;
            DrawRectangle(fieldX, optY, fieldW, fieldH, hov ? Color{55,55,75,250} : Color{40,40,55,250});
            DrawRectangleLines(fieldX, optY, fieldW, fieldH, {100,100,120,255});
            DrawText(options[oi], fieldX + 8, optY + 10, 20, WHITE);
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                structureType = options[oi];
                dropdownOpen = false;
            }
        }
    }

    void drawSizeField(int fieldX, int sizeY, int fieldW, int fieldH) {
        DrawText("Size (1-1000):", fieldX, sizeY - 24, 20, LIGHTGRAY);
        bool focused = (sizeFocusField == 0);
        Color sb = focused ? WHITE : Color{100,100,120,255};
        DrawRectangle(fieldX, sizeY, 120, fieldH, {30,30,40,255});
        DrawRectangleLines(fieldX, sizeY, 120, fieldH, sb);
        DrawText(modelSizeStr.c_str(), fieldX + 8, sizeY + 10, 20, WHITE);

        if (focused && ((int)(GetTime()*2.0)%2)==0) {
            int cx = fieldX + 8 + MeasureText(modelSizeStr.c_str(), 20);
            DrawRectangle(cx, sizeY + 8, 2, 24, WHITE);
        }

        Vector2 m = GetMousePosition();
        if (m.x >= fieldX && m.x < fieldX + 120 && m.y >= sizeY && m.y < sizeY + fieldH)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) sizeFocusField = 0;
    }

    void drawXYZSizeFields(int fieldX, int sizeY, int fieldW, int fieldH) {
        DrawText("Size (X, Y, Z):", fieldX, sizeY - 24, 20, LIGHTGRAY);

        int fW = 100, gap = 10;
        const char* labels[] = {"X:", "Y:", "Z:"};
        std::string* fields[] = {&modelSizeX, &modelSizeY, &modelSizeZ};

        for (int i = 0; i < 3; i++) {
            int fx = fieldX + i * (fW + gap + 24);
            int focusIdx = i + 1; // 1=X, 2=Y, 3=Z
            bool focused = (sizeFocusField == focusIdx);

            DrawText(labels[i], fx, sizeY + 10, 20, LIGHTGRAY);
            int ix = fx + 24;
            Color border = focused ? WHITE : Color{100,100,120,255};
            DrawRectangle(ix, sizeY, fW, fieldH, {30,30,40,255});
            DrawRectangleLines(ix, sizeY, fW, fieldH, border);
            DrawText(fields[i]->c_str(), ix + 6, sizeY + 10, 20, WHITE);

            if (focused && ((int)(GetTime()*2.0)%2)==0) {
                int cx = ix + 6 + MeasureText(fields[i]->c_str(), 20);
                DrawRectangle(cx, sizeY + 8, 2, 24, WHITE);
            }

            Vector2 m = GetMousePosition();
            if (m.x >= ix && m.x < ix + fW && m.y >= sizeY && m.y < sizeY + fieldH)
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) sizeFocusField = focusIdx;
        }
    }

    AppState drawButtons(int screenW, int btnRowY,
                         PlacedBlocks& placedBlocks, EditorCamera& editorCam,
                         StructureEditor& structEditor, ModelEditor& modelEditor,
                         SpriteEditor& spriteEditor,
                         std::string& savePath, std::string& outName,
                         std::string& outType, float& autoSaveTimer) {
        int btnW2 = 180, btnH2 = 45, btnGap = 20;
        int btnStartX = (screenW - (btnW2*2+btnGap)) / 2;

        if (!dropdownOpen && drawMenuButton("Back", btnStartX, btnRowY, btnW2, btnH2))
            return STATE_MENU;

        bool canCreate = !dropdownOpen && !structureName.empty();
        if (structureType == "3D Block") {
            int sz = modelSizeStr.empty() ? 0 : std::atoi(modelSizeStr.c_str());
            canCreate = canCreate && sz >= 1 && sz <= 1000;
        } else if (structureType == "3D Model") {
            int sx = modelSizeX.empty() ? 0 : std::atoi(modelSizeX.c_str());
            int sy = modelSizeY.empty() ? 0 : std::atoi(modelSizeY.c_str());
            int sz = modelSizeZ.empty() ? 0 : std::atoi(modelSizeZ.c_str());
            canCreate = canCreate && sx >= 1 && sx <= 1000 && sy >= 1 && sy <= 1000 && sz >= 1 && sz <= 1000;
        }

        if (drawMenuButton("Create", btnStartX + btnW2 + btnGap, btnRowY, btnW2, btnH2, canCreate)) {
            if (canCreate) {
                std::string subfolder = getSaveSubfolder(structureType);
                fs::create_directories(subfolder);
                savePath = subfolder + "/" + sanitizeFilename(structureName) + ".sav";
                outName = structureName;
                outType = structureType;

                placedBlocks.placeholders = {{0, 0, 0}};
                placedBlocks.matrix.clear();
                placedBlocks.markDirty();
                placedBlocks.modelSize = 0;
                placedBlocks.modelDims = {0, 0, 0};
                placedBlocks.modelOriginPos = {0, 0, 0};
                placedBlocks.removed.clear();

                if (structureType == "3D Block") {
                    int sz = std::atoi(modelSizeStr.c_str());
                    if (sz > 1000) sz = 1000;
                    if (sz < 1) sz = 1;
                    placedBlocks.modelSize = sz;
                } else if (structureType == "3D Model") {
                    int sx = std::atoi(modelSizeX.c_str());
                    int sy = std::atoi(modelSizeY.c_str());
                    int sz = std::atoi(modelSizeZ.c_str());
                    if (sx < 1) sx = 1; if (sx > 1000) sx = 1000;
                    if (sy < 1) sy = 1; if (sy > 1000) sy = 1000;
                    if (sz < 1) sz = 1; if (sz > 1000) sz = 1000;
                    placedBlocks.modelDims = {sx, sy, sz};
                }

                placedBlocks.rebuildPlaceholderSet();
                autoSaveTimer = 0.0f;
                editorCam = EditorCamera();
                editorCam.init();
                structEditor.reset();
                modelEditor.reset();
                spriteEditor.reset();
                saveStructure(savePath, outName, outType, placedBlocks, editorCam,
                             modelEditor.slots, ModelEditor::HOTBAR_SLOTS);
                return STATE_EDITOR;
            }
        }

        return STATE_NAMING;
    }
};
