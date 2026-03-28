#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <cstdio>
#include <filesystem>
#include "engine/core/json.h"

// ── Base definition ─────────────────────────────────────────────────
// Every registry entry has at least these fields. Derived structs
// (BlockDef, ItemDef, etc.) add their own and override parseJson().

struct BaseDef {
    std::string name;
    std::string modelFile;
    std::string iconFile;

    Model model = { 0 };
    bool modelLoaded = false;

    Texture2D icon = { 0 };
    bool iconLoaded = false;

    // Override in derived types to parse type-specific fields.
    // Base fields (name, modelFile, iconFile) are parsed automatically.
    virtual void parseJson(const json::Object& obj) { (void)obj; }

    // Called right after a model is loaded. Override for auto-scaling etc.
    virtual void onModelLoaded() {}

    virtual ~BaseDef() = default;
};

// ── Generic registry ────────────────────────────────────────────────
// T must inherit from BaseDef.

template <typename T>
struct Registry {
    std::unordered_map<std::string, T> entries;
    const char* tag = "REGISTRY";  // log prefix

    ~Registry() { unloadAll(); }

    // Load all .json files from a folder
    void loadFolder(const std::string& folder) {
        if (!std::filesystem::exists(folder)) {
            printf("%s: Folder not found: %s\n", tag, folder.c_str());
            return;
        }
        int count = 0;
        for (auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".json") {
                loadFile(entry.path().string());
                count++;
            }
        }
        printf("%s: Loaded %d definitions from %s\n", tag, count, folder.c_str());
    }

    // Load a single definition file
    void loadFile(const std::string& path) {
        auto obj = json::parseFile(path);
        if (obj.kv.empty()) {
            printf("%s: Failed to parse %s\n", tag, path.c_str());
            return;
        }

        T def;
        def.name      = obj.str("name");
        def.modelFile = obj.str("modelFile", obj.str("model"));
        def.iconFile  = obj.str("icon");
        def.parseJson(obj);

        if (def.name.empty()) {
            std::filesystem::path p(path);
            def.name = p.stem().string();
        }

        auto it = entries.find(def.name);
        if (it != entries.end()) {
            if (it->second.modelLoaded) UnloadModel(it->second.model);
            if (it->second.iconLoaded) UnloadTexture(it->second.icon);
        }
        entries[def.name] = std::move(def);
    }

    const T* get(const std::string& name) const {
        auto it = entries.find(name);
        return it != entries.end() ? &it->second : nullptr;
    }

    bool has(const std::string& name) const {
        return entries.count(name) > 0;
    }

    // Generic auto-detect: iterates entries, calls detectFn to match files in folder.
    template<typename DetectFn>
    void autoDetectFiles(const std::string& folder, const char* assetLabel, DetectFn detectFn) {
        if (!std::filesystem::exists(folder)) return;
        int count = 0;
        for (auto& [name, def] : entries) {
            if (detectFn(name, def, folder)) count++;
        }
        if (count > 0) printf("%s: Auto-detected %d %s from %s\n", tag, count, assetLabel, folder.c_str());
    }

    // Auto-detect .glb models by name in a folder
    void autoDetectModels(const std::string& folder) {
        autoDetectFiles(folder, "models", [](const std::string& name, T& def, const std::string& dir) {
            if (!def.modelFile.empty()) return false;
            std::string path = dir + "/" + name + ".glb";
            if (!std::filesystem::exists(path)) return false;
            def.modelFile = path;
            return true;
        });
    }

    // Auto-detect .png/.jpg icons by name in a folder
    void autoDetectIcons(const std::string& folder) {
        autoDetectFiles(folder, "icons", [](const std::string& name, T& def, const std::string& dir) {
            if (!def.iconFile.empty()) return false;
            std::string path = dir + "/" + name + ".png";
            if (std::filesystem::exists(path)) { def.iconFile = path; return true; }
            path = dir + "/" + name + ".jpg";
            if (std::filesystem::exists(path)) { def.iconFile = path; return true; }
            return false;
        });
    }

    // Generic asset loader: iterates entries, checks file existence, calls loadFn.
    template<typename LoadFn>
    void loadAssetType(const char* assetLabel, LoadFn loadFn) {
        int count = 0;
        for (auto& [name, def] : entries) {
            if (loadFn(name, def)) count++;
        }
        if (count > 0) printf("%s: Loaded %d %s\n", tag, count, assetLabel);
    }

    // Load .glb models. Must be called after InitWindow().
    void loadModels() {
        loadAssetType("models", [](const std::string& name, T& def) {
            if (def.modelFile.empty() || def.modelLoaded) return false;
            if (!FileExists(def.modelFile.c_str())) {
                printf("WARNING: Model not found for '%s': %s\n", name.c_str(), def.modelFile.c_str());
                return false;
            }
            def.model = LoadModel(def.modelFile.c_str());
            def.modelLoaded = true;
            def.onModelLoaded();
            return true;
        });
    }

    // Load icon textures. Must be called after InitWindow().
    void loadIcons() {
        loadAssetType("icons", [](const std::string& name, T& def) {
            if (def.iconFile.empty() || def.iconLoaded) return false;
            if (!FileExists(def.iconFile.c_str())) {
                printf("WARNING: Icon not found for '%s': %s\n", name.c_str(), def.iconFile.c_str());
                return false;
            }
            def.icon = LoadTexture(def.iconFile.c_str());
            def.iconLoaded = true;
            return true;
        });
    }

    // Load both models and icons in one call
    void loadAssets() {
        loadModels();
        loadIcons();
    }

    void unloadModels() {
        for (auto& [name, def] : entries) {
            if (def.modelLoaded) { UnloadModel(def.model); def.modelLoaded = false; }
        }
    }

    void unloadIcons() {
        for (auto& [name, def] : entries) {
            if (def.iconLoaded) { UnloadTexture(def.icon); def.iconLoaded = false; }
        }
    }

    void unloadAll() { unloadModels(); unloadIcons(); }
};

