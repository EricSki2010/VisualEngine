#include "render.h"
#include "ChunkMesh.h"
#include "../EngineGlobals.h"
#include "../sceneManagement/SceneManager.h"
#include "../inputManagement/Camera.h"

void processInput(float dt) {
    getGlobalCamera()->processKeyboard(ctx.window, dt);

    SceneDef* scene = getActiveScene();
    if (scene && scene->onInput)
        scene->onInput(dt);
}

void update() {
    if (ctx.needsRebuild)
        VE::rebuild();

    ctx.scene->view = getGlobalCamera()->getViewMatrix();

    SceneDef* scene = getActiveScene();
    if (scene && scene->onUpdate)
        scene->onUpdate();
}

void render() {
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ctx.shader->use();
    Camera* cam = getGlobalCamera();
    glUniformMatrix4fv(ctx.shader->loc("view"), 1, GL_FALSE, glm::value_ptr(ctx.scene->view));
    glUniform3fv(ctx.shader->loc("viewPos"), 1, glm::value_ptr(cam->position));
    glm::vec3 lightPos = cam->position + glm::vec3(0.0f, 5.0f, 0.0f);
    glUniform3fv(ctx.shader->loc("lightPos"), 1, glm::value_ptr(lightPos));

    glm::mat4 model(1.0f);
    ctx.scene->uploadFrameUniforms(*ctx.shader, model);
    for (auto& entry : ctx.mergedMeshes)
        entry.mesh->draw(*ctx.shader);

    SceneDef* scene = getActiveScene();
    if (scene && scene->onRender)
        scene->onRender();
}
