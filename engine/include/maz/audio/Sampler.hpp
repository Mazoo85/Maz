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
    std::string path_;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
