# VisualEngine API Reference

## PUBLIC API
**Header:** `VisualEngine/VisualEngine.h`
**Implementation:** `VisualEngine/VisualEngine.cpp`

`VE::initWindow(width, height, title, maximized = false) -> bool`
  Creates a window with OpenGL 3.3 context. Initializes shader, scene, and input. Pass `true` for maximized to fill the screen.

`VE::setCamera(x, y, z, yaw, pitch)`
  Sets camera position and orientation.

`VE::loadMesh(name, MeshDef)`
  Registers a mesh from vertex/index data in memory.

`VE::loadMesh(name, meshFilePath)`
  Registers a mesh from a .mesh binary file on disk.

`VE::loadMeshDir(dirPath)`
  Scans a directory for all `.mesh` files and registers each one. The filename (without extension) becomes the mesh name.

`VE::setMode(mode)`
  Sets mesh build mode. SINGLE = one merged mesh with face culling. CHUNK = reserved for future chunked meshing.

`VE::draw(meshName, x, y, z)`
  Places a named mesh at a world position. Also creates a collider for it.

`VE::undraw(x, y, z)`
  Removes the mesh and collider at a world position.

`VE::clearDraws()`
  Removes all placed meshes and colliders.

`VE::hasBlockAt(x, y, z) -> bool`
  Returns true if a collider exists at the given grid position.

`VE::rebuild()`
  Forces a mesh rebuild. Normally happens automatically when needsRebuild is set.

`VE::setBrightness(brightness)`
  Sets global brightness multiplier for the 3D shader. 1.0 = normal, lower = darker, higher = brighter. Does not affect UI.

`VE::registerScene(name, onEnter, onExit, onInput, onUpdate, onRender)`
  Registers a named scene with lifecycle hooks. onEnter receives `void* data`. See Scene Management section.

`VE::setScene(name, data = nullptr)`
  Switches to a registered scene. Optional `void* data` is passed to the new scene's onEnter.

`VE::run()`
  Starts the main loop: processInput -> update -> render -> swap. Blocks until window closes. Calls active scene's onExit before cleanup.


## ENGINE CONTEXT
**Header:** `VisualEngine/EngineGlobals.h`
**Defined in:** `VisualEngine/VisualEngine.cpp`

`EngineContext ctx` — Single global struct holding all engine state:
- `ctx.window` — GLFWwindow pointer
- `ctx.shader` — default Shader
- `ctx.scene` — Scene (projection, view, lighting)
- `ctx.width`, `ctx.height` — current framebuffer dimensions
- `ctx.needsRebuild` — flag set by draw/undraw/setMode, consumed by update
- `ctx.mode` — SINGLE or CHUNK
- `ctx.mergedMeshes` — built mesh data ready for rendering


## RENDERING
**Header:** `VisualEngine/renderingManagement/render.h`
**Implementations:** `Shader.cpp`, `Scene.cpp`, `Texture.cpp`, `Mesh.cpp`

`Shader(vertexSrc, fragmentSrc)`
  Compiles and links a shader program.
  `shader.loc(name) -> int` — returns cached uniform location.

`Scene(aspectRatio)`
  Holds projection matrix, view matrix, and a PointLight.
  `scene.uploadStaticUniforms(shader)` — uploads light properties and default brightness (once).
  `scene.uploadFrameUniforms(shader, model)` — uploads model/projection/normal matrices (per frame).

`Texture(filepath)`
  Loads an image via stb_image and creates a GL texture.
  `texture.bind(unit)` — binds to a texture unit.

`Mesh(vertices, vertexCount, indices, indexCount)`
  Creates VAO/VBO/EBO from vertex data (position3 + uv2, normals computed automatically). Default color is grey (0.8).
  `mesh.setTexture(tex)` — assigns a texture.
  `mesh.setColor(color)` — sets a solid color.
  `mesh.draw(shader)` — binds texture/color uniforms and draws.


