#include "VisualEngine.h"
#include "renderingManagement/render.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <string>

// ── Shaders (baked in) ──────────────────────────────────────────────

static const char* vertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoord = aTexCoord;
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

static const char* fragSrc = R"(
#version 330 core
in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform int shininess;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform sampler2D textureSampler;
uniform bool useTexture;

out vec4 FragColor;

void main() {
    vec3 baseColor;
    if (useTexture) {
        baseColor = texture(textureSampler, TexCoord).rgb;
    } else {
        baseColor = objectColor;
    }

    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}
)";

// ── Internal state ──────────────────────────────────────────────────

struct RegisteredMesh {
    std::vector<float> vertices;      // pos(3) + uv(2) per vertex
    int vertexCount;
    std::vector<unsigned int> indices;
    int indexCount;
    Texture* texture;                 // nullptr = use default color
};

struct DrawInstance {
    glm::vec3 position;
    std::string meshName;
};

static GLFWwindow* gWindow = nullptr;
static Shader* gShader = nullptr;
static Scene* gScene = nullptr;
static glm::vec3 gCameraPos(3.0f, 3.0f, 3.0f);
static glm::vec3 gCameraTarget(0.0f, 0.0f, 0.0f);
static float gYaw = 210.0f;
static float gPitch = -35.0f;
static double gLastMouseX = 0.0;
static double gLastMouseY = 0.0;
static bool gFirstMouse = true;
static float gSensitivity = 0.2f;
static float gMoveSpeed = 5.0f;
static double gLastTime = 0.0;
static std::unordered_map<std::string, RegisteredMesh> gMeshes;
static std::vector<DrawInstance> gDrawList;
static int gWidth = 800;
static int gHeight = 600;

static void updateCameraDir() {
    float yawRad = glm::radians(gYaw);
    float pitchRad = glm::radians(gPitch);
    glm::vec3 dir;
    dir.x = cos(pitchRad) * sin(yawRad);
    dir.y = sin(pitchRad);
    dir.z = cos(pitchRad) * cos(yawRad);
    gCameraTarget = gCameraPos + dir;
}

static void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    if (gFirstMouse) {
        gLastMouseX = xpos;
        gLastMouseY = ypos;
        gFirstMouse = false;
        return;
    }

    float dx = (float)(gLastMouseX - xpos);
    float dy = (float)(gLastMouseY - ypos);
    gLastMouseX = xpos;
    gLastMouseY = ypos;

    gYaw += dx * gSensitivity;
    gPitch += dy * gSensitivity;

    if (gYaw < 0.0f) gYaw += 360.0f;
    if (gYaw >= 360.0f) gYaw -= 360.0f;
    if (gPitch > 89.0f) gPitch = 89.0f;
    if (gPitch < -89.0f) gPitch = -89.0f;

    updateCameraDir();
}

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    gWidth = width;
    gHeight = height;
    if (gScene)
        gScene->projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 500.0f);
}

// ── Mesh merging with face culling ──────────────────────────────────

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 10) ^ (std::hash<int>()(v.z) << 20);
    }
};

struct MergedMeshEntry {
    Mesh* mesh;
    Texture* texture; // nullptr = use color fallback
};

// Neighbor direction per face (matches standard cube face order)
static const int neighborDir[6][3] = {
    { 0, 0, 1},  // front (+Z)
    { 0, 0,-1},  // back (-Z)
    {-1, 0, 0},  // left (-X)
    { 1, 0, 0},  // right (+X)
    { 0, 1, 0},  // top (+Y)
    { 0,-1, 0},  // bottom (-Y)
};

static const int faceVertStart[6] = {0, 4, 8, 12, 16, 20};

static const unsigned int faceIdx[6][6] = {
    {0,1,2, 2,3,0},  // front
    {0,2,1, 2,0,3},  // back
    {0,1,2, 2,3,0},  // left
    {0,1,2, 2,3,0},  // right
    {0,1,2, 2,3,0},  // top
    {0,1,2, 2,3,0},  // bottom
};

