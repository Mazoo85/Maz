// tests/math/dualcontour2d.cpp — verifies 2D dual contouring (math DualContour2D.hpp).
// Ground truths, deterministic (fixed SDF grids, no <random>, no clock):
//   * SHARP CORNERS: on a box SDF, dual contouring places a vertex essentially ON each true corner, while
//     marching squares (which can only use edge midpoints) stays far from it — the defining feature;
//   * a circle SDF's contour vertices lie on the circle;
//   * a field with no sign change yields no contour;
//   * every segment endpoint is a valid finite point inside the grid;
//   * determinism.
#include "maz/math/DualContour2D.hpp"
#include "maz/math/MarchingSquares.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float dist(const vec2& a, const vec2& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    const int n = 16;

    // Axis-aligned box SDF with corners at non-grid positions (3.4 .. 11.6).
    const float lo = 3.4f, hi = 11.6f;
    auto boxSdf = [&](float x, float y) {
        // Distance to axis-aligned box [lo,hi]^2 (negative inside).
        const float cxv = (lo + hi) * 0.5f, cyv = (lo + hi) * 0.5f, hx = (hi - lo) * 0.5f, hy = (hi - lo) * 0.5f;
        const float dx = std::fabs(x - cxv) - hx, dy = std::fabs(y - cyv) - hy;
        const float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
        return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f);
    };
    std::vector<float> box(static_cast<std::size_t>(n * n));
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            box[static_cast<std::size_t>(y * n + x)] = boxSdf(static_cast<float>(x), static_cast<float>(y));

    // --- 1. Sharp-corner recovery vs marching squares. ---
    {
        const auto dc = maz::math::dualContour2D(boxSdf, n, n, 0.0f);
        const auto ms = maz::math::marchingSquares(box, n, n, 0.0f);
        CHECK(!dc.empty(), "dual contouring produces a contour for the box");

        const vec2 corners[4] = {{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}};
        for (int k = 0; k < 4; ++k) {
            float dcMin = 1e30f, msMin = 1e30f;
            for (const auto& s : dc) { dcMin = std::min(dcMin, dist(s.a, corners[k])); dcMin = std::min(dcMin, dist(s.b, corners[k])); }
            for (const auto& s : ms) { msMin = std::min(msMin, dist(s.a, corners[k])); msMin = std::min(msMin, dist(s.b, corners[k])); }
            CHECK(dcMin < 0.15f, "dual contouring places a vertex on the sharp corner");
            CHECK(msMin > 0.4f, "marching squares cannot reach the corner (it rounds it)");
            CHECK(dcMin < msMin * 0.4f, "dual contouring is far closer to the corner than marching squares");
        }
    }

    // --- 2. Circle SDF: vertices lie on the circle. ---
    {
        const vec2 c(8.0f, 8.0f);
        const float R = 5.0f;
        auto circSdf = [&](float x, float y) { return dist(vec2(x, y), c) - R; };
        const auto dc = maz::math::dualContour2D(circSdf, n, n, 0.0f);
        CHECK(!dc.empty(), "the circle produces a contour");
        float maxOff = 0.0f;
        for (const auto& s : dc) {
            maxOff = std::max(maxOff, std::fabs(dist(s.a, c) - R));
            maxOff = std::max(maxOff, std::fabs(dist(s.b, c) - R));
        }
        CHECK(maxOff < 0.6f, "every contour vertex lies on the circle");
    }

    // --- 3. Empty field. ---
    {
        auto allOutside = [](float, float) { return 1.0f; };
        CHECK(maz::math::dualContour2D(allOutside, n, n, 0.0f).empty(), "a field with no sign change has no contour");
    }

    // --- 4. Valid endpoints. ---
    {
        const auto dc = maz::math::dualContour2D(boxSdf, n, n, 0.0f);
        bool valid = true;
        for (const auto& s : dc) {
            if (!(std::isfinite(s.a.x) && std::isfinite(s.a.y) && std::isfinite(s.b.x) && std::isfinite(s.b.y))) valid = false;
            if (s.a.x < -0.01f || s.a.x > static_cast<float>(n) || s.a.y < -0.01f || s.a.y > static_cast<float>(n)) valid = false;
        }
        CHECK(valid, "every segment endpoint is finite and inside the grid");
    }

    // --- 5. Determinism. ---
    {
        const auto a = maz::math::dualContour2D(boxSdf, n, n, 0.0f);
        const auto b = maz::math::dualContour2D(boxSdf, n, n, 0.0f);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i].a.x != b[i].a.x || a[i].a.y != b[i].a.y || a[i].b.x != b[i].b.x || a[i].b.y != b[i].b.y) same = false;
        CHECK(same, "identical inputs produce identical contours");
    }

    if (g_fail == 0) {
        std::printf("dualcontour2d: OK — sharp corners vs marching squares, circle, empty, valid, determinism.\n");
        return 0;
    }
    std::printf("dualcontour2d: %d failure(s).\n", g_fail);
    return 1;
}
