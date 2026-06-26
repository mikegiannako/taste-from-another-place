#include "cutting_minigame.h"
#include "audio.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

namespace {

constexpr int   MAX_ITEMS  = 96;
constexpr int   TRAIL_LEN  = 10;
constexpr float ROUND_TIME = 10.0f;
constexpr float FOOD_SIZE  = 70.0f;
constexpr float GRAVITY    = 1200.0f;
constexpr float SCREEN_W   = 1920.0f;
constexpr float SCREEN_H   = 1080.0f;

enum class ItemKind { Potato, Tentacle, Bomb };

struct Item {
    bool     alive;
    bool     fragment;
    ItemKind kind;
    Vector2  pos;
    Vector2  vel;
    float    rot;
    float    rot_vel;
    float    size;
    Image    icon;
};

Item    items[MAX_ITEMS];
Vector2 trail[TRAIL_LEN];
int     trail_head  = 0;
int     trail_count = 0;
Vector2 mouse_prev;
bool    mouse_initialized = false;

enum class Phase { Playing, Results };

float elapsed             = 0.0f;
float round_time          = ROUND_TIME;  // mutable; increases by 3 on each boot slice
float spawn_timer         = 0.0f;
int   ingredients_spawned = 0;
int   ingredients_sliced  = 0;
int   bombs_sliced        = 0;
bool  finished            = false;
float final_score         = 0.0f;
Phase phase               = Phase::Playing;
float results_elapsed     = 0.0f;
float timer_flash_timer   = 0.0f;   // counts down; timer text draws red while > 0
float timer_popup_timer   = 0.0f;   // counts down; "+3s" floats upward while > 0

Texture tentacle_texture;
Rectangle tentacle_texture_rect;

Texture tentacle_slice_texture;
Rectangle tentacle_slice_texture_rect;

Texture potato_texture;
Rectangle potato_texture_rect;

Texture potato_slice_texture;
Rectangle potato_slice_texture_rect;

Texture boot_texture;
Rectangle boot_texture_rect;

Texture bg_texture;
Rectangle bg_texture_rect;

Texture slice_tex = {};

constexpr int   MAX_FLASHES = 16;
constexpr float FLASH_DUR   = 0.55f;

struct SliceFlash {
    bool    alive;
    Vector2 pos;
    float   rot;
    float   size;
    float   timer;
};
SliceFlash flashes[MAX_FLASHES];

float frand(float a, float b) {
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

int alloc_item() {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].alive) return i;
    }
    return -1;
}

void place_item(ItemKind kind, Vector2 pos, Vector2 vel,
                float rot, float rot_vel, float size, bool fragment) {
    int idx = alloc_item();
    if (idx < 0) return;
    items[idx] = { true, fragment, kind, pos, vel, rot, rot_vel, size };
}

void spawn_random() {
    float roll = frand(0.0f, 1.0f);
    ItemKind kind;
    if      (roll < 0.45f) kind = ItemKind::Potato;
    else if (roll < 0.85f) kind = ItemKind::Tentacle;
    else                   kind = ItemKind::Bomb;

    float x = frand(180.0f, SCREEN_W - 180.0f);
    Vector2 pos = { x, SCREEN_H + 60.0f };
    float vy = frand(-1550.0f, -1370.0f);
    float vx = frand(-220.0f, 220.0f);
    if (x < SCREEN_W * 0.30f && vx < 0.0f) vx = -vx;
    if (x > SCREEN_W * 0.70f && vx > 0.0f) vx = -vx;

    float size = FOOD_SIZE;
    if (kind == ItemKind::Potato) size *= 1.5f;

    place_item(kind, pos, {vx, vy}, frand(0.0f, 360.0f), frand(-120.0f, 120.0f), size, false);
    if (kind != ItemKind::Bomb) ingredients_spawned++;
}

bool segment_hits_item(Vector2 a, Vector2 b, const Item& it) {
    Vector2 ab = { b.x - a.x, b.y - a.y };
    float ab_len_sq = ab.x*ab.x + ab.y*ab.y;
    Vector2 closest;
    if (ab_len_sq < 0.0001f) {
        closest = a;
    } else {
        Vector2 ap = { it.pos.x - a.x, it.pos.y - a.y };
        float t = (ap.x*ab.x + ap.y*ab.y) / ab_len_sq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        closest = { a.x + ab.x * t, a.y + ab.y * t };
    }
    Vector2 d = { it.pos.x - closest.x, it.pos.y - closest.y };
    float dist_sq = d.x*d.x + d.y*d.y;
    float r = it.size;
    if (it.kind == ItemKind::Tentacle) r = it.size * 1.6f;
    return dist_sq < r * r;
}

