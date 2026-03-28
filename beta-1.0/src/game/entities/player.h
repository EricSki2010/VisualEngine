#pragma once
#include "engine/engine.h"

// Game-specific player logic: water/swimming mechanics on top of Entity.
// Call playerUpdate() from onUpdate each frame.

struct PlayerState {
    bool inWater = false;
    bool wasInWater = false;
};

inline void playerUpdate(Engine& engine, PlayerState& state, float dt) {
    Entity& player = engine.player;

    // Detect water
    state.inWater = player.isBaseInCollisionType(CL_TILE_WATER) ||
                    player.isBaseInCollisionType(CL_TILE_RIVER);

    // Swimming: hold space to float up
    if (state.inWater) {
        if (IsKeyDown(KEY_SPACE)) {
            player.position.y += 0.2f * dt * 5.0f;
            player.gravity.velocityY = 0.0f;
            player.gravity.grounded = false;
        }
    } else if (state.wasInWater && IsKeyDown(KEY_SPACE)) {
        // Just left water while holding space — pop out
        player.gravity.launch(3.5f);
    }

    // Halve velocity on entering water
    if (state.inWater && !state.wasInWater) {
        player.gravity.velocityY *= 0.3f;
    }

    state.wasInWater = state.inWater;
}
