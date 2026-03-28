#pragma once
#include "raylib.h"
#include "engine/gameplay/inventory.h"
#include <string>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>

// ── Save system ─────────────────────────────────────────────────────
// Binary save/load for all engine state: world tiles, player, inventory, hotbar.
// Include this BEFORE engine.h or let engine.h include it.
//
// File format (v1):
//   Header:
//     [4 bytes]  magic "EV2S"
//     [uint32]   version
//     [int32]    seed (game-provided, engine just stores it)
//   Player:
//     [float x3] position (x, y, z)
//     [float]    facing angle
//   Inventory:
//     [uint32]   slot count
//     per slot:  [uint16 nameLen] [chars name] [int32 count]
//   HotBar:
//     [uint32]   slot count
//     [int32]    selected index
//     per slot:  [uint16 nameLen] [chars name] [int32 count]
//   World:
//     [uint32]   tile count
//     per tile:
//       [int32 x] [int32 z] [int32 type] [float height]
//       [uchar r,g,b,a] [uint16 nameLen] [chars blockName]

constexpr uint32_t SAVE_MAGIC   = 0x53325645; // "EV2S"
constexpr uint32_t SAVE_VERSION = 2;

namespace _save {

inline void w(std::ofstream& f, const void* d, size_t n) {
    f.write(reinterpret_cast<const char*>(d), n);
}

inline void r(std::ifstream& f, void* d, size_t n) {
    f.read(reinterpret_cast<char*>(d), n);
    if (!f.good()) std::memset(d, 0, n);
}

inline void wStr(std::ofstream& f, const std::string& s) {
    uint16_t len = (uint16_t)s.size();
    w(f, &len, 2);
    if (len > 0) w(f, s.data(), len);
}

inline std::string rStr(std::ifstream& f) {
    uint16_t len = 0;
    r(f, &len, 2);
    if (len == 0) return "";
    std::string s(len, '\0');
    r(f, s.data(), len);
    return s;
}

inline void wVec3(std::ofstream& f, const Vector3& v) {
    w(f, &v.x, sizeof(float));
    w(f, &v.y, sizeof(float));
    w(f, &v.z, sizeof(float));
}

inline Vector3 rVec3(std::ifstream& f) {
    Vector3 v{};
    r(f, &v.x, sizeof(float));
    r(f, &v.y, sizeof(float));
    r(f, &v.z, sizeof(float));
    return v;
}

inline void wColor(std::ofstream& f, unsigned char r_, unsigned char g_, unsigned char b_, unsigned char a_) {
    w(f, &r_, 1); w(f, &g_, 1); w(f, &b_, 1); w(f, &a_, 1);
}

inline void rColor(std::ifstream& f, unsigned char& r_, unsigned char& g_, unsigned char& b_, unsigned char& a_) {
    r(f, &r_, 1); r(f, &g_, 1); r(f, &b_, 1); r(f, &a_, 1);
}

// Write/read just the slot data (no count header)
inline void wSlotData(std::ofstream& f, const std::vector<ItemSlot>& slots) {
    for (auto& slot : slots) {
        wStr(f, slot.itemName);
        int32_t c = slot.count;
        w(f, &c, 4);
    }
}

inline void rSlotData(std::ifstream& f, std::vector<ItemSlot>& slots, uint32_t count, int maxStack = 64) {
    for (uint32_t i = 0; i < count; i++) {
        slots[i].itemName = rStr(f);
        int32_t c = 0;
        r(f, &c, 4);
        if (c < 0) c = 0;
        if (c > maxStack) c = maxStack;
        slots[i].count = c;
    }
}

// Write/read count header + slot data
inline void wSlots(std::ofstream& f, const std::vector<ItemSlot>& slots) {
    uint32_t count = (uint32_t)slots.size();
    w(f, &count, 4);
    wSlotData(f, slots);
}

inline void rSlots(std::ifstream& f, std::vector<ItemSlot>& slots, int& slotCount, uint32_t maxSlots, int maxStack = 64) {
    uint32_t count = 0;
    r(f, &count, 4);
    if (count > maxSlots) count = maxSlots;
    slots.resize(count);
    slotCount = (int)count;
    rSlotData(f, slots, count, maxStack);
}

} // namespace _save
