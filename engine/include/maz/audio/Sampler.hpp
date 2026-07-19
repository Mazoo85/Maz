#pragma once

#include "maz/audio/Filter.hpp"

#include <array>
#include <string>
#include <vector>

namespace maz::audio {

// A polyphonic sample-playback instrument: it holds one mono sample and plays it back pitch-shifted
// per MIDI note (playback rate = the note's frequency relative to the sample's base note, corrected
// for any sample-rate difference), with linear interpolation and a short amp envelope to avoid
// clicks. One-shot: a voice plays to the end of the sample (or until its release finishes). Pure
// DSP — no SDL — so it is unit-testable. This is the DAW's multisampler instrument.
class Sampler {
public:
    static constexpr int kMaxVoices = 16;

    // Load a 16-bit PCM WAV (downmixed to mono). Returns false + sets *err on failure.
    bool load(const std::string& path, std::string* err = nullptr);
    // Inject a mono sample directly (for tests / procedural samples).
    void setSampleMono(std::vector<float> mono, int sampleRate);

    bool loaded() const { return !sample_.empty(); }
    const std::string& path() const { return path_; }

    // Peak absolute amplitude of the loaded sample (0 if empty/silent) — a level readout for the UI.
    float samplePeak() const;
    // Normalize the loaded sample so its peak reaches full scale (±1.0), preserving its shape. No-op
    // on an empty or silent sample. A one-shot edit of the in-memory sample (like Edison's Normalize).
    void normalize();
    // Number of frames in the loaded sample, and a bounds-checked read of one frame (0 out of range)
    // — enough for a UI waveform display or tests.
    size_t sampleLength() const { return sample_.size(); }
    float sampleValue(size_t i) const { return i < sample_.size() ? sample_[i] : 0.0f; }
    // Apply a linear fade-in over the first `ms` and a fade-out over the last `ms` of the sample, to
    // remove clicks at the edges of a one-shot/slice. A one-shot in-memory edit (like Edison's Fade).
    void fadeEdges(float ms);
    // Loop crossfade: blend the `ms` of audio approaching the loop end with the `ms` approaching the
    // loop start, so the seam where playback wraps loopEnd→loopStart is smooth (no click) — the
    // classic sustain-loop crossfade (DirectWave/Slicex). A one-shot destructive edit of the loaded
    // sample; needs audio before loopStart to blend from, so it is a no-op when loopStart is at 0 or
    // the region is too short. Returns the crossfade length applied in frames (0 if nothing changed).
    int crossfadeLoop(float ms);

    void setBasePitch(int midi) { basePitch_ = midi; }
    int basePitch() const { return basePitch_; }
    // Fine tune in cents (-1200..+1200): shifts the playback rate for subtle detune or octave shifts,
    // on top of the per-note pitch. 0 = no detune.
    void setDetuneCents(float cents) {
        detuneCents_ = cents < -1200.0f ? -1200.0f : (cents > 1200.0f ? 1200.0f : cents);
    }
    float detuneCents() const { return detuneCents_; }
    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // Reverse: play the sample backwards (from the end). Loop: instead of stopping at the end (or
    // start, when reversed), wrap around and keep sounding until noteOff — turning a one-shot into a
    // sustained/looped instrument.
    void setReverse(bool on) { reverse_ = on; }
    bool reverse() const { return reverse_; }
    void setLoop(bool on) { loop_ = on; }
    bool loop() const { return loop_; }
    // Ping-pong loop: instead of wrapping at the loop boundary, playback reverses direction and
    // bounces back and forth between the sample ends — a seamless, discontinuity-free sustain.
    // Only has an effect when loop is on. `reverse` sets the initial direction.
    void setPingPong(bool on) { pingPong_ = on; }
    bool pingPong() const { return pingPong_; }

    // Start offset: where each note begins reading, as a fraction [0,1) of the sample length — skip
    // leading silence or play from a slice point. (In reverse mode this is where playback starts
    // before running backwards.)
    void setStartOffset(float frac) { startOffset_ = frac < 0.0f ? 0.0f : (frac > 0.999f ? 0.999f : frac); }
    float startOffset() const { return startOffset_; }

