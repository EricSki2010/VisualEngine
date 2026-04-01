#pragma once

#include <glm/glm.hpp>
#include <vector>

struct SelectedFace {
    glm::ivec3 blockPos;
    int faceIndex; // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
};

void addSelectedFace(const glm::ivec3& pos, int faceIndex);
void removeSelectedFace(const glm::ivec3& pos, int faceIndex);
void clearSelection();
const std::vector<SelectedFace>& getSelection();
