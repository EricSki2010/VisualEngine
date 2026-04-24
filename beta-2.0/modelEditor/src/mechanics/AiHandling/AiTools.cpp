#include "AiTools.h"

#include <iostream>
#include <unordered_map>

namespace AI {

namespace {
    std::unordered_map<std::string, ToolHandler>& registry() {
        static std::unordered_map<std::string, ToolHandler> r;
        return r;
    }
}

void registerTool(const std::string& name, ToolHandler handler) {
    registry()[name] = std::move(handler);
}

Json dispatchTool(const std::string& name, const Json& args) {
    auto& r = registry();
    auto it = r.find(name);
    if (it == r.end()) {
        return Json{{"ok", false}, {"error", "unknown tool: " + name}};
    }
    try {
        Json result = it->second(args);
        return Json{{"ok", true}, {"result", result}};
    } catch (const std::exception& e) {
        return Json{{"ok", false}, {"error", std::string(e.what())}};
    } catch (...) {
        return Json{{"ok", false}, {"error", "unknown exception in tool"}};
    }
}

} // namespace AI
