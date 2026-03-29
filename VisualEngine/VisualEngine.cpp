#include "VisualEngine.h"
#include "renderingManagement/render.h"
#include "renderingManagement/DefaultShaders.h"
#include "renderingManagement/ChunkMesh.h"
#include "inputManagement/Camera.h"
#include <iostream>
#include <memory>

static GLFWwindow* gWindow = nullptr;
static std::unique_ptr<Shader> gShader;
static std::unique_ptr<Scene> gScene;
static int gWidth = 800;
static int gHeight = 600;

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    gWidth = width;
    gHeight = height;
    if (gScene)
        gScene->projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 500.0f);
}

namespace VE {

bool initWindow(int width, int height, const char* title) {
    gWidth = width;
    gHeight = height;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    gWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!gWindow) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(gWindow);
    glfwMaximizeWindow(gWindow);
    glfwGetFramebufferSize(gWindow, &gWidth, &gHeight);

    glfwSetFramebufferSizeCallback(gWindow, framebufferSizeCallback);
    glfwSetCursorPosCallback(gWindow, Camera::mouseCallback);
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, gWidth, gHeight);

    gShader = std::make_unique<Shader>(defaultVertSrc, defaultFragSrc);
    gScene = std::make_unique<Scene>((float)gWidth / (float)gHeight);

    return true;
}

void setCamera(float x, float y, float z, float yaw, float pitch) {
    Camera* cam = getGlobalCamera();
    cam->position = glm::vec3(x, y, z);
    cam->yaw = yaw;
    cam->pitch = pitch;
    cam->updateDir();
}

void loadMesh(const char* name, const MeshDef& def) {
    registerMesh(name, def);
}

void loadMesh(const char* name, const char* meshFilePath) {
    registerMeshFromFile(name, meshFilePath);
}

void draw(const char* meshName, float x, float y, float z) {
    addDrawInstance(meshName, x, y, z);
}

void run() {
    if (!gWindow || !gShader || !gScene) return;

    Camera* cam = getGlobalCamera();
    auto meshes = buildMergedMeshes();

    glm::mat4 model(1.0f);
    gScene->uploadStaticUniforms(*gShader);
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(gWindow)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(gWindow, true);

        cam->processKeyboard(gWindow, dt);
        gScene->view = cam->getViewMatrix();
        glUniformMatrix4fv(gShader->loc("view"), 1, GL_FALSE, glm::value_ptr(gScene->view));
        glUniform3fv(gShader->loc("viewPos"), 1, glm::value_ptr(cam->position));

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gScene->uploadFrameUniforms(*gShader, model);
        for (auto& entry : meshes)
            entry.mesh->draw(*gShader);

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    meshes.clear();
    gScene.reset();
    gShader.reset();
    glfwTerminate();

    gWindow = nullptr;
    clearMeshData();
}

} // namespace VE
