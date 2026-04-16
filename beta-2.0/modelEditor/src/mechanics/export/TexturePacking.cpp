#include "TexturePacking.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>

// stb_image_write callback: append bytes to a std::vector<uint8_t>
static void writeToVector(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<uint8_t>*>(context);
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

// Edge function for barycentric point-in-triangle test
static float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

std::vector<uint8_t> packTrianglesToPNG(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors,
    int size) {

    if (size < 1) size = 1;
    int triCount = (int)(indices.size() / 3);
    if (triCount == 0 || (int)triangleColors.size() < triCount)
        return {};

    // RGBA pixel buffer, transparent by default
    std::vector<uint8_t> pixels(size * size * 4, 0);

    auto writePixel = [&](int x, int y, const glm::vec3& c) {
        if (x < 0 || x >= size || y < 0 || y >= size) return;
        int idx = (y * size + x) * 4;
        pixels[idx + 0] = (uint8_t)(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
        pixels[idx + 1] = (uint8_t)(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
        pixels[idx + 2] = (uint8_t)(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
        pixels[idx + 3] = 255;
    };

    for (int t = 0; t < triCount; t++) {
        uint32_t i0 = indices[t * 3];
        uint32_t i1 = indices[t * 3 + 1];
        uint32_t i2 = indices[t * 3 + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;

        // Convert to pixel space
        glm::vec2 p0 = verts[i0] * (float)size;
        glm::vec2 p1 = verts[i1] * (float)size;
        glm::vec2 p2 = verts[i2] * (float)size;

        // Bounding box (clamp to buffer)
        int minX = std::max(0, (int)std::floor(std::min({p0.x, p1.x, p2.x})));
        int minY = std::max(0, (int)std::floor(std::min({p0.y, p1.y, p2.y})));
        int maxX = std::min(size - 1, (int)std::ceil(std::max({p0.x, p1.x, p2.x})));
        int maxY = std::min(size - 1, (int)std::ceil(std::max({p0.y, p1.y, p2.y})));

        float area = edge(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
        if (std::fabs(area) < 1e-6f) continue; // degenerate

        const glm::vec3& color = triangleColors[t];

        // Conservative rasterization: fill any pixel the triangle touches.
        // Test all 4 corners + center of each pixel; if any is inside OR
        // any triangle edge crosses the pixel, fill it.
        auto pointInside = [&](float px, float py) {
            float w0 = edge(p1.x, p1.y, p2.x, p2.y, px, py);
            float w1 = edge(p2.x, p2.y, p0.x, p0.y, px, py);
            float w2 = edge(p0.x, p0.y, p1.x, p1.y, px, py);
            return (area > 0)
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);
        };

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                // Pixel covers [x, x+1] x [y, y+1]
                float x0 = (float)x, x1 = (float)(x + 1);
                float y0 = (float)y, y1 = (float)(y + 1);

                // 1) Any pixel corner or center inside the triangle
                bool touched =
                    pointInside(x0, y0) || pointInside(x1, y0) ||
                    pointInside(x0, y1) || pointInside(x1, y1) ||
                    pointInside(x0 + 0.5f, y0 + 0.5f);

                // 2) Any triangle vertex inside the pixel
                if (!touched) {
                    auto inPix = [&](const glm::vec2& p) {
                        return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1;
                    };
                    if (inPix(p0) || inPix(p1) || inPix(p2)) touched = true;
                }

                // 3) Any triangle edge crosses the pixel bounds
                if (!touched) {
                    auto segIntersectsBox = [&](const glm::vec2& a, const glm::vec2& b) {
                        // Liang-Barsky line clipping against box [x0,x1]x[y0,y1]
                        float dx = b.x - a.x, dy = b.y - a.y;
                        float p[4] = {-dx, dx, -dy, dy};
                        float q[4] = {a.x - x0, x1 - a.x, a.y - y0, y1 - a.y};
                        float u1 = 0.0f, u2 = 1.0f;
                        for (int i = 0; i < 4; i++) {
                            if (p[i] == 0.0f) {
                                if (q[i] < 0.0f) return false;
                            } else {
                                float u = q[i] / p[i];
                                if (p[i] < 0.0f) { if (u > u1) u1 = u; }
                                else             { if (u < u2) u2 = u; }
                            }
                        }
                        return u1 <= u2;
                    };
                    if (segIntersectsBox(p0, p1) || segIntersectsBox(p1, p2) || segIntersectsBox(p2, p0))
                        touched = true;
                }

                if (touched)
                    writePixel(x, y, color);
            }
        }
    }

    std::vector<uint8_t> pngBuffer;
    int ok = stbi_write_png_to_func(writeToVector, &pngBuffer, size, size, 4,
                                     pixels.data(), size * 4);
    if (!ok) return {};
    return pngBuffer;
}

PackAndExportResult packAndSavePNG(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors,
    const std::string& outPath,
    int size) {

    PackAndExportResult result;
    if (verts.empty() || indices.empty()) return result;

    // 1. Find the bounding box. minX = leftmost vert, minY = topmost vert
    //    (image space: y=0 is the top, so "highest" = smallest y).
    float minX =  std::numeric_limits<float>::max();
    float minY =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    for (const auto& v : verts) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
    }

    // 2/3. Translate by -(minX, minY) and divide by the span so verts fall
    //      into [0,1]. Degenerate axes (span == 0) collapse to 0.
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float invX = (rangeX > 0.0f) ? 1.0f / rangeX : 0.0f;
    float invY = (rangeY > 0.0f) ? 1.0f / rangeY : 0.0f;

    std::vector<glm::vec2> uvs;
    uvs.reserve(verts.size());
    for (const auto& v : verts) {
        uvs.push_back({ (v.x - minX) * invX, (v.y - minY) * invY });
    }

    // 4. Rasterize into a PNG buffer.
    std::vector<uint8_t> png = packTrianglesToPNG(uvs, indices, triangleColors, size);
    if (png.empty()) return result;

    // Save to disk.
    FILE* f = std::fopen(outPath.c_str(), "wb");
    if (!f) return result;
    std::fwrite(png.data(), 1, png.size(), f);
    std::fclose(f);

    std::printf("[TexturePacking] wrote %s  (%dx%d, %zu bytes)\n",
                outPath.c_str(), size, size, png.size());

    result.uvs = std::move(uvs);
    result.pngBytes = png.size();
    result.success = true;
    return result;
}

BakedVoxelFace bakeVoxelFace(
    const std::vector<glm::vec2>& verts,
    const std::vector<uint32_t>& indices,
    const std::vector<glm::vec3>& triangleColors) {

    BakedVoxelFace out;
    int triCount = (int)(indices.size() / 3);
    if (triCount == 0 || (int)triangleColors.size() < triCount) return out;

    // Find bounding box, snap to integer cell grid.
    float minXf =  std::numeric_limits<float>::max();
    float minYf =  std::numeric_limits<float>::max();
    float maxXf = -std::numeric_limits<float>::max();
    float maxYf = -std::numeric_limits<float>::max();
    for (const auto& v : verts) {
        minXf = std::min(minXf, v.x);
        minYf = std::min(minYf, v.y);
        maxXf = std::max(maxXf, v.x);
        maxYf = std::max(maxYf, v.y);
    }
    int minX = (int)std::floor(minXf);
    int minY = (int)std::floor(minYf);
    int maxX = (int)std::ceil(maxXf);
    int maxY = (int)std::ceil(maxYf);
    int width  = std::max(1, maxX - minX);
    int height = std::max(1, maxY - minY);

    out.pixels.assign((size_t)width * height * 4, 0);
    out.triUVs.reserve(triCount);

    for (int t = 0; t < triCount; t++) {
        uint32_t i0 = indices[t * 3];
        uint32_t i1 = indices[t * 3 + 1];
        uint32_t i2 = indices[t * 3 + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) {
            out.triUVs.push_back({0.0f, 0.0f});
            continue;
        }

        // Centroid in local face space, then map to cell indices.
        glm::vec2 c = (verts[i0] + verts[i1] + verts[i2]) / 3.0f;
        int cx = (int)std::floor(c.x) - minX;
        int cy = (int)std::floor(c.y) - minY;
        cx = std::clamp(cx, 0, width  - 1);
        cy = std::clamp(cy, 0, height - 1);

        const glm::vec3& color = triangleColors[t];
        int px = (cy * width + cx) * 4;
        out.pixels[px + 0] = (uint8_t)(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        out.pixels[px + 1] = (uint8_t)(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        out.pixels[px + 2] = (uint8_t)(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        out.pixels[px + 3] = 255;

        // UV points at the center of this cell in the local patch.
        out.triUVs.push_back({
            (cx + 0.5f) / (float)width,
            (cy + 0.5f) / (float)height
        });
    }

    out.width = width;
    out.height = height;
    out.success = true;
    return out;
}
