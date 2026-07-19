#pragma once

#include "maz/audio/Effect.hpp"
#include "maz/audio/Filter.hpp" // Biquad

#include <array>
#include <vector>

namespace maz::audio {

// Shared tempo-sync note divisions for modulation effects (chorus / flanger). Maps a division index
// (1/1 … 1/16) + BPM to an LFO rate in Hz.
inline constexpr int kModSyncDivisions = 6;
const char* modSyncDivisionName(int div);
float modSyncRateHz(int div, double bpm);

// A 3-band parametric EQ: a low shelf (120 Hz), a sweepable mid peak, and a high shelf (6 kHz),
// each with a gain in dB. Real biquad filters — the "pro mixing" EQ.
class ParametricEQ : public Effect {
public:
    const char* name() const override { return "Parametric EQ"; }
    void setLowGain(float db);
    void setMid(float freq, float q, float db);
    void setHighGain(float db);
    float lowGain() const { return lowDb_; }
    float midFreq() const { return midFreq_; }
    float midQ() const { return midQ_; }
    float midGain() const { return midDb_; }
    float highGain() const { return highDb_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    void recompute(int sampleRate);

    float lowDb_ = 0.0f;
    float midFreq_ = 1000.0f;
    float midQ_ = 1.0f;
    float midDb_ = 0.0f;
    float highDb_ = 0.0f;
    int sr_ = 0;
    bool dirty_ = true;
    Biquad lowL_{}, midL_{}, highL_{};
    Biquad lowR_{}, midR_{}, highR_{};
};

// A stereo feedback delay (echo). `time` sets the tap in ms, `feedback` how much of the wet signal
// re-enters (0..~0.95), `mix` the dry/wet blend (0..1).
class Delay : public Effect {
public:
    const char* name() const override { return "Delay"; }
    void setTime(float ms) { timeMs_ = ms; }
    void setFeedback(float f) { feedback_ = f; }
    void setMix(float m) { mix_ = m; }
    // Ping-pong: feed each channel's echo into the *other* channel's delay line, so repeats bounce
    // left↔right across the stereo field.
    void setPingPong(bool on) { pingPong_ = on; }
    // Damping (0..1): high-cut on the feedback path so each repeat gets darker — analog-style echo.
    void setDamping(float d) { damping_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    // Feedback low-cut (Hz): a high-pass on the feedback path so successive repeats shed their low
    // end and the echoes don't build up into boom/mud — the classic dub/tape delay trick. 0 = off.
    void setFeedbackLowCut(float hz) { fbLowCutHz_ = hz < 0.0f ? 0.0f : (hz > 1000.0f ? 1000.0f : hz); }
    float feedbackLowCut() const { return fbLowCutHz_; }
    // Modulation: an LFO sweeps the delay time by ±`depthMs` at `rateHz`, so the repeats wobble in
    // pitch — the warm, detuned character of analog/BBD/tape echoes. depth 0 (default) = off (a clean
    // digital delay). Uses fractional (interpolated) read only when modulating.
    void setModDepth(float ms) { modDepthMs_ = ms < 0.0f ? 0.0f : (ms > 20.0f ? 20.0f : ms); }
    void setModRate(float hz) { modRateHz_ = hz < 0.0f ? 0.0f : (hz > 10.0f ? 10.0f : hz); }
    float modDepth() const { return modDepthMs_; }
    float modRate() const { return modRateHz_; }
    // Tempo sync: when on, the delay time tracks the transport tempo at the chosen note division
    // (1/4, dotted 1/8, 1/8 triplet, …) instead of the fixed millisecond time. Call updateTempo()
    // each block with the current BPM to recompute the time.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int div) { syncDiv_ = div < 0 ? 0 : (div >= kSyncDivisions ? kSyncDivisions - 1 : div); }
    void updateTempo(double bpm); // recompute timeMs_ from bpm + division when sync is on
    static constexpr int kSyncDivisions = 8;
    static const char* syncDivisionName(int div);
    static float syncTimeMs(int div, double bpm); // note division + BPM → delay time in ms
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }
    float time() const { return timeMs_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }
    bool pingPong() const { return pingPong_; }
    float damping() const { return damping_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float timeMs_ = 300.0f;
    float feedback_ = 0.35f;
    float mix_ = 0.30f;
    bool pingPong_ = false;
    float damping_ = 0.0f;
    bool sync_ = false;   // tempo-sync the delay time
    int syncDiv_ = 4;     // note-division index (default 1/8)
    float dampL_ = 0.0f, dampR_ = 0.0f; // feedback high-cut state per channel
    float fbLowCutHz_ = 0.0f;           // feedback high-pass (low-cut) cutoff; 0 = off
    float lcL_ = 0.0f, lcR_ = 0.0f;     // feedback low-cut one-pole LP state per channel
    float modDepthMs_ = 0.0f;           // delay-time modulation depth (ms); 0 = off
    float modRateHz_ = 0.3f;            // delay-time modulation LFO rate (Hz)
    double modPhase_ = 0.0;             // modulation LFO phase [0,1)
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    int size_ = 0;
    int write_ = 0;
};

// A ring modulator: multiplies the signal by an internal sine carrier at `freq` Hz, producing
// metallic, inharmonic sidebands (the classic robot/bell timbre). `mix` blends dry/wet.
class RingMod : public Effect {
public:
    RingMod() { enabled_ = false; }
    const char* name() const override { return "Ring Mod"; }
    void setFreq(float hz) { freqHz_ = hz < 1.0f ? 1.0f : (hz > 8000.0f ? 8000.0f : hz); }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    float freq() const { return freqHz_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float freqHz_ = 200.0f;
    float mix_ = 1.0f;
    double phase_ = 0.0;
};

// A waveshaping distortion/overdrive. `drive` sets how hard the signal is pushed into a tanh
// saturator (more harmonics), `mix` blends dry/wet. Output is level-normalized so drive doesn't
// just get louder.
class Distortion : public Effect {
public:
    // Waveshaper curve: Soft = tanh overdrive, Hard = digital clip, Fold = wavefolding, SineFold =
    // sine wrap. Each gives a distinct harmonic character for the same drive.
    enum class Curve { Soft, Hard, Fold, SineFold };

