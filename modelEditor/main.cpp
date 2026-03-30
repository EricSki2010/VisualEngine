#include "../VisualEngine/VisualEngine.h"
#include "src/Highlight.h"

int main() {
    VE::initWindow(800, 600, "Model Editor");
    // initHighlight();
    // VE::setPostRenderCallback(renderHoverHighlight);
    VE::loadMesh("cube", "assets/cube.mesh");
    VE::setMode(VE::SINGLE);
    VE::setCamera(6, 4, 6, 210, -25);

    for (int x = 0; x < 3; x++)
        for (int y = 0; y < 3; y++)
            for (int z = 0; z < 3; z++)
                VE::draw("cube", x, y, z);

    VE::run();
    // cleanupHighlight();
    return 0;
}