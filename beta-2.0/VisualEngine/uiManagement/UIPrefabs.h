#pragma once

#include "UIElement.h"
#include <functional>

UIElement createButton(const std::string& id, float x, float y, float w, float h,
                       const glm::vec4& color, const std::string& label,
                       std::function<void()> onClick);

UIElement createPanel(const std::string& id, float x, float y, float w, float h,
                      const glm::vec4& color);

UIElement createImage(const std::string& id, float x, float y, float w, float h,
                      unsigned int textureId);

UIElement createTextInput(const std::string& id, float x, float y, float w, float h,
                          const glm::vec4& color, const std::string& placeholder, int maxLength = 32);

// Creates a button positioned relative to a parent element.
// anchorX: 0.0 = left edge, 1.0 = right edge of parent
// anchorY: 0.0 = bottom edge, 1.0 = top edge of parent
// widthRatio/heightRatio: size as fraction of parent (e.g. 0.8 = 80% of parent height)
// padding: inset from anchor as fraction of parent height
UIElement createSubButton(const std::string& id,
                          float parentX, float parentY, float parentW, float parentH,
                          float anchorX, float anchorY,
                          float widthRatio, float heightRatio, float padding,
                          const glm::vec4& color, const std::string& label,
                          std::function<void()> onClick);
