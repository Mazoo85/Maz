#pragma once

#include <cmath>
#include <cstdint>

namespace maz::audio {

// Oscillator / procedural tone generator — Godot's AudioStreamGenerator source material. Where the rest
// of the audio module PROCESSES incoming sound, this GENERATES it: the raw waveforms a synth voice is
// built from — sine, sawtooth, square/pulse, triangle, and white noise — at a chosen frequency, plus a
// detune ratio, phase modulation input (for FM), and a two-operator FM voice. The catch with naive
// digital saw/square is ALIASING: their sharp edges contain harmonics above the Nyquist limit that fold
// back as inharmonic "grit". This uses PolyBLEP (polynomial band-limited step) to round those edges so
// the saw and square stay clean across the musical range — the same anti-aliasing real soft-synths use.
// Pure per-sample math, deterministic (the noise source is a seeded xorshift), so it unit-tests exactly
// (a sine hits 0,1,0,-1 at quarter phases; the band-limited saw's edge jump is softened below the naive
// 2.0 step) and drives a golden waveform gallery.
//
// Scope note (honest): saw and square are PolyBLEP band-limited (the aliasing-prone shapes); sine is
// exact and triangle is the direct piecewise form (its harmonics roll off as 1/n^2, so its aliasing is
// minor). A full wavetable-with-mip synthesis path and higher-order BLAMP triangle correction are
// natural follow-ups.

enum class Waveform { Sine, Saw, Square, Triangle, Noise };

// PolyBLEP correction at normalized phase `t` (0..1) with per-sample phase step `dt`. Adds a smooth
// 2-sample polynomial around a step discontinuity so the edge is band-limited instead of infinitely sharp.
inline float polyBlep(float t, float dt) {
    if (dt <= 0.0f) {
        return 0.0f;
    }
    if (t < dt) {
        const float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt) {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

struct Oscillator {
    float freq = 220.0f;
    float sampleRate = 44100.0f;
    Waveform waveform = Waveform::Sine;
    float pulseWidth = 0.5f; // square/pulse duty in (0,1)
    float detune = 1.0f;     // frequency multiplier (1 = none)

    // Advance one sample and return it. `phaseMod` (in cycles) is added to the phase before shaping — the
    // input for phase-modulation / FM.
    float next(float phaseMod = 0.0f) {
        const float dt = (freq * detune) / sampleRate;
        float p = m_phase + phaseMod;
        p -= std::floor(p); // wrap to [0,1)

        float out = 0.0f;
        switch (waveform) {
        case Waveform::Sine:
            out = std::sin(6.28318530718f * p);
            break;
        case Waveform::Saw:
            out = 2.0f * p - 1.0f - polyBlep(p, dt);
            break;
        case Waveform::Square: {
            float sq = p < pulseWidth ? 1.0f : -1.0f;
            sq += polyBlep(p, dt); // rising edge at phase wrap
            float pf = p - pulseWidth;
            pf -= std::floor(pf);
            sq -= polyBlep(pf, dt); // falling edge at the duty point
            out = sq;
            break;
        }
        case Waveform::Triangle:
            out = p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            break;
        case Waveform::Noise:
            out = whiteNoise();
            break;
        }

        m_phase += dt;
        m_phase -= std::floor(m_phase);
        return out;
    }

    void reset(std::uint32_t seed = 1u) {
        m_phase = 0.0f;
        m_noise = seed ? seed : 1u;
    }
    void setPhase(float ph) { m_phase = ph - std::floor(ph); }
    float phase() const { return m_phase; }

private:
    float whiteNoise() {
        // xorshift32 -> [-1, 1]
        m_noise ^= m_noise << 13;
        m_noise ^= m_noise >> 17;
        m_noise ^= m_noise << 5;
        return static_cast<float>(m_noise) / 2147483647.5f - 1.0f;
    }

    float m_phase = 0.0f;
    std::uint32_t m_noise = 1u;
};

// Two-operator FM voice — a modulator oscillator phase-modulates a carrier. `index` scales how much the
// modulator bends the carrier's phase (the classic FM "brightness" control). Godot has no built-in FM
// operator, so this is a small bonus toward richer synthesis.
struct FMVoice {
    Oscillator carrier;
    Oscillator modulator;
    float index = 1.0f; // modulation depth (in cycles)

    float next() {
        const float m = modulator.next();
        return carrier.next(m * index);
    }
    void reset() {
        carrier.reset();
        modulator.reset();
    }
};

} // namespace maz::audio
