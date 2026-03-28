#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "engine/core/world.h"
#include "engine/core/save.h"
#include "engine/gameplay/entity.h"
#include "engine/gameplay/collision.h"
#include "engine/gameplay/gravity.h"
#include "engine/gameplay/registry.h"
#include "engine/gameplay/inventory.h"
#include "engine/gameplay/collision_effects.h"
#include "engine/gameplay/sprite.h"
#include "engine/default/camera.h"
#include "engine/default/input.h"
#include <cmath>
#include <functional>
#include <string>
#include <fstream>
#include <thread>
#include <atomic>

// ── Engine ──────────────────────────────────────────────────────────
// Ties together world, camera, player entity, and game loop.
// Can run standalone with just placeBlock + setSpawn + run().

// Callback for custom per-frame logic (called each frame before drawing)
using UpdateFn = std::function<void(float dt)>;
// Callback for custom drawing (called inside BeginMode3D)
using DrawFn = std::function<void()>;

struct Engine {
    World world;
    GameCamera camera;
    Entity player;
    BlockRegistry registry;
    ItemRegistry itemRegistry;
    Inventory inventory;
    HotBar hotbar;
    Vector3 spawnPoint = { 0.0f, 1.0f, 0.0f };
    int viewRadius = 56;
    bool showDebug = false;
    bool paused = false;   // when true, skips movement/gravity/input (rendering + onUpdate still run)
    int seed = 0;  // game-provided seed, saved/loaded automatically

    // Collision effects (loaded from assets/collisionTypes/)
    CollisionEffectRegistry collisionEffects;
    CollisionEffectState playerCollisionState;

    // Background build thread
    std::thread buildThread;
    std::atomic<bool> building{false};

    // Non-copyable, non-movable (thread captures `this`)
    Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    ~Engine() {
        if (buildThread.joinable()) buildThread.join();
    }

    // Optional hooks for game-specific logic
    UpdateFn onUpdate = nullptr;
    DrawFn onDraw = nullptr;     // called inside BeginMode3D (3D stuff)
    DrawFn onDrawUI = nullptr;   // called after EndMode3D (2D overlays, always on top)

    // Optional: override how tiles are registered for collision.
    // If set, this replaces the default world-based tile lookup.
    // Use this for custom water surfaces, procedural terrain, etc.
    TileInfoFn onGetTileInfo = nullptr;

    void init(const char* title, bool fullscreen = true) {
        SetConfigFlags(FLAG_MSAA_4X_HINT);
        InitWindow(800, 600, title);  // small window first so raylib can read monitor info

        if (fullscreen) {
            int monitor = GetCurrentMonitor();
            int monW = GetMonitorWidth(monitor);
            int monH = GetMonitorHeight(monitor);
            if (monW > 0 && monH > 0) {
                SetWindowSize(monW, monH);
            }
            ToggleFullscreen();
        }

        SetTargetFPS(60);
        camera.init(VIEW_2_5D);
    }

    template<typename R>
    void loadRegistryAssets(R& reg, const std::string& jsonFolder,
                            const std::string& modelFolder, const std::string& iconFolder) {
        reg.loadFolder(jsonFolder);
        reg.autoDetectModels(modelFolder);
        reg.loadModels();
        reg.autoDetectIcons(iconFolder);
        reg.loadIcons();
    }

    // Load all block definitions from a folder of .json files
    // Also loads any .glb models referenced by the blocks
    // modelFolder is checked for .glb files matching block names (e.g. grassBlock.glb)
    void loadBlocks(const std::string& jsonFolder,
                    const std::string& modelFolder = "assets/entities/blockModels",
                    const std::string& iconFolder = "assets/entities/blockIcons") {
        loadRegistryAssets(registry, jsonFolder, modelFolder, iconFolder);
    }

