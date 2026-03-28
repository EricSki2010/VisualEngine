#pragma once
#include "raylib.h"
#include "rlgl.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cmath>

// ── Sprite Part ──────────────────────────────────────────────────────
// A single named model within a sprite, with its own transform.

struct SpritePart {
    std::string name;
    std::string glbFile;
    float x = 0, y = 0, z = 0;           // position relative to sprite origin
    float rotX = 0, rotY = 0, rotZ = 0;  // rotation (degrees)
    float sizeY = 1.0f;                   // scale
    int groupId = -1;

    Model model = {};
    bool loaded = false;
    float baseScaleX = 1, baseScaleY = 1, baseScaleZ = 1;
    float offsetY = 0;

    // Snapshot of the original transforms (for resetting animations)
    float baseX = 0, baseY = 0, baseZ = 0;
    float baseRotX = 0, baseRotY = 0, baseRotZ = 0;

    void load(const std::string& basePath) {
        std::string path = basePath + "/" + glbFile;
        if (!FileExists(path.c_str())) return;
        model = LoadModel(path.c_str());
        loaded = true;
        BoundingBox bb = GetModelBoundingBox(model);
        float rawH = bb.max.y - bb.min.y;
        float rawW = bb.max.x - bb.min.x;
        float rawD = bb.max.z - bb.min.z;
        float maxExt = rawH;
        if (rawW > maxExt) maxExt = rawW;
        if (rawD > maxExt) maxExt = rawD;
        if (maxExt < 0.001f) maxExt = 1.0f;
        baseScaleX = 1.0f / maxExt;
        baseScaleY = 1.0f / maxExt;
        baseScaleZ = 1.0f / maxExt;
        float scaledMinY = bb.min.y * baseScaleY;
        offsetY = -scaledMinY;
    }

    void unload() {
        if (loaded) { UnloadModel(model); loaded = false; }
    }

    void saveBase() {
        baseX = x; baseY = y; baseZ = z;
        baseRotX = rotX; baseRotY = rotY; baseRotZ = rotZ;
    }

    void resetToBase() {
        x = baseX; y = baseY; z = baseZ;
        rotX = baseRotX; rotY = baseRotY; rotZ = baseRotZ;
    }
};

// ── Sprite Group ─────────────────────────────────────────────────────

struct SpritePartGroup {
    std::string name;
    float x = 0, y = 0, z = 0;
    float originX = 0, originY = 0, originZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    std::vector<int> childIndices;

    // Snapshot
    float baseX = 0, baseY = 0, baseZ = 0;
    float baseRotX = 0, baseRotY = 0, baseRotZ = 0;

    void transformPoint(float inX, float inY, float inZ,
                        float& outX, float& outY, float& outZ) const {
        float dx = inX - originX, dy = inY - originY, dz = inZ - originZ;
        float radY = rotY * DEG2RAD;
        float cosY = cosf(radY), sinY = sinf(radY);
        float tx = dx * cosY + dz * sinY;
        float tz = -dx * sinY + dz * cosY;
        dx = tx; dz = tz;
        float radX = rotX * DEG2RAD;
        float cosX = cosf(radX), sinX = sinf(radX);
        float ty = dy * cosX - dz * sinX;
        tz = dy * sinX + dz * cosX;
        dy = ty; dz = tz;
        float radZ = rotZ * DEG2RAD;
        float cosZ = cosf(radZ), sinZ = sinf(radZ);
        tx = dx * cosZ - dy * sinZ;
        ty = dx * sinZ + dy * cosZ;
        dx = tx; dy = ty;
        outX = originX + dx + x;
        outY = originY + dy + y;
        outZ = originZ + dz + z;
    }

    void saveBase() {
        baseX = x; baseY = y; baseZ = z;
        baseRotX = rotX; baseRotY = rotY; baseRotZ = rotZ;
    }

    void resetToBase() {
        x = baseX; y = baseY; z = baseZ;
        rotX = baseRotX; rotY = baseRotY; rotZ = baseRotZ;
    }
};

