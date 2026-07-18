#pragma once

#include "maz/audio/ClapHost.hpp"
#include "maz/audio/Effects.hpp"
#include "maz/audio/MixerTrack.hpp"
#include "maz/audio/PluginHost.hpp"
#include "maz/audio/Vst3Host.hpp"

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

    // Master limiter output ceiling (0.1–1.0): the peak the soft-limiter asymptotes to. Lower it for
    // headroom (e.g. 0.89 ≈ -1 dBFS true-peak margin).
    void setLimiterCeiling(float c) { limiterCeiling_ = c < 0.1f ? 0.1f : (c > 1.0f ? 1.0f : c); }
    float limiterCeiling() const { return limiterCeiling_; }

    // Typed access for the UI / demos.
    ParametricEQ& peq() { return peq_; }
    TiltEQ& tilt() { return tilt_; }
    Exciter& exciter() { return exciter_; }
    LowPass& eq() { return eq_; }
    Distortion& distortion() { return dist_; }
    TapeSaturation& tape() { return tape_; }
    RingMod& ringmod() { return ringmod_; }
    Bitcrusher& bitcrusher() { return crush_; }
    Gate& gate() { return gate_; }
    HighPass& highpass() { return hp_; }
    Compressor& compressor() { return comp_; }
    TransientShaper& transient() { return transient_; }
    Chorus& chorus() { return chorus_; }
    Flanger& flanger() { return flanger_; }
    Phaser& phaser() { return phaser_; }
    Delay& delay() { return delay_; }
    Reverb& reverb() { return reverb_; }
    StereoWidener& widener() { return widener_; }
    Utility& utility() { return utility_; }
    Limiter& limiter() { return limiter_; }
    DeEsser& deEsser() { return deEsser_; }
    AutoPan& autopan() { return autopan_; }
    AutoWah& autowah() { return autowah_; }
    CombResonator& comb() { return comb_; }
    Tremolo& tremolo() { return tremolo_; }
    StereoDelay& stereoDelay() { return stereoDelay_; }
    FormantFilter& formant() { return formant_; }
    MonoBass& monobass() { return monobass_; }
    PluginHost& plugin() { return plugin_; } // a dynamically-loaded native plugin, last in the chain
    ClapHost& clap() { return clap_; }       // a loaded CLAP-format plugin
    Vst3Host& vst3() { return vst3_; }       // a loaded VST3-format plugin

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
    float limiterCeiling_ = 1.0f;
    ParametricEQ peq_{};
    TiltEQ tilt_{};
    Exciter exciter_{};
    LowPass eq_{};
    Distortion dist_{};
    TapeSaturation tape_{};
    RingMod ringmod_{};
    Bitcrusher crush_{};
    Gate gate_{};
    HighPass hp_{};
    Compressor comp_{};
    TransientShaper transient_{};
    Chorus chorus_{};
    Flanger flanger_{};
    Phaser phaser_{};
    Delay delay_{};
    Reverb reverb_{};
    StereoWidener widener_{};
    Utility utility_{};
    Limiter limiter_{};
    DeEsser deEsser_{};
    AutoPan autopan_{};
    AutoWah autowah_{};
    CombResonator comb_{};
    Tremolo tremolo_{};
    StereoDelay stereoDelay_{};
    FormantFilter formant_{};
    MonoBass monobass_{};
    PluginHost plugin_{};
    ClapHost clap_{};
    Vst3Host vst3_{};
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
