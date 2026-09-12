// tests/math/ahrsfilter.cpp — verifies the Madgwick IMU attitude filter (math AhrsFilter.hpp).
// Every ground truth is INDEPENDENT of the code under test:
//   * GRAVITY DIRECTION (oracle): gravityDirectionBody(q) is cross-checked against glm's own quaternion-
//     vector rotation, conjugate(q) * worldUp — two unrelated ways to express "world +Z in the body frame";
//   * STATIC TILT CONVERGENCE (the defining AHRS property): started at identity with a wrong tilt and zero
//     gyro, the filter's estimated gravity direction converges to the measured accelerometer direction;
//   * DYNAMIC TRACKING: fed the exact body-frame gyro of an analytically-rotated body (glm angleAxis, the
//     independent oracle) plus the matching accel, the filter tracks the true tilt within a tight bound;
//   * PURE INTEGRATION == ANALYTIC: with the accelerometer suppressed, gyro-only integration reproduces the
//     exact exponential-map rotation (glm angleAxis composition) to Euler-truncation tolerance;
//   * YAW IS UNOBSERVABLE (documented behaviour): a pure yaw of the true body leaves the measured gravity
//     unchanged, so the filter neither sees nor corrects it — asserted, not hidden.
// Deterministic: fixed scenarios + a seeded LCG, no <random>, no clock.
#include "maz/math/AhrsFilter.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::gravityDirectionBody;
using maz::math::MadgwickFilter;
using maz::math::quat;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 32); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }             // [0,1)
    float sym() { return unit() * 2.0f - 1.0f; }                                    // [-1,1)
};

// Angle (radians) between two vectors.
static float angleBetween(const vec3& a, const vec3& b) {
    const float d = glm::dot(glm::normalize(a), glm::normalize(b));
    return std::acos(d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d));
}

// Geodesic angle (radians) between two orientations (double-cover aware).
static float quatAngle(const quat& a, const quat& b) {
    float d = std::fabs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
    if (d > 1.0f) d = 1.0f;
    return 2.0f * std::acos(d);
}

// A pseudo-random unit quaternion.
static quat randomQuat(Lcg& rng) {
    vec3 axis(rng.sym(), rng.sym(), rng.sym());
    if (glm::dot(axis, axis) < 1e-6f) axis = vec3(0.0f, 0.0f, 1.0f);
    axis = glm::normalize(axis);
    const float angle = rng.unit() * 6.2831853f;
    return glm::angleAxis(angle, axis);
}

