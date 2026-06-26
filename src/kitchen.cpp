#include "kitchen.h"
#include "inventory.h"
#include "orders.h"
#include "customer.h"
#include "player.h"
#include "station.h"
#include "fly_anim.h"
#include "audio.h"
#include "raylib.h"

namespace {

constexpr int   MAX_STATIONS   = 16;
constexpr float INTERACT_RANGE = 60.0f;
constexpr float GRILL_DURATION = 15.0f;
constexpr float FRYER_DURATION = 12.0f;
constexpr int   BOARD_MAX      = 3;

// ── cooking slots ─────────────────────────────────────────────────────────────

struct CookingSlot {
    bool  active   = false;
    float timer    = 0.0f;
    float duration = 0.0f;
};

constexpr int MAX_COOK_SLOTS = 2;
CookingSlot grill_slots[MAX_COOK_SLOTS];
CookingSlot fryer_slots[MAX_COOK_SLOTS];
int grill_count_added = 0;
int fryer_count_added = 0;

// ── cutting board ─────────────────────────────────────────────────────────────

int board_tentacles       = 0;
int board_potatoes        = 0;
int pending_cut_tentacles = 0;
int pending_cut_potatoes  = 0;

// ── food textures (full-res, scaled at draw-time) ────────────────────────────
// Raw ingredients – used for held-item sprite and board popup.
Texture2D tex_raw_tent  = {};   // plokami.png
Texture2D tex_raw_potato = {};  // potato.png
// Processed – used for inventory icons.
Texture2D tex_cut_tent      = {};   // psito_plokami.png  – cooked tentacle icon
Texture2D tex_raw_cut_tent  = {};   // plokami_slice.png  – raw cut tentacle icon
Texture2D tex_cut_fries     = {};   // potato_slice.png
Texture2D tex_boba      = {};   // boba_tea.png
Texture2D tex_milkshake = {};   // milkshake.png
Texture2D tex_bun       = {};   // psomaki.png
Texture2D tex_fries          = {};   // fries.png – cooked fries product
Texture2D tex_psita_plokamia = {};   // psita_plokamia.png – tentacles on grill
Texture2D tex_fries_baked    = {};   // fries_baked.png – fries in fryer

// ── pending fly-anim queue ────────────────────────────────────────────────────

struct PendingFlyer {
    bool      active = false;
    float     delay  = 0.0f;   // seconds until spawn
    Vector2   from;
    Vector2   to;
    Texture2D tex;
};

constexpr int MAX_PENDING_FLYERS = 32;
PendingFlyer  pending_flyers[MAX_PENDING_FLYERS];

void queue_fly(float delay, Vector2 from, Vector2 to, Texture2D tex) {
    for (int i = 0; i < MAX_PENDING_FLYERS; i++) {
        if (!pending_flyers[i].active) {
            pending_flyers[i] = { true, delay, from, to, tex };
            return;
        }
    }
}

void update_pending_flyers(float dt) {
    for (int i = 0; i < MAX_PENDING_FLYERS; i++) {
        if (!pending_flyers[i].active) continue;
        pending_flyers[i].delay -= dt;
        if (pending_flyers[i].delay <= 0.0f) {
            fly_anim_spawn(pending_flyers[i].from, pending_flyers[i].to, pending_flyers[i].tex);
            pending_flyers[i].active = false;
        }
    }
}

// ── misc kitchen state ────────────────────────────────────────────────────────

Player         player;
Station        stations[MAX_STATIONS];
int            station_count   = 0;
int            focused_station = -1;
Rectangle      play_area;
float          last_minigame_score = -1.0f;
KitchenRequest pending_request     = KitchenRequest::None;
Texture2D      truck_tex           = {};
Texture2D      sky_tex             = {};
Rectangle      cutter_station_rect = {};
Rectangle      drink_station_rect  = {};

// ── helpers ───────────────────────────────────────────────────────────────────

void load_tex(Texture2D& tex, const char* path) {
    if (tex.id == 0 && FileExists(path))
        tex = LoadTexture(path);
}

void unload_tex(Texture2D& tex) {
    if (tex.id != 0) { UnloadTexture(tex); tex = {}; }
}

void add_station(StationKind kind, float x, float y, float w, float h) {
    int slot = -1;
    if (kind == StationKind::Grill) slot = grill_count_added++;
    if (kind == StationKind::Fryer) slot = fryer_count_added++;
    Rectangle r = { x, y, w, h };
    stations[station_count++] = { kind, r, slot };
    if (kind == StationKind::Cutter) cutter_station_rect = r;
    if (kind == StationKind::Drink)  drink_station_rect  = r;
}

int nearest_station_in_range(Vector2 pos, float range) {
    int   best         = -1;
    float best_dist_sq = range * range;
    for (int i = 0; i < station_count; i++) {
        Rectangle r = stations[i].rect;
        float cx = pos.x, cy = pos.y;
        if (cx < r.x)            cx = r.x;
        if (cx > r.x + r.width)  cx = r.x + r.width;
        if (cy < r.y)            cy = r.y;
        if (cy > r.y + r.height) cy = r.y + r.height;
        float dx = pos.x - cx, dy = pos.y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_dist_sq) { best_dist_sq = d2; best = i; }
    }
    return best;
}

