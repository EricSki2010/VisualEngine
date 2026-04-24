#include "../VisualEngine/VisualEngine.h"
#include "src/mechanics/scenes.h"
#include "src/mechanics/setup.h"
#include "src/mechanics/AiHandling/AiHandling.h"

int main() {
    setupDirectories();
    setupAiDependencies();
    VE::initWindow(800, 600, "Model Editor", true);
    VE::loadMeshDir("assets");
    VE::setMode(VE::SINGLE);

    registerScenes();
    AI::init();
    VE::setScene("menu");
    VE::run();
    AI::shutdown();
    return 0;
}
