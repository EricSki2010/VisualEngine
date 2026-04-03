#include "UIPrefabs.h"

UIElement createButton(const std::string& id, float x, float y, float w, float h,
                       const glm::vec4& color, const std::string& label,
                       std::function<void()> onClick) {
    UIElement e;
    e.id = id;
    e.position = glm::vec2(x, y);
    e.size = glm::vec2(w, h);
    e.color = color;
    e.label = label;
    e.onClick = std::move(onClick);
    return e;
}

UIElement createPanel(const std::string& id, float x, float y, float w, float h,
                      const glm::vec4& color) {
    UIElement e;
    e.id = id;
    e.position = glm::vec2(x, y);
    e.size = glm::vec2(w, h);
    e.color = color;
    return e;
}

UIElement createImage(const std::string& id, float x, float y, float w, float h,
                      unsigned int textureId) {
    UIElement e;
    e.id = id;
    e.position = glm::vec2(x, y);
    e.size = glm::vec2(w, h);
    e.color = glm::vec4(1.0f); // white tint = show texture as-is
    e.textureId = textureId;
    return e;
}

UIElement createTextInput(const std::string& id, float x, float y, float w, float h,
                          const glm::vec4& color, const std::string& placeholder, int maxLength) {
    UIElement e;
    e.id = id;
    e.position = glm::vec2(x, y);
    e.size = glm::vec2(w, h);
    e.color = color;
    e.isTextInput = true;
    e.placeholder = placeholder;
    e.maxLength = maxLength;
    return e;
}
