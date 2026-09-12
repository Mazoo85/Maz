// tests/audio/tempoestimate.cpp — verifies tempo (BPM) estimation (TempoEstimate.hpp).
// Ground truths, deterministic (synthetic click tracks of known tempo):
//   * near the perceptual prior centre (120 BPM) the estimate is exact to within a few BPM;
//   * off-centre tempos are correct to within an OCTAVE (the standard "Accuracy-2" criterion — a steady
//     beat autocorrelates equally at half/double its period, so octave-equivalent answers are accepted);
//   * a detected tempo carries positive confidence;
//   * silence reports nothing usable.
#include "maz/audio/TempoEstimate.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::audio::estimateTempo;
using maz::audio::TempoResult;

static constexpr float kSr = 44100.0f;

// A click track at `bpm`: short decaying "tick" bursts on each beat, `seconds` long.
static std::vector<float> clickTrack(float bpm, float seconds) {
    const int n = static_cast<int>(kSr * seconds);
    std::vector<float> s(static_cast<size_t>(n), 0.0f);
    const int period = static_cast<int>(kSr * 60.0f / bpm);
    const int clickLen = 800;
    for (int beat = 0; beat * period < n; ++beat) {
        const int start = beat * period;
        for (int i = 0; i < clickLen && start + i < n; ++i) {
            const float env = std::exp(-static_cast<float>(i) / 150.0f);
            s[static_cast<size_t>(start + i)] =
                env * std::sin(2.0f * 3.14159265f * 2000.0f * static_cast<float>(i) / kSr);
        }
    }
    return s;
}

// Correct to within `tol` of the true tempo OR a clean octave of it (half / double).
static bool octaveMatch(float est, float truth, float tol = 3.0f) {
    return std::fabs(est - truth) < tol || std::fabs(est - truth * 0.5f) < tol ||
           std::fabs(est - truth * 2.0f) < tol;
}

int main() {
    // --- 1. Exactness at the prior centre. ---
    {
        const TempoResult r = estimateTempo(clickTrack(120.0f, 6.0f), kSr);
        CHECK(r.found, "120 BPM click track is detected");
        CHECK(std::fabs(r.bpm - 120.0f) < 3.0f, "at the prior centre the estimate is exact (~120)");
        CHECK(r.confidence > 0.0f, "detected tempo has positive confidence");
    }

    // --- 2. Off-centre tempos: correct to within an octave. ---
    {
        const float tempos[] = {90.0f, 100.0f, 140.0f, 150.0f};
        for (float bpm : tempos) {
            const TempoResult r = estimateTempo(clickTrack(bpm, 6.0f), kSr);
            CHECK(r.found, "off-centre click track is detected");
            CHECK(octaveMatch(r.bpm, bpm), "estimate matches the true tempo within an octave");
        }
    }

    // --- 3. Silence -> nothing usable. ---
    {
        const std::vector<float> quiet(static_cast<size_t>(kSr * 4.0f), 0.0f);
        const TempoResult r = estimateTempo(quiet, kSr);
        CHECK(!r.found || r.confidence < 1e-6f, "silence yields no real tempo");
    }

    if (g_fail == 0) {
        std::printf("tempoestimate: OK — exact at 120, octave-correct at 90/100/140/150, confidence, "
                    "silence.\n");
        return 0;
    }
    std::printf("tempoestimate: %d failure(s).\n", g_fail);
    return 1;
}
