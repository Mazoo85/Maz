// tests/math/dualquat.cpp — verifies dual quaternions + dual-quaternion skinning blend
// (math::DualQuaternion, blendDual). Ground truths, all closed-form quaternion algebra, deterministic:
//   * a rigid transform (rotation + translation) applied to a point matches rotate-then-translate;
//   * identity leaves a point unchanged; a pure-translation DQ adds the translation;
//   * translation() recovers the encoded translation;
//   * blending identical transforms reproduces that transform;
//   * THE POINT OF DQS: blending two rotations and applying to a unit-length point keeps it unit length
//     (no volume collapse) — where linear-blend skinning would shrink it toward the average — and the
//     50/50 blend of 0° and 90° about Z lands at the 45° screw position.
#include "maz/math/DualQuaternion.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::DualQuaternion;
using maz::math::Quaternion;
using maz::math::blendDual;
using maz::math::vec3;

static bool vnear(const vec3& a, const vec3& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}
static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    const float halfPi = 1.57079632679f;
    const Quaternion rotZ90 = Quaternion::fromAxisAngle(vec3(0, 0, 1), halfPi);

    // --- 1. Rigid transform = rotate then translate. ---
    {
        const DualQuaternion dq = DualQuaternion::fromRotationTranslation(rotZ90, vec3(1, 2, 3));
        // rotate (1,0,0) by 90° about Z -> (0,1,0); then + (1,2,3) -> (1,3,3).
        CHECK(vnear(dq.transformPoint(vec3(1, 0, 0)), vec3(1, 3, 3)), "rotate then translate");
        CHECK(vnear(dq.translation(), vec3(1, 2, 3)), "translation() recovers the translation");
    }

    // --- 2. Identity and pure translation. ---
    {
        CHECK(vnear(DualQuaternion::identity().transformPoint(vec3(4, -1, 2)), vec3(4, -1, 2)),
              "identity leaves a point unchanged");
        const DualQuaternion tr = DualQuaternion::fromTranslation(vec3(5, 0, -2));
        CHECK(vnear(tr.transformPoint(vec3(1, 1, 1)), vec3(6, 1, -1)), "pure translation adds t");
    }

    // --- 3. Blending identical transforms reproduces it. ---
    {
        const DualQuaternion dq = DualQuaternion::fromRotationTranslation(rotZ90, vec3(2, 0, 0));
        const DualQuaternion list[2] = {dq, dq};
        const float w[2] = {0.5f, 0.5f};
        const DualQuaternion b = blendDual(list, w, 2);
        CHECK(vnear(b.transformPoint(vec3(1, 0, 0)), dq.transformPoint(vec3(1, 0, 0))),
              "blend of identical transforms == the transform");
    }

    // --- 4. THE DQS PROPERTY: blended rotation stays rigid (no shrink). ---
    {
        const DualQuaternion a = DualQuaternion::fromRotation(Quaternion::identity());   // 0°
        const DualQuaternion c = DualQuaternion::fromRotation(rotZ90);                    // 90° about Z
        const DualQuaternion list[2] = {a, c};
        const float w[2] = {0.5f, 0.5f};
        const DualQuaternion b = blendDual(list, w, 2);
        const vec3 p = b.transformPoint(vec3(1, 0, 0));
        // Rigid: a unit input stays unit length (linear-blend skinning would give length ~0.707).
        CHECK(std::fabs(len(p) - 1.0f) < 1e-4f, "DQS keeps unit length (no candy-wrapper collapse)");
        // 50/50 of 0° and 90° is the 45° screw: (cos45, sin45, 0).
        const float c45 = std::cos(0.25f * 3.14159265358979324f);
        CHECK(vnear(p, vec3(c45, c45, 0.0f)), "50/50 blend lands at the 45° rotation");
    }

    // --- 5. Hemisphere handling: blending a rotation with its negated form is stable. ---
    {
        DualQuaternion a = DualQuaternion::fromRotation(rotZ90);
        DualQuaternion c = a;
        // Negate c's quaternion (same rotation, opposite hemisphere) — DLB must align it, not cancel.
        c.rx = -c.rx; c.ry = -c.ry; c.rz = -c.rz; c.rw = -c.rw;
        c.dx = -c.dx; c.dy = -c.dy; c.dz = -c.dz; c.dw = -c.dw;
        const DualQuaternion list[2] = {a, c};
        const float w[2] = {0.5f, 0.5f};
        const DualQuaternion b = blendDual(list, w, 2);
        CHECK(vnear(b.transformPoint(vec3(1, 0, 0)), a.transformPoint(vec3(1, 0, 0))),
              "opposite-hemisphere duplicate blends to the same rotation, not identity");
    }

    if (g_fail == 0) {
        std::printf("dualquat: OK — rigid transform, identity/translation, blend identity, rigidity "
                    "(no collapse), 45-degree screw, hemisphere.\n");
        return 0;
    }
    std::printf("dualquat: %d failure(s).\n", g_fail);
    return 1;
}
