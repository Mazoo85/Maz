#pragma once

#include <algorithm> // std::clamp
#include <cstdint>

// maz::audio noise-colour generators — the coloured noise sources procedural sound design leans on. WHITE
// noise (flat spectrum) is the raw "static" hiss; PINK noise (1/f, equal energy per octave) is the natural,
// balanced "shhh" of steady rain, a waterfall, ocean surf, or a ventilation hum, and the reference signal
// audio engineers test systems with; BROWN / red noise (1/f², even more low-end) is the deep rumble of
// distant thunder, heavy wind, or a rocket. The engine's Oscillator already had a white source baked in for
// its waveform enum; this exposes all three colours as small, reusable, DETERMINISTIC generators (each seeded
// from an xorshift PRNG) that a mixer, SFX synth, or wind/ambience layer can pull samples from. Output is in
// roughly [-1, 1]. Header-only, std-only, per-sample — unit-testable to the bit.
namespace maz::audio {

// White noise: a flat-spectrum source from a fast seeded xorshift32 PRNG. next() returns a fresh sample in
// [-1, 1). Same seed -> same sequence.
class WhiteNoise {
public:
    explicit WhiteNoise(std::uint32_t seed = 1u) : m_state(seed ? seed : 1u) {}

    void reset(std::uint32_t seed = 1u) { m_state = seed ? seed : 1u; }

    float next() {
        // xorshift32
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return static_cast<float>(m_state) / 2147483648.0f - 1.0f; // [0, 2^32) -> [-1, 1)
    }

private:
    std::uint32_t m_state;
};

// Pink noise (1/f, equal energy per octave) via Paul Kellet's economical filtered-white approximation — the
// standard, cheap, well-behaved pink source. Output normalized to roughly [-1, 1] (clamped for safety).
class PinkNoise {
public:
    explicit PinkNoise(std::uint32_t seed = 1u) : m_white(seed) {}

    void reset(std::uint32_t seed = 1u) {
        m_white.reset(seed);
        m_b0 = m_b1 = m_b2 = m_b3 = m_b4 = m_b5 = m_b6 = 0.0f;
    }

    float next() {
        const float w = m_white.next();
        m_b0 = 0.99886f * m_b0 + w * 0.0555179f;
        m_b1 = 0.99332f * m_b1 + w * 0.0750759f;
        m_b2 = 0.96900f * m_b2 + w * 0.1538520f;
        m_b3 = 0.86650f * m_b3 + w * 0.3104856f;
        m_b4 = 0.55000f * m_b4 + w * 0.5329522f;
        m_b5 = -0.7616f * m_b5 - w * 0.0168980f;
        const float pink = m_b0 + m_b1 + m_b2 + m_b3 + m_b4 + m_b5 + m_b6 + w * 0.5362f;
        m_b6 = w * 0.115926f;
        return std::clamp(pink * 0.11f, -1.0f, 1.0f); // ~0.11 normalizes the filter gain into [-1,1]
    }

private:
    WhiteNoise m_white;
    float m_b0 = 0.0f, m_b1 = 0.0f, m_b2 = 0.0f, m_b3 = 0.0f, m_b4 = 0.0f, m_b5 = 0.0f, m_b6 = 0.0f;
};

// Brown / red noise (1/f²) via a leaky integrator of white noise — a mean-reverting random walk, so it never
// drifts off to a rail the way a pure integral would. Successive samples are strongly correlated (the deep,
// wandering rumble). Output in roughly [-1, 1] (clamped for safety).
class BrownNoise {
public:
    explicit BrownNoise(std::uint32_t seed = 1u) : m_white(seed) {}

    void reset(std::uint32_t seed = 1u) {
        m_white.reset(seed);
        m_last = 0.0f;
    }

    float next() {
        const float w = m_white.next();
        m_last = 0.996f * m_last + 0.04f * w; // leaky integrator: mean-reverting random walk (1/f²-ish)
        return std::clamp(m_last, -1.0f, 1.0f);
    }

private:
    WhiteNoise m_white;
    float m_last = 0.0f;
};

} // namespace maz::audio
