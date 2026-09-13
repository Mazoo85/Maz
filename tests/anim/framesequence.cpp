// tests/anim/framesequence.cpp — verifies anim::FrameSequence: frame counts and times at a fixed fps,
// the single-still degenerate cases, and sampling a Timeline track across the sequence. Headless.
#include "maz/anim/FrameSequence.hpp"

#include <cmath>
#include <cstdio>

using namespace maz::anim;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

int main() {
    // --- fixed cadence: [0, duration] inclusive ---
    {
        FrameSequence seq{30.0f, 2.0f};
        CHECK(seq.count() == 61, "2s @ 30fps -> 61 frames (inclusive endpoint)");
        CHECK(near(seq.timeAt(0), 0.0f) && near(seq.timeAt(30), 1.0f) && near(seq.timeAt(60), 2.0f),
              "frame times");
        const auto t = seq.times();
        CHECK(t.size() == 61 && near(t.front(), 0.0f) && near(t.back(), 2.0f), "times() span");
    }

    // --- degenerate: zero / negative duration or fps -> a single still frame ---
    {
        CHECK((FrameSequence{30.0f, 0.0f}.count() == 1), "zero duration -> 1 frame");
        CHECK((FrameSequence{0.0f, 5.0f}.count() == 1), "zero fps -> 1 frame");
        CHECK(near(FrameSequence{30.0f, 0.0f}.timeAt(0), 0.0f), "still frame at t=0");
    }

    // --- forTimeline + sampling a track ---
    {
        Timeline tl;
        tl.track("y").add(0.0f, 0.0f);
        tl.track("y").add(1.0f, 10.0f); // linear ramp 0..10 over 1 s
        FrameSequence seq = FrameSequence::forTimeline(tl, 10.0f);
        CHECK(near(seq.duration, 1.0f), "forTimeline uses clip length");
        CHECK(seq.count() == 11, "1s @ 10fps -> 11 frames");
        const auto vals = seq.sample(tl, "y");
        CHECK(vals.size() == 11 && near(vals.front(), 0.0f) && near(vals.back(), 10.0f) &&
                  near(vals[5], 5.0f),
              "sampled track ramps 0..10 with midpoint 5");
        const auto missing = seq.sample(tl, "nope");
        CHECK(missing.size() == 11 && near(missing[3], 0.0f), "unknown track samples to 0");
    }

    if (g_fail == 0) {
        std::printf("framesequence: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
