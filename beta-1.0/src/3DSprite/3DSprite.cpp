#include "engine/engine.h"

int main() {
    Engine engine;
    engine.init("3D Sprite");
    engine.setView(VIEW_2_5D);
    engine.setSpawn(0, 1, 0);

    engine.run();
}
