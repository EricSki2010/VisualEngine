#pragma once

static const char* uiVertSrc = R"(
#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 uPosition;
uniform vec2 uSize;

out vec2 TexCoord;

void main() {
    vec2 pos = aPos * uSize + uPosition;
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* uiFragSrc = R"(
#version 430 core
in vec2 TexCoord;

uniform vec4 uColor;
uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 FragColor;

void main() {
    if (uUseTexture) {
        vec4 texColor = texture(uTexture, TexCoord);
        FragColor = texColor * uColor;
    } else {
        FragColor = uColor;
    }
}
)";
