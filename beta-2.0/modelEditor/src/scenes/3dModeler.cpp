#include "3dModeler.h"
#include "../mechanics/Highlight.h"
#include "../mechanics/Selection.h"
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
#include "../../../VisualEngine/renderingManagement/ChunkMesh.h"
#include "../../../VisualEngine/inputManagement/Raycasting.h"
#include "../../../VisualEngine/renderingManagement/LineRenderer.h"
#include "../../../VisualEngine/renderingManagement/DotRenderer.h"
#include "../../../VisualEngine/renderingManagement/Overlay.h"
#include "../../../VisualEngine/renderingManagement/RenderToTexture.h"
#include <cmath>
#include <vector>
#include <fstream>
#include <iostream>
#include "../prefabs/3D_modeler/prefab_cube.h"
#include "../prefabs/EmbeddedSelectors.h"
#include "../prefabs/3D_modeler/prefab_wedge.h"

static bool sPaused = false;
static bool sWasEscDown = false;
static bool sWasLeftDown = false;
static Texture* sBlockSelectorPlus = nullptr;
static Texture* sBlockSelectorMinus = nullptr;
static const int SELECTOR_SLOTS = 15;
static std::string sSlotMesh[SELECTOR_SLOTS]; // empty = unassigned
static int sSelectedSlot = -1;
static RenderTarget sSlotRT[SELECTOR_SLOTS];
static float sGridCellW = 0, sGridCellH = 0, sGridPad = 0, sGridBtnX = 0, sGridY = 0;
static float sSideBtnW = 0, sSideBtnH = 0, sSideBtnX = 0, sSidePad = 0;
static Mesh* sSlotPreviewMesh[SELECTOR_SLOTS] = {};
static float sPreviewAngle = 0.0f;
static double sLastPreviewTime = 0.0;
static int sEditorMode = 0; // 0 = Build, 1 = Paint

#include "SceneData.h"

static int sPendingSlotUpdate = -1;
static std::string sPendingSlotMesh;
static bool sFirstEntry = true;
static std::string sModelName;
static ModelFile sCurrentModel;


static void rebuildSelectorIcons() {
    // Remove old selector icons and previews
    for (int i = 0; i < SELECTOR_SLOTS; i++) {
        removeFromGroup("sidebar", "block_" + std::to_string(i));
        removeFromGroup("sidebar", "block_" + std::to_string(i) + "_preview");
    }

    int cols = 5;
    int rows = 3;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            float cx = sGridBtnX + col * (sGridCellW + sGridPad);
            float cy = sGridY - row * (sGridCellH + sGridPad);
            int idx = row * cols + col;
            std::string id = "block_" + std::to_string(idx);

            unsigned int iconTex = (idx == sSelectedSlot)
                ? sBlockSelectorMinus->id : sBlockSelectorPlus->id;
            UIElement img = createImage(id, cx, cy, sGridCellW, sGridCellH, iconTex);
            img.onClick = [idx]() {
                if (!sSlotMesh[idx].empty()) {
                    sSelectedSlot = idx;
                    setCurrentMesh(sSlotMesh[idx]);
                    rebuildSelectorIcons();
                }
            };
            addToGroup("sidebar", img);

            if (!sSlotMesh[idx].empty()) {
                UIElement preview = createImage(id + "_preview", cx, cy, sGridCellW, sGridCellH,
                    sSlotRT[idx].textureId);
                addToGroup("sidebar", preview);
            }
        }
    }
}

static int ensureBlockType(const std::string& meshName) {
    // Find existing type
    for (int i = 0; i < (int)sCurrentModel.blockTypes.size(); i++)
        if (sCurrentModel.blockTypes[i].name == meshName) return i;

    // Add new type from registered mesh
    const RegisteredMesh* reg = getRegisteredMesh(meshName.c_str());
    if (!reg) return 0;

    BlockTypeDef bt;
    bt.name = meshName;
    bt.vertices = reg->vertices;
    bt.vertexCount = reg->vertexCount;
    bt.floatsPerVertex = reg->floatsPerVertex;
    bt.indices = reg->indices;
    bt.indexCount = reg->indexCount;
    bt.faceColors.resize(reg->indexCount / 3);
    sCurrentModel.blockTypes.push_back(bt);
    return (int)sCurrentModel.blockTypes.size() - 1;
}

