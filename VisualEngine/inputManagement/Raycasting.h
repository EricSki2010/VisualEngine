#pragma once

#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

Ray screenToRay(double mouseX, double mouseY, int screenWidth, int screenHeight,
                const glm::mat4& view, const glm::mat4& projection);
