#include "render.h"
#include <iostream>

Shader::Shader(const char* vertexSrc, const char* fragmentSrc) {
    unsigned int vert = compile(vertexSrc, GL_VERTEX_SHADER);
    unsigned int frag = compile(fragmentSrc, GL_FRAGMENT_SHADER);

    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    glDeleteProgram(program);
}

void Shader::use() const {
    glUseProgram(program);
}

unsigned int Shader::getID() const {
    return program;
}

int Shader::loc(const char* name) {
    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) return it->second;
    int location = glGetUniformLocation(program, name);
    uniformCache[name] = location;
    return location;
}

unsigned int Shader::compile(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }

    return shader;
}
