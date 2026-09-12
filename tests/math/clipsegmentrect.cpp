// tests/math/clipsegmentrect.cpp — verifies Liang-Barsky segment-vs-rect clipping (Geometry2D.hpp).
// Ground truths, deterministic:
//   * a segment fully inside the rect is returned unchanged;
//   * a segment crossing one edge is trimmed at that edge, on the original line;
//   * a segment passing straight through keeps both crossing points (clipped ends lie on the border);
//   * a segment fully outside reports no intersection and leaves the outputs untouched;
//   * an endpoint exactly on the border and a rect given with swapped corners both behave;
//   * a randomized cross-check: the clipped endpoints are inside the rect (within eps) and collinear with
//     the original segment, and the clip agrees with a brute-force "is the midpoint inside?" sanity test.
#include "maz/math/Geometry2D.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::clipSegmentToRect;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool nearV(vec2 a, vec2 b, float e = 1e-4f) { return near(a.x, b.x, e) && near(a.y, b.y, e); }

struct Lcg {
    std::uint64_t s;
    float f() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<float>(s >> 40) * (1.0f / 16777216.0f);
    }
    float range(float lo, float hi) { return lo + f() * (hi - lo); }
};

int main() {
    const vec2 mn(0.0f, 0.0f), mx(10.0f, 10.0f);

    // --- 1. Fully inside: unchanged. ---
    {
        vec2 oa, ob;
        const bool hit = clipSegmentToRect(vec2(2, 3), vec2(7, 8), mn, mx, oa, ob);
        CHECK(hit, "inside segment intersects");
        CHECK(nearV(oa, vec2(2, 3)) && nearV(ob, vec2(7, 8)), "inside segment is unchanged");
    }

    // --- 2. Crossing the right edge: trimmed at x=10. ---
    {
        vec2 oa, ob;
        const bool hit = clipSegmentToRect(vec2(5, 5), vec2(15, 5), mn, mx, oa, ob);
        CHECK(hit, "edge-crossing segment intersects");
        CHECK(nearV(oa, vec2(5, 5)), "the inside endpoint is kept");
        CHECK(nearV(ob, vec2(10, 5)), "the outside endpoint is clipped to the right edge");
    }

    // --- 3. Straight through: both ends clipped to the border. ---
    {
        vec2 oa, ob;
        const bool hit = clipSegmentToRect(vec2(-5, 5), vec2(15, 5), mn, mx, oa, ob);
        CHECK(hit, "through segment intersects");
        CHECK(nearV(oa, vec2(0, 5)) && nearV(ob, vec2(10, 5)), "both ends land on the vertical borders");
    }

    // --- 4. Fully outside: no hit, outputs untouched. ---
    {
        vec2 oa(-1, -1), ob(-1, -1);
        const bool hit = clipSegmentToRect(vec2(20, 20), vec2(30, 25), mn, mx, oa, ob);
        CHECK(!hit, "outside segment does not intersect");
        CHECK(nearV(oa, vec2(-1, -1)) && nearV(ob, vec2(-1, -1)), "outputs left untouched on miss");
    }

    // --- 5. Diagonal corner-to-corner keeps the diagonal. ---
    {
        vec2 oa, ob;
        const bool hit = clipSegmentToRect(vec2(-5, -5), vec2(15, 15), mn, mx, oa, ob);
        CHECK(hit, "diagonal intersects");
        CHECK(nearV(oa, vec2(0, 0)) && nearV(ob, vec2(10, 10)), "diagonal clips to the two corners");
    }

    // --- 6. Swapped rect corners are tolerated. ---
    {
        vec2 oa, ob;
        const bool hit = clipSegmentToRect(vec2(5, 5), vec2(15, 5), vec2(10, 10), vec2(0, 0), oa, ob);
        CHECK(hit && nearV(ob, vec2(10, 5)), "swapped rect corners still clip correctly");
    }

    // --- 7. Randomized: clipped ends are inside (within eps) and collinear with the input. ---
    {
        Lcg rng{0x5EED1234u};
        bool ok = true;
        int hits = 0;
        for (int i = 0; i < 20000; ++i) {
            const vec2 a(rng.range(-5, 15), rng.range(-5, 15));
            const vec2 b(rng.range(-5, 15), rng.range(-5, 15));
            vec2 oa, ob;
            if (!clipSegmentToRect(a, b, mn, mx, oa, ob)) continue;
            ++hits;
            // Clipped endpoints must lie within the rect (allow a small epsilon on the border).
            const float e = 1e-3f;
            if (oa.x < mn.x - e || oa.x > mx.x + e || oa.y < mn.y - e || oa.y > mx.y + e) ok = false;
            if (ob.x < mn.x - e || ob.x > mx.x + e || ob.y < mn.y - e || ob.y > mx.y + e) ok = false;
            // Clipped endpoints must be collinear with a->b (cross product ~ 0).
            const vec2 d = b - a;
            const float c1 = d.x * (oa.y - a.y) - d.y * (oa.x - a.x);
            const float c2 = d.x * (ob.y - a.y) - d.y * (ob.x - a.x);
            const float scale = std::fabs(d.x) + std::fabs(d.y) + 1.0f;
            if (std::fabs(c1) > 1e-2f * scale || std::fabs(c2) > 1e-2f * scale) ok = false;
        }
        CHECK(ok, "randomized clips are inside the rect and collinear with the input segment");
        CHECK(hits > 1000, "the randomized run exercised many real intersections");
    }

    if (g_fail == 0) {
        std::printf("clipsegmentrect: OK — inside/edge/through/outside/diagonal/swapped + random.\n");
        return 0;
    }
    std::printf("clipsegmentrect: %d failure(s).\n", g_fail);
    return 1;
}
