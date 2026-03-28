#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include <functional>

// ── Item slot ──────────────────────────────────────────────────────
// A single slot in an inventory or hotbar.

struct ItemSlot {
    std::string itemName;  // block/item registry name (empty = empty slot)
    int count = 0;

    bool empty() const { return itemName.empty() || count <= 0; }

    void clear() {
        itemName.clear();
        count = 0;
    }

    // Transfer as many items as possible from src into this slot.
    // Items must already match (caller checks). Returns the number transferred.
    static int stackItems(ItemSlot& src, ItemSlot& dst, int maxStack = 64) {
        if (src.empty() || dst.itemName != src.itemName || dst.count >= maxStack) return 0;
        int space = maxStack - dst.count;
        int toMove = (src.count < space) ? src.count : space;
        dst.count += toMove;
        src.count -= toMove;
        if (src.count <= 0) src.clear();
        return toMove;
    }
};

// Stack src into matching slots, then fill empties. Returns leftover amount.
inline int stackThenFill(ItemSlot& src, std::vector<ItemSlot>& slots, int maxStack = 64) {
    for (auto& slot : slots) {
        if (src.empty()) return 0;
        ItemSlot::stackItems(src, slot, maxStack);
    }
    for (auto& slot : slots) {
        if (src.empty()) return 0;
        if (slot.empty()) {
            int toAdd = (src.count < maxStack) ? src.count : maxStack;
            slot.itemName = src.itemName;
            slot.count = toAdd;
            src.count -= toAdd;
            if (src.count <= 0) { src.clear(); return 0; }
        }
    }
    return src.count;
}

// ── Cursor item ──────────────────────────────────────────────────
// The item currently held by the mouse cursor.

struct CursorItem {
    ItemSlot held;       // what's on the cursor
    ItemSlot stashed;    // backup if inventory closes with no room

    bool holding() const { return !held.empty(); }

    // Try to return held item to an inventory's slots.
    // If no room, move to stash so it persists across open/close.
    void returnToSlots(std::vector<ItemSlot>& slots, int maxStack = 64) {
        if (held.empty()) return;
        stackThenFill(held, slots, maxStack);
        if (!held.empty()) {
            stashed = held;
            held.clear();
        }
    }

    // Restore stashed item to cursor when inventory reopens
    void restoreStash() {
        if (!stashed.empty() && held.empty()) {
            held = stashed;
            stashed.clear();
        }
    }
};

// ── Slot click logic ─────────────────────────────────────────────
// Handles left click, right click on a slot with the cursor item.

inline void handleSlotClick(ItemSlot& slot, CursorItem& cursor, bool leftClick, bool rightClick,
                            int maxStack = 64) {
    if (leftClick) {
        if (cursor.held.empty() && !slot.empty()) {
            // Pick up the whole stack
            cursor.held = slot;
            slot.clear();
        } else if (!cursor.held.empty() && slot.empty()) {
            // Place the whole stack
            slot = cursor.held;
            cursor.held.clear();
        } else if (!cursor.held.empty() && !slot.empty()) {
            if (cursor.held.itemName == slot.itemName) {
                // Stack: fill the slot from cursor
                ItemSlot::stackItems(cursor.held, slot, maxStack);
            } else {
                // Swap
                ItemSlot temp = slot;
                slot = cursor.held;
                cursor.held = temp;
            }
        }
    }

    if (rightClick) {
        if (cursor.held.empty() && !slot.empty()) {
            // Pick up half the stack (round up for cursor, round down stays)
            int half = (slot.count + 1) / 2;
            cursor.held.itemName = slot.itemName;
            cursor.held.count = half;
            slot.count -= half;
            if (slot.count <= 0) slot.clear();
        } else if (!cursor.held.empty() && (slot.empty() || slot.itemName == cursor.held.itemName)) {
            // Drop 1 item from cursor into slot
            if (slot.empty()) {
                slot.itemName = cursor.held.itemName;
                slot.count = 0;
            }
            if (slot.count < maxStack) {
                slot.count += 1;
                cursor.held.count -= 1;
                if (cursor.held.count <= 0) cursor.held.clear();
            }
        } else if (!cursor.held.empty() && !slot.empty() && cursor.held.itemName != slot.itemName) {
            // Swap on right click too when different items
            ItemSlot temp = slot;
            slot = cursor.held;
            cursor.held = temp;
        }
    }
}

// ── Shared slot accessor ──────────────────────────────────────────
// Bounds-checked slot access for any type with slots vector + slotCount.

template<typename T>
ItemSlot* getSlotSafe(T& container, int index) {
    if (index < 0 || index >= container.slotCount) return nullptr;
    return &container.slots[index];
}

