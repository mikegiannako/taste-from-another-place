#include "orders.h"
#include "customer.h"
#include "inventory.h"
#include "fly_anim.h"
#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>

namespace {

constexpr float SERVE_STAGGER   = 0.45f;   // seconds between each item flying out
constexpr float SERVE_FLY_DUR   = 0.8f;    // must match fly_anim default
constexpr float CARD_W         = 180.0f;
constexpr float CARD_H         = 330.0f;
constexpr float CARD_SPACING   = 210.0f;
constexpr float BASE_Y         = 1010.0f;
constexpr float EDGE_DROP      =   30.0f;
constexpr float MAX_ROT_DEG    =    6.0f;
constexpr float HOVER_RISE     =  120.0f;
constexpr float HOVER_SCALE    =    1.25f;

Order orders[MAX_ORDERS];
int   active_indices[MAX_ORDERS];
int   active_count             = 0;
int   hovered_ai               = -1;
int   served_customer_index    = -1;  // set when a completing order fully deactivates

Texture2D tex_order_hotdog    = {};
Texture2D tex_order_fries     = {};
Texture2D tex_order_boba      = {};
Texture2D tex_order_milkshake = {};
Texture2D tex_order_card      = {};

// ── helpers ──────────────────────────────────────────────────────────────────

void rebuild_active() {
    active_count = 0;
    for (int i = 0; i < MAX_ORDERS; i++)
        if (orders[i].active) active_indices[active_count++] = i;
}

void card_geometry(int ai, int total, float* cx, float* cy, float* rot_deg) {
    float t  = (total > 1) ? ((float)ai / (float)(total - 1)) * 2.0f - 1.0f : 0.0f;
    *cx      = 960.0f + t * (CARD_SPACING * (float)(total - 1) * 0.5f);
    *cy      = BASE_Y + t * t * EDGE_DROP;
    *rot_deg = t * MAX_ROT_DEG;
}

bool point_in_rotated_rect(Vector2 p, float cx, float cy, float w, float h, float rot_deg) {
    float rad   = rot_deg * DEG2RAD;
    float cos_r = cosf(-rad);
    float sin_r = sinf(-rad);
    float dx    = p.x - cx;
    float dy    = p.y - cy;
    float lx    = dx * cos_r - dy * sin_r;
    float ly    = dx * sin_r + dy * cos_r;
    return lx >= -w * 0.5f && lx <= w * 0.5f
        && ly >= -h * 0.5f && ly <= h * 0.5f;
}

const char* item_name(OrderItem it) {
    switch (it) {
        case OrderItem::HotDog:       return "Hot Dog";
        case OrderItem::Fries:        return "Fries";
        case OrderItem::EyeballBoba:  return "E. Boba";
        case OrderItem::MysteryDrink: return "M. Drink";
    }
    return "?";
}

Color item_color(OrderItem it) {
    switch (it) {
        case OrderItem::HotDog:       return (Color){220, 120,  60, 255};
        case OrderItem::Fries:        return (Color){220, 180,  60, 255};
        case OrderItem::EyeballBoba:  return (Color){ 80, 200,  80, 255};
        case OrderItem::MysteryDrink: return (Color){160,  80, 220, 255};
    }
    return GRAY;
}

Texture2D item_tex(OrderItem it) {
    switch (it) {
        case OrderItem::HotDog:       return tex_order_hotdog;
        case OrderItem::Fries:        return tex_order_fries;
        case OrderItem::EyeballBoba:  return tex_order_boba;
        case OrderItem::MysteryDrink: return tex_order_milkshake;
    }
    return {};
}

// Draw a texture aspect-fitted into size×size, centered at world pos, with card rotation.
void draw_item_image(Texture2D tex, Vector2 pos, float size, float rot_deg, Color tint) {
    if (tex.id == 0) return;
    float sx = size / (float)tex.width;
    float sy = size / (float)tex.height;
    float s  = sx < sy ? sx : sy;
    float dw = tex.width  * s;
    float dh = tex.height * s;
    Rectangle src    = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst    = { pos.x, pos.y, dw, dh };
    Vector2   origin = { dw * 0.5f, dh * 0.5f };
    DrawTexturePro(tex, src, dst, origin, rot_deg, tint);
}

bool order_fulfillable(const Order& o) {
    int tent = g_inventory.cooked_tentacles;
    int frie = g_inventory.cooked_fries;
    int bun  = g_inventory.buns;
    int boba = g_inventory.eyeball_boba;
    int myst = g_inventory.mystery_drink;
    for (int i = 0; i < o.count; i++) {
        switch (o.items[i]) {
            case OrderItem::HotDog:
                if (tent < 1 || bun < 1) return false;
                tent--; bun--; break;
            case OrderItem::Fries:
                if (frie < 1) return false;
                frie--; break;
            case OrderItem::EyeballBoba:
                if (boba < 1) return false;
                boba--; break;
            case OrderItem::MysteryDrink:
                if (myst < 1) return false;
                myst--; break;
        }
    }
    return true;
}

// Fills out_ok[3] with which items in the order are currently satisfiable,
// simulating sequential consumption so duplicates are handled correctly.
void item_check_status(const Order& o, bool out_ok[3]) {
    int tent = g_inventory.cooked_tentacles;
    int frie = g_inventory.cooked_fries;
    int bun  = g_inventory.buns;
    int boba = g_inventory.eyeball_boba;
    int myst = g_inventory.mystery_drink;
    for (int i = 0; i < o.count; i++) {
        out_ok[i] = false;
        switch (o.items[i]) {
            case OrderItem::HotDog:
                if (tent >= 1 && bun >= 1) { out_ok[i] = true; tent--; bun--; } break;
            case OrderItem::Fries:
                if (frie >= 1)             { out_ok[i] = true; frie--; }         break;
            case OrderItem::EyeballBoba:
                if (boba >= 1)             { out_ok[i] = true; boba--; }         break;
            case OrderItem::MysteryDrink:
                if (myst >= 1)             { out_ok[i] = true; myst--; }         break;
        }
    }
}

void deduct_inventory(Order& o) {
    for (int i = 0; i < o.count; i++) {
        switch (o.items[i]) {
            case OrderItem::HotDog:
                g_inventory.cooked_tentacles--;
                g_inventory.buns--;
                break;
            case OrderItem::Fries:        g_inventory.cooked_fries--;    break;
            case OrderItem::EyeballBoba:  g_inventory.eyeball_boba--;    break;
            case OrderItem::MysteryDrink: g_inventory.mystery_drink--;   break;
        }
    }
    float gained = 50.0f * (float)o.count;
    g_inventory.total_score += gained;
    score_add_notify(gained);
}


// ── card drawing ─────────────────────────────────────────────────────────────

void draw_card(int ai, float cx, float cy, float rot_deg, float scale, bool hovered) {
    int          oi      = active_indices[ai];
    const Order& o       = orders[oi];
    bool         fulfill = !o.completing && order_fulfillable(o);

    float w = CARD_W * scale;
    float h = CARD_H * scale;

    // Parchment card background
    if (tex_order_card.id != 0) {
        Rectangle src = { 0, 0, (float)tex_order_card.width, (float)tex_order_card.height };
        Rectangle dst = { cx, cy, w, h };
        DrawTexturePro(tex_order_card, src, dst, { w * 0.5f, h * 0.5f }, rot_deg, WHITE);
    } else {
        Color bg = fulfill ? (Color){40, 75, 40, 245} : (Color){28, 28, 48, 245};
        DrawRectanglePro({cx, cy, w, h}, {w * 0.5f, h * 0.5f}, rot_deg, bg);
    }

    // Build card-local → world transform
    float rad   = rot_deg * DEG2RAD;
    float cos_r = cosf(rad);
    float sin_r = sinf(rad);
    auto wp = [&](float lx, float ly) -> Vector2 {
        return { cx + lx * cos_r - ly * sin_r,
                 cy + lx * sin_r + ly * cos_r };
    };

    // Title
    {
        const char* title = "ORDER";
        float       fs    = 20.0f * scale;
        Vector2     sz    = MeasureTextEx(GetFontDefault(), title, fs, 1.0f);
        Vector2     tpos  = wp(0.0f, -h * 0.5f + 28.0f * scale);
        Vector2     toff  = { tpos.x + 1.5f*scale, tpos.y + 1.5f*scale };
        DrawTextPro(GetFontDefault(), title, toff,
                    {sz.x * 0.5f, 0.0f}, rot_deg, fs, 1.0f, (Color){60, 30, 0, 200});
        DrawTextPro(GetFontDefault(), title, tpos,
                    {sz.x * 0.5f, 0.0f}, rot_deg, fs, 1.0f, BLACK);
    }

    // Items
    bool ok[3] = {};
    if (o.completing)
        for (int i = 0; i < o.count; i++) ok[i] = true;
    else
        item_check_status(o, ok);

    float usable_h  = h - 70.0f * scale;
    float item_step = usable_h / (float)(o.count + 1);
    float img_size  = 62.0f * scale;

    for (int i = 0; i < o.count; i++) {
        float ly = -h * 0.5f + 40.0f * scale + item_step * (float)(i + 1);

        float item_rot = rot_deg + (o.items[i] == OrderItem::HotDog ? 180.0f : 0.0f);
        draw_item_image(item_tex(o.items[i]), wp(0.0f, ly), img_size, item_rot, WHITE);

        // Fallback colored dot if texture missing
        if (item_tex(o.items[i]).id == 0)
            DrawCircleV(wp(0.0f, ly), 14.0f * scale, item_color(o.items[i]));

        // Checkmark drawn as two lines in card-local space
        if (ok[i]) {
            float bcx = img_size * 0.38f;
            float bcy = ly + img_size * 0.38f;
            float cs  = 11.0f * scale;
            float lw  = 3.0f * scale;
            // Short left leg: top-left → middle-bottom
            // Long right leg: middle-bottom → top-right
            float off = 1.5f * scale;
            // Shadow pass
            DrawLineEx(wp(bcx - cs*0.45f + off, bcy - cs*0.05f + off),
                       wp(bcx - cs*0.05f + off, bcy + cs*0.45f + off), lw,
                       (Color){ 0, 60, 0, 200 });
            DrawLineEx(wp(bcx - cs*0.05f + off, bcy + cs*0.45f + off),
                       wp(bcx + cs*0.55f + off, bcy - cs*0.35f + off), lw,
                       (Color){ 0, 60, 0, 200 });
            // Green pass
            DrawLineEx(wp(bcx - cs*0.45f, bcy - cs*0.05f),
                       wp(bcx - cs*0.05f, bcy + cs*0.45f), lw,
                       (Color){ 50, 230, 50, 255 });
            DrawLineEx(wp(bcx - cs*0.05f, bcy + cs*0.45f),
                       wp(bcx + cs*0.55f, bcy - cs*0.35f), lw,
                       (Color){ 50, 230, 50, 255 });
        }
    }

    // "SERVE!" prompt when fulfillable
    if (fulfill) {
        const char* hint = "SERVE!";
        float       fs   = 20.0f * scale;
        Vector2     sz   = MeasureTextEx(GetFontDefault(), hint, fs, 1.0f);
        DrawTextPro(GetFontDefault(), hint,
                    wp(0.0f, h * 0.5f - 20.0f * scale),
                    {sz.x * 0.5f, 0.0f}, rot_deg, fs, 1.0f,
                    (Color){20, 130, 20, 255});
    }
}

} // namespace

