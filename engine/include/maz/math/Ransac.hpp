#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::math RANSAC line fitting — fit a line to points that contain GROSS OUTLIERS, robustly. Ordinary
// least-squares (and the total-least-squares plane/circle fits) assume every point belongs to the shape, so
// a handful of stray points — a mistracked feature, a sensor glitch, a wall behind the floor — drags the fit
// badly off. RANSAC (RANdom SAmple Consensus) instead guesses many candidate lines from tiny random samples,
// keeps the one the most points AGREE with (the "consensus" / inliers), and refits only to those — so
// outliers are ignored rather than averaged in. It is the standard tool for fitting to noisy real-world
// point sets: aligning scanned edges, snapping a wall/floor line out of messy depth points, robust
// trajectory or trend estimation, calibration with bad samples. Deterministic (seeded), header-only,
// std-only. Godot ships no robust estimator.
namespace maz::math {

struct RansacLine {
    vec2 point{0.0f, 0.0f};   // a point on the line (the inlier centroid)
    vec2 dir{1.0f, 0.0f};     // unit direction along the line
    float a = 0.0f, b = 1.0f, c = 0.0f; // normal form a*x + b*y + c = 0, with a^2 + b^2 = 1
    std::vector<int> inliers; // indices of consensus points
    bool ok = false;
};

namespace detail {
// Total-least-squares line direction through points[idx]: principal eigenvector of the 2D covariance.
inline void ransacTlsLine(const std::vector<vec2>& pts, const std::vector<int>& idx, vec2& centroid, vec2& dir) {
    double cx = 0.0, cy = 0.0;
    for (int i : idx) {
        cx += pts[static_cast<std::size_t>(i)].x;
        cy += pts[static_cast<std::size_t>(i)].y;
    }
    const double n = static_cast<double>(idx.size());
    cx /= n;
    cy /= n;
    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (int i : idx) {
        const double dx = pts[static_cast<std::size_t>(i)].x - cx, dy = pts[static_cast<std::size_t>(i)].y - cy;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    const double theta = 0.5 * std::atan2(2.0 * sxy, sxx - syy); // principal axis angle
    centroid = vec2(static_cast<float>(cx), static_cast<float>(cy));
    dir = vec2(static_cast<float>(std::cos(theta)), static_cast<float>(std::sin(theta)));
}
} // namespace detail

// Fit a line to `pts` by RANSAC. A point is an inlier if within `threshold` distance of the model. Runs
// `iterations` random 2-point trials, then refits (total least squares) to the best consensus set.
inline RansacLine ransacLine(const std::vector<vec2>& pts, float threshold, int iterations = 200,
                             std::uint64_t seed = 0x9E3779B97F4A7C15ull) {
    RansacLine res;
    const std::size_t n = pts.size();
    if (n < 2 || threshold <= 0.0f) {
        return res;
    }
    std::uint64_t s = seed;
    auto rnd = [&]() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    };

    int bestCount = -1;
    float bestA = 0.0f, bestB = 1.0f, bestC = 0.0f;
    for (int it = 0; it < iterations; ++it) {
        const std::size_t i = rnd() % n;
        std::size_t j = rnd() % n;
        if (j == i) {
            j = (j + 1) % n;
        }
        const vec2 pi = pts[i], pj = pts[j];
        const float dx = pj.x - pi.x, dy = pj.y - pi.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9f) {
            continue;
        }
        // Line normal (unit) and offset.
        const float a = -dy / len, b = dx / len;
        const float c = -(a * pi.x + b * pi.y);
        int count = 0;
        for (std::size_t k = 0; k < n; ++k) {
            if (std::fabs(a * pts[k].x + b * pts[k].y + c) < threshold) {
                ++count;
            }
        }
        if (count > bestCount) {
            bestCount = count;
            bestA = a;
            bestB = b;
            bestC = c;
        }
    }
    if (bestCount < 2) {
        return res;
    }
    // Gather the best consensus set.
    std::vector<int> inl;
    for (std::size_t k = 0; k < n; ++k) {
        if (std::fabs(bestA * pts[k].x + bestB * pts[k].y + bestC) < threshold) {
            inl.push_back(static_cast<int>(k));
        }
    }
    // Refit (total least squares) to the consensus set for a stable final line.
    vec2 centroid, dir;
    detail::ransacTlsLine(pts, inl, centroid, dir);
    const float dlen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (dlen < 1e-9f) {
        return res;
    }
    dir = vec2(dir.x / dlen, dir.y / dlen);
    const float a = -dir.y, b = dir.x;
    const float c = -(a * centroid.x + b * centroid.y);
    // Recompute inliers against the refit model.
    res.inliers.clear();
    for (std::size_t k = 0; k < n; ++k) {
        if (std::fabs(a * pts[k].x + b * pts[k].y + c) < threshold) {
            res.inliers.push_back(static_cast<int>(k));
        }
    }
    res.point = centroid;
    res.dir = dir;
    res.a = a;
    res.b = b;
    res.c = c;
    res.ok = true;
    return res;
}

} // namespace maz::math
