#include "UIManager.h"
#include "UIRenderer.h"
#include "TextRenderer.h"
#include "../EngineGlobals.h"
#include <algorithm>

static std::vector<UIGroup> sGroups;
static bool sWasLeftDown = false;
static bool sKeyStates[GLFW_KEY_LAST + 1] = {};

// Confirmation system
static std::string sPendingConfirmId;
static std::function<void()> sPendingAction;
static std::string sPendingElementId;

// Find the currently focused text input across all groups
static UIElement* getFocusedInput() {
    for (auto& g : sGroups) {
        if (!g.visible) continue;
        for (auto& e : g.elements)
            if (e.isTextInput && e.focused) return &e;
    }
    return nullptr;
}

// Unfocus all text inputs
static void unfocusAll() {
    for (auto& g : sGroups)
        for (auto& e : g.elements)
            if (e.isTextInput && e.focused) {
                e.focused = false;
                if (e.onUnfocus) e.onUnfocus(e.inputText);
            }
}

void addUIGroup(const std::string& groupId, bool visible) {
    for (const auto& g : sGroups)
        if (g.id == groupId) return;
    sGroups.push_back({groupId, visible, {}});
}

void removeUIGroup(const std::string& groupId) {
    sGroups.erase(
        std::remove_if(sGroups.begin(), sGroups.end(),
            [&](const UIGroup& g) { return g.id == groupId; }),
        sGroups.end()
    );
}

void addToGroup(const std::string& groupId, const UIElement& element) {
    for (auto& g : sGroups) {
        if (g.id == groupId) {
            g.elements.push_back(element);
            return;
        }
    }
}

void removeFromGroup(const std::string& groupId, const std::string& elementId) {
    for (auto& g : sGroups) {
        if (g.id == groupId) {
            g.elements.erase(
                std::remove_if(g.elements.begin(), g.elements.end(),
                    [&](const UIElement& e) { return e.id == elementId; }),
                g.elements.end()
            );
            return;
        }
    }
}

void setGroupVisible(const std::string& groupId, bool visible) {
    for (auto& g : sGroups) {
        if (g.id == groupId) {
            g.visible = visible;
            return;
        }
    }
}

UIElement* getUIElement(const std::string& groupId, const std::string& elementId) {
    for (auto& g : sGroups) {
        if (g.id == groupId) {
            for (auto& e : g.elements)
                if (e.id == elementId) return &e;
            return nullptr;
        }
    }
    return nullptr;
}

void clearUI() {
    sGroups.clear();
}

// Convert normalized coords to pixel coords for text rendering
static float getCorrectedWidth(const UIElement& e) {
    if (e.aspectCorrected && ctx.width > 0) {
        float aspect = (float)ctx.width / (float)ctx.height;
        return e.size.x / aspect;
    }
    return e.size.x;
}

static void normToPixel(const UIElement& e, float& pixX, float& pixY, float& pixW, float& pixH) {
    pixX = (e.position.x + 1.0f) / 2.0f * ctx.width;
    pixY = (1.0f - e.position.y) / 2.0f * ctx.height;
    pixW = getCorrectedWidth(e) / 2.0f * ctx.width;
    pixH = e.size.y / 2.0f * ctx.height;
}

void renderUI() {
    for (const auto& g : sGroups) {
        if (!g.visible) continue;
        for (const auto& e : g.elements) {
            // Draw focused text inputs or pending confirm buttons with lighter color
            bool isPending = !sPendingConfirmId.empty() && e.requireConfirm &&
                             e.id == sPendingElementId;
            if ((e.isTextInput && e.focused) || isPending) {
                UIElement highlight = e;
                highlight.color = e.color + glm::vec4(0.15f, 0.15f, 0.15f, 0.0f);
                drawUIElement(highlight);
            } else {
                drawUIElement(e);
            }

            // Draw label (buttons)
            if (!e.label.empty() && !e.isTextInput) {
                float pixX, pixY, pixW, pixH;
                normToPixel(e, pixX, pixY, pixW, pixH);
                float textW = measureText(e.label, e.labelScale);
                float textH = measureTextHeight(e.labelScale);
                drawText(e.label,
                    pixX + pixW / 2.0f - textW / 2.0f,
                    pixY - pixH / 2.0f - textH / 2.0f,
                    e.labelScale, e.labelColor);
            }

            // Draw text input content
            if (e.isTextInput) {
                float pixX, pixY, pixW, pixH;
                normToPixel(e, pixX, pixY, pixW, pixH);
                float textH = measureTextHeight(e.labelScale);
                float padding = 8.0f;

                if (e.inputText.empty() && !e.focused) {
                    // Show placeholder
                    drawText(e.placeholder, pixX + padding,
                        pixY - pixH / 2.0f - textH / 2.0f,
                        e.labelScale, {0.5f, 0.5f, 0.5f, 0.7f});
                } else {
                    // Show typed text + cursor
                    std::string display = e.inputText;
                    if (e.focused) {
                        // Blinking cursor
                        double time = glfwGetTime();
                        if (((int)(time * 2.0)) % 2 == 0)
                            display += "|";
                    }
                    drawText(display, pixX + padding,
                        pixY - pixH / 2.0f - textH / 2.0f,
                        e.labelScale, e.labelColor);
                }
            }
        }
    }
}

