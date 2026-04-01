# Build Requirements

## Compiler & Build Tools
- **MSVC** (Visual Studio 2022 C++ build tools) or compatible C++17 compiler
- **CMake** 3.20+
- **MSBuild** (included with Visual Studio)

## Graphics
- **OpenGL 3.3** compatible GPU and drivers
- **GLAD** — OpenGL loader (source at `E:/glad/`)
- **GLFW 3.4** — windowing and input (prebuilt binaries, `E:/glfw-3.4.bin.WIN64/`)

## Math & Image Loading
- **GLM** — header-only math library (installed via vcpkg)
- **stb_image** — single-header image loader for textures (installed via vcpkg)

## Package Manager
- **vcpkg** — used for GLM and stb_image (`C:/vcpkg/installed/x64-windows/`)

## System Libraries (Windows)
- `opengl32` — OpenGL runtime
- `gdi32` — Windows graphics device interface
- `user32` — Windows user interface
- `shell32` — Windows shell

## Install Summary
1. Install Visual Studio 2022 with C++ desktop development workload
2. Install CMake 3.20+
3. Install vcpkg, then: `vcpkg install glm stb:x64-windows`
4. Download GLFW 3.4 prebuilt binaries for Windows (64-bit, lib-vc2022)
5. Download GLAD (OpenGL 3.3 Core Profile) from https://glad.dav1d.de
6. Update paths in CMakeLists.txt if your install locations differ
