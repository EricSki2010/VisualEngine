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
// Returns true if the screen point is over any visible UI element.
bool isPointOverUI(double mouseX, double mouseY, int screenWidth, int screenHeight);
std::string getInputText(const std::string& groupId, const std::string& elementId);
bool isAnyInputFocused();
// Returns the number of wrapped lines the given multiline text input needs
// to display its current inputText. Uses the element's actual width (in
// current window dimensions) and labelScale so it matches the renderer.
int inputWrappedLineCount(const UIElement& e);
bool hasPendingConfirm();
std::string getPendingConfirmId();
void cancelPendingConfirm();

// Dropdown: creates a toggle button + a group of option buttons that appear/disappear.
// offsetX/offsetY control where the options appear relative to the main button.
// onSelect receives the index and label of the selected option.
void createDropdown(const std::string& groupId, const std::string& id,
                    float x, float y, float w, float h,
                    const glm::vec4& color, const std::string& label,
                    const std::vector<std::string>& options,
                    std::function<void(int index, const std::string& option)> onSelect,
                    float offsetX = 0.0f, float offsetY = -0.09f);
