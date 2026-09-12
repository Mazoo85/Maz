// tests/math/reuleaux.cpp — verifies the Reuleaux constant-width polygon (math Reuleaux.hpp).
// Ground truths, deterministic (fixed shapes, no <random>, no clock):
//   * CONSTANT WIDTH (the defining, airtight property): measuring the shape's extent (max minus min
//     projection) in 360 directions gives the SAME width every time, for 3/5/7 sides;
//   * DIAMETER = WIDTH: the largest distance between any two boundary points equals the width (a corollary of
//     constant width);
//   * CONVEXITY: the boundary turns the same way at every vertex (a convex curve);
//   * an even/invalid side count is bumped to a valid odd one that still has constant width; determinism.
#include "maz/math/Reuleaux.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float widthInDirection(const std::vector<vec2>& p, float theta) {
    const vec2 d(std::cos(theta), std::sin(theta));
    float mn = 1e30f, mx = -1e30f;
    for (const vec2& q : p) {
        const float pr = q.x * d.x + q.y * d.y;
        mn = std::min(mn, pr);
        mx = std::max(mx, pr);
    }
    return mx - mn;
}

static void checkConstantWidth(int sides, float w) {
    const auto p = maz::math::reuleauxPolygon(sides, w, vec2(1.0f, -2.0f), 48);
    float worst = 0.0f;
    for (int i = 0; i < 360; ++i) {
        const float th = 3.14159265358979324f * static_cast<float>(i) / 180.0f;
        worst = std::max(worst, std::fabs(widthInDirection(p, th) - w));
    }
    CHECK(worst < 3e-3f, "a Reuleaux polygon has the same width in every direction");
}

int main() {
    // --- 1. Constant width for several odd side counts. ---
    checkConstantWidth(3, 4.0f);
    checkConstantWidth(5, 3.0f);
    checkConstantWidth(7, 2.5f);

    // --- 2. Diameter equals width (corollary of constant width). ---
    {
        const float w = 5.0f;
        const auto p = maz::math::reuleauxPolygon(3, w, vec2(0, 0), 64);
        float maxD = 0.0f;
        for (std::size_t i = 0; i < p.size(); ++i) {
            for (std::size_t j = i + 1; j < p.size(); ++j) {
                const float dx = p[i].x - p[j].x, dy = p[i].y - p[j].y;
                maxD = std::max(maxD, std::sqrt(dx * dx + dy * dy));
            }
        }
        CHECK(std::fabs(maxD - w) < 3e-3f, "the diameter (largest chord) equals the width");
    }

    // --- 3. Convexity: every turn has the same sign. ---
    {
        const auto p = maz::math::reuleauxPolygon(5, 3.0f, vec2(0, 0), 40);
        const std::size_t n = p.size();
        int pos = 0, neg = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const vec2 a = p[i];
            const vec2 b = p[(i + 1) % n];
            const vec2 c = p[(i + 2) % n];
            const float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            if (cross > 1e-5f) ++pos;
            else if (cross < -1e-5f) ++neg;
        }
        CHECK(pos == 0 || neg == 0, "the Reuleaux boundary is convex (all turns the same way)");
    }

    // --- 4. Even/invalid side count is bumped to a valid odd constant-width shape. ---
    {
        const float w = 4.0f;
        const auto even = maz::math::reuleauxPolygon(4, w, vec2(0, 0), 48); // -> 5 sides
        float worst = 0.0f;
        for (int i = 0; i < 180; ++i) {
            const float th = 3.14159265358979324f * static_cast<float>(i) / 180.0f;
            worst = std::max(worst, std::fabs(widthInDirection(even, th) - w));
        }
        CHECK(worst < 3e-3f, "an even side count is bumped up to a valid odd constant-width shape");
        CHECK(even.size() == maz::math::reuleauxPolygon(5, w, vec2(0, 0), 48).size(),
              "sides=4 produces the same point count as sides=5");
    }

    // --- 5. Determinism. ---
    {
        CHECK(maz::math::reuleauxPolygon(7, 2.0f) == maz::math::reuleauxPolygon(7, 2.0f),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("reuleaux: OK — constant width, diameter=width, convex, odd-bump, determinism.\n");
        return 0;
    }
    std::printf("reuleaux: %d failure(s).\n", g_fail);
    return 1;
}
