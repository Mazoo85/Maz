// tests/math/orientedrect2.cpp — verifies the oriented 2D rectangle (math::OrientedRect2).
// Ground truths, exact 2D geometry, deterministic:
//   * an axis-aligned OBB contains exactly the points an AABB would;
//   * a rotated OBB accepts/rejects points the AABB gets wrong (a 90deg-rotated 2x1 box is 1 wide, 2 tall);
//   * corners are the rotated rectangle vertices;
//   * two identical / clearly-overlapping boxes overlap; far-apart ones don't (SAT separating axis);
//   * a rotated box tucked near a corner overlaps where its diagonal reaches, and a small gap separates.
#include "maz/math/OrientedRect2.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::OrientedRect2;
using maz::math::orientedRectsOverlap;
using maz::math::vec2;

static const float kPi = 3.14159265358979324f;
static bool vnear(const vec2& a, const vec2& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e;
}

int main() {
    // --- 1. Axis-aligned box behaves like an AABB. ---
    {
        OrientedRect2 r;
        r.center = vec2(0, 0);
        r.halfExtents = vec2(2, 1);
        r.rotation = 0.0f;
        CHECK(r.containsPoint(vec2(1.9f, 0.9f)), "inside the AABB");
        CHECK(!r.containsPoint(vec2(2.1f, 0)), "outside in x");
        CHECK(!r.containsPoint(vec2(0, 1.1f)), "outside in y");
    }

    // --- 2. Rotated box: 90deg turns a 2-wide/1-tall box into 1-wide/2-tall. ---
    {
        OrientedRect2 r;
        r.center = vec2(0, 0);
        r.halfExtents = vec2(2, 1);
        r.rotation = kPi * 0.5f; // 90 degrees
        CHECK(r.containsPoint(vec2(0, 1.9f)), "now extends ~2 in y");
        CHECK(!r.containsPoint(vec2(1.5f, 0)), "now only ~1 wide in x");
        // A point the AABB (2x1) would accept but the rotated box rejects:
        CHECK(!r.containsPoint(vec2(1.5f, 0.5f)), "AABB would accept this, rotated box rejects");
    }

    // --- 3. Corners of an axis-aligned box. ---
    {
        OrientedRect2 r;
        r.center = vec2(1, 1);
        r.halfExtents = vec2(1, 1);
        r.rotation = 0.0f;
        const auto c = r.corners();
        CHECK(vnear(c[0], vec2(0, 0)) && vnear(c[2], vec2(2, 2)), "opposite corners");
    }

    // --- 4. Overlap: identical and separated. ---
    {
        OrientedRect2 a;
        a.halfExtents = vec2(1, 1);
        OrientedRect2 b = a;
        CHECK(orientedRectsOverlap(a, b), "identical boxes overlap");
        b.center = vec2(3, 0);
        CHECK(!orientedRectsOverlap(a, b), "far-apart boxes do not overlap");
        b.center = vec2(1.5f, 0);
        CHECK(orientedRectsOverlap(a, b), "close boxes overlap");
    }

    // --- 5. Rotated box near a corner: diagonal reach. ---
    {
        OrientedRect2 a;
        a.center = vec2(0, 0);
        a.halfExtents = vec2(1, 1); // reaches x=1
        OrientedRect2 b;
        b.halfExtents = vec2(1, 1);
        b.rotation = kPi * 0.25f;    // 45deg: its corner reaches sqrt(2) ~ 1.414 toward center
        b.center = vec2(2.0f, 0);    // nearest corner at x = 2 - 1.414 = 0.586 < 1 -> overlap
        CHECK(orientedRectsOverlap(a, b), "rotated box's diagonal corner overlaps A");
        b.center = vec2(2.5f, 0);    // nearest corner at 2.5 - 1.414 = 1.086 > 1 -> gap
        CHECK(!orientedRectsOverlap(a, b), "a small gap separates them");
    }

    // --- 6. projectedRadius sanity: axis-aligned box onto X is its half-width. ---
    {
        OrientedRect2 r;
        r.halfExtents = vec2(3, 2);
        CHECK(std::fabs(r.projectedRadius(vec2(1, 0)) - 3.0f) < 1e-5f, "radius onto X = half-width");
        CHECK(std::fabs(r.projectedRadius(vec2(0, 1)) - 2.0f) < 1e-5f, "radius onto Y = half-height");
    }

    if (g_fail == 0) {
        std::printf("orientedrect2: OK — AABB match, rotated containment, corners, overlap, diagonal "
                    "reach, projected radius.\n");
        return 0;
    }
    std::printf("orientedrect2: %d failure(s).\n", g_fail);
    return 1;
}
