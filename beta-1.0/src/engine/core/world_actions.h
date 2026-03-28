#pragma once
#include "engine/core/world.h"
#include <functional>

// ── World action types ──────────────────────────────────────────────

enum ActionType {
    ACTION_PLACE_TILE,
    ACTION_REMOVE_TILE,
    ACTION_SPAWN_MOB,
    ACTION_REMOVE_MOB,
};

// ── Callbacks the game registers to handle actions ──────────────────

using SpawnMobFn  = std::function<int(float x, float y, float z, int mobType)>;  // returns mob id
using RemoveMobFn = std::function<bool(int mobId)>;

// ── World action dispatcher ─────────────────────────────────────────
// Wraps a World with mob spawn/remove hooks the game layer provides.

struct WorldActions {
    World* world = nullptr;
    SpawnMobFn  spawnMob  = nullptr;
    RemoveMobFn removeMob = nullptr;

    bool placeTile(int x, int z, int type, float height,
                   unsigned char r = 128, unsigned char g = 128,
                   unsigned char b = 128, unsigned char a = 255) {
        if (!world) return false;
        world->placeTile(x, z, type, height, r, g, b, a);
        return true;
    }

    bool removeTile(int x, int z) {
        if (!world) return false;
        world->removeTile(x, z);
        return true;
    }

    bool canSpawnMob()  const { return spawnMob  != nullptr; }
    bool canRemoveMob() const { return removeMob != nullptr; }
};
