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
    std::vector<uint8_t> grid; // channel-major: grid[channel * numSteps + step]
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
    int stepsPerBeat() const { return stepsPerBeat_; }
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
    void clearArrangement(); // reset to a single empty pattern, empty playlist, pattern mode

    void setSongMode(bool on) { songMode_ = on; }
    bool songMode() const { return songMode_; }
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

    // Bus levels: relative gain of the drum kit vs. the melodic synth before the soft-limited sum.
    void setDrumGain(float g) { drumGain_ = g; }
    void setSynthGain(float g) { synthGain_ = g; }
    float drumGain() const { return drumGain_; }
    float synthGain() const { return synthGain_; }

    // --- Pattern grid --------------------------------------------------------
    bool step(int channel, int step) const;
    void setStep(int channel, int step, bool on);
    void toggle(int channel, int step);
    void clear(); // switch every step off

    // Per-step velocity/accent in [0, 1] (0 = off). setStep uses full velocity.
    float stepVelocity(int channel, int step) const;
    void setStepVelocity(int channel, int step, float velocity);

    // Render `frames` of interleaved STEREO samples, ADDING the panned channel mix into out
    // (out has 2*frames floats). Advances the transport when playing. `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

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
    std::vector<Pattern> patterns_; // at least one; patterns_[current_] is edited/played
    int current_ = 0;
    std::vector<int> playlist_;     // ordered pattern indices for song mode
    bool songMode_ = false;
    int playlistPos_ = 0;

    std::vector<float> mixScratch_;   // per-block, per-channel drum render
    std::vector<float> synthScratch_; // per-block synth sum
    std::vector<float> lBuf_;         // per-block stereo accumulators (pre-limit)
    std::vector<float> rBuf_;

    SynthInstrument synth_{};  // lead instrument playing roll
    SynthInstrument synth2_{}; // bass instrument playing roll2
    Sampler sampler_{};        // alternative lead instrument (sample playback)
    bool useSampler_ = false;
    bool arpOn_ = false;
    int arpMode_ = 0;
    int arpCounter_ = 0;
    int arpCurrentPitch_ = -1;
    float drumGain_ = 1.0f;
    float synthGain_ = 1.0f;

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

    bool playing_ = false;
    int currentStep_ = 0;
    double samplesIntoStep_ = 0.0;
};

} // namespace maz::audio
