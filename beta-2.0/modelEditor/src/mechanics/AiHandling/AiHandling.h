#pragma once

#include <string>

// Top-level facade for the AI agent feature.
//
// Lifecycle:
//   init()      -> spawn Python sidecar and reader thread (call once at startup)
//   update(dt)  -> drain sidecar output, execute tool calls on main thread
//   submitPrompt(text) -> send a user prompt; response arrives asynchronously
//   shutdown()  -> close sidecar, join thread (call once at exit)
//
// If the sidecar fails to start (missing python, missing key, etc.), the
// facade remains a no-op and surfaces a status string. The rest of the app
// keeps working.

namespace AI {

void init();
void shutdown();
void update(float dt);

// Persistent on/off flag. When disabled, init() does nothing, dependency
// setup is skipped, and scenes should not render the AI panel. Backed by
// a text file next to the exe so the preference survives restarts.
bool isEnabled();
void setEnabled(bool enabled);

void submitPrompt(const std::string& text);
// Same as submitPrompt but also asks the sidecar to expose the scene-context
// tool group (camera, hovered block, paint_face, etc.) for this one prompt.
void submitPromptWithContext(const std::string& text);

// UI-facing accessors.
bool isInitialized();
bool isThinking();
std::string getStatus();        // short one-line status, e.g. "Ready", "Thinking...", "Error: ..."
std::string getLastResponse();  // last final text from the agent

} // namespace AI
