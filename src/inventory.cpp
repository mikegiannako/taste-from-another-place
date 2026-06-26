#include "inventory.h"
#include "raylib.h"
#include <math.h>

Inventory g_inventory;

// ── score deduction popups ────────────────────────────────────────────────────

namespace {
struct ScorePopup {
    bool  alive;
    float y;
    float timer;
    float amount;
};
constexpr int   MAX_SCORE_POPUPS = 8;
constexpr float POPUP_LIFETIME   = 1.2f;
ScorePopup s_popups[MAX_SCORE_POPUPS] = {};
float      s_flash_timer     = 0.0f;   // red flash (deduction)
float      s_add_flash_timer = 0.0f;   // green flash (gain)
}

void score_deduct_notify(float amount) {
    s_flash_timer = 0.55f;
    for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
        if (!s_popups[i].alive) {
            s_popups[i] = { true, 55.0f, POPUP_LIFETIME, amount };
            break;
        }
    }
}

void score_add_notify(float amount) {
    s_add_flash_timer = 0.55f;
    for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
        if (!s_popups[i].alive) {
            s_popups[i] = { true, 55.0f, POPUP_LIFETIME, -amount };  // negative = green
            break;
        }
    }
}

void score_notifs_update(float dt) {
    if (s_flash_timer     > 0.0f) s_flash_timer     -= dt;
    if (s_add_flash_timer > 0.0f) s_add_flash_timer -= dt;
    for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
        if (!s_popups[i].alive) continue;
        s_popups[i].timer -= dt;
        s_popups[i].y     -= 38.0f * dt;   // float upward
        if (s_popups[i].timer <= 0.0f) s_popups[i].alive = false;
    }
}

void score_notifs_draw() {
    for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
        if (!s_popups[i].alive) continue;
        float         t     = s_popups[i].timer / POPUP_LIFETIME;
        unsigned char alpha = (unsigned char)(t * 255.0f);
        bool          gain  = s_popups[i].amount < 0.0f;
        const char*   txt   = gain
            ? TextFormat("+%.0f", -s_popups[i].amount)
            : TextFormat("-%.0f",  s_popups[i].amount);
        Color col = gain ? (Color){60, 230, 80, alpha} : (Color){255, 60, 60, alpha};
        int tw = MeasureText(txt, 26);
        DrawText(txt, 1908 - tw, (int)s_popups[i].y, 26, col);
    }
}

// Fit a texture into a box while preserving aspect ratio, centred on (cx, cy).
static void draw_icon(Texture2D tex, float cx, float cy, float box_w, float box_h, Color tint) {
    if (tex.id == 0) return;
    float scale = (tex.width * box_h > tex.height * box_w)
                  ? box_w / (float)tex.width
                  : box_h / (float)tex.height;
    float dw = tex.width  * scale;
    float dh = tex.height * scale;
    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { cx - dw * 0.5f, cy - dh * 0.5f, dw, dh };
    DrawTexturePro(tex, src, dst, { 0, 0 }, 0.0f, tint);
}

void inventory_draw(const InventoryIcons& icons) {
    // Score — top right, flashes red on deduction
    {
        Color       sc  = (s_flash_timer > 0.0f)     ? (Color){255,  60,  60, 255}
                       : (s_add_flash_timer > 0.0f) ? (Color){ 60, 230,  80, 255}
                                                     : (Color){255, 215,   0, 255};
        const char* txt = TextFormat("Score: %.0f", g_inventory.total_score);
        int         tw  = MeasureText(txt, 28);
        DrawText(txt, 1920 - tw - 12, 10, 28, sc);
    }

    constexpr int PX    = 1665;
    constexpr int PY    =  760;
    constexpr int PW    =  245;
    constexpr int PH    =  310;
    constexpr int ROW_H =   32;
    constexpr int ICON  =   26;   // icon box size (square)
    
    DrawRectangle(PX, PY, PW, PH, (Color){ 15,  15,  25, 210 });
    DrawRectangleLinesEx({ (float)PX, (float)PY, (float)PW, (float)PH },
                         2, (Color){ 140, 140, 180, 255 });

    int y = PY + 8;
    DrawText("INVENTORY", PX + 8, y, 20, (Color){ 200, 200, 230, 255 });
    y += 30;

    // Draws one row: icon centred in ICON×ICON box, then label + count.
    // dim = true  → semi-transparent tint (for intermediate / raw-cut items).
    // fallback    → solid-colour box when tex is missing (id == 0).
    auto row = [&](const char* label, int count,
                   Texture2D tex, Color fallback,
                   bool dim = false)
    {
        Color tint = dim ? (Color){ 255, 255, 255, 140 } : WHITE;
        float icx  = (float)(PX + 8 + ICON / 2);
        float icy  = (float)(y + ROW_H / 2);

        if (tex.id != 0) {
            draw_icon(tex, icx, icy, (float)ICON, (float)ICON, tint);
        } else {
            Color fb = dim ? (Color){ (unsigned char)(fallback.r / 2u),
                                      (unsigned char)(fallback.g / 2u),
                                      (unsigned char)(fallback.b / 2u), 150 }
                           : fallback;
            DrawRectangle(PX + 8, y + (ROW_H - ICON) / 2, ICON, ICON, fb);
            DrawRectangleLinesEx({ (float)(PX + 8), (float)(y + (ROW_H - ICON) / 2),
                                   (float)ICON, (float)ICON }, 1, (Color){ 0, 0, 0, 80 });
        }

        Color text_col = dim ? (Color){ 140, 140, 140, 255 } : RAYWHITE;
        DrawText(TextFormat("%s: %d", label, count), PX + 40, y + (ROW_H - 20) / 2, 20, text_col);
        y += ROW_H;
    };

    DrawText("Finished", PX + 8, y, 15, (Color){ 160, 210, 160, 255 }); y += 18;
    row("Tentacle", g_inventory.cooked_tentacles, icons.tentacle,   (Color){ 180,  80, 120, 255 });
    row("Fries",    g_inventory.cooked_fries,     icons.fries,      (Color){ 220, 160,  60, 255 });
    row("Buns",     g_inventory.buns,             icons.bun,        (Color){ 210, 180, 130, 255 });
    row("E. Boba",  g_inventory.eyeball_boba,     icons.boba,       (Color){  80, 200,  80, 255 });
    row("M. Drink", g_inventory.mystery_drink,    icons.milkshake,  (Color){ 160,  80, 220, 255 });

    y += 4;
    DrawText("Raw Cut", PX + 8, y, 15, (Color){ 160, 160, 200, 255 }); y += 18;
    row("Cut Tent.", g_inventory.raw_cut_tentacles, icons.cut_tentacle, (Color){ 180,  80, 120, 255 }, true);
    row("Cut Fries", g_inventory.raw_cut_fries,     icons.cut_fries, (Color){ 220, 160,  60, 255 }, true);

}