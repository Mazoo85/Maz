#pragma once

#include "maz/audio/Effect.hpp"

#include <array>
#include <vector>

namespace maz::audio {

// A stereo feedback delay (echo). `time` sets the tap in ms, `feedback` how much of the wet signal
// re-enters (0..~0.95), `mix` the dry/wet blend (0..1).
class Delay : public Effect {
public:
    const char* name() const override { return "Delay"; }
    void setTime(float ms) { timeMs_ = ms; }
    void setFeedback(float f) { feedback_ = f; }
    void setMix(float m) { mix_ = m; }
    float time() const { return timeMs_; }
    float feedback() const { return feedback_; }
    float mix() const { return mix_; }

    void process(float* stereo, int frames, int sampleRate) override;
    void reset() override;

private:
    float timeMs_ = 300.0f;
    float feedback_ = 0.35f;
    float mix_ = 0.30f;
    std::vector<float> bufL_;
    std::vector<float> bufR_;
    int size_ = 0;
    int write_ = 0;
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
