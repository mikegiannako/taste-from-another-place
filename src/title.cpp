#include "title.h"
#include "raylib.h"
#include <math.h>

namespace {

constexpr float SCREEN_W  = 1920.0f;
constexpr float SCREEN_H  = 1080.0f;
constexpr float BTN_W     = 780.0f;   // desired render width of the button
constexpr float BTN_CY    = 850.0f;   // vertical centre of button (bottom half)
constexpr float BOB_AMP   =  12.0f;   // idle vertical bob amplitude (px)
constexpr float BOB_SPEED =  1.8f;    // radians per second (period ≈ 3.5 s)
constexpr float CLICK_DUR =  0.35f;   // seconds the click animation plays before transitioning

Texture2D bg_tex  = {};
Texture2D btn_tex = {};

float anim_time   = 0.0f;
float click_timer = 0.0f;   // counts down from CLICK_DUR after click
bool  clicked     = false;
bool  done        = false;

// Returns the rendered button height based on the loaded texture aspect ratio.
float btn_h() {
    if (btn_tex.id != 0 && btn_tex.width > 0)
        return BTN_W * (float)btn_tex.height / (float)btn_tex.width;
    return 90.0f;
}

// Hit-test the button in its current idle position (no squish applied for input).
bool mouse_over_button(Vector2 m) {
    float bob = sinf(anim_time * BOB_SPEED) * BOB_AMP;
    float bx  = SCREEN_W * 0.5f - BTN_W * 0.5f;
    float by  = BTN_CY - btn_h() * 0.5f + bob;
    return m.x >= bx && m.x <= bx + BTN_W
        && m.y >= by && m.y <= by + btn_h();
}

} // namespace

void title_init() {
    if (FileExists("assets/main_screen/start_screen.png"))
        bg_tex  = LoadTexture("assets/main_screen/start_screen.png");
    if (FileExists("assets/main_screen/start_button.png"))
        btn_tex = LoadTexture("assets/main_screen/start_button.png");
    anim_time   = 0.0f;
    click_timer = 0.0f;
    clicked     = false;
    done        = false;
}

void title_shutdown() {
    if (bg_tex.id)  { UnloadTexture(bg_tex);  bg_tex  = {}; }
    if (btn_tex.id) { UnloadTexture(btn_tex); btn_tex = {}; }
}

bool title_update(float dt) {
    anim_time += dt;

    if (clicked) {
        click_timer -= dt;
        if (click_timer <= 0.0f) done = true;
        return done;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (mouse_over_button(GetMousePosition())) {
            clicked     = true;
            click_timer = CLICK_DUR;
        }
    }
    return false;
}

void title_draw() {
    // ── background ────────────────────────────────────────────────────────────
    if (bg_tex.id != 0) {
        Rectangle src = { 0, 0, (float)bg_tex.width, (float)bg_tex.height };
        DrawTexturePro(bg_tex, src, { 0, 0, SCREEN_W, SCREEN_H }, { 0, 0 }, 0.0f, WHITE);
    } else {
        ClearBackground((Color){ 18, 10, 38, 255 });
        const char* fb = "OUT OF PLACE";
        int tw = MeasureText(fb, 80);
        DrawText(fb, (int)(SCREEN_W * 0.5f - tw * 0.5f), 340, 80, RAYWHITE);
    }

    // ── button geometry ───────────────────────────────────────────────────────
    float bh = btn_h();

    // Idle bob (frozen during click to avoid jitter)
    float bob = clicked ? sinf(anim_time * BOB_SPEED) * BOB_AMP
                        : sinf(anim_time * BOB_SPEED) * BOB_AMP;

    // Click squish: peaks at midpoint of the animation
    float scaleX = 1.0f, scaleY = 1.0f;
    Color tint   = WHITE;

    if (clicked) {
        float progress = 1.0f - click_timer / CLICK_DUR;   // 0→1
        float squish   = sinf(progress * PI);               // 0→1→0 bell

        scaleX = 1.0f + squish * 0.08f;   // widen slightly
        scaleY = 1.0f - squish * 0.14f;   // squish down

        // Warm orange flash that peaks at the squish peak
        unsigned char g = (unsigned char)(255 - (int)(squish * 110));
        unsigned char b = (unsigned char)(255 - (int)(squish * 200));
        tint = (Color){ 255, g, b, 255 };

        bob = 0.0f;   // freeze bob mid-click
    } else {
        // Idle glow pulse – gentle brightness oscillation in sync with bob
        float glow = 0.5f + 0.5f * sinf(anim_time * BOB_SPEED);
        unsigned char cv = (unsigned char)(210 + (int)(glow * 45));
        tint = (Color){ 255, cv, cv, 255 };
    }

    float dw = BTN_W * scaleX;
    float dh = bh    * scaleY;
    float bx = SCREEN_W * 0.5f - dw * 0.5f + 50;
    float by = BTN_CY        - dh * 0.5f + bob;

    // ── draw button ───────────────────────────────────────────────────────────
    if (btn_tex.id != 0) {
        Rectangle src = { 0, 0, (float)btn_tex.width, (float)btn_tex.height };
        Rectangle dst = { bx, by, dw, dh };
        DrawTexturePro(btn_tex, src, dst, { 0, 0 }, 0.0f, tint);
    } else {
        // Fallback: plain rect + text when texture is missing
        DrawRectangleRounded({ bx, by, dw, dh }, 0.4f, 8, (Color){ 60, 120, 220, 255 });
        DrawRectangleRoundedLines({ bx, by, dw, dh }, 0.4f, 8, (Color){ 180, 210, 255, 200 });
        const char* lbl = "START";
        int lw = MeasureText(lbl, 36);
        DrawText(lbl, (int)(bx + dw * 0.5f - lw * 0.5f),
                      (int)(by + dh * 0.5f - 18), 36, WHITE);
    }
}
