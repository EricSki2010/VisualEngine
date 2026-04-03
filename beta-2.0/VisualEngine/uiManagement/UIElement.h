#pragma once

#include <glm/glm.hpp>
#include <string>
#include <functional>

struct UIElement {
    std::string id;
    glm::vec2 position;     // normalized screen-space (-1 to 1)
    glm::vec2 size;         // width/height in normalized coords
    glm::vec4 color;        // RGBA
    unsigned int textureId = 0; // GL texture ID, 0 = no texture
    std::string label;      // text to display centered on element
    float labelScale = 0.3f;
    glm::vec4 labelColor = glm::vec4(1.0f);
    std::function<void()> onClick;
    bool visible = true;

    // Text input fields
    bool isTextInput = false;
    bool focused = false;
    std::string inputText;
    std::string placeholder;
    int maxLength = 32;
};
