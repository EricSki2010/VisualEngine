#include "Selection.h"
#include <algorithm>

static std::vector<SelectedFace> sSelection;

void addSelectedFace(const glm::ivec3& pos, int faceIndex) {
    for (const auto& s : sSelection)
        if (s.blockPos == pos && s.faceIndex == faceIndex)
            return;
    sSelection.push_back({pos, faceIndex});
}

void removeSelectedFace(const glm::ivec3& pos, int faceIndex) {
    sSelection.erase(
        std::remove_if(sSelection.begin(), sSelection.end(), [&](const SelectedFace& s) {
            return s.blockPos == pos && s.faceIndex == faceIndex;
        }),
        sSelection.end()
    );
}

void clearSelection() {
    sSelection.clear();
}

const std::vector<SelectedFace>& getSelection() {
    return sSelection;
}
