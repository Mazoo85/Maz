#pragma once

#include "maz/audio/Filter.hpp"
#include "maz/audio/Oscillator.hpp" // Waveform + waveSample
#include "maz/audio/Wavetable.hpp"

#include <array>
#include <cstdint>

namespace maz::audio {

// The synth's sound-generation engine: classic subtractive (an oscillator waveform), 2-operator FM
// (a modulator oscillator bends a sine carrier for metallic/bell/electric-piano timbres), or
// Wavetable (a morphing single-cycle table scanned by a position that the envelope can sweep).
enum class SynthMode { Subtractive, FM, Wavetable };

// A small polyphonic synth: a fixed pool of voices, each with an ADSR amplitude envelope, keyed by
// MIDI note number. noteOn/noteOff drive it like a keyboard; render() ADDS the summed voices into
// the caller's buffer. Pure DSP — no SDL — so it is unit-testable and safe to mix. This is the
// pitched instrument the piano roll plays.
class SynthInstrument {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 7;

    void setMode(SynthMode m) { mode_ = m; }
    SynthMode mode() const { return mode_; }

    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // Unison: stack `voices` (1..kMaxUnison) detuned copies of the primary oscillator, spread ±
    // `detuneCents`, for a thick supersaw. 1 = off (a single oscillator). Subtractive mode only.
    void setUnison(int voices, float detuneCents);
    int unisonVoices() const { return unisonVoices_; }
    float unisonDetune() const { return unisonDetune_; }

    // Pitch envelope: each note starts `amountSemitones` away from its pitch and decays back over
    // `timeSeconds` — classic for zaps, laser stabs, and pitched-attack drums. amount 0 = off.
    void setPitchEnv(float amountSemitones, float timeSeconds) {
        pitchEnvAmt_ = amountSemitones < -48.0f ? -48.0f : (amountSemitones > 48.0f ? 48.0f : amountSemitones);
        pitchEnvTime_ = timeSeconds < 0.001f ? 0.001f : timeSeconds;
    }
    float pitchEnvAmount() const { return pitchEnvAmt_; }
    float pitchEnvTime() const { return pitchEnvTime_; }

    // Vibrato: a pitch LFO at `rateHz` modulating ±`depthCents`. depth 0 = off. Applies to all
    // voices for expressive, wavering pitch.
    void setVibrato(float rateHz, float depthCents) {
        vibRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        vibDepth_ = depthCents < 0.0f ? 0.0f : (depthCents > 100.0f ? 100.0f : depthCents);
    }
    float vibratoRate() const { return vibRate_; }
    float vibratoDepth() const { return vibDepth_; }

    // Portamento / glide: when > 0, a new note slides from the previously played pitch to its own
    // pitch over `seconds` (one-pole smoothing). 0 = off (instant pitch). Great for leads and bass.
    void setGlide(float seconds) { glideSeconds_ = seconds < 0.0f ? 0.0f : seconds; }
    float glide() const { return glideSeconds_; }

    // FM: modulator frequency = carrier * ratio; index sets the modulation depth (brightness).
    void setFmRatio(float r) { fmRatio_ = r; }
    void setFmIndex(float i) { fmIndex_ = i; }
    float fmRatio() const { return fmRatio_; }
    float fmIndex() const { return fmIndex_; }

    // Wavetable: `position` [0,1] scans the morphing table (dark→bright); `envAmt` sweeps that
    // position with the amp envelope for evolving timbres. Access the table to reprogram its frames.
    void setWavetablePosition(float position) { wtPosition_ = position; }
    void setWavetableMorph(float envAmt) { wtMorphEnv_ = envAmt; }
    float wavetablePosition() const { return wtPosition_; }
    float wavetableMorph() const { return wtMorphEnv_; }
    // Choose the four waveforms the wavetable morphs between (frame 0 → 3 as the position sweeps).
    void setWavetableFrames(Waveform a, Waveform b, Waveform c, Waveform d) {
        wtFrames_ = {a, b, c, d};
        wavetable_.setMorph(a, b, c, d);
    }
    Waveform wavetableFrame(int i) const {
        return wtFrames_[static_cast<size_t>(i < 0 ? 0 : (i > 3 ? 3 : i))];
    }
    Wavetable& wavetable() { return wavetable_; }
    const Wavetable& wavetable() const { return wavetable_; }

    // Oscillator section (subtractive mode): a detuned 2nd oscillator (cents + level) for width, a
    // sub-oscillator one octave down, and a noise layer. All 0 → a single clean oscillator.
    void setOscillators(float detuneCents, float osc2Level, float subLevel, float noiseLevel);
    // Coarse tune for the 2nd oscillator in semitones (-24..+24): stacks it a fixed musical interval
    // (octave, fifth, …) above/below the note for fat two-oscillator sounds. Combines with the fine
    // `detuneCents`. Ignored while hard sync is on (there the slave pitch is the sync ratio).
    void setOsc2Semitones(float semis) {
        osc2Semitones_ = semis < -24.0f ? -24.0f : (semis > 24.0f ? 24.0f : semis);
    }
    float osc2Semitones() const { return osc2Semitones_; }

