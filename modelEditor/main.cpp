#include "cubeData.h"

int main() {
    VE::initWindow(800, 600, "Model Editor");
    VE::loadMesh("cube", cubeMesh);
    VE::setCamera(6, 4, 6, 210, -25);

    for (int x = 0; x < 3; x++)
        for (int y = 0; y < 3; y++)
            for (int z = 0; z < 3; z++)
                VE::draw("cube", x, y, z);

    VE::run();
    return 0;
}
