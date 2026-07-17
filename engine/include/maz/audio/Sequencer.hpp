#pragma once

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/Sampler.hpp"
#include "maz/audio/SynthInstrument.hpp"

#include <string>
#include <vector>

namespace maz::audio {

// One pattern's musical content: the drum step grid plus the piano-roll notes. Instruments (the
// drum kit, synth, sampler) are shared across patterns; only this content changes per pattern.
struct Pattern {
    std::string name;          // human-readable label for the arrangement/playlist
    std::vector<uint8_t> grid; // channel-major: grid[channel * numSteps + step]
    std::vector<uint8_t> prob; // per-step trigger probability, 0..255 (255 = always). Parallel to grid.
    std::vector<uint8_t> ratchet; // per-step retrigger count 1..4 (0/1 = single hit). Parallel to grid.
    PianoRoll roll;            // lead instrument
    PianoRoll roll2;           // second (bass) instrument
};

// An FL-style step sequencer (a "channel rack"): a grid of channels × steps, a transport
// (play/stop + BPM), and a set of built-in drum voices — one per channel. When playing, it walks
// the step grid in sample-accurate time and strikes each channel whose step is switched on.
//
// It holds multiple patterns and a playlist: in song mode the transport chains the playlist's
// patterns back-to-back into a full arrangement; otherwise it loops the selected pattern.
//
// render() ADDS the mixed channel output into the caller's buffer and internally splits the block
// at step boundaries so triggers land on the sample, not the block edge. Voices keep ringing after
// stop() so tails are not cut off; only the transport freezes.
//
// This is the seam the rest of the DAW grows from: patterns, the piano roll, and automation all
// schedule against this same clock.
class Sequencer {
public:
    Sequencer();

    // --- Transport -----------------------------------------------------------
    void play();  // start from step 0 and strike step 0 immediately
    void stop();  // freeze the transport (ringing voices still decay)
    bool playing() const { return playing_; }

    void setBpm(double bpm) { bpm_ = bpm > 1.0 ? bpm : 1.0; }
    double bpm() const { return bpm_; }

    // Swing/groove: 0 = straight; higher values push the off-beat (odd) steps later for a shuffled
    // feel, while keeping each pair of steps the same total length (tempo preserved). Range 0..0.9.
    void setSwing(float s);
    float swing() const { return swing_; }

    // Humanize: randomize each drum hit's velocity slightly (0 = off, 1 = max) for a less
    // machine-like feel. Deterministic, so renders are reproducible.
    void setHumanize(float amount);
    float humanize() const { return humanize_; }

    // Sidechain ducking: when the kick (channel 0) fires, the melodic bus is ducked and recovers
    // over `releaseMs` — the classic pumping effect. `amount` 0..1 is the depth.
    void setSidechain(bool on, float amount, float releaseMs);
    bool sidechainOn() const { return sidechainOn_; }
    float sidechainAmount() const { return scAmount_; }
    float sidechainReleaseMs() const { return scReleaseMs_; }

    // Number of grid steps that make up one loop of the pattern (default 16 = one 4/4 bar of 16ths).
    int numSteps() const { return numSteps_; }
    // Resize the pattern length (1–64 steps). Every pattern's grids are re-laid-out, preserving the
    // steps that still fit. Lets patterns run shorter/longer than a bar (odd meters, 2-bar loops…).
    void setNumSteps(int steps);
    int stepsPerBeat() const { return stepsPerBeat_; }
    // Steps per beat (grid subdivision): 4 = 16th notes (default), 3 = 8th triplets, 6 = 16th
    // triplets, 8 = 32nds. A beat's duration stays tied to the BPM; this changes how finely it is
    // divided into steps. Clamped to 1–8.
    void setStepsPerBeat(int steps) { stepsPerBeat_ = steps < 1 ? 1 : (steps > 8 ? 8 : steps); }
    int numChannels() const { return static_cast<int>(channels_.size()); }
    const std::string& channelName(int channel) const { return names_[static_cast<size_t>(channel)]; }
    DrumVoice& channelVoice(int channel) { return channels_[static_cast<size_t>(channel)]; }

    // Per-channel mixer strip: volume (linear), mute, and solo. When any channel is soloed, only
    // soloed channels are heard.
    void setChannelVolume(int c, float v);
    void setChannelMute(int c, bool m);
    void setChannelSolo(int c, bool s);
    void setChannelPan(int c, float p); // -1 = hard left, 0 = center, +1 = hard right
    float channelVolume(int c) const;
    bool channelMute(int c) const;
    bool channelSolo(int c) const;
    float channelPan(int c) const;

    // Choke group (0 = none): when a channel triggers, it chokes every other channel in the same
    // group — the classic open-hat/closed-hat cutoff.
    void setChannelChokeGroup(int c, int group);
    int channelChokeGroup(int c) const;

    // Per-channel drum tuning in semitones (pitches the kick/snare up or down).
    void setChannelTune(int c, float semitones);
    float channelTune(int c) const;

