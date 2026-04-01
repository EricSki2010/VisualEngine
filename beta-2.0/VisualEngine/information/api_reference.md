# VisualEngine API Reference

## PUBLIC API
**Header:** `VisualEngine/VisualEngine.h`
**Implementation:** `VisualEngine/VisualEngine.cpp`

`VE::initWindow(width, height, title) -> bool`
  Creates a window with OpenGL 3.3 context. Initializes shader, scene, and input.

`VE::setCamera(x, y, z, yaw, pitch)`
  Sets camera position and orientation.

`VE::loadMesh(name, MeshDef)`
  Registers a mesh from vertex/index data in memory.

`VE::loadMesh(name, meshFilePath)`
  Registers a mesh from a .mesh binary file on disk.

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

`VE::setPostRenderCallback(std::function<void()>)`
  Registers a function called every frame after scene rendering. Used by apps to inject custom rendering (e.g. highlights, overlays, debug drawing).

`VE::run()`
  Starts the main loop: processInput -> update -> render -> swap. Blocks until window closes. Cleans up on exit.


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
- `ctx.postRenderCallback` — optional per-frame callback


## RENDERING
**Header:** `VisualEngine/renderingManagement/render.h`
**Implementations:** `VisualEngine/renderingManagement/Shader.cpp`, `VisualEngine/renderingManagement/Scene.cpp`, `VisualEngine/renderingManagement/Texture.cpp`, `VisualEngine/renderingManagement/Mesh.cpp`

`Shader(vertexSrc, fragmentSrc)`
  Compiles and links a shader program.
  `shader.loc(name) -> int` — returns cached uniform location.

`Scene(aspectRatio)`
  Holds projection matrix, view matrix, and a PointLight.
  `scene.uploadStaticUniforms(shader)` — uploads light properties (once).
  `scene.uploadFrameUniforms(shader, model)` — uploads model/projection/normal matrices (per frame).

`Texture(filepath)`
  Loads an image via stb_image and creates a GL texture.
  `texture.bind(unit)` — binds to a texture unit.

`Mesh(vertices, vertexCount, indices, indexCount)`
  Creates VAO/VBO/EBO from vertex data (position3 + uv2, normals computed automatically).
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
  Draws a single triangle as a semi-transparent overlay on top of existing geometry. Handles blend enable/disable and depth function.

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

`Camera { position, yaw, pitch, sensitivity, moveSpeed, ... }`
  FPS-style camera with mouse look and WASD movement.

`camera.updateDir()`
  Recalculates the look direction from yaw/pitch.

`camera.processKeyboard(window, dt)`
  Handles WASD movement and Q to toggle mouse look.

`camera.getViewMatrix() -> mat4`
  Returns the current view matrix.

`Camera::mouseCallback(window, xpos, ypos)`
  GLFW mouse callback for look rotation.

`getGlobalCamera() -> Camera*`
  Returns the single global camera instance.


## RENDER LOOP
**Header:** `VisualEngine/renderingManagement/RenderLoop.h`
**Implementation:** `VisualEngine/renderingManagement/RenderLoop.cpp`

`processInput(dt)`
  Handles escape key and camera keyboard input.

`update()`
  Triggers rebuild if needed. Updates view matrix from camera.

`render()`
  Clears screen, uploads uniforms, draws all meshes, calls postRenderCallback if set.