## OVERLAY
**Header:** `VisualEngine/renderingManagement/Overlay.h`
**Implementation:** `VisualEngine/renderingManagement/Overlay.cpp`

`initOverlay()`
  Creates a reusable VAO/VBO for drawing single-triangle overlays.

`cleanupOverlay()`
  Deletes the overlay VAO/VBO.

`drawTriangleOverlay(shader, triangle, color, alpha)`
  Draws a single triangle as a semi-transparent overlay on top of existing geometry. Uses polygon offset to prevent z-fighting.

`aabbFaceTriangle(box, faceIndex, half) -> Triangle`
  Converts one half of an AABB face into a triangle. faceIndex: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z. half: 0 or 1 for the two triangles of the quad.


## MESH BUILDING
**Header:** `VisualEngine/renderingManagement/ChunkMesh.h`
**Implementation:** `VisualEngine/renderingManagement/ChunkMesh.cpp`

`registerMesh(name, MeshDef)`
  Stores mesh vertex/index data in the internal registry.

`registerMeshFromFile(name, filepath)`
  Loads a .mesh binary and registers it.

`getRegisteredMesh(name) -> RegisteredMesh*`
  Looks up a registered mesh by name.

`addDrawInstance(meshName, x, y, z)`
  Adds a draw instance to the draw list.

`removeDrawInstance(x, y, z)`
  Removes the draw instance at a position.

`clearDrawInstances()`
  Clears all draw instances.

`buildSingleMeshes() -> vector<MergedMeshEntry>`
  Merges all instances of each mesh type into one draw call with neighbor-based face culling. Adjacent faces between blocks are removed.

`buildMergedMeshes() -> vector<MergedMeshEntry>`
  Placeholder for future chunk-based meshing. Currently forwards to buildSingleMeshes.

`clearMeshData()`
  Clears both the mesh registry and draw list.


## COLLISION
**Header:** `VisualEngine/inputManagement/Collision.h`
**Implementation:** `VisualEngine/inputManagement/Collision.cpp`

`Triangle { v0, v1, v2 }`
  Three vertices defining a triangle.

`AABB { min, max }`
  Axis-aligned bounding box.

`BlockCollider { position, bounds, triangles, meshName, isRectangular }`
  A collider placed in the world. Rectangular colliders use AABB-only raycasting. Non-rectangular store individual triangles.

`CollisionHit { hit, point, normal, distance, collider, triangleIndex }`
  Result of a raycast. triangleIndex is an index into collider->triangles (mesh) or a face index 0-5 (AABB).

`isMeshRectangular(vertices, vertexCount) -> bool`
  Returns true if all vertices lie on the faces of their bounding box.

`addCollider(meshName, vertices, vertexCount, indices, indexCount, rectangular, x, y, z)`
  Creates a collider at a world position.

`removeCollider(x, y, z)`
  Removes the collider at a position.

`clearColliders()`
  Removes all colliders.

`hasColliderAt(x, y, z) -> bool`
  Returns true if a collider exists at the grid position.

`getColliderAt(x, y, z) -> BlockCollider*`
  Returns the collider at a position, or null.

`getAllColliders() -> vector<BlockCollider>&`
  Returns all active colliders.

`raycast(origin, direction, maxDist=50) -> CollisionHit`
  Casts a ray against all colliders. Returns the closest hit.


## RAYCASTING
**Header:** `VisualEngine/inputManagement/Raycasting.h`
**Implementation:** `VisualEngine/inputManagement/Raycasting.cpp`

`Ray { origin, direction }`
  A world-space ray.

`screenToRay(mouseX, mouseY, screenWidth, screenHeight, view, projection) -> Ray`
  Converts a screen-space mouse position to a world-space ray using unProject.


## CAMERA
**Header:** `VisualEngine/inputManagement/Camera.h`
**Implementation:** `VisualEngine/inputManagement/Camera.cpp`

`CameraMode` enum: `CAMERA_FPS` (perspective + movement), `CAMERA_FLAT` (orthographic, no input).

