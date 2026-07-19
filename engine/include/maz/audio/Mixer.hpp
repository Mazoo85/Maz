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

    // Master balance / output pan (-1 = hard left, 0 = centre/transparent, +1 = hard right):
    // attenuates the opposite channel on the final stereo output.
    void setMasterBalance(float b) { masterBalance_ = b < -1.0f ? -1.0f : (b > 1.0f ? 1.0f : b); }
    float masterBalance() const { return masterBalance_; }

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
    PitchShifter& pitchShifter() { return pitchshift_; }
    Bitcrusher& bitcrusher() { return crush_; }
    Gate& gate() { return gate_; }
    HighPass& highpass() { return hp_; }
    Compressor& compressor() { return comp_; }
    MultibandCompressor& multiband() { return mbcomp_; }
    TransientShaper& transient() { return transient_; }
    Chorus& chorus() { return chorus_; }
    Flanger& flanger() { return flanger_; }
    Phaser& phaser() { return phaser_; }
    Delay& delay() { return delay_; }
    Reverb& reverb() { return reverb_; }
    StereoWidener& widener() { return widener_; }
    StereoEnhancer& stereoEnhancer() { return stereoEnhancer_; }
    Utility& utility() { return utility_; }
    Clipper& clipper() { return clipper_; }
    Limiter& limiter() { return limiter_; }
    DeEsser& deEsser() { return deEsser_; }
    AutoPan& autopan() { return autopan_; }
    AutoWah& autowah() { return autowah_; }
    CombResonator& comb() { return comb_; }
    Tremolo& tremolo() { return tremolo_; }
    StereoDelay& stereoDelay() { return stereoDelay_; }
    FormantFilter& formant() { return formant_; }
    MonoBass& monobass() { return monobass_; }
    SubBass& subbass() { return subbass_; }
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

    // Per-bus aux feed: the caller (engine) sums each bus's post-insert signal scaled by that bus's
    // send into these interleaved-stereo buffers and hands them in before process(); process() adds
    // them into the reverb/delay returns (alongside the master send) and clears them. Empty = none.
    void setReverbAux(const std::vector<float>& buf) { reverbAux_ = buf; }
    void setDelayAux(const std::vector<float>& buf) { delayAux_ = buf; }

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
    // True if any per-bus track is soloed (then the engine silences the non-soloed buses).
    bool anyTrackSoloed() const {
        for (const MixerTrack& t : tracks_) {
            if (t.soloed()) {
                return true;
            }
        }
        return false;
    }

    // Process `frames` of interleaved stereo in place: run each enabled effect, then master gain.
    void process(float* stereo, int frames, int sampleRate);

    void reset();

private:
    float masterGain_ = 0.9f;
    float masterBalance_ = 0.0f; // final output stereo balance (-1..1); 0 = centre
    float limiterCeiling_ = 1.0f;
    ParametricEQ peq_{};
    TiltEQ tilt_{};
    Exciter exciter_{};
    LowPass eq_{};
    Distortion dist_{};
    TapeSaturation tape_{};
    RingMod ringmod_{};
    PitchShifter pitchshift_{};
    Bitcrusher crush_{};
    Gate gate_{};
    HighPass hp_{};
    Compressor comp_{};
    MultibandCompressor mbcomp_{};
    TransientShaper transient_{};
    Chorus chorus_{};
    Flanger flanger_{};
    Phaser phaser_{};
    Delay delay_{};
    Reverb reverb_{};
    StereoWidener widener_{};
    StereoEnhancer stereoEnhancer_{};
    Utility utility_{};
    Clipper clipper_{};
    Limiter limiter_{};
    DeEsser deEsser_{};
    AutoPan autopan_{};
    AutoWah autowah_{};
    CombResonator comb_{};
    Tremolo tremolo_{};
    StereoDelay stereoDelay_{};
    FormantFilter formant_{};
    MonoBass monobass_{};
    SubBass subbass_{};
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
    std::vector<float> reverbAux_;   // per-bus reverb-send feed for this block (cleared after process)
    std::vector<float> delayAux_;    // per-bus delay-send feed for this block

    std::array<MixerTrack, static_cast<size_t>(MixerBus::Count)> tracks_{}; // per-bus insert strips
};

} // namespace maz::audio
