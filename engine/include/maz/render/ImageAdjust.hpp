#pragma once

#include "maz/render/ColorOps.hpp" // Color, luminance
#include "maz/render/Image.hpp"     // Image

#include <algorithm> // std::clamp
#include <cmath>     // std::pow

// maz::render IMAGE ADJUSTMENTS — the "levels / adjustments" panel for CPU images: brightness, contrast, gamma,
// invert, greyscale, and threshold. These are the tone-and-value operations every texture pipeline needs, and they
// compose with the procedural generators (noise, cellular, gradients) and `blend`: brighten or add contrast to a
// noise height map before baking a normal map, gamma-correct a gradient, desaturate a colour texture, or threshold
// a field into a crisp black/white mask (for stencils, decals, or a `blend` alpha). Each returns a NEW image the
// same size as the source; alpha is preserved throughout. Header-only, deterministic, headless — pure pixel math.
//
// Scope note (honest): all maths is in the raw 0..1 channel space with no sRGB/linear conversion (matching Godot's
// non-linear Image ops); results are clamped to [0,1]. An empty input yields an empty image.
namespace maz::render {

// Apply a per-channel function to R, G, B (alpha preserved). The building block for the tone operations below.
template <typename Fn>
inline Image mapRGB(const Image& src, Fn&& fn) {
    Image out(src.width(), src.height());
    if (out.empty()) return out;
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x) {
            const Color c = src.getPixel(x, y);
            out.setPixel(x, y, Color{std::clamp(fn(c.r), 0.0f, 1.0f), std::clamp(fn(c.g), 0.0f, 1.0f),
                                     std::clamp(fn(c.b), 0.0f, 1.0f), c.a});
        }
    return out;
}

// Add `delta` to every channel (positive = brighter, negative = darker), clamped.
inline Image adjustBrightness(const Image& src, float delta) {
    return mapRGB(src, [delta](float v) { return v + delta; });
}

// Scale contrast about the mid-grey pivot 0.5: `factor` 1 = unchanged, >1 harder contrast, 0 = flat grey.
inline Image adjustContrast(const Image& src, float factor) {
    return mapRGB(src, [factor](float v) { return (v - 0.5f) * factor + 0.5f; });
}

// Power-law gamma per channel: out = v^gamma. `gamma` > 1 darkens the mid-tones, < 1 brightens them (1 = identity).
inline Image adjustGamma(const Image& src, float gamma) {
    const float g = gamma < 1e-6f ? 1e-6f : gamma;
    return mapRGB(src, [g](float v) { return std::pow(v < 0.0f ? 0.0f : v, g); });
}

// Invert RGB (1 - channel), alpha preserved — Godot Image negative / Color.inverted per pixel.
inline Image invert(const Image& src) {
    return mapRGB(src, [](float v) { return 1.0f - v; });
}

// Desaturate to greyscale using Rec.709 perceptual luminance; each pixel becomes (L, L, L, a).
inline Image grayscale(const Image& src) {
    Image out(src.width(), src.height());
    if (out.empty()) return out;
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x) {
            const Color c = src.getPixel(x, y);
            const float l = std::clamp(luminance(c), 0.0f, 1.0f);
            out.setPixel(x, y, Color{l, l, l, c.a});
        }
    return out;
}

// Binary threshold on luminance: pixels with luminance >= `t` become `high` (default white), else `low` (default
// black); alpha preserved. Turns a smooth field into a crisp mask (stencils, decals, `blend` alphas).
inline Image threshold(const Image& src, float t, const Color& high = Color{1, 1, 1, 1},
                       const Color& low = Color{0, 0, 0, 1}) {
    Image out(src.width(), src.height());
    if (out.empty()) return out;
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x) {
            const Color c = src.getPixel(x, y);
            const Color pick = luminance(c) >= t ? high : low;
            out.setPixel(x, y, Color{pick.r, pick.g, pick.b, c.a});
        }
    return out;
}

} // namespace maz::render
