#pragma once

#include <algorithm>
#include <cmath>

namespace maz::audio {

// A topology-preserving-transform (TPT / Zavalishin) state-variable filter. One instance filters
// one mono signal and holds two state variables. It produces low-pass, high-pass, and band-pass
// outputs simultaneously and stays stable across the full cutoff range with resonance up to
// self-oscillation. This is the building block that makes the synth genuinely subtractive.
struct StateVariableFilter {
    float ic1 = 0.0f; // integrator states
    float ic2 = 0.0f;

    enum class Mode { LowPass, HighPass, BandPass };

    void reset() {
        ic1 = 0.0f;
        ic2 = 0.0f;
    }

    // Process one sample. `cutoffHz` is clamped to a sane range; `resonance` in ~[0.5, 20] (higher =
    // sharper peak). `mode` selects which output to return.
    float process(float x, float cutoffHz, float resonance, int sampleRate, Mode mode) {
        const float fc =
            std::clamp(cutoffHz, 20.0f, static_cast<float>(sampleRate) * 0.45f);
        const float g = std::tan(3.14159265f * fc / static_cast<float>(sampleRate));
        const float k = 1.0f / std::max(resonance, 0.5f);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        switch (mode) {
        case Mode::LowPass:
            return v2;
        case Mode::HighPass:
            return x - k * v1 - v2;
        case Mode::BandPass:
            return v1;
        }
        return v2;
    }
};

} // namespace maz::audio
