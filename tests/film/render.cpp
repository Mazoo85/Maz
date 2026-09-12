// tests/film/render.cpp — verifies the native frame renderer (apps/filmreel/Frame.hpp).
//
// The other film tests check that the ported pieces agree with the browser. This checks the thing
// those cannot: that a whole frame comes out, that it comes out the SAME every time, and that it
// comes out FAST ENOUGH.
//
// The speed check is here because its absence was the worst mistake of this sub-project. The whole
// point of rendering natively is to beat a realtime capture, and the first version ran at 0.3x
// realtime -- three times SLOWER than the browser it replaces -- while every correctness test passed.
// Correctness had six test files and performance had none, so the one property the work existed to
// deliver was the one that broke.
//
// The speed check is mostly written as a RATIO rather than a wall-clock budget, so that it means the
// same thing on a fast laptop and a slow CI runner: four times the pixels must not cost much more
// than four times the work. A generously-set absolute floor sits alongside it to catch the case
// where everything got slower by the same factor.
#include "Frame.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static double meanLuma(const Image& img) {
    double t = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const auto p = img.getPixel(x, y);
            t += static_cast<double>(p.r + p.g + p.b) / 3.0;
        }
    }
    return t / (static_cast<double>(img.width()) * static_cast<double>(img.height()));
}

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "tests/film/reel-fixture.json";
    const maz::film::Reel reel = maz::film::loadReel(path);
    if (!reel.valid) {
        std::printf("FAIL: could not load the reel (%s)\n", reel.error.c_str());
        return 1;
    }

    // A shot long enough to hold many frames, in the middle of the film so it is not a fade.
    const maz::film::Shot* longest = &reel.shots[1];
    for (const maz::film::Shot& s : reel.shots) {
        if (s.duration > longest->duration && s.start > 2.0 && s.end() < reel.duration - 2.0) {
            longest = &s;
        }
    }

    // --- 1. A frame comes out, at the size asked for. ---
    {
        const Image f = filmreel::drawFrame(reel, longest->start + 0.1, 320, 180);
        CHECK(f.width() == 320 && f.height() == 180, "a frame is the size it was asked for");
        CHECK(meanLuma(f) > 0.005, "a frame in the middle of the film is not black");
    }

    // --- 2. The film opens and closes in black. ---
    {
        CHECK(meanLuma(filmreel::drawFrame(reel, 0.0, 320, 180)) < 0.01, "the film opens in black");
        CHECK(meanLuma(filmreel::drawFrame(reel, reel.duration - 0.01, 320, 180)) < 0.01,
              "the film closes in black");
    }

    // --- 3. The same moment always gives the same frame -- including after drawing another shot. ---
    //
    // The renderer keeps a one-entry cache of the shot's unchanging wash. A cache is a correctness
    // hazard as much as a speed trick: if it ever returned a stale or shared buffer, two renders of
    // one moment would differ, and "one idea and one seed give one film" would quietly stop being
    // true. So the check deliberately renders something else in between.
    {
        const double t = longest->start + longest->duration * 0.4;
        const Image a = filmreel::drawFrame(reel, t, 320, 180);
        (void)filmreel::drawFrame(reel, reel.shots.back().start + 0.2, 320, 180);
        (void)filmreel::drawFrame(reel, 1.0, 256, 144);
        const Image b = filmreel::drawFrame(reel, t, 320, 180);
        CHECK(a.data() == b.data(), "the same moment gives the same frame, byte for byte");
    }

    // --- 4. Time moves the picture. ---
    {
        const Image a = filmreel::drawFrame(reel, longest->start + 0.05, 320, 180);
        const Image b = filmreel::drawFrame(reel, longest->end() - 0.05, 320, 180);
        CHECK(!(a.data() == b.data()), "the start and the end of a shot are not the same frame");
    }

    // --- 5. Speed. ---
    //
    // This exists because its absence was the worst mistake of this work. The whole point of
    // rendering natively is to beat a realtime capture, and one version ran at 0.3x realtime --
    // slower than the browser it replaces -- while every correctness test passed. Correctness had
    // seven test files and performance had none, so the one property the renderer exists for was
    // the one that broke.
    //
    // The earlier version of this check pinned a per-shot cache of the unchanging background. That
    // cache is gone: with the real sets in, the background is drawn in three parallax planes that
    // all move with the camera, so there is nothing constant within a shot left to keep. What
    // replaces it is two checks that do not depend on how fast the machine is:
    {
        const int n = 24;
        auto timeAt = [&](int w, int h) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; ++i) {
                const double t = longest->start + longest->duration *
                                                      (0.02 + 0.9 * static_cast<double>(i) /
                                                                  static_cast<double>(n));
                (void)filmreel::drawFrame(reel, t, w, h);
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        };

        // Warm anything cached at each size before timing it, so the comparison is of steady-state
        // drawing rather than of one-off setup.
        (void)filmreel::drawFrame(reel, longest->start + 0.01, 320, 180);
        const double small = timeAt(320, 180);
        (void)filmreel::drawFrame(reel, longest->start + 0.01, 640, 360);
        const double large = timeAt(640, 360);
        const double ratio = small > 0.0 ? large / small : 0.0;

        // (a) LINEARITY. Four times the pixels must not cost much more than four times the work. A
        // rasterizer that goes quadratic in the frame size -- testing every edge against every
        // scanline, recomputing a shape's geometry per pixel -- shows up here and nowhere else, and
        // it shows up the same way on any machine. Eight leaves room for the fixed per-frame costs
        // that do not scale with area at all.
        std::printf("  (4x the pixels cost %.1fx the time: %.1fms then %.1fms for %d frames)\n",
                    ratio, small, large, n);
        CHECK(ratio < 8.0,
              "four times the pixels cost no more than eight times the time -- the cost of a frame "
              "is roughly its area, not worse");

        // (b) A FLOOR, generously set. 640x360 is a quarter of 720p, and the renderer runs several
        // times faster than realtime there, so requiring merely a quarter of realtime leaves more
        // than an order of magnitude of headroom -- enough that a slow or loaded CI runner never
        // trips it, and little enough that losing a factor of thirty does.
        const double filmSeconds = static_cast<double>(n) / 24.0;
        const double speed = filmSeconds / (large / 1000.0);
        std::printf("  (640x360 renders at %.1fx realtime here)\n", speed);
        CHECK(speed > 0.25, "the renderer keeps up with at least a quarter of realtime at 640x360");
    }

    if (g_fail == 0) {
        std::printf("film render: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
