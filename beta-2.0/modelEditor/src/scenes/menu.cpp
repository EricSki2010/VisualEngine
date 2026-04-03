#include "menu.h"
#include "../../../VisualEngine/VisualEngine.h"
#include "../../../VisualEngine/EngineGlobals.h"
#include "../../../VisualEngine/inputManagement/Camera.h"
#include "../../../VisualEngine/uiManagement/UIManager.h"
#include "../../../VisualEngine/uiManagement/UIRenderer.h"
#include "../../../VisualEngine/uiManagement/UIPrefabs.h"
#include "../../../VisualEngine/uiManagement/TextRenderer.h"
#include "../../../VisualEngine/uiManagement/EmbeddedFont.h"
#include <filesystem>

static bool sLoadView = false;
static bool sFileTypeOpen = false;
static std::string sFileDropdownOpen;
static std::string sCurrentFileType;

static void showMainMenu();
static void showLoadMenu();
static void showFileTypeDropdown();
static void showFileList(const std::string& type);
static void toggleFileDropdown(const std::string& name, float subX, float subY, float subW, float subH);

static const float BTN_W = 0.4f;
static const float BTN_H = 0.08f;
static const float BTN_X = -BTN_W / 2.0f;
static const float GAP = 0.12f;
static const float START_Y = 0.1f;
static const glm::vec4 BTN_COLOR = {0.2f, 0.2f, 0.2f, 0.9f};

static void showMainMenu() {
    sLoadView = false;
    removeUIGroup("menu_buttons");
    addUIGroup("menu_buttons");

    float y = START_Y;

    addToGroup("menu_buttons", createButton("create_new",
        BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
        "Create New",
        []() { VE::setScene("createScene"); }
    ));
    y -= GAP;

    addToGroup("menu_buttons", createButton("load",
        BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
        "Load",
        []() { showLoadMenu(); }
    ));
    y -= GAP;

    addToGroup("menu_buttons", createButton("exit",
        BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
        "Exit",
        []() { glfwSetWindowShouldClose(ctx.window, true); }
    ));
}

static void showLoadMenu() {
    sLoadView = true;
    sFileTypeOpen = false;
    removeUIGroup("menu_buttons");
    removeUIGroup("dropdown");
    addUIGroup("menu_buttons");

    float y = START_Y;

    addToGroup("menu_buttons", createButton("select_file",
        BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
        "Select File Type...",
        []() { showFileTypeDropdown(); }
    ));
    y -= GAP;

    addToGroup("menu_buttons", createButton("back",
        BTN_X, y, BTN_W, BTN_H,
        {0.3f, 0.1f, 0.1f, 0.9f},
        "Back",
        []() { showMainMenu(); }
    ));
}

static void showFileTypeDropdown() {
    if (sFileTypeOpen) {
        removeUIGroup("dropdown");
        sFileTypeOpen = false;
        return;
    }
    sFileTypeOpen = true;
    addUIGroup("dropdown");
    addToGroup("dropdown", createButton("type_3dmodel",
        BTN_X, START_Y - GAP, BTN_W, BTN_H,
        {0.25f, 0.25f, 0.25f, 0.95f},
        "3D Model",
        []() { showFileList("3dModels"); }
    ));
}

static void closeFileDropdown() {
    removeUIGroup("file_dropdown");
    sFileDropdownOpen = "";
}

