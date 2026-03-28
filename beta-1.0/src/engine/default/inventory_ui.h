#pragma once
#include "raylib.h"
#include "engine/gameplay/inventory.h"
#include <functional>

// ── UI callbacks ─────────────────────────────────────────────────
using SlotDrawFn = std::function<void(const ItemSlot& slot, float x, float y, float size, bool selected)>;
using IconLookupFn = std::function<const Texture2D*(const std::string& name)>;

// ── Default slot drawing ─────────────────────────────────────────

inline void drawSlotDefault(const ItemSlot& slot, float x, float y, float size, bool selected,
                            const IconLookupFn& iconLookup = nullptr) {
    Color bg = selected ? Color{ 80, 80, 120, 220 } : Color{ 40, 40, 40, 200 };
    DrawRectangle((int)x, (int)y, (int)size, (int)size, bg);

    Color border = selected ? Color{ 200, 200, 255, 255 } : Color{ 100, 100, 100, 255 };
    DrawRectangleLines((int)x, (int)y, (int)size, (int)size, border);

    if (!slot.empty()) {
        int fontSize = (int)(size * 0.25f);
        if (fontSize < 8) fontSize = 8;

        const Texture2D* icon = iconLookup ? iconLookup(slot.itemName) : nullptr;
        if (icon && icon->id > 0) {
            float pad = size * 0.1f;
            float iconSize = size - pad * 2;
            Rectangle src = { 0, 0, (float)icon->width, (float)icon->height };
            Rectangle dst = { x + pad, y + pad, iconSize, iconSize };
            DrawTexturePro(*icon, src, dst, { 0, 0 }, 0.0f, WHITE);
        } else {
            int maxChars = (int)(size / (fontSize * 0.6f));
            std::string display = slot.itemName;
            if ((int)display.size() > maxChars) display = display.substr(0, maxChars);
            DrawText(display.c_str(), (int)x + 3, (int)y + 3, fontSize, WHITE);
        }

        if (slot.count > 1) {
            const char* countText = TextFormat("%d", slot.count);
            int countWidth = MeasureText(countText, fontSize);
            DrawText(countText, (int)(x + size) - countWidth - 3, (int)(y + size) - fontSize - 3, fontSize, LIGHTGRAY);
        }
    }
}

// Draw the cursor item at the mouse position
inline void drawCursorItem(const CursorItem& cursor, const IconLookupFn& iconLookup = nullptr) {
    if (!cursor.holding()) return;

    Vector2 mouse = GetMousePosition();
    float size = 40.0f;
    float x = mouse.x - size / 2;
    float y = mouse.y - size / 2;

    const Texture2D* icon = iconLookup ? iconLookup(cursor.held.itemName) : nullptr;
    if (icon && icon->id > 0) {
        Rectangle src = { 0, 0, (float)icon->width, (float)icon->height };
        Rectangle dst = { x, y, size, size };
        DrawTexturePro(*icon, src, dst, { 0, 0 }, 0.0f, WHITE);
    } else {
        int fontSize = 12;
        DrawText(cursor.held.itemName.c_str(), (int)x, (int)y, fontSize, WHITE);
    }

    if (cursor.held.count > 1) {
        const char* countText = TextFormat("%d", cursor.held.count);
        int countWidth = MeasureText(countText, 12);
        DrawText(countText, (int)(x + size) - countWidth, (int)(y + size) - 12, 12, YELLOW);
    }
}

// ── Shift-click transfer helper ─────────────────────────────────
// Moves an item from src slot into dst slots (stack first, then empties).

inline void shiftClickTransfer(ItemSlot& src, std::vector<ItemSlot>& dstSlots, int maxStack = 64) {
    stackThenFill(src, dstSlots, maxStack);
}

// ── Slot placement ──────────────────────────────────────────────

struct SlotPlacement {
    float x = 0, y = 0;   // px offset from top-left of background/panel
    float size = 48.0f;
    bool placed = false;
};