// ── public API ───────────────────────────────────────────────────────────────

void orders_init() {
    srand((unsigned)time(NULL));
    for (int i = 0; i < MAX_ORDERS; i++) orders[i] = {};
    active_count          = 0;
    hovered_ai            = -1;
    served_customer_index = -1;

    if (FileExists("assets/food/hotdog.png"))     tex_order_hotdog    = LoadTexture("assets/food/hotdog.png");
    if (FileExists("assets/food/fries.png"))      tex_order_fries     = LoadTexture("assets/food/fries.png");
    if (FileExists("assets/food/boba_tea.png"))   tex_order_boba      = LoadTexture("assets/food/boba_tea.png");
    if (FileExists("assets/food/milkshake.png"))  tex_order_milkshake = LoadTexture("assets/food/milkshake.png");
    if (FileExists("assets/misc/xartaki.png"))    tex_order_card      = LoadTexture("assets/misc/xartaki.png");
}

int orders_spawn_for_customer(int customer_idx) {
    for (int i = 0; i < MAX_ORDERS; i++) {
        if (!orders[i].active) {
            orders[i]                = {};
            orders[i].active         = true;
            orders[i].count          = 1 + rand() % 3;
            for (int j = 0; j < orders[i].count; j++)
                orders[i].items[j]   = (OrderItem)(rand() % 4);
            orders[i].anim_scale     = 1.0f;
            orders[i].customer_index = customer_idx;
            return i;
        }
    }
    return -1;
}