    // Sub-oscillator waveform (one octave below the note): Sine (default, pure weight) or Square
    // (buzzy, more harmonics). Only matters when subLevel > 0.
    void setSubWaveform(Waveform w) { subWave_ = w; }
    Waveform subWaveform() const { return subWave_; }

    // Hard sync: the 2nd oscillator becomes a "slave" running at `syncRatio` × the note frequency,
    // its phase force-reset to 0 every time the primary (master) oscillator completes a cycle. The
    // reset injects harmonics that sweep with the ratio — the classic bright, tearing sync lead. Only
    // audible when osc2Level > 0 (the slave IS the 2nd oscillator). Off → osc2 is a plain detuned
    // oscillator as before.
    void setHardSync(bool on) { hardSync_ = on; }
    bool hardSync() const { return hardSync_; }
    void setSyncRatio(float ratio) { syncRatio_ = ratio < 1.0f ? 1.0f : (ratio > 8.0f ? 8.0f : ratio); }
    float syncRatio() const { return syncRatio_; }

    // Pulse width / duty cycle (0.02..0.98) of the Square waveform: 0.5 is a plain square, narrower
    // values thin it toward a bright nasal pulse. Only affects the Square waveform.
    void setPulseWidth(float w) { pulseWidth_ = w < 0.02f ? 0.02f : (w > 0.98f ? 0.98f : w); }
    float pulseWidth() const { return pulseWidth_; }
    // Noise color (0 = bright white, 1 = dark/low-passed): a tone control on the noise layer.
    void setNoiseColor(float c) { noiseColor_ = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c); }
    float noiseColor() const { return noiseColor_; }
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

    // Velocity → filter cutoff (Hz added at full velocity): makes harder-played notes brighter, the
    // classic velocity-sensitive filter. 0 = off. Only has an effect when the filter is engaged.
    void setVelToCutoff(float hz) { velCutoff_ = hz < 0.0f ? 0.0f : (hz > 15000.0f ? 15000.0f : hz); }
    float velToCutoff() const { return velCutoff_; }

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
        std::array<double, kMaxUnison> uniPhase{}; // unison stack phases
        double phase2 = 0.0;   // detuned 2nd oscillator
        double subPhase = 0.0; // sub-oscillator (one octave down)
        double modPhase = 0.0; // FM modulator phase
        uint32_t rng = 0x2545F491u; // per-voice noise state
        float noiseLp = 0.0f;       // one-pole state for the noise tone control
        float freq = 0.0f;       // current (possibly gliding) frequency
        float targetFreq = 0.0f; // note's destination frequency
        float pitchEnv = 0.0f;   // pitch-envelope offset in semitones (decays to 0)
        float velocity = 0.0f;
        float env = 0.0f;
        StateVariableFilter filter{};
    };

    SynthMode mode_ = SynthMode::Subtractive;
    Waveform waveform_ = Waveform::Saw;
    float gain_ = 0.28f;
    float fmRatio_ = 2.0f;
    float fmIndex_ = 3.0f;
    float wtPosition_ = 0.0f;  // wavetable scan position [0,1]
    float wtMorphEnv_ = 0.0f;  // envelope amount added to the scan position
    float glideSeconds_ = 0.0f; // portamento time; 0 = off
    float lastFreq_ = 0.0f;     // last note's frequency, used as a glide start point
    float vibRate_ = 5.0f;      // vibrato LFO rate (Hz)
    float vibDepth_ = 0.0f;     // vibrato depth (cents); 0 = off
    float pitchEnvAmt_ = 0.0f;  // pitch-envelope start offset (semitones); 0 = off
    float pitchEnvTime_ = 0.05f; // pitch-envelope decay time (seconds)
    double vibPhase_ = 0.0;     // vibrato LFO phase (shared across voices)
    int unisonVoices_ = 1;      // 1 = off
    float unisonDetune_ = 12.0f; // cents of spread when unison is on
    std::array<Waveform, 4> wtFrames_ = {Waveform::Sine, Waveform::Triangle, Waveform::Saw,
                                         Waveform::Square};
    Wavetable wavetable_{};
    float attack_ = 0.005f;
    float decay_ = 0.08f;
    float sustain_ = 0.6f;
    float release_ = 0.12f;
    float filterCutoff_ = 20000.0f; // effectively open (bypassed) by default
    float filterReso_ = 0.7f;
    float filterEnvAmt_ = 0.0f;
    float velCutoff_ = 0.0f; // velocity → cutoff amount (Hz at full velocity); 0 = off
    Waveform subWave_ = Waveform::Sine;
    bool hardSync_ = false;     // osc2 hard-syncs to the master when true
    float syncRatio_ = 1.5f;    // slave frequency = note freq × this (when hard sync is on)
    float pulseWidth_ = 0.5f;   // square duty cycle; 0.5 = plain square
    float noiseColor_ = 0.0f;
    float detuneCents_ = 0.0f;
    float osc2Semitones_ = 0.0f; // coarse tune for osc2 (semitones)
    float osc2Level_ = 0.0f;
    float subLevel_ = 0.0f;
    float noiseLevel_ = 0.0f;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