    // Per-channel drum decay-length multiplier (0.25–4).
    void setChannelDecay(int c, float mul);
    float channelDecay(int c) const;

    // The step currently sounding (0..numSteps-1); useful for a playhead in the UI.
    int currentStep() const { return currentStep_; }

    // The melodic side: a pitched synth (or the sampler) playing the piano-roll pattern.
    SynthInstrument& synth() { return synth_; }
    Sampler& sampler() { return sampler_; }
    PianoRoll& roll() { return patterns_[static_cast<size_t>(current_)].roll; }

    // The second (bass) instrument: its own synth and piano-roll lane.
    SynthInstrument& synth2() { return synth2_; }
    PianoRoll& roll2() { return patterns_[static_cast<size_t>(current_)].roll2; }

    // --- Patterns & arrangement ---------------------------------------------
    int patternCount() const { return static_cast<int>(patterns_.size()); }
    int currentPattern() const { return current_; }
    void selectPattern(int i);
    int addPattern(); // append an empty pattern; returns its index
    int clonePattern(int src); // append a full copy of pattern `src` (name + " copy"); returns index
    void clearArrangement(); // reset to a single empty pattern, empty playlist, pattern mode

    // Human-readable pattern name (defaults to "Pattern N"), shown in the arrangement UI.
    void setPatternName(int i, const std::string& name);
    const std::string& patternName(int i) const;

    void setSongMode(bool on) { songMode_ = on; }
    bool songMode() const { return songMode_; }
    // Whether the playlist loops (default) or plays once and stops at the end — the latter for
    // bouncing a finite arrangement without a trailing repeat.
    void setSongLoop(bool on) { songLoop_ = on; }
    bool songLoop() const { return songLoop_; }
    const std::vector<int>& playlist() const { return playlist_; }
    void setPlaylist(std::vector<int> seq) { playlist_ = std::move(seq); }
    void clearPlaylist() { playlist_.clear(); }
    void appendToPlaylist(int patternIndex) { playlist_.push_back(patternIndex); }

    // Route the piano roll to the sampler instead of the synth (when a sample is loaded).
    void setUseSampler(bool on) { useSampler_ = on; }
    bool useSampler() const { return useSampler_; }

    // Arpeggiator: when on, held piano-roll chords are played one note per step, cycling through the
    // held pitches. Mode 0 = up, 1 = down, 2 = up-down. `arpCurrentPitch` reflects the last note it
    // played (-1 if silent) — useful for the UI and tests.
    void setArp(bool on, int mode);
    bool arpOn() const { return arpOn_; }
    int arpMode() const { return arpMode_; }
    int arpCurrentPitch() const { return arpCurrentPitch_; }
    // Octave range (1..4): the arp cycles through the held pitches replicated across this many
    // octaves, so it climbs/descends over a wider range.
    void setArpOctaves(int octaves) { arpOctaves_ = octaves < 1 ? 1 : (octaves > 4 ? 4 : octaves); }
    int arpOctaves() const { return arpOctaves_; }

    // Bus levels: relative gain of the drum kit, the lead synth, and the bass synth before the
    // soft-limited sum. (synthGain is the lead level; bassGain the second instrument.)
    void setDrumGain(float g) { drumGain_ = g; }
    void setSynthGain(float g) { synthGain_ = g; }
    void setBassGain(float g) { bassGain_ = g; }

    // Global transpose in semitones (±48): shifts every melodic note (lead, bass, arp, sampler) at
    // playback without editing the notes — change key on the fly.
    void setTranspose(int semis) { transpose_ = semis < -48 ? -48 : (semis > 48 ? 48 : semis); }
    int transpose() const { return transpose_; }
    float drumGain() const { return drumGain_; }
    float synthGain() const { return synthGain_; }
    float bassGain() const { return bassGain_; }

