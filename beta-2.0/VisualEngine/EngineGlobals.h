#pragma once

#include "renderingManagement/render.h"
#include "renderingManagement/meshing/ChunkMesh.h"
#include "VisualEngine.h"
#include <memory>
#include <vector>
struct EngineContext {
    GLFWwindow* window = nullptr;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Shader> vcShader; // vertex-colored mesh shader
    std::unique_ptr<Scene> scene;
    int width = 800;
    int height = 600;
    bool needsRebuild = true;
    VE::MeshMode mode = VE::SINGLE;
    std::vector<MergedMeshEntry> mergedMeshes;
    float scrollDelta = 0.0f; // mouse scroll this frame
};

extern EngineContext ctx;
