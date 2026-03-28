cmake_minimum_required(VERSION 3.20)
project(beta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Shared libraries ────────────────────────────────────────────────
find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
set(COMMON_LIBS raylib glfw opengl32 gdi32 winmm)

# ── Source files that both targets share (no main()) ────────────────
file(GLOB_RECURSE GAME_SOURCES src/game/world/*.cpp)
# Add more here as you add .cpp files to engine/ or game/entities/

# ── Main game ───────────────────────────────────────────────────────
add_executable(beta
    src/game/main.cpp
    ${GAME_SOURCES}
)
target_include_directories(beta PRIVATE src)
target_link_libraries(beta PRIVATE ${COMMON_LIBS})
set_property(TARGET beta PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Engine test ─────────────────────────────────────────────────────
add_executable(test_engine
    src/test_engine.cpp
)
target_include_directories(test_engine PRIVATE src)
target_link_libraries(test_engine PRIVATE ${COMMON_LIBS})
set_property(TARGET test_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Map Creator ────────────────────────────────────────────────────
add_executable(mapCreator
    src/mapCreator/creator.cpp
)
target_include_directories(mapCreator PRIVATE src)
target_link_libraries(mapCreator PRIVATE ${COMMON_LIBS})
set_property(TARGET mapCreator PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Map Creator V2 ────────────────────────────────────────────────
add_executable(mapCreatorV2
    src/mapCreatorV2/creator2.cpp
)
target_include_directories(mapCreatorV2 PRIVATE src)
target_link_libraries(mapCreatorV2 PRIVATE ${COMMON_LIBS})
set_property(TARGET mapCreatorV2 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── 3D Sprite ─────────────────────────────────────────────────────
add_executable(3DSprite
    src/3DSprite/3DSprite.cpp
)
target_include_directories(3DSprite PRIVATE src)
target_link_libraries(3DSprite PRIVATE ${COMMON_LIBS})
set_property(TARGET 3DSprite PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
