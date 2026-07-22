#pragma once

#include "maz/render/ColorOps.hpp" // Color
#include "maz/render/Image.hpp"     // Image

#include <cmath>

// maz::render PROCEDURAL IMAGE PATTERNS — generate common textures in code, no art files needed. A checkerboard
// for a placeholder / "missing texture" material, UV-check pattern, or floor tiles; a smooth top-to-bottom gradient
// for skies, backdrops, UI panels, and fades; a radial glow for spotlights, vignettes, soft particle sprites, and
// button highlights. Each returns a CPU `Image` (RGBA8) ready to hand to `Renderer::createTexture` or to save with
// the image codecs. Great for prototyping before real art exists, for runtime-generated UI, and for test cards.
// Header-only, deterministic, headless — pure pixel math.
//
// Scope note (honest): these are basic building-block patterns (checker, linear gradient, radial gradient). Colours
// are plain RGBA with no gamma handling — the values are written straight to 8-bit channels. A non-positive width
// or height yields an empty image; a checker cell size below 1 is treated as 1.
namespace maz::render::patterns {

// A checkerboard of `cell`-sized squares alternating `a` and `b`, top-left origin.
inline Image checkerboard(int width, int height, int cell, const Color& a, const Color& b) {
    Image img(width, height);
    if (img.empty()) return img;
    const int c = cell < 1 ? 1 : cell;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool odd = ((x / c) + (y / c)) % 2 != 0;
            img.setPixel(x, y, odd ? b : a);
        }
    }
    return img;
}

// A smooth vertical gradient from `top` (row 0) to `bottom` (last row).
inline Image verticalGradient(int width, int height, const Color& top, const Color& bottom) {
    Image img(width, height);
    if (img.empty()) return img;
    for (int y = 0; y < height; ++y) {
        const float t = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
        const Color c{top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t,
                      top.b + (bottom.b - top.b) * t, top.a + (bottom.a - top.a) * t};
        for (int x = 0; x < width; ++x) img.setPixel(x, y, c);
    }
    return img;
}

// A radial gradient from `centre` at the middle to `edge` at radius = half the shorter side (clamped beyond).
inline Image radialGradient(int width, int height, const Color& centre, const Color& edge) {
    Image img(width, height);
    if (img.empty()) return img;
    const float cx = static_cast<float>(width) * 0.5f, cy = static_cast<float>(height) * 0.5f;
    const float maxR = 0.5f * static_cast<float>(width < height ? width : height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - cx, dy = (static_cast<float>(y) + 0.5f) - cy;
            float t = maxR > 1e-6f ? std::sqrt(dx * dx + dy * dy) / maxR : 1.0f;
            if (t > 1.0f) t = 1.0f;
            const Color c{centre.r + (edge.r - centre.r) * t, centre.g + (edge.g - centre.g) * t,
                          centre.b + (edge.b - centre.b) * t, centre.a + (edge.a - centre.a) * t};
            img.setPixel(x, y, c);
        }
    }
    return img;
}

} // namespace maz::render::patterns
