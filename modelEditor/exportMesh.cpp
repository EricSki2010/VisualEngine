// Standalone tool: converts cubeData.h into a .mesh binary file
// Usage: exportMesh.exe
// Outputs: assets/cube.mesh

#include <fstream>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Pull in the raw vertex/index data (without the VE::MeshDef since we don't need GL)
static float cubeVertices[] = {
    -0.5f,-0.5f, 0.5f, 0.0f, 0.0f,
     0.5f,-0.5f, 0.5f, 0.5f, 0.0f,
     0.5f, 0.5f, 0.5f, 0.5f, 0.3333f,
    -0.5f, 0.5f, 0.5f, 0.0f, 0.3333f,
    -0.5f,-0.5f,-0.5f, 1.0f, 0.0f,
     0.5f,-0.5f,-0.5f, 0.5f, 0.0f,
     0.5f, 0.5f,-0.5f, 0.5f, 0.3333f,
    -0.5f, 0.5f,-0.5f, 1.0f, 0.3333f,
    -0.5f,-0.5f,-0.5f, 0.0f, 0.3333f,
    -0.5f,-0.5f, 0.5f, 0.5f, 0.3333f,
    -0.5f, 0.5f, 0.5f, 0.5f, 0.6667f,
    -0.5f, 0.5f,-0.5f, 0.0f, 0.6667f,
     0.5f,-0.5f, 0.5f, 0.5f, 0.3333f,
     0.5f,-0.5f,-0.5f, 1.0f, 0.3333f,
     0.5f, 0.5f,-0.5f, 1.0f, 0.6667f,
     0.5f, 0.5f, 0.5f, 0.5f, 0.6667f,
    -0.5f, 0.5f, 0.5f, 0.0f, 0.6667f,
     0.5f, 0.5f, 0.5f, 0.5f, 0.6667f,
     0.5f, 0.5f,-0.5f, 0.5f, 1.0f,
    -0.5f, 0.5f,-0.5f, 0.0f, 1.0f,
    -0.5f,-0.5f,-0.5f, 0.5f, 0.6667f,
     0.5f,-0.5f,-0.5f, 1.0f, 0.6667f,
     0.5f,-0.5f, 0.5f, 1.0f, 1.0f,
    -0.5f,-0.5f, 0.5f, 0.5f, 1.0f,
};

static unsigned int cubeIndices[] = {
     0, 1, 2, 2, 3, 0,
     4, 6, 5, 6, 4, 7,
     8, 9,10,10,11, 8,
    12,13,14,14,15,12,
    16,17,18,18,19,16,
    20,21,22,22,23,20,
};

int main() {
    const char* texturePath = "assets/cube.png";
    const char* outputPath = "assets/cube.mesh";

    uint32_t vertexCount = 24;
    uint32_t indexCount = 36;
    uint32_t texturePathLen = (uint32_t)strlen(texturePath) + 1; // include null

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        printf("Failed to create %s\n", outputPath);
        return 1;
    }

    out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    out.write(reinterpret_cast<const char*>(&texturePathLen), sizeof(texturePathLen));
    out.write(reinterpret_cast<const char*>(cubeVertices), vertexCount * 5 * sizeof(float));
    out.write(reinterpret_cast<const char*>(cubeIndices), indexCount * sizeof(uint32_t));
    out.write(texturePath, texturePathLen);

    out.close();
    printf("Wrote %s (%u verts, %u indices, texture: %s)\n", outputPath, vertexCount, indexCount, texturePath);
    return 0;
}