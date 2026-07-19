#pragma once

#include "maz/audio/Filter.hpp"
#include "maz/audio/Oscillator.hpp" // Waveform + waveSample
#include "maz/audio/Wavetable.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace maz::audio {

// The synth's sound-generation engine: classic subtractive (an oscillator waveform), 2-operator FM
// (a modulator oscillator bends a sine carrier for metallic/bell/electric-piano timbres), or
// Wavetable (a morphing single-cycle table scanned by a position that the envelope can sweep).
enum class SynthMode { Subtractive, FM, Wavetable, Pluck, Organ, PhaseDistortion };

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
    // Pluck (Karplus-Strong) string damping [0,1]: 0 = the brightest, longest-ringing string; higher
    // bleeds energy out of the delay line each pass, so the pluck decays faster and darker (a softer,
    // more muted string). Only affects Pluck mode; 0 (default) keeps the natural KS decay.
    void setPluckDamping(float d) { pluckDamping_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    float pluckDamping() const { return pluckDamping_; }
    // Pluck position [0,1): where along the string it is struck, as a fraction of its length. A comb
    // filter on the excitation nulls the harmonics that have a node at the pluck point, so 0.5
    // (centre) removes the even harmonics for a hollow, clarinet-like tone, while values near 0
    // (the bridge) keep every harmonic for a bright, full pluck. Only affects Pluck mode; 0
    // (default) = no comb (the raw, brightest excitation).
    void setPluckPosition(float p) { pluckPosition_ = p < 0.0f ? 0.0f : (p > 0.99f ? 0.99f : p); }
    float pluckPosition() const { return pluckPosition_; }
    // Organ drawbars: 8 additive harmonic levels [0,1] (harmonics 1..8 of the note), summed like a
    // tonewheel organ's drawbars. Only affects Organ mode. Defaults to fundamental-only (a sine).
    static constexpr int kOrganBars = 8;
    void setOrganBar(int i, float level) {
        if (i >= 0 && i < kOrganBars) {
            organBars_[static_cast<size_t>(i)] = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
        }
    }
    float organBar(int i) const {
        return (i >= 0 && i < kOrganBars) ? organBars_[static_cast<size_t>(i)] : 0.0f;
    }
    // Organ percussion (Hammond key-click): a fast-decaying harmonic ping added on each note's attack,
    // on top of the sustained drawbars — the classic percussive organ "chiff". `amount` 0 (default) =
    // off; `third` picks the 3rd harmonic instead of the 2nd. Only affects Organ mode.
    void setOrganPercussion(float amount) {
        organPercAmt_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    }
    float organPercussion() const { return organPercAmt_; }
    void setOrganPercThird(bool third) { organPercThird_ = third; }
    bool organPercThird() const { return organPercThird_; }

    // Phase-distortion (Casio CZ-style) amount [0,1]: warps the read-phase of a sine through a
    // two-segment map so the cosine cycle is squeezed into part of the period, morphing a pure sine
    // (0) toward a bright, saw-like tone (1) — a distinctive digital timbre distinct from FM/wavetable.
    // Only affects PhaseDistortion mode; 0 (default) = a clean sine.
    void setPdAmount(float a) { pdAmount_ = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    float pdAmount() const { return pdAmount_; }

    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }

    // Per-instrument octave shift (-2..+2): transposes the whole synth by whole octaves, on top of
    // the notes it is played. 0 = no shift.
    void setOctave(int oct) { octave_ = oct < -2 ? -2 : (oct > 2 ? 2 : oct); }
    int octave() const { return octave_; }

    // Monophonic mode: play a single voice with last-note priority (for tight mono leads/bass with
    // glide) instead of the default polyphony.
    void setMono(bool on) { mono_ = on; }
    bool mono() const { return mono_; }

    // Velocity → amplitude sensitivity (0..1): how much a note's velocity affects its loudness. 1
    // (default) = velocity fully sets the level (as before); 0 = every note plays at full level
    // regardless of velocity.
    void setVelSensitivity(float s) { velSens_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    float velSensitivity() const { return velSens_; }

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

    // Noise attack transient: a short burst of white noise added at each note's onset, fading over
    // `decayMs`, for a percussive click/chiff attack on plucks, pads, and basses (a "spit" the
    // sustained oscillators can't make on their own). `amount` 0 (default) = off. Per-note, using the
    // voice's own deterministic noise, so renders stay reproducible.
    void setNoiseAttack(float amount, float decayMs) {
        noiseAttackAmt_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
        noiseAttackDecayMs_ = decayMs < 1.0f ? 1.0f : (decayMs > 200.0f ? 200.0f : decayMs);
    }
    float noiseAttackAmount() const { return noiseAttackAmt_; }
    float noiseAttackDecay() const { return noiseAttackDecayMs_; }

    // Vibrato: a pitch LFO at `rateHz` modulating ±`depthCents`. depth 0 = off. Applies to all
    // voices for expressive, wavering pitch.
    void setVibrato(float rateHz, float depthCents) {
        vibRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        vibDepth_ = depthCents < 0.0f ? 0.0f : (depthCents > 100.0f ? 100.0f : depthCents);
    }
    float vibratoRate() const { return vibRate_; }
    float vibratoDepth() const { return vibDepth_; }
    // Vibrato-LFO shape: Sine (smooth waver), Square (a two-pitch trill), Saw (a repeating pitch
    // ramp/gliss), Triangle, or Trapezoid. Reuses the oscillator waveforms; defaults to Sine.
    void setVibratoShape(Waveform w) { vibShape_ = w; }
    Waveform vibratoShape() const { return vibShape_; }
    // Vibrato-LFO sample & hold: instead of the periodic shape, jump the pitch to a new random offset
    // each LFO cycle and hold it — random stepped pitch (chiptune/arp character). Deterministic (a hash
    // of the cycle index). Off (default) = the periodic shape above.
    void setVibratoSampleHold(bool on) { vibSampleHold_ = on; }
    bool vibratoSampleHold() const { return vibSampleHold_; }
    // Tempo-sync the vibrato (pitch) LFO: lock its rate to the transport at the chosen note division
    // (the same 6 divisions as the cutoff/amp LFOs) for rhythmic pitch warble. updateTempo() applies
    // it each block; off (default) = the free-running rate above.
    void setVibratoSync(bool on) { vibSync_ = on; }
    bool vibratoSync() const { return vibSync_; }
    void setVibratoSyncDivision(int d) { vibSyncDiv_ = d < 0 ? 0 : (d > 5 ? 5 : d); }
    int vibratoSyncDivision() const { return vibSyncDiv_; }
    // Vibrato delay (seconds): hold the vibrato off for this long after each note starts, then fade it
    // in over ~50 ms — the expressive "delayed vibrato" a player adds on sustained notes. 0 = off
    // (vibrato is immediate, as before). Per-note (timed from each note's onset).
    void setVibratoDelay(float seconds) { vibDelay_ = seconds < 0.0f ? 0.0f : (seconds > 5.0f ? 5.0f : seconds); }
    float vibratoDelay() const { return vibDelay_; }

    // Oscillator start-phase randomization (0..1): each note starts its oscillators at a random phase
    // rather than always at 0, so repeated notes don't have identical transients and stacked/unison
    // voices decorrelate — the subtle analog "every note is a little different" character. 0 = off
    // (phase-coherent, always starts at 0, the classic clean digital attack). Deterministic RNG.
    void setStartPhaseRandom(float amount) {
        phaseRandom_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    }
    float startPhaseRandom() const { return phaseRandom_; }

    // Analog drift (0..50 cents): each note is detuned by a small random amount within ±this many
    // cents, emulating the pitch instability of analog oscillators for a warmer, less sterile sound.
    // Deterministic (a per-instrument RNG), so renders stay reproducible. 0 = off (perfectly in tune).
    void setDrift(float cents) { drift_ = cents < 0.0f ? 0.0f : (cents > 50.0f ? 50.0f : cents); }
    float drift() const { return drift_; }

    // Portamento / glide: when > 0, a new note slides from the previously played pitch to its own
    // pitch over `seconds` (one-pole smoothing). 0 = off (instant pitch). Great for leads and bass.
    void setGlide(float seconds) { glideSeconds_ = seconds < 0.0f ? 0.0f : seconds; }
    float glide() const { return glideSeconds_; }
    // Legato glide: when on, portamento only happens on overlapping (legato) notes — an isolated note
    // starts on-pitch. Off (default) = the classic always-glide. Only matters when glide > 0.
    void setGlideLegato(bool on) { glideLegato_ = on; }
    bool glideLegato() const { return glideLegato_; }

    // Amplitude LFO (tremolo): modulate every voice's level at `rateHz` by `depth` [0,1] — the output
    // swings between full level (depth 0, off) and (1 − depth) of it at the trough. A shared LFO
    // across voices, like the vibrato. depth 0 = off.
    void setAmpLfo(float rateHz, float depth) {
        ampLfoRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        ampLfoDepth_ = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth);
    }
    float ampLfoRate() const { return ampLfoRate_; }
    float ampLfoDepth() const { return ampLfoDepth_; }
    // Tremolo-LFO shape: Sine (smooth), Square (a hard on/off gate — the classic synth trance-gate),
    // Saw (rhythmic fade), Triangle, or Trapezoid. Reuses the oscillator waveforms; defaults to Sine.
    void setAmpLfoShape(Waveform w) { ampLfoShape_ = w; }
    Waveform ampLfoShape() const { return ampLfoShape_; }
    // Tremolo-LFO sample & hold: instead of the periodic shape, drop the level to a new random value
    // each LFO cycle and hold it — a random stepped tremolo / glitchy trance-gate. Deterministic (a
    // hash of the cycle index). Off (default) = the periodic shape above.
    void setAmpLfoSampleHold(bool on) { ampLfoSampleHold_ = on; }
    bool ampLfoSampleHold() const { return ampLfoSampleHold_; }
    // Tempo-sync the tremolo LFO: lock its rate to the transport at the chosen note division (the
    // same 6 divisions as the cutoff LFO / effects) for a rhythmic synth trance-gate. updateTempo()
    // applies it each block; off (default) = the free-running rate above.
    void setAmpLfoSync(bool on) { ampLfoSync_ = on; }
    bool ampLfoSync() const { return ampLfoSync_; }
    void setAmpLfoSyncDivision(int d) { ampLfoSyncDiv_ = d < 0 ? 0 : (d > 5 ? 5 : d); }
    int ampLfoSyncDivision() const { return ampLfoSyncDiv_; }

    // FM: modulator frequency = carrier * ratio; index sets the modulation depth (brightness).
    void setFmRatio(float r) { fmRatio_ = r; }
    void setFmIndex(float i) { fmIndex_ = i; }
    float fmRatio() const { return fmRatio_; }
    float fmIndex() const { return fmIndex_; }
    // FM feedback (0..1): routes the modulator operator's output back into its own phase, adding
    // richer, more sawtooth-like harmonics. 0 = clean 2-op FM (unchanged).
    void setFmFeedback(float f) { fmFeedback_ = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
    float fmFeedback() const { return fmFeedback_; }
    // Velocity → FM index (added to the index at full velocity): harder-played notes get a brighter,
    // more harmonically rich FM tone — the FM-mode analog of velocity → filter cutoff. 0 = off. Only
    // has an effect in FM mode.
    void setVelToFmIndex(float amt) { velFmIndex_ = amt < 0.0f ? 0.0f : (amt > 20.0f ? 20.0f : amt); }
    float velToFmIndex() const { return velFmIndex_; }
    // FM modulator waveform: the modulating operator is a Sine by default (classic clean 2-op FM), but
    // it can be any oscillator shape — a saw/square/triangle modulator injects far richer sidebands
    // for grittier, more aggressive DX-style tones. Only affects FM mode; Sine = unchanged.
    void setFmModWaveform(Waveform w) { fmModWave_ = w; }
    Waveform fmModWaveform() const { return fmModWave_; }

    // Wavetable: `position` [0,1] scans the morphing table (dark→bright); `envAmt` sweeps that
    // position with the amp envelope for evolving timbres. Access the table to reprogram its frames.
    void setWavetablePosition(float position) { wtPosition_ = position; }
    void setWavetableMorph(float envAmt) { wtMorphEnv_ = envAmt; }
    float wavetablePosition() const { return wtPosition_; }
    float wavetableMorph() const { return wtMorphEnv_; }
    // Velocity → wavetable position (0..1 added to the scan position at full velocity): harder-played
    // notes scan further into the table (usually brighter) — the wavetable analog of velocity → cutoff
    // / FM index. 0 = off. Only has an effect in Wavetable mode.
    void setVelToWavePosition(float amt) { velWavePos_ = amt < 0.0f ? 0.0f : (amt > 1.0f ? 1.0f : amt); }
    float velToWavePosition() const { return velWavePos_; }
    // Wavetable scan LFO: sweep the table position at `rateHz` by `depth` [0,1] for continuous,
    // evolving movement (independent of the envelope morph). depth 0 = off.
    void setWavetableLfo(float rateHz, float depth) {
        wtLfoRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        wtLfoDepth_ = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth);
    }
    float wavetableLfoRate() const { return wtLfoRate_; }
    float wavetableLfoDepth() const { return wtLfoDepth_; }
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
    // Oscillator ring modulation (0..1): mixes in the product of oscillator 1 and oscillator 2 for
    // metallic, clangorous, inharmonic timbres. Independent of osc2Level (the ring path uses osc2
    // even when its level is 0). 0 = off.
    void setRingMod(float amount) { ringMod_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount); }
    float ringMod() const { return ringMod_; }

    // Third oscillator (subtractive mode): a fixed-interval extra oscillator (level + coarse
    // semitones, using the primary waveform) for a fuller, 3xOSC-style stack. level 0 = off.
    void setOsc3Level(float level) { osc3Level_ = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level); }
    float osc3Level() const { return osc3Level_; }
    void setOsc3Semitones(float semis) {
        osc3Semitones_ = semis < -24.0f ? -24.0f : (semis > 24.0f ? 24.0f : semis);
    }
    float osc3Semitones() const { return osc3Semitones_; }
    // Osc3 fine tune (±100 cents): a small detune from its coarse pitch so osc3 beats against the
    // rest of the stack for a fatter, wider sound. 0 (default) = dead-on (no beating).
    void setOsc3FineTune(float cents) { osc3FineCents_ = cents < -100.0f ? -100.0f : (cents > 100.0f ? 100.0f : cents); }
    float osc3FineTune() const { return osc3FineCents_; }
    // Osc3's own waveform (like osc2): linked to the primary by default, or an independent shape.
    void setOsc3Waveform(Waveform w) {
        osc3Waveform_ = w;
        osc3WaveLinked_ = false;
    }
    void setOsc3WaveformLinked(bool linked) { osc3WaveLinked_ = linked; }
    bool osc3WaveformLinked() const { return osc3WaveLinked_; }
    Waveform osc3Waveform() const { return osc3WaveLinked_ ? waveform_ : osc3Waveform_; }

    // Coarse tune for the 2nd oscillator in semitones (-24..+24): stacks it a fixed musical interval
    // (octave, fifth, …) above/below the note for fat two-oscillator sounds. Combines with the fine
    // `detuneCents`. Ignored while hard sync is on (there the slave pitch is the sync ratio).
    void setOsc2Semitones(float semis) {
        osc2Semitones_ = semis < -24.0f ? -24.0f : (semis > 24.0f ? 24.0f : semis);
    }
    float osc2Semitones() const { return osc2Semitones_; }

    // Second oscillator's own waveform (3xOSC-style): lets osc2 be a different shape than the primary
    // (e.g. a saw layered under a square) for richer two-oscillator tones. By default osc2 is *linked*
    // to the primary waveform (so old patches are unchanged); setting an explicit waveform unlinks it.
    // Only matters when osc2Level > 0.
    void setOsc2Waveform(Waveform w) {
        osc2Waveform_ = w;
        osc2WaveLinked_ = false;
    }
    void setOsc2WaveformLinked(bool linked) { osc2WaveLinked_ = linked; }
    bool osc2WaveformLinked() const { return osc2WaveLinked_; }
    // The effective osc2 waveform: the primary's shape while linked, otherwise the explicit choice.
    Waveform osc2Waveform() const { return osc2WaveLinked_ ? waveform_ : osc2Waveform_; }

    // Sub-oscillator waveform (one octave below the note): Sine (default, pure weight) or Square
    // (buzzy, more harmonics). Only matters when subLevel > 0.
    void setSubWaveform(Waveform w) { subWave_ = w; }
    Waveform subWaveform() const { return subWave_; }
    // Sub-oscillator octave: how many octaves below the note the sub sits — 1 (default) or 2 (deeper,
    // for sub-bass weight). Only matters when subLevel > 0.
    void setSubOctave(int octavesDown) { subOctave_ = octavesDown >= 2 ? 2 : 1; }
    int subOctave() const { return subOctave_; }

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
    // Pulse-width (PWM) LFO: sweep the Square duty cycle at `rateHz` by ±`depth` around the set pulse
    // width — the classic lush PWM string/pad movement. depth 0 = off (static pulse width). Shared
    // across voices; only affects the Square waveform.
    void setPwmLfo(float rateHz, float depth) {
        pwmLfoRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        pwmLfoDepth_ = depth < 0.0f ? 0.0f : (depth > 0.48f ? 0.48f : depth);
    }
    float pwmLfoRate() const { return pwmLfoRate_; }
    float pwmLfoDepth() const { return pwmLfoDepth_; }
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
    // Velocity → attack (0..1): how much a note's velocity shortens its amp-envelope attack. At full
    // velocity the attack is the set time; softer notes swell in more slowly (up to ~5× longer at the
    // lowest velocity for amount 1) — the classic velocity-to-envelope expression of a soft pad that
    // blooms on gentle keys and snaps on hard ones. 0 (default) = off (attack is velocity-independent,
    // bit-for-bit unchanged).
    void setVelToAttack(float amt) { velToAttack_ = amt < 0.0f ? 0.0f : (amt > 1.0f ? 1.0f : amt); }
    float velToAttack() const { return velToAttack_; }

    // Resonant low-pass filter driven per voice. `cutoff` is the base cutoff in Hz, `resonance`
    // sharpens the peak, and `envAmt` (Hz) opens the cutoff with the amp envelope for a classic
    // subtractive filter sweep. Set cutoff high (e.g. 20000) to bypass.
    void setFilter(float cutoffHz, float resonance, float envAmt);
    float filterCutoff() const { return filterCutoff_; }
    float filterResonance() const { return filterReso_; }
    float filterEnvAmount() const { return filterEnvAmt_; }

    // Filter type: low-pass (default, dark below cutoff), high-pass (thin, removes lows), or band-pass
    // (keeps a band around the cutoff). Uses the per-voice state-variable filter's mode.
    // Filter slope: 12 dB/oct (one state-variable stage, the default) or 24 dB/oct (two cascaded
    // stages, a steeper Moog-style rolloff that clamps the tone harder below the cutoff). Any value
    // other than 24 is treated as 12.
    void setFilterSlope(int db) { filter24_ = (db >= 24); }
    int filterSlope() const { return filter24_ ? 24 : 12; }

    void setFilterMode(StateVariableFilter::Mode m) { filterMode_ = m; }
    StateVariableFilter::Mode filterMode() const { return filterMode_; }

    // Dedicated filter envelope: its own ADSR (seconds / sustain 0..1) drives the cutoff by
    // `depth` Hz (±) — the classic subtractive filter sweep that is independent of the amp envelope
    // (e.g. a snappy filter decay under a sustained note). depth 0 = off (only has an effect when the
    // filter is engaged). Separate from the amp-envelope-driven `Env->Cutoff` amount.
    void setFilterEnvelope(float attack, float decay, float sustain, float release);
    void setFilterEnvDepth(float hz) {
        filterEnvDepth_ = hz < -18000.0f ? -18000.0f : (hz > 18000.0f ? 18000.0f : hz);
    }
    float filterEnvAttack() const { return filtA_; }
    float filterEnvDecay() const { return filtD_; }
    float filterEnvSustain() const { return filtS_; }
    float filterEnvRelease() const { return filtR_; }
    float filterEnvDepth() const { return filterEnvDepth_; }

    // Velocity → filter cutoff (Hz added at full velocity): makes harder-played notes brighter, the
    // classic velocity-sensitive filter. 0 = off. Only has an effect when the filter is engaged.
    void setVelToCutoff(float hz) { velCutoff_ = hz < 0.0f ? 0.0f : (hz > 15000.0f ? 15000.0f : hz); }
    float velToCutoff() const { return velCutoff_; }

    // Filter cutoff LFO: sweeps the cutoff at `rateHz` by ±`depthOctaves` octaves (a shared LFO
    // across all voices). This is the classic wobble/auto-wah movement — a slow sweep for evolving
    // pads, a fast one for wub bass. depth 0 = off. Only has an effect when the filter is engaged.
    void setFilterLfo(float rateHz, float depthOctaves) {
        filterLfoRate_ = rateHz < 0.0f ? 0.0f : (rateHz > 20.0f ? 20.0f : rateHz);
        filterLfoDepth_ = depthOctaves < 0.0f ? 0.0f : (depthOctaves > 4.0f ? 4.0f : depthOctaves);
    }
    float filterLfoRate() const { return filterLfoRate_; }
    float filterLfoDepth() const { return filterLfoDepth_; }
    // Cutoff-LFO shape: Sine (smooth wobble), Square (stepped/gated two-value jump), Saw (rhythmic
    // ramp), Triangle, or Trapezoid — reusing the oscillator waveforms. Defaults to Sine.
    void setFilterLfoShape(Waveform w) { filterLfoShape_ = w; }
    Waveform filterLfoShape() const { return filterLfoShape_; }
    // Cutoff-LFO sample & hold: instead of the periodic shape, jump the cutoff to a new random level
    // each LFO cycle and hold it — the classic acid/IDM stepped-filter movement. Deterministic (a hash
    // of the cycle index), so renders stay reproducible. Off (default) = the periodic shape above.
    void setFilterLfoSampleHold(bool on) { filterLfoSampleHold_ = on; }
    bool filterLfoSampleHold() const { return filterLfoSampleHold_; }
    // Tempo-sync the cutoff LFO: lock its rate to the transport at the chosen note division (the same
    // 6 divisions as the effects: 1/1, 1/2, 1/4, 1/8, 1/8T, 1/16) for rhythmic filter wobble. Call
    // updateTempo() with the current BPM each block; off (default) = the free-running rate above.
    void setFilterLfoSync(bool on) { filterLfoSync_ = on; }
    bool filterLfoSync() const { return filterLfoSync_; }
    void setFilterLfoSyncDivision(int d) {
        filterLfoSyncDiv_ = d < 0 ? 0 : (d > 5 ? 5 : d); // 6 divisions, matching kModSyncDivisions
    }
    int filterLfoSyncDivision() const { return filterLfoSyncDiv_; }
    // Recompute the cutoff-LFO rate from the tempo + division when sync is on (no-op otherwise).
    void updateTempo(double bpm);

    // Filter drive (0..1): overdrive the signal into the filter with a tanh saturation before it is
    // filtered, adding harmonics and analog grit (the classic driven-filter growl). 0 = clean
    // (bit-transparent). Only has an effect when the filter is engaged (cutoff below ~19 kHz).
    void setFilterDrive(float d) { filterDrive_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    float filterDrive() const { return filterDrive_; }

    // Filter keyboard tracking (0..1): how much the cutoff follows the note's pitch (relative to
    // middle C). 1 = full tracking (an octave up doubles the cutoff, so high notes stay bright);
    // 0 = fixed cutoff. Only has an effect when the filter is engaged.
    void setFilterKeyTrack(float amount) {
        filterKeyTrack_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    }
    float filterKeyTrack() const { return filterKeyTrack_; }

    void noteOn(int midi, float velocity) { noteOn(midi, velocity, 0.0f); }
    // Trigger with a per-note fine-tune offset in cents (piano-roll micro-detune).
    void noteOn(int midi, float velocity, float fineCents);
    void noteOff(int midi);
    void allNotesOff(); // release every held voice

    bool active() const;
    int activeVoices() const; // number of voices currently sounding (for the UI / tests)

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
        double phase3 = 0.0;   // 3rd oscillator (coarse-tuned)
        double subPhase = 0.0; // sub-oscillator (one octave down)
        double modPhase = 0.0; // FM modulator phase
        float fmFb = 0.0f;     // last FM modulator output (for feedback)
        uint32_t rng = 0x2545F491u; // per-voice noise state
        float noiseLp = 0.0f;       // one-pole state for the noise tone control
        float freq = 0.0f;       // current (possibly gliding) frequency
        float targetFreq = 0.0f; // note's destination frequency
        float driftMul = 1.0f;   // per-note analog-drift pitch multiplier (1 = in tune)
        float pitchEnv = 0.0f;   // pitch-envelope offset in semitones (decays to 0)
        float velocity = 0.0f;
        float env = 0.0f;
        double ageSamples = 0.0; // samples since noteOn (for the vibrato onset delay)
        Stage filtStage = Stage::Off; // dedicated filter-envelope stage
        float filtEnv = 0.0f;         // dedicated filter-envelope level
        StateVariableFilter filter{};
        StateVariableFilter filter2{}; // 2nd cascaded stage for the 24 dB/oct slope
        std::vector<float> ksBuf;  // Karplus-Strong delay line (Pluck mode); sized to one period
        int ksPtr = 0;             // KS delay-line read/write index
        bool ksInit = false;       // fill the KS line with noise on the first Pluck render sample
        float percEnv = 0.0f;      // organ percussion (key-click) envelope; seeded on note-on
    };

    SynthMode mode_ = SynthMode::Subtractive;
    float pluckDamping_ = 0.0f;  // Karplus-Strong extra damping [0,1]; 0 = natural (brightest) decay
    float pluckPosition_ = 0.0f; // Karplus-Strong pluck position [0,1); 0 = no excitation comb
    std::array<float, kOrganBars> organBars_{1.0f}; // drawbar levels; default = fundamental only
    float organPercAmt_ = 0.0f;    // organ percussion (key-click) amount; 0 = off
    bool organPercThird_ = false;  // percussion harmonic: false = 2nd, true = 3rd
    Waveform waveform_ = Waveform::Saw;
    float pdAmount_ = 0.0f; // phase-distortion amount [0,1]; 0 = clean sine (PhaseDistortion mode)
    float gain_ = 0.28f;
    int octave_ = 0;       // per-instrument octave shift (-2..+2)
    bool mono_ = false;    // monophonic (single-voice) mode
    float velSens_ = 1.0f; // velocity → amplitude depth; 1 = full (default)
    float fmRatio_ = 2.0f;
    float fmIndex_ = 3.0f;
    float fmFeedback_ = 0.0f; // FM operator self-feedback; 0 = off
    Waveform fmModWave_ = Waveform::Sine; // FM modulator operator waveform; Sine = classic 2-op FM
    float velFmIndex_ = 0.0f; // velocity → FM index depth; 0 = off
    float wtPosition_ = 0.0f;  // wavetable scan position [0,1]
    float velWavePos_ = 0.0f;  // velocity → wavetable position depth; 0 = off
    float wtMorphEnv_ = 0.0f;  // envelope amount added to the scan position
    float wtLfoRate_ = 0.0f;   // wavetable scan LFO rate (Hz)
    float wtLfoDepth_ = 0.0f;  // wavetable scan LFO depth [0,1]; 0 = off
    double wtLfoPhase_ = 0.0;  // wavetable scan LFO phase (shared across voices)
    float glideSeconds_ = 0.0f; // portamento time; 0 = off
    bool glideLegato_ = false;  // glide only on overlapping (legato) notes; false = always
    float lastFreq_ = 0.0f;     // last note's frequency, used as a glide start point
    float drift_ = 0.0f;        // analog drift depth in cents; 0 = off
    uint32_t driftRng_ = 0x51ED2C7u; // deterministic RNG for per-note drift
    float phaseRandom_ = 0.0f;       // oscillator start-phase randomization [0,1]; 0 = off
    uint32_t phaseRng_ = 0x9E3779B1u; // deterministic RNG for start-phase randomization
    float vibRate_ = 5.0f;      // vibrato LFO rate (Hz)
    float vibDepth_ = 0.0f;     // vibrato depth (cents); 0 = off
    Waveform vibShape_ = Waveform::Sine; // vibrato LFO waveform
    bool vibSampleHold_ = false;         // vibrato LFO random stepped (sample & hold) mode
    float vibDelay_ = 0.0f;     // vibrato onset delay (seconds); 0 = immediate
    float noiseAttackAmt_ = 0.0f;     // noise-attack transient level; 0 = off
    float noiseAttackDecayMs_ = 15.0f; // noise-attack decay time (ms)
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
    StateVariableFilter::Mode filterMode_ = StateVariableFilter::Mode::LowPass;
    bool filter24_ = false; // false = 12 dB/oct (one stage), true = 24 dB/oct (two cascaded stages)
    float filtA_ = 0.005f, filtD_ = 0.1f, filtS_ = 0.0f, filtR_ = 0.1f; // dedicated filter ADSR
    float filterEnvDepth_ = 0.0f; // dedicated filter-envelope depth in Hz (±); 0 = off
    float velCutoff_ = 0.0f; // velocity → cutoff amount (Hz at full velocity); 0 = off
    float velToAttack_ = 0.0f; // velocity → amp-attack shortening [0,1]; 0 = off
    float filterKeyTrack_ = 0.0f; // filter cutoff → note pitch tracking [0,1]; 0 = off
    float filterDrive_ = 0.0f;    // pre-filter tanh overdrive amount [0,1]; 0 = clean
    float filterLfoRate_ = 0.0f;  // filter cutoff LFO rate (Hz)
    float filterLfoDepth_ = 0.0f; // filter cutoff LFO depth (octaves, ±); 0 = off
    double filterLfoPhase_ = 0.0; // filter cutoff LFO phase (shared across voices)
    Waveform filterLfoShape_ = Waveform::Sine; // cutoff LFO waveform
    bool filterLfoSampleHold_ = false; // cutoff LFO random stepped (sample & hold) mode
    bool filterLfoSync_ = false;  // tempo-sync the cutoff LFO rate
    int filterLfoSyncDiv_ = 3;    // sync note-division index (default 1/8)
    bool ampLfoSync_ = false;     // tempo-sync the tremolo LFO rate
    int ampLfoSyncDiv_ = 3;       // tremolo sync note-division index (default 1/8)
    bool vibSync_ = false;        // tempo-sync the vibrato LFO rate
    int vibSyncDiv_ = 3;          // vibrato sync note-division index (default 1/8)
    float ampLfoRate_ = 0.0f;     // amplitude LFO (tremolo) rate (Hz)
    float ampLfoDepth_ = 0.0f;    // amplitude LFO depth [0,1]; 0 = off
    Waveform ampLfoShape_ = Waveform::Sine; // tremolo LFO waveform
    bool ampLfoSampleHold_ = false;         // tremolo LFO random stepped (sample & hold) mode
    double ampLfoPhase_ = 0.0;    // amplitude LFO phase (shared across voices)
    Waveform subWave_ = Waveform::Sine;
    int subOctave_ = 1;         // octaves the sub sits below the note (1 or 2)
    bool hardSync_ = false;     // osc2 hard-syncs to the master when true
    float syncRatio_ = 1.5f;    // slave frequency = note freq × this (when hard sync is on)
    float pulseWidth_ = 0.5f;   // square duty cycle; 0.5 = plain square
    float pwmLfoRate_ = 0.0f;   // PWM LFO rate (Hz)
    float pwmLfoDepth_ = 0.0f;  // PWM LFO depth (± around pulse width); 0 = off
    double pwmLfoPhase_ = 0.0;  // PWM LFO phase (shared across voices)
    float noiseColor_ = 0.0f;
    float detuneCents_ = 0.0f;
    float osc2Semitones_ = 0.0f; // coarse tune for osc2 (semitones)
    Waveform osc2Waveform_ = Waveform::Saw; // osc2's own shape when unlinked
    bool osc2WaveLinked_ = true;            // true = osc2 follows the primary waveform (default)
    float osc3Level_ = 0.0f;     // 3rd oscillator level; 0 = off
    float osc3Semitones_ = 0.0f; // coarse tune for osc3 (semitones)
    float osc3FineCents_ = 0.0f; // fine detune for osc3 (cents); 0 = dead-on
    Waveform osc3Waveform_ = Waveform::Saw; // osc3's own shape when unlinked
    bool osc3WaveLinked_ = true;            // true = osc3 follows the primary waveform (default)
    float ringMod_ = 0.0f;       // osc1×osc2 ring-modulation amount; 0 = off
    float osc2Level_ = 0.0f;
    float subLevel_ = 0.0f;
    float noiseLevel_ = 0.0f;
    std::array<Voice, kMaxVoices> voices_{};
};

} // namespace maz::audio
