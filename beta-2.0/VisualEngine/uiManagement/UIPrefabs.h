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
