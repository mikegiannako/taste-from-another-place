#include "customer.h"
#include "orders.h"
#include "inventory.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

namespace {

// ── layout ────────────────────────────────────────────────────────────────────
constexpr float CUSTOMER_Y          = 320.0f;  // feet Y (tweak to match pavement)
constexpr float WINDOW_X            = 700.0f;  // slot 0 – left-side serving window
constexpr float SLOT_SPACING        = 155.0f;  // queue extends RIGHT from window
constexpr float WALK_SPEED          = 110.0f;  // px/sec
constexpr float ARRIVE_THRESH       =   3.0f;
constexpr float LEAVE_TARGET_X      = -220.0f; // walk off the left edge
constexpr float SPRITE_H            = 195.0f;  // rendered height in pixels

// ── timing ────────────────────────────────────────────────────────────────────
constexpr float FIRST_SPAWN_DELAY   =  5.0f;
constexpr float SPAWN_INTERVAL      = 22.0f;
constexpr float WALK_FRAME_DUR      =  0.18f;
constexpr float IDLE_FRAME_DUR      =  0.50f;
constexpr float PATIENCE_SECS       = 66.0f;  // seconds before giving up

// ── animation frames ──────────────────────────────────────────────────────────
constexpr int GRANDPA_FRAMES = 5;
constexpr int MALE_FRAMES    = 4;

Texture2D grandpa_frames[GRANDPA_FRAMES] = {};
Texture2D male_frames[MALE_FRAMES]       = {};

// ── state ─────────────────────────────────────────────────────────────────────
Customer customers[MAX_CUSTOMERS];
float    spawn_timer = FIRST_SPAWN_DELAY;

// ── helpers ───────────────────────────────────────────────────────────────────

float slot_x(int s) { return WINDOW_X + s * SLOT_SPACING; }

int frame_count(int type)              { return type == 1 ? MALE_FRAMES : GRANDPA_FRAMES; }
Texture2D get_frame(int type, int fr)  {
    if (type == 1) return male_frames[fr % MALE_FRAMES];
    return grandpa_frames[fr % GRANDPA_FRAMES];
}

int active_count() {
    int n = 0;
    for (int i = 0; i < MAX_CUSTOMERS; i++)
        if (customers[i].active) n++;
    return n;
}

int next_free_slot() {
    bool used[MAX_CUSTOMERS] = {};
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (customers[i].active &&
            customers[i].state != CustomerState::Leaving &&
            customers[i].queue_slot >= 0 &&
            customers[i].queue_slot < MAX_CUSTOMERS)
            used[customers[i].queue_slot] = true;
    }
    for (int i = 0; i < MAX_CUSTOMERS; i++)
        if (!used[i]) return i;
    return -1;
}

void try_spawn() {
    int slot = next_free_slot();
    if (slot < 0) return;
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].active) {
            customers[i]               = {};
            customers[i].active        = true;
            customers[i].state         = CustomerState::Walking;
            customers[i].queue_slot    = slot;
            customers[i].target_x      = slot_x(slot);
            customers[i].x             = -220.0f;   // spawn off left edge
            customers[i].facing_right  = true;      // walk right toward window
            customers[i].customer_type = rand() % 2;
            return;
        }
    }
}

void compact_queue() {
    int sorted[MAX_CUSTOMERS];
    int n = 0;
    for (int slot = 0; slot < MAX_CUSTOMERS; slot++) {
        for (int i = 0; i < MAX_CUSTOMERS; i++) {
            if (customers[i].active &&
                customers[i].state != CustomerState::Leaving &&
                customers[i].queue_slot == slot) {
                sorted[n++] = i;
                break;
            }
        }
    }
    for (int j = 0; j < n; j++) {
        Customer& c      = customers[sorted[j]];
        float     new_tx = slot_x(j);
        c.queue_slot     = j;
        c.target_x       = new_tx;
        if (fabsf(c.x - new_tx) > ARRIVE_THRESH) {
            c.state        = CustomerState::Walking;
            c.facing_right = (new_tx > c.x);
        }
    }
}

void advance_walk_frame(Customer& c, float dt) {
    c.anim_timer += dt;
    if (c.anim_timer >= WALK_FRAME_DUR) {
        c.anim_timer = 0.0f;
        c.anim_frame = (c.anim_frame + 1) % frame_count(c.customer_type);
    }
}

} // namespace

// ── public API ────────────────────────────────────────────────────────────────

void customers_init() {
    for (int i = 0; i < MAX_CUSTOMERS; i++) customers[i] = {};
    spawn_timer = FIRST_SPAWN_DELAY;

    for (int i = 0; i < GRANDPA_FRAMES; i++) {
        const char* p = TextFormat("assets/animations/grandpa_walk%d.png", i + 1);
        if (FileExists(p)) grandpa_frames[i] = LoadTexture(p);
    }
    for (int i = 0; i < MALE_FRAMES; i++) {
        const char* p = TextFormat("assets/animations/male_walk%d.png", i + 1);
        if (FileExists(p)) male_frames[i] = LoadTexture(p);
    }
}

