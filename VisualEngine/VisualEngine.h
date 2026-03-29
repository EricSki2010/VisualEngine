#pragma once

namespace VE {

struct MeshDef {
    float* vertices;
    int vertexCount;
    unsigned int* indices;
    int indexCount;
    const char* texturePath;
};

bool initWindow(int width, int height, const char* title);
void setCamera(float x, float y, float z,
               float yaw, float pitch);
void loadMesh(const char* name, const MeshDef& def);
void loadMesh(const char* name, const char* meshFilePath);
void draw(const char* meshName, float x, float y, float z);
void run();

} // namespace VE
