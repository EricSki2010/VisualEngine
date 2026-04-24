#include "AiHandling.h"
#include "AiProcess.h"
#include "AiTools.h"

#include "json.hpp"
#include "sceneManagement/SceneManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace AI {

namespace {
    std::unique_ptr<AiProcess> gProc;
    std::string gStatus = "Not started";
    std::string gLastResponse;
    bool gThinking = false;
    std::string gLastKnownScene; // empty until first emit

    // Enabled preference. Lazily loaded from "ai_enabled.txt" on first read;
    // default is OFF (file absent = off) so a fresh install doesn't try to
    // reach for Gemini before the user has opted in. Written on toggle.
    const char* kEnabledPath = "ai_enabled.txt";
    bool gEnabled = false;
    bool gEnabledLoaded = false;

    void ensureEnabledLoaded() {
        if (gEnabledLoaded) return;
        gEnabledLoaded = true;
        std::ifstream in(kEnabledPath);
        if (!in) return; // file missing -> default ON
        std::string line;
        std::getline(in, line);
        gEnabled = !(line == "0" || line == "off" || line == "false");
    }

    void writeEnabled() {
        std::ofstream out(kEnabledPath, std::ios::trunc);
        if (out) out << (gEnabled ? "1" : "0") << "\n";
    }

    void maybeEmitSceneChange() {
        if (!gProc) return;
        const std::string& cur = getActiveSceneName();
        if (cur == gLastKnownScene) return;
        gLastKnownScene = cur;
        nlohmann::json msg = {{"type", "scene_change"}, {"scene", cur}};
        gProc->send(msg.dump());
    }

    std::string envOr(const char* name, const std::string& fallback) {
        const char* v = std::getenv(name);
        return (v && *v) ? std::string(v) : fallback;
    }

    void handleLine(const std::string& line) {
        nlohmann::json msg;
        try {
            msg = nlohmann::json::parse(line);
        } catch (const std::exception& e) {
            std::cerr << "[AI] bad json from sidecar: " << e.what() << " line=" << line << "\n";
            return;
        }
        const std::string type = msg.value("type", "");
        if (type == "ready") {
            gStatus = "Ready";
            return;
        }
        if (type == "log") {
            std::cerr << "[AI/log] " << msg.value("text", "") << "\n";
            return;
        }
        if (type == "error") {
            gStatus = "Error: " + msg.value("text", "");
            gThinking = false;
            std::cerr << "[AI/err] " << msg.value("text", "") << "\n";
            return;
        }
        if (type == "final") {
            gLastResponse = msg.value("text", "");
            gStatus = "Done";
            gThinking = false;
            std::cerr << "[AI/final] " << gLastResponse << "\n";
            return;
        }
        if (type == "tool_call") {
            const std::string callId = msg.value("call_id", "");
            const std::string name = msg.value("name", "");
            nlohmann::json args = msg.value("args", nlohmann::json::object());
            std::cerr << "[AI/call] " << name << " " << args.dump() << "\n";
            nlohmann::json outcome = AI::dispatchTool(name, args);
            // Emit scene_change first so Python updates current_scene before
            // it processes the tool_result (which triggers the next request).
            maybeEmitSceneChange();
            nlohmann::json response = {
                {"type", "tool_result"},
                {"call_id", callId},
                {"name", name},
                {"ok", outcome.value("ok", false)},
            };
            if (outcome.value("ok", false)) {
                response["result"] = outcome.value("result", nlohmann::json());
            } else {
                response["error"] = outcome.value("error", "unknown error");
            }
            if (gProc) gProc->send(response.dump());
            return;
        }
        std::cerr << "[AI] unknown message type: " << type << "\n";
    }

    void registerBuiltinTools() {
        AI::registerTool("get_current_scene", [](const AI::Json&) -> AI::Json {
            return getActiveSceneName();
        });
    }
}

void init() {
    if (gProc) return;
    ensureEnabledLoaded();
    if (!gEnabled) {
        gStatus = "AI disabled";
        return;
    }

    registerBuiltinTools();

    const std::string python = envOr("MODELEDITOR_PYTHON", "python");
    std::string script = envOr("MODELEDITOR_AI_SCRIPT", "");
    if (script.empty()) {
        // Look next to the exe (cwd when launched from run.bat), then fall
        // back to the source tree for direct runs from the project root.
        const char* candidates[] = {
            "agent.py",
            "src/mechanics/AiHandling/agent.py",
        };
        for (const char* c : candidates) {
            if (std::filesystem::exists(c)) { script = c; break; }
        }
    }

    if (script.empty() || !std::filesystem::exists(script)) {
        gStatus = "Disabled: agent.py not found (set MODELEDITOR_AI_SCRIPT to override)";
        std::cerr << "[AI] " << gStatus << "\n";
        return;
    }

    gProc = std::make_unique<AiProcess>();
    const std::string scriptAbs = std::filesystem::absolute(script).string();
    if (!gProc->start(python, scriptAbs)) {
        gStatus = "Disabled: failed to launch python sidecar";
        gProc.reset();
        return;
    }
    gStatus = "Starting...";
}

void shutdown() {
    if (!gProc) return;
    gProc->send(R"({"type":"shutdown"})");
    gProc->stop();
    gProc.reset();
    gStatus = "Stopped";
}

void update(float /*dt*/) {
    if (!gProc) return;
    // Catch scene changes that happen outside an AI tool dispatch (e.g. user
    // pressing Space to save-and-transition, or initial scene after startup).
    maybeEmitSceneChange();
    gProc->drainInbox(&handleLine);
}

void submitPrompt(const std::string& text) {
    if (!gProc) {
        gStatus = "AI not available";
        return;
    }
    nlohmann::json msg = {{"type", "prompt"}, {"text", text}};
    gProc->send(msg.dump());
    gThinking = true;
    gStatus = "Thinking...";
    gLastResponse.clear();
}

void submitPromptWithContext(const std::string& text) {
    if (!gProc) {
        gStatus = "AI not available";
        return;
    }
    nlohmann::json msg = {{"type", "prompt"}, {"text", text}, {"with_context", true}};
    gProc->send(msg.dump());
    gThinking = true;
    gStatus = "Thinking (w/ context)...";
    gLastResponse.clear();
}

bool isInitialized() { return (bool)gProc; }
bool isThinking() { return gThinking; }
std::string getStatus() { return gStatus; }
std::string getLastResponse() { return gLastResponse; }

bool isEnabled() {
    ensureEnabledLoaded();
    return gEnabled;
}

void setEnabled(bool enabled) {
    ensureEnabledLoaded();
    if (gEnabled == enabled) return;
    gEnabled = enabled;
    writeEnabled();
    // Apply live: shut down the running sidecar if turning off, spin it up
    // if turning on. Scenes will pick up the change on their next onEnter.
    if (!enabled) {
        if (gProc) shutdown();
    } else {
        if (!gProc) init();
    }
}

} // namespace AI