    // Loop region: when loop is on, playback cycles only within [loopStart, loopEnd] (fractions of the
    // sample length) instead of the whole sample — so the attack portion before loopStart plays once
    // and just the sustain body loops (the classic sampler sustain loop). Defaults to the full sample
    // (0..1), which reproduces the old whole-sample loop. `end` must exceed `start`; a degenerate
    // region is ignored. No effect when loop is off.
    void setLoopRegion(float start, float end) {
        const float s = start < 0.0f ? 0.0f : (start > 0.999f ? 0.999f : start);
        const float e = end < 0.0f ? 0.0f : (end > 1.0f ? 1.0f : end);
        if (e > s) {
            loopStart_ = s;
            loopEnd_ = e;
        }
    }
    float loopStart() const { return loopStart_; }
    float loopEnd() const { return loopEnd_; }

    // Beat slicer (FL Slicex-style): divide the sample into `n` equal slices mapped across the
    // keyboard from the base note up — note basePitch plays slice 0, basePitch+1 slice 1, and so on
    // (clamped to the available slices). Each note plays its slice once at natural speed (pitch is
    // ignored) and stops at the slice boundary. n = 1 (default) disables slicing (normal playback).
    void setSlices(int n) { slices_ = n < 1 ? 1 : (n > 64 ? 64 : n); }
    int slices() const { return slices_; }

    // Resonant low-pass filter on playback (per voice): `cutoff` in Hz shapes the sample's tone,
    // `resonance` sharpens the peak. Set cutoff high (e.g. 20000, default) to bypass — a fresh sampler
    // is unfiltered. The classic sampler channel filter for darkening/shaping loops and one-shots.
    void setFilter(float cutoffHz, float resonance) {
        filterCutoff_ = cutoffHz < 20.0f ? 20.0f : (cutoffHz > 20000.0f ? 20000.0f : cutoffHz);
        filterReso_ = resonance < 0.5f ? 0.5f : (resonance > 20.0f ? 20.0f : resonance);
    }
    float filterCutoff() const { return filterCutoff_; }
    float filterResonance() const { return filterReso_; }

    // Filter envelope: its own ADSR (seconds / sustain 0..1) sweeps the playback filter cutoff by
    // `depth` Hz (±) — a filter pluck/wow on the sample, independent of the amp envelope. depth 0 = off.
    void setFilterEnvelope(float attack, float decay, float sustain, float release) {
        fEnvA_ = attack < 0.0001f ? 0.0001f : attack;
        fEnvD_ = decay < 0.0001f ? 0.0001f : decay;
        fEnvS_ = sustain < 0.0f ? 0.0f : (sustain > 1.0f ? 1.0f : sustain);
        fEnvR_ = release < 0.0001f ? 0.0001f : release;
    }
    void setFilterEnvDepth(float hz) {
        filterEnvDepth_ = hz < -18000.0f ? -18000.0f : (hz > 18000.0f ? 18000.0f : hz);
    }
    // Velocity → filter cutoff (Hz added at full velocity): harder hits open the filter for dynamic,
    // touch-sensitive brightness. 0 (default) = off (velocity affects only loudness).
    void setFilterVelo(float hz) {
        filterVelo_ = hz < 0.0f ? 0.0f : (hz > 18000.0f ? 18000.0f : hz);
    }
    float filterVelo() const { return filterVelo_; }
    float filterEnvAttack() const { return fEnvA_; }
    float filterEnvDecay() const { return fEnvD_; }
    float filterEnvSustain() const { return fEnvS_; }
    float filterEnvRelease() const { return fEnvR_; }
    float filterEnvDepth() const { return filterEnvDepth_; }

    // Pitch envelope: each note starts `depth` semitones (±36) away from its pitch and slides to the
    // true pitch over `timeSec` — the classic sampler pitch "blip"/drop for zaps, risers, and 808
    // glides. depth 0 = off (a fresh sampler plays at a steady pitch). timeSec is the slide length.
    void setPitchEnv(float depthSemitones, float timeSec) {
        pitchEnvDepth_ =
            depthSemitones < -36.0f ? -36.0f : (depthSemitones > 36.0f ? 36.0f : depthSemitones);
        pitchEnvTime_ = timeSec < 0.001f ? 0.001f : (timeSec > 2.0f ? 2.0f : timeSec);
    }
    float pitchEnvDepth() const { return pitchEnvDepth_; }
    float pitchEnvTime() const { return pitchEnvTime_; }

