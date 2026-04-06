#include "Raycasting.h"
#include <glm/gtc/matrix_transform.hpp>

Ray screenToRay(double mouseX, double mouseY, int screenWidth, int screenHeight,
                const glm::mat4& view, const glm::mat4& projection) {
    glm::vec4 viewport(0, 0, screenWidth, screenHeight);
    glm::vec3 winNear((float)mouseX, (float)(screenHeight - mouseY), 0.0f);
    glm::vec3 winFar((float)mouseX, (float)(screenHeight - mouseY), 1.0f);
    glm::vec3 worldNear = glm::unProject(winNear, view, projection, viewport);
    glm::vec3 worldFar  = glm::unProject(winFar,  view, projection, viewport);

    return { worldNear, glm::normalize(worldFar - worldNear) };
}

TriangleHit rayToTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    TriangleHit result = {};
    result.hit = false;

    const float EPSILON = 1e-7f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON) return result;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return result;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) return result;

    float t = f * glm::dot(edge2, q);
    if (t > EPSILON) {
        result.hit = true;
        result.distance = t;
        result.point = ray.origin + t * ray.direction;
    }
    return result;
}

// Project a 3D point to screen pixel coords
static glm::vec2 projectToScreen(const glm::vec3& point,
                                  const glm::mat4& view, const glm::mat4& projection,
                                  int screenWidth, int screenHeight) {
    glm::vec4 clip = projection * view * glm::vec4(point, 1.0f);
    if (clip.w <= 0.0001f) return glm::vec2(-1.0f); // behind camera
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(
        (ndc.x * 0.5f + 0.5f) * screenWidth,
        (1.0f - (ndc.y * 0.5f + 0.5f)) * screenHeight
    );
}

// Closest point on a 2D line segment to a 2D point, returns t parameter (0-1)
static float closestTOnSegment2D(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 0.0001f) return 0.0f;
    float t = glm::dot(p - a, ab) / len2;
    return glm::clamp(t, 0.0f, 1.0f);
}

LineHit rayToLine(const Ray& ray, const glm::vec3& lineFrom, const glm::vec3& lineTo,
                  double mouseX, double mouseY, int screenWidth, int screenHeight,
                  const glm::mat4& view, const glm::mat4& projection,
                  float threshold) {
    LineHit result = {};
    result.hit = false;

    // Find closest points between the ray and the line segment in 3D
    glm::vec3 d1 = ray.direction;
    glm::vec3 d2 = lineTo - lineFrom;
    glm::vec3 r = ray.origin - lineFrom;

    float a = glm::dot(d1, d1);
    float b = glm::dot(d1, d2);
    float c = glm::dot(d2, d2);
    float d = glm::dot(d1, r);
    float e = glm::dot(d2, r);

    float denom = a * c - b * b;
    float tRay = 0.0f, tLine = 0.0f;

    if (denom > 0.0001f) {
        tRay = (b * e - c * d) / denom;
        tLine = (a * e - b * d) / denom;
    }

    // Clamp tLine to [0, 1] for the line segment
    tLine = glm::clamp(tLine, 0.0f, 1.0f);
    if (tRay < 0.0f) tRay = 0.0f;

    glm::vec3 closestOnLine = lineFrom + tLine * d2;
    glm::vec3 closestOnRay = ray.origin + tRay * d1;
    float worldDist = glm::length(closestOnLine - closestOnRay);

    // Project the closest point on the line to screen for pixel distance check
    glm::vec2 screenPoint = projectToScreen(closestOnLine, view, projection, screenWidth, screenHeight);
    glm::vec2 mouse((float)mouseX, (float)mouseY);
    float screenDist = glm::length(mouse - screenPoint);

    result.t = tLine;
    result.point = closestOnLine;
    result.distanceToLine = worldDist;
    result.screenDistance = screenDist;
    result.hit = screenDist <= threshold;

    return result;
}
