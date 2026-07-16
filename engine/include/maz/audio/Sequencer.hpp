#pragma once

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/SynthInstrument.hpp"

#include <string>
#include <vector>

namespace maz::audio {

// An FL-style step sequencer (a "channel rack"): a grid of channels × steps, a transport
// (play/stop + BPM), and a set of built-in drum voices — one per channel. When playing, it walks
// the step grid in sample-accurate time and strikes each channel whose step is switched on.
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

    // Number of grid steps that make up one loop of the pattern (default 16 = one 4/4 bar of 16ths).
    int numSteps() const { return numSteps_; }
    int numChannels() const { return static_cast<int>(channels_.size()); }
    const std::string& channelName(int channel) const { return names_[static_cast<size_t>(channel)]; }
    DrumVoice& channelVoice(int channel) { return channels_[static_cast<size_t>(channel)]; }

    // The step currently sounding (0..numSteps-1); useful for a playhead in the UI.
    int currentStep() const { return currentStep_; }

    // The melodic side: a pitched synth playing the piano-roll pattern, on the same transport.
    SynthInstrument& synth() { return synth_; }
    PianoRoll& roll() { return roll_; }

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

    // Render `frames` mono samples, ADDING the channel mix into out. Advances the transport when
    // playing. `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

private:
    // Frames per step at the current tempo. stepsPerBeat_ 16th-notes → 4 steps per beat.
    double samplesPerStep(int sampleRate) const;
    void triggerStep(int step);

    std::vector<DrumVoice> channels_;
    std::vector<std::string> names_;
    std::vector<uint8_t> grid_; // channel-major: grid_[channel * numSteps_ + step]
    std::vector<float> mixScratch_;   // per-block drum sum
    std::vector<float> synthScratch_; // per-block synth sum

    SynthInstrument synth_{}; // melodic instrument playing the piano roll
    PianoRoll roll_{};
    float drumGain_ = 1.0f;
    float synthGain_ = 1.0f;

    int numSteps_ = 16;
    int stepsPerBeat_ = 4;
    double bpm_ = 120.0;

    bool playing_ = false;
    int currentStep_ = 0;
    double samplesIntoStep_ = 0.0;
};

} // namespace maz::audio
