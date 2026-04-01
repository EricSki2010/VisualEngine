#pragma once

#include <functional>
#include <string>
#include <unordered_map>

struct SceneDef {
    std::function<void()> onEnter;
    std::function<void()> onExit;
    std::function<void(float dt)> onInput;
    std::function<void()> onUpdate;
    std::function<void()> onRender;
};

void registerScene(const std::string& name, const SceneDef& scene);
void setActiveScene(const std::string& name);
const std::string& getActiveSceneName();
SceneDef* getActiveScene();