// ── Sprite ───────────────────────────────────────────────────────────
// A multi-model sprite loaded from a sprite scene .sav file.
//
// Usage:
//   Sprite chicken;
//   chicken.load("saves/sprites/chicken.sav");
//
//   // Access parts by name
//   chicken.part("left_leg").rotX += 90 * dt;
//   chicken.part("head").rotY = sin(time) * 20;
//
//   // Access groups by name
//   chicken.group("legs").rotX += 45 * dt;
//
//   // Reset all parts to their original transforms
//   chicken.resetPose();
//
//   // Draw at a world position with a facing angle
//   chicken.draw(position, facingAngle);

struct Sprite {
    std::vector<SpritePart> parts;
    std::vector<SpritePartGroup> groups;
    std::map<std::string, int> partIndex;   // name -> index
    std::map<std::string, int> groupIndex;  // name -> index
    Vector3 origin = {0, 0, 0};             // scene origin from editor
    bool originSet = false;

    // Dummy part returned when name not found (prevents crashes)
    SpritePart dummyPart;
    SpritePartGroup dummyGroup;

    ~Sprite() { unload(); }
    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&& o) noexcept
        : parts(std::move(o.parts)), groups(std::move(o.groups)),
          partIndex(std::move(o.partIndex)), groupIndex(std::move(o.groupIndex)),
          origin(o.origin), originSet(o.originSet) {}
    Sprite& operator=(Sprite&& o) noexcept {
        if (this != &o) {
            unload();
            parts = std::move(o.parts);
            groups = std::move(o.groups);
            partIndex = std::move(o.partIndex);
            groupIndex = std::move(o.groupIndex);
            origin = o.origin;
            originSet = o.originSet;
        }
        return *this;
    }

    // ── Load from .sav file ──────────────────────────────────

    void load(const std::string& filepath, const std::string& modelsDir = "AbstractModels") {
        unload();

        std::ifstream in(filepath);
        if (!in.is_open()) return;

        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("spriteorigin=", 0) == 0) {
                float ox, oy, oz;
                if (sscanf(line.c_str() + 13, "%f,%f,%f", &ox, &oy, &oz) == 3) {
                    origin = {ox, oy, oz};
                    originSet = true;
                }
            } else if (line.rfind("spritemodel=", 0) == 0) {
                parseModel(line.substr(12));
            } else if (line.rfind("spritegroup=", 0) == 0) {
                parseGroup(line.substr(12));
            }
        }

        // Rebuild group child indices from part groupIds
        for (auto& g : groups) g.childIndices.clear();
        for (int i = 0; i < (int)parts.size(); i++) {
            int gid = parts[i].groupId;
            if (gid >= 0 && gid < (int)groups.size()) {
                groups[gid].childIndices.push_back(i);
            }
        }

        // Build lookup maps
        for (int i = 0; i < (int)parts.size(); i++) {
            partIndex[parts[i].name] = i;
        }
        for (int i = 0; i < (int)groups.size(); i++) {
            groupIndex[groups[i].name] = i;
        }

        // Load all model files
        for (auto& p : parts) {
            p.load(modelsDir);
        }

        // Center parts relative to scene origin
        if (originSet) {
            for (auto& p : parts) {
                p.x -= origin.x;
                p.y -= origin.y;
                p.z -= origin.z;
            }
            for (auto& g : groups) {
                g.originX -= origin.x;
                g.originY -= origin.y;
                g.originZ -= origin.z;
            }
        }

        // Save base poses
        for (auto& p : parts) p.saveBase();
        for (auto& g : groups) g.saveBase();
    }

    void unload() {
        for (auto& p : parts) p.unload();
        parts.clear();
        groups.clear();
        partIndex.clear();
        groupIndex.clear();
    }

    // ── Access parts and groups by name ──────────────────────

    SpritePart& part(const std::string& name) {
        auto it = partIndex.find(name);
        if (it != partIndex.end()) return parts[it->second];
        return dummyPart;
    }

    SpritePartGroup& group(const std::string& name) {
        auto it = groupIndex.find(name);
        if (it != groupIndex.end()) return groups[it->second];
        return dummyGroup;
    }

    bool hasPart(const std::string& name) const {
        return partIndex.count(name) > 0;
    }

    bool hasGroup(const std::string& name) const {
        return groupIndex.count(name) > 0;
    }

    // ── Reset all transforms to the loaded base pose ─────────

    void resetPose() {
        for (auto& p : parts) p.resetToBase();
        for (auto& g : groups) g.resetToBase();
    }

    // ── Draw at a world position ─────────────────────────────
    // worldPos: where to place the sprite in the world
    // facing: Y-axis rotation in degrees (like entity facingAngle)
    // scale: uniform scale multiplier (default 1.0)

    void draw(Vector3 worldPos, float facing = 0.0f, float scale = 1.0f) const {
        rlPushMatrix();
            rlTranslatef(worldPos.x, worldPos.y, worldPos.z);
            rlRotatef(facing, 0, 1, 0);
            rlScalef(scale, scale, scale);

            for (int i = 0; i < (int)parts.size(); i++) {
                auto& p = parts[i];
                if (!p.loaded) continue;

                // Get effective position (with group transform)
                float ex = p.x, ey = p.y, ez = p.z;
                if (p.groupId >= 0 && p.groupId < (int)groups.size()) {
                    groups[p.groupId].transformPoint(p.x, p.y, p.z, ex, ey, ez);
                }

                float s = p.sizeY;
                rlDisableBackfaceCulling();
                rlPushMatrix();
                    rlTranslatef(ex, ey + p.offsetY * s, ez);
                    rlRotatef(p.rotY, 0, 1, 0);
                    rlRotatef(p.rotX, 1, 0, 0);
                    rlRotatef(p.rotZ, 0, 0, 1);
                    rlScalef(p.baseScaleX * s, p.baseScaleY * s, p.baseScaleZ * s);
                    DrawModel(p.model, {0, 0, 0}, 1.0f, WHITE);
                rlPopMatrix();
                rlEnableBackfaceCulling();
            }

        rlPopMatrix();
    }

