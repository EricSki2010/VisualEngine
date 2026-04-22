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

static void showMainMenu();
static void showLoadMenu();
static void showFileList(const std::string& type);

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
        BTN_X, y, BTN_W, BTN_H,
        {0.3f, 0.1f, 0.1f, 0.9f},
        "Exit",
        []() { glfwSetWindowShouldClose(ctx.window, true); }
    ));
}

static void showLoadMenu() {
    sLoadView = true;
    removeUIGroup("menu_buttons");
    addUIGroup("menu_buttons");

    float y = START_Y;

    createDropdown("menu_buttons", "select_file",
        BTN_X, y, BTN_W, BTN_H, BTN_COLOR,
        "Select File Type...",
        {"3D Model"},
        [](int index, const std::string& option) {
            if (option == "3D Model") showFileList("3dModels");
        },
        0.0f, -GAP
    );
    y -= GAP;

    addToGroup("menu_buttons", createButton("back",
        BTN_X, y, BTN_W, BTN_H,
        {0.3f, 0.1f, 0.1f, 0.9f},
        "Back",
        []() { showMainMenu(); }
    ));
}

static void showFileList(const std::string& type) {
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
                    float subH = BTN_H * 0.8f;
                    float pad = BTN_H * 0.1f;
                    float aspect = (float)ctx.width / (float)ctx.height;
                    float subW = subH / aspect;
                    float subX = BTN_X + BTN_W - subW - pad;
                    float subY = y + pad;

                    float capturedSubX = subX, capturedSubY = subY;
                    float capturedSubW = subW, capturedSubH = subH;
                    UIElement menuBtn = createButton(btnId + "_menu",
                        subX, subY, subW, subH,
                        {0.35f, 0.35f, 0.35f, 1.0f}, ". . .",
                        [name, capturedSubX, capturedSubY, capturedSubW, capturedSubH]() {
                            // Toggle: close if already open
                            if (getUIElement("file_dropdown", "fd_delete")) {
                                removeUIGroup("file_dropdown");
                                return;
                            }

                            addUIGroup("file_dropdown");
                            float dropX = capturedSubX + capturedSubW + capturedSubW * 0.5f;
                            float dropH = BTN_H * 0.7f;
                            float dropGap = dropH + 0.005f;

                            addToGroup("file_dropdown", createButton("fd_delete",
                                dropX, capturedSubY, 0.15f, dropH,
                                {0.5f, 0.1f, 0.1f, 0.95f}, "Delete",
                                [name]() {
                                    std::filesystem::remove("assets/saves/3dModels/" + name + ".mdl");
                                    removeUIGroup("file_dropdown");
                                    showFileList("3dModels");
                                }
                            ));

                            addToGroup("file_dropdown", createButton("fd_duplicate",
                                dropX, capturedSubY - dropGap, 0.15f, dropH,
                                {0.2f, 0.2f, 0.4f, 0.95f}, "Duplicate",
                                [name]() {
                                    std::string src = "assets/saves/3dModels/" + name + ".mdl";
                                    std::string dst = "assets/saves/3dModels/" + name + "_copy.mdl";
                                    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::skip_existing);
                                    removeUIGroup("file_dropdown");
                                    showFileList("3dModels");
                                }
                            ));
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
        []() { removeUIGroup("file_dropdown"); showLoadMenu(); }
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
