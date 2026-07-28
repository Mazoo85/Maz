// tests/platform/touch.cpp — verifies the multi-touch model added to platform::Input. Touch is the primary
// input on phones/tablets, so the engine must track several contacts at once with the same per-frame edge
// detection keys/buttons use. This is pure CPU (no window/SDL), driven by calling onTouch() directly the way
// Window::pumpEvents does from SDL finger events, so it proves here: press/move/release lifecycle, stable
// per-finger ids, motion-delta accumulation, the "pressed this frame" edge, released-this-frame reporting,
// and multi-touch capacity limits.
#include "maz/platform/Input.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

int main() {
    // --- 1. Single-finger press/move/release lifecycle with edge detection. ---
    {
        Input in;
        in.newFrame();
        CHECK(in.touchCount() == 0 && !in.anyTouch(), "no contacts before any touch");

        in.onTouch(7, 100.0f, 200.0f, 0.0f, 0.0f, TouchPhase::Down);
        CHECK(in.touchCount() == 1 && in.anyTouch(), "one contact after Down");
        CHECK(in.touchPressed(0), "contact reads as pressed on the frame it goes down");
        CHECK(in.touch(0).id == 7, "finger id preserved");
        CHECK(in.touch(0).x == 100.0f && in.touch(0).y == 200.0f, "down position recorded");

        // Next frame: the same finger is still down but is no longer "pressed this frame".
        in.newFrame();
        CHECK(in.touchCount() == 1, "contact persists across frames");
        CHECK(!in.touchPressed(0), "held contact is not pressed on later frames");
        CHECK(in.touch(0).dx == 0.0f && in.touch(0).dy == 0.0f, "motion delta cleared at frame start");

        // Move accumulates delta and updates the live position.
        in.onTouch(7, 110.0f, 205.0f, 10.0f, 5.0f, TouchPhase::Move);
        in.onTouch(7, 130.0f, 215.0f, 20.0f, 10.0f, TouchPhase::Move);
        CHECK(in.touch(0).x == 130.0f && in.touch(0).y == 215.0f, "move updates position to latest");
        CHECK(in.touch(0).dx == 30.0f && in.touch(0).dy == 15.0f, "move deltas accumulate within a frame");

        // Release moves the contact into the released list for exactly this frame.
        in.newFrame();
        in.onTouch(7, 130.0f, 215.0f, 0.0f, 0.0f, TouchPhase::Up);
        CHECK(in.touchCount() == 0 && !in.anyTouch(), "contact gone after Up");
        CHECK(in.releasedTouches().size() == 1, "one release reported this frame");
        CHECK(in.releasedTouches()[0].id == 7, "released contact carries its id");

        // The released list is one-frame: cleared on the next newFrame.
        in.newFrame();
        CHECK(in.releasedTouches().empty(), "released list cleared next frame");
    }

    // --- 2. Multi-touch: two independent fingers tracked by stable id, not index. ---
    {
        Input in;
        in.newFrame();
        in.onTouch(1, 50.0f, 50.0f, 0.0f, 0.0f, TouchPhase::Down);   // left stick finger
        in.onTouch(2, 700.0f, 400.0f, 0.0f, 0.0f, TouchPhase::Down); // right aim finger
        CHECK(in.touchCount() == 2, "two contacts tracked at once");
        CHECK(in.touchById(1) != nullptr && in.touchById(2) != nullptr, "both fingers found by id");
        CHECK(in.touchById(3) == nullptr, "unknown finger id returns null");

        // Move only finger 1; finger 2 must be untouched.
        in.newFrame();
        in.onTouch(1, 60.0f, 55.0f, 10.0f, 5.0f, TouchPhase::Move);
        const Touch* f1 = in.touchById(1);
        const Touch* f2 = in.touchById(2);
        CHECK(f1 && f1->x == 60.0f && f1->dx == 10.0f, "finger 1 moved");
        CHECK(f2 && f2->x == 700.0f && f2->dx == 0.0f, "finger 2 unaffected by finger 1's motion");

        // Lift finger 1; finger 2 remains and its id is stable (not reindexed to a released slot).
        in.newFrame();
        in.onTouch(1, 60.0f, 55.0f, 0.0f, 0.0f, TouchPhase::Up);
        CHECK(in.touchCount() == 1, "one contact remains after lifting the other");
        CHECK(in.touchById(2) != nullptr && in.touchById(1) == nullptr, "the right finger id survived");
        CHECK(in.releasedTouches().size() == 1 && in.releasedTouches()[0].id == 1, "correct finger released");
    }

    // --- 3. Capacity: contacts beyond kMaxTouches are ignored, not overflowing. ---
    {
        Input in;
        in.newFrame();
        for (int i = 0; i < Input::kMaxTouches + 5; ++i) {
            in.onTouch(100 + i, static_cast<float>(i), 0.0f, 0.0f, 0.0f, TouchPhase::Down);
        }
        CHECK(in.touchCount() == Input::kMaxTouches, "contact count capped at kMaxTouches");
        // The first kMaxTouches ids were kept; the surplus was dropped.
        CHECK(in.touchById(100) != nullptr, "earliest contact retained");
        CHECK(in.touchById(100 + Input::kMaxTouches + 4) == nullptr, "surplus contact dropped");
    }

    // --- 4. Out-of-range index access is inert (returns a neutral Touch, no crash). ---
    {
        Input in;
        in.newFrame();
        const Touch& none = in.touch(0); // nothing down
        CHECK(none.id == -1, "out-of-range touch() yields an inert contact");
        CHECK(!in.touchPressed(-1) && !in.touchPressed(99), "out-of-range touchPressed is false");
    }

    if (g_fail == 0) {
        std::printf("platform_touch: OK — lifecycle, edges, multi-touch by id, deltas, capacity.\n");
        return 0;
    }
    std::printf("platform_touch: %d failure(s).\n", g_fail);
    return 1;
}
