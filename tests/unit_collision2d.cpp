// Unit tests for maz::math::convexPolygonsOverlap — 2D convex-polygon overlap via
// the Separating Axis Theorem. Inputs are integer-valued vec2 so the float
// coordinates and the projection compares are exact, letting the tests pin
// overlap/separation as hard booleans. Pure C++, no GPU/display.

#include "maz/math/Collision2D.hpp"

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

// A CCW axis-aligned square with lower-left corner (x, y) and side length s.
std::vector<vec2> square(float x, float y, float s) {
    return {vec2(x, y), vec2(x + s, y), vec2(x + s, y + s), vec2(x, y + s)};
}

} // namespace

int main() {
    // --- 1. OVERLAPPING SQUARES (symmetry) -----------------------------------
    {
        std::vector<vec2> a = square(0, 0, 2);
        std::vector<vec2> b = square(1, 1, 2); // overlaps in [1,2]x[1,2].
        check(convexPolygonsOverlap(a, b), "overlapping squares -> true (a,b)");
        check(convexPolygonsOverlap(b, a), "overlapping squares -> true (b,a) symmetry");
    }

    // --- 2. SEPARATED SQUARES (axis-aligned gap) -----------------------------
    {
        std::vector<vec2> a = square(0, 0, 2);
        std::vector<vec2> b = square(3, 3, 2); // gap on x and y.
        check(!convexPolygonsOverlap(a, b), "separated squares -> false (a,b)");
        check(!convexPolygonsOverlap(b, a), "separated squares -> false (b,a)");
    }

    // --- 3. EDGE-TOUCHING (inclusive) ----------------------------------------
    {
        std::vector<vec2> a = square(0, 0, 2);
        std::vector<vec2> b = square(2, 0, 2); // share the edge x=2.
        check(convexPolygonsOverlap(a, b),
              "edge-touching squares -> true (inclusive, strict < disjoint test)");
        check(convexPolygonsOverlap(b, a), "edge-touching squares -> true (b,a)");
    }

    // --- 4. CONTAINMENT ------------------------------------------------------
    {
        std::vector<vec2> big = square(0, 0, 10);
        std::vector<vec2> small = square(3, 3, 2); // fully inside.
        check(convexPolygonsOverlap(big, small), "containment -> true (a,b)");
        check(convexPolygonsOverlap(small, big), "containment -> true (b,a)");
    }

    // --- 5. TRIANGLE vs SQUARE -----------------------------------------------
    {
        std::vector<vec2> tri = {vec2(1, 1), vec2(4, 1), vec2(1, 4)}; // CCW.
        std::vector<vec2> sq = square(0, 0, 2);
        check(convexPolygonsOverlap(tri, sq), "triangle vs square overlap -> true");

        std::vector<vec2> farTri = {vec2(5, 5), vec2(8, 5), vec2(5, 8)}; // CCW, far away.
        check(!convexPolygonsOverlap(farTri, sq), "far triangle vs square -> false");
    }

    // --- 6. DIAGONAL SEPARATION (the SAT correctness anchor) ------------------
    {
        // Triangle A: x+y <= 2 (corners (0,0),(2,0),(0,2)).
        // Triangle B: x+y >= 4 (corners (3,3),(1,3),(3,1)).
        // Their AXIS-ALIGNED bounding boxes overlap (A's bbox is [0,2]x[0,2],
        // B's is [1,3]x[1,3], overlapping in [1,2]x[1,2]) — so a naive AABB test
        // says "overlap". But the shapes are SEPARATED by the line x+y=3: A lies
        // entirely on x+y<=2, B entirely on x+y>=4. SAT finds this via the
        // hypotenuse edge normal. HAND-VERIFY on axis (-2,-2) (normal of A's
        // hypotenuse edge (2,0)->(0,2)): proj = -2*(x+y). A: x+y in [0,2] ->
        // proj in [-4,0]; B: x+y in [4,6] -> proj in [-12,-8]. maxB(-8) < minA(-4)
        // -> disjoint -> separated. So the correct answer is NO overlap.
        std::vector<vec2> triA = {vec2(0, 0), vec2(2, 0), vec2(0, 2)};
        std::vector<vec2> triB = {vec2(3, 3), vec2(1, 3), vec2(3, 1)};
        check(!convexPolygonsOverlap(triA, triB),
              "diagonal separation -> false (SAT via hypotenuse normal, beats naive AABB)");
        check(!convexPolygonsOverlap(triB, triA), "diagonal separation -> false (b,a)");
    }

    // --- 7. SYMMETRY + DETERMINISM -------------------------------------------
    {
        std::vector<vec2> a = square(0, 0, 2);
        std::vector<vec2> b = square(1, 1, 2);
        std::vector<vec2> c = square(3, 3, 2);
        check(convexPolygonsOverlap(a, b) == convexPolygonsOverlap(b, a),
              "symmetry -> overlap(a,b) == overlap(b,a) [overlapping]");
        check(convexPolygonsOverlap(a, c) == convexPolygonsOverlap(c, a),
              "symmetry -> overlap(a,c) == overlap(c,a) [separated]");
        const bool r1 = convexPolygonsOverlap(a, b);
        const bool r2 = convexPolygonsOverlap(a, b);
        const bool r3 = convexPolygonsOverlap(a, b);
        check(r1 == r2 && r2 == r3, "determinism -> repeated calls identical");
    }

    // --- 8. WINDING INDEPENDENCE ---------------------------------------------
    {
        std::vector<vec2> ccw = square(0, 0, 2);
        // The same square in CW order (reverse winding).
        std::vector<vec2> cw = {vec2(0, 0), vec2(0, 2), vec2(2, 2), vec2(2, 0)};
        std::vector<vec2> other = square(1, 1, 2);
        std::vector<vec2> away = square(3, 3, 2);
        check(convexPolygonsOverlap(cw, other) == convexPolygonsOverlap(ccw, other),
              "winding independence -> CW matches CCW [overlapping]");
        check(convexPolygonsOverlap(cw, away) == convexPolygonsOverlap(ccw, away),
              "winding independence -> CW matches CCW [separated]");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
