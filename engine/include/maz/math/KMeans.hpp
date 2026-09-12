#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// maz::math::kMeans — partition N-dimensional points into k clusters, each represented by its centroid, so
// that points end up grouped with their nearest centre (Lloyd's algorithm with k-means++ seeding). This is
// the general clustering workhorse: grouping units/enemies into squads by position, building spatial LOD
// clusters, deriving a representative palette or set of "archetype" values from data, seeding procedural
// distributions, or compressing a cloud of samples down to k prototypes. The engine had median-cut colour
// quantization (fixed to RGB) but no general k-means over arbitrary vectors. It works by alternating two
// steps until stable: ASSIGN every point to its nearest centroid, then MOVE each centroid to the mean of its
// assigned points — which provably never increases the total within-cluster squared distance (the
// "inertia"), so it converges. k-means++ picks well-spread initial centres (proportional to squared
// distance) so it converges fast and avoids poor local minima. Deterministic given a seed. Header-only,
// std-only. Godot has no clustering.
namespace maz::math {

struct KMeansResult {
    std::vector<std::vector<double>> centroids; // k x dim
    std::vector<int> assignment;                // per input point: which cluster (0..k-1)
    double inertia = 0.0;                        // total squared distance of points to their centroid
    int iterations = 0;                          // Lloyd iterations performed
};

namespace detail {
inline std::uint64_t kmSplitmix(std::uint64_t& s) {
    s += 0x9E3779B97F4A7C15ull;
    std::uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline double kmRand01(std::uint64_t& s) {
    return static_cast<double>(kmSplitmix(s) >> 11) * (1.0 / 9007199254740992.0);
}
inline double kmDistSq(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double e = a[i] - b[i];
        d += e * e;
    }
    return d;
}
} // namespace detail

// Cluster `points` (all the same dimension) into `k` groups. `k` is clamped to [1, N]. Empty input returns
// an empty result. Deterministic for a given `seed`.
inline KMeansResult kMeans(const std::vector<std::vector<double>>& points, int k, std::uint64_t seed,
                           int maxIters = 100) {
    KMeansResult out;
    const std::size_t n = points.size();
    if (n == 0 || k <= 0) {
        return out;
    }
    if (static_cast<std::size_t>(k) > n) {
        k = static_cast<int>(n);
    }
    const std::size_t dim = points[0].size();
    const std::size_t kk = static_cast<std::size_t>(k);
    std::uint64_t rng = seed + 0x1234567u;

    // --- k-means++ seeding. ---
    std::vector<std::vector<double>> cent;
    cent.reserve(kk);
    cent.push_back(points[static_cast<std::size_t>(detail::kmSplitmix(rng) % n)]);
    std::vector<double> best(n, std::numeric_limits<double>::max());
    while (cent.size() < kk) {
        double total = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = detail::kmDistSq(points[i], cent.back());
            if (d < best[i]) {
                best[i] = d;
            }
            total += best[i];
        }
        // Choose the next centre with probability proportional to its squared distance.
        std::size_t pick = n - 1;
        if (total > 0.0) {
            double target = detail::kmRand01(rng) * total;
            for (std::size_t i = 0; i < n; ++i) {
                target -= best[i];
                if (target <= 0.0) {
                    pick = i;
                    break;
                }
            }
        } else {
            pick = static_cast<std::size_t>(detail::kmSplitmix(rng) % n);
        }
        cent.push_back(points[pick]);
    }

    // --- Lloyd iterations. ---
    std::vector<int> assign(n, -1);
    int iters = 0;
    for (; iters < maxIters; ++iters) {
        bool changed = false;
        for (std::size_t i = 0; i < n; ++i) {
            double bestD = std::numeric_limits<double>::max();
            int bestC = 0;
            for (std::size_t c = 0; c < kk; ++c) {
                const double d = detail::kmDistSq(points[i], cent[c]);
                if (d < bestD) {
                    bestD = d;
                    bestC = static_cast<int>(c);
                }
            }
            if (assign[i] != bestC) {
                assign[i] = bestC;
                changed = true;
            }
        }
        if (!changed && iters > 0) {
            break;
        }

        // Move centroids to the mean of their members.
        std::vector<std::vector<double>> sum(kk, std::vector<double>(dim, 0.0));
        std::vector<std::size_t> cnt(kk, 0);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t c = static_cast<std::size_t>(assign[i]);
            for (std::size_t d = 0; d < dim; ++d) {
                sum[c][d] += points[i][d];
            }
            ++cnt[c];
        }
        for (std::size_t c = 0; c < kk; ++c) {
            if (cnt[c] == 0) {
                // Reseed an empty cluster to the point farthest from its current centroid.
                double far = -1.0;
                std::size_t idx = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    const double d = detail::kmDistSq(points[i], cent[static_cast<std::size_t>(assign[i])]);
                    if (d > far) {
                        far = d;
                        idx = i;
                    }
                }
                cent[c] = points[idx];
            } else {
                for (std::size_t d = 0; d < dim; ++d) {
                    cent[c][d] = sum[c][d] / static_cast<double>(cnt[c]);
                }
            }
        }
    }

    // Final inertia.
    double inertia = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        inertia += detail::kmDistSq(points[i], cent[static_cast<std::size_t>(assign[i])]);
    }

    out.centroids = std::move(cent);
    out.assignment = std::move(assign);
    out.inertia = inertia;
    out.iterations = iters;
    return out;
}

} // namespace maz::math
