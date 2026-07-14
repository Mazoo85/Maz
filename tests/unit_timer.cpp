// Unit tests for maz::core::TimerManager — the update(dt)-advanced callback
// scheduler (Godot SceneTreeTimer analog). Exercises one-shot fire-at-delay,
// exact/over-delay firing, repeating per-interval firing, catch-up + remainder
// carry, cancel-before-fire, the subtle re-entrancy cases (cancel a timer from
// inside another callback the same tick, schedule a timer from inside a callback
// being deferred to the next tick), registration-order firing, and clear. Pure
// C++, no GPU/display.

#include "maz/core/Timer.hpp"

#include <cstdio>
#include <vector>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- ONE-SHOT FIRES ONCE AT DELAY ----------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        tm.after(1.0f, [&] { ++fired; });
        tm.update(0.5f);
        check(fired == 0, "one-shot not fired before delay elapses");
        tm.update(0.5f);
        check(fired == 1, "one-shot fires when remaining hits 0");
        tm.update(1.0f);
        check(fired == 1, "one-shot does not re-fire");
        check(tm.activeCount() == 0, "one-shot swept after firing");
    }

    // --- ONE-SHOT EXACT/OVER DELAY -------------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        tm.after(1.0f, [&] { ++fired; });
        tm.update(2.0f);
        check(fired == 1, "one-shot fires once when dt overshoots the delay");
        check(tm.activeCount() == 0, "one-shot removed after overshoot fire");
    }

    // --- REPEATING FIRES EACH INTERVAL ---------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        tm.every(1.0f, [&] { ++fired; });
        tm.update(1.0f);
        check(fired == 1, "repeating fires on first interval");
        tm.update(1.0f);
        check(fired == 2, "repeating fires on second interval");
        tm.update(1.0f);
        check(fired == 3, "repeating fires on third interval");
        check(tm.activeCount() == 1, "repeating stays active");
    }

    // --- REPEATING CATCH-UP ---------------------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        tm.every(1.0f, [&] { ++fired; });
        tm.update(3.5f);
        check(fired == 3, "catch-up fires 3 times over a 3.5s dt");
        tm.update(0.5f);
        check(fired == 4, "carried 0.5s remainder fires again after another 0.5s");
    }

    // --- CANCEL BEFORE FIRE ---------------------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        auto id = tm.after(1.0f, [&] { ++fired; });
        check(tm.cancel(id) == true, "cancel of a live timer returns true");
        tm.update(2.0f);
        check(fired == 0, "cancelled timer never fires");
        check(tm.activeCount() == 0, "cancelled timer swept");
        check(tm.cancel(id) == false, "cancel of an already-inactive timer returns false");
        check(tm.cancel(TimerId{}) == false, "cancel of the null id returns false");
    }

    // --- CANCEL DURING CALLBACK ----------------------------------------------
    {
        TimerManager tm;
        int bFired = 0;
        // B is a one-shot due this tick; A is registered FIRST so its callback
        // runs before B is reached and cancels B before it can fire.
        TimerId bId;
        tm.every(1.0f, [&] { tm.cancel(bId); });  // A
        bId = tm.after(1.0f, [&] { ++bFired; });  // B
        tm.update(1.0f);
        check(bFired == 0, "timer cancelled from inside another callback does not fire this tick");
    }

    // --- SCHEDULE DURING CALLBACK IS DEFERRED --------------------------------
    {
        TimerManager tm;
        int cb2Fired = 0;
        tm.after(1.0f, [&] { tm.after(0.0f, [&] { ++cb2Fired; }); });
        tm.update(1.0f);
        check(cb2Fired == 0, "timer scheduled inside a callback does not fire the same tick");
        check(tm.activeCount() == 1, "deferred timer is live after the update");
        tm.update(0.0f);
        check(cb2Fired == 1, "deferred 0-delay one-shot fires on the next tick");
    }

    // --- REPEATING SELF-CANCEL MID-CATCH-UP ----------------------------------
    {
        // Pins the `active` re-check inside the repeating catch-up while loop: a
        // repeating timer that cancels ITSELF from its own callback stops firing
        // immediately, even when a single large dt spans multiple intervals.
        // Would FAIL (fired == 3) if the catch-up loop did not re-check active
        // before each repeat fire — the 3.5s dt spans 3 whole 1.0s intervals.
        TimerManager tm;
        int fired = 0;
        TimerId id;
        id = tm.every(1.0f, [&] { ++fired; tm.cancel(id); });  // cancels ITSELF
        tm.update(3.5f);  // dt spans 3 intervals — only the first fire should land
        check(fired == 1, "self-cancelling repeat fires once, then stops the catch-up loop");
        check(tm.activeCount() == 0, "self-cancelled repeat swept after firing");
        tm.update(5.0f);
        check(fired == 1, "self-cancelled repeat never fires again");
    }

    // --- SCHEDULE-THEN-CANCEL WITHIN THE SAME TICK ---------------------------
    {
        // Pins that cancel() scans m_pending: a timer scheduled during an update
        // lives in m_pending until the post-update merge, so cancelling it in the
        // same tick must find it there and deactivate it — it then never fires and
        // is swept by the merge-then-sweep. Would FAIL if cancel() only searched
        // m_timers (the inner timer would survive and fire on a later tick).
        TimerManager tm;
        int outer = 0, inner = 0;
        TimerId innerId;
        tm.after(1.0f, [&] {
            ++outer;
            innerId = tm.after(0.0f, [&] { ++inner; });  // deferred to m_pending
            bool ok = tm.cancel(innerId);                // must find it in m_pending
            check(ok, "cancel of same-tick-scheduled timer returns true");
        });
        tm.update(1.0f);  // fires outer; inner is scheduled-then-cancelled this tick
        check(outer == 1, "outer one-shot fires once");
        check(inner == 0, "same-tick-scheduled-then-cancelled inner never fires");
        tm.update(0.0f);
        tm.update(1.0f);
        check(inner == 0, "cancelled pending timer stays swept and never fires");
        check(tm.activeCount() == 0, "no timers remain after schedule-then-cancel");
    }

    // --- MULTIPLE TIMERS REGISTRATION ORDER ----------------------------------
    {
        TimerManager tm;
        std::vector<int> order;
        tm.after(1.0f, [&] { order.push_back(1); });
        tm.after(1.0f, [&] { order.push_back(2); });
        tm.update(1.0f);
        check(order.size() == 2 && order[0] == 1 && order[1] == 2,
              "timers fire in registration order within a tick");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        TimerManager tm;
        int fired = 0;
        tm.after(1.0f, [&] { ++fired; });
        tm.every(1.0f, [&] { ++fired; });
        tm.clear();
        check(tm.activeCount() == 0, "clear removes all timers");
        check(tm.empty() == true, "empty() true after clear");
        tm.update(2.0f);
        check(fired == 0, "no timers fire after clear");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
