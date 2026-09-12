#pragma once

#include "maz/math/Math.hpp"      // maz::math::quat (glm)
#include "maz/net/FloatQuant.hpp" // quantizeFloat / dequantizeFloat

#include <cmath>
#include <cstdint>

// maz::net — "smallest three" quaternion compression: pack a full 3D rotation into 32 bits for cheap
// network replication. A unit quaternion has four components but only three degrees of freedom
// (x²+y²+z²+w²=1), and one component always has magnitude ≥ 1/2. The trick: DON'T send the largest
// component. Send a 2-bit index saying which of the four it was, then the OTHER three components — each
// guaranteed to lie in [-1/√2, +1/√2] — quantized to `bits` bits apiece. The receiver reconstructs the
// dropped one from the unit-length constraint. Because q and -q are the same rotation, we first flip the
// sign so the largest component is positive, and so its sign never needs a bit either. At the default 9
// bits per component that is 2 + 3·9 = 29 bits (fits in a 32-bit word with room to spare) for a rotation
// accurate to a small fraction of a degree — versus 128 bits for four raw floats, or 96 for three Euler
// angles that also suffer gimbal issues. This is the standard way shipping engines replicate orientation
// (character facing, projectile spin, ragdoll bones). Header-only, deterministic. Godot's multiplayer has
// no equivalent built in.
namespace maz::net {

// Pack a (not necessarily normalized) quaternion into an integer code using `bits` bits per stored
// component (default 9 -> 29-bit code). The largest component is dropped and reconstructed on decode.
inline std::uint32_t compressQuat(const math::quat& q, int bits = 9) {
    if (bits < 2) {
        bits = 2;
    }
    if (bits > 10) {
        bits = 10; // keep 2 + 3*bits <= 32
    }
    // Normalize (guard the degenerate zero quaternion to identity).
    float c[4] = {q.x, q.y, q.z, q.w};
    float lenSq = c[0] * c[0] + c[1] * c[1] + c[2] * c[2] + c[3] * c[3];
    if (lenSq < 1e-20f) {
        c[0] = c[1] = c[2] = 0.0f;
        c[3] = 1.0f;
        lenSq = 1.0f;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    for (float& v : c) {
        v *= invLen;
    }

    // Index of the largest-magnitude component.
    int maxI = 0;
    for (int i = 1; i < 4; ++i) {
        if (std::fabs(c[i]) > std::fabs(c[maxI])) {
            maxI = i;
        }
    }
    // Canonicalize: make the dropped component positive (q and -q are the same rotation).
    if (c[maxI] < 0.0f) {
        for (float& v : c) {
            v = -v;
        }
    }

    const float kRange = 0.70710678f; // 1/sqrt(2): bound on each non-largest component
    std::uint32_t code = static_cast<std::uint32_t>(maxI) & 0x3u;
    for (int i = 0; i < 4; ++i) {
        if (i == maxI) {
            continue;
        }
        const std::uint32_t q3 = quantizeFloat(c[i], -kRange, kRange, bits);
        code = (code << static_cast<unsigned>(bits)) | q3;
    }
    return code;
}

// Reconstruct the (unit) quaternion from a code produced by compressQuat with the same `bits`.
inline math::quat decompressQuat(std::uint32_t code, int bits = 9) {
    if (bits < 2) {
        bits = 2;
    }
    if (bits > 10) {
        bits = 10;
    }
    const float kRange = 0.70710678f;
    const std::uint32_t mask = (1u << static_cast<unsigned>(bits)) - 1u;

    // The three stored components were pushed in ascending i (skipping maxI); pop them in reverse.
    float restored[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    std::uint32_t work = code;
    float small[3];
    for (int k = 2; k >= 0; --k) {
        small[k] = dequantizeFloat(work & mask, -kRange, kRange, bits);
        work >>= static_cast<unsigned>(bits);
    }
    const int maxI = static_cast<int>(work & 0x3u);

    int s = 0;
    float sumSq = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (i == maxI) {
            continue;
        }
        restored[i] = small[s++];
        sumSq += restored[i] * restored[i];
    }
    const float largest = std::sqrt(std::fmax(0.0f, 1.0f - sumSq));
    restored[maxI] = largest;

    math::quat out;
    out.x = restored[0];
    out.y = restored[1];
    out.z = restored[2];
    out.w = restored[3];
    return out;
}

// Number of bits a code occupies for a given per-component bit count (2 index bits + 3*bits).
inline int compressedQuatBits(int bits = 9) {
    if (bits < 2) {
        bits = 2;
    }
    if (bits > 10) {
        bits = 10;
    }
    return 2 + 3 * bits;
}

} // namespace maz::net
