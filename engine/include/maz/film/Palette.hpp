#pragma once

#include "maz/render/Renderer.hpp" // render::Color

#include <cmath>
#include <string>

// maz::film palette — where a film's look comes from, ported from film/js/film-art.js.
//
// A genre picks four base colours; the hour of day multiplies them (day is bright AND low-contrast,
// night is a hard light in the dark -- separate knobs on purpose); the scene's tension pulls the
// shadows. Out of that come five tones that always separate: a bright KEY, a SKY between, a DEEP
// mid-ground, a darker structure INK, and a near-black SHADOW, so a night scene stays dark without
// collapsing into one flat black.
//
// This has to give the SAME numbers as the browser, exactly. A native render with a palette half a
// shade off is a different film, and the difference would be invisible until someone put the two side
// by side. So the arithmetic below is a transcription, not a reimplementation, down to the rounding:
// JavaScript's Math.round on a positive number is floor(x + 0.5), which is NOT what std::round or
// std::lround do at a tie, so roundJs is spelled out rather than assumed.
namespace maz::film {

// A colour as the films carry it: three 0..255 channels.
struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;

    // To the engine's linear 0..1 Color for drawing.
    render::Color toColor(float alpha = 1.0f) const {
        return render::Color{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                             static_cast<float>(b) / 255.0f, alpha};
    }
};

struct Palette {
    Rgb key;     // the light itself
    Rgb accent;  // the one saturated colour the genre is allowed
    Rgb sky;     // behind everything
    Rgb deep;    // the mid ground
    Rgb ink;     // structure and silhouettes
    Rgb shadow;  // near-black
    // Double, not float, and deliberately: JavaScript numbers are doubles, and 1.05 as a float is
    // 1.04999995, which is enough to round 150*1.05 down to 157 where the browser gets 158. Keeping
    // the whole chain in double is what makes the two agree.
    double lift = 0.0;    // how much of the key reaches the shadows
    double tension = 0.0; // the scene's mood, clamped 0..1
};

namespace detail {

struct GenreColour {
    const char* name;
    Rgb key;
    Rgb shadow;
    Rgb accent;
    Rgb sky;
};

// Ten genres. Identical to GENRE_COLOUR in film/js/film-art.js.
inline constexpr GenreColour kGenreColours[] = {
    {"drama",    {255, 214, 160}, {26, 22, 34}, {232, 176, 106}, {58, 48, 72}},
    {"thriller", {150, 224, 255}, {10, 14, 26}, {255, 96, 84},   {22, 32, 54}},
    {"horror",   {140, 255, 190}, {6, 10, 10},  {180, 32, 48},   {12, 22, 20}},
    {"comedy",   {255, 232, 150}, {46, 32, 58}, {255, 122, 190}, {104, 168, 220}},
    {"romance",  {255, 190, 200}, {38, 20, 44}, {255, 148, 96},  {96, 52, 92}},
    {"scifi",    {150, 240, 255}, {10, 12, 30}, {180, 120, 255}, {20, 26, 62}},
    {"mystery",  {200, 226, 235}, {12, 18, 24}, {255, 176, 64},  {26, 38, 48}},
    {"fantasy",  {190, 235, 255}, {22, 14, 40}, {140, 255, 190}, {46, 30, 78}},
    {"heist",    {180, 214, 255}, {8, 12, 20},  {255, 64, 64},   {18, 26, 44}},
    {"western",  {255, 206, 128}, {40, 24, 18}, {214, 108, 48},  {186, 132, 78}},
};

struct HourLight {
    const char* name;
    double sky;
    double key;
    double lift;
    double warm;
};

// Four hours. Identical to HOUR in film/js/film-art.js.
inline constexpr HourLight kHours[] = {
    {"NIGHT", 0.30, 0.95, 0.06, 0.90},
    {"DAWN",  0.72, 1.00, 0.20, 1.05},
    {"DAY",   1.15, 1.15, 0.34, 1.00},
    {"DUSK",  0.62, 1.05, 0.16, 1.15},
};

// JavaScript's Math.round, for the positive values this file deals in: half goes UP, always.
// std::round and std::lround go half AWAY FROM ZERO, which agrees here but would not for negatives,
// and std::nearbyint goes to even, which would not agree at all. Spelled out so nobody has to wonder.
inline int roundJs(double x) { return static_cast<int>(std::floor(x + 0.5)); }

inline int clampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

inline Rgb scaleRgb(const Rgb& c, double f) {
    return Rgb{clampByte(roundJs(static_cast<double>(c.r) * f)),
               clampByte(roundJs(static_cast<double>(c.g) * f)),
               clampByte(roundJs(static_cast<double>(c.b) * f))};
}

inline Rgb mixRgb(const Rgb& a, const Rgb& b, double t) {
    return Rgb{roundJs(static_cast<double>(a.r) + (static_cast<double>(b.r) - a.r) * t),
               roundJs(static_cast<double>(a.g) + (static_cast<double>(b.g) - a.g) * t),
               roundJs(static_cast<double>(a.b) + (static_cast<double>(b.b) - a.b) * t)};
}

// Unknown names fall back to the first entry, exactly as the browser's `|| GENRE_COLOUR.drama` and
// `|| HOUR.NIGHT` do. drama and NIGHT are first in both tables for that reason.
inline const GenreColour& genreColour(const std::string& genre) {
    for (const GenreColour& g : kGenreColours) {
        if (genre == g.name) {
            return g;
        }
    }
    return kGenreColours[0];
}

inline const HourLight& hourLight(const std::string& time) {
    for (const HourLight& h : kHours) {
        if (time == h.name) {
            return h;
        }
    }
    return kHours[0];
}

} // namespace detail

// The palette for a genre, at an hour, at a mood. `mood` is clamped to 0..1.
inline Palette paletteFor(const std::string& genre, const std::string& time, double mood) {
    const detail::GenreColour& g = detail::genreColour(genre);
    const detail::HourLight& h = detail::hourLight(time);
    const double t = mood < 0.0 ? 0.0 : (mood > 1.0 ? 1.0 : mood);

    Palette p;
    p.key = detail::scaleRgb(g.key, h.key * h.warm);
    p.accent = g.accent;
    p.sky = detail::mixRgb(detail::scaleRgb(g.sky, 0.4 + h.sky * 0.7), p.key, 0.05 + h.lift * 0.25);
    p.deep = detail::mixRgb(g.shadow, p.key, 0.10 + h.lift * 0.35 - t * 0.04);
    p.ink = detail::mixRgb(g.shadow, p.key, 0.03 + h.lift * 0.08);
    p.shadow = detail::scaleRgb(g.shadow, 0.6);
    p.lift = h.lift;
    p.tension = t;
    return p;
}

} // namespace maz::film
