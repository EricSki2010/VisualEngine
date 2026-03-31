#pragma once

#include "renderingManagement/render.h"
#include "renderingManagement/ChunkMesh.h"
#include "VisualEngine.h"
#include <memory>
#include <vector>
#include <functional>

struct EngineContext {
    GLFWwindow* window = nullptr;
    std::unique_ptr<Shader> shader;
    std::unique_ptr<Scene> scene;
    int width = 800;
    int height = 600;
    bool needsRebuild = true;
    VE::MeshMode mode = VE::SINGLE;
    std::vector<MergedMeshEntry> mergedMeshes;
    std::function<void()> postRenderCallback;
};

extern EngineContext ctx;
