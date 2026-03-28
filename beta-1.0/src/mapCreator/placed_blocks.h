#pragma once
#include "engine/gameplay/registry.h"
#include "block_ui.h"
#include "editor_camera.h"
#include "raylib.h"
#include "rlgl.h"
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <cstdio>
#include "raymath.h"

// ── Frustum culling ────────────────────────────────────────
struct Frustum {
    float planes[6][4]; // {a, b, c, d} where ax+by+cz+d >= 0 is inside
};

inline Frustum extractFrustum(Camera3D& cam) {
    Matrix view = GetCameraMatrix(cam);
    Matrix proj = rlGetMatrixProjection();
    Matrix vp = MatrixMultiply(view, proj);
    // Raylib Matrix is column-major: row0={m0,m4,m8,m12}, row3={m3,m7,m11,m15}
    Frustum f;
    // Left: row3 + row0
    f.planes[0][0] = vp.m3+vp.m0;  f.planes[0][1] = vp.m7+vp.m4;
    f.planes[0][2] = vp.m11+vp.m8; f.planes[0][3] = vp.m15+vp.m12;
    // Right: row3 - row0
    f.planes[1][0] = vp.m3-vp.m0;  f.planes[1][1] = vp.m7-vp.m4;
    f.planes[1][2] = vp.m11-vp.m8; f.planes[1][3] = vp.m15-vp.m12;
    // Bottom: row3 + row1
    f.planes[2][0] = vp.m3+vp.m1;  f.planes[2][1] = vp.m7+vp.m5;
    f.planes[2][2] = vp.m11+vp.m9; f.planes[2][3] = vp.m15+vp.m13;
    // Top: row3 - row1
    f.planes[3][0] = vp.m3-vp.m1;  f.planes[3][1] = vp.m7-vp.m5;
    f.planes[3][2] = vp.m11-vp.m9; f.planes[3][3] = vp.m15-vp.m13;
    // Near: row3 + row2
    f.planes[4][0] = vp.m3+vp.m2;  f.planes[4][1] = vp.m7+vp.m6;
    f.planes[4][2] = vp.m11+vp.m10;f.planes[4][3] = vp.m15+vp.m14;
    // Far: row3 - row2
    f.planes[5][0] = vp.m3-vp.m2;  f.planes[5][1] = vp.m7-vp.m6;
    f.planes[5][2] = vp.m11-vp.m10;f.planes[5][3] = vp.m15-vp.m14;
    // Normalize
    for (int i = 0; i < 6; i++) {
        float len = sqrtf(f.planes[i][0]*f.planes[i][0] +
                          f.planes[i][1]*f.planes[i][1] +
                          f.planes[i][2]*f.planes[i][2]);
        if (len > 0.0f) {
            f.planes[i][0] /= len; f.planes[i][1] /= len;
            f.planes[i][2] /= len; f.planes[i][3] /= len;
        }
    }
    return f;
}

inline bool isInFrustum(const Frustum& f, float x, float y, float z) {
    const float radius = 0.866f; // sqrt(3)/2, bounding sphere of 1x1x1 cube
    for (int i = 0; i < 6; i++) {
        float dist = f.planes[i][0]*x + f.planes[i][1]*y + f.planes[i][2]*z + f.planes[i][3];
        if (dist < -radius) return false;
    }
    return true;
}

