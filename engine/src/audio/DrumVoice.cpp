#include "maz/audio/DrumVoice.hpp"

#include <cmath>

namespace maz::audio {

namespace {
constexpr double kTwoPi = 6.283185307179586;

// Longest envelope time-constant per drum; once t exceeds a few of these the hit is silent and the
// voice can deactivate. (tau, in seconds.)
double decayTau(Drum type) {
    switch (type) {
    case Drum::Kick:
        return 0.16;
    case Drum::Snare:
        return 0.12;
    case Drum::ClosedHat:
        return 0.03;
    case Drum::OpenHat:
        return 0.28;
    case Drum::Clap:
        return 0.14;
    case Drum::Tom:
        return 0.20;
    case Drum::Cowbell:
        return 0.12;
    case Drum::Rimshot:
        return 0.05;
    }
    return 0.1;
}
} // namespace

void DrumVoice::trigger(float velocity) {
    active_ = true;
    choking_ = false;
    chokeGain_ = 1.0f;
    t_ = 0.0;
    phase_ = 0.0;
    velocity_ = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
}

void DrumVoice::choke() {
    if (active_) {
        choking_ = true; // render() ramps chokeGain_ to 0 and deactivates
    }
}

float DrumVoice::noise() {
    // xorshift32 → float in [-1, 1).
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.0f - 1.0f;
}

void DrumVoice::render(float* out, int frames, int sampleRate) {
    if (!active_ || sampleRate <= 0 || frames <= 0) {
        return;
    }
    const double dt = 1.0 / static_cast<double>(sampleRate);
    const double tau = decayTau(type_) * static_cast<double>(decayMul_);
    const double pitchMul = std::pow(2.0, static_cast<double>(tuneSemitones_) / 12.0);

    for (int i = 0; i < frames; ++i) {
        const double env = std::exp(-t_ / tau);
        float s = 0.0f;

        switch (type_) {
        case Drum::Kick: {
            // Pitch sweeps from ~120 Hz down to ~45 Hz over the first few ms — the classic thump.
            const double freq = (45.0 + 75.0 * std::exp(-t_ / 0.03)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Snare: {
            const double tone = std::sin(phase_ * kTwoPi);
            const float n = noise();
            s = static_cast<float>((0.4 * tone + 0.9 * static_cast<double>(n)) * env);
            phase_ += 180.0 * pitchMul * dt;
            break;
        }
        case Drum::ClosedHat:
        case Drum::OpenHat:
            s = static_cast<float>(static_cast<double>(noise()) * env);
            break;
        case Drum::Clap:
            // A slightly grittier noise burst.
            s = static_cast<float>(static_cast<double>(noise()) * env * env);
            break;
        case Drum::Tom: {
            // A tuned membrane: a sine with a gentle downward pitch sweep (200 → 100 Hz).
            const double freq = (100.0 + 100.0 * std::exp(-t_ / 0.06)) * pitchMul;
            s = static_cast<float>(std::sin(phase_ * kTwoPi) * env);
            phase_ += freq * dt;
            break;
        }
        case Drum::Cowbell: {
            // The classic 808 cowbell: two detuned square tones (~540 + ~800 Hz) ringing together.
            const double a = std::sin(kTwoPi * 540.0 * pitchMul * t_) >= 0.0 ? 1.0 : -1.0;
            const double b = std::sin(kTwoPi * 800.0 * pitchMul * t_) >= 0.0 ? 1.0 : -1.0;
            s = static_cast<float>(0.5 * (a + b) * env);
            break;
        }
        case Drum::Rimshot: {
            // A short, bright crack: a high tone (~1700 Hz) plus a noise click, very fast decay.
            const double tone = std::sin(kTwoPi * 1700.0 * pitchMul * t_);
            s = static_cast<float>((0.7 * tone + 0.3 * static_cast<double>(noise())) * env);
            break;
        }
        }

        if (phase_ >= 1.0) {
            phase_ -= std::floor(phase_);
        }

        // Drive: push the hit through a tanh soft-clipper (normalised so full-scale stays ~unity) to
        // add harmonics and grit. Skipped entirely at 0 so the clean drum is bit-identical.
        if (drive_ > 0.0f) {
            const float k = 1.0f + drive_ * 8.0f;
            s = std::tanh(s * k) / std::tanh(k);
        }

        // Choke fade: ~4 ms ramp to silence, then the voice deactivates.
        if (choking_) {
            chokeGain_ -= static_cast<float>(dt) / 0.004f;
            if (chokeGain_ <= 0.0f) {
                chokeGain_ = 0.0f;
                active_ = false;
            }
            s *= chokeGain_;
        }

        out[i] += s * gain_ * level_ * velocity_;
        t_ += dt;

        if (!active_) {
            break; // choked out mid-block
        }
    }

    // Deactivate once the envelope has effectively reached silence (~5 time-constants).
    if (std::exp(-t_ / tau) < 0.002) {
        active_ = false;
    }
}

} // namespace maz::audio
