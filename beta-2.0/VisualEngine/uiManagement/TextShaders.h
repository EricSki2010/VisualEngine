#pragma once

static const char* textVertSrc = R"(
#version 330 core
layout (location = 0) in vec4 aVertex; // xy = position, zw = texcoord

uniform vec2 uScreenSize;

out vec2 TexCoord;

void main() {
    // Convert pixel coords to normalized (-1 to 1)
    vec2 pos = (aVertex.xy / uScreenSize) * 2.0 - 1.0;
    pos.y = -pos.y; // flip Y so top-left is origin
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = aVertex.zw;
}
)";

static const char* textFragSrc = R"(
#version 330 core
in vec2 TexCoord;

uniform sampler2D uGlyph;
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    float alpha = texture(uGlyph, TexCoord).r;
    FragColor = vec4(uColor.rgb, uColor.a * alpha);
}
)";
