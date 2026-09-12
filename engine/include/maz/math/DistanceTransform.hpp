#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::math exact Euclidean distance transform (Felzenszwalb & Huttenlocher, 2004).
//
// Given a grid where some cells are "seeds", the distance transform fills EVERY cell with its exact
// Euclidean distance to the nearest seed, and (as a byproduct) which seed is nearest — a grid Voronoi
// labelling. This is the workhorse behind exact SDF baking, "how far is this tile from the nearest
// wall?" navigation clearance fields (spawn placement, corridor widths, influence maps), morphological
// grow/shrink, and grid Voronoi regions.
//
// The engine's ui::Sdf builds glyph fields with dead reckoning, which is APPROXIMATE (accurate to under
// a texel, fine for fonts). This is the EXACT transform: the separable Felzenszwalb-Huttenlocher lower-
// envelope-of-parabolas algorithm, O(n) per row and per column, so the result equals a brute-force
// nearest-seed search to the last bit — which is exactly what the tests check. It also threads the
// argmin through both passes to recover each cell's nearest-seed coordinates. Header-only, deterministic,
// no allocation beyond the output + scratch.
namespace maz::math {

struct DistanceField {
    int width = 0;
    int height = 0;
    std::vector<float> distance;  // Euclidean distance to the nearest seed (row-major w*h)
    std::vector<int> nearestX;    // x of the nearest seed, or -1 if the grid had no seeds
    std::vector<int> nearestY;    // y of the nearest seed, or -1 if the grid had no seeds
};

namespace detail {

// A "very large" finite squared-distance standing in for +infinity. Chosen so it dwarfs any real
// squared distance on a sane grid yet leaves headroom in float; it cancels exactly in the parabola
// intersection formula, so using a sentinel (rather than true inf) keeps the arithmetic well-behaved.
constexpr float kDtInf = 1e20f;

// 1D exact squared-distance transform of a sampled function f (length n). Writes d[q] = min_p (f[p] +
// (q-p)^2) and arg[q] = the p achieving it. Scratch vectors v/z are sized n and n+1 by the caller.
inline void dt1d(const float* f, int n, float* d, int* arg, int* v, float* z) {
    int k = 0;
    v[0] = 0;
    z[0] = -kDtInf;
    z[1] = kDtInf;
    for (int q = 1; q < n; ++q) {
        const float fq = f[q] + static_cast<float>(q) * static_cast<float>(q);
        float s = 0.0f;
        while (true) {
            const float fv = f[v[k]] + static_cast<float>(v[k]) * static_cast<float>(v[k]);
            s = (fq - fv) / (2.0f * static_cast<float>(q) - 2.0f * static_cast<float>(v[k]));
            if (s <= z[k] && k > 0) {
                --k;
            } else {
                break;
            }
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = kDtInf;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < static_cast<float>(q)) {
            ++k;
        }
        const int dq = q - v[k];
        d[q] = static_cast<float>(dq) * static_cast<float>(dq) + f[v[k]];
        arg[q] = v[k];
    }
}

} // namespace detail

// Compute the exact Euclidean distance transform of a seed mask. `seed` is row-major w*h; a cell is a
// seed when its value is non-zero. Every output cell gets its exact distance to the nearest seed and
// that seed's coordinates. If the grid contains no seeds, distances are left at +inf-scale (large) and
// nearest coords are -1.
inline DistanceField distanceTransform(const std::vector<uint8_t>& seed, int w, int h) {
    DistanceField out;
    out.width = w;
    out.height = h;
    if (w <= 0 || h <= 0) {
        return out;
    }
    const int n = w * h;
    out.distance.assign(static_cast<std::size_t>(n), 0.0f);
    out.nearestX.assign(static_cast<std::size_t>(n), -1);
    out.nearestY.assign(static_cast<std::size_t>(n), -1);

    // Squared-distance field: 0 at seeds, "infinite" elsewhere.
    std::vector<float> sq(static_cast<std::size_t>(n), detail::kDtInf);
    bool anySeed = false;
    for (int i = 0; i < n; ++i) {
        if (seed[static_cast<std::size_t>(i)] != 0) {
            sq[static_cast<std::size_t>(i)] = 0.0f;
            anySeed = true;
        }
    }
    if (!anySeed) {
        for (int i = 0; i < n; ++i) {
            out.distance[static_cast<std::size_t>(i)] = detail::kDtInf;
        }
        return out;
    }

    const int maxDim = w > h ? w : h;
    std::vector<int> v(static_cast<std::size_t>(maxDim));
    std::vector<float> z(static_cast<std::size_t>(maxDim) + 1);
    std::vector<float> f(static_cast<std::size_t>(maxDim));
    std::vector<float> d(static_cast<std::size_t>(maxDim));
    std::vector<int> arg(static_cast<std::size_t>(maxDim));

    // Pass 1 — transform each COLUMN. srcY[x + y*w] = the row of the nearest seed within column x.
    std::vector<int> srcY(static_cast<std::size_t>(n), 0);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            f[static_cast<std::size_t>(y)] = sq[static_cast<std::size_t>(x + y * w)];
        }
        detail::dt1d(f.data(), h, d.data(), arg.data(), v.data(), z.data());
        for (int y = 0; y < h; ++y) {
            sq[static_cast<std::size_t>(x + y * w)] = d[static_cast<std::size_t>(y)];
            srcY[static_cast<std::size_t>(x + y * w)] = arg[static_cast<std::size_t>(y)];
        }
    }

    // Pass 2 — transform each ROW using the column results. The row argmin gives the nearest seed's x;
    // the column pass' srcY at (thatX, y) gives its y.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            f[static_cast<std::size_t>(x)] = sq[static_cast<std::size_t>(x + y * w)];
        }
        detail::dt1d(f.data(), w, d.data(), arg.data(), v.data(), z.data());
        for (int x = 0; x < w; ++x) {
            const int idx = x + y * w;
            const float sqDist = d[static_cast<std::size_t>(x)];
            out.distance[static_cast<std::size_t>(idx)] =
                sqDist >= detail::kDtInf ? detail::kDtInf : std::sqrt(sqDist);
            const int nx = arg[static_cast<std::size_t>(x)];
            out.nearestX[static_cast<std::size_t>(idx)] = nx;
            out.nearestY[static_cast<std::size_t>(idx)] = srcY[static_cast<std::size_t>(nx + y * w)];
        }
    }
    return out;
}

} // namespace maz::math
