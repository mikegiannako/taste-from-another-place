#pragma once

enum class DrinkResult { None, EyeballBoba, MysteryDrink };

void        drink_minigame_init();
bool        drink_minigame_update(float dt);
void        drink_minigame_draw();
float       drink_minigame_score();
DrinkResult drink_minigame_result();
