#pragma once

#include "json.hpp"
#include <functional>
#include <string>

// Tool registry shared by the AI agent and the scenes.
//
// A scene registers its tools once (e.g. inside registerXxxScene()) with a
// lambda that captures the scene's static globals. The agent calls
// dispatchTool(name, args) on the main thread when the Python sidecar emits
// a tool_call. The dispatcher catches exceptions and converts them to an
// error-shaped JSON result.

namespace AI {

using Json = nlohmann::json;
using ToolHandler = std::function<Json(const Json&)>;

void registerTool(const std::string& name, ToolHandler handler);

// Returns one of:
//   { "ok": true,  "result": <any JSON> }
//   { "ok": false, "error": "..." }
Json dispatchTool(const std::string& name, const Json& args);

} // namespace AI