template<typename T>
const ItemSlot* getSlotSafe(const T& container, int index) {
    if (index < 0 || index >= container.slotCount) return nullptr;
    return &container.slots[index];
}

// ── Inventory ──────────────────────────────────────────────────────

struct Inventory {
    std::vector<ItemSlot> slots;
    int slotCount = 0;
    int columns = 8;
    KeyboardKey toggleKey = KEY_NULL;
    bool open = false;
    bool wasOpen = false;  // tracks open/close transitions

    // Shared cursor — set this to the same CursorItem for inventory + hotbar
    CursorItem* cursor = nullptr;

    void init(int count, KeyboardKey keybind = KEY_E) {
        slotCount = count;
        toggleKey = keybind;
        slots.resize(count);
    }

    void update() {
        if (toggleKey != KEY_NULL && IsKeyPressed(toggleKey)) {
            open = !open;
        }

        // Handle open/close transitions
        if (open && !wasOpen && cursor) {
            cursor->restoreStash();
        }
        if (!open && wasOpen && cursor) {
            cursor->returnToSlots(slots);
        }
        wasOpen = open;
    }

    int addItem(const std::string& name, int amount = 1, int maxStack = 64) {
        ItemSlot src{name, amount};
        return stackThenFill(src, slots, maxStack);
    }

    int removeItem(const std::string& name, int amount = 1) {
        int removed = 0;
        for (auto& slot : slots) {
            if (amount <= 0) break;
            if (slot.itemName == name) {
                int toRemove = (amount < slot.count) ? amount : slot.count;
                slot.count -= toRemove;
                amount -= toRemove;
                removed += toRemove;
                if (slot.count <= 0) slot.clear();
            }
        }
        return removed;
    }

    int countItem(const std::string& name) const {
        int total = 0;
        for (auto& slot : slots) {
            if (slot.itemName == name) total += slot.count;
        }
        return total;
    }

    bool hasSpace(const std::string& name, int maxStack = 64) const {
        for (auto& slot : slots) {
            if (slot.empty()) return true;
            if (slot.itemName == name && slot.count < maxStack) return true;
        }
        return false;
    }

    ItemSlot* getSlot(int index) { return getSlotSafe(*this, index); }
    const ItemSlot* getSlot(int index) const { return getSlotSafe(*this, index); }
};

// ── HotBar ─────────────────────────────────────────────────────────

struct HotBar {
    std::vector<ItemSlot> slots;
    int slotCount = 0;
    int selected = 0;

    // Shared cursor
    CursorItem* cursor = nullptr;

    // Whether the inventory is open (set by InventoryManager so hotbar
    // knows whether to allow click interactions)
    bool inventoryOpen = false;

    void init(int count) {
        slotCount = (count > 10) ? 10 : count;
        slots.resize(slotCount);
    }

    void update() {
        // Number keys only when inventory is closed
        if (!inventoryOpen) {
            int count = (int)slots.size();
            for (int i = 0; i < count && i < 9; i++) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    selected = i;
                    return;
                }
            }
            if (count >= 10 && IsKeyPressed(KEY_ZERO)) {
                selected = 9;
            }

            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f && count > 0) {
                selected -= (int)wheel;
                if (selected < 0) selected = count - 1;
                if (selected >= count) selected = 0;
            }
        }
    }

    ItemSlot* getSelected() {
        if (selected >= slots.size()) return nullptr;
        return &slots[selected];
    }

    const ItemSlot* getSelected() const {
        if (selected >= slots.size()) return nullptr;
        return &slots[selected];
    }

    bool setSlot(int index, const std::string& name, int count = 1) {
        if (index < 0 || index >= slotCount) return false;
        slots[index].itemName = name;
        slots[index].count = count;
        return true;
    }

    ItemSlot* getSlot(int index) { return getSlotSafe(*this, index); }
    const ItemSlot* getSlot(int index) const { return getSlotSafe(*this, index); }
};

// ── Inventory Manager ──────────────────────────────────────────────
// Ties inventory + hotbar + cursor together and handles shift-click
// transfers between them.

struct InventoryManager {
    Inventory* inventory = nullptr;
    HotBar* hotbar = nullptr;
    CursorItem cursor;

    void init(Inventory* inv, HotBar* hb) {
        inventory = inv;
        hotbar = hb;
        if (inventory) inventory->cursor = &cursor;
        if (hotbar) hotbar->cursor = &cursor;
    }

    void update() {
        if (!inventory || !hotbar) return;

        // Sync state
        hotbar->inventoryOpen = inventory->open;

        // Update both
        inventory->update();
        hotbar->update();
    }
};