void customers_shutdown() {
    for (int i = 0; i < MAX_CUSTOMERS; i++) customers[i] = {};
    for (int i = 0; i < GRANDPA_FRAMES; i++) {
        if (grandpa_frames[i].id) { UnloadTexture(grandpa_frames[i]); grandpa_frames[i] = {}; }
    }
    for (int i = 0; i < MALE_FRAMES; i++) {
        if (male_frames[i].id) { UnloadTexture(male_frames[i]); male_frames[i] = {}; }
    }
    spawn_timer = FIRST_SPAWN_DELAY;
}

void customer_on_order_served(int customer_index) {
    if (customer_index < 0 || customer_index >= MAX_CUSTOMERS) return;
    Customer& c = customers[customer_index];
    if (!c.active) return;
    c.state        = CustomerState::Leaving;
    c.target_x     = LEAVE_TARGET_X;
    c.facing_right = false;
    c.order_index  = -1;
    c.queue_slot   = -1;
    compact_queue();
}

void customers_update(float dt) {
    if (active_count() < MAX_CUSTOMERS) {
        spawn_timer -= dt;
        if (spawn_timer <= 0.0f) {
            try_spawn();
            spawn_timer = SPAWN_INTERVAL;
        }
    }

    // Highlight sync from hovered order card
    int hovered_ci = orders_get_hovered_customer_index();
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].active) continue;
        if (i == hovered_ci)
            customers[i].highlight = 1.0f;
        else {
            customers[i].highlight -= dt * 4.0f;
            if (customers[i].highlight < 0.0f) customers[i].highlight = 0.0f;
        }
    }

    bool any_deactivated = false;
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].active) continue;
        Customer& c = customers[i];

        switch (c.state) {

            case CustomerState::Walking: {
                float dx   = c.target_x - c.x;
                float step = WALK_SPEED * dt;
                if (fabsf(dx) <= step + ARRIVE_THRESH) {
                    // Arrived – enter idle facing the window (left)
                    c.x            = c.target_x;
                    c.state        = CustomerState::Waiting;
                    c.anim_frame   = 0;
                    c.anim_timer   = 0.0f;
                    c.facing_right = false;  // always face left toward window
                    if (c.order_index < 0) {
                        c.order_index    = orders_spawn_for_customer(i);
                        c.patience_timer = PATIENCE_SECS;
                    }
                } else {
                    c.x           += (dx > 0.0f ? 1.0f : -1.0f) * step;
                    c.facing_right = (dx > 0.0f);
                    advance_walk_frame(c, dt);
                }
                break;
            }

            case CustomerState::Waiting: {
                // Patience countdown
                c.patience_timer -= dt;
                if (c.patience_timer <= 0.0f) {
                    orders_cancel_order(c.order_index);
                    c.order_index  = -1;
                    c.state        = CustomerState::Leaving;
                    c.target_x     = LEAVE_TARGET_X;
                    c.facing_right = false;
                    c.queue_slot   = -1;
                    constexpr float PENALTY = 100.0f;
                    g_inventory.total_score -= PENALTY;
                    if (g_inventory.total_score < 0.0f) g_inventory.total_score = 0.0f;
                    score_deduct_notify(PENALTY);
                    compact_queue();
                    break;
                }

                // Slow idle: alternate frames 0 and 2
                c.anim_timer += dt;
                if (c.anim_timer >= IDLE_FRAME_DUR) {
                    c.anim_timer = 0.0f;
                    c.anim_frame = (c.anim_frame == 0) ? 2 : 0;
                }

                // Re-walk if compact changed target
                if (fabsf(c.x - c.target_x) > ARRIVE_THRESH) {
                    c.state        = CustomerState::Walking;
                    c.facing_right = (c.target_x > c.x);
                }
                break;
            }

            case CustomerState::Leaving: {
                float dx   = c.target_x - c.x;
                float step = WALK_SPEED * dt;
                if (fabsf(dx) <= step + ARRIVE_THRESH) {
                    c.active        = false;
                    any_deactivated = true;
                } else {
                    c.x           += (dx > 0.0f ? 1.0f : -1.0f) * step;
                    c.facing_right = (dx > 0.0f);
                    advance_walk_frame(c, dt);
                }
                break;
            }
        }
    }

    if (any_deactivated) compact_queue();
}

Vector2 customers_get_pos(int customer_idx) {
    if (customer_idx < 0 || customer_idx >= MAX_CUSTOMERS || !customers[customer_idx].active)
        return { -1.0f, -1.0f };
    return { customers[customer_idx].x, CUSTOMER_Y - SPRITE_H * 0.5f };
}

void customers_draw() {
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].active) continue;
        const Customer& c = customers[i];

        Texture2D tex = get_frame(c.customer_type, c.anim_frame);
        if (tex.id == 0) continue;

        float scale  = SPRITE_H / (float)tex.height;
        float dw     = tex.width  * scale;
        float dh     = tex.height * scale;
        float head_y = CUSTOMER_Y - dh;

        // Sprite (flip when facing left)
        Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
        if (!c.facing_right) src.width = -src.width;
        Rectangle dst = { c.x - dw * 0.5f, head_y, dw, dh };
        DrawTexturePro(tex, src, dst, { 0, 0 }, 0.0f, WHITE);

        // Speech bubble (highlight + patience passed in)
        if (c.state == CustomerState::Waiting && c.order_index >= 0) {
            float ratio = c.patience_timer / PATIENCE_SECS;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            orders_draw_customer_bubble(c.order_index, c.x, head_y - 6.0f,
                                        c.highlight, ratio);
        }
    }
}
