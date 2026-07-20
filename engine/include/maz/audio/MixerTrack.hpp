#pragma once

#include "maz/audio/Effects.hpp"

namespace maz::audio {

// One mixer track ("insert strip", FL-style): a per-bus chain of insert effects run in order over
// that bus's stereo signal, then a track gain, with mute. Each source bus (drums, lead, bass) owns
// one of these so it can be EQ'd, driven, and compressed independently before hitting the master.
// Every insert is disabled by default, so a fresh track is transparent (just unity gain).
//
// The insert set mirrors the most-used FL channel inserts: a 3-band parametric EQ, a distortion, and
// a compressor. Pure DSP (no SDL), so it is unit-testable on its own.
class MixerTrack {
public:
    MixerTrack() {
        gate_.setEnabled(false);
        hp_.setEnabled(false);
        transient_.setEnabled(false);
        eq_.setEnabled(false);
        dist_.setEnabled(false);
        comp_.setEnabled(false);
        stereoEnh_.setEnabled(false);
        // Order: gate the input, clean the lows, shape transients, then EQ → drive → glue-compress →
        // stereo-widen (imaging last, after dynamics).
        chain_ = {&gate_, &hp_, &transient_, &eq_, &dist_, &comp_, &stereoEnh_};
    }

    void setGain(float g) { gain_ = g; }
    float gain() const { return gain_; }
    void setMuted(bool m) { muted_ = m; }
    bool muted() const { return muted_; }
    // Solo: when any bus is soloed, only soloed buses are heard (the engine silences the rest). A
    // monitoring/focus aid, resolved across buses at mix time.
    void setSoloed(bool s) { soloed_ = s; }
    bool soloed() const { return soloed_; }
    // Per-bus aux sends (0..1): how much of this bus feeds the shared parallel reverb / delay return,
    // on top of the master send — so each bus can have its own amount of reverb/delay. 0 = none.
    void setReverbSend(float s) { reverbSend_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    float reverbSend() const { return reverbSend_; }
    void setDelaySend(float s) { delaySend_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    float delaySend() const { return delaySend_; }
    // Stereo balance for the bus (-1 = hard left, 0 = centre/transparent, +1 = hard right): attenuates
    // the opposite channel, so a stereo bus keeps its image at centre and leans to one side off it.
    void setPan(float p) { pan_ = p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p); }
    float pan() const { return pan_; }
    // Output routing: −1 (default) = straight to the master bus; ≥0 = the index of a mixer group
    // (submix) track this bus feeds instead, so several buses can share one insert chain before master.
    void setOutput(int t) { output_ = t < 0 ? -1 : t; }
    int output() const { return output_; }

    Gate& gate() { return gate_; }       // noise gate on the bus input
    HighPass& highpass() { return hp_; } // clean the bus's low end before the other inserts
    TransientShaper& transientShaper() { return transient_; } // punch/snap or soften the bus
    ParametricEQ& eq() { return eq_; }
    Distortion& distortion() { return dist_; }
    Compressor& compressor() { return comp_; }
    StereoEnhancer& stereoEnhancer() { return stereoEnh_; } // Haas widener on the bus (imaging)

    int effectCount() const { return static_cast<int>(chain_.size()); }
    Effect& effect(int i) { return *chain_[static_cast<size_t>(i)]; }

    // Whether this track changes its input at all (any insert on, non-unity gain, or muted).
    bool active() const {
        if (muted_ || soloed_ || gain_ != 1.0f || pan_ != 0.0f || reverbSend_ > 0.0f ||
            delaySend_ > 0.0f) {
            return true;
        }
        for (const Effect* fx : chain_) {
            if (fx->enabled()) {
                return true;
            }
        }
        return false;
    }

    // Process `frames` of interleaved stereo in place: run each enabled insert, then apply gain
    // (or silence when muted).
    void process(float* stereo, int frames, int sampleRate) {
        if (frames <= 0 || sampleRate <= 0) {
            return;
        }
        const int n = frames * 2;
        if (muted_) {
            for (int i = 0; i < n; ++i) {
                stereo[i] = 0.0f;
            }
            return;
        }
        for (Effect* fx : chain_) {
            fx->process(stereo, frames, sampleRate); // no-ops when disabled
        }
        if (gain_ != 1.0f) {
            for (int i = 0; i < n; ++i) {
                stereo[i] *= gain_;
            }
        }
        // Balance: attenuate the channel opposite the pan direction (transparent at centre).
        if (pan_ != 0.0f) {
            const float lg = pan_ > 0.0f ? 1.0f - pan_ : 1.0f;
            const float rg = pan_ < 0.0f ? 1.0f + pan_ : 1.0f;
            for (int i = 0; i < frames; ++i) {
                stereo[2 * i] *= lg;
                stereo[2 * i + 1] *= rg;
            }
        }
    }

    void reset() {
        for (Effect* fx : chain_) {
            fx->reset();
        }
    }

private:
    float gain_ = 1.0f;
    bool muted_ = false;
    bool soloed_ = false;
    float reverbSend_ = 0.0f; // per-bus send to the shared reverb return (0 = none)
    float delaySend_ = 0.0f;  // per-bus send to the shared delay return (0 = none)
    float pan_ = 0.0f; // stereo balance (-1..1); 0 = centre
    int output_ = -1;  // routing: -1 = master; >=0 = a mixer group (submix) index
    Gate gate_{};
    HighPass hp_{};
    TransientShaper transient_{};
    ParametricEQ eq_{};
    Distortion dist_{};
    Compressor comp_{};
    StereoEnhancer stereoEnh_{};
    std::vector<Effect*> chain_;
};

// The three source buses fed by the sequencer's renderStems(), in order.
enum class MixerBus { Drums = 0, Lead = 1, Bass = 2, Count = 3 };

} // namespace maz::audio