// Convert screen pixel coords to normalized (-1 to 1)
static glm::vec2 screenToNorm(double mx, double my, int w, int h) {
    return glm::vec2(
        (float)(mx / w) * 2.0f - 1.0f,
        1.0f - (float)(my / h) * 2.0f
    );
}

static void processTextInput() {
    UIElement* input = getFocusedInput();
    if (!input) return;

    // Letters A-Z
    for (int key = GLFW_KEY_A; key <= GLFW_KEY_Z; key++) {
        bool down = glfwGetKey(ctx.window, key) == GLFW_PRESS;
        if (down && !sKeyStates[key] && (int)input->inputText.size() < input->maxLength) {
            bool shift = glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            char c = shift ? ('A' + (key - GLFW_KEY_A)) : ('a' + (key - GLFW_KEY_A));
            input->inputText += c;
        }
        sKeyStates[key] = down;
    }

    // Numbers 0-9
    for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; key++) {
        bool down = glfwGetKey(ctx.window, key) == GLFW_PRESS;
        if (down && !sKeyStates[key] && (int)input->inputText.size() < input->maxLength) {
            input->inputText += ('0' + (key - GLFW_KEY_0));
        }
        sKeyStates[key] = down;
    }

    // Space
    {
        bool down = glfwGetKey(ctx.window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (down && !sKeyStates[GLFW_KEY_SPACE] && (int)input->inputText.size() < input->maxLength)
            input->inputText += ' ';
        sKeyStates[GLFW_KEY_SPACE] = down;
    }

    // Backspace
    {
        bool down = glfwGetKey(ctx.window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        if (down && !sKeyStates[GLFW_KEY_BACKSPACE] && !input->inputText.empty())
            input->inputText.pop_back();
        sKeyStates[GLFW_KEY_BACKSPACE] = down;
    }

    // Minus/underscore
    {
        bool down = glfwGetKey(ctx.window, GLFW_KEY_MINUS) == GLFW_PRESS;
        if (down && !sKeyStates[GLFW_KEY_MINUS] && (int)input->inputText.size() < input->maxLength) {
            bool shift = glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            input->inputText += shift ? '_' : '-';
        }
        sKeyStates[GLFW_KEY_MINUS] = down;
    }
}

void processUIInput() {
    bool leftDown = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (leftDown && !sWasLeftDown) {
        double mx, my;
        glfwGetCursorPos(ctx.window, &mx, &my);
        handleUIClick(mx, my, ctx.width, ctx.height);
    }
    sWasLeftDown = leftDown;

    processTextInput();
}

bool handleUIClick(double mouseX, double mouseY, int screenWidth, int screenHeight) {
    glm::vec2 norm = screenToNorm(mouseX, mouseY, screenWidth, screenHeight);

    unfocusAll();

    for (int gi = (int)sGroups.size() - 1; gi >= 0; gi--) {
        UIGroup& g = sGroups[gi];
        if (!g.visible) continue;

        for (int ei = (int)g.elements.size() - 1; ei >= 0; ei--) {
            UIElement& e = g.elements[ei];
            if (!e.visible) continue;

            float corrW = getCorrectedWidth(e);
            if (norm.x >= e.position.x && norm.x <= e.position.x + corrW &&
                norm.y >= e.position.y && norm.y <= e.position.y + e.size.y) {

                if (e.isTextInput) {
                    e.focused = true;
                    cancelPendingConfirm();
                    return true;
                }

                // Button requires confirmation — set or replace pending
                if (e.requireConfirm && !e.confirmId.empty()) {
                    sPendingConfirmId = e.confirmId;
                    sPendingAction = e.onClick;
                    sPendingElementId = e.id;
                    return true;
                }

                // Check if this button confirms a pending action
                // (only non-requireConfirm buttons with matching confirmId)
                if (!sPendingConfirmId.empty() && !e.requireConfirm &&
                    !e.confirmId.empty() && e.confirmId == sPendingConfirmId) {
                    auto action = std::move(sPendingAction);
                    cancelPendingConfirm();
                    if (action) action();
                    return true;
                }

                // Normal button — cancel any pending and run
                cancelPendingConfirm();
                if (e.onClick) e.onClick();
                return true;
            }
        }
    }

    // Clicked empty space — cancel pending
    cancelPendingConfirm();
    return false;
}

bool hasPendingConfirm() {
    return !sPendingConfirmId.empty();
}

std::string getPendingConfirmId() {
    return sPendingConfirmId;
}

void cancelPendingConfirm() {
    sPendingConfirmId.clear();
    sPendingAction = nullptr;
    sPendingElementId.clear();
}

std::string getInputText(const std::string& groupId, const std::string& elementId) {
    UIElement* e = getUIElement(groupId, elementId);
    if (e && e->isTextInput) return e->inputText;
    return "";
}