void slice_item(int idx) {
    Item it = items[idx];
    items[idx].alive = false;

    audio_play_sfx_volume(Sfx::Slice, 0.5f);

    if (it.kind == ItemKind::Bomb) {
        bombs_sliced++;
        round_time       += 3.0f;
        timer_flash_timer = 0.55f;
        timer_popup_timer = 1.0f;
        for (int i = 0; i < 5; i++) {
            place_item(ItemKind::Bomb,
                       { it.pos.x + frand(-8.0f, 8.0f),  it.pos.y + frand(-8.0f, 8.0f) },
                       { it.vel.x + frand(-300.0f, 300.0f), it.vel.y + frand(-400.0f, -120.0f) },
                       frand(0.0f, 360.0f), frand(-300.0f, 300.0f),
                       it.size * 0.4f, true);
        }
        audio_play(Sfx::SliceBoot);
        return;
    }

    int   pieces;
    float frag_size;
    if (it.kind == ItemKind::Potato) { pieces = 5; frag_size = it.size * 0.6f; }
    else                             { pieces = 3; frag_size = it.size * 0.8f; }

    if (it.kind == ItemKind::Tentacle) audio_play(Sfx::SliceTentacle);
    if (it.kind == ItemKind::Potato) audio_play(Sfx::SlicePotato);

    {
        float base_rot  = frand(0.0f, 360.0f);
        float base_size = frand(336.0f, 576.0f);
        int   spawned   = 0;
        for (int i = 0; i < MAX_FLASHES && spawned < 2; i++) {
            if (!flashes[i].alive) {
                flashes[i] = { true, it.pos,
                               base_rot + spawned * 75.0f,
                               base_size,
                               FLASH_DUR };
                spawned++;
            }
        }
    }

    ingredients_sliced++;
    for (int i = 0; i < pieces; i++) {
        place_item(it.kind,
                   { it.pos.x + frand(-15.0f, 15.0f),  it.pos.y + frand(-15.0f, 15.0f) },
                   { it.vel.x + frand(-220.0f, 220.0f), it.vel.y + frand(-380.0f, -100.0f) },
                   frand(0.0f, 360.0f), frand(-360.0f, 360.0f),
                   frag_size, true);
    }
}

void update_items(float dt) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].alive) continue;
        items[i].vel.y += GRAVITY * dt;
        items[i].pos.x += items[i].vel.x * dt;
        items[i].pos.y += items[i].vel.y * dt;
        items[i].rot   += items[i].rot_vel * dt;
        if (items[i].pos.y > SCREEN_H + 120.0f) items[i].alive = false;
    }
}

void draw_item(const Item& it) {
    switch (it.kind) {
        case ItemKind::Potato: {
            float w = it.size * 2.0f;
            Rectangle r = { it.pos.x, it.pos.y, w, w };
            Vector2 origin = { w * 0.5f, w * 0.5f };
            if(it.fragment){
                DrawTexturePro(potato_slice_texture, potato_slice_texture_rect, r, origin, it.rot, WHITE);
            } else {
                DrawTexturePro(potato_texture, potato_texture_rect, r, origin, it.rot, WHITE);
            }
            break;
        }
        case ItemKind::Tentacle: {
            float w = it.size * 2.0f;
            float h = it.size * (it.fragment ? 1.6f : 5.0f);
            Rectangle r = { it.pos.x, it.pos.y, w, h };
            Vector2 origin = { w * 0.5f, h * 0.5f };
            if(it.fragment){
                DrawTexturePro(tentacle_slice_texture, tentacle_slice_texture_rect, r, origin, it.rot, WHITE);
            } else {
                DrawTexturePro(tentacle_texture, tentacle_texture_rect, r, origin, it.rot, WHITE);
            }
            break;
        }
        case ItemKind::Bomb: {
            float w = it.size * 3.0f;
            Rectangle r = { it.pos.x, it.pos.y, w, w };
            Vector2 origin = { w * 0.5f, w * 0.5f };
            DrawTexturePro(boot_texture, boot_texture_rect, r, origin, it.rot, WHITE);
            break;
        }
    }
}

