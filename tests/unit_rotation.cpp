// Unit tests for maz::math::Rotation — quaternion rotation helpers over glm
// (fromAxisAngle/fromEuler/rotate/angleBetween/slerp/rotateTowards/lookRotation).
// Rotation math is float, so vector comparisons use an approx tolerance, but the
// inputs are chosen to land on clean 0/±1 results (axis-aligned unit vectors,
// right-angle rotations) so the approx is trivially satisfied. Right-hand-rule
// expecteds (blocks 2/5) and lookRotation's -Z-forward handedness (block 9) are
// hand-verified, not read back from the code. Pure C++, no GPU/display.

#include "maz/math/Rotation.hpp"

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

// Quaternion equality up to the double-cover sign (q and -q are the same rotation).
// Used for slerp-endpoint checks where the result should equal an endpoint exactly:
// angleBetween is acos-based and, on glm's not-perfectly-unit quats, reports a
// spurious ~7e-4 rad even between a quat and itself, so a direct component compare
// is the tighter, truer assertion.
bool qapprox(quat a, quat b) {
    const bool same = approx(a.w, b.w) && approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z);
    const bool neg = approx(a.w, -b.w) && approx(a.x, -b.x) && approx(a.y, -b.y) &&
                     approx(a.z, -b.z);
    return same || neg;
}

const float PI = 3.14159265358979323846f;
const float HALF_PI = PI * 0.5f;

} // namespace

