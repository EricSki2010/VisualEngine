#pragma once
#include "engine/core/json.h"
#include "engine/core/world.h"
#include "engine/gameplay/collision.h"
#include "engine/gameplay/gravity.h"
#include <string>
#include <unordered_map>
#include <filesystem>

// ── Collision effect ────────────────────────────────────────────────
// Data-driven effects applied when an entity stands in a non-solid
// collision type (water, river, etc.). Loaded from JSON files in
// assets/collisionTypes/.
//
// JSON format:
//   {
//       "name": "water",
//       "speedScale": 0.5,
//       "gravityScale": 0.5,
//       "canSwim": true,
//       "swimUpSpeed": 1.0,
//       "exitJumpForce": 3.5,
//       "entryVelocityScale": 0.3
//   }

struct CollisionEffect {
    std::string name;
    float speedScale        = 1.0f;  // movement speed multiplier
    float gravityScale      = 1.0f;  // gravity multiplier
    bool  canSwim           = false; // hold jump to float up
    float swimUpSpeed       = 1.0f;  // how fast swimming moves you up
    float exitJumpForce     = 0.0f;  // jump force when leaving (while holding jump)
    float entryVelocityScale = 1.0f; // multiply vertical velocity on entry
    bool  useTileFlowDir    = false; // read tile's flowDir and push entity
    float flowForce         = 0.0f;  // push strength in units/sec
};

// ── Collision effect registry ───────────────────────────────────────
// Maps CLTileType -> CollisionEffect. Only non-solid types need entries.

struct CollisionEffectRegistry {
    std::unordered_map<int, CollisionEffect> effects;  // keyed by CLTileType

    void loadFolder(const std::string& folder) {
        if (!std::filesystem::exists(folder)) return;
        int count = 0;
        for (auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".json") {
                loadFile(entry.path().string());
                count++;
            }
        }
        if (count > 0) printf("COLLISION_FX: Loaded %d effect(s) from %s\n", count, folder.c_str());
    }

    void loadFile(const std::string& path) {
        auto obj = json::parseFile(path);
        if (obj.kv.empty()) return;

        CollisionEffect fx;
        fx.name = obj.str("name");
        if (fx.name.empty()) {
            std::filesystem::path p(path);
            fx.name = p.stem().string();
        }

        fx.speedScale         = obj.numf("speedScale", 1.0f);
        fx.gravityScale       = obj.numf("gravityScale", 1.0f);
        fx.canSwim            = obj.boolean("canSwim", false);
        fx.swimUpSpeed        = obj.numf("swimUpSpeed", 1.0f);
        fx.exitJumpForce      = obj.numf("exitJumpForce", 0.0f);
        fx.entryVelocityScale = obj.numf("entryVelocityScale", 1.0f);
        fx.useTileFlowDir     = obj.boolean("useTileFlowDir", false);
        fx.flowForce          = obj.numf("flowForce", 0.0f);

        // Map name to tile type
        CLTileType type = CL_TILE_SOLID;
        if (fx.name == "water")      type = CL_TILE_WATER;
        else if (fx.name == "river") type = CL_TILE_RIVER;
        else {
            printf("COLLISION_FX: Unknown type '%s' in %s\n", fx.name.c_str(), path.c_str());
            return;
        }

        effects[(int)type] = fx;
    }

    const CollisionEffect* get(CLTileType type) const {
        auto it = effects.find((int)type);
        return it != effects.end() ? &it->second : nullptr;
    }
};

// ── Collision effect state ──────────────────────────────────────────
// Per-entity state tracking for effect transitions (entry/exit).

struct CollisionEffectState {
    CLTileType currentType = CL_TILE_SOLID;
    CLTileType previousType = CL_TILE_SOLID;

    bool justEntered(CLTileType type) const { return currentType == type && previousType != type; }
    bool justExited(CLTileType type) const  { return currentType != type && previousType == type; }
    bool inside(CLTileType type) const      { return currentType == type; }
};

// ── Apply collision effects ─────────────────────────────────────────
// Call once per frame after collision registration. Returns the speed
// scale and gravity scale to use for this frame. Handles swimming
// and entry/exit transitions.

struct CollisionEffectResult {
    float speedScale   = 1.0f;
    float gravityScale = 1.0f;
};