// Fit a texture into a destination rect preserving aspect ratio.
void draw_tex_fit(Texture2D tex, float dx, float dy, float dw, float dh, Color tint = WHITE) {
    if (tex.id == 0) return;
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    float scale = (tex.width * dh > tex.height * dw)
                  ? dw / (float)tex.width
                  : dh / (float)tex.height;
    float fw = tex.width  * scale;
    float fh = tex.height * scale;
    // Centre within the destination box
    Rectangle dst = { dx + (dw - fw) * 0.5f, dy + (dh - fh) * 0.5f, fw, fh };
    DrawTexturePro(tex, src, dst, { 0, 0 }, 0.0f, tint);
}

// ── board popup ───────────────────────────────────────────────────────────────

void draw_board_popup(Rectangle r) {
    if (board_tentacles == 0 && board_potatoes == 0) return;

    constexpr float SW  = 28.0f;   // slot width
    constexpr float SH  = 38.0f;   // slot height
    constexpr float GAP =  6.0f;

    float bw = BOARD_MAX * SW + (BOARD_MAX + 1) * GAP;
    float bh = 2.0f * SH + 3.0f * GAP + 14.0f;   // 2 rows + small labels
    float bx = r.x + r.width * 0.5f - bw * 0.5f;
    float by = r.y - bh - 8.0f;

    DrawRectangleRounded({ bx, by, bw, bh }, 0.3f, 8, (Color){ 245, 245, 245, 215 });
    DrawRectangleRoundedLines({ bx, by, bw, bh }, 0.3f, 8, (Color){ 80, 80, 80, 200 });

    // Row 0 – tentacles
    for (int i = 0; i < BOARD_MAX; i++) {
        float sx = bx + GAP + i * (SW + GAP);
        float sy = by + GAP;
        DrawRectangleLinesEx({ sx, sy, SW, SH }, 1,
                             (Color){ 180, 80, 120, (unsigned char)(i < board_tentacles ? 200u : 70u) });
        if (i < board_tentacles)
            draw_tex_fit(tex_raw_tent, sx, sy, SW, SH);
    }

    // Row 1 – potatoes
    for (int i = 0; i < BOARD_MAX; i++) {
        float sx = bx + GAP + i * (SW + GAP);
        float sy = by + GAP + SH + GAP;
        DrawRectangleLinesEx({ sx, sy, SW, SH }, 1,
                             (Color){ 220, 160, 60, (unsigned char)(i < board_potatoes ? 200u : 70u) });
        if (i < board_potatoes)
            draw_tex_fit(tex_raw_potato, sx, sy, SW, SH);
    }

    // Small label row at the bottom of the bubble
    int n_total = board_tentacles + board_potatoes;
    const char* lbl = (n_total == 0) ? "" :
        TextFormat("[E] to cut  (%dT %dP)", board_tentacles, board_potatoes);
    DrawText(lbl, (int)(bx + 4), (int)(by + bh - 14), 11, DARKGRAY);
}

