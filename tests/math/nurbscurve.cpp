// tests/math/nurbscurve.cpp — verifies NURBS curve evaluation (math NurbsCurve.hpp).
// Ground truths, deterministic (fixed curves, no <random>, no clock):
//   * EXACT CONIC: a degree-2 rational curve with the standard circle weights traces a MATHEMATICALLY EXACT
//     circle — every sampled point is exactly radius R from the centre (the thing polynomial Bezier/B-splines
//     cannot do), for both a quarter arc and a full circle;
//   * clamped endpoints interpolate the first and last control points;
//   * with all weights = 1 the curve stays within the convex hull of its control points (B-spline behaviour);
//   * determinism.
#include "maz/math/NurbsCurve.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    const float s = 0.70710678f; // sqrt(2)/2, the circle weight

    // --- 1. Exact quarter circle (rational Bezier). ---
    {
        const float R = 5.0f;
        std::vector<vec2> ctrl{{R, 0}, {R, R}, {0, R}};
        std::vector<float> w{1.0f, s, 1.0f};
        std::vector<float> knots{0, 0, 0, 1, 1, 1};
        float maxErr = 0.0f;
        for (int i = 0; i <= 100; ++i) {
            const float u = static_cast<float>(i) / 100.0f;
            const vec2 pt = maz::math::nurbsPoint(ctrl, w, knots, 2, u);
            maxErr = std::max(maxErr, std::fabs(len(pt) - R));
        }
        CHECK(maxErr < 1e-3f, "a rational quarter arc lies exactly on the circle of radius R");
        // Endpoints interpolate the first/last control points.
        CHECK(len(maz::math::nurbsPoint(ctrl, w, knots, 2, 0.0f) - vec2(R, 0)) < 1e-4f, "u=0 hits the first control point");
        CHECK(len(maz::math::nurbsPoint(ctrl, w, knots, 2, 1.0f) - vec2(0, R)) < 1e-4f, "u=1 hits the last control point");
    }

    // --- 2. Exact full circle (9-point degree-2 NURBS). ---
    {
        const float R = 3.0f;
        std::vector<vec2> ctrl{{R, 0},  {R, R},  {0, R},  {-R, R}, {-R, 0},
                               {-R, -R}, {0, -R}, {R, -R}, {R, 0}};
        std::vector<float> w{1, s, 1, s, 1, s, 1, s, 1};
        std::vector<float> knots{0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4};
        float maxErr = 0.0f;
        for (int i = 0; i <= 400; ++i) {
            const float u = 4.0f * static_cast<float>(i) / 400.0f;
            const vec2 pt = maz::math::nurbsPoint(ctrl, w, knots, 2, u);
            maxErr = std::max(maxErr, std::fabs(len(pt) - R));
        }
        CHECK(maxErr < 1e-3f, "the 9-point NURBS traces an exact full circle");
    }

    // --- 3. Weights=1: convex-hull containment (B-spline behaviour). ---
    {
        std::vector<vec2> ctrl{{0, 0}, {1, 3}, {3, 3}, {4, 0}, {6, 2}};
        std::vector<float> w(ctrl.size(), 1.0f);
        const auto knots = maz::math::nurbsClampedKnots(static_cast<int>(ctrl.size()), 3);
        // Bounding box of the control polygon.
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (const vec2& p : ctrl) {
            minx = std::min(minx, p.x); miny = std::min(miny, p.y);
            maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y);
        }
        bool inside = true;
        for (int i = 0; i <= 100; ++i) {
            const float u = static_cast<float>(i) / 100.0f;
            const vec2 pt = maz::math::nurbsPoint(ctrl, w, knots, 3, u);
            if (pt.x < minx - 1e-3f || pt.x > maxx + 1e-3f || pt.y < miny - 1e-3f || pt.y > maxy + 1e-3f) inside = false;
        }
        CHECK(inside, "a weight-1 NURBS stays within the control polygon's bounding box");
        CHECK(len(maz::math::nurbsPoint(ctrl, w, knots, 3, 0.0f) - ctrl.front()) < 1e-4f, "clamped curve starts at P0");
        CHECK(len(maz::math::nurbsPoint(ctrl, w, knots, 3, 1.0f) - ctrl.back()) < 1e-4f, "clamped curve ends at Pn");
    }

    // --- 4. Determinism. ---
    {
        std::vector<vec2> ctrl{{0, 0}, {2, 4}, {5, 1}};
        std::vector<float> w{1.0f, 2.0f, 1.0f};
        std::vector<float> knots{0, 0, 0, 1, 1, 1};
        const vec2 a = maz::math::nurbsPoint(ctrl, w, knots, 2, 0.37f);
        const vec2 b = maz::math::nurbsPoint(ctrl, w, knots, 2, 0.37f);
        CHECK(a.x == b.x && a.y == b.y, "identical inputs produce identical points");
    }

    if (g_fail == 0) {
        std::printf("nurbscurve: OK — exact quarter/full circle, endpoints, convex hull, determinism.\n");
        return 0;
    }
    std::printf("nurbscurve: %d failure(s).\n", g_fail);
    return 1;
}
