#include "../VisualEngine/VisualEngine.h"
#include "src/Highlight.h"

int main() {
    VE::initWindow(800, 600, "Model Editor", true);
    VE::loadMeshDir("assets");
    VE::setMode(VE::SINGLE);

    VE::registerScene("editor",
        // onEnter
        []() {
            initHighlight();
            VE::setCamera(6, 4, 6, 210, -25);
            for (int x = 0; x < 3; x++)
                for (int y = 0; y < 3; y++)
                    for (int z = 0; z < 3; z++)
                        VE::draw("cube", x, y, z);
            VE::undraw(0, 0, 0);
        },
        // onExit
        []() {
            cleanupHighlight();
            VE::clearDraws();
        },
        // onInput
        nullptr,
        // onUpdate
        nullptr,
        // onRender
        renderHoverHighlight
    );

    VE::setScene("editor");
    VE::run();
    return 0;
}
