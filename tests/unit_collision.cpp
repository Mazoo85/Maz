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

    // Ray-triangle (Moeller-Trumbore). Triangle in the z=0 plane, CCW winding -> +z
    // normal. Barycentrics: u toward v1, v toward v2, 1-u-v toward v0.
    const vec3 tv0(0.0f, 0.0f, 0.0f);
    const vec3 tv1(1.0f, 0.0f, 0.0f);
    const vec3 tv2(0.0f, 1.0f, 0.0f);

    // --- 11. rayTriangle hit inside, straight-down ray ------------------------
    // Hit point (0.25,0.25,0) = v0 + 0.25*(1,0,0) + 0.25*(0,1,0) -> u=0.25, v=0.25,
    // travels 1 unit down (z: 1 -> 0) so t=1.
    {
        Ray r{vec3(0.25f, 0.25f, 1.0f), vec3(0.0f, 0.0f, -1.0f)};
        float t = -1.0f, u = -1.0f, v = -1.0f;
        check(rayTriangle(r, tv0, tv1, tv2, t, u, v), "rayTriangle hit inside");
        check(approx(t, 1.0f), "rayTriangle inside t == 1");
        check(approx(u, 0.25f), "rayTriangle inside u == 0.25");
        check(approx(v, 0.25f), "rayTriangle inside v == 0.25");
    }

    // --- 12. rayTriangle hit at a vertex --------------------------------------
    // Straight down onto v0 -> u=0, v=0, t=1.
    {
        Ray r{vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f)};
        float t = -1.0f, u = -1.0f, v = -1.0f;
        check(rayTriangle(r, tv0, tv1, tv2, t, u, v), "rayTriangle hit at vertex v0");
        check(approx(t, 1.0f), "rayTriangle vertex t == 1");
        check(approx(u, 0.0f), "rayTriangle vertex u == 0");
        check(approx(v, 0.0f), "rayTriangle vertex v == 0");
    }

    // --- 13. rayTriangle miss (outside triangle) ------------------------------
    // (2,2) projects to u=2 (>1) -> rejected -> false. Do not read outputs.
    {
        Ray r{vec3(2.0f, 2.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f)};
        float t = -1.0f;
        check(!rayTriangle(r, tv0, tv1, tv2, t), "rayTriangle miss (u+v>1, outside)");
    }

    // --- 14. rayTriangle miss (u<0 side) --------------------------------------
    // (-0.5,0.25) -> u<0 -> false.
    {
        Ray r{vec3(-0.5f, 0.25f, 1.0f), vec3(0.0f, 0.0f, -1.0f)};
        float t = -1.0f;
        check(!rayTriangle(r, tv0, tv1, tv2, t), "rayTriangle miss (u<0)");
    }

    // --- 15. rayTriangle parallel to plane ------------------------------------
    // dir (1,0,0) lies in a plane parallel to z=0 -> det ~= 0 -> false.
    {
        Ray r{vec3(0.25f, 0.25f, 1.0f), vec3(1.0f, 0.0f, 0.0f)};
        float t = -1.0f;
        check(!rayTriangle(r, tv0, tv1, tv2, t), "rayTriangle parallel to plane");
    }

    // --- 16. rayTriangle behind the ray ---------------------------------------
    // dir +z points away from the z=0 triangle -> t<0 -> false.
    {
        Ray r{vec3(0.25f, 0.25f, 1.0f), vec3(0.0f, 0.0f, 1.0f)};
        float t = -1.0f;
        check(!rayTriangle(r, tv0, tv1, tv2, t), "rayTriangle behind ray (t<0)");
    }

    // --- 17. rayTriangle backface cull ----------------------------------------
    // Ray comes up through the back face (origin below, dir +z). Hits at t=1 without
    // cull; rejected with cullBackface=true (det-sign test).
    {
        Ray r{vec3(0.25f, 0.25f, -1.0f), vec3(0.0f, 0.0f, 1.0f)};
        float t = -1.0f;
        check(rayTriangle(r, tv0, tv1, tv2, t, /*cullBackface=*/false),
              "rayTriangle backface hit without cull");
        check(approx(t, 1.0f), "rayTriangle backface no-cull t == 1");
        float t2 = -1.0f;
        check(!rayTriangle(r, tv0, tv1, tv2, t2, /*cullBackface=*/true),
              "rayTriangle backface rejected with cull");
    }

    // --- 18. rayTriangle unnormalized dir -------------------------------------
    // dir length 2 (0,0,-2): origin z=2, hit z=0 at t=1 (2 + 1*(-2) = 0). t in
    // dir-length units.
    {
        Ray r{vec3(0.25f, 0.25f, 2.0f), vec3(0.0f, 0.0f, -2.0f)};
        float t = -1.0f, u = -1.0f, v = -1.0f;
        check(rayTriangle(r, tv0, tv1, tv2, t, u, v), "rayTriangle hit with unnormalized dir");
        check(approx(t, 1.0f), "rayTriangle unnormalized t == 1 (dir-length units)");
        check(approx(u, 0.25f), "rayTriangle unnormalized u == 0.25");
        check(approx(v, 0.25f), "rayTriangle unnormalized v == 0.25");
    }

    // --- 19. rayTriangle barycentric on an edge -------------------------------
    // Midpoint of edge v0-v1: (0.5,0,0) -> u=0.5, v=0 (asymmetric: catches a u/v swap).
    {
        Ray r{vec3(0.5f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f)};
        float t = -1.0f, u = -1.0f, v = -1.0f;
        check(rayTriangle(r, tv0, tv1, tv2, t, u, v), "rayTriangle hit on edge v0-v1");
        check(approx(t, 1.0f), "rayTriangle edge t == 1");
        check(approx(u, 0.5f), "rayTriangle edge u == 0.5");
        check(approx(v, 0.0f), "rayTriangle edge v == 0");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
