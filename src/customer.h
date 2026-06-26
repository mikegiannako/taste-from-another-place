#pragma once
#include "raylib.h"

constexpr int MAX_CUSTOMERS = 4;

enum class CustomerState { Walking, Waiting, Leaving };

struct Customer {
    bool          active         = false;
    CustomerState state          = CustomerState::Walking;
    float         x              = 0.0f;
    float         target_x       = 0.0f;
    int           queue_slot     = -1;
    int           order_index    = -1;    // absolute index into orders[]
    float         anim_timer     = 0.0f;
    int           anim_frame     = 0;
    bool          facing_right   = true;
    float         highlight      = 0.0f;  // 0..1, set from hovered order card
    int           customer_type  = 0;     // 0 = grandpa (5 frames), 1 = male (4 frames)
    float         patience_timer = 0.0f;  // counts down while Waiting; 0 → leave
};

void    customers_init();
void    customers_shutdown();
void    customers_update(float dt);
void    customers_draw();
void    customer_on_order_served(int order_index);
Vector2 customers_get_pos(int customer_idx); // world position (feet) of a customer, or {-1,-1}
