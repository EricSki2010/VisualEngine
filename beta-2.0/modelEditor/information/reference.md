# modelEditor Reference

## Scenes

### 3dModeler (`src/scenes/3dModeler.cpp`)
Main editor scene with two modes: Build and Paint.

**Entry:** Receives model name via `void* data` (std::string*). Loads `.mdl` file, restores palette and placements.

#### Sidebar UI
- Right panel: x=0.6, width=0.4, full height
- **Block selector grid** (5×3, 15 slots): Bottom of panel, build mode only. Slots 0-1 are cube/wedge. Slots 2-14 load from `assets/saves/vectorMeshes/slot_N.mesh`.
- **Edit Object / Edit Color button**: Switches label based on editor mode.
- **Editor Mode dropdown**: Build / Paint toggle.
- **Color Mode dropdown** (paint mode, inside color edit panel): RGB / Hex.

#### Build Mode (sEditorMode = 0)
- Place blocks with current mesh selection
- Right-drag: Rectangle face selection
- Tab: Toggle block-select mode (purple)
- Ctrl+D: Delete selected blocks
- Ctrl+A (block mode): Replace with current mesh
- Ctrl+A (face mode): Extrude from selected faces
- R + 1/2/3: Rotate selected blocks by 90° on X/Y/Z
- Ctrl+Z: Undo (50-step history)
- Ctrl+Tab: Edit current slot in vectorMesh editor (skips prefabs)
- Scroll: Cycle through mesh slots

#### Paint Mode (sEditorMode = 1)
- Left-drag: Brush select triangles/faces (immediate)
- Right-click: Select single triangle/face
- Right-drag: Plane select (coplanar triangles, commit on release)
- Ctrl+A: Paint selection with current palette color
- Ctrl+Z: Undo (50-step history)
- Scroll: Cycle through 16 color slots
- Shift: Additive selection. Ctrl: Subtractive selection.

#### Color Wheel
- 16 triangular slices in bottom-right corner (quarter circle, center at screen corner)
- Radius: 0.6 NDC
- Selected slice: Yellow outline, drawn on top
- Only visible in paint mode

#### Color Editing
- Click "Edit Color" → hides editor mode dropdown, shows color edit panel
- Color Mode dropdown: RGB or Hex
- RGB: 4 inputs (R, G, B, Bri) 0-255. Color = (R/255, G/255, B/255) × (Bri/255)
- Hex: 2 inputs (hex code, Bri). Hex is 6 chars (RRGGBB).
- "Done" button: Saves color to palette, updates palette texture, triggers rebuild, closes panel
- Pre-fills inputs with current color (empty if default grey)

#### Pause Menu
- ESC toggles. Blocks all input except ESC and menu clicks.
- Camera forced out of looking mode while paused.
- Buttons: Continue / Export As GLB / Save & Exit.

#### Export Dialog
- Opened from pause menu "Export As GLB" button.
- **Name** input (pre-filled with current model name).
- **Path** input (optional, defaults to `assets/exports`).
- **Done**: writes `{path}/{name}.glb` via `exportModelToGlb`.
- **Cancel**: closes dialog without exporting.

#### Auto-Save
- Current model is saved on scene exit / window close via the `onExit` handler.

#### Model Save/Load
- **Save:** Palette → `sCurrentModel.palette`. Colliders → placements (position, rotation, meshName, triColors). Writes `.mdl` v3.
- **Load:** Reads `.mdl`, restores palette to color wheel, registers block types (VN meshes loaded from `.mesh` files), places blocks with rotation, restores triColors onto colliders.

#### VectorMesh Editor Return
- `sPendingSlotUpdate` / `sPendingSlotMesh` set before switching to vectorMesh scene.
- On return: checks for `.mesh` file, re-registers mesh, creates preview, rebuilds selector icons.

---

### vectorMesh (`src/scenes/vectorMesh.cpp`)
Editor for creating custom meshes from dots, lines, and triangles.

**Entry:** Receives `VectorMeshEditData` with meshName, slotIndex, modelName.

#### Modes
- **Vector (dot) mode**: Place 3D points with snap-to-edge. Snaps to cube corners, existing dots, and interpolated points along edges/lines. Snap count configurable.
- **Line mode**: Select 2 dots → Ctrl+A to create edge. Ctrl+D to delete hovered line.
- **Plane mode**: Select 3 dots → Ctrl+A to create triangle. Click triangle to select. "Flip Normal" button on selected triangle. Ctrl+D to delete.

#### Triangle Preview
- Triangles render with the winding order as placed (A→B→C). No auto
  normal-detection heuristic. Use "Flip Normal" to override manually.
- Front and back both drawn so triangles are visible from any angle.