void update_trail(Vector2 mouse_now) {
    trail_head = (trail_head + 1) % TRAIL_LEN;
    trail[trail_head] = mouse_now;
    if (trail_count < TRAIL_LEN) trail_count++;
}

void draw_trail() {
    if (trail_count < 2) return;
    for (int i = 0; i < trail_count - 1; i++) {
        int idx_new = (trail_head - i + TRAIL_LEN) % TRAIL_LEN;
        int idx_old = (trail_head - i - 1 + TRAIL_LEN) % TRAIL_LEN;
        float alpha = 1.0f - (float)i / (float)trail_count;
        Color c = (Color){ 255, 240, 90, (unsigned char)(alpha * 220.0f) };
        float thickness = 3.0f + 4.0f * alpha;
        DrawLineEx(trail[idx_old], trail[idx_new], thickness, c);
    }
}

} // namespace

void cutting_minigame_init() {
    for (int i = 0; i < MAX_ITEMS; i++) items[i].alive = false;
    trail_head          = 0;
    trail_count         = 0;
    mouse_initialized   = false;
    elapsed             = 0.0f;
    round_time          = ROUND_TIME;
    spawn_timer         = 0.0f;
    ingredients_spawned = 0;
    ingredients_sliced  = 0;
    bombs_sliced        = 0;
    finished            = false;
    final_score         = 0.0f;
    phase               = Phase::Playing;
    results_elapsed     = 0.0f;
    timer_flash_timer   = 0.0f;
    timer_popup_timer   = 0.0f;

    tentacle_texture = LoadTexture("assets/food/plokami.png");
    tentacle_texture_rect = { 0.0f, 0.0f, (float)tentacle_texture.width, (float)tentacle_texture.height };

    potato_texture = LoadTexture("assets/food/potato.png");
    potato_texture_rect = { 0.0f, 0.0f, (float)potato_texture.width, (float)potato_texture.height };

    tentacle_slice_texture = LoadTexture("assets/food/plokami_slice.png");
    tentacle_slice_texture_rect = { 0.0f, 0.0f, (float)tentacle_slice_texture.width, (float)tentacle_slice_texture.height };

    potato_slice_texture = LoadTexture("assets/food/potato_slice.png");
    potato_slice_texture_rect = { 0.0f, 0.0f, (float)potato_slice_texture.width, (float)potato_slice_texture.height };

    boot_texture = LoadTexture("assets/misc/arbulo.png");
    boot_texture_rect = { 0.0f, 0.0f, (float)boot_texture.width, (float)boot_texture.height };

    bg_texture = LoadTexture("assets/misc/woodcut.png");
    bg_texture_rect = { 0.0f, 0.0f, (float)bg_texture.width, (float)bg_texture.height };

    slice_tex = LoadTexture("assets/misc/slice.png");

    for (int i = 0; i < MAX_FLASHES; i++) flashes[i].alive = false;

    HideCursor();
}

bool cutting_minigame_update(float dt) {
    if (finished) return false;

    if (phase == Phase::Results) {
        results_elapsed += dt;
        if (results_elapsed > 0.4f &&
            (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetKeyPressed() != 0)) {
            finished = true;
            UnloadTexture(tentacle_texture);
            UnloadTexture(tentacle_slice_texture);
            UnloadTexture(potato_texture);
            UnloadTexture(potato_slice_texture);
            UnloadTexture(boot_texture);
            UnloadTexture(bg_texture);
            if (slice_tex.id) UnloadTexture(slice_tex);
            return true;
        }
        return false;
    }

    elapsed     += dt;
    spawn_timer -= dt;
    if (timer_flash_timer > 0.0f) timer_flash_timer -= dt;
    if (timer_popup_timer > 0.0f) timer_popup_timer -= dt;
    if (elapsed < round_time - 2.5f && spawn_timer <= 0.0f) {
        spawn_random();
        spawn_timer = frand(0.35f, 0.7f);
    }

    Vector2 mouse_now = GetMousePosition();
    update_trail(mouse_now);

    if (mouse_initialized) {
        Vector2 d = { mouse_now.x - mouse_prev.x, mouse_now.y - mouse_prev.y };
        float dist_sq = d.x*d.x + d.y*d.y;
        if (dist_sq > 9.0f) {
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (items[i].alive && !items[i].fragment) {
                    if (segment_hits_item(mouse_prev, mouse_now, items[i])) {
                        slice_item(i);
                    }
                }
            }
        }
    }
    mouse_prev = mouse_now;
    mouse_initialized = true;

    update_items(dt);
    for (int i = 0; i < MAX_FLASHES; i++)
        if (flashes[i].alive && (flashes[i].timer -= dt) <= 0.0f)
            flashes[i].alive = false;

    if (elapsed >= round_time) {
        int spawned = ingredients_spawned > 0 ? ingredients_spawned : 1;
        float raw = (float)(ingredients_sliced - 2 * bombs_sliced) / (float)spawned;
        if (raw < 0.0f) raw = 0.0f;
        if (raw > 1.0f) raw = 1.0f;
        final_score = raw;
        phase = Phase::Results;
        results_elapsed = 0.0f;
        ShowCursor();
        return false;
    }
    return false;
}

