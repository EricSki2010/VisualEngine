# VisualEngine + Model Editor

A custom OpenGL 3.3 voxel-style 3D model editor built from scratch in C++.
The engine (VisualEngine) handles rendering, input, collision, and UI.
The application (modelEditor) is a tool for building, painting, and editing
3D models made of blocks and custom shapes.

## What the Engine Can Do (VisualEngine)

- OpenGL 3.3 rendering with Phong lighting (diffuse, specular, ambient)
- Mesh registration from code or binary .mesh files
- Per-instance mesh placement with position and rotation (degrees, any axis)
- Automatic face culling between adjacent solid blocks
- Per-triangle collision detection via Moller-Trumbore ray-triangle intersection
- AABB fast-path raycasting for rectangular meshes
- FPS camera with mouse look (Q toggle) and WASD movement
- Scene system with lifecycle hooks (enter, exit, input, update, render)
- UI system with buttons, panels, text inputs, dropdowns, images
- Text rendering via FreeType font rasterization
- Line and dot rendering for wireframes and debug visualization
- Triangle overlay system for selection highlights
- Render-to-texture for preview thumbnails
- Gradient background that follows camera direction
- Binary save/load system with RLE compression
- Model file format (.mdl) with block types, placements, rotations,
  per-face paint colors, and 16-color palette

## What the Model Editor Can Do

### Build Mode
- Place blocks on a 3D grid (cubes, wedges, or custom shapes)
- Select faces by clicking or drag-selecting rectangles of faces
- Extrude new blocks from selected faces (Ctrl+A)
- Delete blocks (Ctrl+D)
- Replace blocks with a different mesh type (Ctrl+A in block select)
- Rotate placed blocks in 90-degree increments on any axis (R + 1/2/3)
- 15 mesh slots: 2 built-in (cube, wedge) + 13 custom slots
- Scroll wheel to cycle through available meshes
- Tab to toggle full-block selection mode
- Ctrl+Z undo with 50-step history
- Ctrl+Tab shortcut to edit the current slot in the vectorMesh editor

### Paint Mode
- Select individual triangles on any mesh face
- Three selection methods:
  - Left-drag brush: paint over triangles as you drag
  - Right-click: select single triangle
  - Right-drag: select all coplanar triangles (same surface)
- 16-color palette displayed as a fan widget in the corner
- Set colors via RGB (0-255) or Hex input with brightness control
- Apply selected color to selected faces (Ctrl+A)
- Colors render in real-time on the 3D model
- Palette and per-face colors save/load with the model file

### Custom Mesh Editor (Vector Mesh)
- Create custom 3D shapes from scratch
- Dot mode: place vertices with snap-to-edge (configurable snap count)
- Line mode: connect dots into edges
- Plane mode: create triangles from 3 dots
- Flip triangle normals manually when needed
- Saves as .vmesh (editable) + .mesh (renderable with pre-computed normals)
- Custom meshes appear in the block selector for placement
- Ctrl+Z undo with 50-step history
- Ctrl+Tab to save and return to the 3D modeler
- Built-in prefab meshes (cube, wedge) are protected from editing

### File Management
- Create new models (empty or from cube template, configurable size)
- Save and load models (.mdl binary format)
- Duplicate and delete model files from the menu
- Models store: block types, placements, rotations, paint data, palette
- Backward-compatible file format (v1, v2, v3)
- Auto-save on window close or scene switch

### Export
- Export models to glTF Binary (.glb) via the pause menu
- Per-color primitives using the KHR_materials_unlit extension
- Baked world-space vertices with recomputed normals
- Ready to load into any glTF-compatible engine for your own lighting

### General
- Pause menu with save & exit
- Camera locked during pause
- Ghost block at origin shows where first block goes
- Rotating 3D previews of each mesh in the selector grid
- All rendering, collision, and UI built from scratch (no game engine)

## File Formats

| Format | Description |
|--------|-------------|
| `.mdl` | Model file (block types + placements + rotations + paint + palette) |
| `.mesh` | Renderable mesh (vertices + indices + optional normals + optional texture) |
| `.vmesh` | Vector mesh editor data (dots + lines + triangles, editable) |
| `.glb` | Exported glTF Binary for use in other engines |

## Tech Stack

- C++17
- OpenGL 3.3 Core Profile
- GLFW (windowing + input)
- GLM (math)
- FreeType (font rendering)
- stb_image (texture loading)
- nlohmann/json (single-header, bundled in `beta-2.0/thirdparty/`)
- CMake build system
- MSVC compiler (Windows)