struct Vec3i {
    int x, y, z;
    bool operator<(const Vec3i& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
    bool operator==(const Vec3i& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct PlacedBlocks {
    // placeholders[0] is the structure origin in absolute world coordinates.
    // All entries in 'placed' are offsets relative to this origin.
    // In model mode (modelSize > 0), placeholders only stores the origin.
    std::vector<Vec3i> placeholders = { {0, 0, 0} };
    std::set<Vec3i> placeholderSet = { {0, 0, 0} };
    std::map<Vec3i, std::string> placed;

    // ── Model mode: defined cube instead of individual placeholders ──
    int modelSize = 0;              // 0 = structure mode, >0 = model cube of this size
    std::set<Vec3i> removed;        // blocks carved out of the model cube

    void rebuildPlaceholderSet() {
        placeholderSet.clear();
        placeholderSet.insert(placeholders.begin(), placeholders.end());
    }

    Vec3i getOrigin() const {
        return placeholders.empty() ? Vec3i{0, 0, 0} : placeholders[0];
    }

    // Model bounds helpers
    bool isInModelBounds(Vec3i offset) const {
        int half = modelSize / 2;
        int minX = -half, maxX = -half + modelSize - 1;
        int minY = 0,     maxY = modelSize - 1;
        int minZ = -half, maxZ = -half + modelSize - 1;
        return offset.x >= minX && offset.x <= maxX &&
               offset.y >= minY && offset.y <= maxY &&
               offset.z >= minZ && offset.z <= maxZ;
    }

    Vec3i toWorld(Vec3i offset) const {
        Vec3i o = getOrigin();
        return { o.x + offset.x, o.y + offset.y, o.z + offset.z };
    }

    Vec3i toOffset(Vec3i world) const {
        Vec3i o = getOrigin();
        return { world.x - o.x, world.y - o.y, world.z - o.z };
    }

    bool hasBlockAt(Vec3i offset) const {
        if (modelSize > 0)
            return isInModelBounds(offset) && !removed.count(offset);
        return placed.count(offset) || placeholderSet.count(offset);
    }

    // Collect surface offsets for model mode into a set (no duplicates).
    // Only iterates 6*N² face positions + neighbors of removed blocks.
    void collectModelSurface(std::set<Vec3i>& out) const {
        static const Vec3i dirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        int half = modelSize / 2;
        int minX = -half, maxX = -half + modelSize - 1;
        int minY = 0,     maxY = modelSize - 1;
        int minZ = -half, maxZ = -half + modelSize - 1;

        // 6 faces of the bounding cube
        for (int a = minX; a <= maxX; a++)
            for (int b = minZ; b <= maxZ; b++) {
                if (!removed.count({a, minY, b})) out.insert({a, minY, b});
                if (!removed.count({a, maxY, b})) out.insert({a, maxY, b});
            }
        for (int a = minX; a <= maxX; a++)
            for (int b = minY; b <= maxY; b++) {
                if (!removed.count({a, b, minZ})) out.insert({a, b, minZ});
                if (!removed.count({a, b, maxZ})) out.insert({a, b, maxZ});
            }
        for (int a = minY; a <= maxY; a++)
            for (int b = minZ; b <= maxZ; b++) {
                if (!removed.count({minX, a, b})) out.insert({minX, a, b});
                if (!removed.count({maxX, a, b})) out.insert({maxX, a, b});
            }

        // Interior blocks exposed by removed neighbors
        for (auto& r : removed) {
            for (int i = 0; i < 6; i++) {
                Vec3i nb = {r.x + dirs[i].x, r.y + dirs[i].y, r.z + dirs[i].z};
                if (isInModelBounds(nb) && !removed.count(nb))
                    out.insert(nb);
            }
        }
    }

    // Returns all surface world positions (for raycasting).
    std::vector<Vec3i> allWorldPositions() {
        std::vector<Vec3i> all;

        if (modelSize > 0) {
            std::set<Vec3i> surface;
            collectModelSurface(surface);
            all.reserve(surface.size());
            for (auto& offset : surface)
                all.push_back(toWorld(offset));
        } else {
            static const Vec3i dirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            for (auto& p : placeholders) {
                bool surrounded = true;
                for (int i = 0; i < 6; i++) {
                    Vec3i nb = {p.x+dirs[i].x, p.y+dirs[i].y, p.z+dirs[i].z};
                    if (!hasBlockAt(nb)) { surrounded = false; break; }
                }
                if (surrounded && placed.find(p) == placed.end()) continue;
                all.push_back(toWorld(p));
            }
            for (auto& [offset, name] : placed) {
                if (!placeholderSet.count(offset))
                    all.push_back(toWorld(offset));
            }
        }
        return all;
    }

    struct HitResult { bool hit; Vec3i pos; Vector3 normal; float dist; };

    HitResult raycastBlocks(Ray ray, bool useFrustumCull = true) {
        HitResult best = { false, {0,0,0}, {0,0,0}, 999999.0f };
        auto all = allWorldPositions();
        for (auto& pos : all) {
            if (useFrustumCull && !isInFrustum(cachedFrustum, (float)pos.x, (float)pos.y, (float)pos.z))
                continue;
            BoundingBox bb = {
                { pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f },
                { pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f }
            };
            RayCollision hit = GetRayCollisionBox(ray, bb);
            if (hit.hit && hit.distance < best.dist) {
                best.hit = true;
                best.pos = pos;
                best.normal = hit.normal;
                best.dist = hit.distance;
            }
        }
        return best;
    }

    Vec3i normalToDir(Vector3 n) {
        if (fabsf(n.x) >= fabsf(n.y) && fabsf(n.x) >= fabsf(n.z))
            return { (n.x > 0) ? 1 : -1, 0, 0 };
        if (fabsf(n.y) >= fabsf(n.x) && fabsf(n.y) >= fabsf(n.z))
            return { 0, (n.y > 0) ? 1 : -1, 0 };
        return { 0, 0, (n.z > 0) ? 1 : -1 };
    }

    HitResult hover = { false, {0,0,0}, {0,0,0}, 0.0f };
    Vector3 currentOrigin = { 0.0f, 0.0f, 0.0f };
    Frustum cachedFrustum = {};

    void update(Camera3D& cam, BlockHotbar& hotbar, bool invOpen, EditorCamera& editorCam) {
        currentOrigin = editorCam.origin;

        {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
            hover = raycastBlocks(ray);
        }

        if (invOpen) return;

        static float lmbHoldTime = 0.0f;
        static bool lmbWasDown = false;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            lmbHoldTime += GetFrameTime();
            lmbWasDown = true;
        } else if (lmbWasDown) {
            if (lmbHoldTime < 0.2f) {
                Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
                HitResult hit = raycastBlocks(ray);
                if (hit.hit) {
                    editorCam.setOrigin((float)hit.pos.x, (float)hit.pos.y, (float)hit.pos.z);
                }
            }
            lmbHoldTime = 0.0f;
            lmbWasDown = false;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
            HitResult hit = raycastBlocks(ray);
            if (hit.hit) {
                const std::string& blockName = hotbar.getSelectedName();
                if (!blockName.empty()) {
                    Vec3i o = getOrigin();
                    Vec3i hitOffset = toOffset(hit.pos);

                    bool isEmptyPlaceholder = placeholderSet.count(hitOffset)
                        && placed.find(hitOffset) == placed.end();

                    if (isEmptyPlaceholder) {
                        placed[hitOffset] = blockName;
                    } else {
                        Vec3i dir = normalToDir(hit.normal);
                        Vec3i newWorld = { hit.pos.x + dir.x, hit.pos.y + dir.y, hit.pos.z + dir.z };
                        placed[toOffset(newWorld)] = blockName;
                    }
                }
            }
        }

        if (IsKeyPressed(KEY_X)) {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
            HitResult hit = raycastBlocks(ray);
            if (hit.hit) {
                Vec3i offset = toOffset(hit.pos);
                placed.erase(offset);
            }
        }
    }

    void drawBlock(BlockDef& def, Vector3 v) {
        if (def.formatType == "glb" && def.modelLoaded) {
            Vector3 modelPos = { v.x, v.y - 0.5f, v.z };
            DrawModelEx(def.model, modelPos,
                { 0, 1, 0 }, 0.0f,
                { def.modelScaleX, def.modelScaleY, def.modelScaleZ },
                WHITE);
        } else {
            Color col = { def.r, def.g, def.b, def.a };
            DrawCube(v, 1.0f, 1.0f, 1.0f, col);
            DrawCubeWires(v, 1.0f, 1.0f, 1.0f, {
                (unsigned char)(def.r / 2), (unsigned char)(def.g / 2),
                (unsigned char)(def.b / 2), 255 });
        }
    }

    void drawFaceOutline(Vec3i pos, Vector3 normal) {
        float x = (float)pos.x, y = (float)pos.y, z = (float)pos.z;
        Vec3i n = normalToDir(normal);
        float e = 0.501f;

        Vector3 corners[4];
        if (n.x == 1) {
            corners[0] = { x+e, y-0.5f, z-0.5f }; corners[1] = { x+e, y+0.5f, z-0.5f };
            corners[2] = { x+e, y+0.5f, z+0.5f }; corners[3] = { x+e, y-0.5f, z+0.5f };
        } else if (n.x == -1) {
            corners[0] = { x-e, y-0.5f, z-0.5f }; corners[1] = { x-e, y+0.5f, z-0.5f };
            corners[2] = { x-e, y+0.5f, z+0.5f }; corners[3] = { x-e, y-0.5f, z+0.5f };
        } else if (n.y == 1) {
            corners[0] = { x-0.5f, y+e, z-0.5f }; corners[1] = { x+0.5f, y+e, z-0.5f };
            corners[2] = { x+0.5f, y+e, z+0.5f }; corners[3] = { x-0.5f, y+e, z+0.5f };
        } else if (n.y == -1) {
            corners[0] = { x-0.5f, y-e, z-0.5f }; corners[1] = { x+0.5f, y-e, z-0.5f };
            corners[2] = { x+0.5f, y-e, z+0.5f }; corners[3] = { x-0.5f, y-e, z+0.5f };
        } else if (n.z == 1) {
            corners[0] = { x-0.5f, y-0.5f, z+e }; corners[1] = { x+0.5f, y-0.5f, z+e };
            corners[2] = { x+0.5f, y+0.5f, z+e }; corners[3] = { x-0.5f, y+0.5f, z+e };
        } else {
            corners[0] = { x-0.5f, y-0.5f, z-e }; corners[1] = { x+0.5f, y-0.5f, z-e };
            corners[2] = { x+0.5f, y+0.5f, z-e }; corners[3] = { x-0.5f, y+0.5f, z-e };
        }

        Color c = { 255, 255, 100, 255 };
        DrawLine3D(corners[0], corners[1], c);
        DrawLine3D(corners[1], corners[2], c);
        DrawLine3D(corners[2], corners[3], c);
        DrawLine3D(corners[3], corners[0], c);
    }

    // Face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    static int faceFromNormal(Vector3 n) {
        float ax = fabsf(n.x), ay = fabsf(n.y), az = fabsf(n.z);
        if (ax >= ay && ax >= az) return (n.x > 0) ? 0 : 1;
        if (ay >= ax && ay >= az) return (n.y > 0) ? 2 : 3;
        return (n.z > 0) ? 4 : 5;
    }

    static void decodeFaceColors(const std::string& str, Color faces[6]) {
        Color grey = GRAY;
        for (int i = 0; i < 6; i++) faces[i] = grey;

        if (str.empty()) return;

        // Old format: #RRGGBB = all faces same color
        if (str[0] == '#' && str.size() == 7) {
            unsigned int r, g, b;
            sscanf(str.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
            Color c = {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
            for (int i = 0; i < 6; i++) faces[i] = c;
            return;
        }

        // New format: f:RRGGBB:RRGGBB:RRGGBB:RRGGBB:RRGGBB:RRGGBB
        if (str[0] != 'f' || str.size() < 2) return;
        int fi = 0;
        size_t pos = 2; // skip "f:"
        while (fi < 6 && pos <= str.size()) {
            size_t next = str.find(':', pos);
            if (next == std::string::npos) next = str.size();
            if (next - pos == 6) {
                unsigned int r, g, b;
                sscanf(str.c_str() + pos, "%02x%02x%02x", &r, &g, &b);
                faces[fi] = {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
            }
            fi++;
            pos = next + 1;
        }
    }

    static std::string encodeFaceColors(Color faces[6]) {
        Color grey = GRAY;
        std::string result = "f";
        for (int i = 0; i < 6; i++) {
            result += ':';
            if (faces[i].r != grey.r || faces[i].g != grey.g || faces[i].b != grey.b) {
                char buf[7];
                snprintf(buf, sizeof(buf), "%02x%02x%02x", faces[i].r, faces[i].g, faces[i].b);
                result += buf;
            }
        }
        return result;
    }

    // Draw only the exposed faces of a face-colored block.
    // exposedMask: bit 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    void drawFaceColoredBlock(Vector3 pos, Color faces[6], bool wireframe, int exposedMask = 0x3F) {
        float x = pos.x, y = pos.y, z = pos.z;
        float w = 0.5f, h = 0.5f, l = 0.5f;

        // Count exposed faces for batch limit
        int faceCount = 0;
        for (int i = 0; i < 6; i++) if (exposedMask & (1 << i)) faceCount++;
        if (faceCount == 0) return;

        rlCheckRenderBatchLimit(faceCount * 6);
        rlBegin(RL_TRIANGLES);

        if (exposedMask & (1 << 0)) { // +X
            Color c = faces[0]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x+w,y-h,z-l); rlVertex3f(x+w,y+h,z-l); rlVertex3f(x+w,y+h,z+l);
            rlVertex3f(x+w,y-h,z+l); rlVertex3f(x+w,y-h,z-l); rlVertex3f(x+w,y+h,z+l);
        }
        if (exposedMask & (1 << 1)) { // -X
            Color c = faces[1]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x-w,y+h,z-l);
            rlVertex3f(x-w,y-h,z-l); rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y+h,z-l);
        }
        if (exposedMask & (1 << 2)) { // +Y
            Color c = faces[2]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y+h,z-l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x+w,y+h,z+l);
            rlVertex3f(x+w,y+h,z-l); rlVertex3f(x-w,y+h,z-l); rlVertex3f(x+w,y+h,z+l);
        }
        if (exposedMask & (1 << 3)) { // -Y
            Color c = faces[3]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y-h,z-l); rlVertex3f(x+w,y-h,z-l);
            rlVertex3f(x+w,y-h,z+l); rlVertex3f(x-w,y-h,z+l); rlVertex3f(x+w,y-h,z-l);
        }
        if (exposedMask & (1 << 4)) { // +Z
            Color c = faces[4]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x+w,y-h,z+l); rlVertex3f(x-w,y+h,z+l);
            rlVertex3f(x+w,y+h,z+l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x+w,y-h,z+l);
        }
        if (exposedMask & (1 << 5)) { // -Z
            Color c = faces[5]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x+w,y-h,z-l); rlVertex3f(x-w,y-h,z-l); rlVertex3f(x+w,y+h,z-l);
            rlVertex3f(x-w,y+h,z-l); rlVertex3f(x+w,y+h,z-l); rlVertex3f(x-w,y-h,z-l);
        }

        rlEnd();

        if (wireframe) {
            // Only draw wireframe edges for exposed faces
            float e = 0.501f;
            for (int i = 0; i < 6; i++) {
                if (!(exposedMask & (1 << i))) continue;
                Vector3 corners[4];
                if (i == 0) { // +X
                    corners[0]={x+e,y-h,z-l}; corners[1]={x+e,y+h,z-l};
                    corners[2]={x+e,y+h,z+l}; corners[3]={x+e,y-h,z+l};
                } else if (i == 1) { // -X
                    corners[0]={x-e,y-h,z-l}; corners[1]={x-e,y+h,z-l};
                    corners[2]={x-e,y+h,z+l}; corners[3]={x-e,y-h,z+l};
                } else if (i == 2) { // +Y
                    corners[0]={x-w,y+e,z-l}; corners[1]={x+w,y+e,z-l};
                    corners[2]={x+w,y+e,z+l}; corners[3]={x-w,y+e,z+l};
                } else if (i == 3) { // -Y
                    corners[0]={x-w,y-e,z-l}; corners[1]={x+w,y-e,z-l};
                    corners[2]={x+w,y-e,z+l}; corners[3]={x-w,y-e,z+l};
                } else if (i == 4) { // +Z
                    corners[0]={x-w,y-h,z+e}; corners[1]={x+w,y-h,z+e};
                    corners[2]={x+w,y+h,z+e}; corners[3]={x-w,y+h,z+e};
                } else { // -Z
                    corners[0]={x-w,y-h,z-e}; corners[1]={x+w,y-h,z-e};
                    corners[2]={x+w,y+h,z-e}; corners[3]={x-w,y+h,z-e};
                }
                DrawLine3D(corners[0],corners[1],DARKGRAY);
                DrawLine3D(corners[1],corners[2],DARKGRAY);
                DrawLine3D(corners[2],corners[3],DARKGRAY);
                DrawLine3D(corners[3],corners[0],DARKGRAY);
            }
        }
    }

    // Compute bitmask of exposed faces for a block at the given offset.
    // Bit 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    int getExposedMask(Vec3i offset) const {
        static const Vec3i dirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        int mask = 0;
        for (int i = 0; i < 6; i++) {
            Vec3i neighbor = { offset.x + dirs[i].x, offset.y + dirs[i].y, offset.z + dirs[i].z };
            if (!hasBlockAt(neighbor)) mask |= (1 << i);
        }
        return mask;
    }

    // Draw a solid-color cube with only exposed faces
    void drawSolidBlockFaces(Vector3 pos, Color col, bool wireframe, int exposedMask) {
        if (exposedMask == 0) return;
        Color faces[6];
        for (int i = 0; i < 6; i++) faces[i] = col;
        drawFaceColoredBlock(pos, faces, wireframe, exposedMask);
    }

    void drawPlacedAt(Vector3 v, const std::string& name, Registry<BlockDef>& blocks, bool wireframe = true, int exposedMask = 0x3F) {
        if (!name.empty() && (name[0] == '#' || name[0] == 'f')) {
            Color faces[6];
            decodeFaceColors(name, faces);
            drawFaceColoredBlock(v, faces, wireframe, exposedMask);
        } else {
            auto bIt = blocks.entries.find(name);
            if (bIt != blocks.entries.end()) {
                BlockDef& def = bIt->second;
                if (def.formatType == "glb" && def.modelLoaded) {
                    // GLB models always draw fully (can't do per-face)
                    drawBlock(def, v);
                } else {
                    Color col = { def.r, def.g, def.b, def.a };
                    drawSolidBlockFaces(v, col, wireframe, exposedMask);
                }
            }
        }
    }

    // Shared helper: draw a single block at offset if it has exposed faces
    void drawBlockAtOffset(Vec3i offset, Registry<BlockDef>& blocks, bool wireframe) {
        int mask = getExposedMask(offset);
        if (mask == 0) return;
        Vec3i w = toWorld(offset);
        float fx = (float)w.x, fy = (float)w.y, fz = (float)w.z;
        if (!isInFrustum(cachedFrustum, fx, fy, fz)) return;
        Vector3 v = { fx, fy, fz };
        auto it = placed.find(offset);
        if (it != placed.end()) {
            drawPlacedAt(v, it->second, blocks, wireframe, mask);
        } else {
            drawSolidBlockFaces(v, GRAY, wireframe, mask);
        }
    }

    void draw(Registry<BlockDef>& blocks, Camera3D& cam, bool wireframe = true) {
        cachedFrustum = extractFrustum(cam);

        if (modelSize > 0) {
            // Model mode: only iterate surface positions (6*N² + removed neighbors)
            std::set<Vec3i> surface;
            collectModelSurface(surface);
            for (auto& offset : surface)
                drawBlockAtOffset(offset, blocks, wireframe);
        } else {
            // Structure mode: iterate placeholders
            for (auto& ph : placeholders) {
                drawBlockAtOffset(ph, blocks, wireframe);
            }
            // Draw placed blocks outside the placeholder grid
            for (auto& [offset, name] : placed) {
                if (placeholderSet.count(offset)) continue;
                drawBlockAtOffset(offset, blocks, wireframe);
            }
        }

        if (hover.hit) {
            drawFaceOutline(hover.pos, hover.normal);
        }
    }
};
