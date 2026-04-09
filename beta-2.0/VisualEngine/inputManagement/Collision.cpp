#include "Collision.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <unordered_map>

static glm::vec3 rotatePoint(const glm::vec3& v, float rx, float ry, float rz) {
    if (rx == 0.0f && ry == 0.0f && rz == 0.0f)
        return v;
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(rx), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(ry), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rz), glm::vec3(0, 0, 1));
    return glm::vec3(m * glm::vec4(v, 1.0f));
}

// ── Spatial hash for O(1) grid lookups ──────────────────────────────

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 10) ^ (std::hash<int>()(v.z) << 20);
    }
};

static std::unordered_map<glm::ivec3, size_t, IVec3Hash> gSpatialHash; // grid pos → index into gColliders
static std::vector<BlockCollider> gColliders;
static bool gForceRectangular = false;

void setForceRectangularRaycast(bool force) {
    gForceRectangular = force;
}

bool isForceRectangularRaycast() {
    return gForceRectangular;
}

static glm::ivec3 toGrid(float x, float y, float z) {
    return glm::ivec3((int)roundf(x), (int)roundf(y), (int)roundf(z));
}

// ── Rectangular detection ───────────────────────────────────────────

bool isMeshRectangular(const float* vertices, int vertexCount) {
    // Must have exactly 24 vertices (4 per face x 6 faces) for the cube build path
    if (vertexCount != 24) return false;

    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    for (int i = 0; i < vertexCount; i++) {
        float x = vertices[i * 5 + 0], y = vertices[i * 5 + 1], z = vertices[i * 5 + 2];
        mn = glm::min(mn, glm::vec3(x, y, z));
        mx = glm::max(mx, glm::vec3(x, y, z));
    }

    const float eps = 0.01f;
    for (int i = 0; i < vertexCount; i++) {
        float x = vertices[i * 5 + 0], y = vertices[i * 5 + 1], z = vertices[i * 5 + 2];
        bool onFace =
            std::fabs(x - mn.x) < eps || std::fabs(x - mx.x) < eps ||
            std::fabs(y - mn.y) < eps || std::fabs(y - mx.y) < eps ||
            std::fabs(z - mn.z) < eps || std::fabs(z - mx.z) < eps;
        if (!onFace) return false;
    }

    return true;
}

// ── Building colliders ──────────────────────────────────────────────

void addCollider(const char* meshName, const float* vertices, int vertexCount,
                 const unsigned int* indices, int indexCount,
                 bool rectangular, float x, float y, float z, int floatsPerVertex,
                 float rx, float ry, float rz) {
    int fpv = floatsPerVertex;
    bool hasRot = (rx != 0.0f || ry != 0.0f || rz != 0.0f);
    BlockCollider col;
    col.position = glm::vec3(x, y, z);
    col.rotation = glm::vec3(rx, ry, rz);
    col.meshName = meshName;
    col.isRectangular = rectangular && !hasRot; // rotated boxes use triangle collision

    col.bounds.min = glm::vec3(std::numeric_limits<float>::max());
    col.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (int i = 0; i < vertexCount; i++) {
        glm::vec3 local(vertices[i * fpv + 0], vertices[i * fpv + 1], vertices[i * fpv + 2]);
        glm::vec3 world = rotatePoint(local, rx, ry, rz) + glm::vec3(x, y, z);
        col.bounds.min = glm::min(col.bounds.min, world);
        col.bounds.max = glm::max(col.bounds.max, world);
    }

    if (!col.isRectangular) {
        for (int i = 0; i < indexCount; i += 3) {
            unsigned int i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
            glm::vec3 v0(vertices[i0 * fpv + 0], vertices[i0 * fpv + 1], vertices[i0 * fpv + 2]);
            glm::vec3 v1(vertices[i1 * fpv + 0], vertices[i1 * fpv + 1], vertices[i1 * fpv + 2]);
            glm::vec3 v2(vertices[i2 * fpv + 0], vertices[i2 * fpv + 1], vertices[i2 * fpv + 2]);
            Triangle tri;
            tri.v0 = rotatePoint(v0, rx, ry, rz) + glm::vec3(x, y, z);
            tri.v1 = rotatePoint(v1, rx, ry, rz) + glm::vec3(x, y, z);
            tri.v2 = rotatePoint(v2, rx, ry, rz) + glm::vec3(x, y, z);
            col.triangles.push_back(tri);
        }
    }

    // Initialize triColors: 6 for rectangular (per face), triCount for non-rectangular
    int triCount = col.isRectangular ? 6 : (indexCount / 3);
    col.triColors.assign(triCount, -1);

    size_t idx = gColliders.size();
    gColliders.push_back(std::move(col));
    gSpatialHash[toGrid(x, y, z)] = idx;
}

