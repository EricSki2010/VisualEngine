#include "VisualEngine.h"
#include "EngineGlobals.h"
#include "renderingManagement/DefaultShaders.h"
#include "renderingManagement/RenderLoop.h"
#include "inputManagement/Camera.h"
#include "inputManagement/Collision.h"
#include <iostream>

EngineContext ctx;

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    ctx.width = width;
    ctx.height = height;
    if (ctx.scene)
        ctx.scene->projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 500.0f);
}

namespace VE {

bool initWindow(int width, int height, const char* title) {
    ctx.width = width;
    ctx.height = height;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    ctx.window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!ctx.window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(ctx.window);
    glfwMaximizeWindow(ctx.window);
    glfwGetFramebufferSize(ctx.window, &ctx.width, &ctx.height);

    glfwSetFramebufferSizeCallback(ctx.window, framebufferSizeCallback);
    glfwSetCursorPosCallback(ctx.window, Camera::mouseCallback);
    glfwSetInputMode(ctx.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, ctx.width, ctx.height);

    ctx.shader = std::make_unique<Shader>(defaultVertSrc, defaultFragSrc);
    ctx.scene = std::make_unique<Scene>((float)ctx.width / (float)ctx.height);

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
    ctx.mode = mode;
    ctx.needsRebuild = true;
}

void draw(const char* meshName, float x, float y, float z) {
    addDrawInstance(meshName, x, y, z);
    const RegisteredMesh* reg = getRegisteredMesh(meshName);
    if (reg)
        addCollider(meshName, reg->vertices.data(), reg->vertexCount,
                    reg->indices.data(), reg->indexCount, reg->rectangular, x, y, z);
    ctx.needsRebuild = true;
}

void undraw(float x, float y, float z) {
    removeDrawInstance(x, y, z);
    removeCollider(x, y, z);
    ctx.needsRebuild = true;
}

void clearDraws() {
    clearDrawInstances();
    clearColliders();
    ctx.needsRebuild = true;
}

bool hasBlockAt(int x, int y, int z) {
    return hasColliderAt(x, y, z);
}

void rebuild() {
    ctx.mergedMeshes.clear();
    if (ctx.mode == CHUNK) {
        ctx.mergedMeshes = buildMergedMeshes();
    } else {
        ctx.mergedMeshes = buildSingleMeshes();
    }
    ctx.needsRebuild = false;
}

void setPostRenderCallback(std::function<void()> callback) {
    ctx.postRenderCallback = std::move(callback);
}

void run() {
    if (!ctx.window || !ctx.shader || !ctx.scene) return;

    ctx.scene->uploadStaticUniforms(*ctx.shader);
    double lastTime = glfwGetTime();

    rebuild();

    while (!glfwWindowShouldClose(ctx.window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        processInput(dt);
        update();
        render();

        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    ctx.mergedMeshes.clear();
    ctx.scene.reset();
    ctx.shader.reset();
    glfwTerminate();

    ctx.window = nullptr;
    clearMeshData();
}

} // namespace VE
