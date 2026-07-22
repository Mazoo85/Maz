// tests/math/quaternionsquad.cpp — verifies SQUAD quaternion spline (QuaternionSquad.hpp).
// Ground truths, deterministic:
//   * quatExp is the inverse of quatLog on unit quaternions;
//   * a SQUAD segment hits its endpoints exactly (t=0 -> q0, t=1 -> q1) whatever the controls;
//   * identical keyframes give a constant orientation (no drift);
//   * every sampled orientation stays unit length;
//   * squadIntermediate of three equal orientations returns that orientation;
//   * for rotations sharing an axis the path stays on that axis and is C1 across a shared junction
//     (angular velocity is continuous — the whole point of SQUAD over chained slerp).
#include "maz/math/QuaternionSquad.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Quaternion;
using maz::math::quatExp;
using maz::math::quatLog;
using maz::math::squad;
using maz::math::squadIntermediate;
using maz::math::squadSegment;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

// Same rotation up to sign (q and -q are the same orientation).
static bool qsame(const Quaternion& a, const Quaternion& b, float e = 1e-3f) {
    const float d = a.q.x * b.q.x + a.q.y * b.q.y + a.q.z * b.q.z + a.q.w * b.q.w;
    return std::fabs(d) > 1.0f - e;
}
static float qlen(const Quaternion& a) {
    return std::sqrt(a.q.x * a.q.x + a.q.y * a.q.y + a.q.z * a.q.z + a.q.w * a.q.w);
}
// Rotation of `deg` about +Y as a quaternion.
static Quaternion yrot(float deg) {
    const float h = deg * 3.14159265358979324f / 180.0f * 0.5f;
    return Quaternion(0.0f, std::sin(h), 0.0f, std::cos(h));
}
// Recover the Y-angle (radians) of a pure-Y rotation quaternion.
static float yangle(const Quaternion& q) { return 2.0f * std::atan2(q.q.y, q.q.w); }

int main() {
    // --- 1. quatExp inverts quatLog. ---
    {
        const Quaternion q = yrot(37.0f);
        const Quaternion back = quatExp(quatLog(q));
        CHECK(qsame(back, q), "quatExp(quatLog(q)) == q");
        CHECK(near(qlen(back), 1.0f, 1e-3f), "exp(log(q)) is unit length");
    }

    // --- 2. Endpoints are exact regardless of controls. ---
    {
        const Quaternion q0 = yrot(10.0f), q1 = yrot(80.0f);
        const Quaternion s0 = yrot(-30.0f), s1 = yrot(200.0f); // arbitrary controls
        CHECK(qsame(squad(q0, q1, s0, s1, 0.0f), q0), "squad(t=0) == q0");
        CHECK(qsame(squad(q0, q1, s0, s1, 1.0f), q1), "squad(t=1) == q1");
    }

    // --- 3. Identical keyframes -> constant, and unit length throughout. ---
    {
        const Quaternion q = yrot(42.0f);
        bool constOk = true, unitOk = true;
        for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
            const Quaternion r = squadSegment(q, q, q, q, t);
            if (!qsame(r, q)) constOk = false;
            if (!near(qlen(r), 1.0f, 1e-3f)) unitOk = false;
        }
        CHECK(constOk, "all-equal keyframes give a constant orientation");
        CHECK(unitOk, "constant SQUAD stays unit length");
    }

    // --- 4. squadIntermediate of three equal orientations returns that orientation. ---
    {
        const Quaternion q = yrot(55.0f);
        CHECK(qsame(squadIntermediate(q, q, q), q), "intermediate of equal neighbours is the keyframe");
    }

    // --- 5. Unit length on a real varied segment. ---
    {
        const Quaternion k0 = yrot(0), k1 = yrot(20), k2 = yrot(50), k3 = yrot(80);
        bool unitOk = true;
        for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
            if (!near(qlen(squadSegment(k0, k1, k2, k3, t)), 1.0f, 1e-3f)) unitOk = false;
        }
        CHECK(unitOk, "SQUAD keeps orientations unit length across a segment");
    }

    // --- 6. C1 continuity across a shared junction (angular velocity is continuous). ---
    {
        // Five keyframes about +Y. Junction is at k2, between segment A=[k1,k2] and B=[k2,k3].
        const Quaternion k0 = yrot(0), k1 = yrot(20), k2 = yrot(50), k3 = yrot(80), k4 = yrot(120);

        // Position continuity: both segments meet exactly at k2.
        CHECK(qsame(squadSegment(k0, k1, k2, k3, 1.0f), k2), "segment A ends at k2");
        CHECK(qsame(squadSegment(k1, k2, k3, k4, 0.0f), k2), "segment B starts at k2");

        // Velocity continuity via finite difference of the Y-angle at the junction.
        const float h = 1e-3f;
        const float aEnd = yangle(squadSegment(k0, k1, k2, k3, 1.0f));
        const float aBefore = yangle(squadSegment(k0, k1, k2, k3, 1.0f - h));
        const float bStart = yangle(squadSegment(k1, k2, k3, k4, 0.0f));
        const float bAfter = yangle(squadSegment(k1, k2, k3, k4, 0.0f + h));
        const float velA = (aEnd - aBefore) / h;   // incoming angular speed
        const float velB = (bAfter - bStart) / h;  // outgoing angular speed
        CHECK(near(velA, velB, 1e-1f), "angular velocity is continuous across the junction (C1)");
        // Sanity: the path is genuinely moving (not a degenerate zero-velocity match).
        CHECK(velA > 0.1f, "the junction actually has motion through it");
    }

    if (g_fail == 0) {
        std::printf("quaternionsquad: OK — exp/log inverse, exact endpoints, constant case, unit length, "
                    "intermediate, C1 junction.\n");
        return 0;
    }
    std::printf("quaternionsquad: %d failure(s).\n", g_fail);
    return 1;
}
