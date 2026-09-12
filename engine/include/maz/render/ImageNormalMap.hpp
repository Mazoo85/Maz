#pragma once

#include "maz/render/ColorOps.hpp" // Color
#include "maz/render/Image.hpp"     // Image

#include <cmath>

// maz::render HEIGHT -> NORMAL MAP — turn a grey heightmap (bright = high, dark = low) into a tangent-space NORMAL
// MAP, the blue-purple texture that makes a flat surface look bumpy under lighting. Paint or generate a height image
// — bricks, cobbles, scales, wrinkles, carved detail, hammered metal — and this reads its slopes and writes the
// surface direction at every pixel, so a lighting shader can fake all that relief without extra geometry. It is the
// standard "bake a normal map from a height texture" step (Godot's Image bump-to-normal, Blender's bump node,
// Substance/Photoshop's "Normal from Height"). The height is read from the RED channel; `strength` exaggerates or
// softens the bumps. Output is an RGBA8 image encoding the unit normal as (x,y,z)*0.5+0.5 with alpha 1 — a flat
// region comes out the classic (128,128,255) light blue. Header-only, deterministic, headless — pure pixel math.
//
// Scope note (honest): slopes are measured with central differences and the border pixels clamp to their neighbours
// (so the very edge is flat-ish). Green is +Y (OpenGL convention — the common one; flip G for a DirectX map). The
// input's red channel is the height; colour/other channels are ignored. An empty input yields an empty image.
namespace maz::render {

// Bake a tangent-space normal map from the RED channel of `heights`. `strength` scales the bump depth.
inline Image heightToNormalMap(const Image& heights, float strength = 1.0f) {
    Image out(heights.width(), heights.height());
    if (out.empty()) return out;
    const int w = heights.width(), h = heights.height();
    auto H = [&](int x, int y) {
        const int cx = x < 0 ? 0 : (x >= w ? w - 1 : x);
        const int cy = y < 0 ? 0 : (y >= h ? h - 1 : y);
        return heights.getPixel(cx, cy).r; // height from the red channel
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float dhx = H(x + 1, y) - H(x - 1, y); // central difference in X
            const float dhy = H(x, y + 1) - H(x, y - 1); // central difference in Y
            // Surface rising in +X tilts the normal toward -X; +Y (OpenGL) up.
            float nx = -dhx * strength, ny = -dhy * strength, nz = 1.0f;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;
            out.setPixel(x, y, Color{nx * 0.5f + 0.5f, ny * 0.5f + 0.5f, nz * 0.5f + 0.5f, 1.0f});
        }
    }
    return out;
}

} // namespace maz::render
