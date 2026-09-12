#pragma once

#include "maz/film/Canvas.hpp"
#include "maz/film/Noise.hpp"
#include "maz/film/Palette.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// maz::film air — everything between the audience and the picture, ported from film/js/film-weather.js
// and the film-stock passes at the end of film/js/film-player.js's drawFrame.
//
// Four things, all drawn in SCREEN space, after the planes and after the camera roll has been undone:
//
//   WEATHER    rain, dust, fog, haze, shimmer or embers, chosen by genre, hour and whether the scene
//              is outdoors. Deliberately does not move with the camera: it reads as air in front of
//              the lens, not as part of the set, so a pan leaving it alone is the point.
//   LIGHT LEAK the key colour spilling diagonally across the frame. A set that owns a moving light
//              folds its brightness into this wash's alpha and its offset into where the wash starts,
//              rather than getting a draw call of its own.
//   VIGNETTE   to hold the eye in the middle. Deeper the more wound-up the scene is.
//   GRAIN      a fixed 128-pixel tile of seeded noise, stepped twelve times a second rather than
//              sixty: it still crawls like film, and it stops every frame being different, which is
//              what makes a recorded file enormous. Plus a gate flicker on the same clock.
namespace maz::film {

enum class Weather { None, Rain, Dust, Fog, Haze, Shimmer, Embers };

namespace detail {

// The four sets that are actually outdoors. A rainstorm indoors is a mistake, not a mood.
inline bool weatherOutside(const std::string& set) {
    return set == "street" || set == "woods" || set == "field" || set == "water";
}

} // namespace detail

// What hangs in the air: genre first, then the hour, then whether we are outdoors.
inline Weather weatherFor(const std::string& genre, const std::string& time, const std::string& set) {
    const bool outside = detail::weatherOutside(set);
    const bool night = time == "NIGHT" || time == "DUSK";

    if (genre == "western" && !night) {
        return Weather::Shimmer;
    }
    if (genre == "fantasy") {
        return Weather::Embers;
    }
    if ((genre == "horror" || genre == "mystery") && outside) {
        return Weather::Fog;
    }
    if ((genre == "thriller" || genre == "horror") && night) {
        return outside ? Weather::Rain : Weather::Haze;
    }
    if (!night && !outside) {
        return Weather::Dust;
    }
    if (night) {
        return Weather::Haze;
    }
    return Weather::None;
}

inline const char* weatherName(Weather w) {
    switch (w) {
        case Weather::Rain: return "rain";
        case Weather::Dust: return "dust";
        case Weather::Fog: return "fog";
        case Weather::Haze: return "haze";
        case Weather::Shimmer: return "shimmer";
        case Weather::Embers: return "embers";
        case Weather::None:
        default: return "none";
    }
}

namespace detail {

// Particles scale with the frame, but by its square root: a bigger frame gets more air, not
// proportionally more, because the budget is shared with everything else on screen. Capped at the
// size of the noise pool, past which two "different" particles land on identical coordinates --
// overdraw with no extra air.
inline int particleCount(double base, float w, std::size_t poolLen) {
    const int n = static_cast<int>(std::lround(base * std::sqrt(static_cast<double>(w) / 960.0)));
    return n < static_cast<int>(poolLen) ? n : static_cast<int>(poolLen);
}

// JavaScript's % on positive operands, which is all this file uses.
inline double wrap1(double v) { return std::fmod(v, 1.0); }

} // namespace detail

// The weather, into a canvas whose transform puts (0,0) at the top-left of the letterboxed frame and
// whose clip is that frame. `w` and `h` are the frame's own size.
inline void drawWeather(Canvas& c, Weather kind, const Palette& p, double time, std::uint32_t seed,
                        float w, float h) {
    if (kind == Weather::None) {
        return;
    }
    const auto n = noise(std::string("weather-") + weatherName(kind) + "-" + std::to_string(seed), 120);
    const std::size_t pool = n.size();
    const auto& K = p.key;

    if (kind == Weather::Rain) {
        c.setStroke(K, 0.32f);
        c.setLineWidth(std::fmax(1.0f, w / 700.0f));
        const int total = detail::particleCount(90.0, w, pool);
        for (int i = 0; i < total; ++i) {
            const NoiseTriple& s = n[static_cast<std::size_t>(i) % pool];
            const float x = static_cast<float>(detail::wrap1(s[0] + time * 0.06) * w);
            const float y = static_cast<float>(detail::wrap1(s[1] + time * 0.9) * h);
            c.beginPath();
            c.moveTo(x, y);
            c.lineTo(x - w * 0.006f, y + h * 0.045f);
            c.stroke();
        }
        return;
    }
    if (kind == Weather::Dust) {
        const float size = std::fmax(2.0f, w / 380.0f);
        const int total = detail::particleCount(60.0, w, pool);
        for (int i = 0; i < total; ++i) {
            const NoiseTriple& q = n[static_cast<std::size_t>(i) % pool];
            const float dx = static_cast<float>(detail::wrap1(q[0] + time * 0.01) * w);
            const float dy = static_cast<float>(
                detail::wrap1(q[1] + std::sin(time * 0.3 + q[2] * 6.0) * 0.02 + time * 0.006) * h);
            c.setFill(K, static_cast<float>(0.16 + q[2] * 0.34));
            c.fillRect(dx, dy, size, size);
        }
        return;
    }
    if (kind == Weather::Fog || kind == Weather::Haze) {
        const int bands = kind == Weather::Fog ? 5 : 2;
        for (int b = 0; b < bands; ++b) {
            const NoiseTriple& r = n[static_cast<std::size_t>(b)];
            const float by = static_cast<float>(h * (0.25 + r[0] * 0.6) +
                                                std::sin(time * 0.12 + b) * h * 0.02);
            Gradient g = Canvas::linearGradient(0.0f, by - h * 0.12f, 0.0f, by + h * 0.12f);
            g.addStop(0.0f, K.toColor(0.0f));
            g.addStop(0.5f, K.toColor(kind == Weather::Fog ? 0.22f : 0.16f));
            g.addStop(1.0f, K.toColor(0.0f));
            c.setFillGradient(g);
            c.fillRect(0.0f, by - h * 0.12f, w, h * 0.24f);
        }
        return;
    }
    if (kind == Weather::Shimmer) {
        for (int m = 0; m < 3; ++m) {
            const NoiseTriple& sh = n[static_cast<std::size_t>(m + 7)];
            const float sy = static_cast<float>(h * (0.55 + sh[0] * 0.3));
            c.setFill(K, 0.14f);
            c.fillRect(0.0f, sy + static_cast<float>(std::sin(time * 2.0 + m) * h * 0.006), w,
                       h * 0.02f);
        }
        return;
    }
    // embers
    const float size = std::fmax(2.0f, w / 340.0f);
    const int total = detail::particleCount(40.0, w, pool);
    for (int i = 0; i < total; ++i) {
        const NoiseTriple& v = n[static_cast<std::size_t>(i) % pool];
        const float ex = static_cast<float>(
            detail::wrap1(v[0] + std::sin(time * 0.4 + v[2] * 9.0) * 0.03) * w);
        const float ey = static_cast<float>(h - detail::wrap1(v[1] + time * 0.05) * h);
        c.setFill(p.accent, static_cast<float>(0.35 + v[2] * 0.5));
        c.fillRect(ex, ey, size, size);
    }
}

// The key colour spilling diagonally across the frame. `lightOffset` and `lightBrightness` come from
// the set's own light behaviour, so on the five sets that own a moving light this changes every frame
// and cannot be cached.
inline void drawLightLeak(Canvas& c, const Palette& p, double lightOffset, double lightBrightness,
                          float w, float h) {
    const float leakX = static_cast<float>(lightOffset) * w * 0.3f;
    Gradient leak = Canvas::linearGradient(leakX, 0.0f, leakX + w * 0.7f, h);
    leak.addStop(0.0f, p.key.toColor(static_cast<float>((0.10 + p.tension * 0.05) * lightBrightness)));
    leak.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(leak);
    c.fillRect(0.0f, 0.0f, w, h);
}

// The vignette, to hold the eye in the middle. Deeper the more wound-up the scene is.
//
// Its shape depends on nothing but the frame's size and the scene's tension, and tension is fixed for
// a whole shot -- so it is built once as an alpha mask and reused, rather than evaluating a radial
// gradient (and a square root) for a million pixels every frame. That was the single most expensive
// thing in a frame once the sets were fast.
inline void drawVignette(render::Image& img, const Palette& p, int top, int bottom) {
    const int w = img.width();
    const int rows = std::max(0, std::min(img.height(), bottom) - std::max(0, top));
    if (w <= 0 || rows <= 0) {
        return;
    }
    const int first = std::max(0, top);
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(bottom - top);

    static int haveW = 0, haveRows = 0, haveFirst = 0;
    static float haveTension = -1.0f;
    static std::vector<std::uint8_t> mask;
    const float tension = static_cast<float>(p.tension);
    if (haveW != w || haveRows != rows || haveFirst != first || haveTension != tension) {
        haveW = w;
        haveRows = rows;
        haveFirst = first;
        haveTension = tension;
        mask.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(rows), 0u);
        const float cx = fw * 0.5f;
        const float cy = static_cast<float>(top) + fh * 0.5f;
        const float r0 = fh * 0.28f;
        const float r1 = fh * 0.95f;
        const float peak = static_cast<float>(0.55 + p.tension * 0.2) * 255.0f;
        for (int y = 0; y < rows; ++y) {
            const float dy = static_cast<float>(first + y) - cy;
            for (int x = 0; x < w; ++x) {
                const float dx = static_cast<float>(x) - cx;
                const float d = std::sqrt(dx * dx + dy * dy);
                float t = r1 > r0 ? (d - r0) / (r1 - r0) : 0.0f;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                mask[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                     static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(t * peak + 0.5f);
            }
        }
    }
    for (int y = 0; y < rows; ++y) {
        img.blendSpanMasked(0, w, first + y, render::Color{0.0f, 0.0f, 0.0f, 1.0f},
                            mask.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w));
    }
}

