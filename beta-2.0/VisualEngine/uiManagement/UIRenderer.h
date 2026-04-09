#pragma once

#include "UIElement.h"

class Shader;

void initUIRenderer();
void cleanupUIRenderer();
void drawUIElement(const UIElement& element);
Shader* getUIShader();
