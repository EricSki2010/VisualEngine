#pragma once

#include "UIElement.h"
#include <string>
#include <vector>

struct UIGroup {
    std::string id;
    bool visible = true;
    std::vector<UIElement> elements;
};

void addUIGroup(const std::string& groupId, bool visible = true);
void removeUIGroup(const std::string& groupId);
void addToGroup(const std::string& groupId, const UIElement& element);
void removeFromGroup(const std::string& groupId, const std::string& elementId);
void setGroupVisible(const std::string& groupId, bool visible);
UIElement* getUIElement(const std::string& groupId, const std::string& elementId);
void clearUI();
void renderUI();
void processUIInput();
bool handleUIClick(double mouseX, double mouseY, int screenWidth, int screenHeight);
std::string getInputText(const std::string& groupId, const std::string& elementId);