// ── cooking timer overlay ─────────────────────────────────────────────────────

void draw_cooking_timer(Rectangle r, const CookingSlot& slot, const char* label) {
    if (!slot.active) return;

    float bx = r.x - 4.0f;
    float bw = r.width + 8.0f;
    float by = r.y - 58.0f;

    if (slot.timer <= 0.0f) {
        const char* msg = TextFormat("[E] Collect %s", label);
        int tw = MeasureText(msg, 18);
        DrawRectangle((int)bx - 2, (int)(by + 10), tw + 10, 26, (Color){ 0, 0, 0, 180 });
        DrawText(msg, (int)bx + 3, (int)(by + 14), 18, (Color){ 80, 230, 80, 255 });
    } else {
        // Timer text with dark background
        const char* time_str = TextFormat("%.0fs", slot.timer);
        int tw = MeasureText(time_str, 20);
        DrawRectangle((int)bx - 2, (int)by - 2, tw + 8, 26, (Color){ 0, 0, 0, 180 });
        DrawText(time_str, (int)bx + 3, (int)by, 20, RAYWHITE);
        // Progress bar
        DrawRectangleRounded({ bx, by + 28.0f, bw, 14.0f }, 0.5f, 4, (Color){ 30, 30, 30, 220 });
        DrawRectangleRounded({ bx, by + 28.0f, bw * (1.0f - slot.timer / slot.duration), 14.0f },
                             0.5f, 4, (Color){ 60, 180, 255, 255 });
    }
}

// ── hint bar ─────────────────────────────────────────────────────────────────

void draw_hint_text() {
    const char* msg;
    if (player.held_item == HeldItem::RawTentacle)
        msg = "Holding: Tentacle  |  [E] at Cutter to deposit  (max 3 per type)";
    else if (player.held_item == HeldItem::RawPotato)
        msg = "Holding: Potato    |  [E] at Cutter to deposit  (max 3 per type)";
    else
        msg = "[E] Bins to pick up  |  Carry to Cutter & deposit  |  [E] Cutter (empty) to start cutting";

    int tw = MeasureText(msg, 16);
    DrawRectangle(960 - tw / 2 - 12, 4, tw + 24, 26, (Color){ 0, 0, 0, 160 });
    DrawText(msg, 960 - tw / 2, 8, 16, RAYWHITE);
}

bool any_fryer_cooking() {
    for (int i = 0; i < MAX_COOK_SLOTS; i++)
        if (fryer_slots[i].active && fryer_slots[i].timer > 0.0f) return true;
    return false;
}

bool any_grill_cooking() {
    for (int i = 0; i < MAX_COOK_SLOTS; i++)
        if (grill_slots[i].active && grill_slots[i].timer > 0.0f) return true;
    return false;
}

} // namespace

// ── public API ────────────────────────────────────────────────────────────────

