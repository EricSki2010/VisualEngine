#pragma once

#include <glm/glm.hpp>

void initGradientBackground();
void cleanupGradientBackground();
void setGradientColors(const glm::vec3& top, const glm::vec3& bottom);
void enableGradientBackground(bool enable);
bool isGradientBackgroundEnabled();
void drawGradientBackground();
