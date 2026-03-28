#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "engine/gameplay/gravity.h"
#include "engine/gameplay/collision.h"
#include <cmath>

// ── Base entity ─────────────────────────────────────────────────────
// Generic entity with position, size, physics, collision, and a default draw.
// Game-specific entities (Player, mobs) build on top of this.

inline bool checkEntityXZOverlap(float entityX, float entityZ, float halfW, float halfD, const CollisionBox& box) {
    return (entityX + halfW) > box.rect.x && (entityX - halfW) < (box.rect.x + box.rect.width) &&
           (entityZ + halfD) > box.rect.y && (entityZ - halfD) < (box.rect.y + box.rect.height);
}

struct Entity {
    Vector3 position = { 0.0f, 0.0f, 0.0f };  // feet position
    float width  = 0.3f;
    float depth  = 0.3f;
    float height = 1.8f;
    float speed  = 6.0f;
    float maxStepHeight = 0.26f;
    float jumpForce = 4.9f;
    int collisionRadius = 2;

    float halfW() const { return width / 2.0f; }
    float halfD() const { return depth / 2.0f; }

    GravityBody gravity;
    std::vector<CollisionBox> collisionBoxes;
    float facingAngle = 0.0f;  // degrees, Y-axis rotation

    // Model
    Model model = { 0 };
    bool modelLoaded = false;
    float modelScale = 1.0f;   // uniform scale applied to the model