int orders_get_hovered_customer_index() {
    if (hovered_ai < 0 || hovered_ai >= active_count) return -1;
    return orders[active_indices[hovered_ai]].customer_index;
}

int orders_pop_served_customer() {
    int ci            = served_customer_index;
    served_customer_index = -1;
    return ci;
}

void orders_cancel_order(int order_idx) {
    if (order_idx < 0 || order_idx >= MAX_ORDERS) return;
    orders[order_idx] = {};
}

void orders_draw_customer_bubble(int order_idx, float cx, float head_y,
                                 float highlight, float patience_ratio) {
    if (order_idx < 0 || order_idx >= MAX_ORDERS || !orders[order_idx].active) return;
    const Order& o = orders[order_idx];

    constexpr float ICON    = 46.0f;
    constexpr float PAD     =  7.0f;
    constexpr float TAIL    = 11.0f;
    constexpr float TIMER_R = 11.0f;

    float bw = o.count * ICON + (o.count + 1) * PAD;
    float bh = ICON + 2.0f * PAD;
    float bx = cx - bw * 0.5f;
    float by = head_y - bh - TAIL;

    // Background: warm parchment, shifts to orange-red as patience drains
    float t = 1.0f - patience_ratio;   // 0 = full patience, 1 = no patience
    // Also glow gold when the matching order card is hovered
    float h = highlight;
    Color bg = {
        (unsigned char)(250),
        (unsigned char)(244 - (int)(t * 130) - (int)(h * 44)),
        (unsigned char)(215 - (int)(t * 195) - (int)(h * 165)),
        238
    };
    Color border = {
        (unsigned char)(130 + (int)(t * 80)),
        (unsigned char)(85  - (int)(t * 65)),
        (unsigned char)(20),
        210
    };

    DrawRectangleRounded({ bx, by, bw, bh }, 0.35f, 8, bg);
    DrawRectangleRoundedLines({ bx, by, bw, bh }, 0.35f, 8, border);

    // Tail triangle pointing down toward the customer's head
    Vector2 tl = { cx - 7.0f, by + bh };
    Vector2 tr = { cx + 7.0f, by + bh };
    Vector2 tp = { cx,         by + bh + TAIL };
    DrawTriangle(tr, tl, tp, (Color){ 250, 244, 215, 238 });
    DrawLineEx(tl, tp, 1.5f, (Color){ 130, 85, 20, 210 });
    DrawLineEx(tr, tp, 1.5f, (Color){ 130, 85, 20, 210 });

    // Item icons — all centered in their ICON×ICON cell
    for (int i = 0; i < o.count; i++) {
        float cx2 = bx + PAD + i * (ICON + PAD) + ICON * 0.5f;
        float cy2 = by + PAD + ICON * 0.5f;
        Texture2D tex = item_tex(o.items[i]);
        if (tex.id != 0) {
            float s  = ICON / (float)(tex.width > tex.height ? tex.width : tex.height);
            float fw = tex.width  * s;
            float fh = tex.height * s;
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            Rectangle dst = { cx2, cy2, fw, fh };
            float irot = (o.items[i] == OrderItem::HotDog) ? 180.0f : 0.0f;
            DrawTexturePro(tex, src, dst, { fw * 0.5f, fh * 0.5f }, irot, WHITE);
        } else {
            DrawCircleV({ cx2, cy2 }, ICON * 0.38f, item_color(o.items[i]));
        }
    }

    // Patience timer – small pie clock in the top-right corner of the bubble
    Vector2 tc = { bx + bw + TIMER_R * 0.3f, by - TIMER_R * 0.3f };
    // Dark backing
    DrawCircleV(tc, TIMER_R, (Color){ 30, 30, 30, 220 });
    // Colored arc: green → yellow → red as patience drains
    Color timer_col;
    if (patience_ratio > 0.5f)       timer_col = (Color){  60, 210,  60, 255 };
    else if (patience_ratio > 0.25f) timer_col = (Color){ 220, 185,  30, 255 };
    else                             timer_col = (Color){ 220,  55,  40, 255 };

    if (patience_ratio > 0.005f) {
        float end_a = -90.0f + patience_ratio * 360.0f;
        DrawCircleSector(tc, TIMER_R - 2.0f, -90.0f, end_a, 24, timer_col);
    }
    DrawCircleLinesV(tc, TIMER_R, (Color){ 200, 200, 200, 180 });
}

