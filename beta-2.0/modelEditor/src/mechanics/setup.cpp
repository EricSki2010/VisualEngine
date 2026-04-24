#include "setup.h"
#include "AiHandling/AiHandling.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

void setupDirectories() {
    std::filesystem::create_directories("assets/saves/3dModels");
    std::filesystem::create_directories("assets/saves/vectorMeshes");
}

static std::string pythonExe() {
    const char* v = std::getenv("MODELEDITOR_PYTHON");
    return (v && *v) ? std::string(v) : std::string("python");
}

void setupAiDependencies() {
    if (!AI::isEnabled()) return;
    const std::string py = pythonExe();

    // Skip if already importable.
    const std::string check = "\"" + py + "\" -c \"import google.genai\" >nul 2>&1";
    if (std::system(check.c_str()) == 0) return;

    std::cerr << "[AI] Installing Python dependency google-genai (one-time)...\n";
    const std::string install = "\"" + py + "\" -m pip install --quiet google-genai";
    int rc = std::system(install.c_str());
    if (rc != 0) {
        std::cerr << "[AI] pip install failed (rc=" << rc
                  << "). Install manually: " << py << " -m pip install google-genai\n";
    } else {
        std::cerr << "[AI] google-genai installed.\n";
    }
}
