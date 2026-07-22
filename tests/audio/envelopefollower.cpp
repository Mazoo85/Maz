// tests/audio/envelopefollower.cpp — verifies the envelope follower + level meters (EnvelopeFollower.hpp).
// Ground truths, deterministic:
//   * rms of a unit sine is 1/sqrt(2) ~ 0.707; peakLevel is ~1.0; rms/peak of DC A is A;
//   * a peak follower fed a constant A converges to A;
//   * the one-pole step response reaches exactly 1 - 1/e (~0.632) of the target after one attack time
//     constant, and decays to 1/e (~0.368) after one release time constant;
//   * an RMS follower settles at the true RMS of a steady sine;
//   * a fast-attack / slow-release follower rises quicker than it falls.
#include "maz/audio/EnvelopeFollower.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::audio::DetectMode;
using maz::audio::EnvelopeFollower;
using maz::audio::peakLevel;
using maz::audio::rms;

static constexpr float kPi = 3.14159265358979324f;
static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Block meters on a unit sine and on DC. ---
    {
        std::vector<float> s(2000);
        for (int i = 0; i < 2000; ++i) s[static_cast<size_t>(i)] = std::sin(2.0f * kPi * 5.0f * static_cast<float>(i) / 100.0f);
        CHECK(near(rms(s), 1.0f / std::sqrt(2.0f), 2e-3f), "rms of a unit sine is 1/sqrt(2)");
        CHECK(near(peakLevel(s), 1.0f, 2e-3f), "peak of a unit sine is ~1");
        std::vector<float> dc(100, 0.5f);
        CHECK(near(rms(dc), 0.5f), "rms of DC 0.5 is 0.5");
        CHECK(near(peakLevel(dc), 0.5f), "peak of DC 0.5 is 0.5");
    }

    // --- 2. Peak follower converges to a constant input. ---
    {
        EnvelopeFollower env;
        env.configure(5.0f, 5.0f, 1000.0f, DetectMode::Peak);
        env.reset(0.0f);
        float v = 0.0f;
        for (int i = 0; i < 2000; ++i) v = env.process(0.8f);
        CHECK(near(v, 0.8f, 1e-3f), "peak follower converges to a constant input level");
    }

    // --- 3. Exact one-pole step response after one attack time constant. ---
    {
        EnvelopeFollower env;
        // attack tau = 100 samples (100 ms at 1000 Hz).
        env.configure(100.0f, 100.0f, 1000.0f, DetectMode::Peak);
        env.reset(0.0f);
        float v = 0.0f;
        for (int i = 0; i < 100; ++i) v = env.process(1.0f); // one tau of a 0->1 step
        CHECK(near(v, 1.0f - 1.0f / std::exp(1.0f), 5e-3f), "attack reaches 1-1/e after one time constant");
    }

    // --- 4. Release decays to 1/e after one release time constant. ---
    {
        EnvelopeFollower env;
        env.configure(100.0f, 100.0f, 1000.0f, DetectMode::Peak);
        env.reset(1.0f); // start already charged to 1
        float v = 1.0f;
        for (int i = 0; i < 100; ++i) v = env.process(0.0f); // one tau of a 1->0 step
        CHECK(near(v, 1.0f / std::exp(1.0f), 5e-3f), "release decays to 1/e after one time constant");
    }

    // --- 5. RMS follower settles at the true RMS of a steady sine. ---
    {
        EnvelopeFollower env;
        env.configure(20.0f, 20.0f, 4410.0f, DetectMode::Rms);
        env.reset(0.0f);
        float v = 0.0f;
        for (int i = 0; i < 8820; ++i) { // 2 seconds of 100 Hz sine
            const float x = std::sin(2.0f * kPi * 100.0f * static_cast<float>(i) / 4410.0f);
            v = env.process(x);
        }
        CHECK(near(v, 1.0f / std::sqrt(2.0f), 2e-2f), "RMS follower settles at the sine's true RMS");
    }

    // --- 6. Fast attack, slow release: rises faster than it falls. ---
    {
        EnvelopeFollower env;
        env.configure(5.0f, 200.0f, 1000.0f, DetectMode::Peak);
        env.reset(0.0f);
        float riseVal = 0.0f;
        for (int i = 0; i < 20; ++i) riseVal = env.process(1.0f); // 20 ms of loud
        // riseVal should already be high (fast attack).
        float fallVal = riseVal;
        for (int i = 0; i < 20; ++i) fallVal = env.process(0.0f); // 20 ms of silence
        CHECK(riseVal > 0.9f, "fast attack rises to near the target within a few ms");
        CHECK(fallVal > 0.7f, "slow release still holds a high level after the same time");
        CHECK(fallVal < riseVal, "the level does fall during release");
    }

    if (g_fail == 0) {
        std::printf("envelopefollower: OK — block meters, convergence, attack/release time constants, "
                    "RMS settle, fast-attack/slow-release.\n");
        return 0;
    }
    std::printf("envelopefollower: %d failure(s).\n", g_fail);
    return 1;
}
