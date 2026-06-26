#pragma once
#include "raylib.h"

enum class OrderItem { HotDog, Fries, EyeballBoba, MysteryDrink };

constexpr int MAX_ORDERS = 8;

struct Order {
    bool      active;
    OrderItem items[3];
    int       count;
    float     anim_scale       = 1.0f;
    float     anim_y_offset    = 0.0f;
    // serving animation state
    bool      completing       = false;
    float     completing_timer = 0.0f;
    int       items_spawned    = 0;
    Vector2   completing_pos   = {};   // world position of card when serve was clicked
    // customer tie-in
    int       customer_index   = -1;   // index into customers[] array, -1 if none
};

void orders_init();
void orders_update(float dt);
void orders_draw();
void orders_shutdown();

// Customer integration
int  orders_spawn_for_customer(int customer_idx);
int  orders_get_hovered_customer_index();
int  orders_pop_served_customer();
void orders_cancel_order(int order_idx);
void orders_draw_customer_bubble(int order_idx, float cx, float head_y,
                                 float highlight, float patience_ratio);
