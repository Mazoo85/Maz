// tests/math/inversebilinear.cpp — verifies inverse bilinear mapping (math InverseBilinear.hpp).
// Ground truths, deterministic:
//   * the unit square maps a point to itself (P=(u,v));
//   * the four corners recover exactly (0,0),(1,0),(1,1),(0,1) and the centre recovers (0.5,0.5);
//   * for a general (non-parallelogram) trapezoid, forward-then-inverse round-trips (u,v) — this
//     exercises the quadratic-solve branch;
//   * a point clearly outside the quad is flagged invalid.
#include "maz/math/InverseBilinear.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::bilinear;
using maz::math::invBilinear;
using maz::math::InvBilinearResult;
using maz::math::vec2;

static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Unit square: identity map. ---
    {
        const vec2 A(0, 0), B(1, 0), C(1, 1), D(0, 1);
        const InvBilinearResult r = invBilinear(vec2(0.3f, 0.7f), A, B, C, D);
        CHECK(r.valid, "point inside the unit square is valid");
        CHECK(near(r.uv.x, 0.3f) && near(r.uv.y, 0.7f), "unit square maps a point to itself");
    }

    // --- 2. Corners and centre of an arbitrary (convex) quad. ---
    {
        const vec2 A(2, 1), B(6, 2), C(5, 5), D(1, 4);
        CHECK(near(invBilinear(A, A, B, C, D).uv.x, 0) && near(invBilinear(A, A, B, C, D).uv.y, 0),
              "corner A -> (0,0)");
        CHECK(near(invBilinear(B, A, B, C, D).uv.x, 1) && near(invBilinear(B, A, B, C, D).uv.y, 0),
              "corner B -> (1,0)");
        CHECK(near(invBilinear(C, A, B, C, D).uv.x, 1) && near(invBilinear(C, A, B, C, D).uv.y, 1),
              "corner C -> (1,1)");
        CHECK(near(invBilinear(D, A, B, C, D).uv.x, 0) && near(invBilinear(D, A, B, C, D).uv.y, 1),
              "corner D -> (0,1)");
        const vec2 centre(0.25f * (A.x + B.x + C.x + D.x), 0.25f * (A.y + B.y + C.y + D.y));
        const InvBilinearResult rc = invBilinear(centre, A, B, C, D);
        CHECK(near(rc.uv.x, 0.5f) && near(rc.uv.y, 0.5f), "the average of the corners -> (0.5,0.5)");
    }

    // --- 3. Round trip on a trapezoid (non-parallelogram -> quadratic branch). ---
    {
        const vec2 A(0, 0), B(4, 0), C(3, 3), D(1, 3); // top edge shorter than bottom -> not a parallelogram
        bool ok = true;
        for (float v = 0.1f; v <= 0.9f; v += 0.2f) {
            for (float u = 0.1f; u <= 0.9f; u += 0.2f) {
                const vec2 p = bilinear(A, B, C, D, u, v);
                const InvBilinearResult r = invBilinear(p, A, B, C, D);
                if (!r.valid || !near(r.uv.x, u, 2e-3f) || !near(r.uv.y, v, 2e-3f)) ok = false;
            }
        }
        CHECK(ok, "forward-then-inverse round-trips (u,v) on a trapezoid");
    }

    // --- 4. A point outside the quad is flagged invalid. ---
    {
        const vec2 A(0, 0), B(1, 0), C(1, 1), D(0, 1);
        const InvBilinearResult r = invBilinear(vec2(5.0f, 5.0f), A, B, C, D);
        CHECK(!r.valid, "a point well outside the quad is invalid");
    }

    if (g_fail == 0) {
        std::printf("inversebilinear: OK — identity, corners+centre, trapezoid round trip, outside "
                    "invalid.\n");
        return 0;
    }
    std::printf("inversebilinear: %d failure(s).\n", g_fail);
    return 1;
}