    // Stereo pan for the melodic buses (-1 = hard left, 0 = center, +1 = hard right), equal-power.
    void setLeadPan(float p) { leadPan_ = p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p); }
    void setBassPan(float p) { bassPan_ = p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p); }
    float leadPan() const { return leadPan_; }
    float bassPan() const { return bassPan_; }

    // Metronome: an accented click on each beat while playing (a brighter click on the downbeat,
    // step 0 of the bar). A monitoring aid, mixed into the output.
    void setMetronome(bool on) { metronome_ = on; }
    bool metronome() const { return metronome_; }

    // Count-in: play this many bars of clicks before the pattern starts (0 = none). While counting
    // in, only the metronome sounds; the pattern begins when the count-in finishes.
    void setCountInBars(int bars) { countInBars_ = bars < 0 ? 0 : bars; }
    int countInBars() const { return countInBars_; }
    bool countingIn() const { return countingIn_; }

    // --- Pattern grid --------------------------------------------------------
    bool step(int channel, int step) const;
    void setStep(int channel, int step, bool on);
    void toggle(int channel, int step);
    void clear(); // switch every step off

    // Per-step velocity/accent in [0, 1] (0 = off). setStep uses full velocity.
    float stepVelocity(int channel, int step) const;
    void setStepVelocity(int channel, int step, float velocity);

    // Per-step trigger probability in [0, 1] (1 = always fire). A step below 1.0 fires only some of
    // the time, driven by a deterministic per-transport RNG — humanizing/generative grooves.
    float stepProbability(int channel, int step) const;
    void setStepProbability(int channel, int step, float probability);

    // Per-step ratchet count (1–4): a step > 1 retriggers that many evenly-spaced hits within its
    // slot — drum rolls, stutters, hi-hat rushes.
    int stepRatchet(int channel, int step) const;
    void setStepRatchet(int channel, int step, int count);

    // Render `frames` of interleaved STEREO samples, ADDING the panned channel mix into out
    // (out has 2*frames floats). Advances the transport when playing. `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

    // Render the three mixer buses separately into their own interleaved-stereo buffers (each
    // 2*frames floats, ADDED into). This is the per-track path: a caller can run each bus through
    // its own insert chain before summing. Advances the transport exactly like render(). Bus order
    // matches MixerBus: 0 = drums, 1 = lead, 2 = bass.
    void renderStems(float* drums, float* lead, float* bass, int frames, int sampleRate);

private:
    // Frames for a given step at the current tempo. stepsPerBeat_ 16th-notes → 4 steps per beat.
    // Swing lengthens even steps and shortens odd ones, so `step`'s parity matters.
    double samplesPerStep(int sampleRate, int step) const;
    void triggerStep(int step);

    std::vector<DrumVoice> channels_;
    std::vector<std::string> names_;
    std::vector<float> chanVolume_; // per-channel linear level
    std::vector<uint8_t> chanMute_;
    std::vector<uint8_t> chanSolo_;
    std::vector<float> chanPan_; // per-channel pan (-1..1)
    std::vector<int> chanChoke_; // per-channel choke group (0 = none)
    std::vector<Pattern> patterns_; // at least one; patterns_[current_] is edited/played
    int current_ = 0;
    std::vector<int> playlist_;     // ordered pattern indices for song mode
    bool songMode_ = false;
    bool songLoop_ = true;
    int playlistPos_ = 0;

    std::vector<float> mixScratch_;   // per-block, per-channel drum render
    std::vector<float> synthScratch_; // per-block lead (synth + sampler) sum
    std::vector<float> bassScratch_;  // per-block bass (synth2) sum
    std::vector<float> lBuf_;         // per-block stereo accumulators (pre-limit)
    std::vector<float> rBuf_;
    std::vector<float> stemDrums_;    // render()'s temporaries: the three buses before summing
    std::vector<float> stemLead_;
    std::vector<float> stemBass_;

    SynthInstrument synth_{};  // lead instrument playing roll
    SynthInstrument synth2_{}; // bass instrument playing roll2
    Sampler sampler_{};        // alternative lead instrument (sample playback)
    bool useSampler_ = false;
    bool arpOn_ = false;
    int arpMode_ = 0;
    int arpOctaves_ = 1;
    int arpCounter_ = 0;
    int arpCurrentPitch_ = -1;
    float drumGain_ = 1.0f;
    float synthGain_ = 1.0f;
    float bassGain_ = 1.0f;
    float leadPan_ = 0.0f;
    float bassPan_ = 0.0f;
    int transpose_ = 0;

    int numSteps_ = 16;
    int stepsPerBeat_ = 4;
    double bpm_ = 120.0;
    float swing_ = 0.0f;
    float humanize_ = 0.0f;
    uint32_t humanizeCounter_ = 0;
    bool sidechainOn_ = false;
    float scAmount_ = 0.7f;
    float scReleaseMs_ = 200.0f;
    float scEnv_ = 1.0f; // current ducking gain (1 = open)

    bool metronome_ = false;
    int metroLastStep_ = -1;  // last step a click fired on (avoids double-triggering)
    double metroPhase_ = 0.0; // click oscillator phase
    float metroEnv_ = 0.0f;   // click amplitude envelope
    float metroFreq_ = 0.0f;  // current click pitch (accented on the downbeat)
    int countInBars_ = 0;     // bars of count-in before the pattern starts
    bool countingIn_ = false; // currently playing the count-in
    int countInStepsRemaining_ = 0;
    uint32_t probRng_ = 0x9E3779B9u; // deterministic RNG for per-step probability

    int sampleRate_ = 48000; // last render rate, used to schedule ratchet sub-hits
    struct RatchetHit {
        int channel;
        float velocity;
        int framesUntil;
    };
    std::vector<RatchetHit> ratchets_; // pending ratchet retriggers

    bool playing_ = false;
    int currentStep_ = 0;
    double samplesIntoStep_ = 0.0;
};

} // namespace maz::audio
