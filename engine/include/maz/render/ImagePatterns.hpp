#pragma once

#include "maz/core/Noise.hpp"         // core::Noise (fbm)
#include "maz/core/WorleyNoise.hpp"   // core::WorleyNoise (cellular)
#include "maz/render/ColorOps.hpp" // Color
#include "maz/render/Image.hpp"     // Image

#include <cmath>
#include <cstdint>

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

// A seamless-ish grey NOISE texture from fbm (fractal Perlin): clouds, marble, dirt, smoke, static, or a height
// source for `heightToNormalMap`. `scale` is the world frequency (smaller = broader blobs), `seed` picks the
// pattern (same seed → same texture), `octaves` sets how much fine detail is layered in. Values map fbm's ~[-1,1]
// into grey [0,1].
inline Image noiseTexture(int width, int height, float scale = 0.08f, std::uint64_t seed = 0, int octaves = 4) {
    Image img(width, height);
    if (img.empty()) return img;
    const core::Noise n(seed);
    const int oct = octaves < 1 ? 1 : octaves;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float v = n.fbm2(static_cast<float>(x) * scale, static_cast<float>(y) * scale, oct); // ~[-1,1]
            v = v * 0.5f + 0.5f;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            img.setPixel(x, y, Color{v, v, v, 1.0f});
        }
    }
    return img;
}

// A running-bond BRICK WALL: rows of `brickW`×`brickH` bricks separated by `mortar`-thick lines, each row shifted
// by `offsetFrac` of a brick relative to the one above (0.5 = the classic half-brick stagger). `brick` fills the
// bricks, `mortar` fills the gaps. Great for building fronts, dungeon walls, and paths — pair with `gradientMap`
// to tint and `heightToNormalMap` (bricks proud, mortar recessed) for relief. Sizes below 1 are treated as 1.
inline Image brickWall(int width, int height, int brickW, int brickH, int mortarPx, const Color& brick,
                       const Color& mortar, float offsetFrac = 0.5f) {
    Image img(width, height);
    if (img.empty()) return img;
    const int bw = brickW < 1 ? 1 : brickW;
    const int bh = brickH < 1 ? 1 : brickH;
    const int m = mortarPx < 1 ? 1 : mortarPx;
    const int periodX = bw + m, periodY = bh + m;
    const int shiftPx = static_cast<int>(offsetFrac * static_cast<float>(periodX));
    for (int y = 0; y < height; ++y) {
        const int row = y / periodY;
        const int ly = y % periodY;
        const int shift = (row % 2 != 0) ? shiftPx : 0;
        for (int x = 0; x < width; ++x) {
            const int lx = (x + shift) % periodX;
            const bool isMortar = ly >= bh || lx >= bw;
            img.setPixel(x, y, isMortar ? mortar : brick);
        }
    }
    return img;
}

// Which cellular field a `cellularTexture` bakes.
enum class Cellular {
    Cells,  // F1 distance: dark feature-point centres brightening outward — stone, scales, cracked mud, caustics, bubbles.
    Cracks  // F2-F1: bright interiors with dark lines along the cell boundaries — crack / vein / Voronoi-edge networks.
};

// A grey CELLULAR ("Worley") texture. `mode` picks the look: `Cells` gives rounded blobs (one dark point per grid
// cell, brightening toward the edges) for stone, reptile scales, cracked mud, or water caustics; `Cracks` traces the
// thin boundaries between cells for crack, vein, or Voronoi-edge networks. `scale` is the world frequency (larger =
// more, smaller cells), `seed` picks the pattern (same seed → same texture). The distance field is clamped into grey
// [0,1]. Distinct from the smooth Perlin `noiseTexture`; pairs well with `heightToNormalMap`.
inline Image cellularTexture(int width, int height, float scale = 0.06f, std::uint32_t seed = 0,
                             Cellular mode = Cellular::Cells) {
    Image img(width, height);
    if (img.empty()) return img;
    const core::WorleyNoise w(seed);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float sx = static_cast<float>(x) * scale;
            const float sy = static_cast<float>(y) * scale;
            float v;
            if (mode == Cellular::Cells) {
                v = w.f1(sx, sy);              // 0 at feature points, grows outward
            } else {
                v = w.crackle(sx, sy) * 2.0f;  // F2-F1: ~0 on edges (dark lines), larger inside
            }
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            img.setPixel(x, y, Color{v, v, v, 1.0f});
        }
    }
    return img;
}

// A grey MARBLE texture: parallel sine "veins" whose phase is warped by fbm turbulence, so the bands ripple and
// swirl like polished stone. `veinFrequency` sets how tightly packed the veins are (per pixel), `turbulence` how
// much the noise distorts them (0 = perfectly straight vertical bands), `noiseScale` the turbulence frequency.
// Grey [0,1]; tint through `gradientMap` for coloured marble. Same seed → same texture.
inline Image marbleTexture(int width, int height, float veinFrequency = 0.12f, float turbulence = 4.0f,
                           float noiseScale = 0.05f, std::uint64_t seed = 0, int octaves = 4) {
    Image img(width, height);
    if (img.empty()) return img;
    const core::Noise n(seed);
    const int oct = octaves < 1 ? 1 : octaves;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float turb = n.fbm2(static_cast<float>(x) * noiseScale, static_cast<float>(y) * noiseScale, oct);
            const float phase = static_cast<float>(x) * veinFrequency + turbulence * turb;
            float v = 0.5f + 0.5f * std::sin(phase);
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            img.setPixel(x, y, Color{v, v, v, 1.0f});
        }
    }
    return img;
}

// A grey WOOD-GRAIN texture: concentric growth rings around the image centre, warped by fbm turbulence so the
// rings wander like real timber. `ringScale` sets how many rings (larger = tighter), `turbulence` how much the
// noise bends them (0 = perfect circles), `noiseScale` the turbulence frequency. Grey [0,1]; run through
// `gradientMap` with a brown ramp for planks. Same seed → same texture.
inline Image woodTexture(int width, int height, float ringScale = 0.5f, float turbulence = 3.0f,
                         float noiseScale = 0.04f, std::uint64_t seed = 0, int octaves = 4) {
    Image img(width, height);
    if (img.empty()) return img;
    const core::Noise n(seed);
    const int oct = octaves < 1 ? 1 : octaves;
    const float cx = static_cast<float>(width) * 0.5f, cy = static_cast<float>(height) * 0.5f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float dx = static_cast<float>(x) - cx, dy = static_cast<float>(y) - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float turb = n.fbm2(static_cast<float>(x) * noiseScale, static_cast<float>(y) * noiseScale, oct);
            float v = 0.5f + 0.5f * std::sin(dist * ringScale + turbulence * turb);
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            img.setPixel(x, y, Color{v, v, v, 1.0f});
        }
    }
    return img;
}

} // namespace maz::render::patterns
