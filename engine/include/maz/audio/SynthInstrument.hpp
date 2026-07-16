#pragma once

#include "maz/audio/Oscillator.hpp" // Waveform + waveSample

#include <array>

namespace maz::audio {

// A small polyphonic subtractive-style synth: a fixed pool of oscillator voices, each with an ADSR
// amplitude envelope, keyed by MIDI note number. noteOn/noteOff drive it like a keyboard; render()
// ADDS the summed voices into the caller's buffer. Pure DSP — no SDL — so it is unit-testable and
// safe to mix. This is the pitched instrument the piano roll plays.
class SynthInstrument {
public:
    static constexpr int kMaxVoices = 16;

    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // ADSR times in seconds and sustain level in [0, 1].
    void setEnvelope(float attack, float decay, float sustain, float release);

    void noteOn(int midi, float velocity);
    void noteOff(int midi);
    void allNotesOff(); // release every held voice

    bool active() const;

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

private:
    enum class Stage { Off, Attack, Decay, Sustain, Release };
    struct Voice {
        Stage stage = Stage::Off;
        int midi = -1;
        double phase = 0.0;
        float freq = 0.0f;
        float velocity = 0.0f;
        float env = 0.0f;
    };

    Waveform waveform_ = Waveform::Saw;
    float gain_ = 0.28f;
    float attack_ = 0.005f;
    float decay_ = 0.08f;
    float sustain_ = 0.6f;
    float release_ = 0.12f;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
