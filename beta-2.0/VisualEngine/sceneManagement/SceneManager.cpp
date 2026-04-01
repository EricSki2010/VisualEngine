#include "SceneManager.h"

static std::unordered_map<std::string, SceneDef> sScenes;
static std::string sActiveSceneName;
static SceneDef* sActiveScene = nullptr;

void registerScene(const std::string& name, const SceneDef& scene) {
    sScenes[name] = scene;
}

void setActiveScene(const std::string& name) {
    // Exit current scene
    if (sActiveScene && sActiveScene->onExit)
        sActiveScene->onExit();

    auto it = sScenes.find(name);
    if (it != sScenes.end()) {
        sActiveSceneName = name;
        sActiveScene = &it->second;
        if (sActiveScene->onEnter)
            sActiveScene->onEnter();
    }
}

const std::string& getActiveSceneName() {
    return sActiveSceneName;
}

SceneDef* getActiveScene() {
    return sActiveScene;
}
