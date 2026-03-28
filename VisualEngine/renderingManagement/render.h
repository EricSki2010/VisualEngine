#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <string>

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float ambientStrength;
    float specularStrength;
    int shininess;
};

class Shader {
public:
    unsigned int program;

    Shader(const char* vertexSrc, const char* fragmentSrc);
    void use();
    unsigned int getID();
    int loc(const char* name);
private:
    unsigned int compile(const char* source, GLenum type);
    std::unordered_map<std::string, int> uniformCache;
};

class Scene {
public:
    glm::mat4 projection;
    glm::mat4 view;
    PointLight light;

    Scene(float aspectRatio);
    glm::mat3 getNormalMatrix(const glm::mat4& model);
    void uploadStaticUniforms(Shader& shader);
    void uploadFrameUniforms(Shader& shader, const glm::mat4& model);
};

class Texture {
public:
    unsigned int id;

    Texture(const char* filepath);
    void bind(int unit = 0);
    static Texture* loadFromFile(const char* filepath);
};

class Mesh {
public:
    unsigned int VAO, VBO, EBO;
    int indexCount;
    Texture* texture;
    glm::vec3 color;

    Mesh(float* vertices, int vertexCount, unsigned int* indices, int indexCount);
    void setTexture(Texture* tex);
    void setColor(glm::vec3 col);
    void draw(Shader& shader);
private:
    void computeNormals(float* vertices, int vertexCount, unsigned int* indices, int indexCount,
                        float* outBuffer);
};
