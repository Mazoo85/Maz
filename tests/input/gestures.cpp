// tests/input/gestures.cpp — verifies the touch gesture recognizer (input::GestureDetector) that sits above
// platform::Input's touch points. Pure CPU, deterministic (timing comes from the dt passed to update, not a
// wall clock): a platform::Input is driven with synthetic finger events exactly as Window::pumpEvents feeds
// it, and the detector is stepped once per frame. Ground truths: a quick still down->up is a tap; two taps
// close in time+space are a double-tap; a still contact held past the threshold is a long-press; a fast far
// drag is a directional swipe with a velocity; a slow far drag is neither; and two fingers give a pinch scale
// / rotation / pan.
#include "maz/input/GestureDetector.hpp"
#include "maz/platform/Input.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)
static bool approx(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }

using maz::platform::Input;
using maz::platform::TouchPhase;
using maz::input::GestureDetector;
using maz::input::SwipeDir;

// One frame: newFrame (as the window pump does), optionally feed one finger event, then step the detector.
static void frame(Input& in, GestureDetector& g, float dt) {
    in.newFrame();
    g.update(in, dt);
}
static void down(Input& in, int64_t id, float x, float y) { in.onTouch(id, x, y, 0, 0, TouchPhase::Down); }
static void move(Input& in, int64_t id, float x, float y, float dx, float dy) {
    in.onTouch(id, x, y, dx, dy, TouchPhase::Move);
}
static void up(Input& in, int64_t id, float x, float y) { in.onTouch(id, x, y, 0, 0, TouchPhase::Up); }

int main() {
    const float dt = 1.0f / 60.0f;

    // --- 1. Tap: quick still down then up. ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 200, 300); g.update(in, dt);
        CHECK(!g.tapped(), "no tap on the down frame");
        in.newFrame(); up(in, 1, 200, 300); g.update(in, dt);
        CHECK(g.tapped(), "tap fires on the release frame");
        CHECK(approx(g.tapPos().x, 200) && approx(g.tapPos().y, 300), "tap position is the release point");
        CHECK(!g.swiped() && !g.longPressed(), "a tap is not a swipe or long-press");
        // The edge is one-frame.
        frame(in, g, dt);
        CHECK(!g.tapped(), "tap edge clears next frame");
    }

    // --- 2. Double-tap: two taps close in time and space. ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 100, 100); g.update(in, dt);
        in.newFrame(); up(in, 1, 100, 100); g.update(in, dt);
        CHECK(g.tapped() && !g.doubleTapped(), "first release is a single tap");
        in.newFrame(); down(in, 2, 105, 98); g.update(in, dt);
        in.newFrame(); up(in, 2, 105, 98); g.update(in, dt);
        CHECK(g.doubleTapped(), "second nearby tap within the gap is a double-tap");
    }

    // --- 3. Two taps far apart in TIME are NOT a double-tap. ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 100, 100); g.update(in, dt);
        in.newFrame(); up(in, 1, 100, 100); g.update(in, dt);
        // Let a long gap pass (well beyond doubleTapMaxGap = 0.3s).
        for (int i = 0; i < 40; ++i) frame(in, g, dt); // ~0.66s idle
        in.newFrame(); down(in, 2, 100, 100); g.update(in, dt);
        in.newFrame(); up(in, 2, 100, 100); g.update(in, dt);
        CHECK(g.tapped() && !g.doubleTapped(), "second tap after the gap is a fresh single tap");
    }

    // --- 4. Long-press: a still contact held past the threshold (fires once, no tap on release). ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 400, 400); g.update(in, 0.1f); // t=0.1
        bool firedOnce = false;
        for (int i = 0; i < 6; ++i) {           // hold; longPressTime = 0.5s
            in.newFrame(); g.update(in, 0.1f);  // contact persists across frames without re-feeding
            if (g.longPressed()) firedOnce = !firedOnce ? true : (g_fail++, false); // exactly once
        }
        CHECK(firedOnce, "long-press fires exactly once while held still");
        CHECK(approx(g.longPressPos().x, 400) && approx(g.longPressPos().y, 400), "long-press position");
        in.newFrame(); up(in, 1, 400, 400); g.update(in, 0.1f);
        CHECK(!g.tapped(), "releasing after a long-press does not also emit a tap");
    }

    // --- 5. Swipe: a fast far drag right, with a velocity and direction. ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 100, 100); g.update(in, dt);
        in.newFrame(); up(in, 1, 300, 105); g.update(in, dt); // +200x in one frame => fast
        CHECK(g.swiped(), "fast far drag is a swipe");
        CHECK(g.swipeDir() == SwipeDir::Right, "dominant +x is a rightward swipe");
        CHECK(g.swipeVelocity().x > 0.0f, "swipe velocity points right");
        CHECK(!g.tapped(), "a swipe is not a tap");
    }

    // --- 6. Swipe up (screen Y grows downward, so a smaller y is 'up'). ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 200, 400); g.update(in, dt);
        in.newFrame(); up(in, 1, 205, 200); g.update(in, dt);
        CHECK(g.swiped() && g.swipeDir() == SwipeDir::Up, "dominant -y is an upward swipe");
    }

    // --- 7. Slow far drag is neither a tap nor a swipe (too slow for a fling). ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 100, 100); g.update(in, dt);
        // Creep to (300,100) over ~0.5s (> swipeMaxTime), moving a bit each frame.
        float x = 100;
        for (int i = 0; i < 30; ++i) {
            x += 200.0f / 30.0f;
            in.newFrame(); move(in, 1, x, 100, 200.0f / 30.0f, 0); g.update(in, dt);
        }
        in.newFrame(); up(in, 1, 300, 100); g.update(in, dt);
        CHECK(!g.swiped() && !g.tapped(), "a slow far drag is neither swipe nor tap");
    }

    // --- 8. Pinch: two fingers spreading apart give scale > 1, plus a centroid pan. ---
    {
        Input in; GestureDetector g;
        in.newFrame(); down(in, 1, 100, 100); g.update(in, dt);
        CHECK(!g.pinchActive(), "one finger is not a pinch");
        in.newFrame(); down(in, 2, 200, 100); g.update(in, dt); // second finger down -> pinch begins
        CHECK(g.pinchActive(), "two fingers start a pinch");
        CHECK(approx(g.pinchScale(), 1.0f), "pinch scale starts at 1");
        // Spread finger 2 from x=200 to x=300: distance 100 -> 200, scale 2x; centroid 150 -> 200, pan +50x.
        in.newFrame(); move(in, 2, 300, 100, 100, 0); g.update(in, dt);
        CHECK(approx(g.pinchScale(), 2.0f, 1e-2f), "spreading doubles the pinch scale");
        CHECK(approx(g.pinchPan().x, 50.0f, 1e-2f), "centroid pans by half the finger's move");
        // Lifting a finger ends the pinch.
        in.newFrame(); up(in, 2, 300, 100); g.update(in, dt);
        CHECK(!g.pinchActive(), "pinch ends when a finger lifts");
    }

    if (g_fail == 0) {
        std::printf("input_gestures: OK — tap, double-tap, long-press, swipe (dir+vel), slow-drag, pinch.\n");
        return 0;
    }
    std::printf("input_gestures: %d failure(s).\n", g_fail);
    return 1;
}
