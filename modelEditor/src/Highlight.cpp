#include "Highlight.h"
#include "Selection.h"
#include "../../VisualEngine/EngineGlobals.h"
#include "../../VisualEngine/inputManagement/Raycasting.h"
#include "../../VisualEngine/inputManagement/Collision.h"
#include "../../VisualEngine/renderingManagement/Overlay.h"
#include "../../VisualEngine/VisualEngine.h"
#include <algorithm>
#include <cmath>

// Drag state
static bool sDragging = false;
static bool sWasRightDown = false;
static glm::ivec3 sStartBlock;
static int sStartFace = -1;

static const glm::ivec3 sFaceNormals[6] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1},
};

static void getFacePlaneAxes(int faceIndex, int& axisA, int& axisB) {
    switch (faceIndex / 2) {
        case 0: axisA = 1; axisB = 2; break;
        case 1: axisA = 0; axisB = 2; break;
        case 2: axisA = 0; axisB = 1; break;
        default: axisA = 0; axisB = 1; break;
    }
}

static int getFaceDepthAxis(int faceIndex) {
    return faceIndex / 2;
}

static void drawFaceOverlay(const glm::ivec3& blockPos, int faceIndex,
                            const glm::vec3& color, float alpha) {
    const BlockCollider* col = getColliderAt(blockPos.x, blockPos.y, blockPos.z);
    if (!col) return;

    Triangle t0 = aabbFaceTriangle(col->bounds, faceIndex, 0);
    Triangle t1 = aabbFaceTriangle(col->bounds, faceIndex, 1);
    drawTriangleOverlay(*ctx.shader, t0, color, alpha);
    drawTriangleOverlay(*ctx.shader, t1, color, alpha);
}

// Collect all valid faces in the rectangle between start and end block on the locked plane
static void collectRectangleFaces(const glm::ivec3& startBlock, const glm::ivec3& endBlock,
                                  int faceIndex, std::vector<SelectedFace>& out) {
    int axisA, axisB;
    getFacePlaneAxes(faceIndex, axisA, axisB);
    int depthAxis = getFaceDepthAxis(faceIndex);
    int depth = startBlock[depthAxis];
    glm::ivec3 neighbor = sFaceNormals[faceIndex];

    int minA = std::min(startBlock[axisA], endBlock[axisA]);
    int maxA = std::max(startBlock[axisA], endBlock[axisA]);
    int minB = std::min(startBlock[axisB], endBlock[axisB]);
    int maxB = std::max(startBlock[axisB], endBlock[axisB]);

    for (int a = minA; a <= maxA; a++) {
        for (int b = minB; b <= maxB; b++) {
            glm::ivec3 pos;
            pos[axisA] = a;
            pos[axisB] = b;
            pos[depthAxis] = depth;

            if (!VE::hasBlockAt(pos.x, pos.y, pos.z))
                continue;
            if (VE::hasBlockAt(pos.x + neighbor.x, pos.y + neighbor.y, pos.z + neighbor.z))
                continue;

            out.push_back({pos, faceIndex});
        }
    }
}

void initHighlight() {
    initOverlay();
}

void cleanupHighlight() {
    cleanupOverlay();
}

void renderHoverHighlight() {
    double mx, my;
    glfwGetCursorPos(ctx.window, &mx, &my);
    if (mx < 0 || mx >= ctx.width || my < 0 || my >= ctx.height)
        return;

    Ray ray = screenToRay(mx, my, ctx.width, ctx.height, ctx.scene->view, ctx.scene->projection);
    CollisionHit hit = raycast(ray.origin, ray.direction);

    bool rightDown = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool rightJustPressed = rightDown && !sWasRightDown;
    bool rightJustReleased = !rightDown && sWasRightDown;
    sWasRightDown = rightDown;

    bool shiftHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

    // Start drag
    if (rightJustPressed && hit.hit && hit.collider && hit.collider->isRectangular) {
        sDragging = true;
        sStartBlock = glm::ivec3(glm::round(hit.collider->position));
        sStartFace = hit.triangleIndex;
        if (!shiftHeld && !ctrlHeld)
            clearSelection();
    }

    // Commit selection on release
    if (rightJustReleased && sDragging) {
        if (hit.hit && hit.collider && hit.collider->isRectangular) {
            glm::ivec3 endBlock = glm::ivec3(glm::round(hit.collider->position));
            std::vector<SelectedFace> faces;
            collectRectangleFaces(sStartBlock, endBlock, sStartFace, faces);
            if (ctrlHeld) {
                for (const auto& f : faces)
                    removeSelectedFace(f.blockPos, f.faceIndex);
            } else {
                for (const auto& f : faces)
                    addSelectedFace(f.blockPos, f.faceIndex);
            }
        }
        sDragging = false;
    }

    // Draw stored selection (blue)
    glm::vec3 blue(0.0f, 0.5f, 1.0f);
    for (const auto& sel : getSelection())
        drawFaceOverlay(sel.blockPos, sel.faceIndex, blue, 0.45f);

    // Draw drag preview while dragging (light blue = add, red = remove)
    if (sDragging && hit.hit && hit.collider && hit.collider->isRectangular) {
        glm::ivec3 currentBlock = glm::ivec3(glm::round(hit.collider->position));
        std::vector<SelectedFace> preview;
        collectRectangleFaces(sStartBlock, currentBlock, sStartFace, preview);

        glm::vec3 previewColor = ctrlHeld ? glm::vec3(1.0f, 0.3f, 0.3f) : glm::vec3(0.3f, 0.7f, 1.0f);
        for (const auto& f : preview)
            drawFaceOverlay(f.blockPos, f.faceIndex, previewColor, 0.35f);
        return;
    }

    // Normal hover highlight (yellow) — skip if there's an active selection
    if (!getSelection().empty())
        return;

    if (!hit.hit || hit.triangleIndex < 0 || !hit.collider)
        return;

    glm::vec3 yellow(1.0f, 1.0f, 0.0f);
    if (hit.collider->isRectangular) {
        drawFaceOverlay(glm::ivec3(glm::round(hit.collider->position)), hit.triangleIndex, yellow, 0.45f);
    } else if (hit.triangleIndex < (int)hit.collider->triangles.size()) {
        drawTriangleOverlay(*ctx.shader, hit.collider->triangles[hit.triangleIndex], yellow, 0.45f);
    }
}
