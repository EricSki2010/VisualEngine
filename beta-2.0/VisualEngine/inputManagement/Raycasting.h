#pragma once

#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

Ray screenToRay(double mouseX, double mouseY, int screenWidth, int screenHeight,
                const glm::mat4& view, const glm::mat4& projection);

struct LineHit {
    bool hit;
    glm::vec3 point;          // exact 3D point on the line segment
    float distanceToLine;     // world-space distance between ray and line
    float screenDistance;      // pixel distance on screen
    float t;                  // 0-1 parameter along the line (0 = from, 1 = to)
};

// Test if a mouse position is close to a line segment on screen.
// threshold = max pixel distance to count as a hit.
struct TriangleHit {
    bool hit;
    float distance;
    glm::vec3 point;
};

TriangleHit rayToTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

LineHit rayToLine(const Ray& ray, const glm::vec3& lineFrom, const glm::vec3& lineTo,
                  double mouseX, double mouseY, int screenWidth, int screenHeight,
                  const glm::mat4& view, const glm::mat4& projection,
                  float threshold = 5.0f);
