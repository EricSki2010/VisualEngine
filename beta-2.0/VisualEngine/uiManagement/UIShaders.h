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
uniform vec2 uPixelSize;      // button width/height in pixels (post-aspect-correction)
uniform float uCornerRadius;  // corner radius in pixels; 0 = sharp rect

out vec4 FragColor;

void main() {
    vec4 base = uUseTexture ? texture(uTexture, TexCoord) * uColor : uColor;

    if (uCornerRadius > 0.0) {
        // Rounded-box SDF in pixel space, circular corners regardless of aspect.
        vec2 p = (TexCoord - 0.5) * uPixelSize;
        vec2 b = uPixelSize * 0.5 - vec2(uCornerRadius);
        vec2 q = abs(p) - b;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - uCornerRadius;
        float aa = max(fwidth(d), 1e-4);
        base.a *= 1.0 - smoothstep(-aa, aa, d);
    }

    FragColor = base;
}
)";
