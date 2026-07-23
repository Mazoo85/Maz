// tests/math/alphashape.cpp — verifies alpha shapes / concave hull (math AlphaShape.hpp).
// Ground truths, deterministic (fixed point grids, no <random>, no clock):
//   * with a huge alpha the concave hull degenerates to the CONVEX hull — same enclosed area;
//   * with a notch cut into a filled square and a moderate alpha, the concave hull CARVES the notch:
//     its area drops below the convex area, yet still covers most of the shape;
//   * a probe point sitting in the carved notch is INSIDE the convex hull but OUTSIDE the concave hull,
//     while a probe in the solid body is inside BOTH — the defining concavity-recovery property;
//   * determinism.
#include "maz/math/AlphaShape.hpp"
#include "maz/math/Geometry2D.hpp" // convexHull

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Shoelace area (absolute) of a polygon given as ordered vertices.
static double polyArea(const std::vector<vec2>& poly) {
    double a2 = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const vec2& p = poly[i];
        const vec2& q = poly[(i + 1) % poly.size()];
        a2 += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
    }
    return std::fabs(a2) * 0.5;
}

// Even-odd ray-cast point-in-polygon (strict interior points only).
static bool inside(const std::vector<vec2>& poly, float px, float py) {
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const float xi = poly[i].x, yi = poly[i].y, xj = poly[j].x, yj = poly[j].y;
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            in = !in;
        }
    }
    return in;
}

static std::vector<vec2> ringToPoly(const std::vector<vec2>& pts, const std::vector<std::uint32_t>& ring) {
    std::vector<vec2> poly;
    for (std::uint32_t i : ring) poly.push_back(pts[i]);
    return poly;
}

int main() {
    // Filled 11x11 unit grid (spacing 1), convex hull is the 10x10 square (area 100).
    std::vector<vec2> full;
    for (int y = 0; y <= 10; ++y)
        for (int x = 0; x <= 10; ++x) full.push_back(vec2(static_cast<float>(x), static_cast<float>(y)));

    // --- 1. Huge alpha -> convex hull (same enclosed area). ---
    {
        const std::vector<std::uint32_t> ring = maz::math::concaveHull(full, 1e9f);
        CHECK(ring.size() >= 4, "huge-alpha concave hull is a valid ring");
        const double ca = polyArea(ringToPoly(full, ring));
        const double convexA = polyArea(maz::math::convexHull(full));
        CHECK(std::fabs(convexA - 100.0) < 1e-3, "convex hull of the grid is the 10x10 square (area 100)");
        CHECK(std::fabs(ca - convexA) < 1e-3, "huge-alpha concave hull area equals the convex hull area");
    }

    // Same grid with a 3-wide, 5-tall notch cut from the top-centre (x in {4,5,6}, y >= 6 removed).
    std::vector<vec2> notched;
    for (int y = 0; y <= 10; ++y)
        for (int x = 0; x <= 10; ++x)
            if (!(x >= 4 && x <= 6 && y >= 6))
                notched.push_back(vec2(static_cast<float>(x), static_cast<float>(y)));

    const double convexA = polyArea(maz::math::convexHull(notched));
    CHECK(std::fabs(convexA - 100.0) < 1e-3, "convex hull ignores the notch (still area 100)");

    // --- 2. Moderate alpha carves the notch. ---
    {
        const std::vector<std::uint32_t> ring = maz::math::concaveHull(notched, 1.5f);
        CHECK(ring.size() >= 4, "concave hull of the notched shape is a valid single loop");
        const std::vector<vec2> poly = ringToPoly(notched, ring);
        const double ca = polyArea(poly);
        CHECK(ca < convexA - 8.0, "concave hull area is well below the convex area (notch removed)");
        CHECK(ca > 70.0, "concave hull still covers most of the square (only the notch is gone)");

        // --- 3. Concavity recovery via probe points. ---
        // (5, 9) sits in the carved notch: inside the convex square, outside the concave hull.
        const std::vector<vec2> convex = maz::math::convexHull(notched);
        CHECK(inside(convex, 5.0f, 9.0f), "notch probe is inside the convex hull");
        CHECK(!inside(poly, 5.0f, 9.0f), "notch probe is OUTSIDE the concave hull (concavity recovered)");
        // (5, 2) sits in the solid body: inside both.
        CHECK(inside(convex, 5.0f, 2.0f) && inside(poly, 5.0f, 2.0f),
              "solid-body probe is inside both hulls");
    }

    // --- 4. Determinism. ---
    {
        const std::vector<std::uint32_t> a = maz::math::concaveHull(notched, 1.5f);
        const std::vector<std::uint32_t> b = maz::math::concaveHull(notched, 1.5f);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i] != b[i]) same = false;
        CHECK(same, "identical inputs produce identical concave hulls");
    }

    if (g_fail == 0) {
        std::printf("alphashape: OK — convex limit, notch carving, concavity recovery, determinism.\n");
        return 0;
    }
    std::printf("alphashape: %d failure(s).\n", g_fail);
    return 1;
}