static std::vector<MergedMeshEntry> buildMergedMeshes(const std::vector<DrawInstance>& draws) {
    // Occupancy set across all mesh types
    std::unordered_set<glm::ivec3, IVec3Hash> occupied;
    for (const auto& d : draws)
        occupied.insert(glm::ivec3((int)roundf(d.position.x), (int)roundf(d.position.y), (int)roundf(d.position.z)));

    // Group by mesh name
    std::unordered_map<std::string, std::vector<const DrawInstance*>> groups;
    for (const auto& d : draws)
        groups[d.meshName].push_back(&d);

    std::vector<MergedMeshEntry> result;

    for (const auto& [name, group] : groups) {
        auto mit = gMeshes.find(name);
        if (mit == gMeshes.end()) continue;
        const RegisteredMesh& reg = mit->second;

        std::vector<float> verts;
        std::vector<unsigned int> indices;

        for (const DrawInstance* inst : group) {
            glm::ivec3 pos((int)roundf(inst->position.x), (int)roundf(inst->position.y), (int)roundf(inst->position.z));

            for (int f = 0; f < 6; f++) {
                glm::ivec3 neighbor = pos + glm::ivec3(neighborDir[f][0], neighborDir[f][1], neighborDir[f][2]);
                if (occupied.count(neighbor)) continue;

                unsigned int baseIdx = (unsigned int)(verts.size() / 5);

                for (int v = 0; v < 4; v++) {
                    int src = (faceVertStart[f] + v) * 5;
                    verts.push_back(reg.vertices[src + 0] + inst->position.x);
                    verts.push_back(reg.vertices[src + 1] + inst->position.y);
                    verts.push_back(reg.vertices[src + 2] + inst->position.z);
                    verts.push_back(reg.vertices[src + 3]);
                    verts.push_back(reg.vertices[src + 4]);
                }

                for (int i = 0; i < 6; i++)
                    indices.push_back(baseIdx + faceIdx[f][i]);
            }
        }

        if (!verts.empty()) {
            Mesh* mesh = new Mesh(verts.data(), (int)(verts.size() / 5), indices.data(), (int)indices.size());
            if (reg.texture) mesh->setTexture(reg.texture);
            result.push_back({mesh, reg.texture});
        }
    }

    return result;
}

// ── Public API ──────────────────────────────────────────────────────

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
    glfwSetFramebufferSizeCallback(gWindow, framebufferSizeCallback);
    glfwSetCursorPosCallback(gWindow, mouseCallback);
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);

    gShader = new Shader(vertSrc, fragSrc);
    gScene = new Scene((float)width / (float)height);

    return true;
}

void setCamera(float x, float y, float z,
               float yaw, float pitch) {
    gCameraPos = glm::vec3(x, y, z);
    gYaw = yaw;
    gPitch = pitch;
    updateCameraDir();
}

void loadMesh(const char* name, const MeshDef& def) {
    RegisteredMesh reg;
    reg.vertices.assign(def.vertices, def.vertices + def.vertexCount * 5);
    reg.vertexCount = def.vertexCount;
    reg.indices.assign(def.indices, def.indices + def.indexCount);
    reg.indexCount = def.indexCount;
    reg.texture = def.texturePath ? new Texture(def.texturePath) : nullptr;
    gMeshes[name] = std::move(reg);
}

void draw(const char* meshName, float x, float y, float z) {
    gDrawList.push_back({glm::vec3(x, y, z), meshName});
}

void run() {
    if (!gWindow || !gShader || !gScene) return;

    auto meshes = buildMergedMeshes(gDrawList);

    glm::mat4 model(1.0f);
    gScene->uploadStaticUniforms(*gShader);
    gLastTime = glfwGetTime();

    while (!glfwWindowShouldClose(gWindow)) {
        double now = glfwGetTime();
        float dt = (float)(now - gLastTime);
        gLastTime = now;

        if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(gWindow, true);

        float yawRad = glm::radians(gYaw);
        glm::vec3 forward(sin(yawRad), 0.0f, cos(yawRad));
        glm::vec3 right(-cos(yawRad), 0.0f, sin(yawRad));

        float speed = gMoveSpeed * dt;
        if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS) gCameraPos += forward * speed;
        if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS) gCameraPos -= forward * speed;
        if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS) gCameraPos += right * speed;
        if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS) gCameraPos -= right * speed;
        if (glfwGetKey(gWindow, GLFW_KEY_SPACE) == GLFW_PRESS) gCameraPos.y += speed;
        if (glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) gCameraPos.y -= speed;

        updateCameraDir();
        gScene->view = glm::lookAt(gCameraPos, gCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(gShader->loc("view"), 1, GL_FALSE, glm::value_ptr(gScene->view));
        glUniform3fv(gShader->loc("viewPos"), 1, glm::value_ptr(gCameraPos));

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gScene->uploadFrameUniforms(*gShader, model);
        for (auto& entry : meshes)
            entry.mesh->draw(*gShader);

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    for (auto& entry : meshes) delete entry.mesh;
    for (auto& [name, reg] : gMeshes) delete reg.texture;
    delete gScene;
    delete gShader;
    glfwTerminate();

    gScene = nullptr;
    gShader = nullptr;
    gWindow = nullptr;
    gMeshes.clear();
    gDrawList.clear();
}

} // namespace VE
