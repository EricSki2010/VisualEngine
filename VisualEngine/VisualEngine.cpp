#include "VisualEngine.h"
#include "EngineGlobals.h"
#include "renderingManagement/DefaultShaders.h"
#include "renderingManagement/RenderLoop.h"
#include "inputManagement/Camera.h"
#include "inputManagement/Collision.h"
#include <iostream>

GLFWwindow* gWindow = nullptr;
std::unique_ptr<Shader> gShader;
std::unique_ptr<Scene> gScene;
int gWidth = 800;
int gHeight = 600;
bool gNeedsRebuild = true;
VE::MeshMode gMode = VE::SINGLE;
std::vector<MergedMeshEntry> gMergedMeshes;
void (*gPostRenderCallback)() = nullptr;

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

void setMode(MeshMode mode) {
    gMode = mode;
    gNeedsRebuild = true;
}

void draw(const char* meshName, float x, float y, float z) {
    addDrawInstance(meshName, x, y, z);
    const RegisteredMesh* reg = getRegisteredMesh(meshName);
    if (reg)
        addCollider(meshName, reg->vertices.data(), reg->vertexCount,
                    reg->indices.data(), reg->indexCount, reg->rectangular, x, y, z);
    gNeedsRebuild = true;
}

void undraw(float x, float y, float z) {
    removeDrawInstance(x, y, z);
    removeCollider(x, y, z);
    gNeedsRebuild = true;
}

void clearDraws() {
    clearDrawInstances();
    clearColliders();
    gNeedsRebuild = true;
}

bool hasBlockAt(int x, int y, int z) {
    return hasColliderAt(x, y, z);
}

void rebuild() {
    gMergedMeshes.clear();
    if (gMode == CHUNK) {
        gMergedMeshes = buildMergedMeshes();
    } else {
        gMergedMeshes = buildSingleMeshes();
    }
    gNeedsRebuild = false;
}

void setPostRenderCallback(void (*callback)()) {
    gPostRenderCallback = callback;
}

void run() {
    if (!gWindow || !gShader || !gScene) return;

    gScene->uploadStaticUniforms(*gShader);
    double lastTime = glfwGetTime();

    rebuild();

    while (!glfwWindowShouldClose(gWindow)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        processInput(dt);
        update();
        render();

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    gMergedMeshes.clear();
    gScene.reset();
    gShader.reset();
    glfwTerminate();

    gWindow = nullptr;
    clearMeshData();
}

} // namespace VE
