#include "drink_minigame.h"
#include "audio.h"
#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

namespace {

constexpr float SCREEN_W                  = 1920.0f;
constexpr float SCREEN_H                  = 1080.0f;
constexpr int   MAX_PARTICLES             = 256;
constexpr int   TOTAL_PARTICLES_PER_ROUND = 130;
constexpr float ROUND_TIME                = 7.5f;
constexpr float GRAVITY                   = 1100.0f;
constexpr float CUP_W                     = 230.0f;
constexpr float CUP_H                     = 60.0f;
constexpr float CUP_Y                     = SCREEN_H - 160.0f;
constexpr float SPAWN_Y                   = -40.0f;
constexpr float SPAWN_X_MARGIN            = 220.0f;
constexpr float SPAWN_X_SEED_STEP         = 0.04f;
constexpr float CUP_SPEED                 = 900.0f;
constexpr float EYEBALL_CHANCE            = 0.1f;
constexpr float BASE_RADIUS               = 32.0f;
constexpr float EYEBALL_RADIUS            = 55.0f;

enum class Phase        { Selecting, Playing, Results };
enum class DrinkKind    { Boba, Milkshake, Other };
enum class ParticleKind { Liquid, Eyeball };

struct Particle {
    bool         alive;
    ParticleKind kind;
    Vector2      pos;
    Vector2      vel;
    float        radius;
};

Phase     phase                    = Phase::Selecting;
DrinkKind selected                 = DrinkKind::Boba;
Particle  particles[MAX_PARTICLES];
int       particles_spawned        = 0;
int       particles_caught         = 0;
float     elapsed                  = 0.0f;
float     spawn_timer              = 0.0f;
float     spawn_noise_seed         = 0.0f;
float     spawn_x_seed             = 0.0f;
float     cup_x                    = SCREEN_W * 0.5f;
bool      finished                 = false;
float     final_score              = 0.0f;
float     results_elapsed          = 0.0f;

Rectangle card_left  = { SCREEN_W * 0.5f - 480.0f, 280.0f, 360.0f, 540.0f };
Rectangle card_right = { SCREEN_W * 0.5f + 120.0f, 280.0f, 360.0f, 540.0f };

Texture boba_texture           = {};
bool    boba_texture_loaded    = false;
Texture eyeball_texture        = {};
bool    eyeball_texture_loaded = false;
Texture milkshake_texture      = {};
bool    milkshake_texture_loaded = false;
Texture cup_texture            = {};
bool    cup_texture_loaded     = false;
Texture cup_front_texture      = {};
bool    cup_front_texture_loaded = false;

float frand(float a, float b) {
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

float hash1(int n) {
    n = (n << 13) ^ n;
    int v = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
    return 1.0f - (float)v / 1073741824.0f;
}

float lerp1(float a, float b, float t)  { return a + (b - a) * t; }
float smoothstep1(float t)              { return t * t * (3.0f - 1.8f * t); }

float smooth_noise_1d(float x) {
    int   i = (int)floorf(x);
    float f = x - (float)i;
    return lerp1(hash1(i), hash1(i + 1), smoothstep1(f));
}

int alloc_particle() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].alive) return i;
    }
    return -1;
}

void spawn_particle() {
    if (particles_spawned >= TOTAL_PARTICLES_PER_ROUND) return;
    int idx = alloc_particle();
    if (idx < 0) return;

    spawn_noise_seed += 0.45f;
    float n          = smooth_noise_1d(spawn_noise_seed);
    float size_mult  = 1.0f + 0.4f * n;

    ParticleKind k = ParticleKind::Liquid;
    float radius   = BASE_RADIUS * size_mult;
    if (selected == DrinkKind::Boba && frand(0.0f, 1.0f) < EYEBALL_CHANCE) {
        k      = ParticleKind::Eyeball;
        radius = EYEBALL_RADIUS;
    }

    spawn_x_seed += SPAWN_X_SEED_STEP;
    float xn      = smooth_noise_1d(spawn_x_seed);
    float x_min   = SPAWN_X_MARGIN;
    float x_max   = SCREEN_W - SPAWN_X_MARGIN;
    float x       = (x_min + x_max) * 0.5f + xn * (x_max - x_min) * 0.5f;

    Vector2 pos = { x + frand(-12.0f, 12.0f), SPAWN_Y };
    Vector2 vel = { frand(-40.0f, 40.0f), frand(160.0f, 240.0f) };
    particles[idx] = { true, k, pos, vel, radius };
    particles_spawned++;
}