int main() {
    // --- 1. IDENTITY ROTATE --------------------------------------------------
    {
        quat id(1.0f, 0.0f, 0.0f, 0.0f); // (w,x,y,z)
        check(vapprox(rotate(id, vec3(1.0f, 2.0f, 3.0f)), vec3(1.0f, 2.0f, 3.0f)),
              "identity rotate leaves vector unchanged");
    }

    // --- 2. fromAxisAngle 90 deg about +Y ------------------------------------
    {
        // Right-hand rule: +90 about +Y takes +X -> -Z, -Z -> -X; the axis is fixed.
        quat q = fromAxisAngle(vec3(0.0f, 1.0f, 0.0f), HALF_PI);
        check(vapprox(rotate(q, vec3(1.0f, 0.0f, 0.0f)), vec3(0.0f, 0.0f, -1.0f)),
              "fromAxisAngle +Y 90: +X -> -Z");
        check(vapprox(rotate(q, vec3(0.0f, 0.0f, -1.0f)), vec3(-1.0f, 0.0f, 0.0f)),
              "fromAxisAngle +Y 90: -Z -> -X");
        check(vapprox(rotate(q, vec3(0.0f, 1.0f, 0.0f)), vec3(0.0f, 1.0f, 0.0f)),
              "fromAxisAngle +Y 90: axis +Y fixed");
    }

    // --- 3. fromAxisAngle 90 deg about +Z ------------------------------------
    {
        // Right-hand rule: +90 about +Z takes +X -> +Y.
        quat q = fromAxisAngle(vec3(0.0f, 0.0f, 1.0f), HALF_PI);
        check(vapprox(rotate(q, vec3(1.0f, 0.0f, 0.0f)), vec3(0.0f, 1.0f, 0.0f)),
              "fromAxisAngle +Z 90: +X -> +Y");
    }

    // --- 4. fromAxisAngle non-unit axis normalized ---------------------------
    {
        quat q = fromAxisAngle(vec3(0.0f, 5.0f, 0.0f), HALF_PI);
        check(vapprox(rotate(q, vec3(1.0f, 0.0f, 0.0f)), vec3(0.0f, 0.0f, -1.0f)),
              "fromAxisAngle normalizes a non-unit axis");
    }

    // --- 5. fromEuler single-axis == fromAxisAngle ---------------------------
    {
        // Yaw only: same as +Y 90.
        quat yaw = fromEuler(vec3(0.0f, HALF_PI, 0.0f));
        check(vapprox(rotate(yaw, vec3(1.0f, 0.0f, 0.0f)), vec3(0.0f, 0.0f, -1.0f)),
              "fromEuler yaw +Y 90: +X -> -Z");
        // Pitch only: +90 about +X takes +Y -> +Z, +Z -> -Y, so -Z -> +Y.
        quat pitch = fromEuler(vec3(HALF_PI, 0.0f, 0.0f));
        check(vapprox(rotate(pitch, vec3(0.0f, 0.0f, -1.0f)), vec3(0.0f, 1.0f, 0.0f)),
              "fromEuler pitch +X 90: -Z -> +Y");
    }

    // --- 6. slerp endpoints + shortest path ----------------------------------
    {
        quat a(1.0f, 0.0f, 0.0f, 0.0f);
        quat b = fromAxisAngle(vec3(0.0f, 1.0f, 0.0f), HALF_PI);
        check(qapprox(slerp(a, b, 0.0f), a), "slerp t=0 -> a");
        check(qapprox(slerp(a, b, 1.0f), b), "slerp t=1 -> b");
        check(qapprox(slerp(a, a, 0.5f), a), "slerp a,a -> a");
        // Halfway along the arc: angle from a is half the total (HALF_PI).
        check(std::fabs(angleBetween(a, slerp(a, b, 0.5f)) - HALF_PI * 0.5f) < 1e-4f,
              "slerp midpoint is half the arc from a");
    }

    // --- 7. angleBetween -----------------------------------------------------
    {
        quat id(1.0f, 0.0f, 0.0f, 0.0f);
        check(approx(angleBetween(id, id), 0.0f), "angleBetween(id,id) = 0");
        check(approx(angleBetween(id, fromAxisAngle(vec3(1.0f, 0.0f, 0.0f), HALF_PI)), HALF_PI),
              "angleBetween(id, 90 about +X) = HALF_PI");
        check(approx(angleBetween(id, fromAxisAngle(vec3(0.0f, 0.0f, 1.0f), PI * 0.25f)),
                     PI * 0.25f),
              "angleBetween(id, 45 about +Z) = PI/4");
        // Double-cover safe: 1.5 rad stays 1.5, not 2*pi - 1.5.
        check(approx(angleBetween(id, fromAxisAngle(vec3(0.0f, 1.0f, 0.0f), 1.5f)), 1.5f),
              "angleBetween double-cover-safe = 1.5");
    }

    // --- 8. rotateTowards clamp + reach --------------------------------------
    {
        quat from(1.0f, 0.0f, 0.0f, 0.0f);
        quat to = fromAxisAngle(vec3(0.0f, 1.0f, 0.0f), HALF_PI); // gap = HALF_PI
        quat clamped = rotateTowards(from, to, HALF_PI * 0.5f);
        check(approx(angleBetween(from, clamped), HALF_PI * 0.5f),
              "rotateTowards clamps step to maxRadians");
        quat reached = rotateTowards(from, to, PI); // maxRadians exceeds gap
        check(approx(angleBetween(reached, to), 0.0f),
              "rotateTowards reaches to when maxRadians exceeds gap");
        check(approx(angleBetween(rotateTowards(from, from, HALF_PI), from), 0.0f),
              "rotateTowards from,from -> from");
    }

    // --- 9. lookRotation (local -Z maps to forward) --------------------------
    {
        // Looking down -Z is the identity orientation.
        check(vapprox(rotate(lookRotation(vec3(0.0f, 0.0f, -1.0f)), vec3(0.0f, 0.0f, -1.0f)),
                      vec3(0.0f, 0.0f, -1.0f)),
              "lookRotation(-Z) is identity look");
        // Local forward (-Z) now points +X.
        check(vapprox(rotate(lookRotation(vec3(1.0f, 0.0f, 0.0f)), vec3(0.0f, 0.0f, -1.0f)),
                      vec3(1.0f, 0.0f, 0.0f)),
              "lookRotation(+X): local -Z -> +X");
        // Up is preserved for the identity look.
        check(vapprox(rotate(lookRotation(vec3(0.0f, 0.0f, -1.0f)), vec3(0.0f, 1.0f, 0.0f)),
                      vec3(0.0f, 1.0f, 0.0f)),
              "lookRotation(-Z) preserves up");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
