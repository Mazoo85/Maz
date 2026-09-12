// tests/audio/pitchdetect.cpp — verifies YIN pitch detection (audio PitchDetect.hpp).
// Ground truths, deterministic (synthesised tones at known frequencies):
//   * a pure sine is detected within ~1 Hz across the musical range (220/440/880 Hz);
//   * a harmonic-rich tone (fundamental + overtones) still reports the FUNDAMENTAL, not an octave —
//     the failure mode naive autocorrelation is prone to;
//   * silence reports no pitch (found = false);
//   * a confident detection reports high confidence.
#include "maz/audio/PitchDetect.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::audio::detectPitchYin;
using maz::audio::PitchResult;

static constexpr float kPi = 3.14159265358979324f;
static constexpr float kSr = 44100.0f;

// A pure sine of `freq` Hz, `n` samples.
static std::vector<float> sine(float freq, int n) {
    std::vector<float> s(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        s[static_cast<size_t>(i)] = std::sin(2.0f * kPi * freq * static_cast<float>(i) / kSr);
    }
    return s;
}

int main() {
    // --- 1. Pure sines across the range. ---
    {
        const float freqs[] = {220.0f, 440.0f, 880.0f};
        for (float f : freqs) {
            const std::vector<float> buf = sine(f, 4096);
            const PitchResult r = detectPitchYin(buf, kSr);
            CHECK(r.found, "pure sine is detected");
            CHECK(std::fabs(r.frequency - f) < 1.5f, "detected frequency is within ~1.5 Hz of the tone");
        }
    }

    // --- 2. Harmonic-rich tone still reports the fundamental (no octave error). ---
    {
        const float f0 = 330.0f; // E4
        std::vector<float> buf(4096);
        for (int i = 0; i < 4096; ++i) {
            const float t = static_cast<float>(i) / kSr;
            // Fundamental plus strong 2nd and 3rd harmonics — a "brighter than the fundamental" spectrum.
            buf[static_cast<size_t>(i)] = 0.6f * std::sin(2.0f * kPi * f0 * t) +
                                          0.9f * std::sin(2.0f * kPi * 2.0f * f0 * t) +
                                          0.7f * std::sin(2.0f * kPi * 3.0f * f0 * t);
        }
        const PitchResult r = detectPitchYin(buf, kSr);
        CHECK(r.found, "harmonic tone is detected");
        CHECK(std::fabs(r.frequency - f0) < 3.0f, "reports the fundamental, not a harmonic/octave");
    }

    // --- 3. Silence -> no pitch. ---
    {
        const std::vector<float> quiet(4096, 0.0f);
        const PitchResult r = detectPitchYin(quiet, kSr);
        CHECK(!r.found, "silence reports no pitch");
    }

    // --- 4. Confidence is high for a clean tone. ---
    {
        const std::vector<float> buf = sine(440.0f, 4096);
        const PitchResult r = detectPitchYin(buf, kSr);
        CHECK(r.confidence > 0.8f, "a clean sine yields high confidence");
    }

    if (g_fail == 0) {
        std::printf("pitchdetect: OK — pure sines 220/440/880, harmonic fundamental, silence, "
                    "confidence.\n");
        return 0;
    }
    std::printf("pitchdetect: %d failure(s).\n", g_fail);
    return 1;
}