void kitchen_init() {
    load_tex(truck_tex, "assets/main_screen/main_field.png");
    load_tex(sky_tex, "assets/main_screen/main_sky.png");

    play_area = { 500.0f, 325.0f, 920.0f, 320.0f };
    player.pos   = { play_area.x + play_area.width  * 0.5f,
                     play_area.y + play_area.height * 0.5f };
    player.speed     = 450.0f;
    player.radius    = 30.0f;
    player.held_item = HeldItem::None;

    const int TW = 155, TH = 155;
    for (int i = 0; i < 4; i++) {
        Image imgF = LoadImage(TextFormat("assets/animations/alien_walk_front%d.png", i + 1));
        Image imgB = LoadImage(TextFormat("assets/animations/alien_walk_back%d.png",  i + 1));
        Image imgS = LoadImage(TextFormat("assets/animations/alien_walk_side%d.png",  i + 1));
        ImageResizeNN(&imgF, TW, TH); ImageResizeNN(&imgB, TW, TH); ImageResizeNN(&imgS, TW, TH);
        player.walk_front[i] = LoadTextureFromImage(imgF);
        player.walk_back[i]  = LoadTextureFromImage(imgB);
        player.walk_side[i]  = LoadTextureFromImage(imgS);
        UnloadImage(imgF); UnloadImage(imgB); UnloadImage(imgS);
    }
    player.frameDuration = 0.12f;
    player.currentFrame  = 0;
    player.frameTimer    = 0.0f;
    player.direction     = 0;
    player.isMoving      = false;
    player.flipX         = true;

    // Food textures – full resolution; scaled at draw-time for quality
    load_tex(tex_raw_tent,   "assets/food/plokami.png");
    load_tex(tex_raw_potato, "assets/food/potato.png");
    load_tex(tex_cut_tent,     "assets/food/psito_plokami.png");
    load_tex(tex_raw_cut_tent, "assets/food/plokami_slice.png");
    load_tex(tex_cut_fries,    "assets/food/potato_slice.png");
    load_tex(tex_boba,       "assets/food/boba_tea.png");
    load_tex(tex_milkshake,  "assets/food/milkshake.png");
    load_tex(tex_bun,        "assets/food/psomaki.png");
    load_tex(tex_fries,           "assets/food/fries.png");
    tex_psita_plokamia = LoadTexture("assets/food/psita_plokamia.png");
    tex_fries_baked    = LoadTexture("assets/food/fries_baked.png");

    station_count = 0; focused_station = -1;
    grill_count_added = fryer_count_added = 0;
    board_tentacles = board_potatoes = 0;
    pending_cut_tentacles = pending_cut_potatoes = 0;
    for (int i = 0; i < MAX_PENDING_FLYERS; i++) pending_flyers[i] = {};
    for (int i = 0; i < MAX_COOK_SLOTS; i++) {
        grill_slots[i] = {};
        fryer_slots[i] = {};
    }

    // Station layout (positions unchanged)
    float bin_w = 90.0f, bin_h = 80.0f;
    float bin_x = play_area.x - bin_w - 8.0f;
    float bin_gap = (play_area.height - 3.0f * bin_h) / 3.5f;
    add_station(StationKind::BinTentacle, bin_x, play_area.y + bin_gap - 10,                             bin_w, bin_h);
    add_station(StationKind::BinPotato,   bin_x, play_area.y + bin_gap * 2.0f + bin_h - 10,             bin_w, bin_h);
    add_station(StationKind::BinBun,      bin_x, play_area.y + bin_gap * 3.0f + bin_h * 2.0f - 10,      bin_w, bin_h);

    float right_w = 90.0f, right_h = 80.0f;
    float right_x = play_area.x + play_area.width + 8.0f;
    add_station(StationKind::Cutter, right_x, play_area.y + 20,           right_w, right_h);
    add_station(StationKind::Drink,  right_x, play_area.y + 3 * right_h,  right_w, right_h);

    float ox = 15.0f, gw = 150.0f, fw = 200.0f, fh = 90.0f, gap = 60.0f;
    float fy = play_area.y + play_area.height + 12.0f;
    add_station(StationKind::Fryer, play_area.x + ox,                           fy, fw, fh);
    add_station(StationKind::Grill, play_area.x + ox + gap + fw,               fy, gw, fh);
    add_station(StationKind::Grill, play_area.x + ox + 2*gap + fw + gw,        fy, gw, fh);
    add_station(StationKind::Fryer, play_area.x + ox + 3*gap + 2*gw + fw,      fy, fw, fh);

    orders_init();
    customers_init();
}

