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
    float time() const { return timeMs_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }
    bool pingPong() const { return pingPong_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float timeMs_ = 300.0f;
    float feedback_ = 0.35f;
    float mix_ = 0.30f;
    bool pingPong_ = false;
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    int size_ = 0;
    int write_ = 0;
};

// A waveshaping distortion/overdrive. `drive` sets how hard the signal is pushed into a tanh
// saturator (more harmonics), `mix` blends dry/wet. Output is level-normalized so drive doesn't
// just get louder.
class Distortion : public Effect {
public:
    const char* name() const override { return "Distortion"; }
    void setDrive(float d) { drive_ = d; }
    void setMix(float m) { mix_ = m; }
    float drive() const { return drive_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;

private:
    float drive_ = 2.0f;
    float mix_ = 0.5f;
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
    float thresholdDb() const { return thresholdDb_; }
    float ratio() const { return ratio_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }
    float makeupDb() const { return makeupDb_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -18.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 8.0f;
    float releaseMs_ = 120.0f;
    float makeupDb_ = 0.0f;
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
    float thresholdDb() const { return thresholdDb_; }
    float ratio() const { return ratio_; }
    float rangeDb() const { return rangeDb_; }
    float attackMs() const { return attackMs_; }
    float releaseMs() const { return releaseMs_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float thresholdDb_ = -40.0f;
    float ratio_ = 4.0f;
    float rangeDb_ = -60.0f; // maximum attenuation floor
    float attackMs_ = 2.0f;
    float releaseMs_ = 80.0f;
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
    float roomSize() const { return roomSize_; }
    float damping() const { return damping_; }
    float mix() const { return mix_; }

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
    int sizedFor_ = 0; // sampleRate the buffers were built for (0 = unsized)
    std::array<Comb, kCombs> combsL_{};
    std::array<Comb, kCombs> combsR_{};
    std::array<Allpass, kAllpass> apsL_{};
    std::array<Allpass, kAllpass> apsR_{};
};

} // namespace maz::audio
