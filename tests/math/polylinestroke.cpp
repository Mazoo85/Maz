// tests/math/polylinestroke.cpp — verifies polyline stroking to a filled outline (math PolylineStroke.hpp).
// Ground truths, deterministic (fixed paths, no <random>, no clock):
//   * STRAIGHT-LINE AREAS (airtight): a straight segment of length L stroked to half-width w is exactly the
//     rectangle of area 2*w*L with butt caps; square caps extend each end by w giving 2*w*(L+2*w); round
//     caps add two half-disks (area between 2*w*L and 2*w*L + pi*w^2);
//   * WIDTH: for a straight horizontal segment the outline sits exactly at y = +/- w;
//   * CONTAINMENT: the polyline's own vertices lie inside the stroked polygon (checked for a bent path);
//   * degenerate inputs return empty; determinism.
#include "maz/math/PolylineStroke.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;
using maz::math::StrokeCap;
using maz::math::StrokeOptions;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Shoelace absolute area of a closed polygon (vertices in order, not duplicated).
static float polyArea(const std::vector<vec2>& p) {
    float a = 0.0f;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const vec2& u = p[i];
        const vec2& v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return std::fabs(a) * 0.5f;
}

// Even-odd point-in-polygon.
static bool inside(const std::vector<vec2>& poly, const vec2& q) {
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const vec2& a = poly[i];
        const vec2& b = poly[j];
        if (((a.y > q.y) != (b.y > q.y)) &&
            (q.x < (b.x - a.x) * (q.y - a.y) / (b.y - a.y) + a.x)) {
            in = !in;
        }
    }
    return in;
}

int main() {
    const float pi = 3.14159265358979324f;
    const float L = 6.0f, w = 0.75f;
    const std::vector<vec2> seg = {vec2(0, 0), vec2(L, 0)};

    // --- 1. Straight segment, butt cap => rectangle area 2*w*L; outline at y = +/- w. ---
    {
        StrokeOptions o; o.halfWidth = w; o.cap = StrokeCap::Butt;
        const auto ring = maz::math::strokePolyline(seg, o);
        CHECK(ring.size() == 4, "a straight butt-capped stroke is a 4-vertex rectangle");
        CHECK(std::fabs(polyArea(ring) - 2.0f * w * L) < 1e-4f, "butt-cap stroke area is 2*w*L");
        float maxY = 0.0f;
        for (const auto& p : ring) maxY = std::max(maxY, std::fabs(p.y));
        CHECK(std::fabs(maxY - w) < 1e-5f, "the outline sits at y = +/- halfWidth");
    }

    // --- 2. Square cap => 2*w*(L + 2*w). ---
    {
        StrokeOptions o; o.halfWidth = w; o.cap = StrokeCap::Square;
        const auto ring = maz::math::strokePolyline(seg, o);
        CHECK(std::fabs(polyArea(ring) - 2.0f * w * (L + 2.0f * w)) < 1e-4f, "square-cap area is 2*w*(L+2*w)");
    }

    // --- 3. Round cap => between 2*w*L and 2*w*L + pi*w^2 (two half-disks, polygonal approx). ---
    {
        StrokeOptions o; o.halfWidth = w; o.cap = StrokeCap::Round; o.roundSegments = 16;
        const auto ring = maz::math::strokePolyline(seg, o);
        const float area = polyArea(ring);
        const float lo = 2.0f * w * L + 0.9f * pi * w * w; // fine approx is just under the true half-disks
        const float hi = 2.0f * w * L + pi * w * w + 1e-3f;
        CHECK(area > lo && area < hi, "round-cap area is ~2*w*L + pi*w^2 (two half-disks)");
    }

    // --- 4. Containment: the polyline's vertices are inside its stroked outline (bent path). ---
    {
        const std::vector<vec2> path = {vec2(0, 0), vec2(4, 0), vec2(4, 3), vec2(7, 3)};
        StrokeOptions o; o.halfWidth = 0.5f; o.cap = StrokeCap::Round; o.roundSegments = 8;
        const auto ring = maz::math::strokePolyline(path, o);
        CHECK(ring.size() > 6, "the bent stroke produced a non-trivial ring");
        int insideCount = 0;
        for (const auto& v : path) if (inside(ring, v)) ++insideCount;
        CHECK(insideCount == static_cast<int>(path.size()), "every centreline vertex lies inside the stroke");
        // A point far off the path is outside.
        CHECK(!inside(ring, vec2(0.0f, 5.0f)), "a point far from the path is outside the stroke");
    }

    // --- 5. Degenerate inputs + determinism. ---
    {
        StrokeOptions o; o.halfWidth = w;
        CHECK(maz::math::strokePolyline({vec2(1, 1)}, o).empty(), "a single point strokes to nothing");
        CHECK(maz::math::strokePolyline({vec2(1, 1), vec2(1, 1)}, o).empty(), "a zero-length path strokes to nothing");
        const auto a = maz::math::strokePolyline(seg, o);
        const auto b = maz::math::strokePolyline(seg, o);
        CHECK(a == b, "identical inputs produce identical outlines");
    }

    if (g_fail == 0) {
        std::printf("polylinestroke: OK — rectangle/square/round-cap areas, width, containment, determinism.\n");
        return 0;
    }
    std::printf("polylinestroke: %d failure(s).\n", g_fail);
    return 1;
}