// ── Custom Panel UI ─────────────────────────────────────────────
// A background image with manually placed slots. Works for both
// inventory and hotbar — just point it at the right data.
//
// Usage:
//   panel.setBackground("assets/ui/my_inventory.png");
//   panel.setSlot(0, 10, 30, 48);
//   panel.setSlot(1, 70, 30, 48);

struct CustomPanelUI {
    Texture2D background = {};
    std::vector<SlotPlacement> placements;

    std::vector<ItemSlot>* slots = nullptr;
    int* slotCount = nullptr;
    CursorItem* cursor = nullptr;
    int* selectedIndex = nullptr;  // for hotbar highlight (nullptr = no highlight)
    bool* openFlag = nullptr;      // if set, panel only draws/updates when *openFlag == true

    SlotDrawFn onDrawSlot = nullptr;
    IconLookupFn iconLookup = nullptr;

    // Called before slots are drawn, with the panel's screen-space origin and size.
    // Use this to draw a background rect, title, etc.
    std::function<void(float x, float y, float w, float h)> onDrawBackground = nullptr;

    // Called after each slot is drawn, with slot index and screen-space position.
    // Use this for hotbar number labels, tooltips, etc.
    std::function<void(int index, float x, float y, float size, bool selected)> onDrawSlotOverlay = nullptr;

    // Screen position (centered on screen by default)
    bool centerOnScreen = true;
    float screenX = 0, screenY = 0;

    void setBackground(const char* path) {
        if (background.id > 0) UnloadTexture(background);
        background = LoadTexture(path);
    }

    void clearBindings() {
        slots = nullptr;
        slotCount = nullptr;
        cursor = nullptr;
        selectedIndex = nullptr;
        openFlag = nullptr;
    }

    void setSlot(int index, float x, float y, float size = 48.0f) {
        if (index < 0) return;
        if (index >= (int)placements.size()) placements.resize(index + 1);
        placements[index] = { x, y, size, true };
    }

    float bgX() const {
        if (centerOnScreen && background.id > 0)
            return (GetScreenWidth() - (float)background.width) / 2.0f;
        return screenX;
    }
    float bgY() const {
        if (centerOnScreen && background.id > 0)
            return (GetScreenHeight() - (float)background.height) / 2.0f;
        return screenY;
    }

    int hitTest(float mouseX, float mouseY) const {
        float ox = bgX(), oy = bgY();
        int count = slotCount ? *slotCount : (int)placements.size();
        for (int i = 0; i < count && i < (int)placements.size(); i++) {
            if (!placements[i].placed) continue;
            float sx = ox + placements[i].x;
            float sy = oy + placements[i].y;
            float sz = placements[i].size;
            if (mouseX >= sx && mouseX < sx + sz &&
                mouseY >= sy && mouseY < sy + sz)
                return i;
        }
        return -1;
    }

    void update() {
        if (!slots || !cursor) return;
        if (openFlag && !*openFlag) return;

        bool leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool rightClick = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

        if (leftClick || rightClick) {
            Vector2 mouse = GetMousePosition();
            int hit = hitTest(mouse.x, mouse.y);
            if (hit >= 0 && hit < (int)slots->size()) {
                bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if (!(shiftHeld && leftClick)) {
                    handleSlotClick((*slots)[hit], *cursor, leftClick, rightClick);
                }
            }
        }
    }

