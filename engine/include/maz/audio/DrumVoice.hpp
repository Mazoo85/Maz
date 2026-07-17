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

    // Tune the drum's pitch in semitones (±). Scales the tonal components (kick sweep, snare body) by
    // 2^(semitones/12); noise-only voices (hats/clap) are unaffected.
    void setTune(float semitones) { tuneSemitones_ = semitones; }
    float tune() const { return tuneSemitones_; }

    // Decay multiplier (0.25–4): scales the drum's envelope length — shorter (tight) or longer
    // (boomy) tails. 1 = the voice's natural decay.
    void setDecay(float mul) { decayMul_ = mul < 0.25f ? 0.25f : (mul > 4.0f ? 4.0f : mul); }
    float decay() const { return decayMul_; }

    // Per-channel mixer level (separate from the kit's base gain), driven by the mixer UI.
    void setLevel(float level) { level_ = level; }
    float level() const { return level_; }

    // Drive / saturation (0..1): 0 is clean (bit-transparent); higher values push the hit through a
    // tanh soft-clipper that adds harmonics and grit — punchier, more aggressive drums.
    void setDrive(float d) { drive_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    float drive() const { return drive_; }

    // Strike the drum: reset the envelope/phase and start sounding. `velocity` (0..1) scales the
    // hit's loudness for per-step accents.
    void trigger(float velocity = 1.0f);

    // Choke this voice: ramp it to silence over a few ms (click-free), used by choke groups so one
    // drum cuts off another (e.g. a closed hat chokes an open hat).
    void choke();

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
    float tuneSemitones_ = 0.0f;
    float decayMul_ = 1.0f;
    float drive_ = 0.0f; // tanh saturation amount; 0 = clean

    float velocity_ = 1.0f;
    bool active_ = false;
    bool choking_ = false;   // ramping to silence after a choke()
    float chokeGain_ = 1.0f; // current choke fade level
    double t_ = 0.0;      // seconds since trigger()
    double phase_ = 0.0;  // tonal-component phase in [0, 1)
    uint32_t rng_ = 0x1234567u;
};

} // namespace maz::audio
