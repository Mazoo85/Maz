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

// A biquad filter (transposed direct-form II) with RBJ-cookbook coefficient setters for the shapes
// a parametric EQ needs: peaking, low-shelf, and high-shelf. One instance filters one mono channel.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setPeaking(float freq, float q, float gainDb, int sampleRate) {
        const float w0 = 6.2831853f * std::clamp(freq, 20.0f, static_cast<float>(sampleRate) * 0.45f) /
                         static_cast<float>(sampleRate);
        const float cw = std::cos(w0);
        const float sw = std::sin(w0);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float alpha = sw / (2.0f * std::max(q, 0.1f));
        const float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cw) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cw) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void setShelf(float freq, float gainDb, int sampleRate, bool high) {
        const float w0 = 6.2831853f * std::clamp(freq, 20.0f, static_cast<float>(sampleRate) * 0.45f) /
                         static_cast<float>(sampleRate);
        const float cw = std::cos(w0);
        const float sw = std::sin(w0);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float alpha = sw / 2.0f * 1.4142135f; // Q ~ 0.707
        const float tsA = 2.0f * std::sqrt(A) * alpha;
        const float sign = high ? 1.0f : -1.0f;
        const float a0 = (A + 1.0f) - sign * (A - 1.0f) * cw + tsA;
        b0 = A * ((A + 1.0f) + sign * (A - 1.0f) * cw + tsA) / a0;
        b1 = -sign * 2.0f * A * ((A - 1.0f) + sign * (A + 1.0f) * cw) / a0;
        b2 = A * ((A + 1.0f) + sign * (A - 1.0f) * cw - tsA) / a0;
        a1 = sign * 2.0f * ((A - 1.0f) - sign * (A + 1.0f) * cw) / a0;
        a2 = ((A + 1.0f) - sign * (A - 1.0f) * cw - tsA) / a0;
    }
};

} // namespace maz::audio