bool point_in_cup(Vector2 p) {
    float left   = cup_x - CUP_W * 0.5f;
    float right  = cup_x + CUP_W * 0.5f;
    float top    = CUP_Y;
    float bottom = CUP_Y + CUP_H;
    return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
}

void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].alive) continue;
        Particle& p = particles[i];
        p.vel.y += GRAVITY * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        if (p.pos.x < p.radius)              { p.pos.x = p.radius;              p.vel.x =  fabsf(p.vel.x); }
        if (p.pos.x > SCREEN_W - p.radius)   { p.pos.x = SCREEN_W - p.radius;   p.vel.x = -fabsf(p.vel.x); }
        if (point_in_cup(p.pos)) {
            particles_caught++;
            p.alive = false;
            if (p.kind == ParticleKind::Eyeball) {
                audio_play(Sfx::EyePop);
            } else {
                float ratio = (float)particles_caught / (float)TOTAL_PARTICLES_PER_ROUND;
                float vol   = 0.1f + ratio * 0.9f;
                Sfx   sfx   = rand() % 2 == 0 ? Sfx::BubblePop1 : Sfx::BubblePop2;
                audio_play_sfx_volume(sfx, vol);
            }
            continue;
        }
        if (p.pos.y > SCREEN_H + 50.0f) p.alive = false;
    }
}

void draw_particles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].alive) continue;
        const Particle& p = particles[i];
        if (p.kind == ParticleKind::Liquid) {
            Color c = (selected == DrinkKind::Boba)
                ? (Color){180, 220,  90, 230}
                : (Color){200,  90, 180, 230};
            DrawCircleV(p.pos, p.radius, c);
            DrawCircleLines((int)p.pos.x, (int)p.pos.y, p.radius, BLACK);
        } else if (eyeball_texture_loaded) {
            float w = p.radius * 2.0f;
            Rectangle src = { 0.0f, 0.0f, (float)eyeball_texture.width, (float)eyeball_texture.height };
            Rectangle dst = { p.pos.x, p.pos.y, w, w };
            Vector2 origin = { p.radius, p.radius };
            DrawTexturePro(eyeball_texture, src, dst, origin, 0.0f, WHITE);
        } else {
            DrawCircleV(p.pos, p.radius, RAYWHITE);
            DrawCircleLines((int)p.pos.x, (int)p.pos.y, p.radius, BLACK);
            DrawCircleV(p.pos, p.radius * 0.55f, (Color){90, 60, 35, 255});
            DrawCircleV(p.pos, p.radius * 0.28f, BLACK);
        }
    }
}

void draw_cup_back() {
    if (cup_texture_loaded) {
        float draw_h = CUP_H * 3.5f;
        float aspect = (float)cup_texture.width / (float)cup_texture.height;
        float draw_w = draw_h * aspect;
        Rectangle src = { 0, 0, (float)cup_texture.width, (float)cup_texture.height };
        Rectangle dst = { cup_x - draw_w * 0.5f, CUP_Y + CUP_H * 0.5f - draw_h * 0.5f, draw_w, draw_h };
        DrawTexturePro(cup_texture, src, dst, { 0, 0 }, 0.0f, (Color){255, 255, 255, 128});
    } else {
        Rectangle cup = { cup_x - CUP_W * 0.5f, CUP_Y, CUP_W, CUP_H };
        DrawRectangleRounded(cup, 0.25f, 8, (Color){240, 240, 240, 200});
        DrawRectangleRoundedLines(cup, 0.25f, 8, BLACK);
    }
}

void draw_cup_front() {
    if (!cup_front_texture_loaded) return;
    float draw_h = CUP_H * 3.5f;
    float aspect = (float)cup_front_texture.width / (float)cup_front_texture.height;
    float draw_w = draw_h * aspect;
    Rectangle src = { 0, 0, (float)cup_front_texture.width, (float)cup_front_texture.height };
    Rectangle dst = { cup_x - draw_w * 0.5f, CUP_Y + CUP_H * 0.5f - draw_h * 0.5f, draw_w, draw_h };
    DrawTexturePro(cup_front_texture, src, dst, { 0, 0 }, 0.0f, (Color){255, 255, 255, 128});
}

