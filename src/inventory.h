#pragma once
#include "raylib.h"

struct Inventory {
    int raw_cut_tentacles = 0;
    int raw_cut_fries     = 0;
    int cooked_tentacles  = 0;
    int cooked_fries      = 0;
    int buns              = 0;
    int eyeball_boba      = 0;
    int mystery_drink     = 0;
    float total_score     = 0.0f;
};

// Icon textures supplied by kitchen (loaded at full res, scaled at draw time).
struct InventoryIcons {
    Texture2D tentacle;      // psito_plokami.png  – cooked tentacle (finished product)
    Texture2D fries;         // fries.png          – cooked fries (finished product)
    Texture2D boba;          // boba_tea.png
    Texture2D milkshake;     // milkshake.png
    Texture2D bun;           // psomaki.png
    Texture2D cut_fries;     // potato_slice.png   – raw cut fries (intermediate)
    Texture2D cut_tentacle;  // plokami_slice.png  – raw cut tentacle (intermediate)
};

extern Inventory g_inventory;

void inventory_draw(const InventoryIcons& icons);

void score_deduct_notify(float amount);
void score_add_notify(float amount);
void score_notifs_update(float dt);
void score_notifs_draw();