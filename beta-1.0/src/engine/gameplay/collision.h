#pragma once
#include <vector>
#include <cmath>
#include <functional>

// ── Portable types (no external dependencies) ──────────────────────────

struct CLVec2 {
    float x, y;
};

struct CLRect {
    float x, y, width, height;
};

// ── Tile types for collision ──────────────────────────────────────────

enum CLTileType {
    CL_TILE_SOLID,
    CL_TILE_WATER,
    CL_TILE_RIVER,
};

// ── Tile info returned by the world query ─────────────────────────────

struct CLTileInfo {
    float groundHeight;   // top of the block (terrain surface)
    CLTileType type;
    float waterSurface;   // top of the water volume (sea level for lakes, terrain height for rivers)
};

// Callback: given tile coords, return info about that tile
using TileInfoFn = std::function<CLTileInfo(int tileX, int tileZ)>;

// ── Constants ─────────────────────────────────────────────────────────────

constexpr float CL_PI             = 3.14159265358979f;
constexpr float GROUND_NONE       = -9999.0f;  // sentinel: no ground found
constexpr float COLLISION_SKIN    = 0.01f;      // gap to prevent edge-sitting
constexpr float GROUNDED_EPSILON  = 0.01f;      // tolerance for ground check

// ── Collision box ──────────────────────────────────────────────────────

struct CollisionBox {
    CLRect rect;          // X/Z footprint
    float minY = 0.0f;    // bottom of the obstacle
    float maxY = 999.0f;  // top of the obstacle (default = full height, always blocks)
    bool solid = true;    // false = doesn't block horizontal movement (water/river)
    CLTileType type = CL_TILE_SOLID;  // what kind of tile this box represents
};

// ── Intersection tests ─────────────────────────────────────────────────

