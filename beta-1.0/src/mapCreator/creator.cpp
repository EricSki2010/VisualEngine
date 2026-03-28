#include "engine/engine.h"
#include "engine/gameplay/registry.h"
#include "gradient_sky.h"
#include "editor_camera.h"
#include "placed_blocks.h"
#include "menu_ui.h"
#include "save_system.h"
#include "editor_structure.h"
#include "editor_model.h"
#include <cmath>
#include <string>

// ── App State ───────────────────────────────────────────────────────

enum AppState { STATE_MENU, STATE_NAMING, STATE_EDITOR, STATE_LOAD };

// ── Main ────────────────────────────────────────────────────────────

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Map Creator");

    int monitor = GetCurrentMonitor();
    int monW = GetMonitorWidth(monitor);
    int monH = GetMonitorHeight(monitor);
    if (monW > 0 && monH > 0) {
        SetWindowSize(monW, monH);
    }
    ToggleFullscreen();

    SetTargetFPS(60);
    SetExitKey(0);

    AppState state = STATE_MENU;

    // Shared editor state
    PlacedBlocks placedBlocks;
    GradientSky sky;
    EditorCamera editorCam;

    // Naming screen state
    std::string structureName;
    std::string structureType = "3D Structure";
    bool dropdownOpen = false;
    std::string modelSizeStr;
    bool namingFocusSize = false;

    // Save state
    std::string savePath;
    float autoSaveTimer = 0.0f;

    // Load screen state
    std::vector<SaveEntry> saveEntries;
    int selectedSave = -1;
    int loadScrollOffset = 0;

    // Editors
    StructureEditor structEditor;
    ModelEditor modelEditor;

    bool running = true;

    while (running && !WindowShouldClose()) {
        float dt = GetFrameTime();

        // ── Main Menu ──────────────────────────────────────────
        if (state == STATE_MENU) {
            BeginDrawing();
                ClearBackground({ 15, 15, 20, 255 });

                int screenW = GetScreenWidth();
                int screenH = GetScreenHeight();

                const char* title = "Map Creator";
                int titleSize = 48;
                int titleW = MeasureText(title, titleSize);
                DrawText(title, (screenW - titleW) / 2, screenH / 4, titleSize, WHITE);

                int btnW = 300, btnH = 50, btnX = (screenW - btnW) / 2, btnGap = 20;
                int btnStartY = screenH / 2 - btnH;

                if (drawMenuButton("Create New Structure", btnX, btnStartY, btnW, btnH)) {
                    structureName.clear();
                    structureType = "3D Structure";
                    dropdownOpen = false;
                    modelSizeStr.clear();
                    namingFocusSize = false;
                    state = STATE_NAMING;
                }

                if (drawMenuButton("Load Game", btnX, btnStartY + btnH + btnGap, btnW, btnH)) {
                    saveEntries = listSaves();
                    selectedSave = -1;
                    loadScrollOffset = 0;
                    state = STATE_LOAD;
                }

                if (drawMenuButton("Exit", btnX, btnStartY + 2 * (btnH + btnGap), btnW, btnH)) {
                    running = false;
                }

            EndDrawing();

        // ── Naming Screen ──────────────────────────────────────
        } else if (state == STATE_NAMING) {
            // Text input
            if (!namingFocusSize) {
                int ch = GetCharPressed();
                while (ch > 0) {
                    if (ch >= 32 && ch <= 125 && (int)structureName.size() < 64)
                        structureName += (char)ch;
                    ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !structureName.empty())
                    structureName.pop_back();
            } else {
                int ch = GetCharPressed();
                while (ch > 0) {
                    if (ch >= '0' && ch <= '9' && (int)modelSizeStr.size() < 4)
                        modelSizeStr += (char)ch;
                    ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !modelSizeStr.empty())
                    modelSizeStr.pop_back();
            }

            if (IsKeyPressed(KEY_TAB)) namingFocusSize = !namingFocusSize;

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

                // Name field
                DrawText("Name:", fieldX, fieldY - 24, 20, LIGHTGRAY);
                Color nameBorder = (!namingFocusSize) ? WHITE : Color{ 100, 100, 120, 255 };
                DrawRectangle(fieldX, fieldY, fieldW, fieldH, { 30, 30, 40, 255 });
                DrawRectangleLines(fieldX, fieldY, fieldW, fieldH, nameBorder);
                DrawText(structureName.c_str(), fieldX + 8, fieldY + 10, 20, WHITE);

                if (!namingFocusSize && ((int)(GetTime() * 2.0) % 2) == 0) {
                    int cx = fieldX + 8 + MeasureText(structureName.c_str(), 20);
                    DrawRectangle(cx, fieldY + 8, 2, 24, WHITE);
                }

                { // Click to focus name
                    Vector2 m = GetMousePosition();
                    if (m.x >= fieldX && m.x < fieldX + fieldW && m.y >= fieldY && m.y < fieldY + fieldH)
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) namingFocusSize = false;
                }

                // Type dropdown
                int dropY = fieldY + fieldH + 40;
                DrawText("Type:", fieldX, dropY - 24, 20, LIGHTGRAY);
                DrawRectangle(fieldX, dropY, fieldW, fieldH, { 30, 30, 40, 255 });
                DrawRectangleLines(fieldX, dropY, fieldW, fieldH, { 100, 100, 120, 255 });
                DrawText(structureType.c_str(), fieldX + 8, dropY + 10, 20, WHITE);
                DrawText("v", fieldX + fieldW - 28, dropY + 10, 20, LIGHTGRAY);

                { // Click dropdown header
                    Vector2 m = GetMousePosition();
                    if (m.x >= fieldX && m.x < fieldX + fieldW && m.y >= dropY && m.y < dropY + fieldH)
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dropdownOpen = !dropdownOpen;
                }

                if (dropdownOpen) {
                    const char* options[] = { "3D Structure", "3D Model" };
                    for (int oi = 0; oi < 2; oi++) {
                        int optY = dropY + fieldH + oi * fieldH;
                        Vector2 m = GetMousePosition();
                        bool hov = m.x >= fieldX && m.x < fieldX + fieldW && m.y >= optY && m.y < optY + fieldH;
                        DrawRectangle(fieldX, optY, fieldW, fieldH, hov ? Color{55,55,75,250} : Color{40,40,55,250});
                        DrawRectangleLines(fieldX, optY, fieldW, fieldH, {100,100,120,255});
                        DrawText(options[oi], fieldX + 8, optY + 10, 20, WHITE);
                        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            structureType = options[oi];
                            dropdownOpen = false;
                        }
                    }
                }

                // Size field (3D Model only)
                int extraH = 0;
                if (structureType == "3D Model") {
                    int sizeY = dropY + fieldH + 40;
                    extraH = fieldH + 40;
                    DrawText("Size (1-1000):", fieldX, sizeY - 24, 20, LIGHTGRAY);
                    Color sb = namingFocusSize ? WHITE : Color{100,100,120,255};
                    DrawRectangle(fieldX, sizeY, 120, fieldH, {30,30,40,255});
                    DrawRectangleLines(fieldX, sizeY, 120, fieldH, sb);
                    DrawText(modelSizeStr.c_str(), fieldX + 8, sizeY + 10, 20, WHITE);

                    if (namingFocusSize && ((int)(GetTime()*2.0)%2)==0) {
                        int cx = fieldX + 8 + MeasureText(modelSizeStr.c_str(), 20);
                        DrawRectangle(cx, sizeY + 8, 2, 24, WHITE);
                    }

                    Vector2 m = GetMousePosition();
                    if (m.x >= fieldX && m.x < fieldX + 120 && m.y >= sizeY && m.y < sizeY + fieldH)
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) namingFocusSize = true;
                }

                // Buttons
                int btnW2 = 180, btnH2 = 45, btnGap = 20;
                int btnRowY = dropY + fieldH + extraH + 80;
                int btnStartX = (screenW - (btnW2*2+btnGap)) / 2;

                if (drawMenuButton("Back", btnStartX, btnRowY, btnW2, btnH2)) {
                    state = STATE_MENU;
                }

                bool canCreate = !structureName.empty();
                if (structureType == "3D Model") {
                    int sz = modelSizeStr.empty() ? 0 : std::atoi(modelSizeStr.c_str());
                    canCreate = canCreate && sz >= 1 && sz <= 1000;
                }

                if (drawMenuButton("Create", btnStartX + btnW2 + btnGap, btnRowY, btnW2, btnH2, canCreate)) {
                    if (canCreate) {
                        fs::create_directories("saves");
                        savePath = "saves/" + sanitizeFilename(structureName) + ".sav";
                        placedBlocks.placeholders = {{0, 0, 0}};
                        placedBlocks.placed.clear();
                        placedBlocks.modelSize = 0;
                        placedBlocks.removed.clear();

                        if (structureType == "3D Model") {
                            int sz = std::atoi(modelSizeStr.c_str());
                            if (sz > 1000) sz = 1000;
                            if (sz < 1) sz = 1;
                            placedBlocks.modelSize = sz;
                        }

                        placedBlocks.rebuildPlaceholderSet();
                        autoSaveTimer = 0.0f;
                        editorCam = EditorCamera();
                        editorCam.init();
                        structEditor.reset();
                        modelEditor.reset();
                        saveStructure(savePath, structureName, structureType, placedBlocks, editorCam);
                        state = STATE_EDITOR;
                    }
                }

            EndDrawing();

        // ── Load Screen ────────────────────────────────────────
        } else if (state == STATE_LOAD) {
            BeginDrawing();
                ClearBackground({ 15, 15, 20, 255 });

                int screenW = GetScreenWidth();
                int screenH = GetScreenHeight();

                int titleW2 = MeasureText("Load Game", 40);
                DrawText("Load Game", (screenW - titleW2) / 2, screenH / 8, 40, WHITE);

                int listW = 500, itemH = 50, listX = (screenW - listW) / 2, listStartY = screenH / 4, maxVis = 8;

                if (saveEntries.empty()) {
                    DrawText("No saved structures found.", listX, listStartY, 20, GRAY);
                } else {
                    float wheel = GetMouseWheelMove();
                    if (wheel != 0.0f) {
                        loadScrollOffset -= (int)wheel;
                        if (loadScrollOffset < 0) loadScrollOffset = 0;
                        int mo = (int)saveEntries.size() - maxVis;
                        if (mo < 0) mo = 0;
                        if (loadScrollOffset > mo) loadScrollOffset = mo;
                    }

                    for (int i = 0; i < maxVis && (i + loadScrollOffset) < (int)saveEntries.size(); i++) {
                        int idx = i + loadScrollOffset;
                        int y = listStartY + i * (itemH + 4);
                        Vector2 m = GetMousePosition();
                        bool hov = m.x >= listX && m.x < listX + listW && m.y >= y && m.y < y + itemH;
                        if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedSave = idx;

                        Color bg = (idx == selectedSave) ? Color{60,60,90,230}
                                 : hov ? Color{45,45,60,230} : Color{30,30,40,230};
                        DrawRectangle(listX, y, listW, itemH, bg);
                        DrawRectangleLines(listX, y, listW, itemH, (idx==selectedSave) ? WHITE : Color{80,80,80,200});
                        DrawText(saveEntries[idx].name.c_str(), listX+12, y+8, 22, WHITE);
                        DrawText(saveEntries[idx].type.c_str(), listX+12, y+30, 14, LIGHTGRAY);
                    }

                    if ((int)saveEntries.size() > maxVis) {
                        int totalH = maxVis * (itemH + 4);
                        float ratio = (float)loadScrollOffset / (float)((int)saveEntries.size() - maxVis);
                        int barH = totalH / (int)saveEntries.size() * maxVis;
                        if (barH < 20) barH = 20;
                        int barY = listStartY + (int)(ratio * (totalH - barH));
                        DrawRectangle(listX + listW + 6, barY, 6, barH, {80,80,100,200});
                    }
                }

                int btnW2 = 180, btnH2 = 45, btnGap = 20;
                int btnRowY = listStartY + maxVis * (itemH + 4) + 30;
                int btnStartX = (screenW - (btnW2*2+btnGap)) / 2;

                if (drawMenuButton("Back", btnStartX, btnRowY, btnW2, btnH2)) {
                    state = STATE_MENU;
                }

                bool canLoad = selectedSave >= 0 && selectedSave < (int)saveEntries.size();
                if (drawMenuButton("Load", btnStartX + btnW2 + btnGap, btnRowY, btnW2, btnH2, canLoad)) {
                    if (canLoad) {
                        savePath = saveEntries[selectedSave].filepath;
                        editorCam = EditorCamera();
                        loadStructureInto(savePath, structureName, structureType, placedBlocks, editorCam);
                        autoSaveTimer = 0.0f;
                        structEditor.reset();
                        modelEditor.reset();
                        state = STATE_EDITOR;
                    }
                }

            EndDrawing();

        // ── Editor ─────────────────────────────────────────────
        } else if (state == STATE_EDITOR) {
            bool exitToMenu = false;

            if (structureType == "3D Structure") {
                exitToMenu = structEditor.frame(dt, editorCam, placedBlocks, sky,
                    savePath, structureName, structureType, autoSaveTimer);
            } else {
                exitToMenu = modelEditor.frame(dt, editorCam, placedBlocks, sky,
                    structEditor.blocks, savePath, structureName, structureType, autoSaveTimer);
            }

            if (exitToMenu) state = STATE_MENU;
        }
    }

    structEditor.hotbar.unload();
    structEditor.inventory.unload();
    CloseWindow();
    return 0;
}