    ~Entity() { unloadModel(); }
    Entity() = default;
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&& o) noexcept
        : position(o.position), width(o.width), depth(o.depth), height(o.height),
          speed(o.speed), maxStepHeight(o.maxStepHeight), jumpForce(o.jumpForce),
          collisionRadius(o.collisionRadius), gravity(o.gravity),
          collisionBoxes(std::move(o.collisionBoxes)), facingAngle(o.facingAngle),
          model(o.model), modelLoaded(o.modelLoaded), modelScale(o.modelScale) {
        o.modelLoaded = false;  // prevent double-unload
    }
    Entity& operator=(Entity&& o) noexcept {
        if (this != &o) {
            unloadModel();
            position = o.position; width = o.width; depth = o.depth; height = o.height;
            speed = o.speed; maxStepHeight = o.maxStepHeight; jumpForce = o.jumpForce;
            collisionRadius = o.collisionRadius; gravity = o.gravity;
            collisionBoxes = std::move(o.collisionBoxes); facingAngle = o.facingAngle;
            model = o.model; modelLoaded = o.modelLoaded; modelScale = o.modelScale;
            o.modelLoaded = false;
        }
        return *this;
    }

    // Load a .glb/.gltf model. modelHeight is the real-world height
    // the model should be scaled to match (e.g. entity height).
    void loadModel(const char* path, float modelHeight = 0.0f) {
        model = LoadModel(path);
        modelLoaded = true;
        if (modelHeight > 0.0f) {
            // Auto-scale: measure the model's bounding box and scale to fit
            BoundingBox bb = GetModelBoundingBox(model);
            float rawHeight = bb.max.y - bb.min.y;
            if (rawHeight > 0.0f) {
                modelScale = modelHeight / rawHeight;
            }
        }
    }

    void unloadModel() {
        if (modelLoaded) {
            UnloadModel(model);
            modelLoaded = false;
        }
    }

    // Move with collision sweeping. inputX/inputZ are raw direction (not normalized).
    void move(float inputX, float inputZ, float dt, float speedScale = 1.0f) {
        float len = sqrtf(inputX * inputX + inputZ * inputZ);
        if (len > 0.0f) {
            inputX /= len;
            inputZ /= len;
            facingAngle = atan2f(inputX, inputZ) * (180.0f / CL_PI) + 180.0f;
        }

        float moveX = inputX * speed * speedScale * dt;
        float moveZ = inputZ * speed * speedScale * dt;

        CLVec2 moverSize = { width, depth };
        float moverMinY = position.y + maxStepHeight;
        float moverMaxY = position.y + height;

        // Sweep X
        if (moveX != 0.0f) {
            CLVec2 from = { position.x, position.z };
            CLVec2 to   = { position.x + moveX, position.z };
            float t = sweepMovement(collisionBoxes, from, to, moverSize, moverMinY, moverMaxY);
            position.x += moveX * t;
        }

        // Sweep Z (after X resolved)
        if (moveZ != 0.0f) {
            CLVec2 from = { position.x, position.z };
            CLVec2 to   = { position.x, position.z + moveZ };
            float t = sweepMovement(collisionBoxes, from, to, moverSize, moverMinY, moverMaxY);
            position.z += moveZ * t;
        }
    }

    // Apply gravity + ground collision. Returns true if entity is on ground.
    bool applyGravity(float dt, float gravityScale = 1.0f) {
        float dy = gravity.update(dt, gravityScale);
        position.y += dy;

        float groundHeight = GROUND_NONE;

        for (auto& box : collisionBoxes) {
            if (checkEntityXZOverlap(position.x, position.z, halfW(), halfD(), box) && box.solid && box.maxY > groundHeight && box.maxY <= position.y + maxStepHeight) {
                groundHeight = box.maxY;
            }
        }

        // Land when falling
        if (groundHeight > GROUND_NONE && position.y <= groundHeight && gravity.velocityY <= 0.0f) {
            position.y = groundHeight;
            gravity.land();
        } else if (groundHeight < GROUND_NONE || position.y > groundHeight + GROUNDED_EPSILON) {
            gravity.grounded = false;
        }

        // Safety: unstick from solid blocks
        for (auto& box : collisionBoxes) {
            if (!box.solid) continue;
            if (checkEntityXZOverlap(position.x, position.z, halfW(), halfD(), box) && position.y > box.minY && position.y < box.maxY) {
                position.y = box.maxY;
                gravity.land();
            }
        }

        return gravity.grounded;
    }

    // Check if entity's feet overlap a collision box of the given type
    bool isBaseInCollisionType(CLTileType type) const {
        for (auto& box : collisionBoxes) {
            if (box.type != type) continue;
            if (checkEntityXZOverlap(position.x, position.z, halfW(), halfD(), box) && position.y >= box.minY && position.y < box.maxY) {
                return true;
            }
        }
        return false;
    }

    // Check if any part of the entity's body overlaps a collision box of the given type
    bool isInCollisionType(CLTileType type) const {
        float entityMinY = position.y;
        float entityMaxY = position.y + height;

        for (auto& box : collisionBoxes) {
            if (box.type != type) continue;
            if (checkEntityXZOverlap(position.x, position.z, halfW(), halfD(), box) && entityMaxY > box.minY && entityMinY < box.maxY) {
                return true;
            }
        }
        return false;
    }

    // Register nearby tiles for collision
    void registerTiles(TileInfoFn getTileInfo) {
        registerNearbyTiles(collisionBoxes, position.x, position.z, collisionRadius, getTileInfo);
    }

    // Jump if grounded. Height is in blocks (1 = one block, 2 = two blocks, etc.)
    // Uses jumpForce as default if no height given.
    void jump(float blocks = 0.0f) {
        if (gravity.grounded) {
            if (blocks > 0.0f) {
                // v = sqrt(2 * |gravity| * height)
                gravity.launch(sqrtf(2.0f * (-GRAVITY) * blocks));
            } else {
                gravity.launch(jumpForce);
            }
        }
    }

    // Draw the entity. Uses model if loaded, otherwise a colored cube.
    void draw(Color fallbackColor = RED) const {
        if (modelLoaded) {
            rlDisableBackfaceCulling();
            DrawModelEx(model, position, { 0, 1, 0 }, facingAngle,
                        { modelScale, modelScale, modelScale }, WHITE);
            rlEnableBackfaceCulling();
        } else {
            Vector3 center = { position.x, position.y + height / 2.0f, position.z };
            DrawCube(center, width, height, depth, fallbackColor);
            DrawCubeWires(center, width, height, depth, BLACK);
        }
    }

    // Debug: draw collision boxes
    void drawCollisionDebug() const {
        Vector3 debugCenter = { position.x, position.y + height / 2.0f, position.z };
        DrawCubeWires(debugCenter, width + 0.02f, height + 0.02f, depth + 0.02f, YELLOW);

        for (auto& box : collisionBoxes) {
            float bx = box.rect.x + box.rect.width / 2.0f;
            float bz = box.rect.y + box.rect.height / 2.0f;
            float by = (box.minY + box.maxY) / 2.0f;
            float bh = box.maxY - box.minY;
            DrawCubeWires({ bx, by, bz }, box.rect.width + 0.02f, bh + 0.02f, box.rect.height + 0.02f, { 255, 0, 0, 200 });
        }
    }
};
