#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// maz::core::Pcg32 — the PCG (Permuted Congruential Generator) 32-bit random source, O'Neill's
// minimal `pcg32` (a 64-bit LCG state run through an xorshift+rotate output permutation). It sits
// beside the engine's xoshiro256** generator (core::Random): xoshiro is the default 64-bit source,
// PCG is the compact, statistically excellent 32-bit generator many tools/tests expect — and,
// unlike a bare LCG, it supports independent STREAMS (the `seq` selector), so different subsystems
// can draw from the same seed without correlating.
//
// This is a faithful port of the reference pcg32 (multiplier 6364136223846793005), so it reproduces
// PCG's published test vectors exactly — a deterministic, portable RNG for replays, procedural
// generation, and shareable seeds. next() is the raw 32-bit output; helpers give a bounded int
// (unbiased, rejection-sampled) and a float in [0,1). Header-only, std-only.
namespace maz::core {

class Pcg32 {
  public:
    Pcg32() { seed(0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL); }
    Pcg32(uint64_t initState, uint64_t initSeq) { seed(initState, initSeq); }

    // Seed the generator. `initState` starts the sequence; `initSeq` selects one of 2^63 distinct
    // streams (any two different seq values never overlap for the whole period).
    void seed(uint64_t initState, uint64_t initSeq) {
        m_state = 0u;
        m_inc = (initSeq << 1u) | 1u;
        next();
        m_state += initState;
        next();
    }

    // Raw 32-bit output (the canonical pcg32_random_r).
    uint32_t next() {
        const uint64_t old = m_state;
        m_state = old * 6364136223846793005ULL + m_inc;
        const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    // Unbiased integer in [0, bound) via rejection sampling (bound == 0 returns 0).
    uint32_t nextBounded(uint32_t bound) {
        if (bound == 0) {
            return 0;
        }
        // Reject the low `threshold` outputs so the remaining range divides evenly by `bound`.
        const uint32_t threshold = (0u - bound) % bound;
        for (;;) {
            const uint32_t r = next();
            if (r >= threshold) {
                return r % bound;
            }
        }
    }

    // Inclusive integer range [lo, hi].
    int range(int lo, int hi) {
        if (hi < lo) {
            const int t = lo;
            lo = hi;
            hi = t;
        }
        const uint32_t span = static_cast<uint32_t>(hi - lo) + 1u;
        return lo + static_cast<int>(nextBounded(span));
    }

    // Float in [0, 1) using the top 24 bits.
    float nextFloat() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    // Float in [lo, hi) (Godot's RandomNumberGenerator.randf_range; lo==hi returns lo).
    float rangef(float lo, float hi) { return lo + nextFloat() * (hi - lo); }

    // Normally-distributed float via Box-Muller (Godot's RandomNumberGenerator.randfn). Matches
    // Godot's algorithm and distribution; the underlying bit-stream differs (PCG vs Godot's PCG seed
    // scheme), so values are not stream-identical to Godot — only distributionally equivalent.
    float gaussian(float mean = 0.0f, float deviation = 1.0f) {
        const float twoPi = 6.28318530718f;
        const float u1 = 1.0f - nextFloat(); // (0, 1] so log() is finite
        const float u2 = nextFloat();
        const float mag = deviation * std::sqrt(-2.0f * std::log(u1));
        return mag * std::cos(twoPi * u2) + mean;
    }

    // Pick an index in [0, weights.size()) with probability proportional to its weight (Godot's
    // RandomNumberGenerator.rand_weighted). Negative weights count as 0; returns -1 if the total is
    // not positive (all zero/empty).
    int weighted(const std::vector<float>& weights) {
        float total = 0.0f;
        for (const float w : weights) {
            if (w > 0.0f) {
                total += w;
            }
        }
        if (total <= 0.0f) {
            return -1;
        }
        const float r = nextFloat() * total;
        float acc = 0.0f;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            if (weights[i] > 0.0f) {
                acc += weights[i];
                if (r < acc) {
                    return static_cast<int>(i);
                }
            }
        }
        // Fall back to the last positive-weight index (guards against float rounding at the top end).
        for (std::size_t i = weights.size(); i-- > 0;) {
            if (weights[i] > 0.0f) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

  private:
    uint64_t m_state = 0;
    uint64_t m_inc = 0;
};

} // namespace maz::core
