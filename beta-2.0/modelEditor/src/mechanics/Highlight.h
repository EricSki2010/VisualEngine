#pragma once

#include <string>

void initHighlight();
void cleanupHighlight();
void renderHoverHighlight();
void setCurrentMesh(const std::string& meshName);
const std::string& getCurrentMesh();
