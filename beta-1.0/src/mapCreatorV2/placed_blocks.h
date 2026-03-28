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

// ── Block 3-axis rotation (4 orientations per axis: 0°, 90°, 180°, 270°) ──
// Encoded as ":Ryrxrz" suffix on block name (e.g. ":R100" = Y=1, X=0, Z=0).
// Legacy ":Y1" format is still parsed for backwards compatibility.

struct BlockRot { int y = 0, x = 0, z = 0; };

static std::string stripRotationSuffix(const std::string& name) {
    size_t pos = name.find(":R");
    if (pos != std::string::npos) return name.substr(0, pos);
    pos = name.find(":Y");
    if (pos != std::string::npos) return name.substr(0, pos);
    return name;
}

static BlockRot getRotation(const std::string& name) {
    BlockRot rot;
    size_t pos = name.find(":R");
    if (pos != std::string::npos && pos + 5 <= name.size()) {
        rot.y = name[pos + 2] - '0';
        rot.x = name[pos + 3] - '0';
        rot.z = name[pos + 4] - '0';
        rot.y = (rot.y >= 0 && rot.y <= 3) ? rot.y : 0;
        rot.x = (rot.x >= 0 && rot.x <= 3) ? rot.x : 0;
        rot.z = (rot.z >= 0 && rot.z <= 3) ? rot.z : 0;
        return rot;
    }
    // Legacy ":Y" format
    pos = name.find(":Y");
    if (pos != std::string::npos && pos + 2 < name.size())
        rot.y = std::atoi(name.c_str() + pos + 2) % 4;
    return rot;
}

static int getYRotation(const std::string& name) {
    return getRotation(name).y;
}

static std::string withRotation(const std::string& name, BlockRot rot) {
    std::string base = stripRotationSuffix(name);
    rot.y = ((rot.y % 4) + 4) % 4;
    rot.x = ((rot.x % 4) + 4) % 4;
    rot.z = ((rot.z % 4) + 4) % 4;
    if (rot.y == 0 && rot.x == 0 && rot.z == 0) return base;
    return base + ":R" + std::to_string(rot.y) + std::to_string(rot.x) + std::to_string(rot.z);
}

static std::string withYRotation(const std::string& name, int rot) {
    BlockRot r = getRotation(name);
    r.y = rot;
    return withRotation(name, r);
}

// ── Frustum culling ────────────────────────────────────────
struct Frustum {
    float planes[6][4]; // {a, b, c, d} where ax+by+cz+d >= 0 is inside
};

