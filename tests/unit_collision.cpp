// Unit tests for maz::math::Collision — the Sphere primitive and sphere/ray-sphere
// intersection helpers that complement Geometry.hpp's Aabb/Ray/Plane tests. Overlap
// tests are boolean (inclusive: touching counts), so they assert directly; the
// float-valued t and closest-point results use an approx tolerance. Inputs are chosen
// exactly representable (integer/half values) so the tolerance is trivially satisfied.
// Ray dir need not be normalized — t stays in ray-direction length units. Pure C++,
// no GPU/display.

#include "maz/math/Collision.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }

bool vapprox(vec3 a, vec3 b) {
    return approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z);
}

} // namespace

int main() {
    // --- 1. Sphere::contains --------------------------------------------------
    {
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        check(s.contains(vec3(0.5f, 0.0f, 0.0f)), "contains interior point");
        check(!s.contains(vec3(2.0f, 0.0f, 0.0f)), "does not contain exterior point");
        check(s.contains(vec3(1.0f, 0.0f, 0.0f)), "contains on-surface point (inclusive)");
    }

    // --- 2. sphereSphere ------------------------------------------------------
    {
        Sphere a{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        Sphere b{vec3(3.0f, 0.0f, 0.0f), 1.0f};
        check(!sphereSphere(a, b), "sphereSphere gap rejected (dist 3 > sum 2)");

        Sphere c{vec3(0.0f, 0.0f, 0.0f), 2.0f};
        Sphere d{vec3(3.0f, 0.0f, 0.0f), 2.0f};
        check(sphereSphere(c, d), "sphereSphere overlap detected (sum 4 > 3)");

        Sphere e{vec3(0.0f, 0.0f, 0.0f), 1.5f};
        Sphere f{vec3(3.0f, 0.0f, 0.0f), 1.5f};
        check(sphereSphere(e, f), "sphereSphere touching counts (sum 3 == dist 3, inclusive)");
    }

    // --- 3. closestPointOnAabb ------------------------------------------------
    {
        Aabb box{vec3(-1.0f, -1.0f, -1.0f), vec3(1.0f, 1.0f, 1.0f)};
        check(vapprox(closestPointOnAabb(vec3(5.0f, 0.0f, 0.0f), box), vec3(1.0f, 0.0f, 0.0f)),
              "closestPointOnAabb clamps exterior to face");
        check(vapprox(closestPointOnAabb(vec3(0.0f, 0.0f, 0.0f), box), vec3(0.0f, 0.0f, 0.0f)),
              "closestPointOnAabb of interior point is itself");
        check(vapprox(closestPointOnAabb(vec3(-3.0f, 2.0f, 0.0f), box), vec3(-1.0f, 1.0f, 0.0f)),
              "closestPointOnAabb clamps corner region");
    }

    // --- 4. sphereAabb --------------------------------------------------------
    {
        Aabb box{vec3(-1.0f, -1.0f, -1.0f), vec3(1.0f, 1.0f, 1.0f)};
        check(sphereAabb(Sphere{vec3(2.0f, 0.0f, 0.0f), 1.5f}, box),
              "sphereAabb overlap (dist 1 <= 1.5)");
        check(!sphereAabb(Sphere{vec3(3.0f, 0.0f, 0.0f), 1.0f}, box),
              "sphereAabb gap rejected (dist 2 > 1)");
        check(sphereAabb(Sphere{vec3(0.0f, 0.0f, 0.0f), 0.1f}, box),
              "sphereAabb centre inside box overlaps");
    }

    // --- 5. spherePlane -------------------------------------------------------
    {
        Plane y0 = Plane::fromPointNormal(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f)); // y == 0
        check(!spherePlane(Sphere{vec3(0.0f, 2.0f, 0.0f), 1.0f}, y0),
              "spherePlane above plane rejected (dist 2 > 1)");
        check(spherePlane(Sphere{vec3(0.0f, 2.0f, 0.0f), 2.5f}, y0),
              "spherePlane reaches plane (dist 2 <= 2.5)");
        check(spherePlane(Sphere{vec3(0.0f, 0.5f, 0.0f), 1.0f}, y0),
              "spherePlane straddles plane (dist 0.5 <= 1)");
    }

    // --- 6. raySphere hit from outside ----------------------------------------
    // oc(-5,0,0), aq 1, bq -10, cq 24, disc 4, sq 2, t=(10-2)/2=4 (enters at x=-1).
    {
        Ray r{vec3(-5.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        float t = -1.0f;
        check(raySphere(r, s, t), "raySphere hit from outside");
        check(approx(t, 4.0f), "raySphere entry t == 4");
    }

    // --- 7. raySphere from inside ---------------------------------------------
    // Near root -1 < 0 -> far root (exit) t == 1.
    {
        Ray r{vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        float t = -1.0f;
        check(raySphere(r, s, t), "raySphere from inside hits");
        check(approx(t, 1.0f), "raySphere from inside returns exit t == 1");
    }

    // --- 8. raySphere miss ----------------------------------------------------
    // Closest approach y=2 > radius 1 -> disc < 0 -> false. Do not read t.
    {
        Ray r{vec3(-5.0f, 2.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        float t = -1.0f;
        check(!raySphere(r, s, t), "raySphere miss (closest approach outside radius)");
    }

    // --- 9. raySphere behind --------------------------------------------------
    // Both roots negative -> false.
    {
        Ray r{vec3(5.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        float t = -1.0f;
        check(!raySphere(r, s, t), "raySphere sphere entirely behind ray");
    }

    // --- 10. raySphere unnormalized dir ---------------------------------------
    // dir length 2: aq 4, bq -20, cq 24, disc 16, sq 4, t=(20-4)/8=2 (t in dir-length
    // units: hit still enters at x=-1, i.e. 4 world units / 2 = 2).
    {
        Ray r{vec3(-5.0f, 0.0f, 0.0f), vec3(2.0f, 0.0f, 0.0f)};
        Sphere s{vec3(0.0f, 0.0f, 0.0f), 1.0f};
        float t = -1.0f;
        check(raySphere(r, s, t), "raySphere hit with unnormalized dir");
        check(approx(t, 2.0f), "raySphere t in ray-dir length units == 2");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
