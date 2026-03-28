#include "memory.h"
#include <filesystem>
#include <iostream>

static std::filesystem::path basePath;

void initBasePath(const std::filesystem::path& exePath) {
    basePath = exePath;
}

std::filesystem::path getBasePath() {
    return basePath;
}

bool checkAndCreate(const char* name, std::filesystem::path path) {
    if (std::filesystem::exists(path)) return true;

    try {
        std::filesystem::create_directory(path);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create " << name << " folder: " << e.what() << std::endl;
        return false;
    }
}

void setup() {
    std::filesystem::path assetsPath = basePath / "assets";
    if (checkAndCreate("assets", assetsPath)) {
        std::filesystem::path entitiesPath = assetsPath / "entities";
        if (checkAndCreate("entities", entitiesPath)) {
            checkAndCreate("blockModels", entitiesPath / "blockModels");
            checkAndCreate("blocks", entitiesPath / "blocks");
        }
    }

    std::filesystem::path savesPath = basePath / "saves";
    if (checkAndCreate("saves", savesPath)) {
        checkAndCreate("worldData", savesPath / "worldData");
    }
}
