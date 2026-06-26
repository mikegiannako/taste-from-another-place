#include "fly_anim.h"
#include <math.h>

namespace {

constexpr int MAX_FLYERS = 32;

struct Flyer {
    bool      alive;
    Vector2   p0, p1, p2;   // quadratic bezier: start, control (lifted), end
    float     t;
    float     duration;
    Texture2D tex;
};

Flyer flyers[MAX_FLYERS];

} // namespace

void fly_anim_spawn(Vector2 from, Vector2 to, Texture2D tex, float duration) {
    for (int i = 0; i < MAX_FLYERS; i++) {
        if (!flyers[i].alive) {
            // Control point: midpoint lifted upward
            Vector2 mid = {
                (from.x + to.x) * 0.5f,
                (from.y + to.y) * 0.5f - 300.0f
            };
            flyers[i] = { true, from, mid, to, 0.0f, duration, tex };
            return;
        }
    }
}

void fly_anim_update(float dt) {
    for (int i = 0; i < MAX_FLYERS; i++) {
        if (!flyers[i].alive) continue;
        flyers[i].t += dt / flyers[i].duration;
        if (flyers[i].t >= 1.0f) flyers[i].alive = false;
    }
}

void fly_anim_draw() {
    for (int i = 0; i < MAX_FLYERS; i++) {
        if (!flyers[i].alive) continue;

        float t  = flyers[i].t < 1.0f ? flyers[i].t : 1.0f;
        float it = 1.0f - t;

        // Quadratic bezier position
        Vector2 pos = {
            it*it*flyers[i].p0.x + 2.0f*it*t*flyers[i].p1.x + t*t*flyers[i].p2.x,
            it*it*flyers[i].p0.y + 2.0f*it*t*flyers[i].p1.y + t*t*flyers[i].p2.y,
        };

        // Scale: full size in mid-arc, shrinks at destination
        float scale = 1.0f - t * 0.55f;
        float size  = 120.0f * scale;

        // Alpha: fade out in last 20%
        unsigned char alpha = (t > 0.8f)
            ? (unsigned char)((1.0f - (t - 0.8f) / 0.2f) * 255.0f)
            : 255;

        if (flyers[i].tex.id != 0) {
            float sx = size / (float)flyers[i].tex.width;
            float sy = size / (float)flyers[i].tex.height;
            float s  = sx < sy ? sx : sy;
            float dw = flyers[i].tex.width  * s;
            float dh = flyers[i].tex.height * s;
            Rectangle src    = { 0, 0, (float)flyers[i].tex.width, (float)flyers[i].tex.height };
            Rectangle dst    = { pos.x, pos.y, dw, dh };
            Vector2   origin = { dw * 0.5f, dh * 0.5f };
            DrawTexturePro(flyers[i].tex, src, dst, origin, 0.0f,
                           (Color){ 255, 255, 255, alpha });
        } else {
            DrawCircleV(pos, size * 0.5f, (Color){ 255, 210, 80, alpha });
        }
    }
}

void fly_anim_clear() {
    for (int i = 0; i < MAX_FLYERS; i++) flyers[i].alive = false;
}
