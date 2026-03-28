#pragma once
#include "placed_blocks.h"
#include "editor_camera.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>

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

static void saveStructure(const std::string& filepath, const std::string& name,
                           const std::string& type, const PlacedBlocks& blocks,
                           const EditorCamera& cam) {
    fs::create_directories("saves");
    std::ofstream out(filepath);
    if (!out.is_open()) return;

    out << "name=" << name << "\n";
    out << "type=" << type << "\n";

    if (!blocks.placeholders.empty()) {
        auto& o = blocks.placeholders[0];
        out << "origin=" << o.x << "," << o.y << "," << o.z << "\n";
    }

    if (blocks.modelSize > 0) {
        // Model mode: save dimensions instead of individual scaffolds
        out << "modelsize=" << blocks.modelSize << "\n";
        for (auto& r : blocks.removed)
            out << "removed=" << r.x << "," << r.y << "," << r.z << "\n";
    } else {
        // Structure mode: save individual scaffolds
        for (int i = 1; i < (int)blocks.placeholders.size(); i++) {
            auto& p = blocks.placeholders[i];
            out << "scaffold=" << p.x << "," << p.y << "," << p.z << "\n";
        }
    }

    out << "camera=" << cam.origin.x << "," << cam.origin.y << "," << cam.origin.z
        << "," << cam.yaw << "," << cam.pitch << "," << cam.distance << "\n";

    for (auto& [pos, blockName] : blocks.placed)
        out << "block=" << pos.x << "," << pos.y << "," << pos.z << "," << blockName << "\n";
}

struct SaveEntry {
    std::string filepath;
    std::string name;
    std::string type;
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

static void loadStructureInto(const std::string& filepath, std::string& name,
                               std::string& type, PlacedBlocks& blocks,
                               EditorCamera& cam) {
    std::ifstream in(filepath);
    if (!in.is_open()) return;

    blocks.placeholders.clear();
    blocks.placed.clear();
    blocks.modelSize = 0;
    blocks.removed.clear();

    bool cameraLoaded = false;

    std::string line;
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
        } else if (line.rfind("placeholder=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 12, "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                blocks.placeholders.push_back(p);
        } else if (line.rfind("scaffold=", 0) == 0) {
            Vec3i p;
            if (sscanf(line.c_str() + 9, "%d,%d,%d", &p.x, &p.y, &p.z) == 3) {
                // Legacy save: scaffold lines mean structure mode OR old model saves
                blocks.placeholders.push_back(p);
            }
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
        } else if (line.rfind("block=", 0) == 0) {
            std::string rest = line.substr(6);
            int commaCount = 0, lastComma = -1;
            for (int i = 0; i < (int)rest.size(); i++) {
                if (rest[i] == ',') { commaCount++; if (commaCount == 3) { lastComma = i; break; } }
            }
            if (lastComma > 0) {
                Vec3i p;
                if (sscanf(rest.c_str(), "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                    blocks.placed[p] = rest.substr(lastComma + 1);
            }
        }
    }

    if (blocks.placeholders.empty())
        blocks.placeholders.push_back({0, 0, 0});

    // Legacy save compatibility: if type is 3D Model but no modelsize line,
    // infer modelSize from the scaffold positions
    if (type == "3D Model" && blocks.modelSize == 0 && blocks.placeholders.size() > 1) {
        int maxCoord = 0;
        for (auto& p : blocks.placeholders) {
            maxCoord = std::max(maxCoord, std::max({std::abs(p.x), p.y, std::abs(p.z)}));
        }
        blocks.modelSize = maxCoord + 1;
        // Clear scaffolds since model mode doesn't use them
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
}

static std::map<Vec3i, std::string> loadImportBlocks(const std::string& filepath) {
    std::map<Vec3i, std::string> result;
    std::ifstream in(filepath);
    if (!in.is_open()) return result;

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("block=", 0) == 0) {
            std::string rest = line.substr(6);
            int commaCount = 0, lastComma = -1;
            for (int i = 0; i < (int)rest.size(); i++) {
                if (rest[i] == ',') { commaCount++; if (commaCount == 3) { lastComma = i; break; } }
            }
            if (lastComma > 0) {
                Vec3i p;
                if (sscanf(rest.c_str(), "%d,%d,%d", &p.x, &p.y, &p.z) == 3)
                    result[p] = rest.substr(lastComma + 1);
            }
        }
    }
    return result;
}

static std::vector<SaveEntry> listSaves() {
    std::vector<SaveEntry> entries;
    if (!fs::exists("saves")) return entries;
    for (auto& entry : fs::directory_iterator("saves")) {
        if (entry.path().extension() == ".sav") {
            entries.push_back(loadSaveHeader(entry.path().string()));
        }
    }
    return entries;
}
