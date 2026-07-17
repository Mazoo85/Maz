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

    void setBasePitch(int midi) { basePitch_ = midi; }
    int basePitch() const { return basePitch_; }
    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // Reverse: play the sample backwards (from the end). Loop: instead of stopping at the end (or
    // start, when reversed), wrap around and keep sounding until noteOff — turning a one-shot into a
    // sustained/looped instrument.
    void setReverse(bool on) { reverse_ = on; }
    bool reverse() const { return reverse_; }
    void setLoop(bool on) { loop_ = on; }
    bool loop() const { return loop_; }

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
        float velocity = 0.0f;
        float env = 0.0f;
    };

    std::vector<float> sample_;
    int sampleSr_ = 48000;
    int basePitch_ = 60;
    float gain_ = 0.9f;
    bool reverse_ = false;
    bool loop_ = false;
    float startOffset_ = 0.0f;
    float attack_ = 0.001f;  // seconds
    float release_ = 0.012f; // seconds
    std::string path_;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
