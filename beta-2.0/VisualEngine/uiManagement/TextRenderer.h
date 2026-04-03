#pragma once

#include <string>
#include <glm/glm.hpp>

bool initTextRenderer(const char* fontPath, int fontSize);
void cleanupTextRenderer();
void drawText(const std::string& text, float x, float y, float scale, const glm::vec4& color);
float measureText(const std::string& text, float scale);
float measureTextHeight(float scale);
