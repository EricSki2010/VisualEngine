#include "DotRenderer.h"
#include "DotShaders.h"
#include "render.h"
#include "../EngineGlobals.h"

static unsigned int sDotVAO = 0;
static unsigned int sDotVBO = 0;
static unsigned int sDotEBO = 0;
static Shader* sDotShader = nullptr;

void initDotRenderer() {
    sDotShader = new Shader(dotVertSrc, dotFragSrc);

    // Unit quad centered at origin
    float verts[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &sDotVAO);
    glGenBuffers(1, &sDotVBO);
    glGenBuffers(1, &sDotEBO);

    glBindVertexArray(sDotVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sDotVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sDotEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void cleanupDotRenderer() {
    if (sDotVAO) { glDeleteVertexArrays(1, &sDotVAO); sDotVAO = 0; }
    if (sDotVBO) { glDeleteBuffers(1, &sDotVBO); sDotVBO = 0; }
    if (sDotEBO) { glDeleteBuffers(1, &sDotEBO); sDotEBO = 0; }
    delete sDotShader;
    sDotShader = nullptr;
}

void drawDot(const glm::vec3& position, float size, const glm::vec3& color) {
    if (!sDotShader) return;

    sDotShader->use();
    glUniform3fv(sDotShader->loc("uCenter"), 1, glm::value_ptr(position));
    glUniform1f(sDotShader->loc("uSize"), size);
    glUniform3fv(sDotShader->loc("uColor"), 1, glm::value_ptr(color));
    glUniformMatrix4fv(sDotShader->loc("view"), 1, GL_FALSE, glm::value_ptr(ctx.scene->view));
    glUniformMatrix4fv(sDotShader->loc("projection"), 1, GL_FALSE, glm::value_ptr(ctx.scene->projection));

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(sDotVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
}
