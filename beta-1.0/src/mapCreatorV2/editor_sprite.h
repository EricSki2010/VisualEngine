#pragma once
#include "engine/engine.h"
#include "rlgl.h"
#include "gradient_sky.h"
#include "editor_camera.h"
#include "placed_blocks.h"
#include "menu_ui.h"
#include "save_system.h"
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>

// ── Imported model instance ──────────────────────────────────────────

struct SpriteModel {
    std::string name;       // user-given name
    std::string glbFile;    // filename in AbstractModels/ (e.g. "tree.glb")
    float x = 0, y = 0, z = 0;
    float rotX = 0, rotY = 0, rotZ = 0; // degrees around each axis
    float sizeY = 1.0f;     // height scale (1 = original)
    int groupId = -1;       // -1 = no group, >=0 = index into groups[]
    Model model = {};
    bool loaded = false;
    float baseScaleX = 1, baseScaleY = 1, baseScaleZ = 1;
    float offsetY = 0;
    float boundsW = 1, boundsH = 1, boundsD = 1; // scaled bounding box dimensions

    void load() {
        std::string path = "AbstractModels/" + glbFile;
        if (!FileExists(path.c_str())) return;
        model = LoadModel(path.c_str());
        loaded = true;
        BoundingBox bb = GetModelBoundingBox(model);
        float rawH = bb.max.y - bb.min.y;
        float rawW = bb.max.x - bb.min.x;
        float rawD = bb.max.z - bb.min.z;
        // Base scale fits model in 1x1x1
        float maxExt = rawH;
        if (rawW > maxExt) maxExt = rawW;
        if (rawD > maxExt) maxExt = rawD;
        if (maxExt < 0.001f) maxExt = 1.0f;
        baseScaleX = 1.0f / maxExt;
        baseScaleY = 1.0f / maxExt;
        baseScaleZ = 1.0f / maxExt;
        float scaledMinY = bb.min.y * baseScaleY;
        offsetY = -scaledMinY;
        boundsW = rawW * baseScaleX;
        boundsH = rawH * baseScaleY;
        boundsD = rawD * baseScaleZ;
    }

    void unload() {
        if (loaded) { UnloadModel(model); loaded = false; }
    }

    void draw(float alpha = 255) const {
        drawAt(x, y, z, alpha);
    }

    // Draw at a specific world position (used for group-transformed drawing)
    void drawAt(float wx, float wy, float wz, float alpha = 255) const {
        if (!loaded) return;
        float s = sizeY;
        rlDisableBackfaceCulling();
        rlPushMatrix();
            rlTranslatef(wx, wy + offsetY * s, wz);
            rlRotatef(rotY, 0, 1, 0);
            rlRotatef(rotX, 1, 0, 0);
            rlRotatef(rotZ, 0, 0, 1);
            rlScalef(baseScaleX * s, baseScaleY * s, baseScaleZ * s);
            DrawModel(model, { 0, 0, 0 }, 1.0f, { 255, 255, 255, (unsigned char)alpha });
        rlPopMatrix();
        rlEnableBackfaceCulling();
    }
};

// ── Model group (parent) ─────────────────────────────────────────────

struct SpriteGroup {
    std::string name;
    float x = 0, y = 0, z = 0;                   // group position offset (moves everything)
    float originX = 0, originY = 0, originZ = 0; // rotation pivot point
    float rotX = 0, rotY = 0, rotZ = 0;          // group rotation (degrees)
    std::vector<int> childIndices;                 // indices into models[]

    // Rotate a point around the group origin, then apply group position offset
    void transformPoint(float inX, float inY, float inZ,
                        float& outX, float& outY, float& outZ) const {
        float dx = inX - originX, dy = inY - originY, dz = inZ - originZ;
        // Apply Y rotation
        float radY = rotY * DEG2RAD;
        float cosY = cosf(radY), sinY = sinf(radY);
        float tx = dx * cosY + dz * sinY;
        float tz = -dx * sinY + dz * cosY;
        dx = tx; dz = tz;
        // Apply X rotation
        float radX = rotX * DEG2RAD;
        float cosX = cosf(radX), sinX = sinf(radX);
        float ty = dy * cosX - dz * sinX;
        tz = dy * sinX + dz * cosX;
        dy = ty; dz = tz;
        // Apply Z rotation
        float radZ = rotZ * DEG2RAD;
        float cosZ = cosf(radZ), sinZ = sinf(radZ);
        tx = dx * cosZ - dy * sinZ;
        ty = dx * sinZ + dy * cosZ;
        dx = tx; dy = ty;

        outX = originX + dx + x;
        outY = originY + dy + y;
        outZ = originZ + dz + z;
    }
};

// ── Sprite Editor ────────────────────────────────────────────────────

struct SpriteEditor {
    bool escMenuOpen = false;
    int gridSize = 10;

    // Scene origin
    Vector3 sceneOrigin = {0, 0, 0};
    bool originSet = false;
    bool originPickRequested = false;

    // Imported models
    std::vector<SpriteModel> models;
    int selectedModel = -1;
    std::set<int> multiSelect; // shift-click multi-selection

    // Groups (parents)
    std::vector<SpriteGroup> groups;
    int selectedGroup = -1;    // -1 = editing a model, >=0 = editing a group

    // Group naming prompt
    bool groupNamingOpen = false;
    std::string groupName;

    // Load model dialog
    bool loadDialogOpen = false;
    bool loadPreview = false;
    std::vector<std::string> availableGlbs;
    int selectedGlb = -1;
    int glbScrollOffset = 0;
    std::string importName;
    std::string importX = "0", importY = "0", importZ = "0";
    std::string importRotX = "0", importRotY = "0", importRotZ = "0";
    std::string importSizeY = "1";
    int loadFocusField = 0; // 0=name, 1=X, 2=Y, 3=Z, 4=rotX, 5=rotY, 6=rotZ, 7=sizeY
    SpriteModel previewModel;

    // Right panel editing (shared for model and group)
    std::string editX, editY, editZ, editRotX, editRotY, editRotZ, editSizeY;
    std::string editGX, editGY, editGZ; // group position
    int editFocusField = -1;
    bool editingRight = false;
    bool editingGroup = false; // true = editing group, false = editing model

    // Gizmo drag state
    int gizmoDragAxis = -1;   // -1=none, 0=X, 1=Y, 2=Z
    Vector2 gizmoDragStart = {0, 0};
    float gizmoDragOrigVal = 0;
    static constexpr float GIZMO_LEN = 1.5f;
    static constexpr float GIZMO_THICKNESS = 0.06f;
    static constexpr float GIZMO_TIP = 0.2f;

    static const int PANEL_W = 260;

    void reset() {
        escMenuOpen = false;
        loadDialogOpen = false;
        loadPreview = false;
        previewModel.unload();
        selectedModel = -1;
        multiSelect.clear();
        selectedGroup = -1;
        groupNamingOpen = false;
        editingRight = false;
        editFocusField = -1;
        gizmoDragAxis = -1;
    }

    void unloadAll() {
        for (auto& m : models) m.unload();
        models.clear();
        groups.clear();
        previewModel.unload();
    }

