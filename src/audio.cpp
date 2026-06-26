#include "audio.h"
#include "raylib.h"

namespace {

const char* sfx_path(Sfx id) {
    switch (id) {
        case Sfx::SliceTentacle: return "assets/sounds/slicing_tentacle.wav";
        case Sfx::SlicePotato:   return "assets/sounds/slicing_potato.wav";
        case Sfx::SliceBoot:     return "assets/sounds/slicing_boot.wav";
        case Sfx::Slice:        return "assets/sounds/slice.mp3";
        case Sfx::StepLeft:      return "assets/sounds/step_left2.wav";
        case Sfx::StepRight:     return "assets/sounds/step_right2.wav";
        case Sfx::BubblePop1:   return "assets/sounds/bubble_pop1.mp3";
        case Sfx::BubblePop2:   return "assets/sounds/bubble_pop2.mp3";
        case Sfx::EyePop:        return "assets/sounds/eye_pop.mp3";
        case Sfx::DrinkComplete: return "assets/sounds/drink_complete.mp3";
        case Sfx::Count:         return nullptr;
    }
    return nullptr;
}

const char* bgm_path(Bgm id) {
    switch (id) {
        case Bgm::Theme: return "assets/music/theme_start.mp3";
        case Bgm::Loop: return "assets/music/theme_loop.mp3";
        case Bgm::Count: return nullptr;
    }
    return nullptr;
}

Sound sounds[(int)Sfx::Count];
bool  sfx_loaded[(int)Sfx::Count];

Music bgm_streams[(int)Bgm::Count];
bool  bgm_loaded[(int)Bgm::Count];
int   current_bgm = -1;

Music fry_stream    = {};
bool  fry_loaded    = false;
bool  fry_playing   = false;

Music grill_stream  = {};
bool  grill_loaded  = false;
bool  grill_playing = false;

void load_ambient(Music& stream, bool& loaded, const char* path, float vol) {
    if (FileExists(path)) {
        stream         = LoadMusicStream(path);
        stream.looping = true;
        SetMusicVolume(stream, vol);
        loaded = true;
    } else {
        TraceLog(LOG_WARNING, "AUDIO: missing ambient %s", path);
    }
}

} // namespace

void audio_init() {
    InitAudioDevice();

    for (int i = 0; i < (int)Sfx::Count; i++) {
        const char* path = sfx_path((Sfx)i);
        if (!path) continue;
        if (FileExists(path)) {
            sounds[i]     = LoadSound(path);
            sfx_loaded[i] = true;
        } else {
            TraceLog(LOG_WARNING, "AUDIO: missing %s", path);
        }
    }
    if (sfx_loaded[(int)Sfx::StepLeft])  SetSoundVolume(sounds[(int)Sfx::StepLeft],  0.15f);
    if (sfx_loaded[(int)Sfx::StepRight]) SetSoundVolume(sounds[(int)Sfx::StepRight], 0.15f);

    load_ambient(fry_stream,   fry_loaded,   "assets/sounds/boil.mp3",         1.00f);
    load_ambient(grill_stream, grill_loaded, "assets/sounds/frying_loop.ogg",   0.25f);
}

void audio_shutdown() {
    for (int i = 0; i < (int)Sfx::Count; i++) {
        if (sfx_loaded[i]) {
            UnloadSound(sounds[i]);
            sfx_loaded[i] = false;
        }
    }
    for (int i = 0; i < (int)Bgm::Count; i++) {
        if (bgm_loaded[i]) {
            UnloadMusicStream(bgm_streams[i]);
            bgm_loaded[i] = false;
        }
    }
    current_bgm = -1;

    if (fry_loaded) {
        UnloadMusicStream(fry_stream);
        fry_stream  = {};
        fry_loaded  = false;
        fry_playing = false;
    }
    if (grill_loaded) {
        UnloadMusicStream(grill_stream);
        grill_stream  = {};
        grill_loaded  = false;
        grill_playing = false;
    }

    CloseAudioDevice();
}

void audio_play(Sfx id) {
    int i = (int)id;
    if (i < 0 || i >= (int)Sfx::Count) return;
    if (sfx_loaded[i]) PlaySound(sounds[i]);
}

void audio_play_sfx_volume(Sfx id, float volume) {
    int i = (int)id;
    if (i < 0 || i >= (int)Sfx::Count) return;
    if (!sfx_loaded[i]) return;
    SetSoundVolume(sounds[i], volume);
    PlaySound(sounds[i]);
}

void audio_play_music(Bgm id) {
    int i = (int)id;
    if (i < 0 || i >= (int)Bgm::Count) return;

    if (!bgm_loaded[i]) {
        const char* path = bgm_path(id);
        if (!path || !FileExists(path)) {
            TraceLog(LOG_WARNING, "AUDIO: missing music %s", path ? path : "(null)");
            return;
        }
        bgm_streams[i]         = LoadMusicStream(path);
        bgm_streams[i].looping = (id == Bgm::Loop);
        bgm_loaded[i]          = true;
    }

    if (current_bgm >= 0 && current_bgm != i && bgm_loaded[current_bgm]) {
        StopMusicStream(bgm_streams[current_bgm]);
    }
    current_bgm = i;
    PlayMusicStream(bgm_streams[i]);
    SetMusicVolume(bgm_streams[i], 0.17f);
}

void audio_stop_music() {
    if (current_bgm >= 0 && bgm_loaded[current_bgm]) {
        StopMusicStream(bgm_streams[current_bgm]);
    }
    current_bgm = -1;
}

void audio_fry_start() {
    if (!fry_loaded) return;
    if (!IsMusicStreamPlaying(fry_stream)) {
        PlayMusicStream(fry_stream);
        SetMusicVolume(fry_stream, 1.00f);
        UpdateMusicStream(fry_stream);
        fry_playing = true;
    }
}

void audio_fry_stop() {
    if (fry_loaded && IsMusicStreamPlaying(fry_stream)) {
        StopMusicStream(fry_stream);
    }
    fry_playing = false;
}

void audio_grill_start() {
    if (!grill_loaded) return;
    if (!IsMusicStreamPlaying(grill_stream)) {
        PlayMusicStream(grill_stream);
        SetMusicVolume(grill_stream, 0.15f);
        UpdateMusicStream(grill_stream);
        grill_playing = true;
    }
}

void audio_grill_stop() {
    if (grill_loaded && IsMusicStreamPlaying(grill_stream)) {
        StopMusicStream(grill_stream);
    }
    grill_playing = false;
}

void audio_update_music() {
    if (current_bgm >= 0 && bgm_loaded[current_bgm]) {
        Music currentStream = bgm_streams[current_bgm];
        UpdateMusicStream(currentStream);

        // Transition Logic: Theme -> Loop
        if ((Bgm)current_bgm == Bgm::Theme) {
            float timePlayed = GetMusicTimePlayed(currentStream);
            float timeTotal = GetMusicTimeLength(currentStream);

            // Using a small threshold (0.1s) or checking if timePlayed >= timeTotal
            // Note: Raylib sometimes loops automatically if .looping = true, 
            // so for an intro, set looping = false.
            if (timePlayed >= timeTotal - 0.01f) {
                audio_play_music(Bgm::Loop);
            }
        }
    }

    if (fry_playing && fry_loaded)
        UpdateMusicStream(fry_stream);
    if (grill_playing && grill_loaded)
        UpdateMusicStream(grill_stream);
}