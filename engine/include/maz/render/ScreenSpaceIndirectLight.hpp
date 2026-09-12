#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render screen-space indirect light (SSIL) — the CPU reference for Godot's SSIL pass. Where SSAO
// darkens creases by counting nearby occluders, SSIL goes one step further and gathers the *colored*
// light bouncing off those nearby surfaces: a red wall tints the white floor beside it red, a lit
// surface throws a soft colored glow onto its neighbours. This is one-bounce screen-space global
// illumination.
//
// The full effect runs in a fragment shader over the depth/normal/color G-buffer on the GPU; this
// header is the gather MATH, decoupled from any GPU, so it is unit-testable headlessly (exactly how the
// SSAO pass was validated). For each pixel it samples a screen-space neighbourhood, and for every
// neighbour that sits in the pixel's hemisphere (in front of its surface) and within a world-space
// radius, it accumulates that neighbour's color weighted by the cosine term and a distance falloff.
// The result is the indirect (bounced) light to add to the pixel. Plain float math, no dependencies.
namespace maz::render {

// One G-buffer texel: view/world position (p), surface normal (n), and the direct-lit color (c) that
// this texel can bounce onto its neighbours.
struct SsilSample {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

struct SsilColor {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

struct SsilParams {
    int radiusPx = 1;          // screen kernel half-extent in pixels (samples the (2R+1)^2 neighbourhood)
    float worldRadius = 3.0f;  // max world-space distance a neighbour can influence
    float bias = 0.0f;         // minimum dot(normal, dir) to count a neighbour (hemisphere test)
    float intensity = 1.0f;    // overall strength of the bounced light
};

// Compute per-pixel indirect light for a w*h G-buffer. `img` is row-major, size w*h. Returns a w*h
// buffer of bounced color to be added to the direct lighting.
inline std::vector<SsilColor> ssilGather(int w, int h, const std::vector<SsilSample>& img,
                                         const SsilParams& p) {
    std::vector<SsilColor> out(static_cast<size_t>(w) * static_cast<size_t>(h));
    const int R = p.radiusPx < 0 ? 0 : p.radiusPx;
    const float invRadius = p.worldRadius > 0.0f ? 1.0f / p.worldRadius : 0.0f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const SsilSample& c = img[static_cast<size_t>(y) * static_cast<size_t>(w) +
                                      static_cast<size_t>(x)];
            float ar = 0.0f, ag = 0.0f, ab = 0.0f;
            int considered = 0;

            for (int dy = -R; dy <= R; ++dy) {
                for (int dx = -R; dx <= R; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                        continue;
                    }
                    ++considered;
                    const SsilSample& s = img[static_cast<size_t>(ny) * static_cast<size_t>(w) +
                                              static_cast<size_t>(nx)];
                    const float dltx = s.px - c.px;
                    const float dlty = s.py - c.py;
                    const float dltz = s.pz - c.pz;
                    const float dist = std::sqrt(dltx * dltx + dlty * dlty + dltz * dltz);
                    if (dist <= 0.0f || dist > p.worldRadius) {
                        continue;
                    }
                    const float inv = 1.0f / dist;
                    const float nl = c.nx * (dltx * inv) + c.ny * (dlty * inv) + c.nz * (dltz * inv);
                    if (nl <= p.bias) {
                        continue; // neighbour is behind this surface — no bounce
                    }
                    const float falloff = 1.0f - dist * invRadius; // linear range attenuation
                    const float wgt = nl * falloff;
                    ar += s.r * wgt;
                    ag += s.g * wgt;
                    ab += s.b * wgt;
                }
            }

            SsilColor result;
            if (considered > 0) {
                const float k = p.intensity / static_cast<float>(considered);
                result.r = ar * k;
                result.g = ag * k;
                result.b = ab * k;
            }
            out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = result;
        }
    }
    return out;
}

} // namespace maz::render