    void refreshGlbList() {
        namespace fs = std::filesystem;
        availableGlbs.clear();
        std::string dir = "AbstractModels";
        if (!fs::exists(dir)) { fs::create_directories(dir); return; }
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".glb")
                availableGlbs.push_back(entry.path().filename().string());
        }
        std::sort(availableGlbs.begin(), availableGlbs.end());
    }

    // ── Input ────────────────────────────────────────────────

    void handleInput() {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (groupNamingOpen) { groupNamingOpen = false; }
            else if (loadPreview) { loadPreview = false; previewModel.unload(); }
            else if (loadDialogOpen) { loadDialogOpen = false; }
            else if (editingRight) { editingRight = false; editFocusField = -1; }
            else { escMenuOpen = !escMenuOpen; }
        }

        // O to set scene origin by raycasting
        if (IsKeyPressed(KEY_O) && !escMenuOpen && !loadDialogOpen && !groupNamingOpen && !editingRight) {
            // handled in updateLogic after camera update
            originPickRequested = true;
        }

        // Q to create a group from multi-selection
        if (IsKeyPressed(KEY_Q) && !escMenuOpen && !loadDialogOpen && !groupNamingOpen) {
            if ((int)multiSelect.size() >= 2) {
                groupNamingOpen = true;
                groupName.clear();
            }
        }
    }

    void handleGroupNamingInput() {
        if (!groupNamingOpen) return;
        int ch = GetCharPressed();
        while (ch > 0) {
            if (ch >= 32 && ch <= 125 && (int)groupName.size() < 32)
                groupName += (char)ch;
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !groupName.empty()) groupName.pop_back();

        if (IsKeyPressed(KEY_ENTER) && !groupName.empty()) {
            // Check name not taken
            bool taken = false;
            for (auto& g : groups) { if (g.name == groupName) { taken = true; break; } }
            if (!taken) {
                createGroupFromSelection();
                groupNamingOpen = false;
            }
        }
    }

    void createGroupFromSelection() {
        if (multiSelect.empty()) return;
        SpriteGroup g;
        g.name = groupName;

        // Origin = first selected model's position
        int firstIdx = *multiSelect.begin();
        if (firstIdx >= 0 && firstIdx < (int)models.size()) {
            g.originX = models[firstIdx].x;
            g.originY = models[firstIdx].y;
            g.originZ = models[firstIdx].z;
        }

        int gIdx = (int)groups.size();
        for (int idx : multiSelect) {
            if (idx >= 0 && idx < (int)models.size()) {
                // Remove from any existing group first
                if (models[idx].groupId >= 0) {
                    auto& oldG = groups[models[idx].groupId];
                    auto& ci = oldG.childIndices;
                    ci.erase(std::remove(ci.begin(), ci.end(), idx), ci.end());
                }
                models[idx].groupId = gIdx;
                g.childIndices.push_back(idx);
            }
        }
        groups.push_back(std::move(g));
        multiSelect.clear();
        selectedGroup = gIdx;
        selectedModel = -1;
    }

    void handleLoadDialogInput() {
        if (!loadDialogOpen || loadPreview) return;
        if (IsKeyPressed(KEY_TAB)) {
            loadFocusField = (loadFocusField + 1) % 8;
        }

        std::string* target = nullptr;
        bool numOnly = false;
        bool allowDot = false;
        bool allowNeg = false;
        switch (loadFocusField) {
            case 0: target = &importName; break;
            case 1: target = &importX; numOnly = true; allowNeg = true; allowDot = true; break;
            case 2: target = &importY; numOnly = true; allowNeg = true; allowDot = true; break;
            case 3: target = &importZ; numOnly = true; allowNeg = true; allowDot = true; break;
            case 4: target = &importRotX; numOnly = true; allowNeg = true; allowDot = true; break;
            case 5: target = &importRotY; numOnly = true; allowNeg = true; allowDot = true; break;
            case 6: target = &importRotZ; numOnly = true; allowNeg = true; allowDot = true; break;
            case 7: target = &importSizeY; numOnly = true; allowDot = true; break;
        }

        if (target) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (numOnly) {
                    bool ok = (ch >= '0' && ch <= '9');
                    if (allowDot && ch == '.' && target->find('.') == std::string::npos) ok = true;
                    if (allowNeg && ch == '-' && target->empty()) ok = true;
                    if (ok && (int)target->size() < 12) *target += (char)ch;
                } else {
                    if (ch >= 32 && ch <= 125 && (int)target->size() < 32)
                        *target += (char)ch;
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !target->empty()) target->pop_back();
        }
    }

    void handleRightPanelInput() {
        if (!editingRight || editFocusField < 0 || loadDialogOpen || escMenuOpen || groupNamingOpen) return;

        int maxFields = editingGroup ? 9 : 7;
        if (IsKeyPressed(KEY_TAB)) {
            editFocusField = (editFocusField + 1) % maxFields;
        }

        std::string* target = nullptr;
        bool allowDot = true, allowNeg = true;
        if (editingGroup) {
            // Group: 0-2=pos, 3-5=origin, 6-8=rot
            switch (editFocusField) {
                case 0: target = &editGX; break;
                case 1: target = &editGY; break;
                case 2: target = &editGZ; break;
                case 3: target = &editX; break;
                case 4: target = &editY; break;
                case 5: target = &editZ; break;
                case 6: target = &editRotX; break;
                case 7: target = &editRotY; break;
                case 8: target = &editRotZ; break;
            }
        } else {
            switch (editFocusField) {
                case 0: target = &editX; break;
                case 1: target = &editY; break;
                case 2: target = &editZ; break;
                case 3: target = &editRotX; break;
                case 4: target = &editRotY; break;
                case 5: target = &editRotZ; break;
                case 6: target = &editSizeY; allowNeg = false; break;
            }
        }

        if (target) {
            int ch = GetCharPressed();
            while (ch > 0) {
                bool ok = (ch >= '0' && ch <= '9');
                if (allowDot && ch == '.' && target->find('.') == std::string::npos) ok = true;
                if (allowNeg && ch == '-' && target->empty()) ok = true;
                if (ok && (int)target->size() < 12) *target += (char)ch;
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !target->empty()) target->pop_back();
        }

        // Apply changes in real-time
        if (editingGroup && selectedGroup >= 0 && selectedGroup < (int)groups.size()) {
            auto& g = groups[selectedGroup];
            if (!editGX.empty() && editGX != "-") g.x = std::strtof(editGX.c_str(), nullptr);
            if (!editGY.empty() && editGY != "-") g.y = std::strtof(editGY.c_str(), nullptr);
            if (!editGZ.empty() && editGZ != "-") g.z = std::strtof(editGZ.c_str(), nullptr);
            if (!editX.empty() && editX != "-") g.originX = std::strtof(editX.c_str(), nullptr);
            if (!editY.empty() && editY != "-") g.originY = std::strtof(editY.c_str(), nullptr);
            if (!editZ.empty() && editZ != "-") g.originZ = std::strtof(editZ.c_str(), nullptr);
            if (!editRotX.empty() && editRotX != "-") g.rotX = std::strtof(editRotX.c_str(), nullptr);
            if (!editRotY.empty() && editRotY != "-") g.rotY = std::strtof(editRotY.c_str(), nullptr);
            if (!editRotZ.empty() && editRotZ != "-") g.rotZ = std::strtof(editRotZ.c_str(), nullptr);
        } else if (!editingGroup && selectedModel >= 0 && selectedModel < (int)models.size()) {
            auto& m = models[selectedModel];
            if (!editX.empty() && editX != "-") m.x = std::strtof(editX.c_str(), nullptr);
            if (!editY.empty() && editY != "-") m.y = std::strtof(editY.c_str(), nullptr);
            if (!editZ.empty() && editZ != "-") m.z = std::strtof(editZ.c_str(), nullptr);
            if (!editRotX.empty() && editRotX != "-") m.rotX = std::strtof(editRotX.c_str(), nullptr);
            if (!editRotY.empty() && editRotY != "-") m.rotY = std::strtof(editRotY.c_str(), nullptr);
            if (!editRotZ.empty() && editRotZ != "-") m.rotZ = std::strtof(editRotZ.c_str(), nullptr);
            if (!editSizeY.empty()) { float v = std::strtof(editSizeY.c_str(), nullptr); if (v > 0) m.sizeY = v; }
        }
    }

    // ── Update ───────────────────────────────────────────────

    void updateLogic(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
                     std::string& savePath, std::string& structureName,
                     std::string& structureType, float& autoSaveTimer) {
        autoSaveTimer += dt;
        if (autoSaveTimer >= 10.0f) {
            saveSpriteScene(savePath, structureName, structureType, placedBlocks, editorCam);
            autoSaveTimer = 0.0f;
        }

        if (!escMenuOpen && !loadDialogOpen && !groupNamingOpen && gizmoDragAxis < 0) {
            editorCam.update(dt);
        }

        // Origin pick via O key
        if (originPickRequested) {
            originPickRequested = false;
            Ray ray = GetScreenToWorldRay(GetMousePosition(), editorCam.cam);
            float bestDist = 1e9f;
            Vector3 bestHit = {0, 0, 0};
            bool hit = false;

            // Raycast against model bounding boxes
            for (int i = 0; i < (int)models.size(); i++) {
                auto& sm = models[i];
                if (!sm.loaded) continue;
                float ex, ey, ez;
                getEffectivePos(sm, ex, ey, ez);
                float sy = sm.sizeY;
                BoundingBox bb = GetModelBoundingBox(sm.model);
                BoundingBox worldBB = {
                    { ex + bb.min.x * sm.baseScaleX * sy, ey + sm.offsetY * sy + bb.min.y * sm.baseScaleY * sy, ez + bb.min.z * sm.baseScaleZ * sy },
                    { ex + bb.max.x * sm.baseScaleX * sy, ey + sm.offsetY * sy + bb.max.y * sm.baseScaleY * sy, ez + bb.max.z * sm.baseScaleZ * sy }
                };
                RayCollision rc = GetRayCollisionBox(ray, worldBB);
                if (rc.hit && rc.distance < bestDist) {
                    bestDist = rc.distance;
                    bestHit = rc.point;
                    hit = true;
                }
            }

            // Raycast against the ground plane (Y = 0.05, top of grid tiles)
            if (ray.direction.y != 0.0f) {
                float t = (0.05f - ray.position.y) / ray.direction.y;
                if (t > 0.0f) {
                    Vector3 gp = { ray.position.x + ray.direction.x * t, 0.05f, ray.position.z + ray.direction.z * t };
                    // Check within grid bounds
                    if (gp.x >= -0.5f && gp.x < gridSize - 0.5f && gp.z >= -0.5f && gp.z < gridSize - 0.5f) {
                        if (t < bestDist) {
                            bestHit = gp;
                            hit = true;
                        }
                    }
                }
            }

            if (hit) {
                sceneOrigin = bestHit;
                originSet = true;
            }
        }
    }

    // ── 3D Scene ─────────────────────────────────────────────

    void drawScene(EditorCamera& editorCam, GradientSky& sky) {
        sky.draw(editorCam.pitch);

        BeginMode3D(editorCam.cam);
            // 10x10 grid floor
            for (int x = 0; x < gridSize; x++) {
                for (int z = 0; z < gridSize; z++) {
                    Vector3 pos = { (float)x, 0.0f, (float)z };
                    DrawCube(pos, 1.0f, 0.1f, 1.0f, { 40, 40, 50, 255 });
                    DrawCubeWires(pos, 1.0f, 0.1f, 1.0f, { 80, 80, 100, 255 });
                }
            }

            // Draw all imported models
            for (int i = 0; i < (int)models.size(); i++) {
                auto& sm = models[i];
                // Get effective position (group-transformed)
                float ex, ey, ez;
                getEffectivePos(sm, ex, ey, ez);
                sm.drawAt(ex, ey, ez);

                // Highlight if selected or in multi-select
                bool isSelected = (i == selectedModel) || multiSelect.count(i);
                if (isSelected && sm.loaded) {
                    float sy = sm.sizeY;
                    BoundingBox bb = GetModelBoundingBox(sm.model);
                    float w = (bb.max.x - bb.min.x) * sm.baseScaleX * sy;
                    float h = (bb.max.y - bb.min.y) * sm.baseScaleY * sy;
                    float d = (bb.max.z - bb.min.z) * sm.baseScaleZ * sy;
                    float lcx = (bb.min.x + bb.max.x) * 0.5f * sm.baseScaleX * sy;
                    float lcy = (bb.min.y + bb.max.y) * 0.5f * sm.baseScaleY * sy;
                    float lcz = (bb.min.z + bb.max.z) * 0.5f * sm.baseScaleZ * sy;

                    rlPushMatrix();
                        rlTranslatef(ex, ey + sm.offsetY * sy, ez);
                        rlRotatef(sm.rotY, 0, 1, 0);
                        rlRotatef(sm.rotX, 1, 0, 0);
                        rlRotatef(sm.rotZ, 0, 0, 1);
                        Color wireCol = multiSelect.count(i) ? ORANGE : YELLOW;
                        DrawCubeWires({ lcx, lcy, lcz }, w, h, d, wireCol);
                    rlPopMatrix();

                    if (i == selectedModel) {
                        drawGizmo(sm);
                    }
                }
            }

            // Draw group origin markers
            for (int gi = 0; gi < (int)groups.size(); gi++) {
                auto& g = groups[gi];
                Color c = (gi == selectedGroup) ? MAGENTA : Color{180,80,180,200};
                DrawSphere({g.originX + g.x, g.originY + g.y, g.originZ + g.z}, 0.15f, c);
            }

            // Scene origin marker
            if (originSet) {
                DrawSphere(sceneOrigin, 0.12f, RED);
                // Draw small cross at origin
                float cl = 0.3f;
                DrawLine3D({sceneOrigin.x - cl, sceneOrigin.y, sceneOrigin.z},
                           {sceneOrigin.x + cl, sceneOrigin.y, sceneOrigin.z}, RED);
                DrawLine3D({sceneOrigin.x, sceneOrigin.y - cl, sceneOrigin.z},
                           {sceneOrigin.x, sceneOrigin.y + cl, sceneOrigin.z}, RED);
                DrawLine3D({sceneOrigin.x, sceneOrigin.y, sceneOrigin.z - cl},
                           {sceneOrigin.x, sceneOrigin.y, sceneOrigin.z + cl}, RED);
            }

            // Preview model
            if (loadPreview && previewModel.loaded) {
                previewModel.draw(140);
            }
        EndMode3D();

        // Coordinates
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "X: %.1f  Y: %.1f  Z: %.1f",
                     editorCam.origin.x, editorCam.origin.y, editorCam.origin.z);
            DrawText(buf, PANEL_W + 10, 10, 20, GREEN);
        }
        DrawText("3D Sprite", PANEL_W + 10, 34, 16, LIGHTGRAY);
        DrawText("O: Set Origin", PANEL_W + 10, 52, 14, GRAY);
    }

    // ── Group helpers ─────────────────────────────────────────

    // Get the effective world position of a model, accounting for its group transform
    void getEffectivePos(const SpriteModel& sm, float& ox, float& oy, float& oz) const {
        if (sm.groupId >= 0 && sm.groupId < (int)groups.size()) {
            groups[sm.groupId].transformPoint(sm.x, sm.y, sm.z, ox, oy, oz);
        } else {
            ox = sm.x; oy = sm.y; oz = sm.z;
        }
    }

    // ── Gizmo ────────────────────────────────────────────────

    Vector3 gizmoCenter(const SpriteModel& sm) const {
        float ex, ey, ez;
        getEffectivePos(sm, ex, ey, ez);
        return { ex, ey + sm.offsetY * sm.sizeY, ez };
    }

    void drawGizmo(const SpriteModel& sm) const {
        Vector3 origin = gizmoCenter(sm);
        float len = GIZMO_LEN * sm.sizeY;
        if (len < 0.5f) len = 0.5f;
        if (len > 3.0f) len = 3.0f;
        float thick = GIZMO_THICKNESS * len;
        float tip = GIZMO_TIP * len;

        // X axis (red)
        Color xCol = (gizmoDragAxis == 0) ? Color{255,100,100,255} : RED;
        drawArrowShaft(origin, {len, 0, 0}, thick, xCol);
        drawArrowTip(origin, {len, 0, 0}, tip, thick * 2.5f, xCol);

        // Y axis (green)
        Color yCol = (gizmoDragAxis == 1) ? Color{100,255,100,255} : GREEN;
        drawArrowShaft(origin, {0, len, 0}, thick, yCol);
        drawArrowTip(origin, {0, len, 0}, tip, thick * 2.5f, yCol);

        // Z axis (blue)
        Color zCol = (gizmoDragAxis == 2) ? Color{100,100,255,255} : BLUE;
        drawArrowShaft(origin, {0, 0, len}, thick, zCol);
        drawArrowTip(origin, {0, 0, len}, tip, thick * 2.5f, zCol);
    }

    void drawArrowShaft(Vector3 origin, Vector3 dir, float thick, Color col) const {
        // Draw a line-based shaft from origin to origin+dir
        Vector3 end = {origin.x + dir.x, origin.y + dir.y, origin.z + dir.z};
        DrawCylinderEx(origin, end, thick, thick, 6, col);
    }

    void drawArrowTip(Vector3 origin, Vector3 dir, float tipLen, float tipRadius, Color col) const {
        // Cone at the end of the shaft
        Vector3 shaftEnd = {origin.x + dir.x, origin.y + dir.y, origin.z + dir.z};
        float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len < 0.001f) return;
        Vector3 norm = {dir.x/len, dir.y/len, dir.z/len};
        Vector3 tipEnd = {shaftEnd.x + norm.x * tipLen, shaftEnd.y + norm.y * tipLen, shaftEnd.z + norm.z * tipLen};
        DrawCylinderEx(shaftEnd, tipEnd, tipRadius, 0.0f, 8, col);
    }

    // Returns which axis arrow the mouse is hovering (0=X, 1=Y, 2=Z, -1=none)
    int hitTestGizmo(const SpriteModel& sm, Camera3D cam) const {
        Vector2 mouse = GetMousePosition();
        Vector3 origin = gizmoCenter(sm);
        float len = GIZMO_LEN * sm.sizeY;
        if (len < 0.5f) len = 0.5f;
        if (len > 3.0f) len = 3.0f;
        float hitRadius = len * 0.15f; // generous click area

        Vector3 axes[3] = {{len,0,0}, {0,len,0}, {0,0,len}};

        int bestAxis = -1;
        float bestDist = 1e9f;

        for (int a = 0; a < 3; a++) {
            // Sample points along the arrow and check screen distance
            for (int s = 1; s <= 10; s++) {
                float t = (float)s / 10.0f;
                Vector3 p = {
                    origin.x + axes[a].x * t,
                    origin.y + axes[a].y * t,
                    origin.z + axes[a].z * t
                };
                Vector2 sp = GetWorldToScreen(p, cam);
                float dx = sp.x - mouse.x;
                float dy = sp.y - mouse.y;
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist < 40.0f && dist < bestDist) { // 40 pixel threshold
                    bestDist = dist;
                    bestAxis = a;
                }
            }
        }
        return bestAxis;
    }

    void handleGizmoInput(EditorCamera& editorCam) {
        if (selectedModel < 0 || selectedModel >= (int)models.size()) {
            gizmoDragAxis = -1;
            return;
        }
        if (escMenuOpen || loadDialogOpen || groupNamingOpen) return;

        auto& sm = models[selectedModel];

        // Start drag
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && gizmoDragAxis < 0) {
            Vector2 mouse = GetMousePosition();
            // Don't start gizmo if clicking on panels
            if (mouse.x < PANEL_W || mouse.x > GetScreenWidth() - PANEL_W) return;

            int axis = hitTestGizmo(sm, editorCam.cam);
            if (axis >= 0) {
                gizmoDragAxis = axis;
                gizmoDragStart = mouse;
                if (axis == 0) gizmoDragOrigVal = sm.x;
                else if (axis == 1) gizmoDragOrigVal = sm.y;
                else gizmoDragOrigVal = sm.z;
            }
        }

        // Continue drag
        if (gizmoDragAxis >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();

            // Project the axis direction to screen space to know which screen direction to track
            Vector3 origin = { sm.x, sm.y, sm.z };
            Vector3 axisDir = {0,0,0};
            if (gizmoDragAxis == 0) axisDir.x = 1;
            else if (gizmoDragAxis == 1) axisDir.y = 1;
            else axisDir.z = 1;

            Vector2 screenOrigin = GetWorldToScreen(origin, editorCam.cam);
            Vector2 screenAxis = GetWorldToScreen(
                {origin.x + axisDir.x, origin.y + axisDir.y, origin.z + axisDir.z},
                editorCam.cam);

            float sdx = screenAxis.x - screenOrigin.x;
            float sdy = screenAxis.y - screenOrigin.y;
            float sLen = sqrtf(sdx*sdx + sdy*sdy);
            if (sLen > 0.1f) {
                sdx /= sLen; sdy /= sLen;
                float mouseDx = mouse.x - gizmoDragStart.x;
                float mouseDy = mouse.y - gizmoDragStart.y;
                // Project mouse movement onto screen-space axis direction
                float proj = mouseDx * sdx + mouseDy * sdy;
                // Scale: pixels to world units (approximate based on distance)
                float scale = editorCam.distance / (float)GetScreenWidth() * 2.0f;
                float newVal = gizmoDragOrigVal + proj * scale;

                if (gizmoDragAxis == 0) sm.x = newVal;
                else if (gizmoDragAxis == 1) sm.y = newVal;
                else sm.z = newVal;

                // Update right panel fields if editing
                if (editingRight) startEditingSelected();
            }
        }

        // End drag
        if (gizmoDragAxis >= 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            gizmoDragAxis = -1;
        }
    }

    // ── Left Panel (model list) ──────────────────────────────

    void drawLeftPanel() {
        int sH = GetScreenHeight();
        DrawRectangle(0, 0, PANEL_W, sH, { 18, 18, 24, 240 });
        DrawLine(PANEL_W, 0, PANEL_W, sH, { 60, 60, 80, 200 });

        DrawText("Models", 10, 10, 22, WHITE);
        DrawLine(10, 36, PANEL_W - 10, 36, { 60, 60, 80, 200 });

        int itemH = 44, startY = 44;
        bool blocked = loadDialogOpen || escMenuOpen || groupNamingOpen;
        int curY = startY;

        // Draw groups first
        for (int gi = 0; gi < (int)groups.size(); gi++) {
            auto& g = groups[gi];
            int y = curY;
            Vector2 mouse = GetMousePosition();
            bool hov = !blocked && mouse.x >= 0 && mouse.x < PANEL_W - 36 && mouse.y >= y && mouse.y < y + itemH;

            Color bg = (gi == selectedGroup) ? Color{80,50,90,230}
                     : hov ? Color{55,40,60,230} : Color{35,25,40,230};
            DrawRectangle(4, y, PANEL_W - 44, itemH, bg);
            DrawRectangleLines(4, y, PANEL_W - 44, itemH,
                (gi == selectedGroup) ? MAGENTA : Color{100,60,100,200});

            DrawText(g.name.c_str(), 12, y + 4, 18, MAGENTA);
            char info[64];
            snprintf(info, sizeof(info), "Group (%d models)", (int)g.childIndices.size());
            DrawText(info, 12, y + 24, 12, LIGHTGRAY);

            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selectedGroup = gi;
                selectedModel = -1;
                multiSelect.clear();
                startEditingGroup();
            }

            // Delete group button (ungroups children, doesn't delete models)
            int delX = PANEL_W - 36, delW = 28;
            bool delHov = !blocked && mouse.x >= delX && mouse.x < delX + delW && mouse.y >= y && mouse.y < y + itemH;
            DrawRectangle(delX, y, delW, itemH, delHov ? Color{180,50,50,230} : Color{60,30,30,230});
            DrawRectangleLines(delX, y, delW, itemH, delHov ? RED : Color{100,50,50,200});
            int xw = MeasureText("X", 18);
            DrawText("X", delX + (delW - xw) / 2, y + (itemH - 18) / 2, 18, WHITE);

            if (delHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Ungroup: clear children's groupId, fix indices in other groups
                for (int ci : g.childIndices) {
                    if (ci >= 0 && ci < (int)models.size()) models[ci].groupId = -1;
                }
                groups.erase(groups.begin() + gi);
                // Fix groupId references
                for (auto& m : models) {
                    if (m.groupId > gi) m.groupId--;
                    else if (m.groupId == gi) m.groupId = -1;
                }
                if (selectedGroup == gi) { selectedGroup = -1; editingRight = false; }
                else if (selectedGroup > gi) selectedGroup--;
                break;
            }

            curY += itemH + 4;
        }

        // Separator if there are groups
        if (!groups.empty() && !models.empty()) {
            DrawLine(10, curY, PANEL_W - 10, curY, {60,60,80,200});
            curY += 4;
        }

        if (models.empty()) {
            DrawText("No models", 10, curY + 10, 16, GRAY);
            return;
        }

        for (int i = 0; i < (int)models.size(); i++) {
            int y = curY;
            if (y + itemH > sH - 10) break; // don't overflow
            Vector2 m = GetMousePosition();
            bool hov = !blocked && m.x >= 0 && m.x < PANEL_W - 36 && m.y >= y && m.y < y + itemH;

            bool inMulti = multiSelect.count(i) > 0;
            Color bg = (i == selectedModel) ? Color{60,60,90,230}
                     : inMulti ? Color{90,70,40,230}
                     : hov ? Color{45,45,60,230} : Color{30,30,40,230};
            Color border = (i == selectedModel) ? WHITE
                         : inMulti ? ORANGE : Color{80,80,80,200};
            DrawRectangle(4, y, PANEL_W - 44, itemH, bg);
            DrawRectangleLines(4, y, PANEL_W - 44, itemH, border);

            // Show group tag if in a group
            if (models[i].groupId >= 0 && models[i].groupId < (int)groups.size()) {
                const char* gn = groups[models[i].groupId].name.c_str();
                int gnw = MeasureText(gn, 10);
                DrawRectangle(PANEL_W - 48 - gnw, y + 2, gnw + 6, 14, {80,50,90,200});
                DrawText(gn, PANEL_W - 45 - gnw, y + 4, 10, MAGENTA);
            }

            DrawText(models[i].name.c_str(), 12, y + 4, 18, WHITE);
            DrawText(models[i].glbFile.c_str(), 12, y + 24, 12, LIGHTGRAY);

            // Click to select (shift = multi-select)
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    // Toggle in multi-select
                    if (inMulti) multiSelect.erase(i);
                    else multiSelect.insert(i);
                    // Also add current selectedModel to multi if not already
                    if (selectedModel >= 0 && !multiSelect.count(selectedModel))
                        multiSelect.insert(selectedModel);
                    multiSelect.insert(i);
                    selectedGroup = -1;
                } else {
                    selectedModel = i;
                    selectedGroup = -1;
                    multiSelect.clear();
                    startEditingSelected();
                }
            }

            // Delete button
            int delX = PANEL_W - 36, delW = 28;
            bool delHov = !blocked && m.x >= delX && m.x < delX + delW && m.y >= y && m.y < y + itemH;
            DrawRectangle(delX, y, delW, itemH, delHov ? Color{180,50,50,230} : Color{60,30,30,230});
            DrawRectangleLines(delX, y, delW, itemH, delHov ? RED : Color{100,50,50,200});
            int xw2 = MeasureText("X", 18);
            DrawText("X", delX + (delW - xw2) / 2, y + (itemH - 18) / 2, 18, WHITE);

            if (delHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Remove from group if in one
                if (models[i].groupId >= 0 && models[i].groupId < (int)groups.size()) {
                    auto& ci = groups[models[i].groupId].childIndices;
                    ci.erase(std::remove(ci.begin(), ci.end(), i), ci.end());
                    // Fix child indices > i
                    for (auto& idx : ci) { if (idx > i) idx--; }
                }
                // Fix all group child indices
                for (auto& g : groups) {
                    for (auto& idx : g.childIndices) { if (idx > i) idx--; }
                }
                // Fix groupId for models after this one
                models[i].unload();
                models.erase(models.begin() + i);
                for (auto& md : models) {
                    // groupId stays same, just indices in childIndices shifted
                }
                multiSelect.erase(i);
                // Shift multi-select indices
                std::set<int> newMulti;
                for (int si : multiSelect) { newMulti.insert(si > i ? si - 1 : si); }
                multiSelect = newMulti;

                if (selectedModel == i) { selectedModel = -1; editingRight = false; }
                else if (selectedModel > i) selectedModel--;
                break;
            }

            curY += itemH + 4;
        }

        // Multi-select hint
        if ((int)multiSelect.size() >= 2) {
            char hint[64];
            snprintf(hint, sizeof(hint), "Q: Group %d models", (int)multiSelect.size());
            int hw = MeasureText(hint, 14);
            DrawText(hint, (PANEL_W - hw) / 2, sH - 24, 14, ORANGE);
        }
    }

    // ── Right Panel (edit selected) ──────────────────────────

    void startEditingSelected() {
        if (selectedModel < 0 || selectedModel >= (int)models.size()) {
            editingRight = false;
            return;
        }
        auto& m = models[selectedModel];
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", m.x); editX = buf;
        snprintf(buf, sizeof(buf), "%.2f", m.y); editY = buf;
        snprintf(buf, sizeof(buf), "%.2f", m.z); editZ = buf;
        snprintf(buf, sizeof(buf), "%.1f", m.rotX); editRotX = buf;
        snprintf(buf, sizeof(buf), "%.1f", m.rotY); editRotY = buf;
        snprintf(buf, sizeof(buf), "%.1f", m.rotZ); editRotZ = buf;
        snprintf(buf, sizeof(buf), "%.2f", m.sizeY); editSizeY = buf;
        editingRight = true;
        editingGroup = false;
        editFocusField = 0;
    }

    void startEditingGroup() {
        if (selectedGroup < 0 || selectedGroup >= (int)groups.size()) {
            editingRight = false;
            return;
        }
        auto& g = groups[selectedGroup];
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", g.x); editGX = buf;
        snprintf(buf, sizeof(buf), "%.2f", g.y); editGY = buf;
        snprintf(buf, sizeof(buf), "%.2f", g.z); editGZ = buf;
        snprintf(buf, sizeof(buf), "%.2f", g.originX); editX = buf;
        snprintf(buf, sizeof(buf), "%.2f", g.originY); editY = buf;
        snprintf(buf, sizeof(buf), "%.2f", g.originZ); editZ = buf;
        snprintf(buf, sizeof(buf), "%.1f", g.rotX); editRotX = buf;
        snprintf(buf, sizeof(buf), "%.1f", g.rotY); editRotY = buf;
        snprintf(buf, sizeof(buf), "%.1f", g.rotZ); editRotZ = buf;
        editingRight = true;
        editingGroup = true;
        editFocusField = 0;
    }

    void drawRightPanel() {
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        int px = sW - PANEL_W;
        DrawRectangle(px, 0, PANEL_W, sH, { 18, 18, 24, 240 });
        DrawLine(px, 0, px, sH, { 60, 60, 80, 200 });

        DrawText("Properties", px + 10, 10, 22, WHITE);
        DrawLine(px + 10, 36, sW - 10, 36, { 60, 60, 80, 200 });

        // Group selected
        if (selectedGroup >= 0 && selectedGroup < (int)groups.size()) {
            auto& g = groups[selectedGroup];
            DrawText(g.name.c_str(), px + 10, 46, 20, MAGENTA);
            DrawText("Group", px + 10, 68, 14, LIGHTGRAY);

            int fW = PANEL_W - 20, fH = 32;
            int startRY = 94;
            const char* labels[] = { "X:", "Y:", "Z:", "Origin X:", "Origin Y:", "Origin Z:", "Rot X:", "Rot Y:", "Rot Z:" };
            std::string* fields[] = { &editGX, &editGY, &editGZ, &editX, &editY, &editZ, &editRotX, &editRotY, &editRotZ };

            for (int i = 0; i < 9; i++) {
                int fy = startRY + i * (fH + 14);
                DrawText(labels[i], px + 10, fy, 16, LIGHTGRAY);
                int ix = px + 10, iy = fy + 18;
                bool focused = editingRight && editingGroup && editFocusField == i;
                Color border = focused ? WHITE : Color{100,100,120,255};
                DrawRectangle(ix, iy, fW, fH, {30,30,40,255});
                DrawRectangleLines(ix, iy, fW, fH, border);
                DrawText(fields[i]->c_str(), ix + 6, iy + 6, 18, WHITE);

                if (focused && ((int)(GetTime() * 2.0) % 2) == 0) {
                    int cx2 = ix + 6 + MeasureText(fields[i]->c_str(), 18);
                    DrawRectangle(cx2, iy + 4, 2, 24, WHITE);
                }

                if (!loadDialogOpen && !escMenuOpen && !groupNamingOpen) {
                    Vector2 mo = GetMousePosition();
                    if (mo.x >= ix && mo.x < ix + fW && mo.y >= iy && mo.y < iy + fH) {
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            editingRight = true;
                            editingGroup = true;
                            editFocusField = i;
                        }
                    }
                }
            }
            return;
        }

        // Model selected
        if (selectedModel < 0 || selectedModel >= (int)models.size()) {
            DrawText("Select a model", px + 10, 50, 16, GRAY);
            return;
        }

        auto& m = models[selectedModel];
        DrawText(m.name.c_str(), px + 10, 46, 20, WHITE);
        DrawText(m.glbFile.c_str(), px + 10, 68, 14, LIGHTGRAY);

        int fW = PANEL_W - 20, fH = 32;
        int startRY = 94;
        const char* labels[] = { "X:", "Y:", "Z:", "Rot X:", "Rot Y:", "Rot Z:", "Size:" };
        std::string* fields[] = { &editX, &editY, &editZ, &editRotX, &editRotY, &editRotZ, &editSizeY };

        for (int i = 0; i < 7; i++) {
            int fy = startRY + i * (fH + 20);
            DrawText(labels[i], px + 10, fy, 18, LIGHTGRAY);
            int ix = px + 10, iy = fy + 20;
            bool focused = editingRight && !editingGroup && editFocusField == i;
            Color border = focused ? WHITE : Color{100,100,120,255};
            DrawRectangle(ix, iy, fW, fH, {30,30,40,255});
            DrawRectangleLines(ix, iy, fW, fH, border);
            DrawText(fields[i]->c_str(), ix + 6, iy + 6, 18, WHITE);

            if (focused && ((int)(GetTime() * 2.0) % 2) == 0) {
                int cx2 = ix + 6 + MeasureText(fields[i]->c_str(), 18);
                DrawRectangle(cx2, iy + 4, 2, 24, WHITE);
            }

            if (!loadDialogOpen && !escMenuOpen && !groupNamingOpen) {
                Vector2 mo = GetMousePosition();
                if (mo.x >= ix && mo.x < ix + fW && mo.y >= iy && mo.y < iy + fH) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        editingRight = true;
                        editingGroup = false;
                        editFocusField = i;
                    }
                }
            }
        }
    }

    // ── ESC Menu ─────────────────────────────────────────────

    bool drawEscMenu(std::string& savePath, std::string& structureName,
                     std::string& structureType, PlacedBlocks& placedBlocks,
                     EditorCamera& editorCam) {
        bool exitToMenu = false;
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
            saveSpriteScene(savePath, structureName, structureType, placedBlocks, editorCam);
            placedBlocks.matrix.clear();
            placedBlocks.placeholders = {{0,0,0}};
            placedBlocks.rebuildPlaceholderSet();
            placedBlocks.markDirty();
            placedBlocks.removed.clear();
            unloadAll();
            reset();
            exitToMenu = true;
        }

        if (drawMenuButton("Load Model", bx, by + bh + gap, bw, bh)) {
            escMenuOpen = false;
            loadDialogOpen = true;
            loadPreview = false;
            refreshGlbList();
            selectedGlb = -1;
            glbScrollOffset = 0;
            importName.clear();
            importX = "0"; importY = "0"; importZ = "0";
            importRotX = "0"; importRotY = "0"; importRotZ = "0";
            importSizeY = "1";
            loadFocusField = 0;
        }

        return exitToMenu;
    }

    // ── Load Model Dialog ────────────────────────────────────

    void drawLoadDialog() {
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int pW = 560, pH = 640;
        int pX = (sW-pW)/2, pY = (sH-pH)/2;
        DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
        DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

        int tw = MeasureText("Load Model", 28);
        DrawText("Load Model", (sW-tw)/2, pY+14, 28, WHITE);

        int lx = pX + 16, lw = pW - 32;

        // GLB file list
        int ly = pY + 52, ih = 40, maxVis = 4;
        DrawText("Model file:", lx, ly - 18, 16, LIGHTGRAY);
        drawGlbList(lx, ly, lw, ih, maxVis);

        // Input fields
        int fieldY = ly + maxVis * (ih + 3) + 20;
        int fH = 32, fW = lw;

        // Name
        drawLoadField("Name:", lx, fieldY, fW, fH, importName, 0, false);

        // Position
        int posY = fieldY + fH + 28;
        DrawText("Position:", lx, posY - 18, 16, LIGHTGRAY);
        int f3W = (fW - 20) / 3;
        drawLoadField("X", lx, posY, f3W, fH, importX, 1, true);
        drawLoadField("Y", lx + f3W + 10, posY, f3W, fH, importY, 2, true);
        drawLoadField("Z", lx + 2 * (f3W + 10), posY, f3W, fH, importZ, 3, true);

        // Rotation
        int rotY2 = posY + fH + 28;
        DrawText("Rotation:", lx, rotY2 - 18, 16, LIGHTGRAY);
        drawLoadField("X", lx, rotY2, f3W, fH, importRotX, 4, true);
        drawLoadField("Y", lx + f3W + 10, rotY2, f3W, fH, importRotY, 5, true);
        drawLoadField("Z", lx + 2 * (f3W + 10), rotY2, f3W, fH, importRotZ, 6, true);

        // SizeY
        int sizeRow = rotY2 + fH + 28;
        drawLoadField("Size:", lx, sizeRow, fW / 3, fH, importSizeY, 7, true);

        DrawText("Tab to switch fields", lx, sizeRow + fH + 4, 14, GRAY);

        // Check for duplicate name
        bool nameTaken = false;
        for (auto& existing : models) {
            if (existing.name == importName) { nameTaken = true; break; }
        }
        if (nameTaken) {
            DrawText("Name already in use!", lx, sizeRow + fH + 22, 16, RED);
        }

        // Buttons
        bool hasGlb = selectedGlb >= 0 && selectedGlb < (int)availableGlbs.size();
        bool canImport = hasGlb && !importName.empty() && !nameTaken;

        int bw = 140, bh = 42, bg = 12;
        int tbw = bw * 3 + bg * 2;
        int bsx = (sW - tbw) / 2;
        int by = pY + pH - bh - 20;

        if (drawMenuButton("Cancel", bsx, by, bw, bh)) {
            loadDialogOpen = false;
            previewModel.unload();
        }

        if (drawMenuButton("Preview", bsx + bw + bg, by, bw, bh, canImport)) {
            if (canImport) {
                previewModel.unload();
                previewModel.glbFile = availableGlbs[selectedGlb];
                previewModel.name = importName;
                previewModel.x = importX.empty() ? 0 : std::strtof(importX.c_str(), nullptr);
                previewModel.y = importY.empty() ? 0 : std::strtof(importY.c_str(), nullptr);
                previewModel.z = importZ.empty() ? 0 : std::strtof(importZ.c_str(), nullptr);
                previewModel.rotX = importRotX.empty() ? 0 : std::strtof(importRotX.c_str(), nullptr);
                previewModel.rotY = importRotY.empty() ? 0 : std::strtof(importRotY.c_str(), nullptr);
                previewModel.rotZ = importRotZ.empty() ? 0 : std::strtof(importRotZ.c_str(), nullptr);
                previewModel.sizeY = importSizeY.empty() ? 1 : std::strtof(importSizeY.c_str(), nullptr);
                if (previewModel.sizeY <= 0) previewModel.sizeY = 1;
                previewModel.load();
                loadPreview = true;
            }
        }

        if (drawMenuButton("Import", bsx + 2 * (bw + bg), by, bw, bh, canImport)) {
            if (canImport) {
                SpriteModel m;
                m.glbFile = availableGlbs[selectedGlb];
                m.name = importName;
                m.x = importX.empty() ? 0 : std::strtof(importX.c_str(), nullptr);
                m.y = importY.empty() ? 0 : std::strtof(importY.c_str(), nullptr);
                m.z = importZ.empty() ? 0 : std::strtof(importZ.c_str(), nullptr);
                m.rotX = importRotX.empty() ? 0 : std::strtof(importRotX.c_str(), nullptr);
                m.rotY = importRotY.empty() ? 0 : std::strtof(importRotY.c_str(), nullptr);
                m.rotZ = importRotZ.empty() ? 0 : std::strtof(importRotZ.c_str(), nullptr);
                m.sizeY = importSizeY.empty() ? 1 : std::strtof(importSizeY.c_str(), nullptr);
                if (m.sizeY <= 0) m.sizeY = 1;
                m.load();
                models.push_back(std::move(m));
                selectedModel = (int)models.size() - 1;
                startEditingSelected();
                loadDialogOpen = false;
                loadPreview = false;
                previewModel.unload();
            }
        }
    }

    void drawGlbList(int lx, int ly, int lw, int ih, int maxVis) {
        if (availableGlbs.empty()) {
            DrawText("No .glb files in AbstractModels/", lx, ly, 16, GRAY);
            return;
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            glbScrollOffset -= (int)wheel;
            if (glbScrollOffset < 0) glbScrollOffset = 0;
            int mo = (int)availableGlbs.size() - maxVis;
            if (mo < 0) mo = 0;
            if (glbScrollOffset > mo) glbScrollOffset = mo;
        }

        for (int i = 0; i < maxVis && (i + glbScrollOffset) < (int)availableGlbs.size(); i++) {
            int idx = i + glbScrollOffset;
            int iy = ly + i * (ih + 3);
            Vector2 m = GetMousePosition();
            bool hov = m.x >= lx && m.x < lx + lw && m.y >= iy && m.y < iy + ih;
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedGlb = idx;

            Color bg = (idx == selectedGlb) ? Color{60,60,90,230}
                     : hov ? Color{45,45,60,230} : Color{30,30,40,230};
            DrawRectangle(lx, iy, lw, ih, bg);
            DrawRectangleLines(lx, iy, lw, ih, (idx == selectedGlb) ? WHITE : Color{80,80,80,200});
            DrawText(availableGlbs[idx].c_str(), lx + 10, iy + (ih - 18) / 2, 18, WHITE);
        }
    }

    void drawLoadField(const char* label, int fx, int fy, int fw, int fh,
                       std::string& value, int fieldIdx, bool compact) {
        if (compact) {
            // Label inline above
            DrawText(label, fx, fy - 16, 14, LIGHTGRAY);
        } else {
            DrawText(label, fx, fy - 18, 16, LIGHTGRAY);
        }
        bool focused = (loadFocusField == fieldIdx);
        Color border = focused ? WHITE : Color{100,100,120,255};
        DrawRectangle(fx, fy, fw, fh, {30,30,40,255});
        DrawRectangleLines(fx, fy, fw, fh, border);
        DrawText(value.c_str(), fx + 6, fy + 6, 18, WHITE);

        if (focused && ((int)(GetTime() * 2.0) % 2) == 0) {
            int cx = fx + 6 + MeasureText(value.c_str(), 18);
            DrawRectangle(cx, fy + 4, 2, 24, WHITE);
        }

        Vector2 m = GetMousePosition();
        if (m.x >= fx && m.x < fx + fw && m.y >= fy && m.y < fy + fh)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) loadFocusField = fieldIdx;
    }

    // ── Preview banner ───────────────────────────────────────

    void drawPreviewBanner() {
        const char* t = "MODEL PREVIEW  -  Press ESC to go back";
        int tw = MeasureText(t, 24);
        int sW = GetScreenWidth();
        DrawRectangle(sW/2 - tw/2 - 12, 40, tw + 24, 36, {0,0,0,180});
        DrawText(t, sW/2 - tw/2, 46, 24, YELLOW);
    }

    // ── Group naming dialog ────────────────────────────────────

    void drawGroupNamingDialog() {
        int sW = GetScreenWidth(), sH = GetScreenHeight();
        DrawRectangle(-50, -50, sW+100, sH+100, {0,0,0,160});

        int pW = 400, pH = 160;
        int pX = (sW-pW)/2, pY = (sH-pH)/2;
        DrawRectangle(pX, pY, pW, pH, {20,20,28,240});
        DrawRectangleLines(pX, pY, pW, pH, {80,80,80,200});

        int tw = MeasureText("Name this group", 24);
        DrawText("Name this group", (sW-tw)/2, pY+14, 24, WHITE);

        int fX = pX+20, fY = pY+50, fW = pW-40, fH = 36;
        DrawRectangle(fX, fY, fW, fH, {30,30,40,255});
        DrawRectangleLines(fX, fY, fW, fH, WHITE);
        DrawText(groupName.c_str(), fX+8, fY+8, 20, WHITE);

        if (((int)(GetTime()*2.0)%2)==0) {
            int cx = fX+8+MeasureText(groupName.c_str(), 20);
            DrawRectangle(cx, fY+6, 2, 24, WHITE);
        }

        // Check duplicate
        bool taken = false;
        for (auto& g : groups) { if (g.name == groupName) { taken = true; break; } }
        if (taken && !groupName.empty()) {
            DrawText("Name already in use!", fX, fY+fH+4, 14, RED);
        }

        char hint[64];
        snprintf(hint, sizeof(hint), "Grouping %d models  |  Enter to confirm  |  ESC to cancel", (int)multiSelect.size());
        int hw = MeasureText(hint, 14);
        DrawText(hint, (sW-hw)/2, pY+pH-28, 14, GRAY);
    }

    // ── Save / Load sprite scene data ────────────────────────

    void saveSpriteScene(const std::string& filepath, const std::string& name,
                         const std::string& type, const PlacedBlocks& blocks,
                         const EditorCamera& cam) {
        saveStructure(filepath, name, type, blocks, cam);

        std::ofstream out(filepath, std::ios::app);
        if (!out.is_open()) return;

        // Save scene origin
        if (originSet) {
            out << "spriteorigin=" << sceneOrigin.x << "," << sceneOrigin.y << "," << sceneOrigin.z << "\n";
        }

        // Save models with groupId
        for (int i = 0; i < (int)models.size(); i++) {
            auto& m = models[i];
            out << "spritemodel=" << m.name
                << "," << m.glbFile
                << "," << m.x << "," << m.y << "," << m.z
                << "," << m.rotX << "," << m.rotY << "," << m.rotZ
                << "," << m.sizeY
                << "," << m.groupId << "\n";
        }

        // Save groups
        for (auto& g : groups) {
            out << "spritegroup=" << g.name
                << "," << g.x << "," << g.y << "," << g.z
                << "," << g.originX << "," << g.originY << "," << g.originZ
                << "," << g.rotX << "," << g.rotY << "," << g.rotZ << "\n";
        }
    }

    void loadSpriteModels(const std::string& filepath) {
        unloadAll();
        originSet = false;
        std::ifstream in(filepath);
        if (!in.is_open()) return;
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("spriteorigin=", 0) == 0) {
                float ox, oy, oz;
                if (sscanf(line.c_str() + 13, "%f,%f,%f", &ox, &oy, &oz) == 3) {
                    sceneOrigin = {ox, oy, oz};
                    originSet = true;
                }
            } else if (line.rfind("spritemodel=", 0) == 0) {
                std::string rest = line.substr(12);
                std::vector<int> commas;
                for (int i = 0; i < (int)rest.size(); i++) {
                    if (rest[i] == ',') commas.push_back(i);
                }
                if ((int)commas.size() >= 8) {
                    SpriteModel m;
                    m.name = rest.substr(0, commas[0]);
                    m.glbFile = rest.substr(commas[0]+1, commas[1]-commas[0]-1);
                    m.x = std::strtof(rest.substr(commas[1]+1, commas[2]-commas[1]-1).c_str(), nullptr);
                    m.y = std::strtof(rest.substr(commas[2]+1, commas[3]-commas[2]-1).c_str(), nullptr);
                    m.z = std::strtof(rest.substr(commas[3]+1, commas[4]-commas[3]-1).c_str(), nullptr);
                    m.rotX = std::strtof(rest.substr(commas[4]+1, commas[5]-commas[4]-1).c_str(), nullptr);
                    m.rotY = std::strtof(rest.substr(commas[5]+1, commas[6]-commas[5]-1).c_str(), nullptr);
                    m.rotZ = std::strtof(rest.substr(commas[6]+1, commas[7]-commas[6]-1).c_str(), nullptr);
                    m.sizeY = std::strtof(rest.substr(commas[7]+1, ((int)commas.size() > 8 ? commas[8] : (int)rest.size()) - commas[7]-1).c_str(), nullptr);
                    if (m.sizeY <= 0) m.sizeY = 1;
                    // groupId (optional 10th field)
                    if ((int)commas.size() >= 9) {
                        m.groupId = std::atoi(rest.substr(commas[8]+1).c_str());
                    }
                    m.load();
                    models.push_back(std::move(m));
                }
            } else if (line.rfind("spritegroup=", 0) == 0) {
                std::string rest = line.substr(12);
                std::vector<int> commas;
                for (int i = 0; i < (int)rest.size(); i++) {
                    if (rest[i] == ',') commas.push_back(i);
                }
                if ((int)commas.size() >= 9) {
                    SpriteGroup g;
                    g.name = rest.substr(0, commas[0]);
                    g.x = std::strtof(rest.substr(commas[0]+1, commas[1]-commas[0]-1).c_str(), nullptr);
                    g.y = std::strtof(rest.substr(commas[1]+1, commas[2]-commas[1]-1).c_str(), nullptr);
                    g.z = std::strtof(rest.substr(commas[2]+1, commas[3]-commas[2]-1).c_str(), nullptr);
                    g.originX = std::strtof(rest.substr(commas[3]+1, commas[4]-commas[3]-1).c_str(), nullptr);
                    g.originY = std::strtof(rest.substr(commas[4]+1, commas[5]-commas[4]-1).c_str(), nullptr);
                    g.originZ = std::strtof(rest.substr(commas[5]+1, commas[6]-commas[5]-1).c_str(), nullptr);
                    g.rotX = std::strtof(rest.substr(commas[6]+1, commas[7]-commas[6]-1).c_str(), nullptr);
                    g.rotY = std::strtof(rest.substr(commas[7]+1, commas[8]-commas[7]-1).c_str(), nullptr);
                    g.rotZ = std::strtof(rest.substr(commas[8]+1).c_str(), nullptr);
                    groups.push_back(std::move(g));
                }
            }
        }

        // Rebuild group childIndices from model groupIds
        for (auto& g : groups) g.childIndices.clear();
        for (int i = 0; i < (int)models.size(); i++) {
            int gid = models[i].groupId;
            if (gid >= 0 && gid < (int)groups.size()) {
                groups[gid].childIndices.push_back(i);
            }
        }
    }

    // ── Main frame ───────────────────────────────────────────

    bool frame(float dt, EditorCamera& editorCam, PlacedBlocks& placedBlocks,
               GradientSky& sky, std::string& savePath, std::string& structureName,
               std::string& structureType, float& autoSaveTimer) {
        bool exitToMenu = false;

        handleInput();
        handleGroupNamingInput();
        handleLoadDialogInput();
        handleRightPanelInput();
        handleGizmoInput(editorCam);
        updateLogic(dt, editorCam, placedBlocks, savePath, structureName, structureType, autoSaveTimer);

        BeginDrawing();
            ClearBackground(BLACK);
            drawScene(editorCam, sky);
            drawLeftPanel();
            drawRightPanel();

            if (loadPreview) drawPreviewBanner();

            if (escMenuOpen)    exitToMenu = drawEscMenu(savePath, structureName,
                                                          structureType, placedBlocks, editorCam);
            if (loadDialogOpen && !loadPreview) drawLoadDialog();
            if (groupNamingOpen) drawGroupNamingDialog();
        EndDrawing();

        return exitToMenu;
    }
};