    const char* name() const override { return "Distortion"; }
    void setDrive(float d) { drive_ = d; }
    void setMix(float m) { mix_ = m; }
    void setCurve(Curve c) { curve_ = c; }
    // Post tone: a one-pole low-pass on the distorted (wet) signal that tames the fizzy top saturation
    // adds. 20000 Hz (default) = fully open/off; lower it to darken the drive. (Fruity-Dist "low-pass".)
    void setTone(float hz) { toneHz_ = hz < 200.0f ? 200.0f : (hz > 20000.0f ? 20000.0f : hz); }
    float drive() const { return drive_; }
    float mix() const { return mix_; }
    Curve curve() const { return curve_; }
    float tone() const { return toneHz_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float drive_ = 2.0f;
    float mix_ = 0.5f;
    Curve curve_ = Curve::Soft;
    float toneHz_ = 20000.0f; // post low-pass cutoff; 20000 = off
    float toneL_ = 0.0f, toneR_ = 0.0f; // one-pole LP state per channel
};

// A stereo chorus: two LFO-modulated delay lines (left/right in quadrature) widen and thicken the
// sound. `rate` in Hz, `depth` in ms, `mix` dry/wet.
class Chorus : public Effect {
public:
    const char* name() const override { return "Chorus"; }
    void setRate(float hz) { rateHz_ = hz; }
    void setDepth(float ms) { depthMs_ = ms; }
    void setMix(float m) { mix_ = m; }
    // Feedback (0..0.9): routes the wet output back into the delay lines for a deeper, more resonant
    // chorus that edges toward flanging at higher settings. 0 = off (a clean chorus, unchanged).
    void setFeedback(float f) { feedback_ = f < 0.0f ? 0.0f : (f > 0.9f ? 0.9f : f); }
    float feedback() const { return feedback_; }
    // Tempo sync: lock the LFO rate to the transport at the chosen note division (reusing the Tremolo
    // division set). Call updateTempo() each block with the current BPM.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int d) { syncDiv_ = d < 0 ? 0 : (d >= kModSyncDivisions ? kModSyncDivisions - 1 : d); }
    void updateTempo(double bpm);
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }
    float rate() const { return rateHz_; }
    float depth() const { return depthMs_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 0.8f;
    float depthMs_ = 3.0f;
    float mix_ = 0.4f;
    float feedback_ = 0.0f; // wet→delay feedback; 0 = off
    bool sync_ = false; // tempo-sync the LFO rate
    int syncDiv_ = 0;   // note-division index (default 1/1, a slow chorus)
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    int size_ = 0;
    int write_ = 0;
    double phase_ = 0.0;
};

// A flanger: a very short LFO-swept delay (≈0.5–8 ms) fed back on itself, so the moving comb notches
// sweep through the spectrum for the classic "jet plane" whoosh. `rate` Hz, `depth` ms (sweep
// range), `feedback` (0..0.95, resonance), `mix` dry/wet. Distinct from the chorus by its feedback
// and shorter delay.
class Flanger : public Effect {
public:
    Flanger() { enabled_ = false; }
    const char* name() const override { return "Flanger"; }
    void setRate(float hz) { rateHz_ = hz < 0.0f ? 0.0f : (hz > 10.0f ? 10.0f : hz); }
    void setDepth(float ms) { depthMs_ = ms < 0.1f ? 0.1f : (ms > 8.0f ? 8.0f : ms); }
    void setFeedback(float f) { feedback_ = f < 0.0f ? 0.0f : (f > 0.95f ? 0.95f : f); }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    // Tempo sync: lock the sweep LFO rate to the transport at the chosen note division (rhythmic
    // flanging). Call updateTempo() each block with the current BPM.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int d) { syncDiv_ = d < 0 ? 0 : (d >= kModSyncDivisions ? kModSyncDivisions - 1 : d); }
    void updateTempo(double bpm);
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }
    float rate() const { return rateHz_; }
    float depth() const { return depthMs_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 0.3f;
    float depthMs_ = 2.0f;
    float feedback_ = 0.5f;
    float mix_ = 0.5f;
    bool sync_ = false; // tempo-sync the sweep LFO rate
    int syncDiv_ = 1;   // note-division index (default 1/2)
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    int size_ = 0;
    int write_ = 0;
    double phase_ = 0.0;
};

// A lo-fi bitcrusher: reduces bit depth (quantization) and sample rate (sample-and-hold) for a
// crunchy, digital/retro character. `bits` 1..16, `downsample` 1..64 (how many input samples share
// one output), `mix` dry/wet.
class Bitcrusher : public Effect {
public:
    const char* name() const override { return "Bitcrusher"; }
    void setBits(float b) { bits_ = b; }
    void setDownsample(float d) { downsample_ = d; }
    void setMix(float m) { mix_ = m; }
    float bits() const { return bits_; }
    float downsample() const { return downsample_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float bits_ = 8.0f;
    float downsample_ = 4.0f;
    float mix_ = 0.5f;
    float holdL_ = 0.0f;
    float holdR_ = 0.0f;
    int counter_ = 0;
};

// A phaser: a chain of LFO-modulated all-pass stages (with feedback) mixed with the dry signal,
// creating sweeping notches. `rate` Hz, `depth` 0..1, `feedback` 0..0.9, `mix` dry/wet.
class Phaser : public Effect {
public:
    const char* name() const override { return "Phaser"; }
    void setRate(float hz) { rateHz_ = hz; }
    void setDepth(float d) { depth_ = d; }
    void setFeedback(float f) { feedback_ = f; }
    void setMix(float m) { mix_ = m; }
    // All-pass stage count (2..kMaxStages, default 4): more stages carve more/deeper notches for a
    // richer, more dramatic sweep. Clamped to even counts is not required — any count in range works.
    void setStages(int n) { stages_ = n < 2 ? 2 : (n > kMaxStages ? kMaxStages : n); }
    int stages() const { return stages_; }
    // Tempo sync: lock the sweep LFO rate to the transport at the chosen note division (rhythmic
    // phasing). Call updateTempo() each block with the current BPM.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int d) { syncDiv_ = d < 0 ? 0 : (d >= kModSyncDivisions ? kModSyncDivisions - 1 : d); }
    void updateTempo(double bpm);
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }
    float rate() const { return rateHz_; }
    float depth() const { return depth_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    static constexpr int kMaxStages = 12;
    int stages_ = 4;    // active all-pass stages (2..kMaxStages)
    bool sync_ = false; // tempo-sync the sweep LFO rate
    int syncDiv_ = 0;   // note-division index (default 1/1, a slow phaser)
    struct Allpass1 {
        float z = 0.0f;
        float process(float x, float a) {
            const float y = -a * x + z;
            z = x + a * y;
            return y;
        }
    };
    float rateHz_ = 0.5f;
    float depth_ = 0.7f;
    float feedback_ = 0.3f;
    float mix_ = 0.5f;
    double phase_ = 0.0;
    float fbL_ = 0.0f;
    float fbR_ = 0.0f;
    std::array<Allpass1, kMaxStages> apL_{};
    std::array<Allpass1, kMaxStages> apR_{};
};

// An aural exciter / high-frequency enhancer: high-passes at `crossover` Hz, generates harmonics
// from that band with a soft saturator, and mixes them back at `amount` for added air/sparkle
// without touching the body of the sound.
class Exciter : public Effect {
public:
    Exciter() { enabled_ = false; }
    const char* name() const override { return "Exciter"; }
    void setCrossover(float hz) { crossover_ = hz < 1000.0f ? 1000.0f : (hz > 12000.0f ? 12000.0f : hz); }
    void setAmount(float a) { amount_ = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    float crossover() const { return crossover_; }
    float amount() const { return amount_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float crossover_ = 4000.0f;
    float amount_ = 0.3f;
    float lpL_ = 0.0f, lpR_ = 0.0f; // one-pole low-band state (high band = input − low)
};

// A transient shaper (attack/sustain designer): reshapes a sound's dynamic envelope independently of
// its level. `attack` (-1..1) boosts (+) or softens (−) the initial punch of each onset; `sustain`
// (-1..1) lengthens (+) or tightens (−) the body/tail. Detection is level-relative (ratios against a
// slow envelope) so the shaping tracks dynamics, not absolute loudness. At attack=0, sustain=0 the
// gain is exactly unity, so a fresh/neutral instance is bit-transparent.
class TransientShaper : public Effect {
public:
    TransientShaper() { enabled_ = false; }
    const char* name() const override { return "Transient Shaper"; }
    void setAttack(float a) { attack_ = a < -1.0f ? -1.0f : (a > 1.0f ? 1.0f : a); }
    void setSustain(float s) { sustain_ = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s); }
    float attack() const { return attack_; }
    float sustain() const { return sustain_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float attack_ = 0.0f;
    float sustain_ = 0.0f;
    // Envelope followers on the stereo-summed magnitude. Attack detection compares a fast-attack vs a
    // slow-attack follower (onset = fast rises ahead of slow); sustain detection compares a
    // fast-release vs a slow-release follower (body = slow lags behind fast on the way down).
    float envAttFast_ = 0.0f, envAttSlow_ = 0.0f;
    float envRelFast_ = 0.0f, envRelSlow_ = 0.0f;
};

// A one-knob "tilt" EQ (mastering tone control): a single `tilt` in dB pivots the spectrum around a
// centre frequency — positive brightens (low shelf down, high shelf up by tilt/2), negative darkens.
class TiltEQ : public Effect {
public:
    TiltEQ() { enabled_ = false; }
    const char* name() const override { return "Tilt EQ"; }
    void setTilt(float db) { tilt_ = db < -12.0f ? -12.0f : (db > 12.0f ? 12.0f : db); dirty_ = true; }
    float tilt() const { return tilt_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    void recompute(int sampleRate);
    float tilt_ = 0.0f;
    int sr_ = 0;
    bool dirty_ = true;
    Biquad lowL_{}, highL_{}, lowR_{}, highR_{};
};

// A one-pole low-pass "tone" control — a simple EQ that rolls off highs above `cutoff` Hz.
class LowPass : public Effect {
public:
    const char* name() const override { return "Low-Pass EQ"; }
    void setCutoff(float hz) { cutoff_ = hz; }
    float cutoff() const { return cutoff_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float cutoff_ = 8000.0f;
    float yL_ = 0.0f;
    float yR_ = 0.0f;
};

// A one-pole high-pass filter — rolls off lows below `cutoff` Hz. The go-to tool for removing
// rumble / DC and cleaning up the low end of a bus.
class HighPass : public Effect {
public:
    HighPass() { enabled_ = false; }
    const char* name() const override { return "High-Pass"; }
    void setCutoff(float hz) { cutoff_ = hz; }
    float cutoff() const { return cutoff_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float cutoff_ = 30.0f;
    float xL_ = 0.0f, yL_ = 0.0f; // previous input/output per channel
    float xR_ = 0.0f, yR_ = 0.0f;
};

// A stereo-linked peak compressor. Tames dynamics: above `threshold` dB the signal is reduced by
// `ratio`:1, with `attack`/`release` in ms and `makeup` dB applied after.
class Compressor : public Effect {
public:
    const char* name() const override { return "Compressor"; }
    void setThresholdDb(float db) { thresholdDb_ = db; }
    void setRatio(float r) { ratio_ = r; }
    void setAttackMs(float ms) { attackMs_ = ms; }
    void setReleaseMs(float ms) { releaseMs_ = ms; }
    void setMakeupDb(float db) { makeupDb_ = db; }
    // Knee width in dB: 0 = hard knee (abrupt at the threshold); wider = a gradual onset of
    // compression that starts below the threshold for a smoother, more transparent sound.
    void setKneeDb(float db) { kneeDb_ = db < 0.0f ? 0.0f : (db > 24.0f ? 24.0f : db); }
    // Dry/wet blend for parallel ("New York") compression: 1 = fully compressed (default), lower
    // values mix the uncompressed signal back in to keep transients and punch.
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    // Sidechain high-pass (Hz): high-pass the *detection* signal only, so low frequencies (kick,
    // bass) don't drive the gain reduction and pump the whole mix. The gain is still applied to the
    // full-range signal. 0 (default) = off (detect on the full signal).
    void setSidechainHpf(float hz) { scHpfHz_ = hz < 0.0f ? 0.0f : (hz > 500.0f ? 500.0f : hz); }
    float thresholdDb() const { return thresholdDb_; }
    float ratio() const { return ratio_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }
    float makeupDb() const { return makeupDb_; }
    float kneeDb() const { return kneeDb_; }
    float mix() const { return mix_; }
    float sidechainHpf() const { return scHpfHz_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -18.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 8.0f;
    float releaseMs_ = 120.0f;
    float makeupDb_ = 0.0f;
    float kneeDb_ = 0.0f; // 0 = hard knee
    float mix_ = 1.0f;     // dry/wet blend; 1 = fully compressed
    float scHpfHz_ = 0.0f; // sidechain (detection) high-pass cutoff; 0 = off
    float env_ = 0.0f; // linear peak-envelope follower
    float scLpL_ = 0.0f, scLpR_ = 0.0f; // detection high-pass state (one-pole LP; HP = x − LP)
};

// A 3-band multiband compressor (a Maximus-style master dynamics tool). The signal is split into low
// / mid / high bands at two crossovers, each band is compressed independently (its own threshold and
// ratio), then the bands are summed. The one-pole crossover split reconstructs the input exactly when
// no band is compressing, so an enabled unit with all ratios at 1 (or a disabled unit) is transparent.
// Great for gluing a mix, controlling boomy lows without dulling highs, or taming harsh mids.
class MultibandCompressor : public Effect {
public:
    static constexpr int kBands = 3; // 0 = low, 1 = mid, 2 = high
    MultibandCompressor() { enabled_ = false; }
    const char* name() const override { return "Multiband Comp"; }

    void setCrossoverLow(float hz) { crossLow_ = hz < 20.0f ? 20.0f : (hz > 2000.0f ? 2000.0f : hz); }
    void setCrossoverHigh(float hz) {
        crossHigh_ = hz < 200.0f ? 200.0f : (hz > 18000.0f ? 18000.0f : hz);
    }
    float crossoverLow() const { return crossLow_; }
    float crossoverHigh() const { return crossHigh_; }

    void setBandThreshold(int band, float db) {
        if (band >= 0 && band < kBands) {
            thr_[band] = db < -60.0f ? -60.0f : (db > 0.0f ? 0.0f : db);
        }
    }
    void setBandRatio(int band, float r) {
        if (band >= 0 && band < kBands) {
            ratio_[band] = r < 1.0f ? 1.0f : (r > 20.0f ? 20.0f : r);
        }
    }
    float bandThreshold(int band) const {
        return band >= 0 && band < kBands ? thr_[band] : 0.0f;
    }
    float bandRatio(int band) const { return band >= 0 && band < kBands ? ratio_[band] : 1.0f; }

    void setAttackMs(float ms) { attackMs_ = ms < 0.1f ? 0.1f : (ms > 200.0f ? 200.0f : ms); }
    void setReleaseMs(float ms) { releaseMs_ = ms < 1.0f ? 1.0f : (ms > 1000.0f ? 1000.0f : ms); }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float crossLow_ = 250.0f;
    float crossHigh_ = 2500.0f;
    float thr_[kBands] = {-18.0f, -18.0f, -18.0f};
    float ratio_[kBands] = {3.0f, 3.0f, 3.0f};
    float attackMs_ = 10.0f;
    float releaseMs_ = 120.0f;
    float lp1L_ = 0.0f, lp1R_ = 0.0f; // one-pole LP state at the low/mid crossover (per channel)
    float lp2L_ = 0.0f, lp2R_ = 0.0f; // one-pole LP state at the mid/high crossover
    float env_[kBands] = {0.0f, 0.0f, 0.0f}; // per-band peak-envelope followers
};

// A de-esser: a frequency-selective compressor that tames only the high band (sibilance / harsh "ess"
// sounds) while leaving the body of the signal untouched. The signal is split at `frequency` Hz into
// a low band and a high band; the high band's level is followed and, above `threshold` dB, ducked
// (up to `amount` toward a brickwall at the threshold) before the bands are summed back. Unlike the
// full-band compressor, low frequencies pass through unchanged.
class DeEsser : public Effect {
public:
    DeEsser() { enabled_ = false; }
    const char* name() const override { return "De-Esser"; }
    void setThresholdDb(float db) { thresholdDb_ = db < -60.0f ? -60.0f : (db > 0.0f ? 0.0f : db); }
    void setFrequency(float hz) { frequency_ = hz < 1000.0f ? 1000.0f : (hz > 16000.0f ? 16000.0f : hz); }
    void setAmount(float a) { amount_ = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    void setReleaseMs(float ms) { releaseMs_ = ms < 1.0f ? 1.0f : (ms > 500.0f ? 500.0f : ms); }
    float thresholdDb() const { return thresholdDb_; }
    float frequency() const { return frequency_; }
    float amount() const { return amount_; }
    float releaseMs() const { return releaseMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -24.0f;
    float frequency_ = 6000.0f;
    float amount_ = 0.8f;
    float releaseMs_ = 60.0f;
    float lpL_ = 0.0f, lpR_ = 0.0f; // one-pole low-band state per channel (high band = input − this)
    float env_ = 0.0f;             // high-band peak-envelope follower (stereo-linked)
};

// A stereo-linked noise gate / downward expander. Below `threshold` dB the signal is attenuated:
// for each dB under the threshold the output drops by `ratio`:1, down to a floor of `range` dB.
// `attack`/`release` (ms) smooth the gate opening/closing so it does not click. Above the threshold
// the signal passes untouched — the classic tool for silencing hiss between notes.
class Gate : public Effect {
public:
    Gate() { enabled_ = false; }
    const char* name() const override { return "Gate"; }
    void setThresholdDb(float db) { thresholdDb_ = db; }
    void setRatio(float r) { ratio_ = r; }
    void setRangeDb(float db) { rangeDb_ = db; }
    void setAttackMs(float ms) { attackMs_ = ms; }
    void setReleaseMs(float ms) { releaseMs_ = ms; }
    // Hold time (ms): once opened, keep the gate open at least this long after the signal drops
    // below the threshold — prevents chatter and stops short tails from being clipped.
    void setHoldMs(float ms) { holdMs_ = ms < 0.0f ? 0.0f : (ms > 2000.0f ? 2000.0f : ms); }
    float thresholdDb() const { return thresholdDb_; }
    float ratio() const { return ratio_; }
    float rangeDb() const { return rangeDb_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }
    float holdMs() const { return holdMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -40.0f;
    float ratio_ = 4.0f;
    float rangeDb_ = -60.0f; // maximum attenuation floor
    float attackMs_ = 2.0f;
    float releaseMs_ = 80.0f;
    float holdMs_ = 0.0f;
    int holdCounter_ = 0; // samples remaining that the gate is held open
    float env_ = 0.0f;   // peak-envelope follower
    float gain_ = 1.0f;  // smoothed gate gain
};

// Analogue-style tape saturation. Drives the signal into a tanh soft-knee (adding harmonics), with
// a touch of asymmetry for even-harmonic "warmth" and a gentle one-pole high-frequency roll-off that
// emulates tape's top-end loss. `drive` sets how hard it is pushed (1 = subtle, up ~ crunchy),
// `warmth` (0..1) sets the high-cut amount, `mix` blends dry/wet.
class TapeSaturation : public Effect {
public:
    TapeSaturation() { enabled_ = false; }
    const char* name() const override { return "Tape Saturation"; }
    void setDrive(float d) { drive_ = d < 1.0f ? 1.0f : (d > 12.0f ? 12.0f : d); }
    void setWarmth(float w) { warmth_ = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w); }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    // Wow & flutter (0..1): analog-tape pitch instability — a slow "wow" plus a faster "flutter"
    // wobble the playback speed via a modulated delay, for that unstable vintage character. 0 = off
    // (rock-steady pitch).
    void setWowFlutter(float amount) { wowFlutter_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount); }
    float drive() const { return drive_; }
    float warmth() const { return warmth_; }
    float mix() const { return mix_; }
    float wowFlutter() const { return wowFlutter_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float drive_ = 2.0f;
    float warmth_ = 0.3f;
    float mix_ = 1.0f;
    float wowFlutter_ = 0.0f; // pitch-wobble depth; 0 = off
    float lpL_ = 0.0f; // one-pole high-cut state per channel
    float lpR_ = 0.0f;
    std::vector<float> wfL_, wfR_; // wow/flutter modulated delay lines
    int wfSize_ = 0;
    int wfWrite_ = 0;
    double wowPhase_ = 0.0;
    double flutPhase_ = 0.0;
};

// A bass mono-maker: sums everything below `crossover` Hz to mono (tight, centered low end) while
// leaving the high band stereo — the standard fix for wandering/phasey bass. `crossover` 20–500 Hz.
class MonoBass : public Effect {
public:
    MonoBass() { enabled_ = false; }
    const char* name() const override { return "Mono Bass"; }
    void setCrossover(float hz) { crossover_ = hz < 20.0f ? 20.0f : (hz > 500.0f ? 500.0f : hz); }
    float crossover() const { return crossover_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float crossover_ = 120.0f;
    float lpL_ = 0.0f, lpR_ = 0.0f; // one-pole low-band state per channel
};

// A sub-harmonic bass generator: synthesizes a tone one octave below the input's low-frequency
// content and mixes it in, reinforcing weak kicks/basslines with club-ready sub. It low-passes the
// centre (mid) signal to isolate the fundamental, flips a square once per input cycle (halving the
// frequency → one octave down), tracks the bass amplitude with an envelope follower so it only
// sounds when bass is present, and rounds the square with a tone low-pass. The generated sub is
// centred (mono) so the low end stays tight. `amount` 0 (default) = off (dry passes untouched).
class SubBass : public Effect {
public:
    SubBass() { enabled_ = false; }
    const char* name() const override { return "Sub Bass"; }
    // Wet level of the generated sub-octave, mixed on top of the dry signal (0 = off, 1 = full).
    void setAmount(float a) { amount_ = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    // Only bass below this frequency is tracked/reinforced (the generator follows this band).
    void setCutoff(float hz) { cutoff_ = hz < 40.0f ? 40.0f : (hz > 320.0f ? 320.0f : hz); }
    // Tone of the generated sub: a low-pass that rounds the square toward a sine (softer = lower Hz).
    void setTone(float hz) { tone_ = hz < 60.0f ? 60.0f : (hz > 1000.0f ? 1000.0f : hz); }
    float amount() const { return amount_; }
    float cutoff() const { return cutoff_; }
    float tone() const { return tone_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float amount_ = 0.0f;   // wet sub level; 0 = off
    float cutoff_ = 120.0f; // isolate/track bass below this Hz
    float tone_ = 220.0f;   // low-pass on the generated sub
    float lp_ = 0.0f;       // one-pole low-pass state on the tracked mid
    float env_ = 0.0f;      // amplitude follower on the tracked bass
    float sq_ = 1.0f;       // current square-wave sign (±1), flips each input cycle
    float sub_ = 0.0f;      // tone-smoothed sub output state
    float prevLp_ = 0.0f;   // previous low-passed sample (for zero-cross detection)
};

// An auto-panner: an internal LFO sweeps the stereo position at `rate` Hz, `depth` 0..1 (0 = none,
// 1 = full hard-left↔hard-right), using an equal-power law so the perceived loudness stays constant.
class AutoPan : public Effect {
public:
    AutoPan() { enabled_ = false; }
    const char* name() const override { return "Auto-Pan"; }
    void setRate(float hz) { rateHz_ = hz < 0.01f ? 0.01f : (hz > 20.0f ? 20.0f : hz); }
    void setDepth(float d) { depth_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    float rate() const { return rateHz_; }
    float depth() const { return depth_; }
    // Tempo sync: lock the pan LFO rate to the transport at the chosen note division for rhythmic
    // panning. Call updateTempo() each block with the current BPM.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int d) { syncDiv_ = d < 0 ? 0 : (d >= kModSyncDivisions ? kModSyncDivisions - 1 : d); }
    void updateTempo(double bpm);
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 1.0f;
    float depth_ = 0.5f;
    bool sync_ = false; // tempo-sync the pan LFO rate
    int syncDiv_ = 2;   // note-division index (default 1 bar)
    double phase_ = 0.0; // LFO phase in [0, 1)
};

// An envelope filter / auto-wah: a resonant low-pass whose cutoff rides the input's own amplitude
// envelope. Louder input pushes the cutoff up from `baseHz` toward `baseHz + sensitivity·rangeHz`,
// so each note opens and closes the filter like a wah pedal driven by playing dynamics. `resonance`
// sharpens the peak; `attackMs`/`releaseMs` set how fast the envelope follower tracks. Off by
// default.
class AutoWah : public Effect {
public:
    AutoWah() { enabled_ = false; }
    const char* name() const override { return "Auto-Wah"; }
    void setBaseHz(float hz) { baseHz_ = hz < 40.0f ? 40.0f : (hz > 8000.0f ? 8000.0f : hz); }
    void setRangeHz(float hz) { rangeHz_ = hz < 0.0f ? 0.0f : (hz > 12000.0f ? 12000.0f : hz); }
    void setSensitivity(float s) { sensitivity_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    void setResonance(float r) { resonance_ = r < 0.5f ? 0.5f : (r > 20.0f ? 20.0f : r); }
    void setAttackMs(float ms) { attackMs_ = ms < 0.1f ? 0.1f : ms; }
    void setReleaseMs(float ms) { releaseMs_ = ms < 1.0f ? 1.0f : ms; }
    // Direction: false (default) = louder input sweeps the cutoff UP from base (classic wah); true =
    // DOWNward, louder input closes the filter from base+range toward base (a reverse/"anti" wah).
    void setDownward(bool d) { downward_ = d; }
    bool downward() const { return downward_; }
    float baseHz() const { return baseHz_; }
    float rangeHz() const { return rangeHz_; }
    float sensitivity() const { return sensitivity_; }
    float resonance() const { return resonance_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float baseHz_ = 300.0f;
    float rangeHz_ = 3000.0f;
    float sensitivity_ = 0.7f;
    float resonance_ = 4.0f;
    float attackMs_ = 5.0f;
    float releaseMs_ = 80.0f;
    bool downward_ = false; // true = louder input lowers the cutoff (reverse wah)
    float env_ = 0.0f; // amplitude-envelope follower
    StateVariableFilter lpL_{};
    StateVariableFilter lpR_{};
};

// A tuned feedback comb resonator: it feeds a delayed, scaled copy of its own output back in, so the
// delay length `frequency` (Hz → delay = sampleRate/freq) rings at that pitch and its harmonics.
// `feedback` (0..0.98) sets the resonance/ring time; `mix` blends the resonated signal with the dry.
// Turns any input into a pitched, metallic/plucked resonance — the classic comb/Karplus tone.
class CombResonator : public Effect {
public:
    CombResonator() { enabled_ = false; }
    const char* name() const override { return "Comb Resonator"; }
    void setFrequency(float hz) { freq_ = hz < 20.0f ? 20.0f : (hz > 5000.0f ? 5000.0f : hz); }
    void setFeedback(float f) { feedback_ = f < 0.0f ? 0.0f : (f > 0.98f ? 0.98f : f); }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    float frequency() const { return freq_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float freq_ = 220.0f;
    float feedback_ = 0.8f;
    float mix_ = 0.5f;
    std::vector<float> bufL_; // circular delay lines (sized on first process)
    std::vector<float> bufR_;
    int writePos_ = 0;
};

// A tremolo / trance-gate: an amplitude LFO that dips the level rhythmically. `rate` sets the LFO
// speed (Hz), `depth` how deep the dips go (0 = none, 1 = down to silence), and `shape` picks a Sine
// LFO (smooth tremolo) or Square LFO (a hard on/off trance gate). Both channels are modulated
// together, so the stereo image is untouched. Off by default.
class Tremolo : public Effect {
public:
    enum class Shape { Sine, Square };
    Tremolo() { enabled_ = false; }
    const char* name() const override { return "Tremolo"; }
    void setRate(float hz) { rateHz_ = hz < 0.05f ? 0.05f : (hz > 30.0f ? 30.0f : hz); }
    void setDepth(float d) { depth_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    void setShape(Shape s) { shape_ = s; }
    // Tempo sync: when on, the LFO rate tracks the transport tempo at the chosen note division
    // (a synced trance gate). Call updateTempo() each block with the current BPM to recompute the rate.
    void setSync(bool on) { sync_ = on; }
    void setSyncDivision(int div) { syncDiv_ = div < 0 ? 0 : (div >= kSyncDivisions ? kSyncDivisions - 1 : div); }
    void updateTempo(double bpm); // recompute rateHz_ from bpm + division when sync is on
    static constexpr int kSyncDivisions = 6;
    static const char* syncDivisionName(int div);
    bool sync() const { return sync_; }
    int syncDivision() const { return syncDiv_; }
    float rate() const { return rateHz_; }
    float depth() const { return depth_; }
    Shape shape() const { return shape_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 5.0f;
    float depth_ = 0.5f;
    Shape shape_ = Shape::Sine;
    bool sync_ = false; // tempo-sync the LFO rate
    int syncDiv_ = 3;   // note-division index (default 1/8)
    double phase_ = 0.0; // LFO phase in [0, 1)
};

// A stereo (dual) delay: each channel has its own independent delay time, so the left and right
// echoes fall at different intervals — wide, dubby, cross-rhythmic stereo echoes the single-time
// Delay can't make. `feedback` sets the repeat tail; `mix` the dry/wet blend.
class StereoDelay : public Effect {
public:
    StereoDelay() { enabled_ = false; }
    const char* name() const override { return "Stereo Delay"; }
    void setLeftMs(float ms) { leftMs_ = ms < 1.0f ? 1.0f : (ms > 2000.0f ? 2000.0f : ms); }
    void setRightMs(float ms) { rightMs_ = ms < 1.0f ? 1.0f : (ms > 2000.0f ? 2000.0f : ms); }
    void setFeedback(float f) { feedback_ = f < 0.0f ? 0.0f : (f > 0.95f ? 0.95f : f); }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    // Feedback tone (like the mono Delay): `damping` (0..1) high-cuts each repeat so echoes darken;
    // `feedbackLowCut` (Hz) high-passes the feedback so echoes shed their lows. Both 0 = off/bright.
    void setDamping(float d) { damping_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d); }
    void setFeedbackLowCut(float hz) { fbLowCutHz_ = hz < 0.0f ? 0.0f : (hz > 1000.0f ? 1000.0f : hz); }
    float damping() const { return damping_; }
    float feedbackLowCut() const { return fbLowCutHz_; }
    // Tempo sync: when on, each channel's delay time tracks the transport tempo at its own note
    // division (reusing the Delay division set). Call updateTempo() each block with the current BPM.
    void setSync(bool on) { sync_ = on; }
    void setLeftDivision(int d) { leftDiv_ = d < 0 ? 0 : (d >= Delay::kSyncDivisions ? Delay::kSyncDivisions - 1 : d); }
    void setRightDivision(int d) { rightDiv_ = d < 0 ? 0 : (d >= Delay::kSyncDivisions ? Delay::kSyncDivisions - 1 : d); }
    void updateTempo(double bpm);
    bool sync() const { return sync_; }
    int leftDivision() const { return leftDiv_; }
    int rightDivision() const { return rightDiv_; }
    float leftMs() const { return leftMs_; }
    float rightMs() const { return rightMs_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float leftMs_ = 250.0f;
    float rightMs_ = 375.0f;
    float feedback_ = 0.4f;
    float mix_ = 0.3f;
    bool sync_ = false;  // tempo-sync the L/R delay times
    int leftDiv_ = 4;    // left note-division index (default 1/8)
    int rightDiv_ = 5;   // right note-division index (default dotted 1/8)
    float damping_ = 0.0f;    // feedback high-cut (0 = off/bright)
    float fbLowCutHz_ = 0.0f; // feedback high-pass (low-cut) cutoff; 0 = off
    float dampL_ = 0.0f, dampR_ = 0.0f; // feedback high-cut one-pole state per channel
    float lcL_ = 0.0f, lcR_ = 0.0f;     // feedback low-cut one-pole state per channel
    std::vector<float> bufL_; // circular delay lines (sized on first process)
    std::vector<float> bufR_;
    int writePos_ = 0;
};

// A formant (vowel) filter: two resonant band-pass filters tuned to the first two formants of a
// chosen vowel (A/E/I/O/U), summed and blended with the dry signal — imposes a vocal "aah/eee/…"
// colour on whatever passes through (talkbox/robot-voice character). `mix` sets dry/wet.
class FormantFilter : public Effect {
public:
    enum class Vowel { A, E, I, O, U };
    FormantFilter() { enabled_ = false; }
    const char* name() const override { return "Formant Filter"; }
    void setVowel(Vowel v) { vowel_ = v; }
    void setMix(float m) { mix_ = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m); }
    Vowel vowel() const { return vowel_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    Vowel vowel_ = Vowel::A;
    float mix_ = 0.5f;
    StateVariableFilter f1L_{}, f2L_{}, f1R_{}, f2R_{};
};

// A mid/side stereo widener. Splits the signal into mid (L+R) and side (L-R), scales the side by
// `width`, and recombines: width 1 = unchanged, 0 = mono, >1 widens the stereo image (up to 2).
// A cheap, transparent way to control stereo spread on a bus.
class StereoWidener : public Effect {
public:
    StereoWidener() { enabled_ = false; }
    const char* name() const override { return "Stereo Widener"; }
    void setWidth(float w) { width_ = w < 0.0f ? 0.0f : (w > 2.0f ? 2.0f : w); }
    float width() const { return width_; }
    // Bass mono: collapse the side (stereo) signal below `hz` to mono so the low end stays centred and
    // tight while the highs are widened — the standard "keep the bass mono" mastering move. 0 = off.
    void setBassMonoHz(float hz) { bassMonoHz_ = hz < 0.0f ? 0.0f : (hz > 500.0f ? 500.0f : hz); }
    float bassMonoHz() const { return bassMonoHz_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float width_ = 1.0f;
    float bassMonoHz_ = 0.0f; // mono the side below this frequency; 0 = off
    float sideLp_ = 0.0f;     // one-pole low-pass state on the side signal
};

// A Haas stereo enhancer: delays one channel by a few milliseconds so the signal is decorrelated
// across the ears, widening the image via the Haas/precedence effect — and unlike a mid/side widener
// it broadens even a mono source. `delayMs` (0..40) sets the offset and `amount` (0..1) blends the
// delayed channel in. A `mono` sum for compatibility checking is left to the Utility effect.
class StereoEnhancer : public Effect {
public:
    StereoEnhancer() { enabled_ = false; }
    const char* name() const override { return "Stereo Enhancer"; }
    void setDelayMs(float ms) { delayMs_ = ms < 0.0f ? 0.0f : (ms > 40.0f ? 40.0f : ms); }
    void setAmount(float a) { amount_ = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
    float delayMs() const { return delayMs_; }
    float amount() const { return amount_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float delayMs_ = 12.0f;
    float amount_ = 0.7f;
    std::vector<float> buf_; // delay line for the right channel
    int widx_ = 0;
};

// A utility / output-stage tool: a gain trim in dB, independent left/right phase (polarity) invert,
// and a mono-sum toggle — the standard mixing fixes for polarity issues, level trims, and mono
// checks. Transparent at 0 dB with no inverts and mono off.
class Utility : public Effect {
public:
    Utility() { enabled_ = false; }
    const char* name() const override { return "Utility"; }
    void setGainDb(float db) { gainDb_ = db < -24.0f ? -24.0f : (db > 24.0f ? 24.0f : db); }
    void setInvertL(bool on) { invertL_ = on; }
    void setInvertR(bool on) { invertR_ = on; }
    void setMono(bool on) { mono_ = on; }
    float gainDb() const { return gainDb_; }
    bool invertL() const { return invertL_; }
    bool invertR() const { return invertR_; }
    bool mono() const { return mono_; }

    void process(float* stereo, int frames, int sampleRate) override;

private:
    float gainDb_ = 0.0f;
    bool invertL_ = false;
    bool invertR_ = false;
    bool mono_ = false;
};

// A soft/hard clipper (a Fruity-Soft-Clipper-style loudness tool). Unlike the drive-based Distortion
// (a tanh waveshaper with its own dry/wet) and the look-ahead Limiter (with attack/release), this is
// an *instantaneous*, zero-latency ceiling: an input `drive` (dB) pushes the signal into a `ceiling`
// and every sample is shaped so it can never exceed it. `hardness` morphs the knee from a smooth tanh
// saturation (0 — gentle harmonics, peaks rounded) to a hard clamp (1 — peaks flat-topped, the classic
// "clip for loudness" sound). Off by default (transparent). Stateless (no reset needed).
class Clipper : public Effect {
public:
    Clipper() { enabled_ = false; }
    const char* name() const override { return "Clipper"; }
    void setDriveDb(float db) { driveDb_ = db < 0.0f ? 0.0f : (db > 36.0f ? 36.0f : db); }
    void setCeiling(float c) { ceiling_ = c < 0.05f ? 0.05f : (c > 1.0f ? 1.0f : c); }
    void setHardness(float h) { hardness_ = h < 0.0f ? 0.0f : (h > 1.0f ? 1.0f : h); }
    float driveDb() const { return driveDb_; }
    float ceiling() const { return ceiling_; }
    float hardness() const { return hardness_; }

    void process(float* stereo, int frames, int sampleRate) override;

private:
    float driveDb_ = 0.0f;
    float ceiling_ = 0.9f;
    float hardness_ = 1.0f; // 1 = hard clamp, 0 = soft tanh knee
};

// A brickwall look-ahead limiter (a Fruity-Limiter-style maximizer). An `inputGain` (dB) pushes the
// signal harder for loudness; a short `lookahead` window lets the gain drop *before* a transient
// arrives, so the output is guaranteed never to exceed the `ceiling` (dB, ≤ 0) with no audible
// attack distortion. Gain recovers over `release` ms. The look-ahead adds that many samples of
// latency. Off by default (transparent).
class Limiter : public Effect {
public:
    Limiter() { enabled_ = false; }
    const char* name() const override { return "Limiter"; }
    void setInputGainDb(float db) { inputGainDb_ = db < 0.0f ? 0.0f : (db > 36.0f ? 36.0f : db); }
    void setCeilingDb(float db) { ceilingDb_ = db < -24.0f ? -24.0f : (db > 0.0f ? 0.0f : db); }
    void setReleaseMs(float ms) { releaseMs_ = ms < 1.0f ? 1.0f : (ms > 1000.0f ? 1000.0f : ms); }
    void setLookaheadMs(float ms) { lookaheadMs_ = ms < 0.1f ? 0.1f : (ms > 10.0f ? 10.0f : ms); }
    float inputGainDb() const { return inputGainDb_; }
    float ceilingDb() const { return ceilingDb_; }
    float releaseMs() const { return releaseMs_; }
    float lookaheadMs() const { return lookaheadMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float inputGainDb_ = 0.0f;
    float ceilingDb_ = -0.3f;
    float releaseMs_ = 100.0f;
    float lookaheadMs_ = 2.0f;
    std::vector<float> dL_, dR_, dPeak_; // look-ahead delay lines (L, R, per-sample peak)
    int bufLen_ = 0;   // current look-ahead length in samples
    int widx_ = 0;     // write/oldest index into the ring
    float gain_ = 1.0f; // smoothed gain reduction (≤ 1)
};

// A Schroeder/Freeverb-style reverb (comb filters into allpass diffusers). `roomSize` sets the tail
// length (0..~0.95), `damping` how fast highs decay, `mix` the dry/wet blend.
class Reverb : public Effect {
public:
    const char* name() const override { return "Reverb"; }
    void setRoomSize(float r) { roomSize_ = r; }
    void setDamping(float d) { damping_ = d; }
    void setMix(float m) { mix_ = m; }
    // Pre-delay (ms): a gap before the reverb tail begins, so the dry hit stays clear and the space
    // reads as larger. 0 = none (up to ~250 ms).
    void setPreDelayMs(float ms) { preDelayMs_ = ms < 0.0f ? 0.0f : (ms > 250.0f ? 250.0f : ms); }
    // Stereo width of the wet tail (0 = mono, 1 = natural, 2 = extra-wide).
    void setWidth(float w) { width_ = w < 0.0f ? 0.0f : (w > 2.0f ? 2.0f : w); }
    // Freeze: hold the current tail indefinitely (lossless feedback, input muted) — an infinite
    // ambient pad / performance hold. Off = normal decaying reverb.
    void setFreeze(bool on) { freeze_ = on; }
    // Ducking (0..1): sidechain the wet tail to the dry input's own level — while the dry is loud the
    // wet is pushed down, and it swells back in the gaps. Keeps vocals/leads clear over a big reverb.
    // 0 = off (normal reverb).
    void setDuck(float amount) { duck_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount); }
    // Gated reverb (ms): the classic 80s effect — the wet tail plays at full level while the input is
    // present and for this long after it stops, then is cut off sharply instead of decaying naturally.
    // 0 (default) = off (a normal, freely-decaying tail). Up to 1000 ms.
    void setGateMs(float ms) { gateMs_ = ms < 0.0f ? 0.0f : (ms > 1000.0f ? 1000.0f : ms); }
    float gateMs() const { return gateMs_; }
    // Wet-tail tone: a low-cut (high-pass) and high-cut (low-pass) applied to the wet signal only, so
    // the reverb can be kept out of the mud (low-cut) and the harsh top (high-cut) without touching
    // the dry. lowCut 0 = off (no low removed); highCut 20000 = off (no high removed). Distinct from
    // `damping`, which shapes the tail's decay rather than filtering the wet output.
    void setWetLowCut(float hz) { lowCutHz_ = hz < 0.0f ? 0.0f : (hz > 2000.0f ? 2000.0f : hz); }
    void setWetHighCut(float hz) {
        highCutHz_ = hz < 500.0f ? 500.0f : (hz > 20000.0f ? 20000.0f : hz);
    }
    float roomSize() const { return roomSize_; }
    float damping() const { return damping_; }
    float mix() const { return mix_; }
    float preDelayMs() const { return preDelayMs_; }
    float width() const { return width_; }
    bool freeze() const { return freeze_; }
    float duck() const { return duck_; }
    float wetLowCut() const { return lowCutHz_; }
    float wetHighCut() const { return highCutHz_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    static constexpr int kCombs = 4;
    static constexpr int kAllpass = 2;

    struct Comb {
        std::vector<float> buf;
        int idx = 0;
        float store = 0.0f;
        float process(float in, float feedback, float damp);
        void setSize(int n);
    };
    struct Allpass {
        std::vector<float> buf;
        int idx = 0;
        float process(float in, float feedback);
        void setSize(int n);
    };

    void ensureSized(int sampleRate);

    float roomSize_ = 0.7f;
    float damping_ = 0.35f;
    float mix_ = 0.25f;
    float preDelayMs_ = 0.0f;
    float width_ = 1.0f;
    bool freeze_ = false; // hold the tail indefinitely
    float duck_ = 0.0f;   // sidechain the wet to the dry level; 0 = off
    float duckEnv_ = 0.0f; // dry-input peak-envelope follower for ducking
    float gateMs_ = 0.0f;  // gated-reverb hold time; 0 = off (natural decay)
    float gateGain_ = 1.0f; // current gate gain applied to the wet tail
    int gateCountdown_ = 0; // samples left of the open-hold before the gate closes
    float lowCutHz_ = 0.0f;      // wet-tail high-pass; 0 = off
    float highCutHz_ = 20000.0f; // wet-tail low-pass; 20000 = off
    float lcL_ = 0.0f, lcR_ = 0.0f; // low-cut one-pole LP state (subtracted → high-pass)
    float hcL_ = 0.0f, hcR_ = 0.0f; // high-cut one-pole LP state
    std::vector<float> preBuf_; // pre-delay line (mono input)
    int preWrite_ = 0;
    int sizedFor_ = 0; // sampleRate the buffers were built for (0 = unsized)
    std::array<Comb, kCombs> combsL_{};
    std::array<Comb, kCombs> combsR_{};
    std::array<Allpass, kAllpass> apsL_{};
    std::array<Allpass, kAllpass> apsR_{};
};

} // namespace maz::audio
