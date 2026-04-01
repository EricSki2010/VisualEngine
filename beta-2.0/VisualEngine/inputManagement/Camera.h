#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    glm::vec3 position = glm::vec3(3.0f, 3.0f, 3.0f);
    glm::vec3 target = glm::vec3(0.0f);
    float yaw = 210.0f;
    float pitch = -35.0f;
    float sensitivity = 0.2f;
    float moveSpeed = 5.0f;

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    bool looking = false;
    bool qWasPressed = false;

    void updateDir();
    void processKeyboard(GLFWwindow* window, float dt);
    glm::mat4 getViewMatrix() const;

    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
};

Camera* getGlobalCamera();