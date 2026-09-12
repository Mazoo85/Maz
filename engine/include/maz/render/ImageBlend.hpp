#pragma once

#include "maz/render/ColorOps.hpp" // Color
#include "maz/render/Image.hpp"     // Image

// maz::render IMAGE BLEND / COMPOSITE — layer one image over another with the Photoshop-style blend modes. This is
// what lets the engine's procedural textures be *combined*: multiply a `cellularTexture` stone pattern under a
// `gradientMap` colour to tint it, screen a noise "grunge" layer over a base to weather it, add a glow sprite,
// overlay detail, or difference two fields for edges. The `top` image is composited over `base` (treated as an
// opaque backdrop): for each pixel the two colours are combined by the chosen mode, then mixed toward the base by
// the top pixel's alpha times `opacity`. The result is `base`'s size; where `top` is smaller (or a pixel is fully
// transparent) the base shows through unchanged. Header-only, deterministic, headless — pure pixel math.
//
// Scope note (honest): channels are blended in raw 8-bit space with no gamma/linear conversion (matching Godot's
// non-linear Image ops and most 2D paint tools); the base is treated as opaque and its alpha is preserved; `top`
// is aligned to the base's top-left origin (no scaling — resize first if you need a fit). An empty base yields an
// empty image.
namespace maz::render {

// Photoshop-style layer blend modes; `Normal` is a plain alpha-over.
enum class ImageBlendMode {
    Normal,     // top replaces base (by alpha)
    Multiply,   // base * top — darkens; white top is a no-op
    Screen,     // inverse-multiply — brightens; black top is a no-op
    Add,        // base + top, clamped — linear dodge / glow
    Subtract,   // base - top, clamped
    Darken,     // min(base, top)
    Lighten,    // max(base, top)
    Difference, // |base - top| — edges / compare
    Overlay     // multiply the darks, screen the lights (contrast)
};

namespace detail {
inline float clampUnit(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Combine one channel: b = backdrop, s = source, both in [0,1].
inline float blendChannel(ImageBlendMode mode, float b, float s) {
    switch (mode) {
        case ImageBlendMode::Normal:     return s;
        case ImageBlendMode::Multiply:   return b * s;
        case ImageBlendMode::Screen:     return 1.0f - (1.0f - b) * (1.0f - s);
        case ImageBlendMode::Add:        return clampUnit(b + s);
        case ImageBlendMode::Subtract:   return clampUnit(b - s);
        case ImageBlendMode::Darken:     return b < s ? b : s;
        case ImageBlendMode::Lighten:    return b > s ? b : s;
        case ImageBlendMode::Difference: return b > s ? b - s : s - b;
        case ImageBlendMode::Overlay:    return b < 0.5f ? 2.0f * b * s : 1.0f - 2.0f * (1.0f - b) * (1.0f - s);
    }
    return s;
}
} // namespace detail

// Composite `top` over `base` with `mode`. `opacity` (clamped [0,1]) scales the whole top layer's contribution.
inline Image blend(const Image& base, const Image& top, ImageBlendMode mode, float opacity = 1.0f) {
    Image out = base;
    if (out.empty()) return out;
    const float op = detail::clampUnit(opacity);
    for (int y = 0; y < base.height(); ++y) {
        for (int x = 0; x < base.width(); ++x) {
            if (!top.inBounds(x, y)) continue; // outside the top layer -> base shows through unchanged
            const Color t = top.getPixel(x, y);
            const float sa = detail::clampUnit(t.a) * op;
            if (sa <= 0.0f) continue;
            const Color b = base.getPixel(x, y);
            const float br = detail::blendChannel(mode, b.r, t.r);
            const float bg = detail::blendChannel(mode, b.g, t.g);
            const float bb = detail::blendChannel(mode, b.b, t.b);
            out.setPixel(x, y, Color{b.r + (br - b.r) * sa, b.g + (bg - b.g) * sa, b.b + (bb - b.b) * sa, b.a});
        }
    }
    return out;
}

} // namespace maz::render
