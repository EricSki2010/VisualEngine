#include "Overlay.h"

static unsigned int sOverlayVAO = 0;
static unsigned int sOverlayVBO = 0;

void initOverlay() {
    glGenVertexArrays(1, &sOverlayVAO);
    glGenBuffers(1, &sOverlayVBO);

    glBindVertexArray(sOverlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sOverlayVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void cleanupOverlay() {
    if (sOverlayVAO) { glDeleteVertexArrays(1, &sOverlayVAO); sOverlayVAO = 0; }
    if (sOverlayVBO) { glDeleteBuffers(1, &sOverlayVBO); sOverlayVBO = 0; }
}

void drawTriangleOverlay(Shader& shader, const Triangle& tri, const glm::vec3& color, float alpha) {
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));

    float data[3 * 8] = {
        tri.v0.x, tri.v0.y, tri.v0.z,  0, 0,  n.x, n.y, n.z,
        tri.v1.x, tri.v1.y, tri.v1.z,  0, 0,  n.x, n.y, n.z,
        tri.v2.x, tri.v2.y, tri.v2.z,  0, 0,  n.x, n.y, n.z,
    };

    glBindBuffer(GL_ARRAY_BUFFER, sOverlayVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    glDepthFunc(GL_LEQUAL);

    glUniform1i(shader.loc("useTexture"), 0);
    glUniform3fv(shader.loc("objectColor"), 1, glm::value_ptr(color));
    glUniform1f(shader.loc("alpha"), alpha);
    glUniform1f(shader.loc("ambientStrength"), 1.0f);

    glBindVertexArray(sOverlayVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform1f(shader.loc("alpha"), 1.0f);
    glUniform1f(shader.loc("ambientStrength"), 0.15f);
    glDepthFunc(GL_LESS);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
}

Triangle aabbFaceTriangle(const AABB& box, int faceIndex, int half) {
    glm::vec3 mn = box.min, mx = box.max;
    Triangle t;
    switch (faceIndex) {
        case 0: // +X
            if (half == 0) { t.v0 = {mx.x,mn.y,mn.z}; t.v1 = {mx.x,mx.y,mn.z}; t.v2 = {mx.x,mx.y,mx.z}; }
            else           { t.v0 = {mx.x,mn.y,mn.z}; t.v1 = {mx.x,mx.y,mx.z}; t.v2 = {mx.x,mn.y,mx.z}; }
            break;
        case 1: // -X
            if (half == 0) { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mn.x,mx.y,mx.z}; t.v2 = {mn.x,mx.y,mn.z}; }
            else           { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mn.x,mx.y,mn.z}; t.v2 = {mn.x,mn.y,mn.z}; }
            break;
        case 2: // +Y
            if (half == 0) { t.v0 = {mn.x,mx.y,mn.z}; t.v1 = {mn.x,mx.y,mx.z}; t.v2 = {mx.x,mx.y,mx.z}; }
            else           { t.v0 = {mn.x,mx.y,mn.z}; t.v1 = {mx.x,mx.y,mx.z}; t.v2 = {mx.x,mx.y,mn.z}; }
            break;
        case 3: // -Y
            if (half == 0) { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mn.x,mn.y,mn.z}; t.v2 = {mx.x,mn.y,mn.z}; }
            else           { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mx.x,mn.y,mn.z}; t.v2 = {mx.x,mn.y,mx.z}; }
            break;
        case 4: // +Z
            if (half == 0) { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mx.x,mn.y,mx.z}; t.v2 = {mx.x,mx.y,mx.z}; }
            else           { t.v0 = {mn.x,mn.y,mx.z}; t.v1 = {mx.x,mx.y,mx.z}; t.v2 = {mn.x,mx.y,mx.z}; }
            break;
        case 5: // -Z
            if (half == 0) { t.v0 = {mx.x,mn.y,mn.z}; t.v1 = {mn.x,mn.y,mn.z}; t.v2 = {mn.x,mx.y,mn.z}; }
            else           { t.v0 = {mx.x,mn.y,mn.z}; t.v1 = {mn.x,mx.y,mn.z}; t.v2 = {mx.x,mx.y,mn.z}; }
            break;
    }
    return t;
}