`Camera { mode, position, yaw, pitch, sensitivity, moveSpeed, looking, ... }`
  FPS-style camera with mouse look and WASD movement. Supports two modes.

`camera.setMode(newMode)`
  Switches between FPS and FLAT mode. FLAT resets position to origin.

`camera.updateDir()`
  Recalculates the look direction from yaw/pitch.

`camera.processKeyboard(window, dt)`
  Handles WASD movement and Q to toggle mouse look. Disabled in FLAT mode. Movement only when Q is active.

`camera.getViewMatrix() -> mat4`
  Returns the current view matrix. Identity in FLAT mode.

`camera.getProjectionMatrix(aspectRatio) -> mat4`
  Returns perspective (FPS) or orthographic (FLAT) projection.

`Camera::mouseCallback(window, xpos, ypos)`
  GLFW mouse callback for look rotation. Disabled in FLAT mode.

`getGlobalCamera() -> Camera*`
  Returns the single global camera instance.


## RENDER LOOP
**Header:** `VisualEngine/renderingManagement/RenderLoop.h`
**Implementation:** `VisualEngine/renderingManagement/RenderLoop.cpp`

`processInput(dt)`
  Handles camera keyboard input and calls active scene's onInput.

`update()`
  Triggers rebuild if needed. Updates view matrix from camera. Calls active scene's onUpdate.

`render()`
  Re-binds 3D shader, clears screen, uploads uniforms, draws all meshes, calls active scene's onRender.


## SCENE MANAGEMENT
**Header:** `VisualEngine/sceneManagement/SceneManager.h`
**Implementation:** `VisualEngine/sceneManagement/SceneManager.cpp`

`SceneDef { onEnter(void*), onExit, onInput(float dt), onUpdate, onRender }`
  Defines a scene's lifecycle hooks. All are optional (nullable). onEnter receives data from setScene.

`VE::registerScene(name, onEnter, onExit, onInput, onUpdate, onRender)`
  Registers a named scene with its hooks. Nothing runs until setScene is called.

`VE::setScene(name, data = nullptr)`
  Switches to a scene. Calls onExit on the current scene, then onEnter on the new one with the provided data pointer.

`getActiveScene() -> SceneDef*`
  Returns the currently active scene definition, or nullptr.

`getActiveSceneName() -> string&`
  Returns the name of the currently active scene.


## MEMORY MANAGEMENT
**Header:** `VisualEngine/memoryManagement/memory.h`
**Implementation:** `VisualEngine/memoryManagement/memory.cpp`

`setMemoryPath(dir)`
  Sets the base directory for saving/loading binary files.

`saveToMemory(name, formatPath, data) -> bool`
  Packs a vector of comma-separated strings into a compact binary file using a format definition. Auto-detects 32 or 64 bit mode based on total bit width. RLE compressed. Saves to `{memoryPath}/{name}.bin`.

`loadFromMemory(name, formatPath) -> vector<string>`
  Reads a `.bin` file, RLE decompresses, unpacks each entry, and returns comma-separated strings matching the original data.

Format files define the bit layout per entry, one field per line:
```
x -> 4
y -> 4
z -> 4
id -> 10
rx -> 3
ry -> 3
rz -> 3
```


## MODEL FILES
**Header:** `VisualEngine/memoryManagement/ModelData.h`

`FaceColor { color }`
  RGB color for a triangle face. Default grey (0.8).

`BlockTypeDef { name, vertices, vertexCount, indices, indexCount, faceColors }`
  Defines a custom block shape with per-triangle colors.

`BlockPlacement { x, y, z, typeId, rx, ry, rz }`
  A placed block: position, which block type, rotation.

`ModelFile { blockTypes, placements }`
  Complete model file containing block type definitions and all placements.

`saveModel(name, model) -> bool`
  Saves a ModelFile to `{memoryPath}/{name}.mdl`. Binary format with magic header, block types section (vertices, indices, face colors), and placements section.

