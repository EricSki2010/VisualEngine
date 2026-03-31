cmake_minimum_required(VERSION 3.20)
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
    ../VisualEngine/inputManagement/Camera.cpp
    ../VisualEngine/inputManagement/Collision.cpp
    ../VisualEngine/inputManagement/Raycasting.cpp
    src/Highlight.cpp
    src/Selection.cpp
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

target_link_libraries(modelEditor PRIVATE
    glfw3
    opengl32
    gdi32
    user32
    shell32
)
