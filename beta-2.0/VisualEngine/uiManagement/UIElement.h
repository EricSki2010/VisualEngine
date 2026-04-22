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
    // When true, element grows + lightens on hover, and click bounds match the inflated rect.
    bool hoverable = false;

    // Text input fields
    bool isTextInput = false;
    bool focused = false;
    std::string inputText;
    std::string placeholder;
    int maxLength = 32;
    std::function<void(std::string&)> onUnfocus;

    // When true, width is adjusted by aspect ratio so equal w/h values render as a square
    bool aspectCorrected = false;

    // Confirmation system
    bool requireConfirm = false;
    std::string confirmId;
};
