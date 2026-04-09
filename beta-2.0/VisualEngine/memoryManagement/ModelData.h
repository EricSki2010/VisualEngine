#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct FaceColor {
    glm::vec3 color = glm::vec3(0.8f); // default grey
};

struct BlockTypeDef {
    std::string name;
    std::vector<float> vertices;     // pos3+uv2 (5) or pos3+uv2+normal3 (8) per vertex
    int vertexCount = 0;
    int floatsPerVertex = 5;
    std::vector<unsigned int> indices;
    int indexCount = 0;
    std::vector<FaceColor> faceColors; // one per triangle (indexCount / 3)
};

struct BlockPlacement {
    int x, y, z;
    int typeId;
    int rx, ry, rz;
    std::vector<int8_t> triColors; // palette index per triangle (-1 = unpainted)
};

struct ModelFile {
    std::vector<BlockTypeDef> blockTypes;
    std::vector<BlockPlacement> placements;
    glm::vec3 palette[16] = {
        {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
        {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
        {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
        {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f}, {0.6f, 0.6f, 0.6f},
    };
};
