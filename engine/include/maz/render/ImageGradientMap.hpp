#pragma once

#include "maz/render/ColorOps.hpp" // Color
#include "maz/render/Image.hpp"     // Image

#include <vector>

// maz::render GRADIENT MAP (colorize) — recolour a greyscale image by running each pixel's brightness through a
// colour ramp. This is the natural colour stage for the procedural grey textures in this engine: feed a
// `patterns::noiseTexture` or `patterns::cellularTexture` (or any height field / mask) in, and get lava (black →
// red → yellow), terrain (deep → shallow → sand → grass → rock → snow), fire, marble tint, a heat-map, or a toon
// colour ramp out. Each input pixel's perceptual luminance (0.299 R + 0.587 G + 0.114 B, clamped to [0,1]) is the
// ramp parameter; the ramp's colour becomes the output pixel (RGB from the ramp, and — for the stop/two-colour
// forms — the ramp's alpha too). Three forms: a two-colour `lo → hi` ramp, a multi-stop ramp (linear between
// sorted stops, clamped past the ends, exactly like a colour gradient), and a fully general callback form that
// accepts any `Color(float t)` callable (e.g. `anim::Gradient::sample`). Header-only, deterministic, headless.
//
// Scope note (honest): luminance is computed from the raw 8-bit channel values with no gamma handling; a stop
// list is used as given (assumed sorted ascending by `t`); an empty stop list leaves the image unchanged in
// shape but paints it transparent black. A non-positive / empty input yields an empty image.
namespace maz::render {

// One colour stop for a `gradientMap` ramp: the colour reached at parameter `t` (usually within [0,1]).
struct ColorStop {
    float t = 0.0f;
    Color color{0.0f, 0.0f, 0.0f, 1.0f};
};

namespace detail {
inline float luminance01(const Color& c) {
    float l = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
    if (l < 0.0f) l = 0.0f;
    if (l > 1.0f) l = 1.0f;
    return l;
}
} // namespace detail

// General form: colorize `grey` by mapping each pixel's luminance through `ramp`, any callable `Color(float)`.
template <typename Ramp>
inline Image gradientMap(const Image& grey, Ramp&& ramp) {
    Image out(grey.width(), grey.height());
    if (out.empty()) return out;
    for (int y = 0; y < grey.height(); ++y)
        for (int x = 0; x < grey.width(); ++x)
            out.setPixel(x, y, ramp(detail::luminance01(grey.getPixel(x, y))));
    return out;
}

// Two-colour ramp: luminance 0 → `lo`, luminance 1 → `hi`, linear (all four channels) between.
inline Image gradientMap(const Image& grey, const Color& lo, const Color& hi) {
    return gradientMap(grey, [&](float t) {
        return Color{lo.r + (hi.r - lo.r) * t, lo.g + (hi.g - lo.g) * t,
                     lo.b + (hi.b - lo.b) * t, lo.a + (hi.a - lo.a) * t};
    });
}

// Multi-stop ramp: sample the sorted `stops` at each pixel's luminance — the first colour below the first stop,
// the last colour above the last stop, linear between neighbours (a colour gradient baked over an image).
inline Image gradientMap(const Image& grey, const std::vector<ColorStop>& stops) {
    return gradientMap(grey, [&](float t) -> Color {
        if (stops.empty()) return Color{0.0f, 0.0f, 0.0f, 0.0f};
        if (t <= stops.front().t) return stops.front().color;
        if (t >= stops.back().t) return stops.back().color;
        std::size_t i = 1;
        while (i < stops.size() && stops[i].t <= t) ++i;
        const ColorStop& a = stops[i - 1];
        const ColorStop& b = stops[i];
        const float span = b.t - a.t;
        const float f = span > 1e-9f ? (t - a.t) / span : 0.0f;
        return Color{a.color.r + (b.color.r - a.color.r) * f, a.color.g + (b.color.g - a.color.g) * f,
                     a.color.b + (b.color.b - a.color.b) * f, a.color.a + (b.color.a - a.color.a) * f};
    });
}

} // namespace maz::render
