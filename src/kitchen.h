#pragma once

enum class KitchenRequest {
    None,
    LaunchCutting,
    LaunchDrink,
};

void           kitchen_init();
void           kitchen_update(float dt);
void           kitchen_timers_tick(float dt);   // tick cooking timers only (call during mini-games)
void           kitchen_draw();
void           kitchen_shutdown();
KitchenRequest kitchen_consume_request();
void           kitchen_set_last_minigame_score(float score);
void           kitchen_fly_drink_result(bool is_boba, int qty);
