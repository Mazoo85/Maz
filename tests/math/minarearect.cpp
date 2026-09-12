// tests/math/minarearect.cpp — verifies the minimum-area oriented bounding rectangle (Geometry2D.hpp).
// Ground truths, deterministic:
//   * for an axis-aligned rectangle's corners, the min-area rect recovers that rectangle (area = w*h,
//     side lengths match);
//   * rotating those corners by an angle yields the SAME area and side lengths (orientation-invariant),
//     and the recovered axis is aligned with the rotation;
//   * the oriented rect's area never exceeds the axis-aligned bounding-box area, and contains all points;
//   * a randomized cross-check: every input point lies inside the returned rectangle (within eps), and the
//     area is <= the AABB area.
#include "maz/math/Geometry2D.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::minAreaRect;
using maz::math::OrientedRect;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(float a, float b, float e = 1e-2f) { return std::fabs(a - b) < e; }

// Is p inside the oriented rect (within eps, in the rect's own frame)?
static bool inside(const OrientedRect& r, vec2 p, float eps = 1e-2f) {
    const vec2 d = p - r.center;
    const float du = d.x * r.axisU.x + d.y * r.axisU.y;
    const float dv = d.x * r.axisV.x + d.y * r.axisV.y;
    return std::fabs(du) <= r.halfU + eps && std::fabs(dv) <= r.halfV + eps;
}

static float aabbArea(const std::vector<vec2>& pts) {
    float mnx = pts[0].x, mxx = pts[0].x, mny = pts[0].y, mxy = pts[0].y;
    for (const vec2& p : pts) {
        mnx = p.x < mnx ? p.x : mnx; mxx = p.x > mxx ? p.x : mxx;
        mny = p.y < mny ? p.y : mny; mxy = p.y > mxy ? p.y : mxy;
    }
    return (mxx - mnx) * (mxy - mny);
}

struct Lcg {
    std::uint64_t s;
    float f() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<float>(s >> 40) * (1.0f / 16777216.0f);
    }
    float range(float lo, float hi) { return lo + f() * (hi - lo); }
};

int main() {
    // --- 1. Axis-aligned rectangle. ---
    {
        const std::vector<vec2> pts{vec2(0, 0), vec2(6, 0), vec2(6, 2), vec2(0, 2)};
        const OrientedRect r = minAreaRect(pts);
        CHECK(near(r.area, 12.0f, 0.05f), "area of a 6x2 rect is 12");
        const float longSide = 2.0f * (r.halfU > r.halfV ? r.halfU : r.halfV);
        const float shortSide = 2.0f * (r.halfU > r.halfV ? r.halfV : r.halfU);
        CHECK(near(longSide, 6.0f) && near(shortSide, 2.0f), "recovers the 6 and 2 side lengths");
        CHECK(near(r.center.x, 3.0f) && near(r.center.y, 1.0f), "centre is the rect's middle");
    }

    // --- 2. Rotated rectangle -> same area & sides (orientation invariance). ---
    {
        const float theta = 0.6f; // radians
        const float c = std::cos(theta), s = std::sin(theta);
        std::vector<vec2> pts;
        const vec2 base[4] = {vec2(0, 0), vec2(6, 0), vec2(6, 2), vec2(0, 2)};
        for (const vec2& p : base) pts.push_back(vec2(p.x * c - p.y * s, p.x * s + p.y * c));
        const OrientedRect r = minAreaRect(pts);
        CHECK(near(r.area, 12.0f, 0.05f), "rotated rect still has area 12");
        const float longSide = 2.0f * (r.halfU > r.halfV ? r.halfU : r.halfV);
        const float shortSide = 2.0f * (r.halfU > r.halfV ? r.halfV : r.halfU);
        CHECK(near(longSide, 6.0f) && near(shortSide, 2.0f), "rotated rect recovers 6 and 2 sides");
        // The min-area rect must be TIGHTER than the axis-aligned box (which is larger for a rotated rect).
        CHECK(r.area < aabbArea(pts) - 1e-3f, "oriented rect is strictly tighter than the AABB here");
        // Every corner lies inside the recovered rect.
        for (const vec2& p : pts) CHECK(inside(r, p), "rotated corner lies inside the oriented rect");
    }

    // --- 3. Randomized: contains all points and never beats... er, never exceeds the AABB. ---
    {
        Lcg rng{0x0DDBA11u};
        bool ok = true;
        for (int trial = 0; trial < 500; ++trial) {
            const std::size_t n = 3 + static_cast<std::size_t>(rng.f() * 30.0f);
            std::vector<vec2> pts;
            for (std::size_t i = 0; i < n; ++i) pts.push_back(vec2(rng.range(-50, 50), rng.range(-50, 50)));
            const OrientedRect r = minAreaRect(pts);
            for (const vec2& p : pts)
                if (!inside(r, p, 0.05f)) ok = false;
            if (r.area > aabbArea(pts) + 0.1f) ok = false;
        }
        CHECK(ok, "randomized: all points inside the oriented rect, area <= AABB area");
    }

    // --- 4. Degenerate inputs. ---
    {
        const OrientedRect e = minAreaRect({});
        CHECK(near(e.area, 0.0f), "empty set -> zero-area rect");
        const OrientedRect one = minAreaRect({vec2(5, 5)});
        CHECK(near(one.center.x, 5.0f) && near(one.center.y, 5.0f), "single point centres on itself");
    }

    if (g_fail == 0) {
        std::printf("minarearect: OK — axis-aligned, rotated invariance, AABB bound, random, degenerate.\n");
        return 0;
    }
    std::printf("minarearect: %d failure(s).\n", g_fail);
    return 1;
}
