#pragma once

#include "render.h"
#include "../VisualEngine.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

struct RegisteredMesh {
    std::vector<float> vertices;
    int vertexCount;
    std::vector<unsigned int> indices;
    int indexCount;
    std::shared_ptr<Texture> texture;
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
std::vector<MergedMeshEntry> buildMergedMeshes();
void clearMeshData();