bool point_in_rect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width
        && p.y >= r.y && p.y <= r.y + r.height;
}

void draw_drink_card(Rectangle r, Color bg, const char* title, const char* hotkey) {
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 4, RAYWHITE);
    if (!strcmp("Eyeball Boba", title) && boba_texture_loaded) {
        Rectangle src = { 0, 0, (float)boba_texture.width, (float)boba_texture.height };
        Rectangle dst = { r.x + (r.width - 240.0f) * 0.5f, r.y + 50.0f, 240.0f, 360.0f };
        DrawTexturePro(boba_texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    } else if (!strcmp("Mystery Drink", title) && milkshake_texture_loaded) {
        Rectangle src = { 0, 0, (float)milkshake_texture.width, (float)milkshake_texture.height };
        Rectangle dst = { r.x + (r.width - 240.0f) * 0.5f, r.y + 50.0f, 240.0f, 360.0f };
        DrawTexturePro(milkshake_texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    }
    int tw = MeasureText(title, 32);
    DrawText(title, (int)(r.x + r.width * 0.5f - tw * 0.5f), (int)(r.y + r.height - 70), 32, RAYWHITE);
    DrawText(hotkey, (int)(r.x + r.width - 60), (int)(r.y + r.height - 60), 32, RAYWHITE);
}

void start_playing() {
    phase             = Phase::Playing;
    elapsed           = 0.0f;
    spawn_timer       = 0.0f;
    particles_spawned = 0;
    particles_caught  = 0;
    spawn_noise_seed  = frand(0.0f, 1000.0f);
    spawn_x_seed      = frand(0.0f, 1000.0f);
    cup_x             = SCREEN_W * 0.5f;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].alive = false;
    HideCursor();
}

void update_selection() {
    Vector2 m = GetMousePosition();
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (IsKeyPressed(KEY_ONE) || (clicked && point_in_rect(m, card_left))) {
        selected = DrinkKind::Boba;
        start_playing();
    } else if (IsKeyPressed(KEY_TWO) || (clicked && point_in_rect(m, card_right))) {
        selected = DrinkKind::Other;
        start_playing();
    }
}

void update_cup_input(float dt) {
    bool keyboard_active = IsKeyDown(KEY_A) || IsKeyDown(KEY_D);
    if (keyboard_active) {
        if (IsKeyDown(KEY_A)) cup_x -= CUP_SPEED * dt;
        if (IsKeyDown(KEY_D)) cup_x += CUP_SPEED * dt;
    } else {
        Vector2 md = GetMouseDelta();
        if (md.x * md.x + md.y * md.y > 4.0f) {
            cup_x = GetMousePosition().x;
        }
    }
    float min_x = CUP_W * 0.5f;
    float max_x = SCREEN_W - CUP_W * 0.5f;
    if (cup_x < min_x) cup_x = min_x;
    if (cup_x > max_x) cup_x = max_x;
}

bool any_particle_alive() {
    for (int i = 0; i < MAX_PARTICLES; i++) if (particles[i].alive) return true;
    return false;
}

} // namespace

void drink_minigame_init() {
    phase             = Phase::Selecting;
    selected          = DrinkKind::Boba;
    particles_spawned = 0;
    particles_caught  = 0;
    elapsed           = 0.0f;
    spawn_timer       = 0.0f;
    spawn_noise_seed  = 0.0f;
    cup_x             = SCREEN_W * 0.5f;
    finished          = false;
    final_score       = 0.0f;
    results_elapsed   = 0.0f;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].alive = false;

    if (!boba_texture_loaded && FileExists("assets/food/boba_tea.png")) {
        boba_texture = LoadTexture("assets/food/boba_tea.png");
        boba_texture_loaded = true;
    }

    if (!milkshake_texture_loaded && FileExists("assets/food/milkshake.png")) {
        milkshake_texture = LoadTexture("assets/food/milkshake.png");
        milkshake_texture_loaded = true;
    }

    if (!eyeball_texture_loaded && FileExists("assets/misc/mati.png")) {
        eyeball_texture = LoadTexture("assets/misc/mati.png");
        eyeball_texture_loaded = true;
    }

    if (!cup_texture_loaded) {
        cup_texture = LoadTexture("assets/misc/cup.png");
        cup_texture_loaded = (cup_texture.id != 0);
    }
    if (!cup_front_texture_loaded) {
        cup_front_texture = LoadTexture("assets/misc/cup_front.png");
        cup_front_texture_loaded = (cup_front_texture.id != 0);
    }

    ShowCursor();
}

