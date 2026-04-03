#include "scenes.h"
#include "../scenes/3dModeler.h"
#include "../scenes/menu.h"
#include "../scenes/createScene.h"

void registerScenes() {
    register3dModelerScene();
    registerMenuScene();
    registerCreateScene();
}
