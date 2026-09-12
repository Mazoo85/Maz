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
// It is written as a RATIO rather than a wall-clock budget, so it means the same thing on a fast
// laptop and a slow CI runner: the expensive full-frame washes do not change within a shot, so
// forty frames of one shot must not cost anything like forty times one frame. If that cache is ever
// lost, this fails everywhere rather than only on slow machines.
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

    // --- 5. Speed: the shot's wash is built once, not once per frame. ---
    {
        const int w = 640, h = 360;
        // Warm one frame of the shot, so the cache holds it, then time a fresh cold one.
        (void)filmreel::drawFrame(reel, reel.shots.back().start + 0.2, w, h);

        const auto t0 = std::chrono::steady_clock::now();
        (void)filmreel::drawFrame(reel, longest->start + 0.01, w, h); // cold: builds the wash
        const auto t1 = std::chrono::steady_clock::now();

        const int n = 40;
        for (int i = 0; i < n; ++i) {
            const double t = longest->start + longest->duration * (0.02 + 0.9 * static_cast<double>(i) /
                                                                              static_cast<double>(n));
            (void)filmreel::drawFrame(reel, t, w, h);
        }
        const auto t2 = std::chrono::steady_clock::now();

        const double cold = std::chrono::duration<double, std::milli>(t1 - t0).count();
        const double warm = std::chrono::duration<double, std::milli>(t2 - t1).count();
        const double ratio = cold > 0.0 ? warm / cold : 0.0;

        // The threshold is measured, not guessed. With the cache this ratio is about 12; with the
        // cache deliberately disabled it is about 41, because every frame rebuilds the wash. 22 sits
        // between the two with the same factor of margin either way, so it does not fire on a loaded
        // machine and does not sleep through the regression it exists for.
        std::printf("  (%d further frames cost %.1fx the first: %.1fms cold, %.1fms for %d)\n", n,
                    ratio, cold, warm, n);
        CHECK(ratio < 22.0,
              "forty frames of one shot cost far less than forty times the first -- the shot's "
              "unchanging wash is built once, not rebuilt every frame");
    }

    if (g_fail == 0) {
        std::printf("film render: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
