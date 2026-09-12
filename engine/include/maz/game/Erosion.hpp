#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::game thermal erosion — weather a procedural heightmap so it looks geologically aged instead of raw
// fractal noise. Terrain straight out of Perlin/fbm or diamond-square has implausibly steep, jagged slopes;
// real hillsides can't hold material past a "talus angle" — anything steeper crumbles and the debris slides
// downhill until the slope relaxes. Thermal erosion simulates exactly that: wherever an adjacent height
// difference exceeds the talus threshold, a fraction of the excess material is moved down to the lower
// neighbours, spread in proportion to how much lower each is. Run for a number of passes it softens ridges,
// fills gullies, and forms natural scree slopes at the talus angle. It is the cheap, stable half of terrain
// weathering (the other being water/hydraulic erosion) and a staple of procedural landscape tools. Crucially
// it only MOVES material between cells — never creates or destroys it — so total volume is conserved exactly.
// Uses a Jacobi (double-buffered) update so the result is order-independent and deterministic. Header-only,
// std-only. Godot has no terrain erosion.
namespace maz::game {

// In-place thermal erosion of a width*height row-major heightmap. `talus` is the slope (height units per
// cell) above which material slips; `factor` in (0,0.5] is the fraction of the excess moved per pass;
// `iterations` passes. Material is only redistributed, so the sum of all heights is unchanged.
inline void thermalErosion(std::vector<float>& height, int width, int height_, float talus,
                           int iterations, float factor = 0.5f) {
    if (width <= 0 || height_ <= 0 ||
        height.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height_)) {
        return;
    }
    if (factor < 0.0f) {
        factor = 0.0f;
    }
    if (factor > 0.5f) {
        factor = 0.5f; // above 0.5 the explicit update can overshoot/oscillate
    }
    const int w = width;
    const int h = height_;
    auto idx = [w](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                          static_cast<std::size_t>(x); };
    std::vector<float> delta(height.size());

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    for (int it = 0; it < iterations; ++it) {
        std::fill(delta.begin(), delta.end(), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float hc = height[idx(x, y)];
                float dMax = 0.0f;
                float dTotal = 0.0f;
                float diff[4] = {0, 0, 0, 0};
                for (int k = 0; k < 4; ++k) {
                    const int nx = x + dx[k];
                    const int ny = y + dy[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                        continue;
                    }
                    const float d = hc - height[idx(nx, ny)];
                    if (d > 0.0f) {
                        diff[k] = d;
                        dTotal += d;
                        if (d > dMax) {
                            dMax = d;
                        }
                    }
                }
                if (dMax <= talus || dTotal <= 0.0f) {
                    continue;
                }
                const float amount = factor * (dMax - talus);
                for (int k = 0; k < 4; ++k) {
                    if (diff[k] <= 0.0f) {
                        continue;
                    }
                    const int nx = x + dx[k];
                    const int ny = y + dy[k];
                    const float move = amount * (diff[k] / dTotal);
                    delta[idx(nx, ny)] += move;
                    delta[idx(x, y)] -= move;
                }
            }
        }
        for (std::size_t i = 0; i < height.size(); ++i) {
            height[i] += delta[i];
        }
    }
}

} // namespace maz::game
