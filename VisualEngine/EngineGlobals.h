#pragma once

#include "renderingManagement/render.h"
#include "renderingManagement/ChunkMesh.h"
#include "VisualEngine.h"
#include <memory>
#include <vector>

extern GLFWwindow* gWindow;
extern std::unique_ptr<Shader> gShader;
extern std::unique_ptr<Scene> gScene;
extern int gWidth;
extern int gHeight;
extern bool gNeedsRebuild;
extern VE::MeshMode gMode;
extern std::vector<MergedMeshEntry> gMergedMeshes;
