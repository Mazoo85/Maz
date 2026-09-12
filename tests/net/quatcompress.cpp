// tests/net/quatcompress.cpp — verifies smallest-three quaternion compression (net QuatCompress.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a compressed+decompressed rotation is within a small angular error of the original (thousands of
//     uniformly-random unit quaternions), and the error shrinks as bits per component grow;
//   * the identity and the axis quaternions survive the round-trip;
//   * q and -q (same rotation) decode to the same rotation;
//   * the code fits within the advertised bit budget (<= 32 bits);
//   * a non-normalized input is normalized before compression.
#include "maz/net/QuatCompress.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::quat;
using maz::net::compressedQuatBits;
using maz::net::compressQuat;
using maz::net::decompressQuat;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; } // [0,1)
};

// Angle (radians) between the rotations represented by two unit quaternions.
static float rotAngle(const quat& a, const quat& b) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    d = std::fabs(d); // q and -q are the same rotation
    if (d > 1.0f) d = 1.0f;
    return 2.0f * std::acos(d);
}

// Uniformly-random unit quaternion (Shoemake).
static quat randomQuat(Lcg& rng) {
    const float u1 = rng.unit(), u2 = rng.unit(), u3 = rng.unit();
    const float s1 = std::sqrt(1.0f - u1), s2 = std::sqrt(u1);
    const float twoPi = 6.2831853f;
    quat q;
    q.x = s1 * std::sin(twoPi * u2);
    q.y = s1 * std::cos(twoPi * u2);
    q.z = s2 * std::sin(twoPi * u3);
    q.w = s2 * std::cos(twoPi * u3);
    return q;
}

int main() {
    // --- 1. Round-trip accuracy over random rotations, and it improves with more bits. ---
    {
        Lcg rng{0x0FFEE01u};
        float worst9 = 0.0f, worst10 = 0.0f;
        for (int trial = 0; trial < 20000; ++trial) {
            const quat q = randomQuat(rng);
            const float e9 = rotAngle(q, decompressQuat(compressQuat(q, 9), 9));
            const float e10 = rotAngle(q, decompressQuat(compressQuat(q, 10), 10));
            if (e9 > worst9) worst9 = e9;
            if (e10 > worst10) worst10 = e10;
        }
        // 9 bits/comp -> quantization step ~ sqrt(2)/512; worst-case angular error stays well under 1 degree.
        CHECK(worst9 < 0.02f, "9-bit worst-case error under ~1.1 degrees");
        CHECK(worst10 < 0.01f, "10-bit worst-case error under ~0.6 degrees");
        CHECK(worst10 < worst9, "more bits -> smaller worst-case error");
    }

    // --- 2. Identity and axis quaternions survive. ---
    {
        quat id; id.x = 0; id.y = 0; id.z = 0; id.w = 1;
        CHECK(rotAngle(id, decompressQuat(compressQuat(id))) < 0.02f, "identity round-trips");
        const float r2 = 0.70710678f;
        quat qx; qx.x = r2; qx.y = 0; qx.z = 0; qx.w = r2; // 90deg about X
        CHECK(rotAngle(qx, decompressQuat(compressQuat(qx))) < 0.02f, "90deg-about-X round-trips");
    }

    // --- 3. q and -q map to the same rotation. ---
    {
        Lcg r{123456u};
        bool ok = true;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            const quat q = randomQuat(r);
            quat nq; nq.x = -q.x; nq.y = -q.y; nq.z = -q.z; nq.w = -q.w;
            // The sign-canonicalization makes q and -q produce the exact same code (the strong invariant).
            if (compressQuat(q) != compressQuat(nq)) ok = false;
        }
        CHECK(ok, "q and -q compress to the identical code");
    }

    // --- 4. Bit budget. ---
    {
        CHECK(compressedQuatBits(9) == 29, "9 bits/comp -> 29-bit code");
        CHECK(compressedQuatBits(10) == 32, "10 bits/comp -> 32-bit code");
        // The actual code never exceeds the advertised width.
        Lcg r{777u};
        bool ok = true;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            const std::uint32_t code = compressQuat(randomQuat(r), 10);
            (void)code; // 32-bit type can't exceed 32 bits; assert 9-bit codes fit in 29 bits
            const std::uint32_t code9 = compressQuat(randomQuat(r), 9);
            if (code9 >= (1u << 29)) ok = false;
        }
        CHECK(ok, "9-bit codes fit within 29 bits");
    }

    // --- 5. Non-normalized input is normalized. ---
    {
        quat q; q.x = 0.0f; q.y = 0.0f; q.z = 3.0f; q.w = 3.0f; // 90deg about Z, scaled by ~4.24
        quat unit; unit.x = 0; unit.y = 0; unit.z = 0.70710678f; unit.w = 0.70710678f;
        CHECK(rotAngle(unit, decompressQuat(compressQuat(q))) < 0.02f, "unnormalized input handled");
    }

    if (g_fail == 0) {
        std::printf("quatcompress: OK — accuracy scales with bits, identity/axes, q==-q, bit budget, normalize.\n");
        return 0;
    }
    std::printf("quatcompress: %d failure(s).\n", g_fail);
    return 1;
}
