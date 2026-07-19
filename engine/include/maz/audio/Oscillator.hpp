#pragma once

#include <cmath>

namespace maz::audio {

// The available oscillator waveforms. Sine is the default; the rest are cheap analogue-style
// shapes (naive, not band-limited — good enough for the current milestones).
enum class Waveform { Sine, Square, Saw, Triangle, Trapezoid, StepSine, RectSine };

// Evaluate a waveform at a phase in [0, 1). Shared by the Oscillator and the poly synth so there is
// a single source of truth for each shape.
inline float waveSample(Waveform w, double phase) {
    constexpr double kTwoPi = 6.283185307179586;
    switch (w) {
    case Waveform::Sine:
        return static_cast<float>(std::sin(phase * kTwoPi));
    case Waveform::Square:
        return (phase < 0.5) ? 1.0f : -1.0f;
    case Waveform::Saw:
        return static_cast<float>(2.0 * phase - 1.0);
    case Waveform::Triangle:
        return static_cast<float>(4.0 * std::fabs(phase - 0.5) - 1.0);
    case Waveform::Trapezoid: {
        // A triangle whose peaks are clamped into flat plateaus: linear ramps like a triangle but
        // flat tops like a square, so its (odd-only) harmonics roll off between the two — a warm,
        // hollow tone brighter than a triangle yet softer than a square. DC-free and symmetric.
        const float tri = static_cast<float>(4.0 * std::fabs(phase - 0.5) - 1.0);
        const float t = tri * 2.0f;
        return t < -1.0f ? -1.0f : (t > 1.0f ? 1.0f : t);
    }
    case Waveform::StepSine: {
        // A sine quantized to a small ladder of amplitude steps — a lo-fi / chiptune tone. The
        // ladder is symmetric about zero so the wave stays DC-free, and the fundamental (its zero
        // crossings) matches a pure sine, but the staircase adds harmonics for extra brightness.
        constexpr double kHalfLevels = 4.0; // steps per polarity → 9 distinct levels across [-1, 1]
        const double s = std::sin(phase * kTwoPi);
        return static_cast<float>(std::round(s * kHalfLevels) / kHalfLevels);
    }
    case Waveform::RectSine: {
        // A half-wave rectified sine: the positive half of the sine, the negative half flattened to
        // zero, then DC-removed and normalized. It keeps the fundamental (same period as a sine) but
        // adds strong even harmonics for a bright, reedy/hollow tone distinct from the pure sine.
        const double s = std::sin(phase * kTwoPi);
        const double half = s > 0.0 ? s : 0.0;       // half-wave rectify
        constexpr double kMean = 0.3183098861837907; // 1/pi = the DC of a half-wave sine
        constexpr double kScale = 1.0 / (1.0 - kMean); // so the positive peak lands at +1
        return static_cast<float>((half - kMean) * kScale);
    }
    }
    return 0.0f;
}

// Variable-pulse-width variant: for a Square wave, `pulseWidth` (0..1) sets the duty cycle — the
// fraction of each cycle spent high — so 0.5 is the plain square and other values give a pulse with
// a different harmonic balance (classic PWM tone). Every other waveform ignores pulseWidth and
// matches the plain waveSample above, so this is a drop-in for the square path.
inline float waveSample(Waveform w, double phase, float pulseWidth) {
    if (w == Waveform::Square) {
        return (phase < static_cast<double>(pulseWidth)) ? 1.0f : -1.0f;
    }
    return waveSample(w, phase);
}

// A single monophonic oscillator voice with a short click-free amplitude envelope.
//
// Pure DSP: it touches no SDL, no device, and no global state, so it can be unit-tested on its own
// (see tests/unit_audio.cpp). noteOn()/noteOff() gate a linear attack/release ramp so starting and
// stopping a tone does not click. render() ADDS its output into the caller's buffer so a future
// mixer can sum many voices.
class Oscillator {
public:
    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setAmplitude(float a) { amplitude_ = a; }
    float amplitude() const { return amplitude_; }

    void setFrequency(float hz) { frequency_ = hz; }
    float frequency() const { return frequency_; }

    // Start sounding at freqHz (opens the gate → attack ramp).
    void noteOn(float freqHz);
    // Stop sounding (closes the gate → release ramp). The voice keeps emitting until the ramp
    // reaches zero, which is why active() can stay true briefly after noteOff().
    void noteOff();

    // True while the note is held or the release ramp has not yet reached silence.
    bool active() const { return gate_ || env_ > 0.0f; }

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

private:
    Waveform waveform_ = Waveform::Sine;
    float frequency_ = 440.0f;
    float amplitude_ = 0.5f;
    double phase_ = 0.0; // cycle position in [0, 1)
    bool gate_ = false;  // is the note currently held?
    float env_ = 0.0f;   // current envelope level in [0, 1]
};

} // namespace maz::audio
