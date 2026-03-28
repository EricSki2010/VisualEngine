#include "memory.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>

void NewScene3D(const char* name, int width, int height, int depth) {
    std::filesystem::path savesPath = getBasePath() / "saves" / "worldData";
    std::filesystem::path filePath = savesPath / (std::string(name) + ".bin");
    std::filesystem::path chunksPath = savesPath / (std::string(name) + "_chunks");

    if (std::filesystem::exists(filePath)) {
        std::cerr << "Scene already exists: " << filePath << std::endl;
        return;
    }

    std::filesystem::create_directories(savesPath);
    std::filesystem::create_directories(chunksPath);

    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create scene file: " << filePath << std::endl;
        return;
    }

    // Write header
    WorldHeader header;
    header.width = width;
    header.height = height;
    header.depth = depth;
    header.chunkSize = CHUNK_SIZE;
    file.write(reinterpret_cast<const char*>(&header), HEADER_BYTES);

    // Write empty index (1 byte per slot, all zeros = no chunk files yet)
    int totalSlots = width * height * depth;
    char zero[4096];
    std::memset(zero, 0, sizeof(zero));

    int written = 0;
    while (written < totalSlots) {
        int toWrite = totalSlots - written;
        if (toWrite > 4096) toWrite = 4096;
        file.write(zero, toWrite);
        written += toWrite;
    }

    file.close();
    if (!file) {
        std::cerr << "Error writing scene file" << std::endl;
    }
}
