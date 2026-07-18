#pragma once

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

    // Amplitude envelope (seconds): a click-free attack ramp on trigger and a release fade on
    // noteOff. Longer release lets sustained/looped samples fade out smoothly.
    void setAmpEnv(float attackSec, float releaseSec);
    float attack() const { return attack_; }
    float release() const { return release_; }

    void noteOn(int midi, float velocity);
    void noteOff(int midi);
    void allNotesOff();
    bool active() const;

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is the engine rate.
    void render(float* out, int frames, int sampleRate);

private:
    struct Voice {
        bool active = false;
        bool releasing = false;
        int midi = -1;
        double pos = 0.0; // fractional read index into sample_
        int dir = 1;      // playback direction: +1 forward, -1 backward (flips on ping-pong bounce)
        float velocity = 0.0f;
        float env = 0.0f;
    };

    std::vector<float> sample_;
    int sampleSr_ = 48000;
    int basePitch_ = 60;
    float detuneCents_ = 0.0f;
    float gain_ = 0.9f;
    bool reverse_ = false;
    bool loop_ = false;
    bool pingPong_ = false;
    float startOffset_ = 0.0f;
    float attack_ = 0.001f;  // seconds
    float release_ = 0.012f; // seconds
    std::string path_;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
