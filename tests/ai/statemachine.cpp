// Unit tests for maz::ai::StateMachine — the event-driven finite state machine.
//
// The interesting part of an FSM is not that it changes state; it is WHEN the
// callbacks run relative to that change, because gameplay code reads current()
// from inside them. StateMachine's header promises a specific order — onExit is
// called while current() is still the OLD state, and onEnter after current() is
// already the NEW one — so these tests assert that from inside the callbacks
// themselves rather than only observing the end state, which would pass even if
// the order were inverted. The same goes for the two documented edge cases: a
// self-transition must fire BOTH onExit and onEnter, and reset() must NOT fire
// onExit. Callbacks append to a trace string, so an assertion failure shows the
// whole sequence that actually ran. Pure C++, no GPU/display.

#include "maz/ai/StateMachine.hpp"

#include <cstdio>
#include <string>
#include <string_view>

using maz::ai::StateMachine;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

void checkEq(const std::string& got, const char* want, const char* msg) {
    const bool ok = got == want;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", msg);
    if (!ok) {
        std::printf("       want \"%s\"\n       got  \"%s\"\n", want, got.c_str());
        ++g_failures;
    }
}

// This engine's StringId is a handle interned at runtime by core::StringTable,
// so every id in a test must come from one table for equal text to compare equal.
maz::core::StringTable g_strings;
maz::core::StringId sid(std::string_view s) { return g_strings.intern(s); }

} // namespace

