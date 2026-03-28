#include "engine/engine.h"
#include "gradient_sky.h"
#include "editor_camera.h"
#include "placed_blocks.h"
#include "screen_menu.h"
#include "screen_naming.h"
#include "screen_load.h"
#include "editor_structure.h"
#include "editor_model.h"
#include "editor_sprite.h"
#include <string>
#include <filesystem>

int main() {
    // Create all required folders upfront to avoid race conditions on first run
    {
        namespace fs = std::filesystem;
        fs::create_directories("saves/models");
        fs::create_directories("saves/blocks");
        fs::create_directories("saves/structures");
        fs::create_directories("saves/sprites");
        fs::create_directories("AbstractModels");
        fs::create_directories("assets/entities/blocks");
        fs::create_directories("assets/entities/blockModels");
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Map Creator");

    int monitor = GetCurrentMonitor();
    int monW = GetMonitorWidth(monitor);
    int monH = GetMonitorHeight(monitor);
    if (monW > 0 && monH > 0) SetWindowSize(monW, monH);
    ToggleFullscreen();

    SetTargetFPS(60);
    SetExitKey(0);

    AppState state = STATE_MENU;

    PlacedBlocks placedBlocks;
    GradientSky sky;
    EditorCamera editorCam;
    std::string structureName;
    std::string structureType = "3D Structure";
    std::string savePath;
    float autoSaveTimer = 0.0f;

    MenuScreen menuScreen;
    NamingScreen namingScreen;
    LoadScreen loadScreen;
    StructureEditor structEditor;
    ModelEditor modelEditor;
    SpriteEditor spriteEditor;

    bool running = true;

    while (running && !WindowShouldClose()) {
        float dt = GetFrameTime();

        switch (state) {
            case STATE_MENU: {
                AppState next = menuScreen.drawAndUpdate();
                if (next == (AppState)-1) { running = false; break; }
                if (next == STATE_NAMING) namingScreen.reset();
                if (next == STATE_LOAD) loadScreen.refresh();
                state = next;
                break;
            }
            case STATE_NAMING: {
                state = namingScreen.drawAndUpdate(placedBlocks, editorCam, structEditor, modelEditor,
                                                   spriteEditor, savePath, structureName, structureType, autoSaveTimer);
                break;
            }
            case STATE_LOAD: {
                state = loadScreen.drawAndUpdate(placedBlocks, editorCam, structEditor, modelEditor,
                                                  spriteEditor, savePath, structureName, structureType, autoSaveTimer);
                break;
            }
            case STATE_EDITOR: {
                bool exitToMenu = false;
                if (structureType == "3D Structure")
                    exitToMenu = structEditor.frame(dt, editorCam, placedBlocks, sky,
                                     savePath, structureName, structureType, autoSaveTimer);
                else if (structureType == "3D Sprite")
                    exitToMenu = spriteEditor.frame(dt, editorCam, placedBlocks, sky,
                                     savePath, structureName, structureType, autoSaveTimer);
                else
                    exitToMenu = modelEditor.frame(dt, editorCam, placedBlocks, sky,
                                     structEditor.blocks, savePath, structureName, structureType, autoSaveTimer);
                if (exitToMenu) state = STATE_MENU;
                break;
            }
        }
    }

    structEditor.hotbar.unload();
    structEditor.inventory.unload();
    spriteEditor.unloadAll();
    CloseWindow();
    return 0;
}
