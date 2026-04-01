#include "Raycasting.h"
#include <glm/gtc/matrix_transform.hpp>

Ray screenToRay(double mouseX, double mouseY, int screenWidth, int screenHeight,
                const glm::mat4& view, const glm::mat4& projection) {
    glm::vec4 viewport(0, 0, screenWidth, screenHeight);
    glm::vec3 winNear((float)mouseX, (float)(screenHeight - mouseY), 0.0f);
    glm::vec3 winFar((float)mouseX, (float)(screenHeight - mouseY), 1.0f);
    glm::vec3 worldNear = glm::unProject(winNear, view, projection, viewport);
    glm::vec3 worldFar  = glm::unProject(winFar,  view, projection, viewport);

    return { worldNear, glm::normalize(worldFar - worldNear) };
}