void kitchen_update(float dt) {
    player_update(player, dt, play_area);
    focused_station = nearest_station_in_range(player.pos, INTERACT_RANGE);

    for (int i = 0; i < MAX_COOK_SLOTS; i++) {
        if (grill_slots[i].active && grill_slots[i].timer > 0.0f) grill_slots[i].timer -= dt;
        if (fryer_slots[i].active && fryer_slots[i].timer > 0.0f) fryer_slots[i].timer -= dt;
    }

    orders_update(dt);
    // Notify customer system when a served order finishes its animation
    int served_ci = orders_pop_served_customer();
    if (served_ci >= 0) customer_on_order_served(served_ci);
    customers_update(dt);
    update_pending_flyers(dt);
    fly_anim_update(dt);
    score_notifs_update(dt);

    if (any_fryer_cooking()) audio_fry_start();   else audio_fry_stop();
    if (any_grill_cooking()) audio_grill_start(); else audio_grill_stop();

    if (focused_station < 0 || !IsKeyPressed(KEY_E)) return;

    StationKind k    = stations[focused_station].kind;
    int         slot = stations[focused_station].slot;

    switch (k) {
        case StationKind::BinTentacle:
            if (player.held_item == HeldItem::None)
                player.held_item = HeldItem::RawTentacle;
            break;

        case StationKind::BinPotato:
            if (player.held_item == HeldItem::None)
                player.held_item = HeldItem::RawPotato;
            break;

        case StationKind::BinBun:
            g_inventory.buns++;
            fly_anim_spawn(
                { stations[focused_station].rect.x + stations[focused_station].rect.width  * 0.5f,
                  stations[focused_station].rect.y + stations[focused_station].rect.height * 0.5f },
                { 1787.0f, 885.0f }, tex_bun);
            break;

        case StationKind::Cutter:
            if (player.held_item == HeldItem::RawTentacle && board_tentacles < BOARD_MAX) {
                board_tentacles++;
                player.held_item = HeldItem::None;
            } else if (player.held_item == HeldItem::RawPotato && board_potatoes < BOARD_MAX) {
                board_potatoes++;
                player.held_item = HeldItem::None;
            } else if (player.held_item == HeldItem::None &&
                       (board_tentacles > 0 || board_potatoes > 0)) {
                pending_cut_tentacles = board_tentacles;
                pending_cut_potatoes  = board_potatoes;
                board_tentacles = board_potatoes = 0;
                pending_request = KitchenRequest::LaunchCutting;
            }
            break;

        case StationKind::Drink:
            if (player.held_item == HeldItem::None)
                pending_request = KitchenRequest::LaunchDrink;
            break;

        case StationKind::Grill:
            if (slot < 0 || slot >= MAX_COOK_SLOTS) break;
            if (grill_slots[slot].active && grill_slots[slot].timer <= 0.0f) {
                g_inventory.cooked_tentacles++;
                grill_slots[slot].active = false;
                fly_anim_spawn(
                    { stations[focused_station].rect.x + stations[focused_station].rect.width  * 0.5f,
                      stations[focused_station].rect.y + stations[focused_station].rect.height * 0.5f },
                    { 1787.0f, 885.0f }, tex_cut_tent);
            } else if (!grill_slots[slot].active && g_inventory.raw_cut_tentacles > 0) {
                // Cook exactly one at a time
                g_inventory.raw_cut_tentacles--;
                grill_slots[slot] = { true, GRILL_DURATION, GRILL_DURATION };
                fly_anim_spawn(
                    { 1787.0f, 885.0f },
                    { stations[focused_station].rect.x + stations[focused_station].rect.width  * 0.5f,
                      stations[focused_station].rect.y + stations[focused_station].rect.height * 0.5f },
                    tex_raw_cut_tent);
            }
            break;

        case StationKind::Fryer:
            if (slot < 0 || slot >= MAX_COOK_SLOTS) break;
            if (fryer_slots[slot].active && fryer_slots[slot].timer <= 0.0f) {
                g_inventory.cooked_fries++;
                fryer_slots[slot].active = false;
                fly_anim_spawn(
                    { stations[focused_station].rect.x + stations[focused_station].rect.width  * 0.5f,
                      stations[focused_station].rect.y + stations[focused_station].rect.height * 0.5f },
                    { 1787.0f, 885.0f }, tex_fries);
            } else if (!fryer_slots[slot].active && g_inventory.raw_cut_fries > 0) {
                g_inventory.raw_cut_fries--;
                fryer_slots[slot] = { true, FRYER_DURATION, FRYER_DURATION };
                fly_anim_spawn(
                    { 1787.0f, 885.0f },
                    { stations[focused_station].rect.x + stations[focused_station].rect.width  * 0.5f,
                      stations[focused_station].rect.y + stations[focused_station].rect.height * 0.5f },
                    tex_cut_fries);
            }
            break;
    }
}

