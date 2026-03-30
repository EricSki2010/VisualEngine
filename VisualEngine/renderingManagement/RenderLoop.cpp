#include "render.h"
#include "ChunkMesh.h"
#include "../EngineGlobals.h"
#include "../inputManagement/Camera.h"

void processInput(float dt) {
    if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(gWindow, true);

    getGlobalCamera()->processKeyboard(gWindow, dt);
}

void update() {
    if (gNeedsRebuild)
        VE::rebuild();

    gScene->view = getGlobalCamera()->getViewMatrix();
}

void render() {
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Camera* cam = getGlobalCamera();
    glUniformMatrix4fv(gShader->loc("view"), 1, GL_FALSE, glm::value_ptr(gScene->view));
    glUniform3fv(gShader->loc("viewPos"), 1, glm::value_ptr(cam->position));

    glm::mat4 model(1.0f);
    gScene->uploadFrameUniforms(*gShader, model);
    for (auto& entry : gMergedMeshes)
        entry.mesh->draw(*gShader);

    if (gPostRenderCallback) gPostRenderCallback();
}
