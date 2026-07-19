#pragma once

#include "maz/audio/Oscillator.hpp" // Waveform + waveSample

#include <cmath>
#include <cstdint>

namespace maz::audio {

// A control-rate low-frequency oscillator used to modulate parameters over time. It reuses the
// audio waveforms; valueBipolar(t) returns the shape in [-1, 1] at absolute time t (seconds), so it
// is stateless and deterministic — the same t always gives the same value.
struct LFO {
    Waveform shape = Waveform::Sine;
    float rateHz = 1.0f;
    float phase = 0.0f; // starting phase offset in [0, 1)
    // Sample & hold: instead of the periodic `shape`, emit a pseudo-random value that holds constant
    // for each cycle and jumps to a new random level at the start of the next — the classic stepped
    // "random" LFO for generative movement (random filter jumps, panning, pitch, …). Deterministic:
    // the value is a hash of the integer cycle index, so the same t always gives the same output.
    bool sampleHold = false;

    float valueBipolar(double t) const {
        double p = static_cast<double>(rateHz) * t + static_cast<double>(phase);
        if (sampleHold) {
            // Hash the integer cycle index to a well-distributed value in [-1, 1) that holds for the
            // whole cycle (same floor(p) → same output) and decorrelates from one cycle to the next.
            const long long step = static_cast<long long>(std::floor(p));
            uint32_t x = static_cast<uint32_t>(step) * 2654435761u + 0x9E3779B9u;
            x ^= x >> 15;
            x *= 0x85EBCA6Bu;
            x ^= x >> 13;
            x *= 0xC2B2AE35u;
            x ^= x >> 16;
            return static_cast<float>(x) / 2147483648.0f - 1.0f;
        }
        p -= std::floor(p);
        return waveSample(shape, p);
    }

    // Convenience: 0..1 unipolar.
    float valueUnipolar(double t) const { return valueBipolar(t) * 0.5f + 0.5f; }
};

} // namespace maz::audio