void kitchen_timers_tick(float dt) {
    for (int i = 0; i < MAX_COOK_SLOTS; i++) {
        if (grill_slots[i].active && grill_slots[i].timer > 0.0f) grill_slots[i].timer -= dt;
        if (fryer_slots[i].active && fryer_slots[i].timer > 0.0f) fryer_slots[i].timer -= dt;
    }
    fly_anim_update(dt);

    orders_update(dt);
    int served_ci = orders_pop_served_customer();
    if (served_ci >= 0) customer_on_order_served(served_ci);
    customers_update(dt);
    score_notifs_update(dt);

    if (any_fryer_cooking()) audio_fry_start();   else audio_fry_stop();
    if (any_grill_cooking()) audio_grill_start(); else audio_grill_stop();
}

KitchenRequest kitchen_consume_request() {
    KitchenRequest r = pending_request;
    pending_request  = KitchenRequest::None;
    return r;
}

void kitchen_draw() {
    ClearBackground((Color){ 170, 170, 170, 255 });

    if (sky_tex.id != 0) DrawTexture(sky_tex, 0, 0, WHITE);

    // Customers drawn before the truck so the truck window covers them correctly
    customers_draw();

    if (truck_tex.id != 0) DrawTexture(truck_tex, 0, 0, WHITE);

    for (int i = 0; i < station_count; i++) {
        Rectangle r = stations[i].rect;

        if (stations[i].kind == StationKind::Grill && stations[i].slot >= 0)
            draw_cooking_timer(r, grill_slots[stations[i].slot], "Tentacle");
        if (stations[i].kind == StationKind::Fryer && stations[i].slot >= 0)
            draw_cooking_timer(r, fryer_slots[stations[i].slot], "Fries");
        if (stations[i].kind == StationKind::Cutter)
            draw_board_popup(r);
    }

    player_draw(player);

    // Cooking food images – drawn after player so they're always on top
    for (int i = 0; i < station_count; i++) {
        Rectangle r = stations[i].rect;
        if (stations[i].kind == StationKind::Grill && stations[i].slot >= 0) {
            const CookingSlot& s = grill_slots[stations[i].slot];
            if (s.active && tex_psita_plokamia.id != 0) {
                float bw = r.width * 0.95f, bh = r.height * 1.3f;
                draw_tex_fit(tex_psita_plokamia,
                             r.x + r.width*0.5f - bw*0.5f,
                             r.y + r.height*0.5f - bh*0.5f + 20.0f, bw, bh);
            }
        }
        if (stations[i].kind == StationKind::Fryer && stations[i].slot >= 0) {
            const CookingSlot& s = fryer_slots[stations[i].slot];
            if (s.active && tex_fries_baked.id != 0) {
                float bw = r.width * 1.1f, bh = r.height * 1.8f;
                draw_tex_fit(tex_fries_baked,
                             r.x + r.width*0.5f - bw*0.5f - r.width*0.05f,
                             r.y + r.height*0.5f - bh*0.5f + 20.0f, bw, bh);
            }
        }
    }

    // Held-item sprite: drawn to the right of the player, aspect-ratio preserved
    if (player.held_item != HeldItem::None) {
        Texture2D& tex = (player.held_item == HeldItem::RawTentacle)
                         ? tex_raw_tent : tex_raw_potato;
        bool is_tent = (player.held_item == HeldItem::RawTentacle);
        float H  = is_tent ? 68.0f : 38.0f;
        float ox = is_tent ? 36.0f : 22.0f;
        float oy = is_tent ? 46.0f : 38.0f;
        if (tex.id != 0) {
            float w = H * (float)tex.width / (float)tex.height;
            draw_tex_fit(tex, player.pos.x + ox, player.pos.y - oy, w, H);
        } else {
            Color c = (player.held_item == HeldItem::RawTentacle)
                      ? (Color){ 180, 80, 120, 255 } : (Color){ 220, 160, 60, 255 };
            DrawCircle((int)(player.pos.x + 44), (int)(player.pos.y - 30), 14, c);
        }
    }

    if (focused_station >= 0) {
        const char* hint = TextFormat("[E]  %s", station_name(stations[focused_station].kind));
        constexpr int FONT_SIZE = 26;
        int tw = MeasureText(hint, FONT_SIZE);
        float px = player.pos.x - tw * 0.5f;
        float py = player.pos.y - 125.0f;
        Color sc = station_color(stations[focused_station].kind);
        Rectangle pill = { px - 14.0f, py - 8.0f, (float)(tw + 28), (float)(FONT_SIZE + 16) };
        DrawRectangleRounded(pill, 0.5f, 8, (Color){ 0, 0, 0, 210 });
        DrawRectangleRoundedLines(pill, 0.5f, 8, (Color){ sc.r, sc.g, sc.b, 230 });
        DrawText(hint, (int)px, (int)py, FONT_SIZE, WHITE);
    }

    DrawText("WASD to move, E to interact", 40, 30, 18, DARKGRAY);
    draw_hint_text();

    InventoryIcons icons = { tex_cut_tent, tex_fries, tex_boba, tex_milkshake, tex_bun, tex_cut_fries, tex_raw_cut_tent };
    inventory_draw(icons);
    score_notifs_draw();
    orders_draw();
    fly_anim_draw();
}