void orders_shutdown() {
    if (tex_order_hotdog.id)    UnloadTexture(tex_order_hotdog);
    if (tex_order_fries.id)     UnloadTexture(tex_order_fries);
    if (tex_order_boba.id)      UnloadTexture(tex_order_boba);
    if (tex_order_milkshake.id) UnloadTexture(tex_order_milkshake);
    if (tex_order_card.id)      UnloadTexture(tex_order_card);
    tex_order_hotdog = tex_order_fries = tex_order_boba = tex_order_milkshake = tex_order_card = {};
}

void orders_update(float dt) {
    rebuild_active();

    Vector2 mouse   = GetMousePosition();
    bool    clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    hovered_ai      = -1;

    for (int ai = 0; ai < active_count; ai++) {
        float cx, cy, rot;
        card_geometry(ai, active_count, &cx, &cy, &rot);

        Order& o  = orders[active_indices[ai]];

        // Tick completing orders: stagger-spawn items then deactivate.
        if (o.completing) {
            o.completing_timer += dt;
            while (o.items_spawned < o.count &&
                   o.completing_timer >= o.items_spawned * SERVE_STAGGER) {
                fly_anim_spawn({ 1787.0f, 885.0f }, o.completing_pos,
                               item_tex(o.items[o.items_spawned]), SERVE_FLY_DUR);
                o.items_spawned++;
            }
            float done_at = (o.count - 1) * SERVE_STAGGER + SERVE_FLY_DUR + 0.1f;
            if (o.completing_timer >= done_at) {
                // shrink card to zero over 0.3 s then deactivate
                o.anim_scale -= dt / 0.3f;
                if (o.anim_scale <= 0.01f) {
                    served_customer_index = o.customer_index;
                    o.active              = false;
                    o.completing          = false;
                    rebuild_active();
                    hovered_ai = -1;
                    break;
                }
            }
            continue;  // skip hover/click while serving
        }

        bool hit = point_in_rotated_rect(mouse, cx, cy,
                                         CARD_W, CARD_H, rot)
                || point_in_rotated_rect(mouse, cx, cy - HOVER_RISE,
                                         CARD_W * HOVER_SCALE,
                                         CARD_H * HOVER_SCALE, 0.0f);
        if (hit) {
            hovered_ai = ai;
            if (clicked) {
                int oi = active_indices[ai];
                if (order_fulfillable(orders[oi])) {
                    deduct_inventory(orders[oi]);
                    orders[oi].completing       = true;
                    orders[oi].completing_timer = 0.0f;
                    orders[oi].items_spawned    = 0;
                    // Fly to the customer if available, otherwise fall back to the card
                    Vector2 cpos = customers_get_pos(orders[oi].customer_index);
                    orders[oi].completing_pos = (cpos.x >= 0.0f)
                        ? cpos
                        : Vector2{ cx, cy + orders[oi].anim_y_offset };
                    hovered_ai = -1;
                }
            }
            break;
        }
    }

    // Lerp per-order animation values toward their targets
    constexpr float ANIM_SPEED = 10.0f;
    for (int ai = 0; ai < active_count; ai++) {
        Order& o = orders[active_indices[ai]];
        if (o.completing) continue;  // don't animate completing cards
        float target_scale  = (ai == hovered_ai) ? HOVER_SCALE  : 1.0f;
        float target_offset = (ai == hovered_ai) ? -HOVER_RISE  : 0.0f;
        o.anim_scale    += (target_scale  - o.anim_scale)    * ANIM_SPEED * dt;
        o.anim_y_offset += (target_offset - o.anim_y_offset) * ANIM_SPEED * dt;
    }
}

void orders_draw() {
    for (int ai = 0; ai < active_count; ai++) {
        float cx, cy, rot;
        card_geometry(ai, active_count, &cx, &cy, &rot);

        bool         hovered    = (ai == hovered_ai);
        const Order& o          = orders[active_indices[ai]];
        float        draw_cy    = cy + o.anim_y_offset;
        float        draw_rot   = rot * (1.0f - (o.anim_scale - 1.0f) / (HOVER_SCALE - 1.0f));
        float        draw_scale = o.anim_scale;

        draw_card(ai, cx, draw_cy, draw_rot, draw_scale, hovered);
    }
}
