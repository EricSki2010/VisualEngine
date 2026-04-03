#include "../VisualEngine/VisualEngine.h"
#include "src/mechanics/scenes.h"
#include "src/mechanics/setup.h"

int main() {
    setupDirectories();
    VE::initWindow(800, 600, "Model Editor", true);
    VE::loadMeshDir("assets");
    VE::setMode(VE::SINGLE);

    registerScenes();
    VE::setScene("menu");
    VE::run();
    return 0;
}
