#include "maz/audio/Oscillator.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

namespace {
constexpr double kTwoPi = 6.283185307179586;
// Attack/release length in seconds — a few milliseconds, long enough to kill clicks, short enough
// to feel instant.
constexpr float kRampSeconds = 0.005f;
} // namespace

void Oscillator::noteOn(float freqHz) {
    frequency_ = freqHz;
    gate_ = true;
}

void Oscillator::noteOff() {
    gate_ = false;
}

void Oscillator::render(float* out, int frames, int sampleRate) {
    if (sampleRate <= 0 || frames <= 0) {
        return;
    }
    const double phaseInc = static_cast<double>(frequency_) / static_cast<double>(sampleRate);
    const float envStep = 1.0f / (kRampSeconds * static_cast<float>(sampleRate));

    for (int i = 0; i < frames; ++i) {
        // Advance the envelope one step toward its target (1 while gated, 0 while released).
        const float target = gate_ ? 1.0f : 0.0f;
        if (env_ < target) {
            env_ = std::min(target, env_ + envStep);
        } else if (env_ > target) {
            env_ = std::max(target, env_ - envStep);
        }

        float s = 0.0f;
        switch (waveform_) {
        case Waveform::Sine:
            s = static_cast<float>(std::sin(phase_ * kTwoPi));
            break;
        case Waveform::Square:
            s = (phase_ < 0.5) ? 1.0f : -1.0f;
            break;
        case Waveform::Saw:
            s = static_cast<float>(2.0 * phase_ - 1.0);
            break;
        case Waveform::Triangle:
            s = static_cast<float>(4.0 * std::fabs(phase_ - 0.5) - 1.0);
            break;
        }

        out[i] += s * amplitude_ * env_;

        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

} // namespace maz::audio
