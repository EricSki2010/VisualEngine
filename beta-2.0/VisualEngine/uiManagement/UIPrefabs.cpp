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
    e.hoverable = true;
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

UIElement createSubButton(const std::string& id,
                          float parentX, float parentY, float parentW, float parentH,
                          float anchorX, float anchorY,
                          float widthRatio, float heightRatio, float padding,
                          const glm::vec4& color, const std::string& label,
                          std::function<void()> onClick) {
    float subW = parentW * widthRatio;
    float subH = parentH * heightRatio;
    float pad = parentH * padding;

    // Position based on anchor (0=left/bottom, 1=right/top)
    // anchorX: 0 = flush left + padding, 1 = flush right - padding
    // anchorY: 0 = flush bottom + padding, 1 = flush top - padding
    float subX = parentX + (parentW - subW - pad) * anchorX + pad * (1.0f - anchorX);
    float subY = parentY + (parentH - subH - pad) * anchorY + pad * (1.0f - anchorY);

    UIElement e;
    e.id = id;
    e.position = glm::vec2(subX, subY);
    e.size = glm::vec2(subW, subH);
    e.color = color;
    e.label = label;
    e.labelScale = 0.2f;
    e.onClick = std::move(onClick);
    e.hoverable = true;
    return e;
}
