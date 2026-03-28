#pragma once

// ── Gravity system (shared by any entity that needs it) ──────────────

const float GRAVITY = -20.0f;        // units per second squared
const float TERMINAL_VELOCITY = -40.0f;

struct GravityBody {
    float velocityY = 0.0f;
    bool grounded = false;

    // Apply gravity to velocity, returns Y displacement for this frame
    // gravityScale: 0.5 for water, 1.0 for normal
    float update(float dt, float gravityScale = 1.0f) {
        if (grounded) {
            velocityY = 0.0f;
            return 0.0f;
        }

        velocityY += GRAVITY * gravityScale * dt;
        if (velocityY < TERMINAL_VELOCITY * gravityScale) velocityY = TERMINAL_VELOCITY * gravityScale;

        return velocityY * dt;
    }

    // Call when the entity lands on something
    void land() {
        grounded = true;
        velocityY = 0.0f;
    }

    // Call to jump or get launched upward
    void launch(float force) {
        grounded = false;
        velocityY = force;
    }
};
