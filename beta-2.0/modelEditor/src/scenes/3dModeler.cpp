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
#include <functional>
#include <algorithm>
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
static bool sColorEditOpen = false;
static int sColorMode = 0; // 0 = RGB, 1 = Hex, 2 = Color Wheel
static std::function<void()> sRebuildColorInputs;
static std::function<void()> sRebuildActionButton;
static int sSelectedColor = 0; // which color wheel slice is selected
static glm::vec3 sColorWheel[8] = {
    {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
    {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
};

static unsigned int sWheelVAO = 0, sWheelVBO = 0;

static void initColorWheel() {
    glGenVertexArrays(1, &sWheelVAO);
    glGenBuffers(1, &sWheelVBO);
    glBindVertexArray(sWheelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sWheelVBO);
    glBufferData(GL_ARRAY_BUFFER, 8 * 3 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    // pos(2) + uv(2) = 4 floats per vertex
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static void drawColorWheel(Shader* uiShader) {
    float aspect = (float)ctx.width / (float)ctx.height;
    float radius = 0.6f;
    float cx = 1.0f;  // bottom-right corner
    float cy = -1.0f;

    float verts[8 * 3 * 4]; // 8 triangles * 3 verts * 4 floats
    int vi = 0;
    for (int i = 0; i < 8; i++) {
        float a1 = glm::radians(90.0f + i * 11.25f);
        float a2 = glm::radians(90.0f + (i + 1) * 11.25f);

        // center point
        verts[vi++] = cx; verts[vi++] = cy; verts[vi++] = 0; verts[vi++] = 0;
        // edge point 1
        verts[vi++] = cx + cosf(a1) * radius / aspect;
        verts[vi++] = cy + sinf(a1) * radius;
        verts[vi++] = 0; verts[vi++] = 0;
        // edge point 2
        verts[vi++] = cx + cosf(a2) * radius / aspect;
        verts[vi++] = cy + sinf(a2) * radius;
        verts[vi++] = 0; verts[vi++] = 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, sWheelVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    uiShader->use();
    glUniform1i(uiShader->loc("uUseTexture"), 0);
    glUniform2f(uiShader->loc("uPosition"), 0.0f, 0.0f);
    glUniform2f(uiShader->loc("uSize"), 1.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(sWheelVAO);

    // Draw each triangle slice
    for (int i = 0; i < 8; i++) {
        glm::vec3 c = sColorWheel[i];
        float alpha = (i == sSelectedColor) ? 1.0f : 0.8f;
        glUniform4f(uiShader->loc("uColor"), c.r, c.g, c.b, alpha);
        glDrawArrays(GL_TRIANGLES, i * 3, 3);
    }

    // Draw black outlines first
    glLineWidth(2.0f);
    for (int i = 0; i < 8; i++) {
        if (i == sSelectedColor) continue;
        glUniform4f(uiShader->loc("uColor"), 0.0f, 0.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_LOOP, i * 3, 3);
    }
    // Draw selected slice and outline on top
    if (sSelectedColor >= 0 && sSelectedColor < 8) {
        glm::vec3 c = sColorWheel[sSelectedColor];
        glUniform4f(uiShader->loc("uColor"), c.r, c.g, c.b, 1.0f);
        glDrawArrays(GL_TRIANGLES, sSelectedColor * 3, 3);
        glLineWidth(3.0f);
        glUniform4f(uiShader->loc("uColor"), 1.0f, 1.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_LOOP, sSelectedColor * 3, 3);
    }
    glLineWidth(1.0f);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

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

    // Only show in build mode
    if (sEditorMode != 0) return;

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

int getSelectedPaintColor() {
    return sSelectedColor;
}

static void saveCurrentModel() {
    // Save palette
    for (int i = 0; i < 8; i++)
        sCurrentModel.palette[i] = sColorWheel[i];

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
        p.rx = (int)col.rotation.x; p.ry = (int)col.rotation.y; p.rz = (int)col.rotation.z;
        p.triColors = col.triColors;
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
            initColorWheel();
            setPaintPalette(sColorWheel);

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

            // Store grid layout constants (anchored to bottom of panel)
            {
                int cols = 5;
                int rows = 3;
                sGridPad = 0.005f;
                float aspect = (float)ctx.width / (float)ctx.height;
                sGridCellW = (btnW - sGridPad * (cols - 1)) / cols;
                sGridCellH = sGridCellW * aspect;
                sGridBtnX = btnX;
                // Position grid at bottom of panel
                float gridTotalH = rows * (sGridCellH + sGridPad) - sGridPad;
                sGridY = -1.0f + panelPad + gridTotalH;
            }
            rebuildSelectorIcons();

            // Store sidebar layout for later use
            sSideBtnW = btnW;
            sSideBtnH = btnH;
            sSideBtnX = btnX;
            sSidePad = panelPad;

            // Action button (changes based on editor mode)
            {
                float actionBtnY = y;
                auto rebuildActionButton = [btnX, btnW, btnH, actionBtnY]() {
                    removeFromGroup("sidebar", "action_btn");
                    // Close color edit panel if open
                    if (sColorEditOpen) {
                        removeUIGroup("color_edit");
                        removeUIGroup("color_mode_dropdown");
                        sColorEditOpen = false;
                    }
                    if (sEditorMode == 0) {
                        addToGroup("sidebar", createButton("action_btn",
                            btnX, actionBtnY, btnW, btnH,
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
                    } else {
                        addToGroup("sidebar", createButton("action_btn",
                            btnX, actionBtnY, btnW, btnH,
                            {0.3f, 0.25f, 0.25f, 0.95f}, "Edit Color",
                            [btnX, btnW, btnH, actionBtnY]() {
                                sColorEditOpen = !sColorEditOpen;
                                auto closeColorEdit = []() {
                                    sColorEditOpen = false;
                                    removeUIGroup("color_edit");
                                    removeUIGroup("color_mode_dropdown");
                                    UIElement* editorMode = getUIElement("sidebar", "editor_mode");
                                    if (editorMode) editorMode->visible = true;
                                };

                                if (sColorEditOpen) {
                                    // Hide editor mode dropdown
                                    UIElement* editorMode = getUIElement("sidebar", "editor_mode");
                                    if (editorMode) editorMode->visible = false;

                                    float panelPad = 0.02f;
                                    float inputsY = actionBtnY - btnH - panelPad;

                                    addUIGroup("color_edit");

                                    // Color mode dropdown
                                    std::string modeLabel = sColorMode == 0 ? "RGB" : sColorMode == 1 ? "Hex" : "Color Wheel";
                                    createDropdown("color_edit", "color_mode",
                                        btnX, inputsY, btnW, btnH,
                                        {0.18f, 0.18f, 0.18f, 0.95f},
                                        modeLabel,
                                        {"RGB", "Hex"},
                                        [btnX, btnW, btnH, actionBtnY](int index, const std::string& option) {
                                            sColorMode = index;
                                            if (sRebuildColorInputs) sRebuildColorInputs();
                                        },
                                        0.0f, -(btnH + panelPad)
                                    );
                                    inputsY -= btnH + panelPad;

                                    // Store inputsY for rebuild
                                    float fieldsY = inputsY;

                                    auto buildInputs = [btnX, btnW, btnH, fieldsY, panelPad, closeColorEdit]() {
                                        // Remove old inputs
                                        removeFromGroup("color_edit", "color_r");
                                        removeFromGroup("color_edit", "color_g");
                                        removeFromGroup("color_edit", "color_b");
                                        removeFromGroup("color_edit", "color_br");
                                        removeFromGroup("color_edit", "color_hex");
                                        removeFromGroup("color_edit", "color_hex_br");
                                        removeFromGroup("color_edit", "color_done");

                                        float y = fieldsY;
                                        glm::vec4 inputColor = {0.2f, 0.2f, 0.2f, 0.95f};

                                        glm::vec3 cur = (sSelectedColor >= 0 && sSelectedColor < 8)
                                            ? sColorWheel[sSelectedColor] : glm::vec3(0.6f);
                                        bool isDefault = (cur == glm::vec3(0.6f));
                                        float maxC = std::max({cur.r, cur.g, cur.b, 0.001f});

                                        if (sColorMode == 0) {
                                            // RGB mode
                                            float inputPad = 0.005f;
                                            float inputW = (btnW - inputPad * 3) / 4.0f;

                                            int ri = isDefault ? 0 : (int)std::round(cur.r / maxC * 255.0f);
                                            int gi = isDefault ? 0 : (int)std::round(cur.g / maxC * 255.0f);
                                            int bi = isDefault ? 0 : (int)std::round(cur.b / maxC * 255.0f);
                                            int bri = isDefault ? 0 : (int)std::round(maxC * 255.0f);

                                            auto mkInput = [&](const std::string& id, float x, const std::string& ph, int val) {
                                                UIElement e = createTextInput(id, x, y, inputW, btnH, inputColor, ph, 3);
                                                if (!isDefault) e.inputText = std::to_string(val);
                                                return e;
                                            };
                                            addToGroup("color_edit", mkInput("color_r", btnX, "R", ri));
                                            addToGroup("color_edit", mkInput("color_g", btnX + inputW + inputPad, "G", gi));
                                            addToGroup("color_edit", mkInput("color_b", btnX + (inputW + inputPad) * 2, "B", bi));
                                            addToGroup("color_edit", mkInput("color_br", btnX + (inputW + inputPad) * 3, "Bri", bri));
                                        } else if (sColorMode == 1) {
                                            // Hex mode
                                            float inputPad = 0.005f;
                                            float hexW = btnW * 0.7f;
                                            float briW = btnW - hexW - inputPad;

                                            // Convert to hex string
                                            int ri = isDefault ? 0 : (int)std::round(cur.r / maxC * 255.0f);
                                            int gi = isDefault ? 0 : (int)std::round(cur.g / maxC * 255.0f);
                                            int bi = isDefault ? 0 : (int)std::round(cur.b / maxC * 255.0f);
                                            int bri = isDefault ? 0 : (int)std::round(maxC * 255.0f);

                                            char hexBuf[8];
                                            snprintf(hexBuf, sizeof(hexBuf), "%02X%02X%02X", ri, gi, bi);

                                            UIElement hexInput = createTextInput("color_hex",
                                                btnX, y, hexW, btnH, inputColor, "Hex", 6);
                                            if (!isDefault) hexInput.inputText = hexBuf;
                                            addToGroup("color_edit", hexInput);

                                            UIElement briInput = createTextInput("color_hex_br",
                                                btnX + hexW + inputPad, y, briW, btnH, inputColor, "Bri", 3);
                                            if (!isDefault) briInput.inputText = std::to_string(bri);
                                            addToGroup("color_edit", briInput);
                                        }
                                        y -= btnH + panelPad;

                                        // Done button
                                        addToGroup("color_edit", createButton("color_done",
                                            btnX, y, btnW, btnH,
                                            {0.15f, 0.5f, 0.2f, 0.95f}, "Done",
                                            [closeColorEdit]() {
                                                float r = 0, g = 0, b = 0, bri = 1.0f;

                                                if (sColorMode == 0) {
                                                    std::string rS = getInputText("color_edit", "color_r");
                                                    std::string gS = getInputText("color_edit", "color_g");
                                                    std::string bS = getInputText("color_edit", "color_b");
                                                    std::string brS = getInputText("color_edit", "color_br");
                                                    r = rS.empty() ? 0.0f : std::clamp(std::stof(rS) / 255.0f, 0.0f, 1.0f);
                                                    g = gS.empty() ? 0.0f : std::clamp(std::stof(gS) / 255.0f, 0.0f, 1.0f);
                                                    b = bS.empty() ? 0.0f : std::clamp(std::stof(bS) / 255.0f, 0.0f, 1.0f);
                                                    bri = brS.empty() ? 1.0f : std::clamp(std::stof(brS) / 255.0f, 0.0f, 1.0f);
                                                } else if (sColorMode == 1) {
                                                    std::string hex = getInputText("color_edit", "color_hex");
                                                    std::string brS = getInputText("color_edit", "color_hex_br");
                                                    if (hex.size() == 6) {
                                                        unsigned int val = std::stoul(hex, nullptr, 16);
                                                        r = ((val >> 16) & 0xFF) / 255.0f;
                                                        g = ((val >> 8) & 0xFF) / 255.0f;
                                                        b = (val & 0xFF) / 255.0f;
                                                    }
                                                    bri = brS.empty() ? 1.0f : std::clamp(std::stof(brS) / 255.0f, 0.0f, 1.0f);
                                                }

                                                if (sSelectedColor >= 0 && sSelectedColor < 8)
                                                    sColorWheel[sSelectedColor] = glm::vec3(r, g, b) * bri;

                                                setPaintPalette(sColorWheel);
                                                ctx.needsRebuild = true;
                                                closeColorEdit();
                                            }
                                        ));
                                    };

                                    sRebuildColorInputs = buildInputs;
                                    buildInputs();
                                } else {
                                    closeColorEdit();
                                }
                            }
                        ));
                    }
                };
                rebuildActionButton();
                sRebuildActionButton = rebuildActionButton;
            }
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
                    rebuildSelectorIcons();
                    if (sRebuildActionButton) sRebuildActionButton();
                    if (sEditorMode == 1 && sSelectedColor < 0)
                        sSelectedColor = 0;
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
                    // Restore palette
                    for (int i = 0; i < 8; i++)
                        sColorWheel[i] = sCurrentModel.palette[i];
                    setPaintPalette(sColorWheel);

                    for (int i = 0; i < (int)sCurrentModel.blockTypes.size(); i++) {
                        const BlockTypeDef& bt = sCurrentModel.blockTypes[i];
                        if (bt.floatsPerVertex == 8) {
                            // VN mesh: load directly from .mesh file
                            std::string meshPath = "assets/saves/vectorMeshes/" + bt.name + ".mesh";
                            VE::loadMesh(bt.name.c_str(), meshPath.c_str());
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
                        if (p.typeId < (int)sCurrentModel.blockTypes.size()) {
                            VE::draw(sCurrentModel.blockTypes[p.typeId].name.c_str(),
                                     (float)p.x, (float)p.y, (float)p.z,
                                     (float)p.rx, (float)p.ry, (float)p.rz);
                            // Restore paint colors
                            if (!p.triColors.empty()) {
                                BlockCollider* col = const_cast<BlockCollider*>(
                                    getColliderAt(p.x, p.y, p.z));
                                if (col) col->triColors = p.triColors;
                            }
                        }
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

            // Block camera when paused
            if (sPaused) {
                Camera* cam = getGlobalCamera();
                if (cam->looking) {
                    cam->looking = false;
                    glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
            }

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

            if (sPaused) {
                processUIInput();
                return;
            }

            // Scroll: build mode = cycle slots, paint mode = cycle colors
            if (ctx.scrollDelta != 0.0f && sEditorMode == 1) {
                int dir = (ctx.scrollDelta > 0.0f) ? 1 : -1;
                sSelectedColor = (sSelectedColor + dir + 8) % 8;
            }
            if (ctx.scrollDelta != 0.0f && sEditorMode == 0) {
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

            if (!sPaused)
                renderHoverHighlight();

            renderUI();

            if (sEditorMode == 1 && !sPaused)
                drawColorWheel(getUIShader());
        }
    );
}