bool drink_minigame_update(float dt) {
    if (finished) return false;

    if (phase == Phase::Selecting) {
        update_selection();
        return false;
    }

    if (phase == Phase::Results) {
        results_elapsed += dt;
        if (results_elapsed > 0.4f &&
            (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetKeyPressed() != 0)) {
            finished = true;
            return true;
        }
        return false;
    }

    elapsed     += dt;
    spawn_timer -= dt;

    update_cup_input(dt);

    if (particles_spawned < TOTAL_PARTICLES_PER_ROUND && spawn_timer <= 0.0f) {
        spawn_particle();
        float base_interval = ROUND_TIME / (float)TOTAL_PARTICLES_PER_ROUND;
        float n             = smooth_noise_1d(elapsed * 1.7f);
        float interval      = base_interval * (0.5f + (n + 1.0f) * 0.5f);
        spawn_timer         = interval;
    }

    update_particles(dt);

    bool all_spawned = particles_spawned >= TOTAL_PARTICLES_PER_ROUND;
    bool all_done    = all_spawned && !any_particle_alive();
    bool hard_cutoff = elapsed >= ROUND_TIME + 5.0f;
    if (all_done || hard_cutoff) {
        float raw = (float)particles_caught / (float)TOTAL_PARTICLES_PER_ROUND;
        if (raw < 0.0f) raw = 0.0f;
        if (raw > 1.0f) raw = 1.0f;
        final_score = raw;
        phase = Phase::Results;
        results_elapsed = 0.0f;
        ShowCursor();
        audio_play(Sfx::DrinkComplete);
        return false;
    }
    return false;
}