    void draw() const {
        if (!slots) return;
        if (openFlag && !*openFlag) return;

        float ox = bgX(), oy = bgY();

        // Background image or custom callback
        if (background.id > 0) {
            DrawTexture(background, (int)ox, (int)oy, WHITE);
        }
        if (onDrawBackground) {
            // Compute bounding size from placements
            float w = 0, h = 0;
            int count = slotCount ? *slotCount : (int)placements.size();
            for (int i = 0; i < count && i < (int)placements.size(); i++) {
                if (!placements[i].placed) continue;
                float r = placements[i].x + placements[i].size;
                float b = placements[i].y + placements[i].size;
                if (r > w) w = r;
                if (b > h) h = b;
            }
            onDrawBackground(ox, oy, w, h);
        }

        // Draw slots
        int count = slotCount ? *slotCount : (int)placements.size();
        for (int i = 0; i < count && i < (int)placements.size(); i++) {
            if (!placements[i].placed) continue;
            if (i >= (int)slots->size()) break;

            float sx = ox + placements[i].x;
            float sy = oy + placements[i].y;
            float sz = placements[i].size;
            bool isSel = selectedIndex && (*selectedIndex == i);

            if (onDrawSlot) {
                onDrawSlot((*slots)[i], sx, sy, sz, isSel);
            } else {
                drawSlotDefault((*slots)[i], sx, sy, sz, isSel, iconLookup);
            }

            if (onDrawSlotOverlay) {
                onDrawSlotOverlay(i, sx, sy, sz, isSel);
            }
        }
    }

    void unload() {
        if (background.id > 0) {
            UnloadTexture(background);
            background = {};
        }
    }
};

// ── Inventory UI ─────────────────────────────────────────────────
// Auto-grid wrapper around CustomPanelUI.

struct InventoryUI {
    Inventory* inventory = nullptr;
    SlotDrawFn onDrawSlot = nullptr;
    IconLookupFn iconLookup = nullptr;
    CustomPanelUI panel;

    ~InventoryUI() { panel.clearBindings(); }

    static constexpr float kSlotSize = 56.0f;
    static constexpr float kPadding = 4.0f;

    float gridW() const {
        if (!inventory) return 0.0f;
        return inventory->columns * (kSlotSize + kPadding) + kPadding;
    }
    float gridH() const {
        if (!inventory) return 0.0f;
        int rows = (inventory->slotCount + inventory->columns - 1) / inventory->columns;
        return rows * (kSlotSize + kPadding) + kPadding + 28.0f;
    }

    void rebuildPlacements() {
        if (!inventory) return;

        panel.placements.resize(inventory->slotCount);
        for (int i = 0; i < inventory->slotCount; i++) {
            int col = i % inventory->columns;
            int row = i / inventory->columns;
            panel.placements[i] = {
                kPadding + col * (kSlotSize + kPadding),
                28.0f + kPadding + row * (kSlotSize + kPadding),
                kSlotSize, true
            };
        }

        panel.slots = &inventory->slots;
        panel.slotCount = &inventory->slotCount;
        panel.cursor = inventory->cursor;
        panel.openFlag = &inventory->open;
        panel.onDrawSlot = onDrawSlot;
        panel.iconLookup = iconLookup;
        panel.centerOnScreen = false;
        panel.screenX = (GetScreenWidth() - gridW()) / 2.0f;
        panel.screenY = (GetScreenHeight() - gridH()) / 2.0f;

        panel.onDrawBackground = [](float x, float y, float w, float h) {
            DrawRectangle((int)x, (int)y, (int)w, (int)h, { 20, 20, 20, 230 });
            DrawRectangleLines((int)x, (int)y, (int)w, (int)h, { 100, 100, 100, 255 });
            DrawText("Inventory", (int)x + 6, (int)y + 4, 20, WHITE);
        };
    }

    void update() {
        if (!inventory || !inventory->open || !inventory->cursor) return;
        rebuildPlacements();
        panel.update();
    }

    void draw() {
        if (!inventory || !inventory->open || inventory->slotCount == 0) return;
        rebuildPlacements();
        panel.draw();
    }
};

// ── HotBar UI ────────────────────────────────────────────────────
// Auto-row wrapper around CustomPanelUI.

struct HotBarUI {
    HotBar* hotbar = nullptr;
    SlotDrawFn onDrawSlot = nullptr;
    IconLookupFn iconLookup = nullptr;
    CustomPanelUI panel;

    ~HotBarUI() { panel.clearBindings(); }

