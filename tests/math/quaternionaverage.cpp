// tests/math/quaternionaverage.cpp — verifies quaternion averaging (math QuaternionAverage.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rotations, no <random>, no clock):
//   * IDEMPOTENT: the average of N copies of q is q (up to sign);
//   * SIGN-INSENSITIVE (airtight): averaging q and -q returns q (double-cover safe);
//   * TWO-ROTATION == SLERP MIDPOINT (airtight): the average of q0 and q1 equals glm::slerp(q0,q1,0.5),
//     an INDEPENDENT formula;
//   * SYMMETRIC SPREAD: rotations at +/-theta about an axis average back to the base rotation;
//   * WEIGHTING: a dominant weight pulls the average toward that rotation.
#include "maz/math/QuaternionAverage.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <glm/gtc/quaternion.hpp>

using maz::math::quat;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// |dot| close to 1 means the same rotation (up to double-cover sign).
static bool sameRot(const quat& a, const quat& b, float tol = 1e-3f) {
    const float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return std::fabs(d) > 1.0f - tol;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 32); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
};

static quat axisAngle(vec3 axis, float ang) {
    const float l = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    axis = axis * (1.0f / l);
    const float h = ang * 0.5f;
    const float s = std::sin(h);
    return quat(std::cos(h), axis.x * s, axis.y * s, axis.z * s); // (w,x,y,z)
}

int main() {
    using namespace maz::math;

    // --- 1. Idempotent + sign-insensitive. ---
    {
        const quat q = axisAngle(vec3(0.3f, 0.8f, -0.5f), 1.1f);
        std::vector<quat> five(5, q);
        CHECK(sameRot(averageQuaternions(five), q), "average of N copies of q is q");
        std::vector<quat> pair = {q, quat(-q.w, -q.x, -q.y, -q.z)};
        CHECK(sameRot(averageQuaternions(pair), q), "averaging q and -q returns q (double-cover safe)");
    }

    // --- 2. Two-rotation average equals the slerp midpoint (independent formula). ---
    {
        Lcg rng{0xA5A5u};
        bool ok = true;
        for (int i = 0; i < 200; ++i) {
            const quat q0 = axisAngle(vec3(rng.sym(), rng.sym(), rng.sym()), rng.sym() * 3.0f);
            quat q1 = axisAngle(vec3(rng.sym(), rng.sym(), rng.sym()), rng.sym() * 3.0f);
            // Align q1 to q0's hemisphere so slerp takes the same (shortest) arc the mean does.
            if (q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w < 0.0f) {
                q1 = quat(-q1.w, -q1.x, -q1.y, -q1.z);
            }
            const quat avg = averageQuaternions({q0, q1});
            const quat mid = glm::normalize(glm::slerp(q0, q1, 0.5f));
            if (!sameRot(avg, mid, 3e-3f)) {
                ok = false;
                break;
            }
        }
        CHECK(ok, "the average of two rotations equals their slerp midpoint");
    }

    // --- 3. Symmetric spread about an axis averages to the base. ---
    {
        const vec3 axis(0.0f, 1.0f, 0.0f);
        const quat base = axisAngle(axis, 0.6f);
        // base * rot(+t) and base * rot(-t) for several t.
        std::vector<quat> qs;
        for (float t : {0.2f, 0.5f, 0.9f}) {
            qs.push_back(base * axisAngle(axis, t));
            qs.push_back(base * axisAngle(axis, -t));
        }
        CHECK(sameRot(averageQuaternions(qs), base, 5e-3f), "a symmetric +/- spread averages back to the base");
    }

    // --- 4. Weighting pulls toward the heavy rotation. ---
    {
        const quat qa = axisAngle(vec3(1, 0, 0), 0.2f);
        const quat qb = axisAngle(vec3(1, 0, 0), 1.4f);
        const quat heavyA = averageQuaternions({qa, qb}, {100.0f, 1.0f});
        CHECK(sameRot(heavyA, qa, 2e-2f), "a dominant weight pulls the average toward that rotation");
        // Equal weights land between them (closer to the 0.8 midpoint angle).
        const quat mid = averageQuaternions({qa, qb});
        const quat expectMid = axisAngle(vec3(1, 0, 0), 0.8f);
        CHECK(sameRot(mid, expectMid, 5e-3f), "equal weights land at the mid-angle rotation");
    }

    if (g_fail == 0) {
        std::printf("quaternionaverage: OK — idempotent, sign-safe, slerp midpoint, symmetric, weighting.\n");
        return 0;
    }
    std::printf("quaternionaverage: %d failure(s).\n", g_fail);
    return 1;
}
