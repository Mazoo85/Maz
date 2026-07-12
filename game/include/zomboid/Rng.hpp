// Deterministic RNG (PCG32) — replaces the reference game's Math.random() with a
// seeded, reproducible stream so the simulation is testable and replayable.
// (Roadmap Phase 2: "deterministic RNG (PCG/xoshiro)".)
#pragma once

#include <cstdint>

namespace zb {

class Rng {
public:
    explicit Rng(uint64_t seed = 0x853c49e6748fea9bULL,
                 uint64_t seq = 0xda3e39cb94b95bdbULL) {
        m_state = 0;
        m_inc = (seq << 1u) | 1u;
        next();
        m_state += seed;
        next();
    }

    // Raw 32-bit output.
    uint32_t next() {
        uint64_t old = m_state;
        m_state = old * 6364136223846793005ULL + m_inc;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Uniform float in [0, 1), mirroring JS Math.random() semantics.
    float nextFloat() {
        return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); // 24-bit mantissa
    }

    // Uniform float in [lo, hi).
    float range(float lo, float hi) { return lo + (hi - lo) * nextFloat(); }

    // Internal state accessors — for exact save/load of the RNG stream.
    uint64_t state() const { return m_state; }
    uint64_t inc() const { return m_inc; }
    void setState(uint64_t s, uint64_t i) {
        m_state = s;
        m_inc = i;
    }

    // Uniform int in [0, n).
    uint32_t below(uint32_t n) {
        // Debiased bounded generation.
        uint32_t threshold = (~n + 1u) % n;
        for (;;) {
            uint32_t r = next();
            if (r >= threshold) return r % n;
        }
    }

private:
    uint64_t m_state;
    uint64_t m_inc;
};

} // namespace zb