static void saveCurrentModel() {
    // Rebuild placements from current colliders
    sCurrentModel.placements.clear();
    const auto& colliders = getAllColliders();
    for (const auto& col : colliders) {
        if (col.meshName == "_ghost") continue;
        BlockPlacement p;
        p.x = (int)roundf(col.position.x);
        p.y = (int)roundf(col.position.y);
        p.z = (int)roundf(col.position.z);
        p.typeId = ensureBlockType(col.meshName);
        p.rx = 0; p.ry = 0; p.rz = 0;
        sCurrentModel.placements.push_back(p);
    }
    setMemoryPath("assets/saves/3dModels");
    saveModel(sModelName, sCurrentModel);
}

static void openPauseMenu() {
    sPaused = true;

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
            sFirstEntry = true;
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
            VE::setGradientBackground(true);
            initOverlay();

            // Register cube mesh for ghost block collider
            VE::MeshDef cubeDef;
            cubeDef.vertices = cubeVertices;
            cubeDef.vertexCount = 24;
            cubeDef.indices = cubePlainIndices;
            cubeDef.indexCount = 36;
            cubeDef.texturePath = nullptr;
            VE::loadMesh("_ghost", cubeDef);
            registerMeshWithStates("cube", cubeVertices, 24, cubeIndices, 12);
            registerMeshWithStates("wedge", wedgeVertices, 18, wedgeIndices, 8);

            // Right sidebar
            float panelX = 0.6f;
            float panelW = 0.4f;
            float panelPad = 0.02f;
            float btnW = panelW - panelPad * 2;
            float btnH = 0.06f;
            float btnX = panelX + panelPad;
            float y = 0.82f;

            addUIGroup("sidebar");
            addToGroup("sidebar", createPanel("sidebar_bg",
                panelX, -1.0f, panelW, 2.0f,
                {0.12f, 0.12f, 0.12f, 0.85f}
            ));

            // Reset slots only on first entry
            if (sFirstEntry) {
                for (int i = 0; i < SELECTOR_SLOTS; i++)
                    sSlotMesh[i] = "";
                sSlotMesh[0] = "cube";
                sSlotMesh[1] = "wedge";

                // Restore custom slots from existing .mesh files
                for (int i = 0; i < SELECTOR_SLOTS; i++) {
                    if (!sSlotMesh[i].empty()) continue;
                    std::string slotName = "slot_" + std::to_string(i);
                    std::string meshPath = "assets/saves/vectorMeshes/" + slotName + ".mesh";
                    std::ifstream check(meshPath);
                    if (check.good()) {
                        check.close();
                        sSlotMesh[i] = slotName;
                    }
                }

                sSelectedSlot = 0;
                setCurrentMesh("cube");
                sFirstEntry = false;
            }

            // Load block selector textures
            sBlockSelectorPlus = new Texture(EMBEDDED_SELECTOR_PLUS, EMBEDDED_SELECTOR_PLUS_SIZE);
            sBlockSelectorMinus = new Texture(EMBEDDED_SELECTOR_MINUS, EMBEDDED_SELECTOR_MINUS_SIZE);
            sPreviewAngle = 0.0f;
            sLastPreviewTime = glfwGetTime();

            // Reload all custom slot meshes from files
            for (int i = 0; i < SELECTOR_SLOTS; i++) {
                if (sSlotMesh[i].empty() || sSlotMesh[i] == "cube" || sSlotMesh[i] == "wedge")
                    continue;
                std::string meshPath = "assets/saves/vectorMeshes/" + sSlotMesh[i] + ".mesh";
                std::ifstream checkFile(meshPath);
                if (checkFile.good()) {
                    checkFile.close();
                    VE::loadMesh(sSlotMesh[i].c_str(), meshPath.c_str());
                }
            }

            // Create render targets and preview meshes for slots
            for (int i = 0; i < SELECTOR_SLOTS; i++) {
                sSlotRT[i] = createRenderTarget(128, 128);
                // Clear the render target to transparent
                bindRenderTarget(sSlotRT[i]);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                unbindRenderTarget(ctx.width, ctx.height);
                sSlotPreviewMesh[i] = nullptr;
                if (!sSlotMesh[i].empty()) {
                    const RegisteredMesh* reg = getRegisteredMesh(sSlotMesh[i].c_str());
                    if (reg) {
                        if (reg->floatsPerVertex == 8)
                            sSlotPreviewMesh[i] = new Mesh(
                                const_cast<float*>(reg->vertices.data()), reg->vertexCount,
                                const_cast<unsigned int*>(reg->indices.data()), reg->indexCount, true);
                        else
                            sSlotPreviewMesh[i] = new Mesh(
                                const_cast<float*>(reg->vertices.data()), reg->vertexCount,
                                const_cast<unsigned int*>(reg->indices.data()), reg->indexCount);
                    }
                }
            }

            // Store grid layout constants
            {
                int cols = 5;
                int rows = 3;
                sGridPad = 0.005f;
                float aspect = (float)ctx.width / (float)ctx.height;
                sGridCellW = (btnW - sGridPad * (cols - 1)) / cols;
                sGridCellH = sGridCellW * aspect;
                sGridBtnX = btnX;
                sGridY = y;
                y = sGridY - rows * (sGridCellH + sGridPad) - panelPad;
            }
            rebuildSelectorIcons();

            // Store sidebar layout for later use
            sSideBtnW = btnW;
            sSideBtnH = btnH;
            sSideBtnX = btnX;
            sSidePad = panelPad;

            // Edit Object button
            addToGroup("sidebar", createButton("edit_object",
                btnX, y, btnW, btnH,
                {0.25f, 0.25f, 0.3f, 0.95f}, "Edit Object",
                []() {
                    saveCurrentModel();

                    std::string meshName = sSlotMesh[sSelectedSlot].empty()
                        ? "slot_" + std::to_string(sSelectedSlot)
                        : sSlotMesh[sSelectedSlot];

                    sPendingSlotUpdate = sSelectedSlot;
                    sPendingSlotMesh = meshName;

                    auto* editData = new VectorMeshEditData{meshName, sSelectedSlot, sModelName};
                    VE::setScene("vectorMesh", editData);
                }
            ));
            y -= btnH + panelPad;

            // Editor mode dropdown
            createDropdown("sidebar", "editor_mode",
                btnX, y, btnW, btnH,
                {0.18f, 0.18f, 0.18f, 0.95f},
                "Build",
                {"Build", "Paint"},
                [](int index, const std::string& option) {
                    sEditorMode = index;
                    clearSelection();
                },
                0.0f, -(btnH + panelPad)
            );
            y -= btnH + panelPad;

            // Load model if name was passed
            if (data) {
                std::string* name = static_cast<std::string*>(data);
                sModelName = *name;
                setMemoryPath("assets/saves/3dModels");
                if (loadModel(sModelName, sCurrentModel)) {
                    std::cout << "[3dModeler] Block types: " << sCurrentModel.blockTypes.size()
                              << " Placements: " << sCurrentModel.placements.size() << std::endl;
                    for (int i = 0; i < (int)sCurrentModel.blockTypes.size(); i++) {
                        const BlockTypeDef& bt = sCurrentModel.blockTypes[i];
                        if (bt.floatsPerVertex == 8) {
                            // Register with pre-computed normals
                            RegisteredMesh reg;
                            reg.vertices = bt.vertices;
                            reg.vertexCount = bt.vertexCount;
                            reg.floatsPerVertex = 8;
                            reg.indices = bt.indices;
                            reg.indexCount = bt.indexCount;
                            reg.rectangular = false;
                            // Use the internal registration
                            VE::MeshDef def;
                            def.vertices = const_cast<float*>(bt.vertices.data());
                            def.vertexCount = bt.vertexCount;
                            def.indices = const_cast<unsigned int*>(bt.indices.data());
                            def.indexCount = bt.indexCount;
                            def.texturePath = nullptr;
                            VE::loadMesh(bt.name.c_str(), def);
                            // Override with 8-float data
                            auto* regPtr = const_cast<RegisteredMesh*>(getRegisteredMesh(bt.name.c_str()));
                            if (regPtr) {
                                regPtr->vertices = bt.vertices;
                                regPtr->floatsPerVertex = 8;
                                regPtr->rectangular = false;
                            }
                        } else {
                            VE::MeshDef def;
                            def.vertices = const_cast<float*>(bt.vertices.data());
                            def.vertexCount = bt.vertexCount;
                            def.indices = const_cast<unsigned int*>(bt.indices.data());
                            def.indexCount = bt.indexCount;
                            def.texturePath = nullptr;
                            VE::loadMesh(bt.name.c_str(), def);
                        }
                    }
                    for (const auto& p : sCurrentModel.placements) {
                        std::cout << "[3dModeler] Place typeId=" << p.typeId << " at(" << p.x << "," << p.y << "," << p.z << ")" << std::endl;
                        if (p.typeId < (int)sCurrentModel.blockTypes.size())
                            VE::draw(sCurrentModel.blockTypes[p.typeId].name.c_str(),
                                     (float)p.x, (float)p.y, (float)p.z);
                    }
                }
                delete name;
            }

            // Apply pending slot update from vector mesh editor
            if (sPendingSlotUpdate >= 0) {
                std::string meshPath = "assets/saves/vectorMeshes/" + sPendingSlotMesh + ".mesh";
                std::ifstream check(meshPath);
                if (check.good()) {
                    check.close();
                    VE::loadMesh(sPendingSlotMesh.c_str(), meshPath.c_str());
                    sSlotMesh[sPendingSlotUpdate] = sPendingSlotMesh;
                    sSelectedSlot = sPendingSlotUpdate;
                    setCurrentMesh(sPendingSlotMesh);

                    // Create preview mesh for the slot
                    const RegisteredMesh* reg = getRegisteredMesh(sPendingSlotMesh.c_str());
                    if (reg) {
                        delete sSlotPreviewMesh[sPendingSlotUpdate];
                        if (reg->floatsPerVertex == 8)
                            sSlotPreviewMesh[sPendingSlotUpdate] = new Mesh(
                                const_cast<float*>(reg->vertices.data()), reg->vertexCount,
                                const_cast<unsigned int*>(reg->indices.data()), reg->indexCount, true);
                        else
                            sSlotPreviewMesh[sPendingSlotUpdate] = new Mesh(
                                const_cast<float*>(reg->vertices.data()), reg->vertexCount,
                                const_cast<unsigned int*>(reg->indices.data()), reg->indexCount);
                    }
                    rebuildSelectorIcons();
                }
                sPendingSlotUpdate = -1;
                sPendingSlotMesh = "";
            }

            // Add ghost collider at origin if no real block there
            if (!VE::hasBlockAt(0, 0, 0)) {
                const RegisteredMesh* reg = getRegisteredMesh("_ghost");
                if (reg)
                    addCollider("_ghost", reg->vertices.data(), reg->vertexCount,
                                reg->indices.data(), reg->indexCount, reg->rectangular, 0, 0, 0);
            }
        },
        // onExit
        []() {
            cleanupHighlight();
            cleanupOverlay();
            delete sBlockSelectorPlus;
            delete sBlockSelectorMinus;
            sBlockSelectorPlus = nullptr;
            sBlockSelectorMinus = nullptr;
            for (int i = 0; i < SELECTOR_SLOTS; i++) {
                destroyRenderTarget(sSlotRT[i]);
                delete sSlotPreviewMesh[i];
                sSlotPreviewMesh[i] = nullptr;
            }
            cleanupTextRenderer();
            cleanupUIRenderer();
            clearUI();
            VE::clearDraws();
            VE::setGradientBackground(false);
        },
        // onInput
        [](float dt) {
            // Build mode = AABB selection, Paint mode = per-triangle
            setForceRectangularRaycast(sEditorMode == 0);

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

            // Scroll to cycle through all slots (including empty)
            if (!sPaused && ctx.scrollDelta != 0.0f) {
                int dir = (ctx.scrollDelta > 0.0f) ? -1 : 1;
                sSelectedSlot = (sSelectedSlot + dir + SELECTOR_SLOTS) % SELECTOR_SLOTS;
                if (!sSlotMesh[sSelectedSlot].empty())
                    setCurrentMesh(sSlotMesh[sSelectedSlot]);
                rebuildSelectorIcons();
            }

            processUIInput();
        },
        // onUpdate
        []() {
            // Re-add ghost collider at origin if block was removed
            if (!VE::hasBlockAt(0, 0, 0)) {
                const BlockCollider* col = getColliderAt(0, 0, 0);
                if (!col) {
                    const RegisteredMesh* reg = getRegisteredMesh("_ghost");
                    if (reg)
                        addCollider("_ghost", reg->vertices.data(), reg->vertexCount,
                                    reg->indices.data(), reg->indexCount, reg->rectangular, 0, 0, 0);
                }
            }
        },
        // onRender
        []() {
            // Apply brightness dim while paused
            if (sPaused)
                VE::setBrightness(0.5f);

            // Render preview meshes into slot render targets
            double now = glfwGetTime();
            float dt = (float)(now - sLastPreviewTime);
            sLastPreviewTime = now;
            sPreviewAngle += 15.0f * dt; // 15 degrees per second
            if (sPreviewAngle > 360.0f) sPreviewAngle -= 360.0f;

            for (int i = 0; i < SELECTOR_SLOTS; i++) {
                if (!sSlotPreviewMesh[i]) continue;

                bindRenderTarget(sSlotRT[i]);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                ctx.shader->use();

                glm::mat4 previewView = glm::lookAt(
                    glm::vec3(3.0f, 2.0f, 3.0f),
                    glm::vec3(0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );
                glm::mat4 previewProj = glm::perspective(glm::radians(35.0f), 1.0f, 0.1f, 20.0f);
                glm::mat4 previewModel = glm::rotate(glm::mat4(1.0f),
                    glm::radians(sPreviewAngle), glm::vec3(0.0f, 1.0f, 0.0f));

                glUniformMatrix4fv(ctx.shader->loc("view"), 1, GL_FALSE, glm::value_ptr(previewView));
                glUniformMatrix4fv(ctx.shader->loc("projection"), 1, GL_FALSE, glm::value_ptr(previewProj));
                glUniform3f(ctx.shader->loc("viewPos"), 3.0f, 2.0f, 3.0f);
                glUniform3f(ctx.shader->loc("lightPos"), 3.0f, 5.0f, 3.0f);

                glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(previewModel)));
                glUniformMatrix4fv(ctx.shader->loc("model"), 1, GL_FALSE, glm::value_ptr(previewModel));
                glUniformMatrix3fv(ctx.shader->loc("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMat));

                sSlotPreviewMesh[i]->draw(*ctx.shader);

                unbindRenderTarget(ctx.width, ctx.height);
            }

            // Restore main shader state
            ctx.shader->use();
            glm::mat4 identity(1.0f);
            ctx.scene->uploadFrameUniforms(*ctx.shader, identity);
            glUniformMatrix4fv(ctx.shader->loc("view"), 1, GL_FALSE, glm::value_ptr(ctx.scene->view));
            glUniformMatrix4fv(ctx.shader->loc("projection"), 1, GL_FALSE, glm::value_ptr(ctx.scene->projection));
            Camera* cam = getGlobalCamera();
            glUniform3fv(ctx.shader->loc("viewPos"), 1, glm::value_ptr(cam->position));
            glm::vec3 lightPos = cam->position + glm::vec3(0.0f, 2.0f, 0.0f);
            glUniform3fv(ctx.shader->loc("lightPos"), 1, glm::value_ptr(lightPos));

            // Draw ghost block at origin if only ghost collider is there
            const BlockCollider* ghostCol = getColliderAt(0, 0, 0);
            if (!ghostCol || ghostCol->meshName == "_ghost") {
                ctx.shader->use();
                AABB ghostBox;
                ghostBox.min = glm::vec3(-0.5f);
                ghostBox.max = glm::vec3(0.5f);
                glm::vec3 ghostColor(0.5f);
                for (int face = 0; face < 6; face++) {
                    Triangle t0 = aabbFaceTriangle(ghostBox, face, 0);
                    Triangle t1 = aabbFaceTriangle(ghostBox, face, 1);
                    drawTriangleOverlay(*ctx.shader, t0, ghostColor, 0.15f);
                    drawTriangleOverlay(*ctx.shader, t1, ghostColor, 0.15f);
                }
            }

            if (!sPaused && sEditorMode == 0)
                renderHoverHighlight();

            renderUI();
        }
    );
}
