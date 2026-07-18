#pragma once

#include "maz/math/Math.hpp"        // math::vec2
#include "maz/render/ColorOps.hpp"  // render::fromHsv
#include "maz/render/Renderer.hpp"  // render::Color

#include <algorithm>
#include <cmath>
#include <cstdint>

// maz::ui RichTextLabel effects — the per-glyph animation maths behind Godot's RichTextLabel BBCode
// effects ([wave], [tornado], [shake], [fade], [rainbow], [pulse]). The BBCode parser (M141) tags
// which characters an effect covers; this computes, for a given character index and time, the offset
// to nudge that glyph by and/or the colour to modulate it with. Each effect is a pure, deterministic
// function of (index, time, params) so the widget layer just calls it per visible glyph per frame —
// and it unit-tests exactly. No engine state, header-only.
namespace maz::ui {

// What an effect does to one glyph this frame: a positional offset plus a colour multiplier (its
// alpha carries fade/visibility). A glyph with no active effect uses the identity {(0,0), white}.
struct CharFx {
    math::vec2 offset{0.0f, 0.0f};
    render::Color color{1.0f, 1.0f, 1.0f, 1.0f};
};

namespace detail {
// A cheap deterministic hash -> float in [0,1). Used by shake so each glyph jitters independently but
// reproducibly (no global RNG; same inputs always give the same output).
inline float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x) / 4294967296.0f;
}
inline float hash01(std::uint32_t a, std::uint32_t b) {
    return hash01(a * 73856093U ^ b * 19349663U);
}
} // namespace detail

// [wave]: glyphs ride a vertical sine wave; each glyph is phase-shifted by its index so the row
// undulates. `amp` in px, `freq` in cycles, `t` in seconds.
inline CharFx rtWave(int index, float t, float amp = 10.0f, float freq = 5.0f) {
    CharFx fx;
    const float phase = static_cast<float>(index) * 0.5f + t * freq;
    fx.offset.y = std::sin(phase) * amp;
    return fx;
}

// [tornado]: glyphs orbit their home position in a small circle (x and y a quarter-cycle apart).
inline CharFx rtTornado(int index, float t, float radius = 10.0f, float freq = 2.0f) {
    CharFx fx;
    const float phase = static_cast<float>(index) * 0.5f + t * freq;
    fx.offset.x = std::cos(phase) * radius;
    fx.offset.y = std::sin(phase) * radius;
    return fx;
}

// [shake]: random per-glyph jitter that resteps `rate` times per second. Deterministic: the offset is
// a hash of (index, time-step), so it is bounded by `level` and reproducible for a given time.
inline CharFx rtShake(int index, float t, float rate = 20.0f, float level = 5.0f) {
    CharFx fx;
    const std::uint32_t step = static_cast<std::uint32_t>(std::floor(t * rate));
    const std::uint32_t idx = static_cast<std::uint32_t>(index);
    fx.offset.x = (detail::hash01(idx, step) * 2.0f - 1.0f) * level;
    fx.offset.y = (detail::hash01(idx + 1u, step + 977u) * 2.0f - 1.0f) * level;
    return fx;
}

// [rainbow]: cycle the glyph colour through hue over time, offset per glyph so a gradient runs along
// the text. `freq` cycles/sec, `sat`/`val` in [0,1].
inline render::Color rtRainbow(int index, float t, float freq = 1.0f, float sat = 1.0f,
                               float val = 1.0f) {
    float hue = t * freq + static_cast<float>(index) * 0.1f;
    hue -= std::floor(hue); // wrap into [0,1)
    return render::fromHsv(hue, sat, val, 1.0f);
}

// [fade]: glyphs from `start` fade to invisible across `length` characters. Before `start` -> fully
// visible (alpha 1); from start over length -> linear ramp to 0; past that -> invisible.
inline float rtFadeAlpha(int index, int start, int length) {
    if (length <= 0) {
        return index < start ? 1.0f : 0.0f;
    }
    if (index < start) {
        return 1.0f;
    }
    const float f = 1.0f - static_cast<float>(index - start) / static_cast<float>(length);
    return std::clamp(f, 0.0f, 1.0f);
}

// [pulse]: the glyph's alpha (and thus brightness against the background) breathes between `base`'s
// alpha and `base.a * (1 - ease)`. `freq` cycles/sec, `ease` in [0,1] is the depth of the dip.
inline render::Color rtPulse(int index, float t, const render::Color& base, float freq = 1.0f,
                             float ease = 0.5f) {
    const float phase = t * freq + static_cast<float>(index) * 0.1f;
    const float s = (std::sin(phase * 6.2831853f) + 1.0f) * 0.5f; // [0,1]
    const float k = 1.0f - ease * (1.0f - s);                     // in [1-ease, 1]
    render::Color c = base;
    c.a = base.a * k;
    return c;
}

} // namespace maz::ui
