#include "setup.h"
#include <filesystem>

void setupDirectories() {
    std::filesystem::create_directories("assets/saves/3dModels");
}
