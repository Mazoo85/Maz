#pragma once

#include <cstdint>

namespace maz::audio {

// The built-in synthesized percussion sounds. No samples on disk — each is generated from an
// oscillator and/or noise plus an amplitude envelope, so the whole kit is self-contained.
enum class Drum {
    Kick,
    Snare,
    ClosedHat,
    OpenHat,
    Clap,
    Tom,
    Cowbell,
    Rimshot,
    Crash,
    Ride,
    Shaker,
    Clave,
    Tambourine,
    Conga,
    Woodblock,
    Bongo,
    Triangle,
    Kick808,
    Zap,
    Riser,
    Snare808,
    Hat808,
    Clap808,
    Snap,
    Timbale,
    Agogo,
    Splash,
    China,
    Guiro
};

// The General-MIDI channel-10 percussion note each drum type maps to, for MIDI export/import. Every
// type gets a distinct note so a pattern round-trips through a .mid regardless of which drums are on
// which channels (the old export hard-coded only the first five channels by position).
int gmNoteForDrum(Drum type);

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

    // Pitch-envelope depth / "punch" (0..2): scales the built-in initial pitch sweep of the tonal
    // drums (kick, tom). 1 = the natural sweep; 0 = a flat, sub-only tone (no click); 2 = a deeper,
    // snappier sweep (more attack/punch). No effect on the noise-based voices (hats/clap/etc.).
    void setPitchEnv(float amount) { pitchEnv_ = amount < 0.0f ? 0.0f : (amount > 2.0f ? 2.0f : amount); }
    float pitchEnv() const { return pitchEnv_; }

    // Pitch-envelope time (0.25..4): scales how long the kick/tom pitch sweep takes to fall. 1 = the
    // natural time; higher = a slower drop (a longer, boomier 808-style tail); lower = a tighter,
    // snappier click. Works with the pitch-env depth above to fully shape the tonal drums' attack.
    void setPitchEnvTime(float mul) { pitchEnvTime_ = mul < 0.25f ? 0.25f : (mul > 4.0f ? 4.0f : mul); }
    float pitchEnvTime() const { return pitchEnvTime_; }

    // Tone: a per-voice one-pole low-pass that darkens the hit (FL-style channel filter). Cutoff in
    // Hz; 20000 (default) = fully open/bypassed (bit-transparent). Lower it to tame bright hats, take
    // the edge off a snare, or round a kick.
    void setToneCutoff(float hz) { toneCutoff_ = hz < 200.0f ? 200.0f : (hz > 20000.0f ? 20000.0f : hz); }
    float toneCutoff() const { return toneCutoff_; }

    // High-pass / low-cut: a per-voice one-pole high-pass that thins the hit — tighten a boomy kick,
    // shave rumble off hats/claps (the other half of an FL channel filter). Cutoff in Hz; 0 (default)
    // = off/bypassed (bit-transparent). Up to 2 kHz.
    void setHighpassCutoff(float hz) { hpCutoff_ = hz < 0.0f ? 0.0f : (hz > 2000.0f ? 2000.0f : hz); }
    float highpassCutoff() const { return hpCutoff_; }

    // Snap (0..1): the snare's noise-vs-tone balance. 0 = all body (the tuned tone, a tom-like
    // thud), 1 = all snare wires (bright noise crack), 0.5 (default) = the classic mix. Only affects
    // the Snare voice.
    void setSnap(float s) { snap_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    float snap() const { return snap_; }

    // Strike the drum: reset the envelope/phase and start sounding. `velocity` (0..1) scales the
    // hit's loudness for per-step accents; `extraSemitones` is a per-hit pitch offset (added to the
    // channel's base tune) captured at strike time, for per-step pitch.
    void trigger(float velocity = 1.0f, float extraSemitones = 0.0f, float pan = 0.0f);

    // Stereo pan of the current hit (−1 left … +1 right), captured at trigger time for per-step pan.
    // The voice renders mono; the caller applies this at its stereo bus mix.
    float pan() const { return pan_; }

    // Choke this voice: ramp it to silence over a few ms (click-free), used by choke groups so one
    // drum cuts off another (e.g. a closed hat chokes an open hat).
    void choke();

    // True while the hit is still ringing.
    bool active() const { return active_; }

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is in Hz. No-op when
    // inactive, so it is cheap to call on every voice every block.
    void render(float* out, int frames, int sampleRate);

    // Seed the per-voice noise generator. Distinct seeds decorrelate the noise of two voices playing
    // the same drum type at once (e.g. two channels both set to Closed Hat), so they sum incoherently
    // (~+3 dB) like real hits instead of doubling coherently (~+6 dB). Deterministic per seed.
    void setNoiseSeed(uint32_t seed) { rng_ = seed != 0u ? seed : 0x1234567u; }

private:
    // Deterministic white-noise source (xorshift), seeded per-voice so hits are reproducible.
    float noise();

    Drum type_ = Drum::Kick;
    float gain_ = 0.8f;
    float level_ = 1.0f;
    float pan_ = 0.0f; // per-hit stereo pan (−1..+1), captured at trigger
    float tuneSemitones_ = 0.0f;
    float hitTune_ = 0.0f; // pitch (base + per-step offset) captured at the last trigger()
    float decayMul_ = 1.0f;
    float drive_ = 0.0f; // tanh saturation amount; 0 = clean
    float pitchEnv_ = 1.0f; // scales the tonal drums' initial pitch sweep depth; 1 = natural
    float pitchEnvTime_ = 1.0f; // scales the pitch-sweep time constant; 1 = natural
    float toneCutoff_ = 20000.0f; // per-voice low-pass cutoff Hz; 20000 = open/bypassed
    float snap_ = 0.5f; // snare noise/tone balance; 0.5 = classic mix
    float toneLp_ = 0.0f;         // one-pole low-pass state for the tone filter
    float hpCutoff_ = 0.0f;       // per-voice high-pass/low-cut cutoff Hz; 0 = off/bypassed
    float hpLp_ = 0.0f;           // one-pole low-pass state (subtracted → high-pass)

    float velocity_ = 1.0f;
    bool active_ = false;
    bool choking_ = false;   // ramping to silence after a choke()
    float chokeGain_ = 1.0f; // current choke fade level
    double t_ = 0.0;      // seconds since trigger()
    double phase_ = 0.0;  // tonal-component phase in [0, 1)
    uint32_t rng_ = 0x1234567u;
};

} // namespace maz::audio
