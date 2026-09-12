#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// maz::ui SDF — turn a coverage bitmap (a rasterized glyph's alpha) into a Signed Distance Field:
// per texel, the signed distance to the shape's edge (positive inside, negative outside, ~0 on the
// contour). An SDF glyph atlas is what lets text stay crisp at ANY scale from one small texture —
// the fragment shader thresholds the interpolated distance instead of the blurry alpha, so a 32px
// SDF renders sharp at 8px or 200px (Godot's "MSDF"/SDF font mode; Valve's classic technique).
//
// generateSdf() computes the field with dead reckoning — an O(n) two-pass sweep that tracks each
// texel's nearest boundary point and recomputes the true Euclidean distance to it, so the result is
// accurate to well under a texel (validated against a brute-force exact transform in the tests).
// packSdf() maps the signed field to bytes with 0.5 on the edge and a chosen spread (texels per
// 0.5 unit), the layout an SDF shader samples. Header-only, deterministic, no GPU — a bake step.
namespace maz::ui {

// Signed distance (in texels) to the glyph edge for every texel. `coverage` is row-major w*h; a
// texel is INSIDE when its coverage >= `threshold` (default 128, i.e. alpha >= 0.5). Positive =
// inside, negative = outside. Out-of-bounds counts as outside.
inline std::vector<float> generateSdf(const uint8_t* coverage, int w, int h,
                                      uint8_t threshold = 128) {
    const int n = w * h;
    std::vector<float> dist(static_cast<size_t>(n), std::numeric_limits<float>::infinity());
    std::vector<int> nx(static_cast<size_t>(n), -1); // nearest boundary texel coords
    std::vector<int> ny(static_cast<size_t>(n), -1);
    std::vector<uint8_t> inside(static_cast<size_t>(n), 0);

    auto idx = [w](int x, int y) { return y * w + x; };
    for (int i = 0; i < n; ++i) {
        inside[static_cast<size_t>(i)] = coverage[i] >= threshold ? 1u : 0u;
    }

    // Seed: a texel on the boundary (its membership differs from a 4-neighbour, OOB = outside) is
    // at distance 0 and is its own nearest boundary point.
    auto insideAt = [&](int x, int y) -> uint8_t {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return 0u;
        return inside[static_cast<size_t>(idx(x, y))];
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const uint8_t c = insideAt(x, y);
            if (insideAt(x - 1, y) != c || insideAt(x + 1, y) != c || insideAt(x, y - 1) != c ||
                insideAt(x, y + 1) != c) {
                const int i = idx(x, y);
                dist[static_cast<size_t>(i)] = 0.0f;
                nx[static_cast<size_t>(i)] = x;
                ny[static_cast<size_t>(i)] = y;
            }
        }
    }

    const float d1 = 1.0f;
    const float d2 = 1.4142135623730951f;
    auto relax = [&](int x, int y, int ox, int oy, float edge) {
        if (x + ox < 0 || y + oy < 0 || x + ox >= w || y + oy >= h)
            return;
        const int i = idx(x, y);
        const int j = idx(x + ox, y + oy);
        if (dist[static_cast<size_t>(j)] + edge < dist[static_cast<size_t>(i)]) {
            const int bx = nx[static_cast<size_t>(j)];
            const int by = ny[static_cast<size_t>(j)];
            if (bx < 0)
                return;
            nx[static_cast<size_t>(i)] = bx;
            ny[static_cast<size_t>(i)] = by;
            const float dx = static_cast<float>(x - bx);
            const float dy = static_cast<float>(y - by);
            dist[static_cast<size_t>(i)] = std::sqrt(dx * dx + dy * dy);
        }
    };

    // Forward pass (top-left to bottom-right): pull from already-visited neighbours.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            relax(x, y, -1, -1, d2);
            relax(x, y, 0, -1, d1);
            relax(x, y, 1, -1, d2);
            relax(x, y, -1, 0, d1);
        }
    }
    // Backward pass (bottom-right to top-left).
    for (int y = h - 1; y >= 0; --y) {
        for (int x = w - 1; x >= 0; --x) {
            relax(x, y, 1, 0, d1);
            relax(x, y, -1, 1, d2);
            relax(x, y, 0, 1, d1);
            relax(x, y, 1, 1, d2);
        }
    }

    // Sign by membership.
    std::vector<float> out(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        float dd = dist[static_cast<size_t>(i)];
        if (!std::isfinite(dd)) {
            dd = static_cast<float>(w + h); // fully-uniform bitmap: everything "far"
        }
        out[static_cast<size_t>(i)] = inside[static_cast<size_t>(i)] ? dd : -dd;
    }
    return out;
}

// Pack a signed field (texels) to bytes: 0.5 (127/128) at the edge, +spread texels -> 1.0, -spread
// -> 0.0, clamped. `spread` is how many texels the gradient covers on each side (larger =
// softer/further usable range). This is the byte layout an SDF text shader samples and thresholds
// at 0.5.
inline std::vector<uint8_t> packSdf(const std::vector<float>& signedField, float spread) {
    const float s = spread > 0.0f ? spread : 1.0f;
    std::vector<uint8_t> out(signedField.size());
    for (size_t i = 0; i < signedField.size(); ++i) {
        float v = signedField[i] / (2.0f * s) + 0.5f; // edge -> 0.5
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        out[i] = static_cast<uint8_t>(v * 255.0f + 0.5f);
    }
    return out;
}

} // namespace maz::ui
