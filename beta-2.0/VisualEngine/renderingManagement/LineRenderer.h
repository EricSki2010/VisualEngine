#pragma once

#include <glm/glm.hpp>
#include <vector>

void initLineRenderer();
void cleanupLineRenderer();
void drawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color, float width = 1.0f);
void drawLines(const std::vector<glm::vec3>& points, const glm::vec3& color, float width = 1.0f, bool loop = false);
