// Unit tests for maz::math::convexHull2D — the 2D convex hull via Andrew's
// monotone chain. Inputs are integer-valued vec2 so the float coordinates are
// exact (== comparisons and the cross-product turn test are exact), letting the
// tests pin the hull by set membership + size + a counter-clockwise validity
// check, plus one deterministic exact-sequence assertion. Pure C++, no GPU/display.

#include "maz/math/ConvexHull.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Any hull vertex matches (x, y) exactly (integer-valued inputs -> exact float).
bool hasVertex(const std::vector<vec2>& h, float x, float y) {
    return std::any_of(h.begin(), h.end(),
                       [x, y](vec2 v) { return v.x == x && v.y == y; });
}

// Every consecutive triple (incl. wrap-around) is a left turn (strictly CCW), or
// the hull has fewer than 3 vertices. Uses the same cross formula as the hull.
bool isConvexCCW(const std::vector<vec2>& h) {
    if (h.size() < 3) {
        return true;
    }
    auto cross = [](vec2 o, vec2 a, vec2 b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    const std::size_t n = h.size();
    for (std::size_t i = 0; i < n; ++i) {
        const vec2 o = h[i];
        const vec2 a = h[(i + 1) % n];
        const vec2 b = h[(i + 2) % n];
        if (cross(o, a, b) <= 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    // --- 1. SQUARE + INTERIOR -> 4 CORNERS -----------------------------------
    {
        std::vector<vec2> h = convexHull2D(
            {vec2(0, 0), vec2(4, 0), vec2(4, 4), vec2(0, 4), vec2(2, 2)});
        check(h.size() == 4, "square+interior -> hull size 4");
        check(hasVertex(h, 0, 0) && hasVertex(h, 4, 0) && hasVertex(h, 4, 4) &&
                  hasVertex(h, 0, 4),
              "square+interior -> all four corners present");
        check(!hasVertex(h, 2, 2), "square+interior -> interior (2,2) excluded");
        check(isConvexCCW(h), "square+interior -> hull is convex CCW");
    }

    // --- 2. SQUARE + INTERIOR + EDGE-MIDPOINTS -> STILL 4 CORNERS -------------
    {
        std::vector<vec2> h = convexHull2D({vec2(0, 0), vec2(4, 0), vec2(4, 4),
                                            vec2(0, 4), vec2(2, 0), vec2(4, 2),
                                            vec2(2, 4), vec2(0, 2), vec2(1, 1),
                                            vec2(3, 3)});
        check(h.size() == 4, "square+midpoints -> hull STILL size 4 (collinear excluded)");
        check(hasVertex(h, 0, 0) && hasVertex(h, 4, 0) && hasVertex(h, 4, 4) &&
                  hasVertex(h, 0, 4),
              "square+midpoints -> four corners present");
        check(!hasVertex(h, 2, 0) && !hasVertex(h, 4, 2) && !hasVertex(h, 2, 4) &&
                  !hasVertex(h, 0, 2),
              "square+midpoints -> edge midpoints excluded");
        check(isConvexCCW(h), "square+midpoints -> hull is convex CCW");
    }

    // --- 3. TRIANGLE ---------------------------------------------------------
    {
        std::vector<vec2> h = convexHull2D({vec2(0, 0), vec2(4, 0), vec2(2, 3)});
        check(h.size() == 3, "triangle -> hull size 3");
        check(hasVertex(h, 0, 0) && hasVertex(h, 4, 0) && hasVertex(h, 2, 3),
              "triangle -> all three vertices present");
        check(isConvexCCW(h), "triangle -> hull is convex CCW");
    }

    // --- 4. COLLINEAR POINTS -> 2 EXTREMES -----------------------------------
    {
        std::vector<vec2> h =
            convexHull2D({vec2(0, 0), vec2(1, 0), vec2(2, 0), vec2(3, 0)});
        check(h.size() == 2, "all-collinear -> hull size 2 (the extremes)");
        check(hasVertex(h, 0, 0) && hasVertex(h, 3, 0),
              "all-collinear -> the two segment endpoints");
        check(!hasVertex(h, 1, 0) && !hasVertex(h, 2, 0),
              "all-collinear -> interior points excluded");
    }

    // --- 5. DUPLICATES -------------------------------------------------------
    {
        std::vector<vec2> h = convexHull2D({vec2(0, 0), vec2(0, 0), vec2(4, 0),
                                            vec2(4, 0), vec2(4, 4), vec2(0, 4),
                                            vec2(0, 4)});
        check(h.size() == 4, "duplicates -> hull size 4 (dupes removed)");
        check(hasVertex(h, 0, 0) && hasVertex(h, 4, 0) && hasVertex(h, 4, 4) &&
                  hasVertex(h, 0, 4),
              "duplicates -> the square corners");
        check(isConvexCCW(h), "duplicates -> hull is convex CCW");
    }

    // --- 6. DEGENERATE -------------------------------------------------------
    {
        std::vector<vec2> empty = convexHull2D({});
        check(empty.size() == 0, "empty -> hull size 0");

        std::vector<vec2> one = convexHull2D({vec2(1, 1)});
        check(one.size() == 1 && hasVertex(one, 1, 1), "single point -> hull size 1");

        std::vector<vec2> two = convexHull2D({vec2(0, 0), vec2(1, 1)});
        check(two.size() == 2 && hasVertex(two, 0, 0) && hasVertex(two, 1, 1),
              "two distinct -> hull size 2 (a segment)");

        std::vector<vec2> same = convexHull2D({vec2(5, 5), vec2(5, 5)});
        check(same.size() == 1 && hasVertex(same, 5, 5),
              "two identical -> hull size 1 (deduped)");
    }

    // --- 7. CCW ORDER + FIRST VERTEX -----------------------------------------
    {
        std::vector<vec2> h = convexHull2D(
            {vec2(0, 0), vec2(4, 0), vec2(4, 4), vec2(0, 4), vec2(2, 2)});
        check(h.size() == 4, "CCW square -> hull size 4");
        // Andrew's is deterministic: CCW starting at the lexicographically-smallest
        // point (0,0) -> (4,0) -> (4,4) -> (0,4).
        check(h.size() == 4 && h[0].x == 0 && h[0].y == 0 && h[1].x == 4 &&
                  h[1].y == 0 && h[2].x == 4 && h[2].y == 4 && h[3].x == 0 &&
                  h[3].y == 4,
              "CCW square -> exact sequence (0,0),(4,0),(4,4),(0,4)");
        check(isConvexCCW(h), "CCW square -> hull is convex CCW");
    }

    // --- 8. LARGER SET: PENTAGON OF EXTREMES + INTERIOR ----------------------
    {
        // Convex pentagon extremes: (-1,3),(0,0),(4,0),(5,3),(2,5) with four
        // points clustered near the centroid (~2,2.2) that must be excluded.
        std::vector<vec2> h = convexHull2D({vec2(0, 0), vec2(4, 0), vec2(5, 3),
                                            vec2(2, 5), vec2(-1, 3), vec2(2, 2),
                                            vec2(2, 3), vec2(1, 3), vec2(3, 2)});
        check(h.size() == 5, "pentagon+interior -> hull size 5");
        check(hasVertex(h, -1, 3) && hasVertex(h, 0, 0) && hasVertex(h, 4, 0) &&
                  hasVertex(h, 5, 3) && hasVertex(h, 2, 5),
              "pentagon+interior -> all five extreme vertices present");
        check(!hasVertex(h, 2, 2) && !hasVertex(h, 2, 3) && !hasVertex(h, 1, 3) &&
                  !hasVertex(h, 3, 2),
              "pentagon+interior -> interior points excluded");
        check(isConvexCCW(h), "pentagon+interior -> hull is convex CCW");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