inline Frustum extractFrustum(Camera3D& cam) {
    Matrix view = GetCameraMatrix(cam);
    Matrix proj = rlGetMatrixProjection();
    Matrix vp = MatrixMultiply(view, proj);
    Frustum f;
    f.planes[0][0] = vp.m3+vp.m0;  f.planes[0][1] = vp.m7+vp.m4;
    f.planes[0][2] = vp.m11+vp.m8; f.planes[0][3] = vp.m15+vp.m12;
    f.planes[1][0] = vp.m3-vp.m0;  f.planes[1][1] = vp.m7-vp.m4;
    f.planes[1][2] = vp.m11-vp.m8; f.planes[1][3] = vp.m15-vp.m12;
    f.planes[2][0] = vp.m3+vp.m1;  f.planes[2][1] = vp.m7+vp.m5;
    f.planes[2][2] = vp.m11+vp.m9; f.planes[2][3] = vp.m15+vp.m13;
    f.planes[3][0] = vp.m3-vp.m1;  f.planes[3][1] = vp.m7-vp.m5;
    f.planes[3][2] = vp.m11-vp.m9; f.planes[3][3] = vp.m15-vp.m13;
    f.planes[4][0] = vp.m3+vp.m2;  f.planes[4][1] = vp.m7+vp.m6;
    f.planes[4][2] = vp.m11+vp.m10;f.planes[4][3] = vp.m15+vp.m14;
    f.planes[5][0] = vp.m3-vp.m2;  f.planes[5][1] = vp.m7-vp.m6;
    f.planes[5][2] = vp.m11-vp.m10;f.planes[5][3] = vp.m15-vp.m14;
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
    const float radius = 0.866f;
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

// ── Chunk-based 3D block storage ───────────────────────────
static const int CHUNK_SZ = 16;

struct Vec2i {
    int x, z;
    bool operator<(const Vec2i& o) const { return x != o.x ? x < o.x : z < o.z; }
    bool operator==(const Vec2i& o) const { return x == o.x && z == o.z; }
};

inline int chunkDiv(int v) { return (v >= 0) ? v / CHUNK_SZ : (v - CHUNK_SZ + 1) / CHUNK_SZ; }
inline int chunkMod(int v) { int r = v % CHUNK_SZ; return r < 0 ? r + CHUNK_SZ : r; }

struct ChunkData {
    int minY = 0, maxY = -1;
    std::vector<std::string> cells;

    int ySpan() const { return maxY >= minY ? maxY - minY + 1 : 0; }
    bool empty() const { return maxY < minY; }

    void ensureY(int y) {
        if (maxY < minY) {
            minY = y; maxY = y;
            cells.resize(CHUNK_SZ * CHUNK_SZ);
            return;
        }
        if (y >= minY && y <= maxY) return;
        int newMin = std::min(minY, y);
        int newMax = std::max(maxY, y);
        int newSpan = newMax - newMin + 1;
        int oldSpan = ySpan();
        int yOff = minY - newMin;
        std::vector<std::string> newCells(CHUNK_SZ * CHUNK_SZ * newSpan);
        for (int lx = 0; lx < CHUNK_SZ; lx++)
            for (int lz = 0; lz < CHUNK_SZ; lz++)
                for (int yi = 0; yi < oldSpan; yi++) {
                    auto& val = cells[(lx * CHUNK_SZ + lz) * oldSpan + yi];
                    if (!val.empty())
                        newCells[(lx * CHUNK_SZ + lz) * newSpan + yi + yOff] = std::move(val);
                }
        cells = std::move(newCells);
        minY = newMin;
        maxY = newMax;
    }

    int index(int lx, int lz, int y) const {
        return (lx * CHUNK_SZ + lz) * ySpan() + (y - minY);
    }

    const std::string& get(int lx, int lz, int y) const {
        static const std::string emptyStr;
        if (maxY < minY || y < minY || y > maxY) return emptyStr;
        return cells[index(lx, lz, y)];
    }

    bool has(int lx, int lz, int y) const {
        return !get(lx, lz, y).empty();
    }

    void set(int lx, int lz, int y, const std::string& name) {
        ensureY(y);
        cells[index(lx, lz, y)] = name;
    }

    void erase(int lx, int lz, int y) {
        if (maxY < minY || y < minY || y > maxY) return;
        cells[index(lx, lz, y)].clear();
    }
};

struct ChunkMatrix {
    std::map<Vec2i, ChunkData> chunks;

    static Vec2i chunkKey(int x, int z) { return { chunkDiv(x), chunkDiv(z) }; }
    static void toLocal(int x, int z, int& lx, int& lz) { lx = chunkMod(x); lz = chunkMod(z); }

    ChunkData& chunk(int x, int z) { return chunks[chunkKey(x, z)]; }
    const ChunkData* chunkAt(int x, int z) const {
        auto it = chunks.find(chunkKey(x, z));
        return it != chunks.end() ? &it->second : nullptr;
    }

    void set(int x, int y, int z, const std::string& name) {
        int lx, lz; toLocal(x, z, lx, lz);
        chunk(x, z).set(lx, lz, y, name);
    }

    const std::string& get(int x, int y, int z) const {
        static const std::string emptyStr;
        int lx, lz; toLocal(x, z, lx, lz);
        auto* c = chunkAt(x, z);
        return c ? c->get(lx, lz, y) : emptyStr;
    }

    bool has(int x, int y, int z) const { return !get(x, y, z).empty(); }

    void erase(int x, int y, int z) {
        int lx, lz; toLocal(x, z, lx, lz);
        auto it = chunks.find(chunkKey(x, z));
        if (it != chunks.end()) it->second.erase(lx, lz, y);
    }

    void clear() { chunks.clear(); }

    template<typename Fn>
    void forEach(Fn&& fn) const {
        for (auto& [key, chk] : chunks) {
            if (chk.empty()) continue;
            int baseX = key.x * CHUNK_SZ;
            int baseZ = key.z * CHUNK_SZ;
            int span = chk.ySpan();
            for (int lx = 0; lx < CHUNK_SZ; lx++)
                for (int lz = 0; lz < CHUNK_SZ; lz++)
                    for (int yi = 0; yi < span; yi++) {
                        const auto& name = chk.cells[(lx * CHUNK_SZ + lz) * span + yi];
                        if (!name.empty())
                            fn(baseX + lx, chk.minY + yi, baseZ + lz, name);
                    }
        }
    }

    int count() const {
        int n = 0;
        for (auto& [k, c] : chunks)
            for (auto& s : c.cells) if (!s.empty()) n++;
        return n;
    }
};

// ── PlacedBlocks: data + spatial queries ──────────────────
// Responsible for block storage, coordinate transforms, raycasting.
// Rendering is handled separately by BlockRenderer (below).
struct PlacedBlocks {
    std::vector<Vec3i> placeholders = { {0, 0, 0} };
    std::set<Vec3i> placeholderSet = { {0, 0, 0} };
    ChunkMatrix matrix;

    // Model mode (3D Block: cubic)
    int modelSize = 0;
    std::set<Vec3i> removed;

    // Model mode (3D Model: independent XYZ dimensions)
    Vec3i modelDims = {0, 0, 0};
    Vector3 modelOriginPos = {0.0f, 0.0f, 0.0f}; // precise origin in offset space

    // Hover state (set by input handling)
    struct HitResult { bool hit; Vec3i pos; Vector3 normal; float dist; };
    HitResult hover = { false, {0,0,0}, {0,0,0}, 0.0f };
    Frustum cachedFrustum = {};

    // Selected block (set when origin is clicked on a block, cleared on camera move)
    bool hasSelectedBlock = false;
    Vec3i selectedBlock = {0, 0, 0};

    // ── Shared constants ────────────────────────────────────
    static inline const Vec3i DIRS6[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

    // ── Position cache ──────────────────────────────────────
    bool positionsDirty = true;
    std::vector<Vec3i> cachedPositions;

    void markDirty() { positionsDirty = true; }

    // ── Coordinate helpers ──────────────────────────────────

    void rebuildPlaceholderSet() {
        placeholderSet.clear();
        placeholderSet.insert(placeholders.begin(), placeholders.end());
    }

    Vec3i getOrigin() const {
        return placeholders.empty() ? Vec3i{0, 0, 0} : placeholders[0];
    }

    bool isInModelBounds(Vec3i offset) const {
        if (modelDims.x > 0) {
            int halfX = modelDims.x / 2, halfZ = modelDims.z / 2;
            return offset.x >= -halfX && offset.x < -halfX + modelDims.x &&
                   offset.y >= 0 && offset.y < modelDims.y &&
                   offset.z >= -halfZ && offset.z < -halfZ + modelDims.z;
        }
        int half = modelSize / 2;
        int minX = -half, maxX = -half + modelSize - 1;
        int minY = 0,     maxY = modelSize - 1;
        int minZ = -half, maxZ = -half + modelSize - 1;
        return offset.x >= minX && offset.x <= maxX &&
               offset.y >= minY && offset.y <= maxY &&
               offset.z >= minZ && offset.z <= maxZ;
    }

    bool isModelMode() const { return modelSize > 0 || modelDims.x > 0; }

    Vec3i toWorld(Vec3i offset) const {
        Vec3i o = getOrigin();
        return { o.x + offset.x, o.y + offset.y, o.z + offset.z };
    }

    Vec3i toOffset(Vec3i world) const {
        Vec3i o = getOrigin();
        return { world.x - o.x, world.y - o.y, world.z - o.z };
    }

    // ── Block queries ───────────────────────────────────────

    bool hasBlockAt(Vec3i offset) const {
        if (isModelMode())
            return isInModelBounds(offset) && !removed.count(offset);
        return matrix.has(offset.x, offset.y, offset.z) || placeholderSet.count(offset);
    }

    const std::string& getBlockName(Vec3i offset) const {
        return matrix.get(offset.x, offset.y, offset.z);
    }

    bool hasPlacedBlock(Vec3i offset) const {
        return matrix.has(offset.x, offset.y, offset.z);
    }

    void setBlock(Vec3i offset, const std::string& name) {
        matrix.set(offset.x, offset.y, offset.z, name);
        positionsDirty = true;
    }

    void eraseBlock(Vec3i offset) {
        matrix.erase(offset.x, offset.y, offset.z);
        positionsDirty = true;
    }

    // ── Surface / position queries ──────────────────────────

    void getModelBoundsMinMax(int& minX, int& maxX, int& minY, int& maxY, int& minZ, int& maxZ) const {
        if (modelDims.x > 0) {
            int halfX = modelDims.x / 2, halfZ = modelDims.z / 2;
            minX = -halfX; maxX = -halfX + modelDims.x - 1;
            minY = 0;      maxY = modelDims.y - 1;
            minZ = -halfZ; maxZ = -halfZ + modelDims.z - 1;
        } else {
            int half = modelSize / 2;
            minX = -half; maxX = -half + modelSize - 1;
            minY = 0;     maxY = modelSize - 1;
            minZ = -half; maxZ = -half + modelSize - 1;
        }
    }

    void collectModelSurface(std::set<Vec3i>& out) const {
        int minX, maxX, minY, maxY, minZ, maxZ;
        getModelBoundsMinMax(minX, maxX, minY, maxY, minZ, maxZ);

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

        for (auto& r : removed) {
            for (int i = 0; i < 6; i++) {
                Vec3i nb = {r.x + DIRS6[i].x, r.y + DIRS6[i].y, r.z + DIRS6[i].z};
                if (isInModelBounds(nb) && !removed.count(nb))
                    out.insert(nb);
            }
        }
    }

    const std::vector<Vec3i>& allWorldPositions() {
        if (!positionsDirty) return cachedPositions;
        cachedPositions.clear();

        if (isModelMode()) {
            std::set<Vec3i> surface;
            collectModelSurface(surface);
            cachedPositions.reserve(surface.size());
            for (auto& offset : surface)
                cachedPositions.push_back(toWorld(offset));
        } else {
            for (auto& p : placeholders) {
                bool surrounded = true;
                for (int i = 0; i < 6; i++) {
                    Vec3i nb = {p.x+DIRS6[i].x, p.y+DIRS6[i].y, p.z+DIRS6[i].z};
                    if (!hasBlockAt(nb)) { surrounded = false; break; }
                }
                if (surrounded && !matrix.has(p.x, p.y, p.z)) continue;
                cachedPositions.push_back(toWorld(p));
            }
            matrix.forEach([&](int x, int y, int z, const std::string&) {
                Vec3i offset = {x, y, z};
                if (!placeholderSet.count(offset))
                    cachedPositions.push_back(toWorld(offset));
            });
        }
        positionsDirty = false;
        return cachedPositions;
    }

    int getExposedMask(Vec3i offset) const {
        int mask = 0;
        for (int i = 0; i < 6; i++) {
            Vec3i neighbor = { offset.x + DIRS6[i].x, offset.y + DIRS6[i].y, offset.z + DIRS6[i].z };
            if (!hasBlockAt(neighbor)) mask |= (1 << i);
        }
        return mask;
    }

    // ── Raycasting ──────────────────────────────────────────

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

    // ── Normal / face helpers ───────────────────────────────

    static Vec3i normalToDir(Vector3 n) {
        if (fabsf(n.x) >= fabsf(n.y) && fabsf(n.x) >= fabsf(n.z))
            return { (n.x > 0) ? 1 : -1, 0, 0 };
        if (fabsf(n.y) >= fabsf(n.x) && fabsf(n.y) >= fabsf(n.z))
            return { 0, (n.y > 0) ? 1 : -1, 0 };
        return { 0, 0, (n.z > 0) ? 1 : -1 };
    }

    static int faceFromNormal(Vector3 n) {
        float ax = fabsf(n.x), ay = fabsf(n.y), az = fabsf(n.z);
        if (ax >= ay && ax >= az) return (n.x > 0) ? 0 : 1;
        if (ay >= ax && ay >= az) return (n.y > 0) ? 2 : 3;
        return (n.z > 0) ? 4 : 5;
    }

    // ── Face color encoding/decoding ────────────────────────

    static void decodeFaceColors(const std::string& str, Color faces[6]) {
        Color grey = GRAY;
        for (int i = 0; i < 6; i++) faces[i] = grey;

        if (str.empty()) return;

        if (str[0] == '#' && str.size() == 7) {
            unsigned int r, g, b;
            sscanf(str.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
            Color c = {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
            for (int i = 0; i < 6; i++) faces[i] = c;
            return;
        }

        if (str[0] != 'f' || str.size() < 2) return;
        int fi = 0;
        size_t pos = 2;
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

    // ── Drag state (structure mode) ─────────────────────────
    bool structDragActive = false;
    bool structDragMoved = false;
    Vector2 structDragStart = {0, 0};
    Vector2 structLastDragMouse = {0, 0};
    int structDragFace = -1;
    struct StructDragHit { Vec3i offset; int face; };
    std::vector<StructDragHit> structDragHighlights;

    // ── Input handling (structure mode) ─────────────────────

    void handleInput(Camera3D& cam, BlockHotbar& hotbar, bool invOpen, EditorCamera& editorCam) {
        if (invOpen) return;

        handleOriginClick(cam, editorCam);
        handleStructDrag(cam, hotbar, editorCam);
        handleBlockRotate();
    }

    void handleBlockRotate() {
        if (!hover.hit) return;
        bool rPressed = IsKeyPressed(KEY_R);
        bool tPressed = IsKeyPressed(KEY_T);
        if (!rPressed && !tPressed) return;

        Vec3i offset = toOffset(hover.pos);
        const std::string& name = matrix.get(offset.x, offset.y, offset.z);
        if (name.empty()) return;
        // Only rotate non-color blocks (GLB models and named blocks)
        if (name[0] == '#' || name[0] == 'f') return;

        BlockRot rot = getRotation(name);
        if (rPressed) rot.y = (rot.y + 1) % 4;  // R: rotate Y axis (spin left/right)
        if (tPressed) rot.x = (rot.x + 1) % 4;  // T: rotate X axis (tilt forward/back)
        matrix.set(offset.x, offset.y, offset.z, withRotation(name, rot));
        positionsDirty = true;
    }

    void handleStructDrag(Camera3D& cam, BlockHotbar& hotbar, EditorCamera& editorCam) {
        // Start drag on right-click
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            structDragStart = GetMousePosition();
            structLastDragMouse = structDragStart;
            structDragActive = true;
            structDragMoved = false;
            structDragFace = hover.hit ? faceFromNormal(hover.normal) : -1;
        }

        // Update highlights while dragging
        if (structDragActive && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 cur = GetMousePosition();
            float dx = cur.x - structDragStart.x;
            float dy = cur.y - structDragStart.y;
            if (sqrtf(dx*dx + dy*dy) > 5.0f) structDragMoved = true;

            float mdx = cur.x - structLastDragMouse.x;
            float mdy = cur.y - structLastDragMouse.y;
            if (sqrtf(mdx*mdx + mdy*mdy) < 2.0f) goto skipHighlights;
            structLastDragMouse = cur;

            structDragHighlights.clear();
            {
                bool xHeld = IsKeyDown(KEY_X);
                bool hasBlock = !hotbar.getSelectedName().empty();
                if (structDragMoved && structDragFace >= 0 && (hasBlock || xHeld)) {
                    float minSX = fminf(structDragStart.x, cur.x);
                    float maxSX = fmaxf(structDragStart.x, cur.x);
                    float minSY = fminf(structDragStart.y, cur.y);
                    float maxSY = fmaxf(structDragStart.y, cur.y);

                    Vec3i faceDir = DIRS6[structDragFace];
                    Vector3 camPos = editorCam.cam.position;
                    Vector3 camTarget = editorCam.cam.target;
                    Vector3 camFwd = {camTarget.x-camPos.x, camTarget.y-camPos.y, camTarget.z-camPos.z};
                    float fwdLen = sqrtf(camFwd.x*camFwd.x + camFwd.y*camFwd.y + camFwd.z*camFwd.z);
                    if (fwdLen > 0) { camFwd.x/=fwdLen; camFwd.y/=fwdLen; camFwd.z/=fwdLen; }

                    const auto& positions = allWorldPositions();
                    for (auto& w : positions) {
                        Vec3i ph = toOffset(w);
                        Vec3i neighbor = {ph.x+faceDir.x, ph.y+faceDir.y, ph.z+faceDir.z};
                        if (hasBlockAt(neighbor)) continue;

                        float bx = (float)w.x, by = (float)w.y, bz = (float)w.z;
                        Vector3 toBlock = {bx-camPos.x, by-camPos.y, bz-camPos.z};
                        if (toBlock.x*camFwd.x + toBlock.y*camFwd.y + toBlock.z*camFwd.z < 0.0f) continue;

                        float sMinX2 = 99999, sMaxX2 = -99999, sMinY2 = 99999, sMaxY2 = -99999;
                        for (int ci = 0; ci < 8; ci++) {
                            Vector3 corner = {
                                bx + ((ci & 1) ? 0.5f : -0.5f),
                                by + ((ci & 2) ? 0.5f : -0.5f),
                                bz + ((ci & 4) ? 0.5f : -0.5f)
                            };
                            Vector2 sp = GetWorldToScreen(corner, editorCam.cam);
                            if (sp.x < sMinX2) sMinX2 = sp.x;
                            if (sp.x > sMaxX2) sMaxX2 = sp.x;
                            if (sp.y < sMinY2) sMinY2 = sp.y;
                            if (sp.y > sMaxY2) sMaxY2 = sp.y;
                        }

                        if (sMaxX2 >= minSX && sMinX2 <= maxSX && sMaxY2 >= minSY && sMinY2 <= maxSY) {
                            structDragHighlights.push_back({ph, structDragFace});
                        }
                    }
                }
            }
            skipHighlights:;
        }

        // Release: apply action
        if (structDragActive && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            bool xHeld = IsKeyDown(KEY_X);
            if (structDragMoved && xHeld) {
                // Drag remove
                for (auto& dh : structDragHighlights) {
                    if (matrix.has(dh.offset.x, dh.offset.y, dh.offset.z)) {
                        matrix.erase(dh.offset.x, dh.offset.y, dh.offset.z);
                    }
                }
                positionsDirty = true;
                structDragHighlights.clear();
            } else if (structDragMoved && !hotbar.getSelectedName().empty()) {
                // Drag place: place blocks adjacent to highlighted surface
                const std::string& blockName = hotbar.getSelectedName();
                Vec3i faceDir = DIRS6[structDragFace];
                for (auto& dh : structDragHighlights) {
                    Vec3i target = {dh.offset.x + faceDir.x, dh.offset.y + faceDir.y, dh.offset.z + faceDir.z};
                    if (!matrix.has(target.x, target.y, target.z) && !placeholderSet.count(target)) {
                        matrix.set(target.x, target.y, target.z, blockName);
                    } else if (placeholderSet.count(target) && !matrix.has(target.x, target.y, target.z)) {
                        matrix.set(target.x, target.y, target.z, blockName);
                    }
                }
                positionsDirty = true;
                structDragHighlights.clear();
            } else if (!structDragMoved) {
                // Single click: place or remove
                if (xHeld) {
                    handleBlockRemove(cam);
                } else {
                    handleBlockPlace(cam, hotbar);
                }
            }
            structDragActive = false;
            structDragHighlights.clear();
        }

        if (!structDragActive && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            structDragHighlights.clear();
        }
    }

    // Renamed from update() for clarity — called by StructureEditor
    void update(Camera3D& cam, BlockHotbar& hotbar, bool invOpen, EditorCamera& editorCam) {
        // Deselect if camera moves via keys
        if (hasSelectedBlock) {
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D) ||
                IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_LEFT_SHIFT)) {
                hasSelectedBlock = false;
            }
        }

        currentOrigin = editorCam.origin;
        Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
        hover = raycastBlocks(ray);
        handleInput(cam, hotbar, invOpen, editorCam);
    }

    // ── Rendering ───────────────────────────────────────────

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

    void drawFaceColoredBlock(Vector3 pos, Color faces[6], bool wireframe, int exposedMask = 0x3F) {
        float x = pos.x, y = pos.y, z = pos.z;
        float w = 0.5f, h = 0.5f, l = 0.5f;

        int faceCount = 0;
        for (int i = 0; i < 6; i++) if (exposedMask & (1 << i)) faceCount++;
        if (faceCount == 0) return;

        rlCheckRenderBatchLimit(faceCount * 6);
        rlBegin(RL_TRIANGLES);

        if (exposedMask & (1 << 0)) {
            Color c = faces[0]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x+w,y-h,z-l); rlVertex3f(x+w,y+h,z-l); rlVertex3f(x+w,y+h,z+l);
            rlVertex3f(x+w,y-h,z+l); rlVertex3f(x+w,y-h,z-l); rlVertex3f(x+w,y+h,z+l);
        }
        if (exposedMask & (1 << 1)) {
            Color c = faces[1]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x-w,y+h,z-l);
            rlVertex3f(x-w,y-h,z-l); rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y+h,z-l);
        }
        if (exposedMask & (1 << 2)) {
            Color c = faces[2]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y+h,z-l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x+w,y+h,z+l);
            rlVertex3f(x+w,y+h,z-l); rlVertex3f(x-w,y+h,z-l); rlVertex3f(x+w,y+h,z+l);
        }
        if (exposedMask & (1 << 3)) {
            Color c = faces[3]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x-w,y-h,z-l); rlVertex3f(x+w,y-h,z-l);
            rlVertex3f(x+w,y-h,z+l); rlVertex3f(x-w,y-h,z+l); rlVertex3f(x+w,y-h,z-l);
        }
        if (exposedMask & (1 << 4)) {
            Color c = faces[4]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x-w,y-h,z+l); rlVertex3f(x+w,y-h,z+l); rlVertex3f(x-w,y+h,z+l);
            rlVertex3f(x+w,y+h,z+l); rlVertex3f(x-w,y+h,z+l); rlVertex3f(x+w,y-h,z+l);
        }
        if (exposedMask & (1 << 5)) {
            Color c = faces[5]; rlColor4ub(c.r, c.g, c.b, 255);
            rlVertex3f(x+w,y-h,z-l); rlVertex3f(x-w,y-h,z-l); rlVertex3f(x+w,y+h,z-l);
            rlVertex3f(x-w,y+h,z-l); rlVertex3f(x+w,y+h,z-l); rlVertex3f(x-w,y-h,z-l);
        }

        rlEnd();

        if (wireframe) {
            drawWireframeEdges(pos, exposedMask);
        }
    }

    void draw(Registry<BlockDef>& blocks, Camera3D& cam, bool wireframe = true) {
        cachedFrustum = extractFrustum(cam);

        if (isModelMode()) {
            std::set<Vec3i> surface;
            collectModelSurface(surface);
            for (auto& offset : surface)
                drawBlockAtOffset(offset, blocks, wireframe);
        } else {
            for (auto& ph : placeholders)
                drawBlockAtOffset(ph, blocks, wireframe);
            matrix.forEach([&](int x, int y, int z, const std::string&) {
                Vec3i offset = {x, y, z};
                if (!placeholderSet.count(offset))
                    drawBlockAtOffset(offset, blocks, wireframe);
            });
        }

        if (hover.hit) {
            drawFaceOutline(hover.pos, hover.normal);
        }

        // Draw drag highlights (structure mode)
        if (!structDragHighlights.empty()) {
            bool xHeld = IsKeyDown(KEY_X);
            Color hlColor = xHeld ? Color{255, 60, 60, 100} : Color{60, 255, 60, 100};
            for (auto& dh : structDragHighlights) {
                Vec3i w = toWorld(dh.offset);
                Vector3 v = {(float)w.x, (float)w.y, (float)w.z};
                DrawCubeWires(v, 1.02f, 1.02f, 1.02f, hlColor);
            }
        }
    }

    // Draw the 2D drag rectangle overlay (call after EndMode3D)
    void drawStructDragRect() {
        if (structDragActive && structDragMoved && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 cur = GetMousePosition();
            float rx = fminf(structDragStart.x, cur.x);
            float ry = fminf(structDragStart.y, cur.y);
            float rw = fabsf(cur.x - structDragStart.x);
            float rh = fabsf(cur.y - structDragStart.y);
            bool xHeld = IsKeyDown(KEY_X);
            Color fillCol = xHeld ? Color{255,60,60,30} : Color{255,255,255,30};
            Color lineCol = xHeld ? RED : WHITE;
            DrawRectangle((int)rx, (int)ry, (int)rw, (int)rh, fillCol);
            DrawRectangleLines((int)rx, (int)ry, (int)rw, (int)rh, lineCol);
        }
    }

