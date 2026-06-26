#pragma once
#include "raylib.h"

void fly_anim_spawn(Vector2 from, Vector2 to, Texture2D tex, float duration = 0.8f);
void fly_anim_update(float dt);
void fly_anim_draw();
void fly_anim_clear();
