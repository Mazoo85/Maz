#pragma once

#include "maz/render/Image.hpp" // Image, Color

#include <cmath>
#include <vector>

// maz::render::bilateralFilter — edge-preserving smoothing. An ordinary blur (box / Gaussian) averages each
// pixel with its neighbours regardless of content, so it kills noise but also smears every edge into mush.
// The bilateral filter weights each neighbour by TWO things: how close it is (spatial) AND how similar its
// colour is (range). Neighbours across a strong edge have a very different colour, so they get almost no
// weight — the edge stays crisp while flat regions still get cleaned up. This is the staple behind photo
// denoise, "beautify"/skin-smoothing, cartoon/stylize preprocessing, and cleaning noisy procedural or
// baked textures before use. Godot's Image has no bilateral (only whole-image resize/blur-free ops).
// `spatialSigma` sets the neighbourhood size, `rangeSigma` how different a colour must be to be ignored.
// Output is a convex blend of the input, so it never overshoots. Header-only, std-only, deterministic.
namespace maz::render {

inline Image bilateralFilter(const Image& src, float spatialSigma, float rangeSigma) {
    Image out(src.width(), src.height());
    if (src.empty()) {
        return out;
    }
    const int w = src.width(), h = src.height();
    const float ss = spatialSigma > 1e-3f ? spatialSigma : 1e-3f;
    const float rs = rangeSigma > 1e-3f ? rangeSigma : 1e-3f;
    const int radius = static_cast<int>(std::ceil(2.5f * ss));
    const float invSpatial = 1.0f / (2.0f * ss * ss);
    const float invRange = 1.0f / (2.0f * rs * rs);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color c0 = src.getPixel(x, y);
            float wsum = 0.0f, r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            for (int dy = -radius; dy <= radius; ++dy) {
                const int ny = y + dy;
                if (ny < 0 || ny >= h) {
                    continue;
                }
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= w) {
                        continue;
                    }
                    const Color cn = src.getPixel(nx, ny);
                    const float sd = static_cast<float>(dx * dx + dy * dy);
                    const float cr = cn.r - c0.r, cg = cn.g - c0.g, cb = cn.b - c0.b;
                    const float cd = cr * cr + cg * cg + cb * cb;
                    const float weight = std::exp(-sd * invSpatial - cd * invRange);
                    wsum += weight;
                    r += weight * cn.r;
                    g += weight * cn.g;
                    b += weight * cn.b;
                    a += weight * cn.a;
                }
            }
            if (wsum > 1e-20f) {
                const float inv = 1.0f / wsum;
                out.setPixel(x, y, Color{r * inv, g * inv, b * inv, a * inv});
            } else {
                out.setPixel(x, y, c0);
            }
        }
    }
    return out;
}

} // namespace maz::render
