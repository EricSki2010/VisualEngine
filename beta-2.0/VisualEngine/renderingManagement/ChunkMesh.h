#pragma once

#include "render.h"
#include "../VisualEngine.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

struct RegisteredMesh {
    std::vector<float> vertices;     // pos3+uv2 (5 floats) or pos3+uv2+normal3 (8 floats)
    int vertexCount;
    int floatsPerVertex = 5;         // 5 = no normals, 8 = has normals
    std::vector<unsigned int> indices;
    int indexCount;
    std::shared_ptr<Texture> texture;
    bool rectangular;
    // Per-triangle cull state: 0=never cull, 1=partial wall, 2=solid wall
    std::vector<int> triCullState;
    // Derived: which face directions have state 2 triangles
    bool solidFaces[6] = {false, false, false, false, false, false};
};

struct DrawInstance {
    glm::vec3 position;
    std::string meshName;
};

struct MergedMeshEntry {
    std::unique_ptr<Mesh> mesh;
    std::shared_ptr<Texture> texture;
};

void registerMesh(const char* name, const VE::MeshDef& def);
void registerMeshFromFile(const char* name, const char* meshFilePath);
void addDrawInstance(const char* meshName, float x, float y, float z);
void removeDrawInstance(float x, float y, float z);
void clearDrawInstances();
std::vector<MergedMeshEntry> buildMergedMeshes();  // CHUNK mode: face culling + merge
std::vector<MergedMeshEntry> buildSingleMeshes();  // SINGLE mode: full meshes, no culling
void clearMeshData();
const RegisteredMesh* getRegisteredMesh(const char* name);
void setMeshSolidFaces(const char* name, bool faces[6]);
// Register mesh with interleaved indices+state: v0,v1,v2,state, v0,v1,v2,state, ...
void registerMeshWithStates(const char* name, float* vertices, int vertexCount,
                            unsigned int* interleavedIndices, int triCount,
                            const char* texturePath = nullptr);
