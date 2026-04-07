#include "RenderToTexture.h"
#include "render.h"

RenderTarget createRenderTarget(int width, int height) {
    RenderTarget rt;
    rt.width = width;
    rt.height = height;

    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    // Color texture
    glGenTextures(1, &rt.textureId);
    glBindTexture(GL_TEXTURE_2D, rt.textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.textureId, 0);

    // Depth renderbuffer
    glGenRenderbuffers(1, &rt.depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.depthRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return rt;
}

void bindRenderTarget(const RenderTarget& rt) {
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
    glViewport(0, 0, rt.width, rt.height);
}

void unbindRenderTarget(int screenWidth, int screenHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth, screenHeight);
}

void destroyRenderTarget(RenderTarget& rt) {
    if (rt.textureId) { glDeleteTextures(1, &rt.textureId); rt.textureId = 0; }
    if (rt.depthRbo) { glDeleteRenderbuffers(1, &rt.depthRbo); rt.depthRbo = 0; }
    if (rt.fbo) { glDeleteFramebuffers(1, &rt.fbo); rt.fbo = 0; }
}