    // Amplitude envelope (seconds): a click-free attack ramp on trigger and a release fade on
    // noteOff. Longer release lets sustained/looped samples fade out smoothly.
    void setAmpEnv(float attackSec, float releaseSec);
    // Decay/sustain complete the amp ADSR: after the attack peaks the level decays over `decaySec`
    // to the `sustain` level (0..1), held while the note is on, then the release fades from there.
    // sustain = 1 (default) makes decay a no-op — the plain attack/hold/release of setAmpEnv.
    void setAmpDecay(float decaySec) { ampDecay_ = decaySec < 0.0001f ? 0.0001f : decaySec; }
    void setAmpSustain(float s) { ampSustain_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    float attack() const { return attack_; }
    float release() const { return release_; }
    float ampDecay() const { return ampDecay_; }
    float ampSustain() const { return ampSustain_; }

    // Monophonic mode: play a single voice with last-note priority — each new note steals the one
    // before it, for tight mono sampled bass/leads (no overlapping tails). Off (default) = polyphonic.
    void setMono(bool on) { mono_ = on; }
    bool mono() const { return mono_; }

    void noteOn(int midi, float velocity);
    void noteOff(int midi);
    void allNotesOff();
    bool active() const;
    int activeVoices() const; // number of voices currently sounding (for the UI / tests)

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is the engine rate.
    void render(float* out, int frames, int sampleRate);

private:
    struct Voice {
        bool active = false;
        bool releasing = false;
        int midi = -1;
        double pos = 0.0; // fractional read index into sample_
        int dir = 1;      // playback direction: +1 forward, -1 backward (flips on ping-pong bounce)
        bool sliced = false; // this voice plays a fixed slice (one-shot, ignores loop/reverse)
        double sliceEnd = 0.0; // read index at which a sliced voice stops
        float velocity = 0.0f;
        float env = 0.0f;
        int ampStage = 0;                     // amp ADSR: 0=attack, 1=decay, 2=sustain
        float filtEnv = 0.0f;                 // dedicated filter-envelope level [0,1]
        int filtStage = 0;                    // 0=attack, 1=decay, 2=sustain, 3=release
        double penv = 0.0;                    // pitch-envelope level [1→0]; scales the initial pitch offset
        StateVariableFilter filter{}; // per-voice playback filter
    };

    std::vector<float> sample_;
    int sampleSr_ = 48000;
    int basePitch_ = 60;
    float detuneCents_ = 0.0f;
    float gain_ = 0.9f;
    bool reverse_ = false;
    bool mono_ = false; // monophonic (single-voice, last-note priority) mode
    bool loop_ = false;
    bool pingPong_ = false;
    float startOffset_ = 0.0f;
    int slices_ = 1;         // beat-slicer slice count (1 = off, normal playback)
    float loopStart_ = 0.0f; // loop region start as a fraction of the sample (0 = sample start)
    float loopEnd_ = 1.0f;   // loop region end as a fraction of the sample (1 = sample end)
    float filterCutoff_ = 20000.0f; // playback low-pass cutoff Hz (20000 = open/bypass)
    float filterReso_ = 0.7f;       // playback low-pass resonance
    float ampDecay_ = 0.05f;   // amp-envelope decay time (s); no-op while ampSustain_ == 1
    float ampSustain_ = 1.0f;  // amp-envelope sustain level (0..1); 1 = plain attack/hold/release
    float fEnvA_ = 0.005f, fEnvD_ = 0.1f, fEnvS_ = 0.0f, fEnvR_ = 0.1f; // filter-envelope ADSR
    float filterEnvDepth_ = 0.0f;   // filter-envelope depth in Hz (±); 0 = off
    float filterVelo_ = 0.0f;       // velocity → cutoff amount in Hz; 0 = off
    float pitchEnvDepth_ = 0.0f;    // pitch-envelope depth in semitones (±); 0 = off
    float pitchEnvTime_ = 0.05f;    // pitch-envelope slide time in seconds
    float attack_ = 0.001f;  // seconds
    float release_ = 0.012f; // seconds
    std::string path_;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
