#pragma once

#include <glm/glm.hpp>

void initDotRenderer();
void cleanupDotRenderer();
void drawDot(const glm::vec3& position, float size, const glm::vec3& color, float alpha = 1.0f);