void drink_minigame_draw() {
    if (phase == Phase::Selecting) {
        ClearBackground((Color){40, 30, 50, 255});
        const char* title = "Select a drink";
        int tw = MeasureText(title, 64);
        DrawText(title, (int)(SCREEN_W * 0.5f - tw * 0.5f), 100, 64, RAYWHITE);
        const char* hint = "Click a card or press 1 / 2";
        int hw = MeasureText(hint, 28);
        DrawText(hint, (int)(SCREEN_W * 0.5f - hw * 0.5f), 190, 28, (Color){200, 200, 200, 255});
        draw_drink_card(card_left,  (Color){70, 110,  60, 255}, "Eyeball Boba",  "[1]");
        draw_drink_card(card_right, (Color){80,  50, 110, 255}, "Mystery Drink", "[2]");
        return;
    }

    Color bg = (selected == DrinkKind::Boba)
        ? (Color){25, 50, 30, 255}
        : (Color){45, 25, 55, 255};
    ClearBackground(bg);

    draw_cup_back();
    draw_particles();
    draw_cup_front();

    DrawText(TextFormat("Caught: %d / %d", particles_caught, TOTAL_PARTICLES_PER_ROUND),
             40, 30, 22, (Color){220, 220, 220, 255});
    DrawText("A/D or mouse to move cup", (int)(SCREEN_W - 380), 30, 22, (Color){180, 180, 180, 255});

    if (phase == Phase::Results) {
        bool perfect = (final_score >= 1.0f);
        bool failed  = (final_score < 0.5f);

        DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){0, 0, 0, 190});

        // Drink image(s) — two side-by-side on perfect, dimmed on fail
        Texture drink_tex = (selected == DrinkKind::Boba) ? boba_texture : milkshake_texture;
        bool    drink_ok  = (selected == DrinkKind::Boba) ? boba_texture_loaded : milkshake_texture_loaded;
        if (drink_ok && drink_tex.id != 0) {
            float cy     = 400.0f;
            float dh     = perfect ? 210.0f : 260.0f;
            float aspect = (float)drink_tex.width / (float)drink_tex.height;
            float dw     = dh * aspect;
            float tilt   = 14.0f;
            float pulse  = 0.5f + 0.5f * sinf(results_elapsed * 4.0f);
            Rectangle src    = { 0, 0, (float)drink_tex.width, (float)drink_tex.height };
            Color     img_tint = failed ? (Color){255, 255, 255, 80} : WHITE;

            Color glow_col = (selected == DrinkKind::Boba)
                ? (Color){ 120, 255, 120, 255 }
                : (Color){ 255, 180, 255, 255 };

            int   draw_count = perfect ? 2 : 1;
            float offsets[]  = { -110.0f, 110.0f };
            float tilts[]    = { -tilt, tilt };

            for (int d = 0; d < draw_count; d++) {
                float cx = SCREEN_W * 0.5f + (perfect ? offsets[d] : 0.0f);
                float dt_tilt = perfect ? tilts[d] : tilt;
                Vector2 origin = { dw * 0.5f, dh * 0.5f };

                if (!failed) {
                    float glow_scales[] = { 1.55f, 1.35f, 1.18f };
                    unsigned char glow_alphas[] = {
                        (unsigned char)(25 + 20 * pulse),
                        (unsigned char)(35 + 25 * pulse),
                        (unsigned char)(50 + 30 * pulse)
                    };
                    for (int g = 0; g < 3; g++) {
                        float gw = dw * glow_scales[g], gh = dh * glow_scales[g];
                        Rectangle gdst = { cx, cy, gw, gh };
                        Vector2   go   = { gw * 0.5f, gh * 0.5f };
                        DrawTexturePro(drink_tex, src, gdst, go, dt_tilt,
                                       (Color){ glow_col.r, glow_col.g, glow_col.b, glow_alphas[g] });
                    }
                }
                Rectangle dst = { cx, cy, dw, dh };
                DrawTexturePro(drink_tex, src, dst, origin, dt_tilt, img_tint);
            }
        }

        // Title
        const char* title;
        Color       title_col;
        if (perfect)      { title = "PERFECT!";       title_col = (Color){255, 215,  0, 255}; }
        else if (failed)  { title = "FAILED!";         title_col = (Color){255,  60, 60, 255}; }
        else              { title = "Drink Complete";  title_col = RAYWHITE; }
        int tw = MeasureText(title, 56);
        DrawText(title, (int)(SCREEN_W * 0.5f - tw * 0.5f), 130, 56, title_col);

        // Score percentage
        Color score_col = perfect ? (Color){255, 215, 0, 255}
                        : (failed  ? (Color){255, 60, 60, 255} : RAYWHITE);
        const char* score_str = TextFormat("%.0f%%", final_score * 100.0f);
        int sw = MeasureText(score_str, 130);
        DrawText(score_str, (int)(SCREEN_W * 0.5f - sw * 0.5f), 610, 130, score_col);

        // Caught count
        const char* sub = TextFormat("Caught %d / %d", particles_caught, TOTAL_PARTICLES_PER_ROUND);
        int subw = MeasureText(sub, 28);
        DrawText(sub, (int)(SCREEN_W * 0.5f - subw * 0.5f), 760, 28, (Color){210, 210, 210, 255});

        // Outcome message
        const char* outcome;
        Color       outcome_col;
        if (perfect)     { outcome = "Bonus: +1 extra drink added to inventory!"; outcome_col = (Color){ 80, 230,  80, 255}; }
        else if (failed) { outcome = "Not enough caught - no drink added!";        outcome_col = (Color){255, 100, 100, 255}; }
        else             { outcome = "Drink added to inventory!";                  outcome_col = (Color){210, 210, 210, 255}; }
        int ow = MeasureText(outcome, 26);
        DrawText(outcome, (int)(SCREEN_W * 0.5f - ow * 0.5f), 800, 26, outcome_col);

        const char* hint = "Click or press any key to continue";
        int hw = MeasureText(hint, 26);
        DrawText(hint, (int)(SCREEN_W * 0.5f - hw * 0.5f), 848, 26, (Color){170, 170, 170, 255});
    }
}

float drink_minigame_score() {
    return final_score;
}

DrinkResult drink_minigame_result() {
    if (selected == DrinkKind::Boba)  return DrinkResult::EyeballBoba;
    if (selected == DrinkKind::Other) return DrinkResult::MysteryDrink;
    return DrinkResult::None;
}
