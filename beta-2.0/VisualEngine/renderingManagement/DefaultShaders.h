#pragma once

static const char* defaultVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoord = aTexCoord;
    Normal = normalMatrix * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

static const char* defaultFragSrc = R"(
#version 330 core
in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform int shininess;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform sampler2D textureSampler;
uniform bool useTexture;
uniform float alpha;
uniform float brightness;

out vec4 FragColor;

void main() {
    vec3 baseColor;
    if (useTexture) {
        baseColor = texture(textureSampler, TexCoord).rgb;
    } else {
        baseColor = objectColor;
    }

    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * baseColor;
    result *= brightness;
    FragColor = vec4(result, alpha);
}
)";