#pragma once

#include "render.h"
#include "../inputManagement/Collision.h"

void initOverlay();
void cleanupOverlay();
void drawTriangleOverlay(Shader& shader, const Triangle& tri, const glm::vec3& color, float alpha, bool flatShade = true);

Triangle aabbFaceTriangle(const AABB& box, int faceIndex, int half);
