#include "engine/engine.h"
#include "engine/default/inventory_ui.h"
#include "engine/core/FastNoiseLite.h"
#include "game/world/worldgen_common.h"
#include "game/world/terrain_gen.h"
#include "game/world/spring_river_system.h"
#include <filesystem>
#include <unordered_set>

// ── Height calculation ──────────────────────────────────────────────
// Must match exactly between rendering and collision.

static const float MOUNTAIN_THRESHOLD = 0.75f;
static const float PEAK_THRESHOLD     = 0.77f;
static const float MOUNTAIN_BASE_Y    = floorf(MOUNTAIN_THRESHOLD / 0.05f) * 0.25f;
static const float PEAK_BASE_Y        = MOUNTAIN_BASE_Y + floorf((PEAK_THRESHOLD - MOUNTAIN_THRESHOLD) / 0.01f) * 0.5f;
static const float SEA_LEVEL          = floorf(WATER_THRESHOLD / 0.05f) * 0.25f - 0.2f;

float getBlockY(float elev, bool isRiver) {
    if (isRiver) return floorf(elev / 0.025f) * 0.125f - 0.1f;
    if (elev >= PEAK_THRESHOLD) {
        float above = elev - PEAK_THRESHOLD;
        return PEAK_BASE_Y + floorf(above / 0.002f) * 0.5f;
    }
    if (elev >= MOUNTAIN_THRESHOLD) {
        float above = elev - MOUNTAIN_THRESHOLD;
        return MOUNTAIN_BASE_Y + floorf(above / 0.01f) * 0.5f;
    }
    return floorf(elev / 0.05f) * 0.25f;
}

float getGroundHeight(float elev) {
    return getBlockY(elev, false) + 0.5f;  // top of cube
}

// ── Biome colors ────────────────────────────────────────────────────

const char* getBiomeBlockName(TileType tile) {
    switch (tile) {
        case TILE_DEEP_WATER:    return "deepWater";
        case TILE_SHALLOW_WATER: return "shallowWater";
        case TILE_SAND:          return "sand";
        case TILE_DESERT:        return "desert";
        case TILE_DRY_GRASS:     return "dryGrass";
        case TILE_GRASS:         return "grassBlock";
        case TILE_SWAMP:         return "swamp";
        case TILE_DRY_HILLS:     return "dryHills";
        case TILE_FOREST:        return "forest";
        case TILE_DENSE_FOREST:  return "denseForest";
        case TILE_BARE_STONE:    return "bareStone";
        case TILE_MOSSY_STONE:   return "mossyStone";
        case TILE_MOUNTAIN_PEAK: return "mountainPeak";
        case TILE_SNOWY_PEAK:    return "snowyPeak";
        default:                 return "grassBlock";
    }
}

// ── Chunk generation ────────────────────────────────────────────────
// Generate a 16x16 chunk of tiles into the engine's world.

void generateChunk(Engine& engine, int chunkX, int chunkZ,
                   FastNoiseLite& elevNoise, FastNoiseLite& moistNoise,
                   FastNoiseLite& vegNoise, SpringRiverSystem& rivers) {
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int x = chunkX * CHUNK_SIZE + lx;
            int z = chunkZ * CHUNK_SIZE + lz;

            float elev  = sampleNoise(elevNoise, (float)x, (float)z);
            float moist = sampleNoise(moistNoise, (float)x, (float)z);
            float veg   = sampleNoise(vegNoise, (float)x, (float)z);

            TileType biome = getBiome(elev, moist, veg);
            bool isRiver = rivers.isRiver(x, z);
            bool isBank  = rivers.isBank(x, z);
            bool isWater = (biome == TILE_DEEP_WATER || biome == TILE_SHALLOW_WATER);

            float blockY = getBlockY(elev, isRiver);
            if (isWater) blockY = SEA_LEVEL;
            if (isWater || isRiver) blockY -= 0.1f;

            // Pick block name and flow direction
            const char* blockName;
            int flowDir = -1;
            if (isRiver) {
                blockName = "river";
                flowDir = (int)rivers.getFlowDir(x, z);
            } else if (isBank) {
                blockName = "riverBank";
            } else {
                blockName = getBiomeBlockName(biome);
            }

            engine.placeBlock(blockName, x, z, blockY, flowDir);
        }
    }
}

