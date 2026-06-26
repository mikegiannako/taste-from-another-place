#pragma once
#include "raylib.h"

enum class HeldItem { None, RawTentacle, RawPotato };


struct Player {
    Vector2  pos;
    float    radius;
    float    speed;
    HeldItem held_item;

    // Animation Data
    Texture2D walk_front[4];
    Texture2D walk_back[4];
    Texture2D walk_side[4];
    Texture2D idle[2];
    
    int currentFrame;
    float frameTimer;
    float frameDuration; // e.g., 0.1f for 10 FPS
    int direction;       // 0: Front, 1: Back, 2: Side
    bool isMoving;
    bool flipX;          // To reuse "side" frames for left AND right
};

void player_update(Player& p, float dt, Rectangle bounds);
void player_draw(const Player& p);
