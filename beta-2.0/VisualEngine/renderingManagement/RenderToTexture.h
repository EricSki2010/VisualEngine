#pragma once

struct RenderTarget {
    unsigned int fbo = 0;
    unsigned int textureId = 0;
    unsigned int depthRbo = 0;
    int width = 0;
    int height = 0;
};

RenderTarget createRenderTarget(int width, int height);
void bindRenderTarget(const RenderTarget& rt);
void unbindRenderTarget(int screenWidth, int screenHeight);
void destroyRenderTarget(RenderTarget& rt);
