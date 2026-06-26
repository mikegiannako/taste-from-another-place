#pragma once

enum class Sfx {
    SliceTentacle,
    SlicePotato,
    SliceBoot,
    Slice,
    StepLeft,
    StepRight,
    BubblePop1,
    BubblePop2,
    EyePop,
    DrinkComplete,
    Count,
};

enum class Bgm {
    Theme,
    Loop,
    Count,
};

void audio_init();
void audio_shutdown();
void audio_play(Sfx id);
void audio_play_sfx_volume(Sfx id, float volume);

void audio_play_music(Bgm id);
void audio_stop_music();
void audio_update_music();

void audio_fry_start();
void audio_fry_stop();
void audio_grill_start();
void audio_grill_stop();
