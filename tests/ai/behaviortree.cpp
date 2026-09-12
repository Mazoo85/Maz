// Unit tests for maz::ai::BehaviorTree — the classic game-AI behavior tree.
// Exercises Sequence (AND, short-circuit on Failure), Selector (OR, first success
// wins), Inverter, the Running-resume semantics of composites (a Running child
// suspends the composite there and it resumes at that child next tick without
// re-ticking earlier children), the Repeater decorator, reset() restarting from
// the top, and the BehaviorTree wrapper delegating to a nested root. Pure C++,
// no GPU/display.

#include "maz/ai/BehaviorTree.hpp"

#include <cstdio>
#include <vector>
#include <string>

using namespace maz::ai;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Counts how many times a value appears in a log.
std::size_t count(const std::vector<std::string>& hay, const std::string& s) {
    std::size_t n = 0;
    for (const std::string& e : hay) { if (e == s) { ++n; } }
    return n;
}

} // namespace

int main() {
    // A leaf that appends its name to the shared log and returns a fixed Status.
    auto leaf = [](std::vector<std::string>& log, const char* name, Status s) {
        return action([&log, name, s] { log.push_back(name); return s; });
    };

    // --- SEQUENCE ALL-SUCCESS ------------------------------------------------
    {
        std::vector<std::string> log;
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   leaf(log, "B", Status::Success),
                                   leaf(log, "C", Status::Success)));
        check(tree.tick() == Status::Success, "sequence(all success) -> Success");
        check(log == std::vector<std::string>({"A", "B", "C"}), "sequence ticks all children in order");
    }

    // --- SEQUENCE SHORT-CIRCUIT ON FAILURE -----------------------------------
    {
        std::vector<std::string> log;
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   leaf(log, "B", Status::Failure),
                                   leaf(log, "C", Status::Success)));
        check(tree.tick() == Status::Failure, "sequence short-circuits to Failure");
        check(log == std::vector<std::string>({"A", "B"}), "sequence does NOT tick children past the failure");
    }

    // --- SELECTOR FIRST-SUCCESS ----------------------------------------------
    {
        std::vector<std::string> log;
        BehaviorTree tree(selector(leaf(log, "A", Status::Failure),
                                   leaf(log, "B", Status::Success),
                                   leaf(log, "C", Status::Success)));
        check(tree.tick() == Status::Success, "selector returns on first success");
        check(log == std::vector<std::string>({"A", "B"}), "selector does NOT tick children past the first success");
    }

    // --- SELECTOR ALL-FAIL ---------------------------------------------------
    {
        std::vector<std::string> log;
        BehaviorTree tree(selector(leaf(log, "A", Status::Failure),
                                   leaf(log, "B", Status::Failure)));
        check(tree.tick() == Status::Failure, "selector(all fail) -> Failure");
        check(log == std::vector<std::string>({"A", "B"}), "selector ticks all children when all fail");
    }

    // --- INVERTER ------------------------------------------------------------
    {
        std::vector<std::string> log;
        Inverter invS(leaf(log, "s", Status::Success));
        check(invS.tick() == Status::Failure, "inverter(Success) -> Failure");
        Inverter invF(leaf(log, "f", Status::Failure));
        check(invF.tick() == Status::Success, "inverter(Failure) -> Success");
        Inverter invR(leaf(log, "r", Status::Running));
        check(invR.tick() == Status::Running, "inverter(Running) -> Running (pass through)");
    }

    // --- SEQUENCE RUNNING-RESUME ---------------------------------------------
    {
        std::vector<std::string> log;
        // gate returns Running on the first tick, then Success thereafter.
        int gateTicks = 0;
        NodePtr gate = action([&log, &gateTicks] {
            log.push_back("gate");
            ++gateTicks;
            return gateTicks == 1 ? Status::Running : Status::Success;
        });
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   std::move(gate),
                                   leaf(log, "C", Status::Success)));
        check(tree.tick() == Status::Running, "sequence suspends at Running child");
        check(log == std::vector<std::string>({"A", "gate"}), "tick1 ticks A then gate, stops at Running");
        check(tree.tick() == Status::Success, "sequence resumes and completes on tick2");
        check(count(log, "A") == 1, "A ticked exactly once (not re-ticked on resume)");
        check(count(log, "gate") == 2, "gate ticked twice (resume lands on it)");
        check(count(log, "C") == 1, "C ticked once after gate succeeds");
    }

    // --- SELECTOR RUNNING-RESUME ---------------------------------------------
    {
        std::vector<std::string> log;
        int gateTicks = 0;
        NodePtr gate = action([&log, &gateTicks] {
            log.push_back("gate");
            ++gateTicks;
            return gateTicks == 1 ? Status::Running : Status::Success;
        });
        BehaviorTree tree(selector(leaf(log, "A", Status::Failure),
                                   std::move(gate)));
        check(tree.tick() == Status::Running, "selector suspends at Running child");
        check(log == std::vector<std::string>({"A", "gate"}), "tick1 ticks A then gate, stops at Running");
        check(tree.tick() == Status::Success, "selector resumes and succeeds on tick2");
        check(count(log, "A") == 1, "A ticked exactly once (not re-ticked on resume)");
        check(count(log, "gate") == 2, "gate ticked twice (resume lands on it)");
    }

    // --- REPEATER ------------------------------------------------------------
    {
        int childTicks = 0;
        int childResets = 0;
        // A child that counts ticks and resets so we can observe reset-between-reps.
        class CountingLeaf : public Node {
          public:
            CountingLeaf(int& ticks, int& resets) : m_ticks(ticks), m_resets(resets) {}
            Status tick() override { ++m_ticks; return Status::Success; }
            void reset() override { ++m_resets; }
          private:
            int& m_ticks;
            int& m_resets;
        };
        BehaviorTree tree(repeater(std::make_unique<CountingLeaf>(childTicks, childResets), 3));
        check(tree.tick() == Status::Running, "repeater tick1 -> Running");
        check(tree.tick() == Status::Running, "repeater tick2 -> Running");
        check(tree.tick() == Status::Success, "repeater tick3 -> Success (3 completions)");
        check(childTicks == 3, "repeater ticked the child 3 times across 3 ticks");
        check(childResets == 3, "repeater reset the child between/after each completed repetition");
    }

    // --- RESET ---------------------------------------------------------------
    {
        std::vector<std::string> log;
        int gateTicks = 0;
        NodePtr gate = action([&log, &gateTicks] {
            log.push_back("gate");
            ++gateTicks;
            return gateTicks == 1 ? Status::Running : Status::Success;
        });
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   std::move(gate)));
        check(tree.tick() == Status::Running, "sequence runs to a Running state");
        check(log == std::vector<std::string>({"A", "gate"}), "before reset: A then gate");
        log.clear();
        tree.reset();
        tree.tick();  // gateTicks now 2 -> gate Success, whole sequence Success
        check(count(log, "A") == 1, "after reset: A is re-ticked from the top");
        check(log.front() == "A", "after reset the sequence restarts at the first child");
    }

    // --- WRAPPER + NESTED ----------------------------------------------------
    {
        std::vector<std::string> log;
        // selector( sequence(fail, X) , success ) — inner sequence fails fast on
        // its first child, so the selector falls through to the trailing success.
        BehaviorTree tree(selector(sequence(leaf(log, "seqFail", Status::Failure),
                                            leaf(log, "seqUnreached", Status::Success)),
                                   leaf(log, "fallback", Status::Success)));
        check(tree.tick() == Status::Success, "nested selector/sequence -> Success via fallback");
        check(log == std::vector<std::string>({"seqFail", "fallback"}), "inner sequence short-circuits, selector uses fallback");
        // reset() delegates to the root without crashing; re-tick reproduces.
        log.clear();
        tree.reset();
        check(tree.tick() == Status::Success, "wrapper reset()+tick() delegate to root");
        check(log == std::vector<std::string>({"seqFail", "fallback"}), "post-reset nested tree reproduces the same order");
    }

    // --- TERMINAL RE-ENTRANCY (completed composite restarts from child 0) ----
    // Per-frame BT semantics: a composite that ran to a terminal Status (not
    // Running) resets its resume bookmark to child 0, so the NEXT tick re-runs it
    // from the top WITHOUT an explicit reset(). Verified above only by code trace;
    // asserted directly here. If m_current were left at the end on completion, the
    // second tick would skip children and these count checks would fail.
    {
        // Sequence, terminal Success: both children always succeed.
        std::vector<std::string> log;
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   leaf(log, "B", Status::Success)));
        check(tree.tick() == Status::Success, "seq re-entrancy: tick1 -> Success");
        check(log == std::vector<std::string>({"A", "B"}), "seq re-entrancy: tick1 ticks A then B");
        log.clear();
        check(tree.tick() == Status::Success, "seq re-entrancy: tick2 (no reset) -> Success again");
        check(log == std::vector<std::string>({"A", "B"}), "seq re-entrancy: tick2 restarts from child 0 (A then B again)");
        check(count(log, "A") == 1 && count(log, "B") == 1, "seq re-entrancy: both children re-ticked on tick2");
    }
    {
        // Selector, terminal Success at B (A fails first): completing at a
        // non-zero child must still rewind to child 0 for the next tick.
        std::vector<std::string> log;
        BehaviorTree tree(selector(leaf(log, "A", Status::Failure),
                                   leaf(log, "B", Status::Success)));
        check(tree.tick() == Status::Success, "sel re-entrancy: tick1 -> Success");
        check(log == std::vector<std::string>({"A", "B"}), "sel re-entrancy: tick1 ticks A then B");
        log.clear();
        check(tree.tick() == Status::Success, "sel re-entrancy: tick2 (no reset) -> Success again");
        check(log == std::vector<std::string>({"A", "B"}), "sel re-entrancy: tick2 restarts from child 0 (A then B again)");
        check(count(log, "A") == 1 && count(log, "B") == 1, "sel re-entrancy: both children re-ticked on tick2");
    }
    {
        // Sequence, terminal Failure at B: the fail path also rewinds to child 0.
        std::vector<std::string> log;
        BehaviorTree tree(sequence(leaf(log, "A", Status::Success),
                                   leaf(log, "B", Status::Failure)));
        check(tree.tick() == Status::Failure, "seq-fail re-entrancy: tick1 -> Failure");
        check(log == std::vector<std::string>({"A", "B"}), "seq-fail re-entrancy: tick1 ticks A then B");
        log.clear();
        check(tree.tick() == Status::Failure, "seq-fail re-entrancy: tick2 (no reset) -> Failure again");
        check(log == std::vector<std::string>({"A", "B"}), "seq-fail re-entrancy: tick2 restarts from child 0 (A then B again)");
        check(count(log, "A") == 1 && count(log, "B") == 1, "seq-fail re-entrancy: both children re-ticked on tick2");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
