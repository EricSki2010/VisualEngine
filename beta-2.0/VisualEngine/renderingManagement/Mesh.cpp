#include "render.h"
#include <iostream>
#include <cstring>

void Mesh::computeNormals(float* vertices, int vertCount, unsigned int* indices, int idxCount,
                          float* outBuffer) {
    // outBuffer layout: pos(3) uv(2) normal(3) = 8 floats per vertex
    // first copy pos+uv and zero out normals
    for (int i = 0; i < vertCount; i++) {
        outBuffer[i * 8 + 0] = vertices[i * 5 + 0]; // x
        outBuffer[i * 8 + 1] = vertices[i * 5 + 1]; // y
        outBuffer[i * 8 + 2] = vertices[i * 5 + 2]; // z
        outBuffer[i * 8 + 3] = vertices[i * 5 + 3]; // u
        outBuffer[i * 8 + 4] = vertices[i * 5 + 4]; // v
        outBuffer[i * 8 + 5] = 0.0f; // nx
        outBuffer[i * 8 + 6] = 0.0f; // ny
        outBuffer[i * 8 + 7] = 0.0f; // nz
    }

    // calculate face normals from each triangle and accumulate onto vertices
    for (int i = 0; i < idxCount; i += 3) {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];

        glm::vec3 v0(outBuffer[i0 * 8], outBuffer[i0 * 8 + 1], outBuffer[i0 * 8 + 2]);
        glm::vec3 v1(outBuffer[i1 * 8], outBuffer[i1 * 8 + 1], outBuffer[i1 * 8 + 2]);
        glm::vec3 v2(outBuffer[i2 * 8], outBuffer[i2 * 8 + 1], outBuffer[i2 * 8 + 2]);

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        for (int j = 0; j < 3; j++) {
            unsigned int idx = indices[i + j];
            outBuffer[idx * 8 + 5] += normal.x;
            outBuffer[idx * 8 + 6] += normal.y;
            outBuffer[idx * 8 + 7] += normal.z;
        }
    }

    // normalize accumulated normals
    for (int i = 0; i < vertCount; i++) {
        glm::vec3 n(outBuffer[i * 8 + 5], outBuffer[i * 8 + 6], outBuffer[i * 8 + 7]);
        n = glm::normalize(n);
        outBuffer[i * 8 + 5] = n.x;
        outBuffer[i * 8 + 6] = n.y;
        outBuffer[i * 8 + 7] = n.z;
    }
}

Mesh::Mesh(float* vertices, int vertCount, unsigned int* indices, int idxCount) {
    indexCount = idxCount;
    texture = nullptr;
    color = glm::vec3(0.8f);

    // build interleaved buffer: pos(3) uv(2) normal(3)
    float* buffer = new float[vertCount * 8];
    computeNormals(vertices, vertCount, indices, idxCount, buffer);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertCount * 8 * sizeof(float), buffer, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    // position (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // uv (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    delete[] buffer;
}

Mesh::~Mesh() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Mesh::setTexture(Texture* tex) {
    texture = tex;
}

void Mesh::setColor(glm::vec3 col) {
    color = col;
}

void Mesh::draw(Shader& shader) {
    glUniform1f(shader.loc("alpha"), 1.0f);

    if (texture) {
        texture->bind(0);
        glUniform1i(shader.loc("textureSampler"), 0);
        glUniform1i(shader.loc("useTexture"), 1);
    } else {
        glUniform1i(shader.loc("useTexture"), 0);
        glUniform3fv(shader.loc("objectColor"), 1, glm::value_ptr(color));
    }

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}