void cutting_minigame_draw() {
    ClearBackground(BLACK);
    DrawTexturePro(bg_texture, bg_texture_rect,
                   { 0.0f, 0.0f, SCREEN_W, SCREEN_H }, { 0.0f, 0.0f }, 0.0f, WHITE);

    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].alive) draw_item(items[i]);
    }

    if (slice_tex.id) {
        for (int i = 0; i < MAX_FLASHES; i++) {
            if (!flashes[i].alive) continue;
            float t      = 1.0f - flashes[i].timer / FLASH_DUR;  // 0→1
            unsigned char alpha = (unsigned char)((1.0f - t) * 255.0f);
            float dw = flashes[i].size;
            float dh = dw * (float)slice_tex.height / (float)slice_tex.width;
            Rectangle src = { 0, 0, (float)slice_tex.width, (float)slice_tex.height };
            Rectangle dst = { flashes[i].pos.x, flashes[i].pos.y, dw, dh };
            DrawTexturePro(slice_tex, src, dst, { dw * 0.482f, dh * 0.738f },
                           flashes[i].rot, (Color){ 255, 255, 255, alpha });
        }
    }

    draw_trail();

    Color timer_col = (timer_flash_timer > 0.0f) ? (Color){255, 60, 60, 255} : RAYWHITE;
    {
        constexpr int TIMER_FONT = 80;
        const char* time_str = TextFormat("Time: %.1f", round_time - elapsed > 0.0f ? round_time - elapsed : 0.0f);
        int tw = MeasureText(time_str, TIMER_FONT);
        DrawText(time_str, (int)(SCREEN_W * 0.5f - tw * 0.5f), 18, TIMER_FONT, timer_col);
    }
    if (timer_popup_timer > 0.0f) {
        float t       = timer_popup_timer;                       // 1→0
        float popup_y = 115.0f - (1.0f - t) * 50.0f;           // starts at 115, floats up to 65
        unsigned char alpha = (unsigned char)(t * 255.0f);
        const char* ps = "+3s";
        int pw = MeasureText(ps, 44);
        DrawText(ps, (int)(SCREEN_W * 0.5f - pw * 0.5f), (int)popup_y, 44, (Color){255, 60, 60, alpha});
    }
    DrawText("Move mouse to slice", SCREEN_W - 290, 30, 20, (Color){240, 240, 240, 255});

    if (phase == Phase::Results) {
        DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){0, 0, 0, 180});
        const char* title = "Preparation Complete";
        int tw = MeasureText(title, 64);
        DrawText(title, (int)(SCREEN_W * 0.5f - tw * 0.5f), 220, 64, RAYWHITE);

        const char* score_str = TextFormat("%.0f%%", final_score * 100.0f);
        int sw = MeasureText(score_str, 160);
        DrawText(score_str, (int)(SCREEN_W * 0.5f - sw * 0.5f), 340, 160, RAYWHITE);

        const char* sub = TextFormat("Sliced %d / %d     Boots hit: %d",
                                     ingredients_sliced, ingredients_spawned, bombs_sliced);
        int subw = MeasureText(sub, 30);
        DrawText(sub, (int)(SCREEN_W * 0.5f - subw * 0.5f), 560, 30, (Color){210, 210, 210, 255});

        const char* hint = "Click or press any key to continue";
        int hw = MeasureText(hint, 28);
        DrawText(hint, (int)(SCREEN_W * 0.5f - hw * 0.5f), 720, 28, (Color){170, 170, 170, 255});
    }
}

float cutting_minigame_score() {
    return final_score;
}
