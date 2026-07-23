// tests/math/ransac.cpp — verifies RANSAC robust line fitting (math Ransac.hpp).
// Ground truths, deterministic (seeded LCG in the data + fixed RANSAC seed, no <random>, no clock):
//   * with ~30% gross outliers, RANSAC recovers the TRUE line direction (normal aligned) where an ordinary
//     total-least-squares fit over all points is dragged noticeably off;
//   * the consensus (inlier) count is close to the true inlier count, and every reported inlier is within
//     the threshold of the fitted line;
//   * a clean line (no outliers) is fit essentially exactly;
//   * too few points fail;
//   * determinism (same seed -> same model).
#include "maz/math/Ransac.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

// Total-least-squares normal over ALL points (the non-robust baseline).
static void tlsNormalAll(const std::vector<vec2>& pts, float& a, float& b) {
    double cx = 0, cy = 0;
    for (const vec2& p : pts) { cx += p.x; cy += p.y; }
    cx /= static_cast<double>(pts.size());
    cy /= static_cast<double>(pts.size());
    double sxx = 0, sxy = 0, syy = 0;
    for (const vec2& p : pts) {
        const double dx = p.x - cx, dy = p.y - cy;
        sxx += dx * dx; sxy += dx * dy; syy += dy * dy;
    }
    const double th = 0.5 * std::atan2(2 * sxy, sxx - syy);
    const double dx = std::cos(th), dy = std::sin(th);
    a = static_cast<float>(-dy);
    b = static_cast<float>(dx);
}

int main() {
    const float m = 0.5f, bb = 1.0f; // true line y = 0.5 x + 1
    // True unit normal of y = m x + b  ->  m x - y + b = 0  ->  (m, -1)/sqrt(1+m^2).
    const float tn = std::sqrt(1.0f + m * m);
    const vec2 trueNormal(m / tn, -1.0f / tn);

    // --- 1 & 2. 30% outliers: robust recovery + inlier consensus. ---
    {
        Lcg rng{0x2020u};
        std::vector<vec2> pts;
        const int nInliers = 140, nOutliers = 100;
        for (int i = 0; i < nInliers; ++i) {
            const float x = rng.range(-10, 10);
            pts.push_back(vec2(x, m * x + bb + rng.range(-0.05f, 0.05f)));
        }
        for (int i = 0; i < nOutliers; ++i) // a competing near-vertical bar that drags a naive fit off
            pts.push_back(vec2(rng.range(4.5f, 5.5f), rng.range(-10.0f, 10.0f)));

        const auto r = maz::math::ransacLine(pts, 0.2f, 400, 0x1234ull);
        CHECK(r.ok, "RANSAC produces a fit");
        const float align = std::fabs(r.a * trueNormal.x + r.b * trueNormal.y);
        CHECK(align > 0.99f, "RANSAC recovers the true line direction despite outliers");
        CHECK(r.inliers.size() >= 120 && r.inliers.size() <= 170, "the consensus size is close to the true inlier count");
        bool allWithin = true;
        for (int idx : r.inliers)
            if (std::fabs(r.a * pts[static_cast<std::size_t>(idx)].x + r.b * pts[static_cast<std::size_t>(idx)].y + r.c) >= 0.2f)
                allWithin = false;
        CHECK(allWithin, "every reported inlier lies within the threshold of the fitted line");

        // The non-robust baseline is dragged off: measure each fit's worst error on the TRUE inliers.
        float ba, bbn;
        tlsNormalAll(pts, ba, bbn);
        double cx = 0, cy = 0;
        for (const vec2& p : pts) { cx += p.x; cy += p.y; }
        cx /= static_cast<double>(pts.size());
        cy /= static_cast<double>(pts.size());
        const float bc = static_cast<float>(-(static_cast<double>(ba) * cx + static_cast<double>(bbn) * cy));
        float ransMax = 0.0f, naiveMax = 0.0f;
        for (int i = 0; i < nInliers; ++i) {
            ransMax = std::max(ransMax, std::fabs(r.a * pts[static_cast<std::size_t>(i)].x + r.b * pts[static_cast<std::size_t>(i)].y + r.c));
            naiveMax = std::max(naiveMax, std::fabs(ba * pts[static_cast<std::size_t>(i)].x + bbn * pts[static_cast<std::size_t>(i)].y + bc));
        }
        CHECK(ransMax < 0.1f, "RANSAC fits the true inliers tightly");
        CHECK(naiveMax > ransMax * 5.0f, "the non-robust baseline is dragged far off the true inliers by the outliers");
    }

    // --- 3. Clean line: near-exact. ---
    {
        Lcg rng{0x7777u};
        std::vector<vec2> pts;
        for (int i = 0; i < 50; ++i) {
            const float x = rng.range(-10, 10);
            pts.push_back(vec2(x, m * x + bb));
        }
        const auto r = maz::math::ransacLine(pts, 0.1f, 100, 0x55ull);
        CHECK(r.ok && std::fabs(std::fabs(r.a * trueNormal.x + r.b * trueNormal.y) - 1.0f) < 1e-3f,
              "a clean line is fit essentially exactly");
        CHECK(r.inliers.size() == pts.size(), "all points are inliers on a clean line");
    }

    // --- 4. Degenerate. ---
    {
        std::vector<vec2> one{{0, 0}};
        CHECK(!maz::math::ransacLine(one, 0.1f).ok, "a single point cannot fit a line");
    }

    // --- 5. Determinism. ---
    {
        Lcg rng{0x9001u};
        std::vector<vec2> pts;
        for (int i = 0; i < 40; ++i) pts.push_back(vec2(rng.range(-5, 5), rng.range(-5, 5)));
        const auto a = maz::math::ransacLine(pts, 0.5f, 100, 0xABCDull);
        const auto b = maz::math::ransacLine(pts, 0.5f, 100, 0xABCDull);
        CHECK(a.a == b.a && a.b == b.b && a.c == b.c && a.inliers.size() == b.inliers.size(),
              "same seed produces the same model");
    }

    if (g_fail == 0) {
        std::printf("ransac: OK — robust recovery, consensus, clean-exact, degenerate, determinism.\n");
        return 0;
    }
    std::printf("ransac: %d failure(s).\n", g_fail);
    return 1;
}