private:
    Vector3 currentOrigin = { 0.0f, 0.0f, 0.0f };

    // ── Input sub-methods ───────────────────────────────────

    void handleOriginClick(Camera3D& cam, EditorCamera& editorCam) {
        static float lmbHoldTime = 0.0f;
        static bool lmbWasDown = false;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            lmbHoldTime += GetFrameTime();
            lmbWasDown = true;
            // Deselect when dragging (camera is moving)
            if (lmbHoldTime > 0.2f && hasSelectedBlock) {
                hasSelectedBlock = false;
            }
        } else if (lmbWasDown) {
            if (lmbHoldTime < 0.2f) {
                Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
                HitResult hit = raycastBlocks(ray);
                if (hit.hit) {
                    editorCam.setOrigin((float)hit.pos.x, (float)hit.pos.y, (float)hit.pos.z);
                    selectedBlock = hit.pos;
                    hasSelectedBlock = true;
                }
            }
            lmbHoldTime = 0.0f;
            lmbWasDown = false;
        }
    }

    void handleBlockPlace(Camera3D& cam, BlockHotbar& hotbar) {
        if (!IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) return;
        Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
        HitResult hit = raycastBlocks(ray);
        if (!hit.hit) return;

        const std::string& blockName = hotbar.getSelectedName();
        if (blockName.empty()) return;

        Vec3i hitOffset = toOffset(hit.pos);
        bool isEmptyPlaceholder = placeholderSet.count(hitOffset)
            && !matrix.has(hitOffset.x, hitOffset.y, hitOffset.z);

        if (isEmptyPlaceholder) {
            matrix.set(hitOffset.x, hitOffset.y, hitOffset.z, blockName);
            positionsDirty = true;
        } else {
            Vec3i dir = normalToDir(hit.normal);
            Vec3i newWorld = { hit.pos.x + dir.x, hit.pos.y + dir.y, hit.pos.z + dir.z };
            Vec3i off = toOffset(newWorld);
            matrix.set(off.x, off.y, off.z, blockName);
            positionsDirty = true;
        }
    }

    void handleBlockRemove(Camera3D& cam) {
        if (!IsKeyPressed(KEY_X)) return;
        Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
        HitResult hit = raycastBlocks(ray);
        if (hit.hit) {
            Vec3i offset = toOffset(hit.pos);
            matrix.erase(offset.x, offset.y, offset.z);
            positionsDirty = true;
        }
    }

    // ── Render sub-methods ──────────────────────────────────

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
            std::string baseName = stripRotationSuffix(name);
            BlockRot rot = getRotation(name);
            auto bIt = blocks.entries.find(baseName);
            if (bIt != blocks.entries.end()) {
                BlockDef& def = bIt->second;
                if (def.formatType == "glb" && def.modelLoaded) {
                    Vector3 modelPos = { v.x, v.y - 0.5f + def.modelOffsetY, v.z };
                    Vector3 sc = { def.modelScaleX, def.modelScaleY, def.modelScaleZ };
                    if (rot.x == 0 && rot.z == 0) {
                        // Y-only rotation: use simple DrawModelEx
                        float angle = rot.y * 90.0f;
                        modelPos.y = v.y - 0.5f;
                        DrawModelEx(def.model, modelPos,
                            { 0, 1, 0 }, angle, sc, WHITE);
                    } else {
                        // Multi-axis rotation: use rlgl transforms
                        rlPushMatrix();
                        rlTranslatef(v.x, v.y, v.z);
                        rlRotatef(rot.y * 90.0f, 0, 1, 0);
                        rlRotatef(rot.x * 90.0f, 1, 0, 0);
                        rlRotatef(rot.z * 90.0f, 0, 0, 1);
                        rlTranslatef(0, -0.5f, 0);
                        DrawModelEx(def.model, {0, 0, 0},
                            {0, 1, 0}, 0.0f, sc, WHITE);
                        rlPopMatrix();
                    }
                } else {
                    Color col = { def.r, def.g, def.b, def.a };
                    drawSolidBlockFaces(v, col, wireframe, exposedMask);
                }
            }
        }
    }

    void drawBlockAtOffset(Vec3i offset, Registry<BlockDef>& blocks, bool wireframe) {
        int mask = getExposedMask(offset);
        if (mask == 0) return;
        Vec3i w = toWorld(offset);
        float fx = (float)w.x, fy = (float)w.y, fz = (float)w.z;
        if (!isInFrustum(cachedFrustum, fx, fy, fz)) return;
        Vector3 v = { fx, fy, fz };
        const std::string& blockName = matrix.get(offset.x, offset.y, offset.z);
        if (!blockName.empty()) {
            drawPlacedAt(v, blockName, blocks, wireframe, mask);
        } else {
            drawSolidBlockFaces(v, GRAY, wireframe, mask);
        }
    }

    void drawWireframeEdges(Vector3 pos, int exposedMask) {
        float x = pos.x, y = pos.y, z = pos.z;
        float w = 0.5f, h = 0.5f, l = 0.5f;
        float e = 0.501f;
        for (int i = 0; i < 6; i++) {
            if (!(exposedMask & (1 << i))) continue;
            Vector3 corners[4];
            if (i == 0) {
                corners[0]={x+e,y-h,z-l}; corners[1]={x+e,y+h,z-l};
                corners[2]={x+e,y+h,z+l}; corners[3]={x+e,y-h,z+l};
            } else if (i == 1) {
                corners[0]={x-e,y-h,z-l}; corners[1]={x-e,y+h,z-l};
                corners[2]={x-e,y+h,z+l}; corners[3]={x-e,y-h,z+l};
            } else if (i == 2) {
                corners[0]={x-w,y+e,z-l}; corners[1]={x+w,y+e,z-l};
                corners[2]={x+w,y+e,z+l}; corners[3]={x-w,y+e,z+l};
            } else if (i == 3) {
                corners[0]={x-w,y-e,z-l}; corners[1]={x+w,y-e,z-l};
                corners[2]={x+w,y-e,z+l}; corners[3]={x-w,y-e,z+l};
            } else if (i == 4) {
                corners[0]={x-w,y-h,z+e}; corners[1]={x+w,y-h,z+e};
                corners[2]={x+w,y+h,z+e}; corners[3]={x-w,y+h,z+e};
            } else {
                corners[0]={x-w,y-h,z-e}; corners[1]={x+w,y-h,z-e};
                corners[2]={x+w,y+h,z-e}; corners[3]={x-w,y+h,z-e};
            }
            DrawLine3D(corners[0],corners[1],DARKGRAY);
            DrawLine3D(corners[1],corners[2],DARKGRAY);
            DrawLine3D(corners[2],corners[3],DARKGRAY);
            DrawLine3D(corners[3],corners[0],DARKGRAY);
        }
    }
};