// ── Main ────────────────────────────────────────────────────────────

int main() {
    Engine engine;
    engine.init("V2 Beta 1.0");
    engine.loadBlocks("assets/entities/blocks");
    engine.loadItems("assets/entities/items");
    engine.setView(VIEW_2_5D);
    engine.setSpawn(0, 5, 0);
    engine.seed = WorldSeed;
    engine.viewRadius = 56;

    // Player setup
    engine.player.loadModel("assets/player.glb", 3.6f);  // TODO: temp enlarged, revert to 1.8f
    engine.player.maxStepHeight = 0.26f;
    engine.player.jumpForce = 4.9f;

    // Load collision effects (water swimming, river effects, etc.)
    engine.loadCollisionEffects();

    // ── Noise generators ──
    FastNoiseLite elevationNoise;
    elevationNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    elevationNoise.SetSeed(WorldSeed);
    elevationNoise.SetFrequency(0.008f);

    FastNoiseLite moistureNoise;
    moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    moistureNoise.SetSeed(WorldSeed + 1);
    moistureNoise.SetFrequency(0.008f);

    FastNoiseLite vegetationNoise;
    vegetationNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    vegetationNoise.SetSeed(WorldSeed + 2);
    vegetationNoise.SetFrequency(0.01f);

    // ── River system ──
    SpringRiverSystem rivers;

    // ── Chunk tracking ──
    std::unordered_set<int64_t> generatedChunks;
    int genRadius = 5;  // chunks around player to generate (5 * 16 = 80 tiles)

    // Generate initial chunks synchronously so the player has ground to land on
    auto ensureChunksAround = [&](int px, int pz) {
        int pcx = (px >= 0) ? px / CHUNK_SIZE : (px - CHUNK_SIZE + 1) / CHUNK_SIZE;
        int pcz = (pz >= 0) ? pz / CHUNK_SIZE : (pz - CHUNK_SIZE + 1) / CHUNK_SIZE;

        for (int cz = pcz - genRadius; cz <= pcz + genRadius; cz++) {
            for (int cx = pcx - genRadius; cx <= pcx + genRadius; cx++) {
                int64_t key = tileKey(cx, cz);
                if (generatedChunks.count(key)) continue;

                // Ensure river regions are generated for this chunk area
                int worldX = cx * CHUNK_SIZE;
                int worldZ = cz * CHUNK_SIZE;
                if (!rivers.isRegionGenerated(worldX, worldZ)) {
                    rivers.generateRegion(
                        SpringRiverSystem::worldToRegion(worldX),
                        SpringRiverSystem::worldToRegion(worldZ),
                        elevationNoise
                    );
                }

                generateChunk(engine, cx, cz, elevationNoise, moistureNoise,
                              vegetationNoise, rivers);
                generatedChunks.insert(key);
            }
        }
    };

    // Generate starting area
    ensureChunksAround(0, 0);

    // ── Custom tile info for collision (water surfaces) ──
    engine.onGetTileInfo = [&](int tileX, int tileZ) -> CLTileInfo {
        float elev = sampleNoise(elevationNoise, (float)tileX, (float)tileZ);
        bool isRiver = rivers.isRiver(tileX, tileZ);
        float blockTop = getGroundHeight(elev);
        bool isWater = (elev < WATER_THRESHOLD);

        CLTileType type = CL_TILE_SOLID;
        float waterSurf = 0.0f;

        if (isRiver) {
            type = CL_TILE_RIVER;
            waterSurf = blockTop;
        } else if (isWater) {
            type = CL_TILE_WATER;
            waterSurf = SEA_LEVEL + 0.5f;
        }

        return { blockTop, type, waterSurf };
    };

    // ── Inventory + hotbar ──
    engine.inventory.init(24, KEY_E);
    engine.hotbar.init(9);

    auto iconLookup = [&](const std::string& name) -> const Texture2D* {
        const BlockDef* b = engine.registry.get(name);
        if (b && b->iconLoaded) return &b->icon;
        return nullptr;
    };

    InventoryManager invManager;
    invManager.init(&engine.inventory, &engine.hotbar);

    InventoryManagerUI invUI;
    invUI.init(&invManager);
    invUI.iconLookup = iconLookup;

    // ── Game state ──
    SetExitKey(KEY_NULL);

    // ── Update ──
    engine.onUpdate = [&](float dt) {
        // ESC handling
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (engine.inventory.open)
                engine.inventory.open = false;
            else
                engine.paused = !engine.paused;
        }

        // Save / Load
        if (IsKeyPressed(KEY_F5)) {
            std::filesystem::create_directories("saves");
            engine.save("saves/world.sav");
        }
        if (IsKeyPressed(KEY_F9)) engine.load("saves/world.sav");
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // Exit button click while engine.paused
        if (engine.paused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            int btnW = 200, btnH = 50;
            int btnX = (GetScreenWidth() - btnW) / 2;
            int btnY = GetScreenHeight() / 2 + 60;
            if (mouse.x >= btnX && mouse.x <= btnX + btnW &&
                mouse.y >= btnY && mouse.y <= btnY + btnH) {
                CloseWindow();
                return;
            }
        }

        if (engine.paused) return;

        // Generate chunks around the player
        int px = (int)floorf(engine.player.position.x);
        int pz = (int)floorf(engine.player.position.z);
        ensureChunksAround(px, pz);

        // Unload distant river regions
        rivers.unloadDistant(px, pz, 5);

        // Inventory
        invManager.update();
        invUI.update();
    };

    // ── Draw UI ──
    engine.onDrawUI = [&]() {
        if (!engine.paused) invUI.draw();

        // HUD
        if (!engine.paused) DrawText("F5 Save | F9 Load | F11 Fullscreen", 10, 60, 16, GRAY);

        if (!engine.paused) {
            float px = engine.player.position.x;
            float pz = engine.player.position.z;
            float elev  = sampleNoise(elevationNoise, roundf(px), roundf(pz));
            float moist = sampleNoise(moistureNoise, roundf(px), roundf(pz));
            float veg   = sampleNoise(vegetationNoise, roundf(px), roundf(pz));
            TileType biome = getBiome(elev, moist, veg);

            DrawText(TextFormat("Elev: %.3f  Moist: %.3f  Veg: %.3f", elev, moist, veg),
                     10, 80, 18, WHITE);
            bool inWater = engine.playerCollisionState.currentType != CL_TILE_SOLID;
            DrawText(TextFormat("Biome: %s%s",
                     TILE_NAMES[biome],
                     inWater ? "  [SWIMMING]" : ""),
                     10, 100, 18, WHITE);

            if (engine.isBuilding())
                DrawText("Generating...", 10, 120, 20, YELLOW);
        }

        if (engine.paused) {
            DrawRectangle(-100, -100, GetScreenWidth() + 200, GetScreenHeight() + 200, { 0, 0, 0, 150 });
            const char* txt = "PAUSED";
            int w = MeasureText(txt, 40);
            DrawText(txt, (GetScreenWidth() - w) / 2,
                     GetScreenHeight() / 2 - 20, 40, WHITE);
            DrawText("ESC to resume",
                     (GetScreenWidth() - MeasureText("ESC to resume", 18)) / 2,
                     GetScreenHeight() / 2 + 30, 18, GRAY);

            // Exit button
            int btnW = 200, btnH = 50;
            int btnX = (GetScreenWidth() - btnW) / 2;
            int btnY = GetScreenHeight() / 2 + 60;
            Vector2 mouse = GetMousePosition();
            bool hovered = mouse.x >= btnX && mouse.x <= btnX + btnW &&
                           mouse.y >= btnY && mouse.y <= btnY + btnH;
            DrawRectangle(btnX, btnY, btnW, btnH, hovered ? Color{180, 50, 50, 255} : Color{120, 30, 30, 255});
            DrawRectangleLines(btnX, btnY, btnW, btnH, WHITE);
            const char* exitTxt = "Exit Game";
            int exitW = MeasureText(exitTxt, 24);
            DrawText(exitTxt, btnX + (btnW - exitW) / 2, btnY + (btnH - 24) / 2, 24, WHITE);
        }
    };

    engine.run();
}
