#include "render.h"
#include "ChunkMesh.h"
#include "../EngineGlobals.h"
#include "../inputManagement/Camera.h"

void processInput(float dt) {
    if (glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(ctx.window, true);

    getGlobalCamera()->processKeyboard(ctx.window, dt);
}

void update() {
    if (ctx.needsRebuild)
        VE::rebuild();

    ctx.scene->view = getGlobalCamera()->getViewMatrix();
}

void render() {
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Camera* cam = getGlobalCamera();
    glUniformMatrix4fv(ctx.shader->loc("view"), 1, GL_FALSE, glm::value_ptr(ctx.scene->view));
    glUniform3fv(ctx.shader->loc("viewPos"), 1, glm::value_ptr(cam->position));

    glm::mat4 model(1.0f);
    ctx.scene->uploadFrameUniforms(*ctx.shader, model);
    for (auto& entry : ctx.mergedMeshes)
        entry.mesh->draw(*ctx.shader);

    if (ctx.postRenderCallback) ctx.postRenderCallback();
}
