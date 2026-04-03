#include "vectorMesh.h"
#include "../../../VisualEngine/VisualEngine.h"

void registerVectorMeshScene() {
    VE::registerScene("vectorMesh",
        nullptr,  // onEnter
        nullptr,  // onExit
        nullptr,  // onInput
        nullptr,  // onUpdate
        nullptr   // onRender
    );
}
