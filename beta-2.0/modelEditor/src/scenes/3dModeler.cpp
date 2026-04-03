#include "3dModeler.h"
#include "../mechanics/Highlight.h"
#include "../../../VisualEngine/VisualEngine.h"
#include "../../../VisualEngine/EngineGlobals.h"
#include "../../../VisualEngine/inputManagement/Camera.h"
#include "../../../VisualEngine/uiManagement/UIManager.h"
#include "../../../VisualEngine/uiManagement/UIRenderer.h"
#include "../../../VisualEngine/uiManagement/UIPrefabs.h"
#include "../../../VisualEngine/uiManagement/TextRenderer.h"
#include "../../../VisualEngine/uiManagement/EmbeddedFont.h"
#include "../../../VisualEngine/memoryManagement/memory.h"
#include "../../../VisualEngine/inputManagement/Collision.h"
#include "../../../VisualEngine/inputManagement/Raycasting.h"
#include "../../../VisualEngine/renderingManagement/LineRenderer.h"
#include "../../../VisualEngine/renderingManagement/DotRenderer.h"
#include <cmath>
#include <vector>
#include "../prefabs/3D_modeler/prefab_cube.h"

static bool sPaused = false;
static bool sWasEscDown = false;
static bool sWasLeftDown = false;
static std::string sModelName;
static ModelFile sCurrentModel;


static void saveCurrentModel() {
    // Rebuild placements from current colliders
    sCurrentModel.placements.clear();
    const auto& colliders = getAllColliders();
    for (const auto& col : colliders) {
        BlockPlacement p;
        p.x = (int)roundf(col.position.x);
        p.y = (int)roundf(col.position.y);
        p.z = (int)roundf(col.position.z);
        // Find type ID by mesh name
        p.typeId = 0;
        for (int i = 0; i < (int)sCurrentModel.blockTypes.size(); i++) {
            if (sCurrentModel.blockTypes[i].name == col.meshName) {
                p.typeId = i;
                break;
            }
        }
        p.rx = 0; p.ry = 0; p.rz = 0;
        sCurrentModel.placements.push_back(p);
    }
    setMemoryPath("assets/saves/3dModels");
    saveModel(sModelName, sCurrentModel);
}

static void openPauseMenu() {
    sPaused = true;
    VE::setBrightness(0.5f);

    // Exit looking mode if active
    Camera* cam = getGlobalCamera();
    if (cam->looking) {
        cam->looking = false;
        glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    addUIGroup("pause_menu");

    float btnW = 0.3f;
    float btnH = 0.08f;
    float btnX = -btnW / 2.0f;
    glm::vec4 btnColor = {0.15f, 0.15f, 0.15f, 0.95f};

    addToGroup("pause_menu", createButton("continue",
        btnX, 0.05f, btnW, btnH, btnColor,
        "Continue",
        []() {
            sPaused = false;
            VE::setBrightness(1.0f);
            removeUIGroup("pause_menu");
        }
    ));

    addToGroup("pause_menu", createButton("exit",
        btnX, -0.08f, btnW, btnH, btnColor,
        "Save & Exit",
        []() {
            saveCurrentModel();
            sPaused = false;
            VE::setBrightness(1.0f);
            removeUIGroup("pause_menu");
            VE::setScene("menu");
        }
    ));
}

void register3dModelerScene() {
    VE::registerScene("3dModeler",
        // onEnter
        [](void* data) {
            sPaused = false;
            getGlobalCamera()->setMode(CAMERA_FPS);
            initHighlight();
            initUIRenderer();
            initTextRendererFromMemory(EMBEDDED_FONT_DATA, EMBEDDED_FONT_SIZE, 48);
            VE::setBrightness(1.0f);
            VE::setCamera(6, 4, 6, 210, -25);

            // Load model if name was passed
            if (data) {
                std::string* name = static_cast<std::string*>(data);
                sModelName = *name;
                setMemoryPath("assets/saves/3dModels");
                if (loadModel(sModelName, sCurrentModel)) {
                    for (int i = 0; i < (int)sCurrentModel.blockTypes.size(); i++) {
                        const BlockTypeDef& bt = sCurrentModel.blockTypes[i];
                        VE::MeshDef def;
                        def.vertices = const_cast<float*>(bt.vertices.data());
                        def.vertexCount = bt.vertexCount;
                        def.indices = const_cast<unsigned int*>(bt.indices.data());
                        def.indexCount = bt.indexCount;
                        def.texturePath = nullptr;
                        VE::loadMesh(bt.name.c_str(), def);
                    }
                    for (const auto& p : sCurrentModel.placements) {
                        if (p.typeId < (int)sCurrentModel.blockTypes.size())
                            VE::draw(sCurrentModel.blockTypes[p.typeId].name.c_str(),
                                     (float)p.x, (float)p.y, (float)p.z);
                    }
                }
                delete name;
            }
        },
        // onExit
        []() {
            cleanupHighlight();
            cleanupTextRenderer();
            cleanupUIRenderer();
            clearUI();
            VE::clearDraws();
        },
        // onInput
        [](float dt) {
            bool escDown = glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !sWasEscDown) {
                if (sPaused) {
                    sPaused = false;
                    VE::setBrightness(1.0f);
                    removeUIGroup("pause_menu");
                } else {
                    openPauseMenu();
                }
            }
            sWasEscDown = escDown;

            processUIInput();
        },
        // onUpdate
        nullptr,
        // onRender
        []() {
            if (!sPaused)
                renderHoverHighlight();

            renderUI();
        }
    );
}
