#include "../VisualEngine/VisualEngine.h"
#include "src/scenes.h"

int main() {
    VE::initWindow(800, 600, "Model Editor", true);
    VE::loadMeshDir("assets");
    VE::setMode(VE::SINGLE);

    registerScenes();
    VE::setScene("menu");
    VE::run();
    return 0;
}