int main() {
    const auto idle = sid("idle");
    const auto run = sid("run");
    const auto hurt = sid("hurt");
    const auto go = sid("go");
    const auto hit = sid("hit");
    const auto nope = sid("nope");

    // ---- start() enters the initial state and fires only its onEnter ----------
    {
        std::string trace;
        StateMachine sm;
        sm.addState(idle, [&] { trace += "enter:idle;"; }, {}, [&] { trace += "exit:idle;"; });
        sm.addState(run, [&] { trace += "enter:run;"; }, {}, [&] { trace += "exit:run;"; });

        check(!sm.started(), "a fresh machine is not started");
        check(!sm.current().valid(), "current() is invalid before start()");
        check(sm.stateCount() == 2, "stateCount() counts registered states");
        check(sm.hasState(idle) && !sm.hasState(sid("absent")), "hasState() distinguishes registered states");

        sm.setInitial(idle);
        check(trace.empty(), "setInitial() does not enter the state");

        sm.start();
        check(sm.started(), "started() is true after start()");
        check(sm.current() == idle, "start() makes the initial state current");
        checkEq(trace, "enter:idle;", "start() fires exactly the initial state's onEnter");
    }

    // ---- fire(): onExit sees the OLD state, onEnter sees the NEW one ----------
    // The header guarantees this ordering; assert it from inside the callbacks.
    {
        std::string trace;
        StateMachine sm;
        bool exitSawOld = false;
        bool enterSawNew = false;

        sm.addState(idle, {}, {}, [&] {
            trace += "exit:idle;";
            exitSawOld = (sm.current() == idle);
        });
        sm.addState(run, [&] {
            trace += "enter:run;";
            enterSawNew = (sm.current() == run);
        });
        sm.addTransition(idle, go, run);
        sm.setInitial(idle);
        sm.start();

        const bool fired = sm.fire(go);
        check(fired, "fire() returns true when a transition exists");
        check(sm.current() == run, "fire() moves to the transition target");
        checkEq(trace, "exit:idle;enter:run;", "fire() runs onExit before onEnter");
        check(exitSawOld, "onExit observes current() as the OLD state");
        check(enterSawNew, "onEnter observes current() as the NEW state");
    }

    // ---- an unmatched event is a no-op ---------------------------------------
    {
        std::string trace;
        StateMachine sm;
        sm.addState(idle, [&] { trace += "enter:idle;"; }, {}, [&] { trace += "exit:idle;"; });
        sm.addState(run);
        sm.addTransition(idle, go, run);
        sm.setInitial(idle);
        sm.start();
        trace.clear();

        // No transition registered for this event...
        check(!sm.fire(nope), "fire() returns false for an event with no transition");
        check(sm.current() == idle, "a rejected event leaves the state unchanged");
        checkEq(trace, "", "a rejected event fires no callbacks");

        // ...nor for an event that exists, but not out of the current state.
        sm.fire(go); // now in `run`, which has no outgoing transitions at all
        trace.clear();
        check(!sm.fire(go), "fire() returns false when the current state has no transitions");
        check(sm.current() == run, "that rejection also leaves the state unchanged");
        checkEq(trace, "", "and fires no callbacks");
    }

    // ---- a self-transition fires BOTH onExit and onEnter ----------------------
    {
        std::string trace;
        StateMachine sm;
        sm.addState(hurt, [&] { trace += "enter:hurt;"; }, {}, [&] { trace += "exit:hurt;"; });
        sm.addTransition(hurt, hit, hurt); // to == from
        sm.setInitial(hurt);
        sm.start();
        trace.clear();

        check(sm.fire(hit), "a self-transition is still a transition");
        check(sm.current() == hurt, "a self-transition stays in the same state");
        checkEq(trace, "exit:hurt;enter:hurt;", "a self-transition fires both onExit and onEnter");
    }

    // ---- update() ticks only the current state, with the dt it was given ------
    {
        float idleDt = 0.0f;
        float runDt = 0.0f;
        int idleTicks = 0;
        int runTicks = 0;
        StateMachine sm;
        sm.addState(idle, {}, [&](float dt) { idleDt += dt; ++idleTicks; });
        sm.addState(run, {}, [&](float dt) { runDt += dt; ++runTicks; });
        sm.addTransition(idle, go, run);
        sm.setInitial(idle);
        sm.start();

        sm.update(0.25f);
        sm.update(0.5f);
        check(idleTicks == 2 && runTicks == 0, "update() ticks only the current state");
        check(idleDt == 0.75f, "update() forwards dt unchanged (0.25 + 0.5)");

        sm.fire(go);
        sm.update(0.125f);
        check(runTicks == 1 && idleTicks == 2, "after a transition, update() ticks the new state");
        check(runDt == 0.125f, "the new state receives its own dt");
    }

    // ---- a state with no callbacks is safe to drive ---------------------------
    {
        StateMachine sm;
        sm.addState(idle); // all three callbacks empty
        sm.addState(run);
        sm.addTransition(idle, go, run);
        sm.setInitial(idle);
        sm.start();
        sm.update(0.1f);
        check(sm.fire(go), "a callback-free state transitions");
        check(sm.current() == run, "and lands in the target");
    }

    // ---- a duplicate (from,event) overwrites: last write wins -----------------
    {
        StateMachine sm;
        sm.addState(idle);
        sm.addState(run);
        sm.addState(hurt);
        sm.addTransition(idle, go, run);
        sm.addTransition(idle, go, hurt); // same (from,event), new target
        sm.setInitial(idle);
        sm.start();

        check(sm.fire(go), "the overwritten transition still fires");
        check(sm.current() == hurt, "a duplicate (from,event) takes the LAST target");
    }

    // ---- reset() is abrupt: no onExit, and start() re-enters ------------------
    {
        std::string trace;
        StateMachine sm;
        sm.addState(idle, [&] { trace += "enter:idle;"; }, {}, [&] { trace += "exit:idle;"; });
        sm.addState(run, [&] { trace += "enter:run;"; }, {}, [&] { trace += "exit:run;"; });
        sm.addTransition(idle, go, run);
        sm.setInitial(idle);
        sm.start();
        sm.fire(go); // now in `run`
        trace.clear();

        sm.reset();
        checkEq(trace, "", "reset() does NOT fire the current state's onExit");
        check(!sm.started(), "reset() returns the machine to un-started");
        check(!sm.current().valid(), "reset() clears current()");
        check(sm.stateCount() == 2, "reset() keeps the registered states");

        sm.start();
        check(sm.current() == idle, "start() after reset() re-enters the INITIAL state, not the last one");
        checkEq(trace, "enter:idle;", "and fires the initial state's onEnter");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