    // Load all item definitions from a folder of .json files
    // Also loads any .glb models and icons referenced by the items
    void loadItems(const std::string& jsonFolder,
                   const std::string& modelFolder = "assets/entities/itemModels",
                   const std::string& iconFolder = "assets/entities/itemIcons") {
        loadRegistryAssets(itemRegistry, jsonFolder, modelFolder, iconFolder);
    }

    // Load collision effect definitions (water, river, etc.)
    void loadCollisionEffects(const std::string& folder = "assets/collisionTypes") {
        collisionEffects.loadFolder(folder);
    }

    void setView(ViewMode mode) {
        camera.setView(mode);
    }

    void setSpawn(float x, float y, float z) {
        spawnPoint = { x, y, z };
    }

    // Run world generation on a background thread.
    // The game loop can start immediately — tiles appear as they're placed.
    // World read/write is thread-safe, so placeBlock works from any thread.
    template<typename Fn>
    void buildAsync(Fn&& fn) {
        // Wait for any previous build to finish
        if (buildThread.joinable()) buildThread.join();
        building = true;
        buildThread = std::thread([this, f = std::forward<Fn>(fn)]() {
            f();
            building = false;
        });
    }

    // Returns true while a buildAsync task is still running
    bool isBuilding() const { return building; }

    // Wait for background build to finish (blocks until done)
    void waitForBuild() {
        if (buildThread.joinable()) buildThread.join();
    }

    // Place a block by registry name: placeBlock("grassBlock", 0, 0, 0.0f)
    // flowDir: -1 = none, 0 = N, 1 = E, 2 = S, 3 = W (for rivers, etc.)
    void placeBlock(const std::string& name, int x, int z, float height = 0.0f, int flowDir = -1) {
        const BlockDef* def = registry.get(name);
        if (def) {
            CLTileType clType = CL_TILE_SOLID;
            if (def->collisionType == "water") clType = CL_TILE_WATER;
            else if (def->collisionType == "river") clType = CL_TILE_RIVER;

            world.placeTile(x, z, (int)clType, height, def->r, def->g, def->b, def->a, name, flowDir);
        } else {
            printf("WARNING: Block '%s' not found in registry. Block not placed.\n", name.c_str());
        }
    }

    // Place a block by RGB color: placeBlock(60, 160, 50, 0, 0, 0.0f)
    void placeBlock(unsigned char r, unsigned char g, unsigned char b,
                    int x, int z, float height = 0.0f) {
        world.placeTile(x, z, 0, height, r, g, b, 255);
    }

    // Place a block by RGBA color
    void placeBlock(unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                    int x, int z, float height = 0.0f) {
        world.placeTile(x, z, 0, height, r, g, b, a);
    }

    void removeBlock(int x, int z) {
        world.removeTile(x, z);
    }

    // ── Save / Load ──────────────────────────────────────────────────

    bool save(const std::string& path) const {
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) {
            printf("SAVE: Failed to open %s for writing\n", path.c_str());
            return false;
        }

        using namespace _save;

        // Header
        uint32_t magic = SAVE_MAGIC;
        uint32_t ver = SAVE_VERSION;
        int32_t s = seed;
        w(f, &magic, 4);
        w(f, &ver, 4);
        w(f, &s, 4);

        // Player
        wVec3(f, player.position);
        w(f, &player.facingAngle, sizeof(float));

        // Inventory
        wSlots(f, inventory.slots);

        // HotBar (selected index goes between count and slot data)
        uint32_t hbCount = (uint32_t)hotbar.slots.size();
        int32_t hbSel = hotbar.selected;
        w(f, &hbCount, 4);
        w(f, &hbSel, 4);
        wSlotData(f, hotbar.slots);


        // World tiles (lock to prevent races with buildAsync)
        std::shared_lock lock(world.mtx);
        uint32_t tileCount = 0;
        for (auto& [ck, chunk] : world.chunks)
            tileCount += (uint32_t)chunk.tiles.size();
        w(f, &tileCount, 4);

