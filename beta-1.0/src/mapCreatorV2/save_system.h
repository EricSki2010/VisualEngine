#pragma once
#include "placed_blocks.h"
#include "editor_camera.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

static std::string sanitizeFilename(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ')
            result += c;
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    while (!result.empty() && result.front() == ' ') result.erase(result.begin());
    if (result.empty()) result = "unnamed";
    return result;
}

// ── Shared block-line parsing ─────────────────────────────
// Parses "cb=lx,lz,y,blockName" lines (chunk-relative) and
// "block=x,y,z,blockName" lines (legacy world-relative).
// Used by both loadStructureInto and loadImportBlocks to avoid duplication.

static bool parseBlockLine(const std::string& rest, int& x, int& y, int& z, std::string& blockName) {
    int commaCount = 0, lastComma = -1;
    for (int i = 0; i < (int)rest.size(); i++) {
        if (rest[i] == ',') { commaCount++; if (commaCount == 3) { lastComma = i; break; } }
    }
    if (lastComma <= 0) return false;
    if (sscanf(rest.c_str(), "%d,%d,%d", &x, &y, &z) != 3) return false;
    blockName = rest.substr(lastComma + 1);
    return true;
}

// Loads chunk/block data from a stream into a ChunkMatrix.
// Shared logic for both full structure loads and import-only loads.
static void loadBlocksFromStream(std::ifstream& in, ChunkMatrix& matrix,
                                  std::string* outLine = nullptr) {
    Vec2i currentChunkKey = {0, 0};
    bool inChunk = false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("chunk=", 0) == 0) {
            int cx, cz, cminY, cmaxY;
            if (sscanf(line.c_str() + 6, "%d,%d,%d,%d", &cx, &cz, &cminY, &cmaxY) == 4) {
                currentChunkKey = {cx, cz};
                inChunk = true;
            }
        } else if (line == "endchunk") {
            inChunk = false;
        } else if (line.rfind("cb=", 0) == 0 && inChunk) {
            int lx, lz, y;
            std::string blockName;
            if (parseBlockLine(line.substr(3), lx, lz, y, blockName)) {
                int worldX = currentChunkKey.x * CHUNK_SZ + lx;
                int worldZ = currentChunkKey.z * CHUNK_SZ + lz;
                matrix.set(worldX, y, worldZ, blockName);
            }
        } else if (line.rfind("block=", 0) == 0) {
            int x, y, z;
            std::string blockName;
            if (parseBlockLine(line.substr(6), x, y, z, blockName))
                matrix.set(x, y, z, blockName);
        }
    }
}

// ── Type-based subfolder ──────────────────────────────────
static std::string getSaveSubfolder(const std::string& type) {
    if (type == "3D Model") return "saves/models";
    if (type == "3D Block") return "saves/blocks";
    if (type == "3D Sprite") return "saves/sprites";
    return "saves/structures";
}

// ── Save ──────────────────────────────────────────────────
struct HotbarSlotData {
    bool hasColor = false;
    Color color = GRAY;
};