// The grain tile: 128 pixels square, seeded rather than random. Randomising it quietly broke the
// promise that one film comes out the same every time -- two recordings differed in every pixel.
inline const std::vector<std::uint8_t>& grainTile() {
    static const std::vector<std::uint8_t> kTile = [] {
        std::vector<std::uint8_t> t(128u * 128u);
        Rng rng(hashText("film-grain"));
        for (std::size_t i = 0; i < t.size(); ++i) {
            t[i] = static_cast<std::uint8_t>(110.0 + rng.next() * 90.0);
        }
        return t;
    }();
    return kTile;
}

inline constexpr int kGrainSize = 128;
// The tile's own alpha (26/255) times the pass's 0.3.
inline constexpr int kGrainAlpha = 8; // round(26 * 0.3)

// Grain over the frame, plus the gate flicker. `top` and `bottom` are the letterboxed rows.
inline void drawGrain(render::Image& img, double time, int top, int bottom) {
    const auto& tile = grainTile();
    // Twelve steps a second, not sixty.
    const long step = static_cast<long>(std::floor(time * 12.0));
    // The browser translates the pattern by a negative offset; C++ and JavaScript both keep the sign
    // of the dividend for %, so the same expression lands on the same phase.
    const int ox = static_cast<int>(-(step * 53) % kGrainSize);
    const int oy = static_cast<int>(-(step * 37) % kGrainSize);

    static std::vector<std::uint8_t> row;
    row.resize(static_cast<std::size_t>(img.width()));
    for (int y = std::max(0, top); y < std::min(img.height(), bottom); ++y) {
        int ty = (y - oy) % kGrainSize;
        if (ty < 0) {
            ty += kGrainSize;
        }
        const std::uint8_t* src = tile.data() + static_cast<std::size_t>(ty) * kGrainSize;
        for (int x = 0; x < img.width(); ++x) {
            int tx = (x - ox) % kGrainSize;
            if (tx < 0) {
                tx += kGrainSize;
            }
            row[static_cast<std::size_t>(x)] = src[static_cast<std::size_t>(tx)];
        }
        img.blendGraySpan(0, img.width(), y, row.data(), kGrainAlpha);
    }

    // Gate flicker, on the same clock.
    const float flicker =
        static_cast<float>(0.02 + 0.03 * std::fabs(std::sin(static_cast<double>(step) * 0.94)));
    for (int y = std::max(0, top); y < std::min(img.height(), bottom); ++y) {
        img.blendSpan(0, img.width(), y, render::Color{0.0f, 0.0f, 0.0f, 1.0f}, flicker);
    }
}

} // namespace maz::film
