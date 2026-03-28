#include "engine/engine.h"
#include "engine/default/inventory_ui.h"
#include <filesystem>

int main() {
    Engine engine;
    engine.init("Engine Test");
    engine.loadBlocks("assets/entities/blocks");
    engine.loadItems("assets/entities/items");
    engine.setView(VIEW_2_5D);
    engine.setSpawn(0, 2, 0);

    // Ground using registry name
    for (int x = -10; x <= 10; x++)
        for (int z = -10; z <= 10; z++)
            engine.placeBlock("grassBlock", x, z);

    // Stairs using registry name
    for (int i = 0; i < 5; i++)
        engine.placeBlock("stoneBlock", 3 + i, 0, 0.25f * (i + 1));

    // Wall using raw RGB (no json needed)
    for (int z = -3; z <= 3; z++)
        engine.placeBlock(180, 80, 80, -5, z, 1.0f);

    // This will print a warning but not crash
    engine.placeBlock("diamondBlock", 0, 5);

    // Inventory: 24 slots, toggle with E
    engine.inventory.init(24, KEY_E);

    // Hotbar: 5 slots, selected with 1-5 / mouse wheel
    engine.hotbar.init(5);

    // Icon lookup — checks block registry first, then item registry
    auto iconLookup = [&](const std::string& name) -> const Texture2D* {
        const BlockDef* block = engine.registry.get(name);
        if (block && block->iconLoaded) return &block->icon;
        const ItemDef* item = engine.itemRegistry.get(name);
        if (item && item->iconLoaded) return &item->icon;
        return nullptr;
    };

    // Inventory manager + UI
    InventoryManager invManager;
    invManager.init(&engine.inventory, &engine.hotbar);

    InventoryManagerUI invUI;
    invUI.init(&invManager);
    invUI.iconLookup = iconLookup;

    // Pre-fill some items
    engine.inventory.addItem("grassBlock", 32);
    engine.inventory.addItem("stoneBlock", 16);
    engine.inventory.addItem("waterBlock", 8);

    engine.hotbar.setSlot(0, "grassBlock", 10);
    engine.hotbar.setSlot(1, "stoneBlock", 5);

    // Update inventory manager each frame
    engine.onUpdate = [&](float dt) {
        invManager.update();
        invUI.update();

        // F5 to save, F9 to load
        if (IsKeyPressed(KEY_F5)) {
            std::filesystem::create_directories("saves");
            engine.save("saves/test.sav");
        }
        if (IsKeyPressed(KEY_F9)) engine.load("saves/test.sav");
    };

    // Draw UI (always on top)
    engine.onDrawUI = [&]() {
        invUI.draw();
        DrawText("F5 Save  |  F9 Load", 10, 60, 18, GRAY);
    };

    engine.run();
}
