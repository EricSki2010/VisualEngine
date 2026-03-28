#include "render.h"

Scene::Scene(float aspectRatio) {
    projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    view = glm::mat4(1.0f);

    light.position = glm::vec3(40.0f, 60.0f, 30.0f);
    light.color = glm::vec3(1.0f, 1.0f, 1.0f);
    light.ambientStrength = 0.3f;
    light.specularStrength = 0.5f;
    light.shininess = 32;
}

glm::mat3 Scene::getNormalMatrix(const glm::mat4& model) {
    return glm::mat3(glm::transpose(glm::inverse(model)));
}

void Scene::uploadStaticUniforms(Shader& shader) {
    shader.use();
    glUniformMatrix4fv(shader.loc("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(shader.loc("view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform3fv(shader.loc("lightPos"), 1, glm::value_ptr(light.position));
    glUniform3fv(shader.loc("lightColor"), 1, glm::value_ptr(light.color));
    glUniform1f(shader.loc("ambientStrength"), light.ambientStrength);
    glUniform1f(shader.loc("specularStrength"), light.specularStrength);
    glUniform1i(shader.loc("shininess"), light.shininess);
}

void Scene::uploadFrameUniforms(Shader& shader, const glm::mat4& model) {
    glUniformMatrix4fv(shader.loc("model"), 1, GL_FALSE, glm::value_ptr(model));
    glm::mat3 normalMatrix = getNormalMatrix(model);
    glUniformMatrix3fv(shader.loc("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
}
