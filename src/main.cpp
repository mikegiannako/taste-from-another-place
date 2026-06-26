#include "raylib.h"
#include "game_state.h"
#include "kitchen.h"
#include "cutting_minigame.h"
#include "drink_minigame.h"
#include "inventory.h"
#include "audio.h"
#include "title.h"

int main() {
    const int screen_width  = 1920;
    const int screen_height = 1080;

    InitWindow(screen_width, screen_height, "Out of Place - Alien Chef");
    SetTargetFPS(60);

    audio_init();
    // Music starts only after the start button is clicked.

    GameState state = GameState::Title;
    title_init();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        audio_update_music();

        switch (state) {
            case GameState::Title:
                if (title_update(dt)) {
                    title_shutdown();
                    audio_play_music(Bgm::Theme);
                    kitchen_init();
                    state = GameState::Kitchen;
                }
                break;
            case GameState::Kitchen:
                kitchen_update(dt);
                switch (kitchen_consume_request()) {
                    case KitchenRequest::LaunchCutting:
                        cutting_minigame_init();
                        state = GameState::CuttingMinigame;
                        break;
                    case KitchenRequest::LaunchDrink:
                        drink_minigame_init();
                        state = GameState::DrinkMinigame;
                        break;
                    case KitchenRequest::None:
                        break;
                }
                break;
            case GameState::CuttingMinigame:
                kitchen_timers_tick(dt);
                if (cutting_minigame_update(dt)) {
                    kitchen_set_last_minigame_score(cutting_minigame_score());
                    state = GameState::Kitchen;
                }
                break;
            case GameState::DrinkMinigame:
                kitchen_timers_tick(dt);
                if (drink_minigame_update(dt)) {
                    float       dscore = drink_minigame_score();
                    DrinkResult dr     = drink_minigame_result();
                    kitchen_set_last_minigame_score(dscore);
                    if (dscore < 0.5f) {
                        audio_play(Sfx::SliceBoot);
                    } else {
                        int  qty     = (dscore >= 1.0f) ? 2 : 1;
                        bool is_boba = (dr == DrinkResult::EyeballBoba);
                        if (is_boba) g_inventory.eyeball_boba  += qty;
                        else         g_inventory.mystery_drink += qty;
                        kitchen_fly_drink_result(is_boba, qty);
                    }
                    state = GameState::Kitchen;
                }
                break;
            case GameState::GameOver:
                break;
        }

        BeginDrawing();
        ClearBackground((Color){30, 30, 40, 255});

        switch (state) {
            case GameState::Title:           title_draw(); break;
            case GameState::Kitchen:         kitchen_draw(); break;
            case GameState::CuttingMinigame: cutting_minigame_draw(); break;
            case GameState::DrinkMinigame:   drink_minigame_draw(); break;
            case GameState::GameOver:        DrawText("Game Over",      40, 40, 48, RAYWHITE); break;
        }

        EndDrawing();
    }

    kitchen_shutdown();
    audio_shutdown();
    CloseWindow();
    return 0;
}
