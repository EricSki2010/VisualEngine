#include "templates.h"
#include "3D_modeler/prefab_cube.h"

ModelFile cubeTemplate(int size) {
    if (size < 1) size = 1;
    if (size > 16) size = 16;

    ModelFile model;

    BlockTypeDef cube;
    cube.name = "cube";
    cube.vertexCount = 24;
    cube.indexCount = 36;
    cube.vertices.assign(cubeVertices, cubeVertices + 24 * 5);
    cube.indices.assign(cubeIndices, cubeIndices + 36);
    cube.faceColors.resize(12);
    model.blockTypes.push_back(cube);

    for (int x = 0; x < size; x++)
        for (int y = 0; y < size; y++)
            for (int z = 0; z < size; z++)
                model.placements.push_back({x, y, z, 0, 0, 0, 0});

    return model;
}
