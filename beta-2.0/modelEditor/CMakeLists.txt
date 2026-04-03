cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(modelEditor)

set(CMAKE_CXX_STANDARD 17)

add_executable(modelEditor
    main.cpp
    ../VisualEngine/VisualEngine.cpp
    ../VisualEngine/renderingManagement/Mesh.cpp
    ../VisualEngine/renderingManagement/Scene.cpp
    ../VisualEngine/renderingManagement/Shader.cpp
    ../VisualEngine/renderingManagement/Texture.cpp
    ../VisualEngine/renderingManagement/ChunkMesh.cpp
    ../VisualEngine/renderingManagement/Overlay.cpp
    ../VisualEngine/renderingManagement/RenderLoop.cpp
    ../VisualEngine/sceneManagement/SceneManager.cpp
    ../VisualEngine/memoryManagement/memory.cpp
    ../VisualEngine/uiManagement/UIRenderer.cpp
    ../VisualEngine/uiManagement/UIManager.cpp
    ../VisualEngine/uiManagement/UIPrefabs.cpp
    ../VisualEngine/uiManagement/TextRenderer.cpp
    ../VisualEngine/inputManagement/Camera.cpp
    ../VisualEngine/inputManagement/Collision.cpp
    ../VisualEngine/inputManagement/Raycasting.cpp
    src/Highlight.cpp
    src/Selection.cpp
    src/scenes.cpp
    src/prefabs/templates.cpp
    src/scenes/3dModeler.cpp
    src/scenes/createScene.cpp
    src/scenes/menu.cpp
    E:/glad/src/glad.c
)

target_include_directories(modelEditor PRIVATE
    ../VisualEngine
    E:/glad/include
    E:/glfw-3.4.bin.WIN64/glfw-3.4.bin.WIN64/include
    C:/vcpkg/installed/x64-windows/include
    C:/vcpkg/installed/x64-windows-static/include
)

target_link_directories(modelEditor PRIVATE
    E:/glfw-3.4.bin.WIN64/glfw-3.4.bin.WIN64/lib-vc2022
)

find_package(Freetype REQUIRED)

target_link_libraries(modelEditor PRIVATE
    glfw3
    opengl32
    gdi32
    user32
    shell32
    Freetype::Freetype
)
