#pragma once

#include <string>
#include <functional>

namespace VE {

enum MeshMode {
    SINGLE,  // one mesh rendered as-is (models, props)
    CHUNK,   // voxel grid with face culling + merging
};

struct MeshDef {
    float* vertices;
    int vertexCount;
    unsigned int* indices;
    int indexCount;
    const char* texturePath;
};

bool initWindow(int width, int height, const char* title, bool maximized = false);
void setCamera(float x, float y, float z,
               float yaw, float pitch);
void loadMesh(const char* name, const MeshDef& def);
void loadMesh(const char* name, const char* meshFilePath);
void loadMeshDir(const char* dirPath);
void setMode(MeshMode mode);
void draw(const char* meshName, float x, float y, float z);
void undraw(float x, float y, float z);
void clearDraws();
void rebuild();
bool hasBlockAt(int x, int y, int z);
void registerScene(const std::string& name, std::function<void()> onEnter,
                   std::function<void()> onExit,
                   std::function<void(float dt)> onInput,
                   std::function<void()> onUpdate,
                   std::function<void()> onRender);
void setScene(const std::string& name);
void run();

} // namespace VE
