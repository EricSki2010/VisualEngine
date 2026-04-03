#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct FaceColor {
    glm::vec3 color = glm::vec3(0.8f); // default grey
};

struct BlockTypeDef {
    std::string name;
    std::vector<float> vertices;     // position3 + uv2 per vertex
    int vertexCount = 0;
    std::vector<unsigned int> indices;
    int indexCount = 0;
    std::vector<FaceColor> faceColors; // one per triangle (indexCount / 3)
};

struct BlockPlacement {
    int x, y, z;
    int typeId;
    int rx, ry, rz;
};

struct ModelFile {
    std::vector<BlockTypeDef> blockTypes;
    std::vector<BlockPlacement> placements;
};
