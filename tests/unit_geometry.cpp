// Unit tests for maz::math geometry primitives (Aabb, Ray, Plane) and the
// ray/box/plane intersection helpers. Pure math, no GPU required.

#include "maz/math/Geometry.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

bool approxVec(math::vec3 a, math::vec3 b, float eps = 1e-5f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

} // namespace

int main() {
    using math::Aabb;
    using math::Plane;
    using math::Ray;
    using math::vec3;

    // --- AABB basics ---------------------------------------------------------
    Aabb box{vec3(-1.0f, -2.0f, -3.0f), vec3(1.0f, 2.0f, 3.0f)};
    check(approxVec(box.center(), vec3(0.0f, 0.0f, 0.0f)), "aabb center");
    check(approxVec(box.size(), vec3(2.0f, 4.0f, 6.0f)), "aabb size");
    check(approxVec(box.extents(), vec3(1.0f, 2.0f, 3.0f)), "aabb extents");

    check(box.contains(vec3(0.0f, 0.0f, 0.0f)), "contains interior point");
    check(!box.contains(vec3(2.0f, 0.0f, 0.0f)), "does not contain exterior point");
    check(box.contains(vec3(1.0f, 2.0f, 3.0f)), "contains on-face point (inclusive)");

    // --- expand / expanded ---------------------------------------------------
    Aabb grow = box;
    grow.expand(vec3(5.0f, 0.0f, 0.0f));
    check(approx(grow.max.x, 5.0f), "expand grows max");
    check(approx(box.max.x, 1.0f), "expand on copy does not mutate source");

    Aabb before = box;
    Aabb after = before.expanded(vec3(0.0f, 10.0f, 0.0f));
    check(approx(after.max.y, 10.0f), "expanded produces grown box");
    check(approx(before.max.y, 2.0f), "expanded does not mutate source");

    // --- merge / merged ------------------------------------------------------
    Aabb a1{vec3(0.0f), vec3(1.0f)};
    Aabb a2{vec3(2.0f), vec3(3.0f)};
    Aabb m = a1.merged(a2);
    check(approxVec(m.min, vec3(0.0f)) && approxVec(m.max, vec3(3.0f)), "merged spans both");
    check(approx(a1.max.x, 1.0f), "merged does not mutate source");
    Aabb mm = a1;
    mm.merge(a2);
    check(approxVec(mm.min, vec3(0.0f)) && approxVec(mm.max, vec3(3.0f)), "merge mutates in place");

    // --- intersects ----------------------------------------------------------
    Aabb b0{vec3(0.0f), vec3(2.0f)};
    Aabb bOverlap{vec3(1.0f), vec3(3.0f)};
    Aabb bDisjoint{vec3(5.0f), vec3(6.0f)};
    Aabb bTouch{vec3(2.0f), vec3(4.0f)};
    check(b0.intersects(bOverlap), "aabb overlap detected");
    check(!b0.intersects(bDisjoint), "aabb disjoint rejected");
    check(b0.intersects(bTouch), "aabb touching counts (inclusive)");
    check(math::aabbAabb(b0, bOverlap), "aabbAabb delegates to intersects");

    // --- invalid() -> degenerate box -----------------------------------------
    Aabb inv = Aabb::invalid();
    check(!inv.isValid(), "invalid() is not valid");
    inv.expand(vec3(4.0f, 5.0f, 6.0f));
    check(inv.isValid(), "invalid box becomes valid after one expand");
    check(approxVec(inv.min, vec3(4.0f, 5.0f, 6.0f)) && approxVec(inv.max, vec3(4.0f, 5.0f, 6.0f)),
          "single-point expand yields degenerate box (min==max==point)");

    // --- point-in-aabb helper ------------------------------------------------
    check(math::pointInAabb(vec3(0.0f), box), "pointInAabb delegates to contains");

    // --- ray-plane -----------------------------------------------------------
    Plane xyPlane{vec3(0.0f, 0.0f, 1.0f), 0.0f}; // z == 0
    Ray down{vec3(0.0f, 0.0f, 5.0f), vec3(0.0f, 0.0f, -1.0f)};
    float t = 0.0f;
    check(math::rayPlane(down, xyPlane, t), "ray-plane hit");
    check(approx(t, 5.0f), "ray-plane t correct");
    check(approx(xyPlane.signedDistance(down.at(t)), 0.0f), "ray-plane hit lies on plane");

    Ray parallel{vec3(0.0f, 0.0f, 5.0f), vec3(1.0f, 0.0f, 0.0f)};
    float tPar = -123.0f;
    check(!math::rayPlane(parallel, xyPlane, tPar), "ray parallel to plane misses");

    Ray behind{vec3(0.0f, 0.0f, 5.0f), vec3(0.0f, 0.0f, 1.0f)}; // pointing away from plane
    float tBehind = -123.0f;
    check(!math::rayPlane(behind, xyPlane, tBehind), "plane behind ray origin misses");

    // --- ray-aabb ------------------------------------------------------------
    Aabb unit{vec3(-1.0f), vec3(1.0f)};
    Ray toward{vec3(-5.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
    float tmin = 0.0f;
    check(math::rayAabb(toward, unit, tmin), "ray-aabb hit");
    check(approx(tmin, 4.0f), "ray-aabb tmin correct");

    Ray miss{vec3(-5.0f, 5.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)};
    float tMiss = 0.0f;
    check(!math::rayAabb(miss, unit, tMiss), "ray-aabb miss");

    // Axis-aligned ray with zero direction components on two axes: must not NaN.
    Ray axis{vec3(0.0f, 0.0f, -5.0f), vec3(0.0f, 0.0f, 1.0f)};
    float tAxis = 0.0f;
    bool axisHit = math::rayAabb(axis, unit, tAxis);
    check(axisHit && approx(tAxis, 4.0f) && !std::isnan(tAxis), "axis-aligned ray hits without NaN");

    // Ray starting inside reports entry clamped to 0.
    Ray inside{vec3(0.0f), vec3(1.0f, 0.0f, 0.0f)};
    float tIn = -1.0f;
    check(math::rayAabb(inside, unit, tIn) && approx(tIn, 0.0f), "ray inside box reports tmin 0");

    // --- plane sign conventions ----------------------------------------------
    Plane p{vec3(0.0f, 1.0f, 0.0f), 0.0f}; // y == 0, normal +Y
    check(p.signedDistance(vec3(0.0f, 2.0f, 0.0f)) > 0.0f, "signedDistance positive toward normal");
    check(p.signedDistance(vec3(0.0f, -2.0f, 0.0f)) < 0.0f, "signedDistance negative behind normal");
    check(p.side(vec3(0.0f, 2.0f, 0.0f)) == 1, "side +1 in front");
    check(p.side(vec3(0.0f, -2.0f, 0.0f)) == -1, "side -1 behind");
    check(p.side(vec3(0.0f, 0.0f, 0.0f)) == 0, "side 0 on plane");

    // fromPointNormal round-trip: the defining point lies on the plane.
    vec3 pt(3.0f, -4.0f, 5.0f);
    vec3 n = math::normalize(vec3(1.0f, 2.0f, 3.0f));
    Plane fpn = Plane::fromPointNormal(pt, n);
    check(approx(fpn.signedDistance(pt), 0.0f), "fromPointNormal: defining point on plane");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
