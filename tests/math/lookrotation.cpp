// tests/math/lookrotation.cpp — verifies the orientation-only look helper (math::Quaternion::lookRotation).
// Ground truths: facing the default forward (-Z) with default up is the identity; the resulting rotation maps
// local -Z onto the requested forward direction (unit-length, exact); +Y stays on the same side as the up
// hint; it agrees with Transform3D::lookingAt's rotation (same convention); the result is a unit quaternion;
// non-unit forward is normalized; a degenerate (zero) forward returns identity; and up parallel to forward is
// handled without NaN. Checked against the -Z-forward convention. Pure CPU, deterministic.
#include "maz/math/Quaternion.hpp"
#include "maz/math/Transform3D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Quaternion;
using maz::math::Transform3D;
using maz::math::vec3;

static bool vnear(const vec3& a, const vec3& b, float eps = 1e-4f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}
static vec3 norm(const vec3& v) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return vec3(v.x / l, v.y / l, v.z / l);
}

int main() {
    const vec3 kForwardLocal(0, 0, -1); // the engine/Godot "forward" axis

    // --- 1. Facing default forward is the identity. ---
    {
        const Quaternion q = Quaternion::lookRotation(vec3(0, 0, -1), vec3(0, 1, 0));
        CHECK(vnear(q.xform(kForwardLocal), vec3(0, 0, -1)), "facing -Z keeps -Z");
        CHECK(vnear(q.xform(vec3(0, 1, 0)), vec3(0, 1, 0)), "and keeps +Y up (identity)");
    }

    // --- 2. The rotation maps local -Z onto the requested direction, exactly and unit-length. ---
    {
        const vec3 dirs[] = {vec3(1, 0, 0), vec3(-1, 0, 0), vec3(0, 0, 1), vec3(3, 2, -1), vec3(-2, 5, 4)};
        for (const vec3& d : dirs) {
            const Quaternion q = Quaternion::lookRotation(d, vec3(0, 1, 0));
            const vec3 faced = q.xform(kForwardLocal);
            CHECK(vnear(faced, norm(d)), "local -Z maps onto the (normalized) forward direction");
            CHECK(std::fabs(std::sqrt(faced.x * faced.x + faced.y * faced.y + faced.z * faced.z) - 1.0f) < 1e-4f,
                  "faced direction is unit length");
        }
    }

    // --- 3. Up hint respected: rotated +Y stays on the same side as up. ---
    {
        const Quaternion q = Quaternion::lookRotation(vec3(1, 0, 0), vec3(0, 1, 0));
        const vec3 upWorld = q.xform(vec3(0, 1, 0));
        CHECK(upWorld.y > 0.0f, "rotated up keeps a positive Y component");
    }

    // --- 4. Agrees with Transform3D::lookingAt (same convention). ---
    {
        const vec3 target(4, 1, -3);
        const Transform3D t; // identity at origin
        const Transform3D look = t.lookingAt(target, vec3(0, 1, 0));
        const Quaternion q = Quaternion::lookRotation(target, vec3(0, 1, 0)); // origin is 0, so target == dir
        // Both should face the same direction with local -Z.
        CHECK(vnear(q.xform(kForwardLocal), look.basis * kForwardLocal, 1e-4f),
              "lookRotation faces the same way as Transform3D::lookingAt");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        const Quaternion zero = Quaternion::lookRotation(vec3(0, 0, 0), vec3(0, 1, 0));
        CHECK(vnear(zero.xform(kForwardLocal), vec3(0, 0, -1)), "zero forward -> identity");
        // up parallel to forward (both +Y): must not produce NaN and must still face the direction.
        const Quaternion par = Quaternion::lookRotation(vec3(0, 1, 0), vec3(0, 1, 0));
        const vec3 faced = par.xform(kForwardLocal);
        CHECK(std::isfinite(faced.x) && std::isfinite(faced.y) && std::isfinite(faced.z), "no NaN when up∥forward");
        CHECK(vnear(faced, vec3(0, 1, 0)), "still faces +Y when up was parallel");
    }

    if (g_fail == 0) {
        std::printf("lookrotation: OK — identity, -Z->dir, up side, lookingAt agreement, degenerate-safe.\n");
        return 0;
    }
    std::printf("lookrotation: %d failure(s).\n", g_fail);
    return 1;
}