int main() {
    // --- 0. gravityDirectionBody cross-checked against glm's own quat-vector rotation. ---
    {
        Lcg rng{0xA4E5u};
        float worst = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            const quat q = randomQuat(rng);
            const vec3 mine = gravityDirectionBody(q);
            const vec3 glmv = glm::conjugate(q) * vec3(0.0f, 0.0f, 1.0f); // R(q)^T * worldUp
            // Compare by vector distance, not acos(dot): near-identical unit vectors amplify float noise
            // through acos, whereas the component distance reflects the true (tiny) evaluation difference.
            const float e = glm::length(mine - glmv);
            if (e > worst) worst = e;
        }
        CHECK(worst < 1e-5f, "gravityDirectionBody matches glm conjugate-rotate of world-up");
        CHECK(angleBetween(gravityDirectionBody(quat(1.0f, 0.0f, 0.0f, 0.0f)), vec3(0.0f, 0.0f, 1.0f)) < 1e-5f,
              "identity orientation reads gravity straight down the body +Z");
    }

    // --- 1. Static tilt convergence: wrong start + zero gyro -> estimate finds the measured gravity. ---
    {
        const quat qTrue = glm::angleAxis(0.7f, glm::normalize(vec3(0.3f, -0.8f, 0.5f)));
        const vec3 accel = gravityDirectionBody(qTrue); // ideal accelerometer at this tilt
        MadgwickFilter f;
        f.beta = 0.5f;
        const float startErr = angleBetween(gravityDirectionBody(f.q), accel);
        CHECK(startErr > 0.3f, "the initial (identity) estimate really is far from the true tilt");
        for (int i = 0; i < 6000; ++i) {
            f.updateImu(vec3(0.0f), accel, 0.01f);
        }
        const float endErr = angleBetween(gravityDirectionBody(f.q), accel);
        CHECK(endErr < 0.02f, "static filter converges the gravity direction to under ~1 degree");
    }

    // --- 2. Dynamic tracking: exact analytic body rotation, filter fed matching gyro + accel. ---
    {
        const vec3 axis = glm::normalize(vec3(0.2f, 0.9f, -0.3f));
        const float rate = 1.5f;              // rad/s, body-frame angular speed
        const vec3 gyro = axis * rate;        // constant body-frame angular velocity
        const float dt = 0.005f;
        quat qTrue = glm::angleAxis(0.4f, glm::normalize(vec3(1.0f, 0.2f, 0.1f)));
        MadgwickFilter f;
        f.q = qTrue; // correct initial condition
        f.beta = 0.1f;
        const quat dq = glm::angleAxis(rate * dt, axis); // exact per-step rotation (independent oracle)
        float worstTilt = 0.0f;
        for (int i = 0; i < 1500; ++i) {                 // 7.5 s
            qTrue = glm::normalize(qTrue * dq);
            const vec3 accel = gravityDirectionBody(qTrue);
            f.updateImu(gyro, accel, dt);
            const float tiltErr = angleBetween(gravityDirectionBody(f.q), accel);
            if (tiltErr > worstTilt) worstTilt = tiltErr;
        }
        CHECK(worstTilt < 0.05f, "dynamic filter tracks the true tilt within ~3 degrees throughout");
        CHECK(quatAngle(f.q, qTrue) < 0.1f, "full orientation stays locked to the analytic truth");
    }

    // --- 3. Pure gyro integration (accel suppressed) reproduces the exact exponential-map rotation. ---
    {
        const vec3 axis = glm::normalize(vec3(0.1f, 0.2f, 1.0f));
        const float rate = 0.9f;
        const vec3 gyro = axis * rate;
        const float dt = 1e-4f;
        MadgwickFilter f; // starts at identity
        quat qRef(1.0f, 0.0f, 0.0f, 0.0f);
        const quat dq = glm::angleAxis(rate * dt, axis);
        for (int i = 0; i < 20000; ++i) { // 2 s, total angle 1.8 rad
            f.updateImu(gyro, vec3(0.0f), dt); // zero accel -> correction skipped, pure integration
            qRef = glm::normalize(qRef * dq);
        }
        CHECK(quatAngle(f.q, qRef) < 1e-2f, "gyro-only integration matches analytic rotation to truncation error");
        // A genuine rotation happened (guards against a no-op passing trivially).
        CHECK(quatAngle(f.q, quat(1.0f, 0.0f, 0.0f, 0.0f)) > 1.5f, "the integrated rotation is the expected ~1.8 rad");
    }

    // --- 4. Yaw about the gravity axis is unobservable to a gyro+accel filter. ---
    {
        // Level body (identity): gravity reads straight down; a pure yaw does not change that reading.
        const quat qLevel(1.0f, 0.0f, 0.0f, 0.0f);
        const quat qYawed = glm::angleAxis(1.0f, vec3(0.0f, 0.0f, 1.0f)); // 1 rad of heading
        const vec3 gLevel = gravityDirectionBody(qLevel);
        const vec3 gYawed = gravityDirectionBody(qYawed);
        CHECK(angleBetween(gLevel, gYawed) < 1e-5f, "a pure yaw leaves the measured gravity direction unchanged");
        // So a static filter fed the level accel cannot recover the yaw — it settles on zero-tilt, any heading.
        MadgwickFilter f;
        f.beta = 0.5f;
        for (int i = 0; i < 4000; ++i) {
            f.updateImu(vec3(0.0f), gLevel, 0.01f);
        }
        CHECK(angleBetween(gravityDirectionBody(f.q), gLevel) < 0.02f, "filter still nails the (zero) tilt without heading info");
    }

    if (g_fail == 0) {
        std::printf("ahrsfilter: OK — gravity oracle, static convergence, dynamic tracking, integration, yaw observability.\n");
        return 0;
    }
    std::printf("ahrsfilter: %d failure(s).\n", g_fail);
    return 1;
}
