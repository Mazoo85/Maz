#pragma once

#include <cstdint>

#include "maz/core/Assert.hpp"

namespace maz::core {

// A small, fast, deterministic pseudo-random number generator (PCG32).
// Header-only; every member is inline. Seeded generators with identical
// (seedValue, seq) produce identical sequences across platforms.
struct Rng {
    uint64_t m_state;
    uint64_t m_inc;

    static constexpr uint64_t kDefaultSeq = 0xDA3E39CB94B95BDBULL;

    explicit Rng(uint64_t seedValue, uint64_t seq = kDefaultSeq) { seed(seedValue, seq); }

    void seed(uint64_t seedValue, uint64_t seq) {
        m_state = 0u;
        m_inc = (seq << 1u) | 1u;
        nextU32();
        m_state += seedValue;
        nextU32();
    }

    uint32_t nextU32() {
        uint64_t oldstate = m_state;
        m_state = oldstate * 6364136223846793005ULL + m_inc;
        uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18) ^ oldstate) >> 27);
        uint32_t rot = static_cast<uint32_t>(oldstate >> 59);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    float nextFloat() { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }

    // Returns a uniformly distributed integer in the inclusive range [lo, hi].
    // Preconditions: hi >= lo, and the span (hi - lo + 1) must be < 2^32.
    // Uses rejection sampling so the result is unbiased. Note: a full 32-bit
    // range (span == 2^32) is NOT representable here; call nextU32() directly
    // when you want the entire 32-bit range.
    int rangeInt(int lo, int hi) {
        MAZ_ASSERT(hi >= lo, "rangeInt requires hi >= lo");
        uint32_t range = static_cast<uint32_t>(static_cast<int64_t>(hi) - static_cast<int64_t>(lo) + 1);
        // range == 0 means the span was the full 2^32 (INT_MIN..INT_MAX) — unsupported (see doc).
        MAZ_ASSERT(range != 0u, "rangeInt: full 2^32 span is unsupported; use nextU32() directly");
        uint32_t threshold = (0u - range) % range;
        for (;;) {
            uint32_t r = nextU32();
            if (r >= threshold) {
                // Add in 64-bit and narrow once: for wide spans (> 2^31) r % range can
                // exceed INT_MAX, so `lo + static_cast<int>(...)` would overflow (UB).
                return static_cast<int>(static_cast<int64_t>(lo) + static_cast<int64_t>(r % range));
            }
        }
    }

    float rangeFloat(float lo, float hi) { return lo + (hi - lo) * nextFloat(); }

    bool nextBool() { return (nextU32() & 1u) != 0u; }
};

} // namespace maz::core