// Check if two line segments (a1->a2) and (b1->b2) intersect
inline bool lineSegmentsIntersect(CLVec2 a1, CLVec2 a2, CLVec2 b1, CLVec2 b2) {
    float d1x = a2.x - a1.x, d1y = a2.y - a1.y;
    float d2x = b2.x - b1.x, d2y = b2.y - b1.y;
    float cross = d1x * d2y - d1y * d2x;
    if (cross == 0.0f) return false;

    float dx = b1.x - a1.x, dy = b1.y - a1.y;
    float t = (dx * d2y - dy * d2x) / cross;
    float u = (dx * d1y - dy * d1x) / cross;

    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

// Check if a line segment (p1 -> p2) intersects with a rectangle
inline bool lineIntersectsRect(CLVec2 p1, CLVec2 p2, CLRect rect) {
    float left   = rect.x;
    float right  = rect.x + rect.width;
    float top    = rect.y;
    float bottom = rect.y + rect.height;

    // Either endpoint inside the rectangle
    if (p1.x >= left && p1.x <= right && p1.y >= top && p1.y <= bottom) return true;
    if (p2.x >= left && p2.x <= right && p2.y >= top && p2.y <= bottom) return true;

    // Line crosses any edge
    CLVec2 tl = {left, top};
    CLVec2 tr = {right, top};
    CLVec2 bl = {left, bottom};
    CLVec2 br = {right, bottom};

    if (lineSegmentsIntersect(p1, p2, tl, tr)) return true;
    if (lineSegmentsIntersect(p1, p2, bl, br)) return true;
    if (lineSegmentsIntersect(p1, p2, tl, bl)) return true;
    if (lineSegmentsIntersect(p1, p2, tr, br)) return true;

    return false;
}

// Check if a point is inside a rectangle
inline bool pointInRect(CLVec2 point, CLRect rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

// Check if two rectangles overlap
inline bool rectsOverlap(CLRect a, CLRect b) {
    return a.x < b.x + b.width  && a.x + a.width  > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}

// ── Movement collision ─────────────────────────────────────────────────

// Check if moving from->to would collide with any box in the list
// moverMinY/moverMaxY: vertical range of the moving entity
inline bool checkMovementCollision(const std::vector<CollisionBox>& boxes,
                                   CLVec2 from, CLVec2 to,
                                   float moverMinY = 0.0f, float moverMaxY = 999.0f) {
    for (auto& box : boxes) {
        if (!box.solid) continue;
        if (moverMaxY <= box.minY || moverMinY >= box.maxY) continue;
        if (lineIntersectsRect(from, to, box.rect)) return true;
    }
    return false;
}

// Returns how far (0.0 to 1.0) along the line from->to before hitting a box.
// 1.0 means no collision (full movement is safe).
// moverSize: the size of the moving box (e.g. player 32x32).
// Expands each collision box by half the mover size so a point sweep is equivalent to box-vs-box.
inline float sweepMovement(const std::vector<CollisionBox>& boxes,
                           CLVec2 from, CLVec2 to, CLVec2 moverSize = {0, 0},
                           float moverMinY = 0.0f, float moverMaxY = 999.0f) {
    float halfW = moverSize.x / 2.0f;
    float halfH = moverSize.y / 2.0f;
    float closest = 1.0f;

    for (auto& box : boxes) {
        // Skip non-solid boxes (water/river) — they don't block horizontal movement
        if (!box.solid) continue;
        // Skip if no vertical overlap
        if (moverMaxY <= box.minY || moverMinY >= box.maxY) continue;
        CLRect r = box.rect;
        float left   = r.x - halfW, right = r.x + r.width + halfW;
        float top    = r.y - halfH, bottom = r.y + r.height + halfH;

        CLVec2 edges[4][2] = {
            {{left, top}, {right, top}},
            {{left, bottom}, {right, bottom}},
            {{left, top}, {left, bottom}},
            {{right, top}, {right, bottom}},
        };

        float d1x = to.x - from.x, d1y = to.y - from.y;
        for (int i = 0; i < 4; i++) {
            float d2x = edges[i][1].x - edges[i][0].x;
            float d2y = edges[i][1].y - edges[i][0].y;
            float cross = d1x * d2y - d1y * d2x;
            if (cross == 0.0f) continue;

            float dx = edges[i][0].x - from.x;
            float dy = edges[i][0].y - from.y;
            float t = (dx * d2y - dy * d2x) / cross;
            float u = (dx * d1y - dy * d1x) / cross;

            if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
                if (t < closest) closest = t;
            }
        }

        // Check if 'from' is already inside the expanded box
        if (from.x >= left && from.x <= right && from.y >= top && from.y <= bottom) {
            closest = 0.0f;
        }
    }

    // Leave a tiny gap so we don't sit exactly on the edge
    if (closest < 1.0f) closest = (closest > COLLISION_SKIN) ? closest - COLLISION_SKIN : 0.0f;
    return closest;
}

// ── Nearby tile registration ──────────────────────────────────────────

// Register collision boxes for tiles around a position into the given vector
// Any entity (player, mob) can call this with their position and a tile query
inline void registerNearbyTiles(std::vector<CollisionBox>& outBoxes,
                                float posX, float posZ, int radius, TileInfoFn getTileInfo) {
    outBoxes.clear();  // retains capacity — no reallocation after warmup

    int tileX = (int)roundf(posX);
    int tileZ = (int)roundf(posZ);

    for (int z = tileZ - radius; z <= tileZ + radius; z++) {
        for (int x = tileX - radius; x <= tileX + radius; x++) {
            CLTileInfo info = getTileInfo(x, z);

            float blockTop = info.groundHeight;
            CLRect rect = { (float)x - 0.5f, (float)z - 0.5f, 1.0f, 1.0f };

            if (info.type == CL_TILE_WATER || info.type == CL_TILE_RIVER) {
                // Solid floor 1 block below the water surface
                float surface = info.waterSurface;
                float floorTop = surface - 1.0f;
                outBoxes.push_back({rect, floorTop - 1.0f, floorTop, true, CL_TILE_SOLID});

                // Non-solid water volume from floor to water surface
                outBoxes.push_back({rect, floorTop, surface, false, info.type});
            } else {
                outBoxes.push_back({rect, blockTop - 1.0f, blockTop, true, CL_TILE_SOLID});
            }
        }
    }
}
