// Unit tests for maz::math::Rect2 — a 2D axis-aligned rectangle stored as
// position (min corner) + size, the Godot Rect2 analog / 2D counterpart to iter2's
// 3D Aabb. Pins the Godot conventions: contains is half-open (inclusive min edge,
// exclusive max edge) and intersects uses exclusive borders (edge-touching rects do
// NOT overlap). Coords are integer-valued so the derived rects compare exactly.
// Pure C++, no GPU/display.

#include "maz/math/Rect2.hpp"

#include <cstdio>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Exact == on the four floats — inputs are integers so exact is fine.
bool rectEq(const Rect2& r, float px, float py, float sx, float sy) {
    return r.position.x == px && r.position.y == py &&
           r.size.x == sx && r.size.y == sy;
}

} // namespace

int main() {
    // --- 1. BASIC GETTERS ----------------------------------------------------
    {
        Rect2 r{vec2(1, 2), vec2(4, 6)};
        check(r.min() == vec2(1, 2), "min == (1,2)");
        check(r.max() == vec2(5, 8), "max == (5,8)");
        check(r.center() == vec2(3, 5), "center == (3,5)");
        check(r.area() == 24.0f, "area == 24");
        check(r.hasArea(), "hasArea true for non-empty rect");

        Rect2 empty{};
        check(!empty.hasArea(), "zero-size Rect2{} -> hasArea false");
        check(empty.area() == 0.0f, "zero-size Rect2{} -> area 0");
    }

    // --- 2. CONTAINS (HALF-OPEN: inclusive min, exclusive max) ---------------
    {
        Rect2 r{vec2(0, 0), vec2(4, 4)};
        check(r.contains(vec2(2, 2)), "contains inside point (2,2)");
        check(r.contains(vec2(0, 0)), "contains min corner (0,0) — inclusive");
        check(!r.contains(vec2(4, 4)), "excludes max corner (4,4) — exclusive");
        check(!r.contains(vec2(4, 2)), "excludes right edge (4,2) — exclusive");
        check(!r.contains(vec2(2, 4)), "excludes bottom edge (2,4) — exclusive");
        check(!r.contains(vec2(-1, 2)), "excludes point left of min (-1,2)");
        check(!r.contains(vec2(2, 5)), "excludes point below max (2,5)");
    }

    // --- 3. INTERSECTS (EXCLUSIVE BORDERS: edge-touching does NOT count) ------
    {
        Rect2 a{vec2(0, 0), vec2(2, 2)};
        Rect2 b{vec2(1, 1), vec2(2, 2)};
        Rect2 c{vec2(3, 3), vec2(2, 2)};
        Rect2 d{vec2(2, 0), vec2(2, 2)};
        check(a.intersects(b), "overlapping rects intersect");
        check(!a.intersects(c), "disjoint rects do not intersect");
        check(!a.intersects(d), "edge-touching (shared x=2) do NOT intersect — exclusive borders");
        check(a.intersects(b) == b.intersects(a), "intersects is symmetric");
        check(a.intersects(d) == d.intersects(a), "intersects is symmetric for the edge-touching case");
    }

    // --- 4. INTERSECTION -----------------------------------------------------
    {
        Rect2 a{vec2(0, 0), vec2(4, 4)};
        Rect2 b{vec2(2, 2), vec2(4, 4)};
        check(rectEq(a.intersection(b), 2, 2, 2, 2), "intersection overlap [2,4]x[2,4] == {(2,2),(2,2)}");

        Rect2 c{vec2(5, 5), vec2(2, 2)};
        Rect2 aa{vec2(0, 0), vec2(2, 2)};
        check(!aa.intersection(c).hasArea(), "disjoint intersection is zero-size (no area)");
    }

    // --- 5. MERGE ------------------------------------------------------------
    {
        Rect2 a{vec2(0, 0), vec2(2, 2)};
        Rect2 b{vec2(3, 3), vec2(1, 1)};
        check(rectEq(a.merge(b), 0, 0, 4, 4), "merge bounding [0,4]x[0,4] == {(0,0),(4,4)}");
        check(rectEq(a.merge(b), b.merge(a).position.x, b.merge(a).position.y,
                     b.merge(a).size.x, b.merge(a).size.y), "merge is symmetric");
    }

    // --- 6. EXPAND -----------------------------------------------------------
    {
        Rect2 r{vec2(1, 1), vec2(2, 2)}; // covers [1,3]x[1,3]
        check(rectEq(r.expand(vec2(5, 5)), 1, 1, 4, 4), "expand to (5,5) -> [1,5]x[1,5]");
        check(rectEq(r.expand(vec2(0, 0)), 0, 0, 3, 3), "expand to (0,0) -> [0,3]x[0,3]");
        check(rectEq(r.expand(vec2(2, 2)), 1, 1, 2, 2), "expand to inside point unchanged");
    }

    // --- 7. GROW -------------------------------------------------------------
    {
        Rect2 r{vec2(2, 2), vec2(4, 4)};
        check(rectEq(r.grow(1.0f), 1, 1, 6, 6), "grow(1) inflates all sides -> {(1,1),(6,6)}");
        check(rectEq(r.grow(-1.0f), 3, 3, 2, 2), "grow(-1) shrinks -> {(3,3),(2,2)}");
        check(rectEq(r.grow(0.0f), 2, 2, 4, 4), "grow(0) unchanged");
    }

    // --- 8. fromMinMax -------------------------------------------------------
    {
        check(rectEq(Rect2::fromMinMax(vec2(1, 2), vec2(5, 8)), 1, 2, 4, 6),
              "fromMinMax((1,2),(5,8)) == {(1,2),(4,6)}");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
