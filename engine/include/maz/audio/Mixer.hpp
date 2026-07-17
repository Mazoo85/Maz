#pragma once

#include "maz/audio/ClapHost.hpp"
#include "maz/audio/Effects.hpp"
#include "maz/audio/MixerTrack.hpp"
#include "maz/audio/PluginHost.hpp"

#include <array>
#include <vector>

namespace maz::audio {

// The master bus mixer: a fixed chain of built-in effects (EQ → compressor → delay → reverb) run in
// order over the stereo output, then a master gain. Individual effects are disabled by default so
// the dry signal is untouched until the user (or a demo) switches them on.
//
// Per-instrument track levels (drums, synth, tone) live upstream on the sources; this is the final
// stage every sound passes through.
class Mixer {
public:
    Mixer();

    void setMasterGain(float g) { masterGain_ = g; }
    float masterGain() const { return masterGain_; }

    // Typed access for the UI / demos.
    ParametricEQ& peq() { return peq_; }
    LowPass& eq() { return eq_; }
    Distortion& distortion() { return dist_; }
    Bitcrusher& bitcrusher() { return crush_; }
    Compressor& compressor() { return comp_; }
    Chorus& chorus() { return chorus_; }
    Phaser& phaser() { return phaser_; }
    Delay& delay() { return delay_; }
    Reverb& reverb() { return reverb_; }
    PluginHost& plugin() { return plugin_; } // a dynamically-loaded native plugin, last in the chain
    ClapHost& clap() { return clap_; }       // a loaded CLAP-format plugin

    // Aux send/return buses (FL-style parallel routing). Unlike the inline inserts above, a send
    // taps a scaled copy of the signal into a dedicated return effect (processed 100% wet) and sums
    // it back — so reverb/delay can sit on a shared parallel bus. Send level 0 → the bus is silent
    // (transparent). These are separate from, and additive to, the inline reverb_/delay_ inserts.
    Reverb& reverbReturn() { return reverbReturn_; }
    float reverbSend() const { return reverbSend_; }
    void setReverbSend(float s) { reverbSend_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    Delay& delayReturn() { return delayReturn_; }
    float delaySend() const { return delaySend_; }
    void setDelaySend(float s) { delaySend_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }

    // Generic iteration over the chain (for a mixer strip that lists every effect).
    int effectCount() const { return static_cast<int>(chain_.size()); }
    Effect& effect(int i) { return *chain_[static_cast<size_t>(i)]; }

    // Per-bus insert strips (drums / lead / bass). A caller with separated stems processes each bus
    // through its track before summing to the master. `anyTrackActive()` lets the engine skip the
    // per-bus path entirely when every track is transparent.
    MixerTrack& track(MixerBus b) { return tracks_[static_cast<size_t>(b)]; }
    MixerTrack& track(int i) { return tracks_[static_cast<size_t>(i)]; }
    static constexpr int trackCount() { return static_cast<int>(MixerBus::Count); }
    bool anyTrackActive() const;

    // Process `frames` of interleaved stereo in place: run each enabled effect, then master gain.
    void process(float* stereo, int frames, int sampleRate);

    void reset();

private:
    float masterGain_ = 0.9f;
    ParametricEQ peq_{};
    LowPass eq_{};
    Distortion dist_{};
    Bitcrusher crush_{};
    Compressor comp_{};
    Chorus chorus_{};
    Phaser phaser_{};
    Delay delay_{};
    Reverb reverb_{};
    PluginHost plugin_{};
    ClapHost clap_{};
    std::vector<Effect*> chain_; // processing order; points at the members above

    // Parallel send/return buses.
    Reverb reverbReturn_{};
    float reverbSend_ = 0.0f;
    Delay delayReturn_{};
    float delaySend_ = 0.0f;
    std::vector<float> sendScratch_; // scratch for the send-tapped copy

    std::array<MixerTrack, static_cast<size_t>(MixerBus::Count)> tracks_{}; // per-bus insert strips
};

} // namespace maz::audio
