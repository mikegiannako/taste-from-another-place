#include "player.h"
#include "audio.h"
#include <math.h>

void player_update(Player& p, float dt, Rectangle bounds) {
    Vector2 dir = {0.0f, 0.0f};
    p.isMoving = false;

    if (IsKeyDown(KEY_W)) { dir.y -= 1.0f; p.direction = 1; p.isMoving = true; }
    if (IsKeyDown(KEY_S)) { dir.y += 1.0f; p.direction = 0; p.isMoving = true; }
    if (IsKeyDown(KEY_A)) { dir.x -= 1.0f; p.direction = 2; p.isMoving = true; p.flipX = false; }
    if (IsKeyDown(KEY_D)) { dir.x += 1.0f; p.direction = 2; p.isMoving = true; p.flipX = true; }

    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) {
        // --- MOVING ---
        p.pos.x += (dir.x / len) * p.speed * dt;
        p.pos.y += (dir.y / len) * p.speed * dt;

        p.frameTimer += dt;
        if (p.frameTimer >= p.frameDuration) {
            p.frameTimer = 0.0f;
            p.currentFrame = (p.currentFrame + 1) % 4;
            audio_play(p.currentFrame % 2 == 0 ? Sfx::StepLeft : Sfx::StepRight);
        }
    } else {
        // --- IDLE ---
        if (p.direction == 0) {
            // Front-facing idle: Alternate between frame 1 and 3 (2nd and 4th images)
            
            // If we just stopped moving, snap immediately to an idle frame
            if (p.currentFrame != 1 && p.currentFrame != 3) {
                p.currentFrame = 1;
                p.frameTimer = 0.0f;
            }

            p.frameTimer += dt;
            // Multiplying p.frameDuration by 3.0f makes the idle animation 
            // slower and more relaxed than the walking speed.
            if (p.frameTimer >= p.frameDuration * 3.0f) {
                p.frameTimer = 0.0f;
                // Toggle between index 1 and 3
                p.currentFrame = (p.currentFrame == 1) ? 3 : 1;
            }
        } else {
            // Back/Side idle: Just snap to the first frame
            p.currentFrame = 0;
            p.frameTimer = 0.0f;
        }
    }

    // Boundary collisions
    float min_x = bounds.x + p.radius;
    float min_y = bounds.y + p.radius;
    float max_x = bounds.x + bounds.width  - p.radius;
    float max_y = bounds.y + bounds.height - p.radius;
    if (p.pos.x < min_x) p.pos.x = min_x;
    if (p.pos.y < min_y) p.pos.y = min_y;
    if (p.pos.x > max_x) p.pos.x = max_x;
    if (p.pos.y > max_y) p.pos.y = max_y;
}

void player_draw(const Player& p) {
    Texture2D currentTex;
    
    // Select the correct animation set
    if (p.direction == 0)      currentTex = p.walk_front[p.currentFrame];
    else if (p.direction == 1) currentTex = p.walk_back[p.currentFrame];
    else                       currentTex = p.walk_side[p.currentFrame];

    // Define source rectangle
    Rectangle source = { 0.0f, 0.0f, (float)currentTex.width, (float)currentTex.height };
    if (p.flipX) source.width *= -1; 

    Vector2 origin = { (float)currentTex.width / 2.0f, (float)currentTex.height / 2.0f };

    // --- Visual Offset ---
    // A positive value moves the image DOWN. 
    // A negative value moves the image UP.
    float yOffset = -25.0f; 

    Rectangle dest = { 
        p.pos.x, 
        p.pos.y + yOffset, // Apply the offset here
        (float)currentTex.width, 
        (float)currentTex.height 
    };

    DrawTexturePro(currentTex, source, dest, origin, 0.0f, WHITE);

    // DrawCircleLines((int)p.pos.x, (int)p.pos.y, p.radius, RED);
}