static bool saveStructure(const std::string& filepath, const std::string& name,
                           const std::string& type, const PlacedBlocks& blocks,
                           const EditorCamera& cam,
                           const HotbarSlotData* hotbar = nullptr, int hotbarCount = 0) {
    fs::create_directories(fs::path(filepath).parent_path());
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "name=" << name << "\n";
    out << "type=" << type << "\n";

    if (!blocks.placeholders.empty()) {
        auto& o = blocks.placeholders[0];
        out << "origin=" << o.x << "," << o.y << "," << o.z << "\n";
    }

    if (blocks.modelSize > 0) {
        out << "modelsize=" << blocks.modelSize << "\n";
        for (auto& r : blocks.removed)
            out << "removed=" << r.x << "," << r.y << "," << r.z << "\n";
    } else if (blocks.modelDims.x > 0) {
        out << "modeldims=" << blocks.modelDims.x << "," << blocks.modelDims.y << "," << blocks.modelDims.z << "\n";
        out << "modelorigin=" << blocks.modelOriginPos.x << "," << blocks.modelOriginPos.y << "," << blocks.modelOriginPos.z << "\n";
        for (auto& r : blocks.removed)
            out << "removed=" << r.x << "," << r.y << "," << r.z << "\n";
    } else {
        for (int i = 1; i < (int)blocks.placeholders.size(); i++) {
            auto& p = blocks.placeholders[i];
            out << "scaffold=" << p.x << "," << p.y << "," << p.z << "\n";
        }
    }

    out << "camera=" << cam.origin.x << "," << cam.origin.y << "," << cam.origin.z
        << "," << cam.yaw << "," << cam.pitch << "," << cam.distance << "\n";

    // Save hotbar colors
    if (hotbar && hotbarCount > 0) {
        for (int i = 0; i < hotbarCount; i++) {
            if (hotbar[i].hasColor) {
                char buf[32];
                snprintf(buf, sizeof(buf), "hotbar=%d,%02x%02x%02x%02x",
                    i, hotbar[i].color.r, hotbar[i].color.g, hotbar[i].color.b, hotbar[i].color.a);
                out << buf << "\n";
            }
        }
    }

    for (auto& [key, chk] : blocks.matrix.chunks) {
        if (chk.empty()) continue;
        int blockCount = 0;
        int span = chk.ySpan();
        for (int lx = 0; lx < CHUNK_SZ; lx++)
            for (int lz = 0; lz < CHUNK_SZ; lz++)
                for (int yi = 0; yi < span; yi++)
                    if (!chk.cells[(lx * CHUNK_SZ + lz) * span + yi].empty())
                        blockCount++;
        if (blockCount == 0) continue;

        out << "chunk=" << key.x << "," << key.z << "," << chk.minY << "," << chk.maxY << "\n";
        for (int lx = 0; lx < CHUNK_SZ; lx++)
            for (int lz = 0; lz < CHUNK_SZ; lz++)
                for (int yi = 0; yi < span; yi++) {
                    const auto& bname = chk.cells[(lx * CHUNK_SZ + lz) * span + yi];
                    if (!bname.empty())
                        out << "cb=" << lx << "," << lz << "," << (chk.minY + yi) << "," << bname << "\n";
                }
        out << "endchunk\n";
    }
    return true;
}

// ── Save header (for list display) ────────────────────────
struct SaveEntry {
    std::string filepath;
    std::string name;
    std::string type;
    bool starred = false;
};

static SaveEntry loadSaveHeader(const std::string& filepath) {
    SaveEntry entry;
    entry.filepath = filepath;
    std::ifstream in(filepath);
    if (!in.is_open()) return entry;

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("name=", 0) == 0) entry.name = line.substr(5);
        else if (line.rfind("type=", 0) == 0) entry.type = line.substr(5);
        else break;
    }
    return entry;
}