        for (auto& [ck, chunk] : world.chunks) {
            int chunkX, chunkZ;
            unpackTileKey(ck, chunkX, chunkZ);
            int baseX = chunkX * CHUNK_SIZE;
            int baseZ = chunkZ * CHUNK_SIZE;

            for (auto& [tk, tile] : chunk.tiles) {
                int localX, localZ;
                unpackTileKey(tk, localX, localZ);
                int32_t wx = baseX + localX;
                int32_t wz = baseZ + localZ;
                w(f, &wx, 4);
                w(f, &wz, 4);
                w(f, &tile.type, 4);
                w(f, &tile.height, sizeof(float));
                wColor(f, tile.r, tile.g, tile.b, tile.a);
                wStr(f, tile.blockName);
                int32_t fd = tile.flowDir;
                w(f, &fd, 4);
            }
        }

        f.close();
        printf("SAVE: Wrote %u tiles, %u inv slots, %u hotbar slots to %s\n",
               tileCount, (uint32_t)inventory.slots.size(), hbCount, path.c_str());
        return true;
    }

    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            printf("SAVE: File not found: %s\n", path.c_str());
            return false;
        }

        using namespace _save;

        // Header
        uint32_t magic = 0, ver = 0;
        int32_t s = 0;
        r(f, &magic, 4);
        if (magic != SAVE_MAGIC) {
            printf("SAVE: Invalid save file (bad magic)\n");
            return false;
        }
        r(f, &ver, 4);
        if (ver > SAVE_VERSION) {
            printf("SAVE: Save version %u is newer than supported (%u)\n", ver, SAVE_VERSION);
            return false;
        }
        r(f, &s, 4);
        if (!f.good()) { printf("SAVE: Truncated header in %s\n", path.c_str()); return false; }
        seed = s;

        // Player
        player.position = rVec3(f);
        r(f, &player.facingAngle, sizeof(float));

        if (!f.good()) { printf("SAVE: Truncated player data in %s\n", path.c_str()); return false; }

        // Inventory
        rSlots(f, inventory.slots, inventory.slotCount, 1000);

        if (!f.good()) { printf("SAVE: Truncated inventory in %s\n", path.c_str()); return false; }

        // HotBar (selected index sits between count and slot data)
        uint32_t hbCount = 0;
        int32_t hbSel = 0;
        r(f, &hbCount, 4);
        r(f, &hbSel, 4);
        if (hbCount > 10) hbCount = 10;
        hotbar.slots.resize(hbCount);
        hotbar.slotCount = (int)hbCount;
        if (hbSel < 0) hbSel = 0;
        if (hbCount > 0 && hbSel >= (int32_t)hbCount) hbSel = (int32_t)hbCount - 1;
        hotbar.selected = hbSel;
        rSlotData(f, hotbar.slots, hbCount);


        if (!f.good()) { printf("SAVE: Truncated hotbar in %s\n", path.c_str()); return false; }

        // World tiles
        constexpr uint32_t MAX_TILES = 100000000; // 100M tiles sanity cap
        uint32_t tileCount = 0;
        r(f, &tileCount, 4);
        if (tileCount > MAX_TILES) {
            printf("SAVE: Tile count %u exceeds maximum (%u) in %s\n", tileCount, MAX_TILES, path.c_str());
            return false;
        }
        world.clear();
        for (uint32_t i = 0; i < tileCount; i++) {
            if (!f.good()) { printf("SAVE: Truncated tile data in %s\n", path.c_str()); return false; }
            int32_t wx, wz;
            int type;
            float height;
            unsigned char cr, cg, cb, ca;
            r(f, &wx, 4);
            r(f, &wz, 4);
            r(f, &type, 4);
            r(f, &height, sizeof(float));
            rColor(f, cr, cg, cb, ca);
            std::string blockName = rStr(f);
            int32_t fd = -1;
            r(f, &fd, 4);
            world.placeTile(wx, wz, type, height, cr, cg, cb, ca, blockName, fd);
        }

        f.close();
        printf("SAVE: Loaded %u tiles, %u inv slots, %u hotbar slots from %s\n",
               tileCount, (uint32_t)inventory.slots.size(), hbCount, path.c_str());
        return true;
    }

    void run() {
        player.position = spawnPoint;
        player.gravity.land();

        while (!WindowShouldClose()) {
            float dt = GetFrameTime();

            if (!paused) {
                // Input
                InputDir input = getMovementInput(camera.mode);

                // Register collision from world tiles (or custom callback)
                if (onGetTileInfo) {
                    player.registerTiles(onGetTileInfo);
                } else {
                    player.registerTiles([&](int tileX, int tileZ) -> CLTileInfo {
                        auto tile = world.getTile(tileX, tileZ);
                        if (tile) {
                            return { tile->height + 0.5f, (CLTileType)tile->type, 0.0f };
                        }
                        return { -100.0f, CL_TILE_SOLID, 0.0f };
                    });
                }

                // Collision effects (water, river, etc.)
                CollisionEffectResult fxResult = applyCollisionEffects(
                    player, playerCollisionState, collisionEffects, dt, &world);

                // Movement (with effect speed scale)
                player.move(input.x, input.z, dt, fxResult.speedScale);

                // Jump (only if not swimming — swimming is handled by effects)
                if (jumpPressed() && playerCollisionState.currentType == CL_TILE_SOLID)
                    player.jump();

                // Gravity + ground (with effect gravity scale)
                player.applyGravity(dt, fxResult.gravityScale);

                // Exit effects (e.g. water exit jump)
                applyCollisionExitEffects(player, playerCollisionState, collisionEffects);

                // Debug toggle
                if (IsKeyPressed(KEY_F1)) showDebug = !showDebug;
            }

            // Custom update hook
            if (onUpdate) onUpdate(dt);

            // Camera
            camera.follow(player.position);

            // ── Draw ──
            BeginDrawing();
                ClearBackground({ 10, 10, 15, 255 });

                BeginMode3D(camera.cam);

                    // Draw world tiles in view (only visits nearby chunks)
                    int cx = (int)floorf(player.position.x);
                    int cz = (int)floorf(player.position.z);

                    world.forEachTileInRadius(cx, cz, viewRadius, [&](int tx, int tz, const WorldTile& tile) {
                        // Check if this block has a model
                        const BlockDef* def = nullptr;
                        if (!tile.blockName.empty()) {
                            def = registry.get(tile.blockName);
                        }

                        if (def && def->modelLoaded) {
                            // Draw .glb model scaled to fit modelSize
                            rlDisableBackfaceCulling();
                            DrawModelEx(def->model,
                                { (float)tx, tile.height + def->modelOffsetY, (float)tz },
                                { 0, 1, 0 }, 0.0f,
                                { def->modelScaleX, def->modelScaleY, def->modelScaleZ },
                                WHITE);
                            rlEnableBackfaceCulling();
                        } else {
                            // Draw colored cube with outline
                            Color col = { tile.r, tile.g, tile.b, tile.a };
                            DrawCube({ (float)tx, tile.height, (float)tz }, 1.0f, 1.0f, 1.0f, col);
                            DrawCubeWires({ (float)tx, tile.height, (float)tz }, 1.0f, 1.0f, 1.0f, BLACK);
                        }
                    });

                    // Custom draw hook
                    if (onDraw) onDraw();

                    // Player
                    player.draw();
                    if (showDebug) player.drawCollisionDebug();

                EndMode3D();

                // 2D UI layer (always on top of everything)
                if (onDrawUI) onDrawUI();

                DrawFPS(10, 10);
                DrawText(TextFormat("Pos: %.1f, %.1f, %.1f",
                    player.position.x, player.position.y, player.position.z),
                    10, 30, 20, WHITE);

            EndDrawing();
        }

        // Cleanup
        if (buildThread.joinable()) buildThread.join();
        player.unloadModel();
        registry.unloadModels();
        registry.unloadIcons();
        itemRegistry.unloadModels();
        itemRegistry.unloadIcons();
        CloseWindow();
    }
};
