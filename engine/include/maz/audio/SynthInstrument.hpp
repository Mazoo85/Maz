#pragma once

#include "maz/audio/Filter.hpp"
#include "maz/audio/Oscillator.hpp" // Waveform + waveSample

#include <array>
#include <cstdint>

namespace maz::audio {

// The synth's sound-generation engine: classic subtractive (an oscillator waveform) or 2-operator
// FM (a modulator oscillator bends a sine carrier for metallic/bell/electric-piano timbres).
enum class SynthMode { Subtractive, FM };

// A small polyphonic synth: a fixed pool of voices, each with an ADSR amplitude envelope, keyed by
// MIDI note number. noteOn/noteOff drive it like a keyboard; render() ADDS the summed voices into
// the caller's buffer. Pure DSP — no SDL — so it is unit-testable and safe to mix. This is the
// pitched instrument the piano roll plays.
class SynthInstrument {
public:
    static constexpr int kMaxVoices = 16;

    void setMode(SynthMode m) { mode_ = m; }
    SynthMode mode() const { return mode_; }

    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // FM: modulator frequency = carrier * ratio; index sets the modulation depth (brightness).
    void setFmRatio(float r) { fmRatio_ = r; }
    void setFmIndex(float i) { fmIndex_ = i; }
    float fmRatio() const { return fmRatio_; }
    float fmIndex() const { return fmIndex_; }

    // Oscillator section (subtractive mode): a detuned 2nd oscillator (cents + level) for width, a
    // sub-oscillator one octave down, and a noise layer. All 0 → a single clean oscillator.
    void setOscillators(float detuneCents, float osc2Level, float subLevel, float noiseLevel);
    float detuneCents() const { return detuneCents_; }
    float osc2Level() const { return osc2Level_; }
    float subLevel() const { return subLevel_; }
    float noiseLevel() const { return noiseLevel_; }

    // ADSR times in seconds and sustain level in [0, 1].
    void setEnvelope(float attack, float decay, float sustain, float release);
    float attack() const { return attack_; }
    float decay() const { return decay_; }
    float sustain() const { return sustain_; }
    float release() const { return release_; }

    // Resonant low-pass filter driven per voice. `cutoff` is the base cutoff in Hz, `resonance`
    // sharpens the peak, and `envAmt` (Hz) opens the cutoff with the amp envelope for a classic
    // subtractive filter sweep. Set cutoff high (e.g. 20000) to bypass.
    void setFilter(float cutoffHz, float resonance, float envAmt);
    float filterCutoff() const { return filterCutoff_; }
    float filterResonance() const { return filterReso_; }
    float filterEnvAmount() const { return filterEnvAmt_; }

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
        double phase = 0.0;    // carrier phase
        double phase2 = 0.0;   // detuned 2nd oscillator
        double subPhase = 0.0; // sub-oscillator (one octave down)
        double modPhase = 0.0; // FM modulator phase
        uint32_t rng = 0x2545F491u; // per-voice noise state
        float freq = 0.0f;
        float velocity = 0.0f;
        float env = 0.0f;
        StateVariableFilter filter{};
    };

    SynthMode mode_ = SynthMode::Subtractive;
    Waveform waveform_ = Waveform::Saw;
    float gain_ = 0.28f;
    float fmRatio_ = 2.0f;
    float fmIndex_ = 3.0f;
    float attack_ = 0.005f;
    float decay_ = 0.08f;
    float sustain_ = 0.6f;
    float release_ = 0.12f;
    float filterCutoff_ = 20000.0f; // effectively open (bypassed) by default
    float filterReso_ = 0.7f;
    float filterEnvAmt_ = 0.0f;
    float detuneCents_ = 0.0f;
    float osc2Level_ = 0.0f;
    float subLevel_ = 0.0f;
    float noiseLevel_ = 0.0f;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