    static constexpr float kSlotSize = 52.0f;
    static constexpr float kPadding = 4.0f;

    float totalW() const {
        if (!hotbar) return 0.0f;
        return hotbar->slotCount * (kSlotSize + kPadding) + kPadding;
    }

    void rebuildPlacements() {
        if (!hotbar) return;

        panel.placements.resize(hotbar->slotCount);
        for (int i = 0; i < hotbar->slotCount; i++) {
            panel.placements[i] = {
                kPadding + i * (kSlotSize + kPadding), 0,
                kSlotSize, true
            };
        }

        panel.slots = &hotbar->slots;
        panel.slotCount = &hotbar->slotCount;
        panel.cursor = hotbar->cursor;
        panel.selectedIndex = &hotbar->selected;
        panel.onDrawSlot = onDrawSlot;
        panel.iconLookup = iconLookup;
        panel.centerOnScreen = false;
        panel.screenX = (GetScreenWidth() - totalW()) / 2.0f;
        panel.screenY = GetScreenHeight() - kSlotSize - kPadding * 2.0f;

        panel.onDrawBackground = [this](float x, float y, float w, float h) {
            DrawRectangle((int)(x - kPadding), (int)(y - kPadding),
                          (int)(w + kPadding * 2), (int)(h + kPadding * 3),
                          { 20, 20, 20, 200 });
        };

        panel.onDrawSlotOverlay = [this](int i, float x, float y, float sz, bool sel) {
            const char* label = (i < 9) ? TextFormat("%d", i + 1) : "0";
            int labelW = MeasureText(label, 14);
            DrawText(label, (int)(x + sz / 2 - labelW / 2), (int)(y - 16), 14,
                     sel ? WHITE : Color{ 150, 150, 150, 255 });
        };
    }

    void update() {
        if (!hotbar || !hotbar->inventoryOpen || !hotbar->cursor) return;
        rebuildPlacements();
        panel.update();
    }

    void draw() {
        if (!hotbar || hotbar->slotCount == 0) return;
        rebuildPlacements();
        panel.draw();
    }
};

// ── Inventory Manager UI ─────────────────────────────────────────
// Wraps InventoryManager with UI for both inventory and hotbar.
// Handles drawing, click interactions, and shift-click transfers.
// Works the same whether the panels use auto-grid or custom layouts.

struct InventoryManagerUI {
    InventoryManager* manager = nullptr;
    InventoryUI invUI;
    HotBarUI hbUI;
    IconLookupFn iconLookup = nullptr;

    void init(InventoryManager* mgr) {
        manager = mgr;
        if (manager) {
            invUI.inventory = manager->inventory;
            hbUI.hotbar = manager->hotbar;
        }
    }

    void update() {
        if (!manager || !manager->inventory || !manager->hotbar) return;

        invUI.rebuildPlacements();
        hbUI.rebuildPlacements();
        invUI.update();
        hbUI.update();

        // Shift-click transfers
        if (manager->inventory->open) {
            bool leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

            if (leftClick && shiftHeld) {
                Vector2 mouse = GetMousePosition();

                int invHit = invUI.panel.hitTest(mouse.x, mouse.y);
                if (invHit >= 0 && invHit < (int)manager->inventory->slots.size()
                    && !manager->inventory->slots[invHit].empty()) {
                    shiftClickTransfer(manager->inventory->slots[invHit], manager->hotbar->slots);
                    return;
                }

                int hbHit = hbUI.panel.hitTest(mouse.x, mouse.y);
                if (hbHit >= 0 && hbHit < (int)manager->hotbar->slots.size()
                    && !manager->hotbar->slots[hbHit].empty()) {
                    shiftClickTransfer(manager->hotbar->slots[hbHit], manager->inventory->slots);
                }
            }
        }
    }

    void draw() {
        if (!manager || !manager->inventory || !manager->hotbar) return;
        hbUI.draw();
        invUI.draw();
        drawCursorItem(manager->cursor, iconLookup);
    }
};
