// Unit tests for maz::ai::StateMachine — the event-driven finite state machine.
// Exercises basic transitions, no-op on unmatched events, callback ordering
// (onExit old -> onEnter new), self-transitions (both callbacks fire), multiple
// events from one state, update ticking only the current state, reset+restart
// re-entering the initial state, and callback-less states. Pure C++, no GPU/display.

#include "maz/ai/StateMachine.hpp"

#include <cstdio>
#include <vector>
#include <string>

using namespace maz::ai;
using maz::core::StringId;
using maz::core::operator""_sid;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Returns true if needle appears as a contiguous ordered subsequence within hay.
bool containsSubseq(const std::vector<std::string>& hay, const std::vector<std::string>& needle) {
    if (needle.size() > hay.size()) { return false; }
    for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (hay[i + j] != needle[j]) { match = false; break; }
        }
        if (match) { return true; }
    }
    return false;
}

bool contains(const std::vector<std::string>& hay, const std::string& s) {
    for (const std::string& e : hay) { if (e == s) { return true; } }
    return false;
}

} // namespace

int main() {
    // --- BASIC TRANSITION ----------------------------------------------------
    {
        StateMachine sm;
        sm.addState("A"_sid);
        sm.addState("B"_sid);
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        check(sm.current() == "A"_sid, "starts in initial state A");
        check(sm.hasState("A"_sid) && sm.hasState("B"_sid) && !sm.hasState("Z"_sid), "hasState reports registered states");
        check(sm.stateCount() == 2, "stateCount == 2");
        check(sm.fire("go"_sid) == true, "fire('go') returns true");
        check(sm.current() == "B"_sid, "transitioned to B");
    }

    // --- NO TRANSITION -------------------------------------------------------
    {
        StateMachine sm;
        sm.addState("A"_sid);
        sm.addState("B"_sid);
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        sm.fire("go"_sid);  // now in B
        check(sm.fire("nope"_sid) == false, "fire of unmapped event returns false");
        check(sm.current() == "B"_sid, "state unchanged after unmapped event");
    }

    // --- CALLBACK ORDER ------------------------------------------------------
    {
        std::vector<std::string> log;
        StateMachine sm;
        sm.addState("A"_sid,
                    [&]{ log.push_back("enterA"); },
                    [&](float){ log.push_back("updA"); },
                    [&]{ log.push_back("exitA"); });
        sm.addState("B"_sid,
                    [&]{ log.push_back("enterB"); },
                    [&](float){ log.push_back("updB"); },
                    [&]{ log.push_back("exitB"); });
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        check(contains(log, "enterA"), "start fires onEnter of A");
        sm.update(0.016f);
        check(contains(log, "updA"), "update ticks A.onUpdate");
        sm.fire("go"_sid);
        check(containsSubseq(log, {"exitA", "enterB"}), "fire fires exitA then enterB, adjacent+ordered");
        sm.update(0.016f);
        check(log.back() == "updB", "update after transition ticks B.onUpdate");
    }

    // --- SELF-TRANSITION -----------------------------------------------------
    {
        std::vector<std::string> log;
        StateMachine sm;
        sm.addState("A"_sid,
                    [&]{ log.push_back("enterA"); },
                    {},
                    [&]{ log.push_back("exitA"); });
        sm.addTransition("A"_sid, "loop"_sid, "A"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        log.clear();  // drop the start() enterA
        check(sm.fire("loop"_sid) == true, "self-transition returns true");
        check(containsSubseq(log, {"exitA", "enterA"}), "self-transition fires exitA then enterA");
        check(sm.current() == "A"_sid, "self-transition stays in A");
    }

    // --- MULTIPLE EVENTS FROM ONE STATE --------------------------------------
    {
        StateMachine sm;
        sm.addState("A"_sid);
        sm.addState("B"_sid);
        sm.addState("C"_sid);
        sm.addTransition("A"_sid, "x"_sid, "B"_sid);
        sm.addTransition("A"_sid, "y"_sid, "C"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        sm.fire("y"_sid);
        check(sm.current() == "C"_sid, "fire('y') routes A->C, not A->B");
    }

    // --- UPDATE TICKS ONLY CURRENT -------------------------------------------
    {
        std::vector<std::string> log;
        StateMachine sm;
        sm.addState("A"_sid, {}, [&](float){ log.push_back("updA"); }, {});
        sm.addState("B"_sid, {}, [&](float){ log.push_back("updB"); }, {});
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        sm.fire("go"_sid);
        log.clear();
        sm.update(0.016f);
        check(log.size() == 1 && log[0] == "updB", "update after transition ticks only B, never A");
    }

    // --- RESET + RESTART -----------------------------------------------------
    {
        std::vector<std::string> log;
        StateMachine sm;
        sm.addState("A"_sid, [&]{ log.push_back("enterA"); }, {}, [&]{ log.push_back("exitA"); });
        sm.addState("B"_sid, [&]{ log.push_back("enterB"); }, {}, [&]{ log.push_back("exitB"); });
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();
        sm.fire("go"_sid);  // now in B
        log.clear();
        sm.reset();
        check(sm.started() == false, "reset -> not started");
        check(!contains(log, "exitB"), "reset does NOT fire current state's onExit");
        sm.start();
        check(sm.current() == "A"_sid, "restart returns to initial state A");
        check(contains(log, "enterA"), "restart re-fires initial onEnter");
    }

    // --- STATES WITHOUT CALLBACKS --------------------------------------------
    {
        StateMachine sm;
        sm.addState("A"_sid);  // all-empty callbacks
        sm.addState("B"_sid);
        sm.addTransition("A"_sid, "go"_sid, "B"_sid);
        sm.setInitial("A"_sid);
        sm.start();          // empty onEnter not called
        sm.update(0.016f);   // empty onUpdate not called
        sm.fire("go"_sid);   // empty onExit/onEnter not called
        check(sm.current() == "B"_sid, "callback-less states start/update/fire without crashing");
    }

    // --- DUPLICATE-STATE add and FIRE/UPDATE-before-start are MAZ_ASSERT
    //     programmer-error guards (compiled out in release), not exercised here.

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
