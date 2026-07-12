#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::core {

// Deterministic random-number generator: one seeded, reproducible source of randomness for gameplay
// and procedural generation, so a given seed always produces the same world/loot/spread — essential
// for replays, tests, and shareable "seed" content. Replaces the ad-hoc xorshift each demo used to
// hand-roll. The core is xoshiro256** (fast, high quality) seeded through SplitMix64 so even a small
// or zero seed fills the state well. Everything derives from next(): floats in [0,1), inclusive int
// ranges, weighted picks, Fisher-Yates shuffles, a Gaussian, and an angle. std-only (no glm) so core
// keeps zero dependencies. Header-only.

class Random {
public:
    Random() { seed(0xC0FFEEULL); }
    explicit Random(uint64_t s) { seed(s); }

    // Reset the stream to a seed (SplitMix64 expands it into the 256-bit state).
    void seed(uint64_t s) {
        uint64_t z = s + 0x9E3779B97F4A7C15ULL;
        for (int i = 0; i < 4; ++i) {
            z += 0x9E3779B97F4A7C15ULL;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
            x = x ^ (x >> 31);
            m_state[i] = x;
        }
        // Avoid the all-zero state (impossible here, but cheap insurance).
        if ((m_state[0] | m_state[1] | m_state[2] | m_state[3]) == 0) m_state[0] = 1;
    }

    // Raw 64-bit output (xoshiro256**).
    uint64_t nextU64() {
        const uint64_t result = rotl(m_state[1] * 5, 7) * 9;
        const uint64_t t = m_state[1] << 17;
        m_state[2] ^= m_state[0];
        m_state[3] ^= m_state[1];
        m_state[1] ^= m_state[2];
        m_state[0] ^= m_state[3];
        m_state[2] ^= t;
        m_state[3] = rotl(m_state[3], 45);
        return result;
    }
    uint32_t nextU32() { return static_cast<uint32_t>(nextU64() >> 32); }

    // Float / double in [0, 1).
    float nextFloat() {
        // Top 24 bits -> [0,1) with float precision.
        return static_cast<float>(nextU64() >> 40) * (1.0f / 16777216.0f);
    }
    double nextDouble() {
        return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Inclusive integer range [lo, hi]. If hi < lo, returns lo.
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        const uint64_t span = static_cast<uint64_t>(hi - lo) + 1ULL;
        return lo + static_cast<int>(nextU64() % span);
    }

    // Float range [lo, hi).
    float range(float lo, float hi) { return lo + nextFloat() * (hi - lo); }

    // True with probability p (clamped to [0,1]).
    bool chance(float p) {
        if (p <= 0.0f) return false;
        if (p >= 1.0f) return true;
        return nextFloat() < p;
    }

    // Angle in [0, 2*pi).
    float nextAngle() { return nextFloat() * 6.28318530718f; }

    // Standard-normal sample scaled/shifted (Box-Muller, one cached spare per pair).
    float gaussian(float mean = 0.0f, float stddev = 1.0f) {
        if (m_hasSpare) {
            m_hasSpare = false;
            return mean + stddev * m_spare;
        }
        float u1 = nextFloat();
        if (u1 < 1e-7f) u1 = 1e-7f; // avoid log(0)
        const float u2 = nextFloat();
        const float mag = std::sqrt(-2.0f * std::log(u1));
        const float theta = 6.28318530718f * u2;
        m_spare = mag * std::sin(theta);
        m_hasSpare = true;
        return mean + stddev * mag * std::cos(theta);
    }

    // Uniformly pick an element from a non-empty vector.
    template <class T>
    const T& pick(const std::vector<T>& v) {
        return v[static_cast<size_t>(range(0, static_cast<int>(v.size()) - 1))];
    }
    template <class T>
    T& pick(std::vector<T>& v) {
        return v[static_cast<size_t>(range(0, static_cast<int>(v.size()) - 1))];
    }

    // Pick an index in proportion to its weight. Zero/negative weights are never chosen; an all-zero
    // (or empty) weight list returns 0.
    size_t weighted(const std::vector<float>& weights) {
        double total = 0.0;
        for (float w : weights)
            if (w > 0.0f) total += static_cast<double>(w);
        if (total <= 0.0) return 0;
        double r = nextDouble() * total;
        for (size_t i = 0; i < weights.size(); ++i) {
            if (weights[i] <= 0.0f) continue;
            r -= static_cast<double>(weights[i]);
            if (r < 0.0) return i;
        }
        return weights.size() - 1;
    }

    // In-place Fisher-Yates shuffle.
    template <class T>
    void shuffle(std::vector<T>& v) {
        for (size_t i = v.size(); i > 1; --i) {
            const size_t j = static_cast<size_t>(range(0, static_cast<int>(i) - 1));
            T tmp = v[i - 1];
            v[i - 1] = v[j];
            v[j] = tmp;
        }
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    uint64_t m_state[4] = {1, 2, 3, 4};
    float m_spare = 0.0f;
    bool m_hasSpare = false;
};

} // namespace maz::core