#### File Formats
- **.vmesh**: Binary. Dots (vec3 array), lines (index pairs), triangles (index triples + flipped flag).
- **.mesh export**: Converts triangles to pos3+uv2+normal3 vertices. "VN" magic header. Normals flipped for correct lighting. Each triangle gets 3 unique vertices.

#### Pause Menu
- "Continue", "Save Model" (saves .vmesh + exports .mesh), "Exit" (returns to 3dModeler or menu).

#### Shortcuts
- Ctrl+Z: Undo (50-step history across dots, lines, triangles, flips)
- Ctrl+Tab: Save + export .mesh + return to 3dModeler

---

### menu (`src/scenes/menu.cpp`)
Main menu with Create New, Load (file browser with delete/duplicate), and Exit.

### createScene (`src/scenes/createScene.cpp`)
New model wizard. Type dropdown (3D Model), name input, template selection (None / Starting Cube with size 1-64).

---

## Mechanics

### Highlight (`src/mechanics/Highlight.cpp`)
Handles all selection rendering and input for both Build and Paint modes.

#### Selection Rendering
- **Block select (purple)**: Full block outline, visible faces only
- **Paint select (green)**: Individual triangles or AABB faces
- **Face select (blue)**: AABB face quads
- **Hover (yellow)**: Single face/triangle under cursor

#### Face Index Mapping
Face state indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z.
Raycast returns face indices in the same order.
Each triangle is tagged with a `faceDir` (0-5 or -1=none) at mesh registration.
Rotation is handled by `rotMap`: each local face direction is rotated to world space during the face-pair collection phase.

#### Paint Plane Selection
Stores starting triangle's normal and plane distance (`dot(normal, center)`). Only selects triangles with matching normal (dot product > 0.95) and plane distance (within 0.1f).

### Selection (`src/mechanics/Selection.cpp`)
Simple vector storage of `SelectedFace { blockPos (ivec3), faceIndex (int) }`. Deduplicates on add.

### GltfExporter (`src/mechanics/GltfExporter.cpp`)
`exportModelToGlb(outPath)` — exports the current scene as a glTF Binary file.

- Iterates all colliders, transforms vertices into world space (rotation + position baked).
- Groups triangles into 17 buckets: 16 palette colors + 1 unpainted (uses neutral grey 0.8).
- Each bucket becomes a separate glTF primitive with its own material.
- Materials use `KHR_materials_unlit` so your engine applies its own lighting.
- Writes a single `.glb` file (12-byte header + JSON chunk + BIN chunk).
- Uses nlohmann/json (bundled in `beta-2.0/thirdparty/json.hpp`).

---

## Prefabs

### prefab_cube.h
24 vertices (4 per face), 5 floats each (pos3+uv2). Centered at origin, 1×1×1.
Index format: `v0, v1, v2, faceDir` per triangle.
All faces assigned to their direction (0=+X through 5=-Z).
Face states: all 6 faces = state 2 (solid).
Plain indices version for ghost block rendering.

### prefab_wedge.h
18 vertices, 8 triangles. Wedge/ramp shape.
Face states: bottom(-Y)=2, back(-Z)=2, left(-X)=1, right(+X)=1, slope/top=0.
Slope and triangular side faces have faceDir=none (0xFFFFFFFF) — invisible to culling.
Left/right partial faces (state 1) are culled when adjacent to a state 2 face.

### EmbeddedSelectors.h
Embedded 32×32 PNG data for "+" (empty slot) and "-" (selected slot) icons.

### templates.cpp
`createCubeTemplate(size)`: Fills size³ volume with cube blocks.

---

## Shared Data

### SceneData.h
```
VectorMeshEditData { meshName, slotIndex, modelName }
```
Passed from 3dModeler to vectorMesh scene on "Edit Object".

---

## Controls Summary

| Key/Mouse | Build Mode | Paint Mode |
|-----------|-----------|------------|
| Right-drag | Rectangle face select | Plane select (coplanar) |
| Left-drag | — | Brush select triangles |
| Right-click | Select single face | Select single triangle |
| Tab | Toggle block select (purple) | — |
| Ctrl+A | Replace (block) / Extrude (face) | Paint with current color |
| Ctrl+D | Delete selected blocks | — |
| Ctrl+Z | Undo (50-step history) | Undo (50-step history) |
| Ctrl+Tab | Edit current slot in vectorMesh | — |
| R + 1/2/3 | Rotate 90° on X/Y/Z | — |
| Scroll | Cycle mesh slots | Cycle color slots |
| ESC | Pause menu | Pause menu |
| Q | Toggle FPS camera | Toggle FPS camera |
| WASD | Move (when Q active) | Move (when Q active) |
| Space/Shift | Up/Down (when Q active) | Up/Down (when Q active) |
