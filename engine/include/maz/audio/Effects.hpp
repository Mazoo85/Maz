#pragma once

#include "maz/audio/Effect.hpp"
#include "maz/audio/Filter.hpp" // Biquad

#include <array>
#include <vector>

namespace maz::audio {

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
    float dampL_ = 0.0f, dampR_ = 0.0f; // feedback high-cut state per channel
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
    float drive() const { return drive_; }
    float mix() const { return mix_; }
    Curve curve() const { return curve_; }

    void process(float* stereo, int frames, int sampleRate) override;

private:
    float drive_ = 2.0f;
    float mix_ = 0.5f;
    Curve curve_ = Curve::Soft;
};

// A stereo chorus: two LFO-modulated delay lines (left/right in quadrature) widen and thicken the
// sound. `rate` in Hz, `depth` in ms, `mix` dry/wet.
class Chorus : public Effect {
public:
    const char* name() const override { return "Chorus"; }
    void setRate(float hz) { rateHz_ = hz; }
    void setDepth(float ms) { depthMs_ = ms; }
    void setMix(float m) { mix_ = m; }
    float rate() const { return rateHz_; }
    float depth() const { return depthMs_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 0.8f;
    float depthMs_ = 3.0f;
    float mix_ = 0.4f;
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
    float rate() const { return rateHz_; }
    float depth() const { return depth_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    static constexpr int kStages = 4;
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
    std::array<Allpass1, kStages> apL_{};
    std::array<Allpass1, kStages> apR_{};
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
    float thresholdDb() const { return thresholdDb_; }
    float ratio() const { return ratio_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }
    float makeupDb() const { return makeupDb_; }
    float kneeDb() const { return kneeDb_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -18.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 8.0f;
    float releaseMs_ = 120.0f;
    float makeupDb_ = 0.0f;
    float kneeDb_ = 0.0f; // 0 = hard knee
    float env_ = 0.0f; // linear peak-envelope follower
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
    float drive() const { return drive_; }
    float warmth() const { return warmth_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float drive_ = 2.0f;
    float warmth_ = 0.3f;
    float mix_ = 1.0f;
    float lpL_ = 0.0f; // one-pole high-cut state per channel
    float lpR_ = 0.0f;
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

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 1.0f;
    float depth_ = 0.5f;
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
    float rate() const { return rateHz_; }
    float depth() const { return depth_; }
    Shape shape() const { return shape_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float rateHz_ = 5.0f;
    float depth_ = 0.5f;
    Shape shape_ = Shape::Sine;
    double phase_ = 0.0; // LFO phase in [0, 1)
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

    void process(float* stereo, int frames, int sampleRate) override;

private:
    float width_ = 1.0f;
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
    float roomSize() const { return roomSize_; }
    float damping() const { return damping_; }
    float mix() const { return mix_; }
    float preDelayMs() const { return preDelayMs_; }
    float width() const { return width_; }

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
    std::vector<float> preBuf_; // pre-delay line (mono input)
    int preWrite_ = 0;
    int sizedFor_ = 0; // sampleRate the buffers were built for (0 = unsized)
    std::array<Comb, kCombs> combsL_{};
    std::array<Comb, kCombs> combsR_{};
    std::array<Allpass, kAllpass> apsL_{};
    std::array<Allpass, kAllpass> apsR_{};
};

} // namespace maz::audio