// ── Block definition ────────────────────────────────────────────────

struct BlockDef : BaseDef {
    std::string formatType    = "cube";
    std::string collisionType = "solid";
    unsigned char r = 128, g = 128, b = 128, a = 255;
    std::string lootTable;

    float modelW = 1.0f, modelH = 1.0f, modelD = 1.0f;
    float modelScaleX = 1.0f, modelScaleY = 1.0f, modelScaleZ = 1.0f;
    float modelOffsetY = 0.0f;

    void parseJson(const json::Object& obj) override {
        formatType    = obj.str("formatType", "cube");
        collisionType = obj.str("collisionType", "solid");
        lootTable     = obj.str("lootTable");

        auto c = obj.intArray("color");
        if (c.size() >= 3) {
            r = (unsigned char)c[0];
            g = (unsigned char)c[1];
            b = (unsigned char)c[2];
            a = (c.size() >= 4) ? (unsigned char)c[3] : 255;
        }

        auto s = obj.floatArray("modelSize");
        if (s.size() >= 3) {
            modelW = s[0]; modelH = s[1]; modelD = s[2];
        } else if (s.size() == 1) {
            modelW = modelH = modelD = s[0];
        }
    }

    void onModelLoaded() override {
        formatType = "glb";
        BoundingBox bb = GetModelBoundingBox(model);
        float rawW = bb.max.x - bb.min.x;
        float rawH = bb.max.y - bb.min.y;
        float rawD = bb.max.z - bb.min.z;
        // Use uniform scale to preserve model proportions
        float maxRaw = rawW;
        if (rawH > maxRaw) maxRaw = rawH;
        if (rawD > maxRaw) maxRaw = rawD;
        float uniformScale = (maxRaw > 0.0f) ? modelW / maxRaw : 1.0f;
        modelScaleX = uniformScale;
        modelScaleY = uniformScale;
        modelScaleZ = uniformScale;
        float scaledMinY = bb.min.y * modelScaleY;
        modelOffsetY = -0.5f - scaledMinY;
    }
};

using BlockRegistry = Registry<BlockDef>;

// ── Item definition ─────────────────────────────────────────────────

struct ItemDef : BaseDef {
    // Add item-specific fields here as needed
    void parseJson(const json::Object& obj) override { (void)obj; }
};

using ItemRegistry = Registry<ItemDef>;
