#pragma once

static const char* gradientVertSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 uInvViewProj;

out vec3 vDir;

void main() {
    gl_Position = vec4(aPos, 0.9999, 1.0);
    // Reconstruct view direction from clip space
    vec4 worldPos = uInvViewProj * vec4(aPos, 1.0, 1.0);
    vDir = normalize(worldPos.xyz / worldPos.w);
}
)";

static const char* gradientFragSrc = R"(
#version 330 core
in vec3 vDir;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;

out vec4 FragColor;

void main() {
    // Map Y direction to 0-1 range
    float t = clamp(vDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 color = mix(uBottomColor, uTopColor, t);
    FragColor = vec4(color, 1.0);
}
)";
