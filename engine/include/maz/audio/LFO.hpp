#pragma once

#include "maz/audio/Oscillator.hpp" // Waveform + waveSample

#include <cmath>

namespace maz::audio {

// A control-rate low-frequency oscillator used to modulate parameters over time. It reuses the
// audio waveforms; valueBipolar(t) returns the shape in [-1, 1] at absolute time t (seconds), so it
// is stateless and deterministic — the same t always gives the same value.
struct LFO {
    Waveform shape = Waveform::Sine;
    float rateHz = 1.0f;
    float phase = 0.0f; // starting phase offset in [0, 1)

    float valueBipolar(double t) const {
        double p = static_cast<double>(rateHz) * t + static_cast<double>(phase);
        p -= std::floor(p);
        return waveSample(shape, p);
    }

    // Convenience: 0..1 unipolar.
    float valueUnipolar(double t) const { return valueBipolar(t) * 0.5f + 0.5f; }
};

} // namespace maz::audio