`loadModel(name, model) -> bool`
  Loads a `.mdl` file back into a ModelFile struct.


## UI SYSTEM
**Header:** `VisualEngine/uiManagement/UIElement.h`

`UIElement { id, position, size, color, textureId, label, labelScale, labelColor, onClick, visible, isTextInput, focused, inputText, placeholder, maxLength, onUnfocus, requireConfirm, confirmId }`
  UI element struct. Supports buttons, panels, images, and text inputs. Optional confirmation system via requireConfirm/confirmId.

**Header:** `VisualEngine/uiManagement/UIRenderer.h`
**Implementation:** `VisualEngine/uiManagement/UIRenderer.cpp`

`initUIRenderer()`
  Creates quad VAO/VBO and compiles the UI shader.

`cleanupUIRenderer()`
  Deletes UI GL resources and shader.

`drawUIElement(element)`
  Draws a single UI element as a colored/textured quad. Disables depth test, enables blending.

**Header:** `VisualEngine/uiManagement/UIManager.h`
**Implementation:** `VisualEngine/uiManagement/UIManager.cpp`

`UIGroup { id, visible, elements }`
  Named container of UI elements with shared visibility.

`addUIGroup(groupId, visible = true)`
  Creates a new UI group.

`removeUIGroup(groupId)`
  Removes a group and all its elements.

`addToGroup(groupId, element)`
  Adds a UI element to an existing group.

`removeFromGroup(groupId, elementId)`
  Removes a specific element from a group.

`setGroupVisible(groupId, visible)`
  Shows or hides all elements in a group.

`getUIElement(groupId, elementId) -> UIElement*`
  Returns a pointer to a specific element, or nullptr.

`clearUI()`
  Removes all groups and elements.

`renderUI()`
  Draws all visible groups/elements. Renders labels centered on buttons. Renders text input content with blinking cursor. Highlights focused inputs and pending confirm buttons.

`processUIInput()`
  Handles left-click detection and keyboard input for focused text fields. Supports letters, numbers, space, dash, underscore, backspace.

`handleUIClick(mouseX, mouseY, screenWidth, screenHeight) -> bool`
  Hit tests all visible elements back-to-front. Handles focus, confirmation system, and onClick. Returns true if something was hit.

`getInputText(groupId, elementId) -> string`
  Returns the current text in a text input element.

`hasPendingConfirm() -> bool`
  Returns true if a button is waiting for confirmation.

`getPendingConfirmId() -> string`
  Returns the confirmId of the pending action.

`cancelPendingConfirm()`
  Cancels any pending confirmation.

**Header:** `VisualEngine/uiManagement/UIPrefabs.h`
**Implementation:** `VisualEngine/uiManagement/UIPrefabs.cpp`

`createButton(id, x, y, w, h, color, label, onClick) -> UIElement`
  Creates a button with centered text label.

`createPanel(id, x, y, w, h, color) -> UIElement`
  Creates a solid colored rectangle.

`createImage(id, x, y, w, h, textureId) -> UIElement`
  Creates a textured rectangle.

`createTextInput(id, x, y, w, h, color, placeholder, maxLength) -> UIElement`
  Creates a clickable text input field with placeholder text.


## TEXT RENDERING
**Header:** `VisualEngine/uiManagement/TextRenderer.h`
**Implementation:** `VisualEngine/uiManagement/TextRenderer.cpp`

`initTextRenderer(fontPath, fontSize) -> bool`
  Loads a .ttf font via FreeType, rasterizes ASCII glyphs into textures.

`cleanupTextRenderer()`
  Deletes all glyph textures and the text shader.

`drawText(text, x, y, scale, color)`
  Draws a string at pixel coordinates (top-left origin). Each character is a textured quad.

`measureText(text, scale) -> float`
  Returns the width in pixels of a rendered string.

`measureTextHeight(scale) -> float`
  Returns the height in pixels of capital letters at the given scale.
