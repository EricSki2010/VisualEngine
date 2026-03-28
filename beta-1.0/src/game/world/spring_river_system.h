#pragma once
#include "game/world/worldgen_common.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

// River tile shape based on inflow analysis
enum RiverShape {
    SHAPE_STRAIGHT = 0,  // flow comes from behind
    SHAPE_BEND,          // flow comes from one side (no inflow behind)
    SHAPE_STILL,         // flow comes from both sides (no inflow behind)
    SHAPE_SOURCE,        // no inflow from behind or sides (spring head)
};

const int RIVER_REGION_SIZE = 512;      // tiles per region axis
const int RIVER_REGION_PADDING = 256;   // extra elevation sampling for river tracing
const int RIVER_SAMPLE_SIZE = RIVER_REGION_SIZE + 2 * RIVER_REGION_PADDING; // 1024

struct RiverParams {
    float springFrequency    = 0.036f;
    float springThreshold    = 0.388f;
    float minSpringElevation = 0.55f;
    int   localMaxRadius     = 3;
    int   maxRiverLength     = 2000;
    int   minRiverLength     = 30;
    // Widen thresholds: flow >= widenFlow1 -> radius 1, etc.
    int   widenFlow1         = 10;
    int   widenFlow2         = 30;
    int   widenFlow3         = 60;
};

class SpringRiverSystem {
public:
    RiverParams params;

    // Generate rivers for a single region (called on demand)
    void generateRegion(int regionX, int regionY, FastNoiseLite& elevationNoise);

    // Check if the region containing this world tile has been generated
    bool isRegionGenerated(int worldTileX, int worldTileY) const;

    // Convert world tile coord to region coord
    static int worldToRegion(int worldTile);

    // Query tiles (returns false for ungenerated regions)
    bool isRiver(int worldTileX, int worldTileY) const;
    bool isBank(int worldTileX, int worldTileY) const;
    int getFlow(int worldTileX, int worldTileY) const;
    FlowDir getFlowDir(int worldTileX, int worldTileY) const;
    RiverShape getShape(int worldTileX, int worldTileY) const;

    // Unload regions beyond a radius (in regions) from a world tile position
    void unloadDistant(int worldTileX, int worldTileY, int keepRadius);

    // Clear all generated regions (for terrain regeneration)
    void clearAll();

private:
    struct Region {
        std::vector<bool> river;   // RIVER_REGION_SIZE * RIVER_REGION_SIZE
        std::vector<bool> bank;
        std::vector<int> flow;
        std::vector<FlowDir> direction;
        std::vector<RiverShape> shape;
    };

    mutable std::shared_mutex regionMutex;
    std::unordered_map<int64_t, Region> regions;

    int64_t regionKey(int rx, int ry) const {
        return ((int64_t)rx << 32) | (uint32_t)ry;
    }

    // Shared lookup: returns region pointer and local index, or nullptr if ungenerated.
    // Caller must hold regionMutex.
    const Region* findRegionTile(int worldTileX, int worldTileY, int& index) const;
};
