#include "Highlight.h"
#include "render.h"
#include "../EngineGlobals.h"
#include "../inputManagement/Collision.h"

static unsigned int gHighlightVAO = 0;
static unsigned int gHighlightVBO = 0;
static bool gHighlightActive = false;

void initHighlight() {
    glGenVertexArrays(1, &gHighlightVAO);
    glGenBuffers(1, &gHighlightVBO);

    glBindVertexArray(gHighlightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gHighlightVBO);
    // Reserve space for 1 triangle: 3 verts * 8 floats (pos3 uv2 normal3)
    glBufferData(GL_ARRAY_BUFFER, 3 * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

static void updateHighlight(const Triangle& tri) {
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));

    float data[3 * 8] = {
        tri.v0.x, tri.v0.y, tri.v0.z,  0, 0,  n.x, n.y, n.z,
        tri.v1.x, tri.v1.y, tri.v1.z,  0, 0,  n.x, n.y, n.z,
        tri.v2.x, tri.v2.y, tri.v2.z,  0, 0,  n.x, n.y, n.z,
    };

    glBindBuffer(GL_ARRAY_BUFFER, gHighlightVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
}

static void drawHighlight(Shader& shader) {
    if (!gHighlightActive) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    glUniform1i(shader.loc("useTexture"), 0);
    glm::vec3 yellow(1.0f, 1.0f, 0.0f);
    glUniform3fv(shader.loc("objectColor"), 1, glm::value_ptr(yellow));
    glUniform1f(shader.loc("alpha"), 0.45f);

    glBindVertexArray(gHighlightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform1f(shader.loc("alpha"), 1.0f);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
}

void cleanupHighlight() {
    if (gHighlightVAO) { glDeleteVertexArrays(1, &gHighlightVAO); gHighlightVAO = 0; }
    if (gHighlightVBO) { glDeleteBuffers(1, &gHighlightVBO); gHighlightVBO = 0; }
}

// Build a triangle for an AABB face (for rectangular collider highlight)
static Triangle aabbFaceTriangles(const AABB& box, int faceIndex, int half) {
    // faceIndex: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
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

void renderHoverHighlight() {
    gHighlightActive = false;

    double mx, my;
    glfwGetCursorPos(gWindow, &mx, &my);
    if (mx < 0 || mx >= gWidth || my < 0 || my >= gHeight)
        return;

    glm::vec4 viewport(0, 0, gWidth, gHeight);
    glm::vec3 winNear((float)mx, (float)(gHeight - my), 0.0f);
    glm::vec3 winFar((float)mx, (float)(gHeight - my), 1.0f);
    glm::vec3 worldNear = glm::unProject(winNear, gScene->view, gScene->projection, viewport);
    glm::vec3 worldFar  = glm::unProject(winFar,  gScene->view, gScene->projection, viewport);
    glm::vec3 rayDir = glm::normalize(worldFar - worldNear);

    CollisionHit hit = raycast(worldNear, rayDir);
    if (!hit.hit || hit.triangleIndex < 0 || !hit.collider)
        return;

    gHighlightActive = true;
    if (hit.collider->isRectangular) {
        Triangle t0 = aabbFaceTriangles(hit.collider->bounds, hit.triangleIndex, 0);
        Triangle t1 = aabbFaceTriangles(hit.collider->bounds, hit.triangleIndex, 1);
        updateHighlight(t0);
        drawHighlight(*gShader);
        updateHighlight(t1);
        drawHighlight(*gShader);
    } else if (hit.triangleIndex < (int)hit.collider->triangles.size()) {
        updateHighlight(hit.collider->triangles[hit.triangleIndex]);
        drawHighlight(*gShader);
    }
    gHighlightActive = false;
}