static void toggleFileDropdown(const std::string& name, float subX, float subY, float subW, float subH) {
    if (sFileDropdownOpen == name) {
        closeFileDropdown();
        return;
    }

    closeFileDropdown();
    sFileDropdownOpen = name;
    addUIGroup("file_dropdown");

    float dropX = subX + subW + subW * 0.5f;
    float dropW = 0.15f;
    float dropH = BTN_H * 0.7f;
    float dropGap = dropH + 0.005f;
    float dropY = subY;

    addToGroup("file_dropdown", createButton("fd_delete",
        dropX, dropY, dropW, dropH,
        {0.5f, 0.1f, 0.1f, 0.95f}, "Delete",
        [name]() {
            std::string path = "assets/saves/3dModels/" + name + ".mdl";
            std::filesystem::remove(path);
            closeFileDropdown();
            showFileList("3dModels");
        }
    ));

    addToGroup("file_dropdown", createButton("fd_duplicate",
        dropX, dropY - dropGap, dropW, dropH,
        {0.2f, 0.2f, 0.4f, 0.95f}, "Duplicate",
        [name]() {
            std::string srcPath = "assets/saves/3dModels/" + name + ".mdl";
            std::string dstPath = "assets/saves/3dModels/" + name + "_copy.mdl";
            std::filesystem::copy_file(srcPath, dstPath, std::filesystem::copy_options::skip_existing);
            closeFileDropdown();
            showFileList("3dModels");
        }
    ));
}

static void showFileList(const std::string& type) {
    sFileTypeOpen = false;
    removeUIGroup("menu_buttons");
    removeUIGroup("dropdown");
    addUIGroup("menu_buttons");

    float y = START_Y;
    std::string dir = "assets/saves/" + type;

    if (std::filesystem::exists(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".mdl") {
                std::string name = entry.path().stem().string();
                std::string btnId = "file_" + name;

                UIElement fileBtn = createButton(btnId,
                    BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
                    name,
                    [name]() {
                        VE::setScene("3dModeler", new std::string(name));
                    }
                );
                fileBtn.requireConfirm = true;
                fileBtn.confirmId = "load_file";
                addToGroup("menu_buttons", fileBtn);

                {
                    // Square button on the right: use parent height for both w and h
                    float subH = BTN_H * 0.8f;
                    float pad = BTN_H * 0.1f;
                    float aspect = (float)ctx.width / (float)ctx.height;
                    float subW = subH / aspect; // compensate for widescreen
                    float subX = BTN_X + BTN_W - subW - pad;
                    float subY = y + pad;

                    float capturedSubX = subX, capturedSubY = subY;
                    float capturedSubW = subW, capturedSubH = subH;
                    UIElement menuBtn = createButton(btnId + "_menu",
                        subX, subY, subW, subH,
                        {0.35f, 0.35f, 0.35f, 1.0f}, ". . .",
                        [name, capturedSubX, capturedSubY, capturedSubW, capturedSubH]() {
                            toggleFileDropdown(name, capturedSubX, capturedSubY, capturedSubW, capturedSubH);
                        }
                    );
                    menuBtn.labelScale = 0.2f;
                    addToGroup("menu_buttons", menuBtn);
                }

                y -= GAP;
            }
        }
    }

    // Load button — confirms the file selection
    UIElement loadBtn = createButton("load_confirm",
        BTN_X, y, BTN_W, BTN_H,
        {0.8f, 0.7f, 0.0f, 0.95f},
        "Load",
        nullptr
    );
    loadBtn.confirmId = "load_file";
    addToGroup("menu_buttons", loadBtn);
    y -= GAP;

    addToGroup("menu_buttons", createButton("back",
        BTN_X, y, BTN_W, BTN_H,
        {0.3f, 0.1f, 0.1f, 0.9f},
        "Back",
        []() { showLoadMenu(); }
    ));
}

void registerMenuScene() {
    VE::registerScene("menu",
        // onEnter
        [](void* data) {
            sLoadView = false;
            getGlobalCamera()->setMode(CAMERA_FLAT);
            initUIRenderer();
            initTextRendererFromMemory(EMBEDDED_FONT_DATA, EMBEDDED_FONT_SIZE, 48);
            showMainMenu();
        },
        // onExit
        []() {
            cleanupTextRenderer();
            cleanupUIRenderer();
            clearUI();
        },
        // onInput
        [](float dt) {
            processUIInput();
        },
        // onUpdate
        nullptr,
        // onRender
        []() {
            renderUI();
        }
    );
}