private:
    void parseModel(const std::string& rest) {
        std::vector<int> commas;
        for (int i = 0; i < (int)rest.size(); i++) {
            if (rest[i] == ',') commas.push_back(i);
        }
        if ((int)commas.size() < 8) return;

        SpritePart p;
        p.name = rest.substr(0, commas[0]);
        p.glbFile = rest.substr(commas[0]+1, commas[1]-commas[0]-1);
        p.x = std::strtof(rest.substr(commas[1]+1, commas[2]-commas[1]-1).c_str(), nullptr);
        p.y = std::strtof(rest.substr(commas[2]+1, commas[3]-commas[2]-1).c_str(), nullptr);
        p.z = std::strtof(rest.substr(commas[3]+1, commas[4]-commas[3]-1).c_str(), nullptr);
        p.rotX = std::strtof(rest.substr(commas[4]+1, commas[5]-commas[4]-1).c_str(), nullptr);
        p.rotY = std::strtof(rest.substr(commas[5]+1, commas[6]-commas[5]-1).c_str(), nullptr);
        p.rotZ = std::strtof(rest.substr(commas[6]+1, commas[7]-commas[6]-1).c_str(), nullptr);
        p.sizeY = std::strtof(rest.substr(commas[7]+1, ((int)commas.size() > 8 ? commas[8] : (int)rest.size()) - commas[7]-1).c_str(), nullptr);
        if (p.sizeY <= 0) p.sizeY = 1;
        if ((int)commas.size() >= 9) {
            p.groupId = std::atoi(rest.substr(commas[8]+1).c_str());
        }
        parts.push_back(std::move(p));
    }

    void parseGroup(const std::string& rest) {
        std::vector<int> commas;
        for (int i = 0; i < (int)rest.size(); i++) {
            if (rest[i] == ',') commas.push_back(i);
        }
        if ((int)commas.size() < 9) return;

        SpritePartGroup g;
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
};
