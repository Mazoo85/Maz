// tests/game/inputsequencer.cpp — verifies the fighting-game motion-input detector (game::InputSequencer):
// register moves as token sequences with a timing window, feed inputs, fire the move when the recent inputs
// end-match a sequence within its window. Ground truths: an in-window sequence fires; the same sequence
// stretched past its window does not; the longest match wins over a shorter one sharing the tail; a match
// consumes the buffer; wrong order never matches; stale inputs age out; reset clears. Deterministic (clock
// driven by tick).
#include "maz/game/InputSequencer.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::InputSequencer;

// Token vocabulary for the tests: directions 1..3, punch 10.
int main() {
    // --- 1. A quarter-circle-forward + punch fires within its window. ---
    {
        InputSequencer s;
        const int qcfP = s.registerMove({1, 2, 3, 10}, 0.6f); // down, down-fwd, fwd, punch
        CHECK(qcfP == 0 && s.moveCount() == 1, "registerMove returns id 0");
        CHECK(s.feed(1) == -1 && s.feed(2) == -1 && s.feed(3) == -1, "partial input does not fire");
        CHECK(s.feed(10) == qcfP, "completed motion fires the move");
        CHECK(s.bufferSize() == 0, "a match consumes the buffer");
    }

    // --- 2. The same motion stretched past the window does not fire. ---
    {
        InputSequencer s;
        s.registerMove({1, 2, 3}, 0.5f); // must complete within 0.5s
        s.feed(1);
        s.tick(0.3f);
        s.feed(2);
        s.tick(0.3f); // first token is now 0.6s old (> 0.5 window)
        CHECK(s.feed(3) == -1, "sequence too slow: window exceeded, no fire");
    }

    // --- 3. In-window across ticks fires. ---
    {
        InputSequencer s;
        const int m = s.registerMove({1, 2, 3}, 0.5f);
        s.feed(1);
        s.tick(0.1f);
        s.feed(2);
        s.tick(0.1f); // span so far 0.2s
        CHECK(s.feed(3) == m, "sequence within window fires");
    }

    // --- 4. Longest match wins over a shorter one sharing the tail. ---
    {
        InputSequencer s;
        const int punch = s.registerMove({10}, 1.0f);        // id 0: bare punch
        const int dpPunch = s.registerMove({5, 10}, 0.5f);   // id 1: down + punch
        (void)punch;
        s.feed(5);
        CHECK(s.feed(10) == dpPunch, "longer sequence beats the shorter tail match");
    }

    // --- 5. A bare token still fires its single-token move when nothing longer matches. ---
    {
        InputSequencer s;
        const int punch = s.registerMove({10}, 1.0f);
        s.registerMove({5, 10}, 0.5f);
        CHECK(s.feed(10) == punch, "single-token move fires when no longer match applies");
    }

    // --- 6. Wrong order never matches. ---
    {
        InputSequencer s;
        s.registerMove({1, 2, 3}, 1.0f);
        CHECK(s.feed(3) == -1 && s.feed(2) == -1 && s.feed(1) == -1, "reversed order does not match");
    }

    // --- 7. Stale inputs age out so an old direction cannot complete a motion later. ---
    {
        InputSequencer s;
        const int m = s.registerMove({1, 2, 3}, 0.5f);
        s.feed(1);
        s.tick(1.0f); // the '1' is now far older than any window -> pruned
        s.feed(2);
        s.feed(3);
        CHECK(s.feed(3) != m, "expired lead input cannot complete the motion");
        // But a fresh, timely motion still works.
        InputSequencer s2;
        const int m2 = s2.registerMove({1, 2, 3}, 0.5f);
        s2.feed(1); s2.feed(2);
        CHECK(s2.feed(3) == m2, "fresh motion still fires");
    }

    // --- 8. Consuming prevents immediate re-trigger; re-entering the motion fires again. ---
    {
        InputSequencer s;
        const int m = s.registerMove({1, 2}, 1.0f);
        s.feed(1);
        CHECK(s.feed(2) == m, "first fire");
        CHECK(s.feed(2) == -1, "buffer consumed: a lone repeat does not refire");
        s.feed(1);
        CHECK(s.feed(2) == m, "re-entering the motion fires again");
    }

    // --- 9. reset clears the buffer. ---
    {
        InputSequencer s;
        const int m = s.registerMove({1, 2}, 1.0f);
        s.feed(1);
        s.reset();
        CHECK(s.bufferSize() == 0, "reset clears buffer");
        CHECK(s.feed(2) == -1, "after reset the half-motion is gone");
        s.feed(1);
        CHECK(s.feed(2) == m, "and a full motion after reset fires");
    }

    // --- 10. Invalid registrations are rejected. ---
    {
        InputSequencer s;
        CHECK(s.registerMove({}, 1.0f) == -1, "empty sequence rejected");
        CHECK(s.registerMove({1}, 0.0f) == -1, "non-positive window rejected");
        CHECK(s.moveCount() == 0, "no invalid moves registered");
    }

    if (g_fail == 0) std::printf("input sequencer: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