void removeCollider(float x, float y, float z) {
    glm::ivec3 key = toGrid(x, y, z);
    auto it = gSpatialHash.find(key);
    if (it == gSpatialHash.end()) return;

    size_t idx = it->second;
    gSpatialHash.erase(it);

    // Swap with last to avoid shifting
    if (idx < gColliders.size() - 1) {
        glm::ivec3 lastKey = toGrid(gColliders.back().position.x,
                                     gColliders.back().position.y,
                                     gColliders.back().position.z);
        gColliders[idx] = std::move(gColliders.back());
        gSpatialHash[lastKey] = idx;
    }
    gColliders.pop_back();
}

void clearColliders() {
    gColliders.clear();
    gSpatialHash.clear();
}

bool hasColliderAt(int x, int y, int z) {
    return gSpatialHash.count(glm::ivec3(x, y, z)) > 0;
}

const BlockCollider* getColliderAt(int x, int y, int z) {
    auto it = gSpatialHash.find(glm::ivec3(x, y, z));
    if (it == gSpatialHash.end()) return nullptr;
    return &gColliders[it->second];
}

const std::vector<BlockCollider>& getAllColliders() {
    return gColliders;
}

// ── Ray-triangle intersection (Möller–Trumbore) ────────────────────

static bool rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                        const Triangle& tri, float& outDist, glm::vec3& outNormal) {
    const float EPSILON = 1e-7f;

    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON) return false;

    float f = 1.0f / a;
    glm::vec3 s = origin - tri.v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * glm::dot(edge2, q);
    if (t > EPSILON) {
        outDist = t;
        outNormal = glm::normalize(glm::cross(edge1, edge2));
        return true;
    }

    return false;
}

// ── Ray vs AABB ─────────────────────────────────────────────────────

static bool rayAABB(const glm::vec3& origin, const glm::vec3& invDir, const AABB& box,
                    float maxDist, float& outDist, glm::vec3& outNormal) {
    glm::vec3 t1 = (box.min - origin) * invDir;
    glm::vec3 t2 = (box.max - origin) * invDir;
    glm::vec3 tmin = glm::min(t1, t2);
    glm::vec3 tmax = glm::max(t1, t2);
    float enter = std::max({tmin.x, tmin.y, tmin.z});
    float exit  = std::min({tmax.x, tmax.y, tmax.z});

    if (exit < enter || exit < 0.0f || enter > maxDist) return false;

    outDist = (enter > 0.0f) ? enter : exit;

    if (enter == tmin.x)      outNormal = glm::vec3((invDir.x > 0) ? -1 : 1, 0, 0);
    else if (enter == tmin.y) outNormal = glm::vec3(0, (invDir.y > 0) ? -1 : 1, 0);
    else                      outNormal = glm::vec3(0, 0, (invDir.z > 0) ? -1 : 1);

    return true;
}

// ── Raycast against all colliders ───────────────────────────────────

CollisionHit raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDist) {
    CollisionHit closest{};
    closest.hit = false;
    closest.distance = maxDist;
    closest.triangleIndex = -1;

    glm::vec3 dir = glm::normalize(direction);
    glm::vec3 invDir = 1.0f / dir;

    for (const auto& col : gColliders) {
        if (col.isRectangular || gForceRectangular) {
            float dist;
            glm::vec3 normal;
            if (rayAABB(origin, invDir, col.bounds, closest.distance, dist, normal)) {
                if (dist < closest.distance && dist > 0.0f) {
                    closest.hit = true;
                    closest.distance = dist;
                    closest.point = origin + dir * dist;
                    closest.normal = normal;
                    closest.collider = &col;
                    // Determine which face of the AABB was hit (0-5)
                    if (normal.x > 0.5f)       closest.triangleIndex = 0;  // +X
                    else if (normal.x < -0.5f)  closest.triangleIndex = 1;  // -X
                    else if (normal.y > 0.5f)   closest.triangleIndex = 2;  // +Y
                    else if (normal.y < -0.5f)  closest.triangleIndex = 3;  // -Y
                    else if (normal.z > 0.5f)   closest.triangleIndex = 4;  // +Z
                    else                        closest.triangleIndex = 5;  // -Z
                }
            }
        } else {
            float aabbDist;
            glm::vec3 aabbNormal;
            if (!rayAABB(origin, invDir, col.bounds, closest.distance, aabbDist, aabbNormal))
                continue;

            for (int ti = 0; ti < (int)col.triangles.size(); ti++) {
                float dist;
                glm::vec3 normal;
                if (rayTriangle(origin, dir, col.triangles[ti], dist, normal)) {
                    if (dist < closest.distance && dist > 0.0f) {
                        closest.hit = true;
                        closest.distance = dist;
                        closest.point = origin + dir * dist;
                        closest.normal = normal;
                        closest.collider = &col;
                        closest.triangleIndex = ti;
                    }
                }
            }
        }
    }

    return closest;
}
