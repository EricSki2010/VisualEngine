#include "Camera.h"
#include <cmath>

static Camera gCamera;

Camera* getGlobalCamera() {
    return &gCamera;
}

void Camera::updateDir() {
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    glm::vec3 dir;
    dir.x = cos(pitchRad) * sin(yawRad);
    dir.y = sin(pitchRad);
    dir.z = cos(pitchRad) * cos(yawRad);
    target = position + dir;
}

void Camera::processKeyboard(GLFWwindow* window, float dt) {
    float yawRad = glm::radians(yaw);
    glm::vec3 forward(sin(yawRad), 0.0f, cos(yawRad));
    glm::vec3 right(-cos(yawRad), 0.0f, sin(yawRad));

    float speed = moveSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += right * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) position.y -= speed;

    updateDir();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Camera* cam = getGlobalCamera();
    bool qDown = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;

    // Toggle on Q press (not hold)
    if (qDown && !cam->qWasPressed) {
        cam->looking = !cam->looking;
        if (cam->looking) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            int w, h;
            glfwGetWindowSize(window, &w, &h);
            glfwSetCursorPos(window, w / 2.0, h / 2.0);
            cam->lastMouseX = w / 2.0;
            cam->lastMouseY = h / 2.0;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    cam->qWasPressed = qDown;

    if (!cam->looking) return;

    float dx = (float)(cam->lastMouseX - xpos);
    float dy = (float)(cam->lastMouseY - ypos);
    cam->lastMouseX = xpos;
    cam->lastMouseY = ypos;

    cam->yaw += dx * cam->sensitivity;
    cam->pitch += dy * cam->sensitivity;

    if (cam->yaw < 0.0f) cam->yaw += 360.0f;
    if (cam->yaw >= 360.0f) cam->yaw -= 360.0f;
    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    cam->updateDir();
}