// ── Full structure load ───────────────────────────────────
static void loadStructureInto(const std::string& filepath, std::string& name,
                               std::string& type, PlacedBlocks& blocks,
                               EditorCamera& cam,
                               HotbarSlotData* hotbar = nullptr, int hotbarCount = 0) {
    std::ifstream in(filepath);
    if (!in.is_open()) return;

    blocks.placeholders.clear();
    blocks.matrix.clear();
    blocks.modelSize = 0;
    blocks.modelDims = {0, 0, 0};
    blocks.modelOriginPos = {0, 0, 0};
    blocks.removed.clear();

    bool cameraLoaded = false;

    // First pass: read metadata lines (before chunk/block data)
    std::string line;
    std::streampos blockDataStart;
    while (std::getline(in, line)) {
        if (line.rfind("name=", 0) == 0) {
            name = line.substr(5);
        } else if (line.rfind("type=", 0) == 0) {
            type = line.substr(5);
        } else if (line.rfind("origin=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 7, "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                blocks.placeholders.push_back(p);
        } else if (line.rfind("modelsize=", 0) == 0) {
            blocks.modelSize = std::atoi(line.c_str() + 10);
        } else if (line.rfind("modeldims=", 0) == 0) {
            sscanf(line.c_str() + 10, "%d,%d,%d", &blocks.modelDims.x, &blocks.modelDims.y, &blocks.modelDims.z);
        } else if (line.rfind("modelorigin=", 0) == 0) {
            sscanf(line.c_str() + 12, "%f,%f,%f", &blocks.modelOriginPos.x, &blocks.modelOriginPos.y, &blocks.modelOriginPos.z);
        } else if (line.rfind("placeholder=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 12, "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                blocks.placeholders.push_back(p);
        } else if (line.rfind("scaffold=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 9, "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                blocks.placeholders.push_back(p);
        } else if (line.rfind("removed=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 8, "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                blocks.removed.insert(p);
        } else if (line.rfind("camera=", 0) == 0) {
            float cx, cy, cz, cyaw, cpitch, cdist;
            if (sscanf(line.c_str() + 7, "%f,%f,%f,%f,%f,%f", &cx, &cy, &cz, &cyaw, &cpitch, &cdist) == 6) {
                cam.origin = { cx, cy, cz };
                cam.yaw = cyaw;
                cam.pitch = cpitch;
                cam.distance = cdist;
                cameraLoaded = true;
            }
        } else if (line.rfind("hotbar=", 0) == 0) {
            int slot; unsigned int r, g, b, a;
            if (hotbar && sscanf(line.c_str() + 7, "%d,%02x%02x%02x%02x", &slot, &r, &g, &b, &a) == 5) {
                if (slot >= 0 && slot < hotbarCount) {
                    hotbar[slot].hasColor = true;
                    hotbar[slot].color = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
                }
            }
        } else if (line.rfind("chunk=", 0) == 0 || line.rfind("cb=", 0) == 0 || line.rfind("block=", 0) == 0) {
            // We've hit block data — re-read from the beginning of block section
            // by seeking back and using the shared loader
            break;
        }
    }

    // Seek back to re-parse block data lines — simpler to just re-open
    // since ifstream doesn't easily support putback of full lines
    std::ifstream blockIn(filepath);
    if (blockIn.is_open()) {
        loadBlocksFromStream(blockIn, blocks.matrix);
    }

    if (blocks.placeholders.empty())
        blocks.placeholders.push_back({0, 0, 0});

    // Legacy save compatibility: infer modelSize from scaffolds
    if (type == "3D Block" && blocks.modelSize == 0 && blocks.placeholders.size() > 1) {
        int maxCoord = 0;
        for (auto& p : blocks.placeholders) {
            maxCoord = std::max(maxCoord, std::max({std::abs(p.x), p.y, std::abs(p.z)}));
        }
        blocks.modelSize = maxCoord + 1;
        Vec3i origin = blocks.placeholders[0];
        blocks.placeholders.clear();
        blocks.placeholders.push_back(origin);
    }

    if (!cameraLoaded) {
        auto o = blocks.placeholders[0];
        cam.origin = { (float)o.x, (float)o.y, (float)o.z };
    }
    cam.init();
    blocks.rebuildPlaceholderSet();
    blocks.markDirty();
}

// ── Import blocks only (no metadata) ──────────────────────
static ChunkMatrix loadImportBlocks(const std::string& filepath) {
    ChunkMatrix result;
    std::ifstream in(filepath);
    if (!in.is_open()) return result;
    loadBlocksFromStream(in, result);
    return result;
}

// ── Starred saves ─────────────────────────────────────────

static std::string getStarredFilePath() { return "saves/starred.txt"; }

static std::set<std::string> loadStarredSet() {
    std::set<std::string> starred;
    std::ifstream in(getStarredFilePath());
    if (!in.is_open()) return starred;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) starred.insert(line);
    }
    return starred;
}

static void saveStarredSet(const std::set<std::string>& starred) {
    fs::create_directories("saves");
    std::ofstream out(getStarredFilePath());
    if (!out.is_open()) return;
    for (auto& s : starred) out << s << "\n";
}

static void toggleStarred(const std::string& filepath) {
    auto starred = loadStarredSet();
    if (starred.count(filepath)) starred.erase(filepath);
    else starred.insert(filepath);
    saveStarredSet(starred);
}

static bool isStarred(const std::string& filepath) {
    auto starred = loadStarredSet();
    return starred.count(filepath) > 0;
}

// ── Delete / List saves ───────────────────────────────────

static bool deleteSave(const std::string& filepath) {
    // Also remove from starred if present
    auto starred = loadStarredSet();
    if (starred.count(filepath)) {
        starred.erase(filepath);
        saveStarredSet(starred);
    }
    return fs::remove(filepath);
}

static std::vector<SaveEntry> listSaves() {
    std::vector<SaveEntry> entries;
    if (!fs::exists("saves")) return entries;
    auto starred = loadStarredSet();
    // Scan saves/ and all subdirectories
    for (auto& entry : fs::recursive_directory_iterator("saves")) {
        if (entry.path().extension() == ".sav") {
            SaveEntry se = loadSaveHeader(entry.path().string());
            se.starred = starred.count(se.filepath) > 0;
            entries.push_back(se);
        }
    }
    // Starred saves first, then alphabetical
    std::sort(entries.begin(), entries.end(), [](const SaveEntry& a, const SaveEntry& b) {
        if (a.starred != b.starred) return a.starred > b.starred;
        return a.name < b.name;
    });
    return entries;
}
