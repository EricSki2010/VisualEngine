#pragma once

static const char* dotVertSrc = R"(
#version 430 core
layout (location = 0) in vec2 aPos;

uniform vec3 uCenter;
uniform float uSize;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Extract camera right and up from view matrix
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 worldPos = uCenter + right * aPos.x * uSize + up * aPos.y * uSize;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

static const char* dotFragSrc = R"(
#version 430 core

uniform vec3 uColor;
uniform float uAlpha;

out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, uAlpha);
}
)";
