#pragma once

#include <cstdint>

namespace maz::audio {

// The built-in synthesized percussion sounds. No samples on disk — each is generated from an
// oscillator and/or noise plus an amplitude envelope, so the whole kit is self-contained.
enum class Drum { Kick, Snare, ClosedHat, OpenHat, Clap };

// A one-shot percussion voice: trigger() (re)starts the hit, render() ADDS the decaying sound into
// the caller's buffer, and the voice goes inactive once its envelope falls to silence. Pure DSP —
// no SDL, no device — so it is unit-testable on its own and safe to mix many at once.
class DrumVoice {
public:
    void setType(Drum type) { type_ = type; }
    Drum type() const { return type_; }

    void setGain(float gain) { gain_ = gain; }
    float gain() const { return gain_; }

    // Per-channel mixer level (separate from the kit's base gain), driven by the mixer UI.
    void setLevel(float level) { level_ = level; }
    float level() const { return level_; }

    // Strike the drum: reset the envelope/phase and start sounding.
    void trigger();

    // True while the hit is still ringing.
    bool active() const { return active_; }

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is in Hz. No-op when
    // inactive, so it is cheap to call on every voice every block.
    void render(float* out, int frames, int sampleRate);

private:
    // Deterministic white-noise source (xorshift), seeded per-voice so hits are reproducible.
    float noise();

    Drum type_ = Drum::Kick;
    float gain_ = 0.8f;
    float level_ = 1.0f;
    bool active_ = false;
    double t_ = 0.0;      // seconds since trigger()
    double phase_ = 0.0;  // tonal-component phase in [0, 1)
    uint32_t rng_ = 0x1234567u;
};

} // namespace maz::audio