void kitchen_set_last_minigame_score(float score) {
    last_minigame_score = score;
    g_inventory.raw_cut_tentacles += pending_cut_tentacles;
    g_inventory.raw_cut_fries     += pending_cut_potatoes;

    // Queue staggered fly animations from cutter → inventory
    constexpr float STAGGER = 0.28f;
    Vector2 cutter_center = {
        cutter_station_rect.x + cutter_station_rect.width  * 0.5f,
        cutter_station_rect.y + cutter_station_rect.height * 0.5f
    };
    constexpr Vector2 INV_POS = { 1787.0f, 885.0f };
    float delay = 0.05f;
    for (int i = 0; i < pending_cut_tentacles; i++) {
        queue_fly(delay, cutter_center, INV_POS, tex_raw_cut_tent);
        delay += STAGGER;
    }
    for (int i = 0; i < pending_cut_potatoes; i++) {
        queue_fly(delay, cutter_center, INV_POS, tex_cut_fries);
        delay += STAGGER;
    }

    pending_cut_tentacles = pending_cut_potatoes = 0;
    g_inventory.total_score += score * 100.0f;
}

void kitchen_fly_drink_result(bool is_boba, int qty) {
    constexpr Vector2 INV_POS = { 1787.0f, 885.0f };
    Vector2 drink_center = {
        drink_station_rect.x + drink_station_rect.width  * 0.5f,
        drink_station_rect.y + drink_station_rect.height * 0.5f
    };
    Texture2D tex = is_boba ? tex_boba : tex_milkshake;
    float delay = 0.05f;
    for (int i = 0; i < qty; i++) {
        queue_fly(delay, drink_center, INV_POS, tex);
        delay += 0.35f;
    }
}

void kitchen_shutdown() {
    for (int i = 0; i < 4; i++) {
        UnloadTexture(player.walk_front[i]);
        UnloadTexture(player.walk_back[i]);
        UnloadTexture(player.walk_side[i]);
    }
    unload_tex(truck_tex);
    unload_tex(sky_tex);
    unload_tex(tex_raw_tent);
    unload_tex(tex_raw_potato);
    unload_tex(tex_cut_tent);
    unload_tex(tex_raw_cut_tent);
    unload_tex(tex_cut_fries);
    unload_tex(tex_boba);
    unload_tex(tex_milkshake);
    unload_tex(tex_bun);
    unload_tex(tex_fries);
    unload_tex(tex_psita_plokamia);
    unload_tex(tex_fries_baked);
    customers_shutdown();
    orders_shutdown();
}
