#pragma once

#include "maz/core/Random.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::audio {

// Stream randomizer — Godot's AudioStreamRandomizer. Repetitive one-shots (footsteps, gunshots, impacts,
// UI blips) sound robotic when the exact same clip plays every time. A randomizer wraps a pool of
// interchangeable streams and, on each trigger, picks one and jitters its pitch and volume so the ear
// never hears a mechanical repeat. Three pick modes match Godot: Random (weighted uniform), RandomNoRepeat
// (weighted, but never the clip that just played — Godot's default), and Sequential (round-robin). Pitch
// is scaled by a log-symmetric factor in [1/randomPitch, randomPitch] and volume offset by ± a dB range.
// This is the SELECTION + variance logic only (which clip, what pitch/volume) — it returns a RandomPick
// the caller feeds to the mixer; it does not itself decode or play audio. Deterministic (seeded
// core::Random), std-only, so it unit-tests exactly and drives a golden histogram/scatter.

enum class RandomizerMode { Random, RandomNoRepeat, Sequential };

struct RandomPick {
    int index = -1;         // chosen stream (-1 when the pool is empty)
    float pitchScale = 1.0f; // multiplicative pitch, 1 = unchanged
    float volumeDb = 0.0f;   // additive volume offset in decibels
};

class StreamRandomizer {
public:
    RandomizerMode mode = RandomizerMode::RandomNoRepeat;
    float randomPitch = 1.0f;          // 1 = no variation; pitch scaled within [1/p, p] (log-symmetric)
    float randomVolumeOffsetDb = 0.0f; // volume offset drawn from [-x, +x] dB

    void addStream(float weight = 1.0f) { m_weights.push_back(weight > 0.0f ? weight : 0.0f); }
    std::size_t streamCount() const { return m_weights.size(); }
    float weight(std::size_t i) const { return m_weights[i]; }

    void setSeed(std::uint64_t s) { m_rng.seed(s); }
    void reset() {
        m_cursor = 0;
        m_last = -1;
    }

    RandomPick next() {
        RandomPick pick;
        const int n = static_cast<int>(m_weights.size());
        if (n == 0) {
            return pick;
        }
        int idx = 0;
        switch (mode) {
            case RandomizerMode::Sequential:
                idx = m_cursor % n;
                m_cursor = (m_cursor + 1) % n;
                break;
            case RandomizerMode::Random:
                idx = weightedPick();
                break;
            case RandomizerMode::RandomNoRepeat:
                idx = weightedPick();
                if (n > 1 && idx == m_last) {
                    // Re-roll a bounded number of times; if still stuck (degenerate weights), step off it.
                    for (int t = 0; t < 8 && idx == m_last; ++t) {
                        idx = weightedPick();
                    }
                    if (idx == m_last) {
                        idx = (m_last + 1) % n;
                    }
                }
                break;
        }
        m_last = idx;
        pick.index = idx;

        if (randomPitch > 1.0f) {
            const float k = std::log(randomPitch);
            pick.pitchScale = std::exp(m_rng.range(-1.0f, 1.0f) * k);
        }
        if (randomVolumeOffsetDb > 0.0f) {
            pick.volumeDb = m_rng.range(-randomVolumeOffsetDb, randomVolumeOffsetDb);
        }
        return pick;
    }

private:
    int weightedPick() {
        float total = 0.0f;
        for (float w : m_weights) {
            total += w;
        }
        const int n = static_cast<int>(m_weights.size());
        if (total <= 0.0f) {
            return m_rng.range(0, n - 1);
        }
        float r = m_rng.range(0.0f, total);
        for (int i = 0; i < n; ++i) {
            r -= m_weights[static_cast<std::size_t>(i)];
            if (r < 0.0f) {
                return i;
            }
        }
        return n - 1;
    }

    core::Random m_rng{0xC0FFEEULL};
    std::vector<float> m_weights;
    int m_cursor = 0;
    int m_last = -1;
};

} // namespace maz::audio
