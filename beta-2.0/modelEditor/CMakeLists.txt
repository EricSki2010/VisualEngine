cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
project(modelEditor)

set(CMAKE_CXX_STANDARD 17)
set_property(GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable AddressSanitizer for memory bug detection (uncomment for debugging)
# add_compile_options(/fsanitize=address /Zi)
# add_link_options(/DEBUG)

add_executable(modelEditor
    main.cpp
    ../VisualEngine/VisualEngine.cpp
    ../VisualEngine/renderingManagement/Mesh.cpp
    ../VisualEngine/renderingManagement/Scene.cpp
    ../VisualEngine/renderingManagement/Shader.cpp
    ../VisualEngine/renderingManagement/Texture.cpp
    ../VisualEngine/renderingManagement/ChunkMesh.cpp
    ../VisualEngine/renderingManagement/Overlay.cpp
    ../VisualEngine/renderingManagement/LineRenderer.cpp
    ../VisualEngine/renderingManagement/DotRenderer.cpp
    ../VisualEngine/renderingManagement/GradientBackground.cpp
    ../VisualEngine/renderingManagement/RenderToTexture.cpp
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
    src/mechanics/Highlight.cpp
    src/mechanics/Selection.cpp
    src/mechanics/scenes.cpp
    src/mechanics/setup.cpp
    src/prefabs/templates.cpp
    src/scenes/3dModeler.cpp
    src/scenes/createScene.cpp
    src/scenes/vectorMesh.cpp
    src/scenes/menu.cpp
    E:/glad/src/glad.c
)

target_include_directories(modelEditor PRIVATE
    ../VisualEngine
    E:/glad/include
    C:/vcpkg/installed/x64-windows-static/include
)

set_property(TARGET modelEditor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)

target_link_libraries(modelEditor PRIVATE
    glfw
    opengl32
    gdi32
    user32
    shell32
    Freetype::Freetype
)