// Flow direction offsets: N, E, S, W
static const int CL_FLOW_DX[] = { 0, 1, 0, -1 };
static const int CL_FLOW_DZ[] = { -1, 0, 1, 0 };

inline CollisionEffectResult applyCollisionEffects(
        Entity& entity, CollisionEffectState& state,
        const CollisionEffectRegistry& registry, float dt,
        World* world = nullptr) {

    // Detect what type the entity is standing in
    state.previousType = state.currentType;
    if (entity.isBaseInCollisionType(CL_TILE_WATER))
        state.currentType = CL_TILE_WATER;
    else if (entity.isBaseInCollisionType(CL_TILE_RIVER))
        state.currentType = CL_TILE_RIVER;
    else
        state.currentType = CL_TILE_SOLID;

    // Look up effect for current type
    const CollisionEffect* fx = registry.get(state.currentType);
    if (!fx) return { 1.0f, 1.0f };

    // Entry: dampen vertical velocity
    if (state.justEntered(state.currentType)) {
        entity.gravity.velocityY *= fx->entryVelocityScale;
    }

    // Swimming: hold space to float up
    if (fx->canSwim && IsKeyDown(KEY_SPACE)) {
        entity.position.y += 0.2f * dt * fx->swimUpSpeed * 5.0f;
        entity.gravity.velocityY = 0.0f;
        entity.gravity.grounded = false;
    }

    // Flow force: push entity based on overlap with flow tiles
    // Each tile contributes force proportional to how much of the
    // player's footprint it covers. Multiple tiles stack.
    if (fx->useTileFlowDir && fx->flowForce > 0.0f && world) {
        float hw = entity.halfW();
        float hd = entity.halfD();
        float playerArea = entity.width * entity.depth;
        if (playerArea <= 0.0f) playerArea = 0.01f;

        float px = entity.position.x;
        float pz = entity.position.z;

        // Player footprint bounds
        float pMinX = px - hw, pMaxX = px + hw;
        float pMinZ = pz - hd, pMaxZ = pz + hd;

        // Tiles that could overlap (each tile is 1x1 centered at integer coords)
        int minTX = (int)floorf(pMinX + 0.5f);
        int maxTX = (int)floorf(pMaxX + 0.5f);
        int minTZ = (int)floorf(pMinZ + 0.5f);
        int maxTZ = (int)floorf(pMaxZ + 0.5f);

        float totalFlowX = 0.0f, totalFlowZ = 0.0f;

        for (int tz = minTZ; tz <= maxTZ; tz++) {
            for (int tx = minTX; tx <= maxTX; tx++) {
                auto tile = world->getTile(tx, tz);
                if (!tile || tile->flowDir < 0 || tile->flowDir > 3) continue;

                // Tile bounds
                float tMinX = tx - 0.5f, tMaxX = tx + 0.5f;
                float tMinZ = tz - 0.5f, tMaxZ = tz + 0.5f;

                // Overlap area
                float overlapX = fmaxf(0.0f, fminf(pMaxX, tMaxX) - fmaxf(pMinX, tMinX));
                float overlapZ = fmaxf(0.0f, fminf(pMaxZ, tMaxZ) - fmaxf(pMinZ, tMinZ));
                float fraction = (overlapX * overlapZ) / playerArea;

                if (fraction > 0.0f) {
                    totalFlowX += CL_FLOW_DX[tile->flowDir] * fraction;
                    totalFlowZ += CL_FLOW_DZ[tile->flowDir] * fraction;
                }
            }
        }

        if (totalFlowX != 0.0f || totalFlowZ != 0.0f) {
            entity.move(totalFlowX * fx->flowForce, totalFlowZ * fx->flowForce, dt);
        }
    }

    return { fx->speedScale, fx->gravityScale };
}

// Handle exit effects (call after applyCollisionEffects)
inline void applyCollisionExitEffects(
        Entity& entity, CollisionEffectState& state,
        const CollisionEffectRegistry& registry) {

    // Check all non-solid types for exit
    for (auto& [typeInt, fx] : registry.effects) {
        CLTileType type = (CLTileType)typeInt;
        if (state.justExited(type) && fx.exitJumpForce > 0.0f && IsKeyDown(KEY_SPACE)) {
            entity.gravity.launch(fx.exitJumpForce);
        }
    }
}
