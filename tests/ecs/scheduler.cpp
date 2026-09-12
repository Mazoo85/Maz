// tests/ecs/scheduler.cpp — verifies ecs::Scheduler, the ordered system runner, which was the last
// module in the engine with neither a test nor a golden frame behind it. What it promises is
// specific and easy to get subtly wrong: systems run phase by phase and, inside a phase, by order
// and then by the order they were registered; run() does that serially, runParallel() may overlap
// the systems marked parallelSafe WITHIN a phase but must never let one phase bleed into the next.
// Both paths are checked here, the parallel one against a real core::JobSystem rather than a mock —
// the barrier between phases is the thing worth proving, and a mock would prove nothing about it.
// Deterministic CPU: no GPU, no window.
#include "maz/ecs/Scheduler.hpp"

#include "maz/core/Jobs.hpp"
#include "maz/ecs/World.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::JobSystem;
using maz::ecs::Scheduler;
using maz::ecs::World;

static std::string joined(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) {
        if (!s.empty()) s += ",";
        s += x;
    }
    return s;
}

int main() {
    // --- 1. An empty scheduler runs nothing and says so. ---
    {
        Scheduler s;
        World w;
        CHECK(s.count() == 0, "a new scheduler holds no systems");
        CHECK(s.runOrder().empty(), "and has nothing to run");
        s.run(w);   // must not crash
    }

    // --- 2. Lower phases run before higher ones, whatever order they were added in. ---
    {
        Scheduler s;
        std::vector<std::string> ran;
        s.add("late", [&](World&) { ran.push_back("late"); }, 20);
        s.add("input", [&](World&) { ran.push_back("input"); }, 0);
        s.add("sim", [&](World&) { ran.push_back("sim"); }, 10);

        CHECK(joined(s.runOrder()) == "input,sim,late", "phases sort ahead of registration order");
        World w;
        s.run(w);
        CHECK(joined(ran) == "input,sim,late", "and run() honours that");
    }

    // --- 3. Within a phase, `order` breaks the tie. ---
    {
        Scheduler s;
        s.add("third", [](World&) {}, 0, 30);
        s.add("first", [](World&) {}, 0, 10);
        s.add("second", [](World&) {}, 0, 20);
        CHECK(joined(s.runOrder()) == "first,second,third", "order sorts within a phase");
    }

    // --- 4. Equal phase AND order falls back to registration order, stably. ---
    //     Two systems with nothing to separate them must not swap around between
    //     runs, or a frame stops being reproducible.
    {
        Scheduler s;
        s.add("a", [](World&) {}, 0, 0);
        s.add("b", [](World&) {}, 0, 0);
        s.add("c", [](World&) {}, 0, 0);
        CHECK(joined(s.runOrder()) == "a,b,c", "ties keep insertion order");
        CHECK(joined(s.runOrder()) == "a,b,c", "and keep it when asked again");
    }

    // --- 5. Negative phases are allowed and still sort first. ---
    {
        Scheduler s;
        s.add("normal", [](World&) {}, 0);
        s.add("pre", [](World&) {}, -100);
        CHECK(joined(s.runOrder()) == "pre,normal", "a negative phase runs before zero");
    }

    // --- 6. A system added after the first run still lands in the right place. ---
    //     The sort is cached; adding must invalidate it, or the new system is
    //     silently never run.
    {
        Scheduler s;
        World w;
        s.add("sim", [](World&) {}, 10);
        CHECK(joined(s.runOrder()) == "sim", "one system to start");
        s.add("input", [](World&) {}, 0);
        CHECK(joined(s.runOrder()) == "input,sim", "a system added later is sorted in, not appended");
        int ran = 0;
        s.add("counter", [&](World&) { ran++; }, 5);
        s.run(w);
        CHECK(ran == 1, "and it actually runs");
    }

    // --- 7. add() hands back a usable index, and systems() reports what was stored. ---
    {
        Scheduler s;
        const size_t a = s.add("a", [](World&) {}, 1, 2, true);
        const size_t b = s.add("b", [](World&) {});
        CHECK(a == 0 && b == 1, "indices count up from zero");
        CHECK(s.count() == 2, "count follows");
        CHECK(s.systems()[a].name == "a" && s.systems()[a].phase == 1 &&
              s.systems()[a].order == 2 && s.systems()[a].parallelSafe,
              "the system was stored as described");
        CHECK(!s.systems()[b].parallelSafe, "parallelSafe defaults to false — opt in, not out");
    }

    // --- 8. Every system runs exactly once per run(), and again on the next run. ---
    {
        Scheduler s;
        World w;
        int n = 0;
        s.add("once", [&](World&) { n++; });
        s.run(w);
        CHECK(n == 1, "run() runs each system once");
        s.run(w);
        CHECK(n == 2, "and again the next frame");
    }

    // --- 9. Systems are handed the world they were given. ---
    {
        Scheduler s;
        World w;
        World* seen = nullptr;
        s.add("peek", [&](World& given) { seen = &given; });
        s.run(w);
        CHECK(seen == &w, "the system received the caller's world, not a copy");
    }

    // --- 10. runParallel keeps the barrier between phases. ---
    //     This is the promise worth proving: systems inside a phase may overlap,
    //     but nothing in a later phase may begin until the earlier one is done.
    //     The phase-0 systems sleep, so a missing barrier shows up as the next
    //     phase starting early rather than as a rare flake.
    {
        Scheduler s;
        World w;
        JobSystem jobs(4);
        std::atomic<int> running{0};
        std::atomic<int> done{0};
        std::atomic<bool> overlap{false};

        for (int i = 0; i < 3; ++i) {
            s.add("slow" + std::to_string(i), [&](World&) {
                running++;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                running--;
                done++;
            }, 0, i, true);
        }
        s.add("after", [&](World&) {
            if (running.load() != 0 || done.load() != 3) overlap = true;
        }, 10, 0, false);

        s.runParallel(w, jobs);
        CHECK(done.load() == 3, "every parallel system in the phase completed");
        CHECK(!overlap.load(), "the next phase did not start until the previous one had finished");
    }

    // --- 11. runParallel runs every system exactly once, serial and parallel alike. ---
    {
        Scheduler s;
        World w;
        JobSystem jobs(3);
        std::atomic<int> total{0};
        for (int i = 0; i < 5; ++i) s.add("p" + std::to_string(i), [&](World&) { total++; }, 0, i, true);
        for (int i = 0; i < 3; ++i) s.add("s" + std::to_string(i), [&](World&) { total++; }, 0, 10 + i, false);
        s.runParallel(w, jobs);
        CHECK(total.load() == 8, "all eight systems ran once each");
    }

    // --- 12. runParallel with no parallelSafe systems is just run(), in order. ---
    {
        Scheduler s;
        World w;
        JobSystem jobs(2);
        std::vector<std::string> ran;
        std::mutex m;
        int idx = 0;
        for (const char* name : {"a", "b", "c"}) {
            const std::string n = name;
            s.add(n, [&, n](World&) { std::lock_guard<std::mutex> g(m); ran.push_back(n); }, 0, idx++, false);
        }
        s.runParallel(w, jobs);
        CHECK(joined(ran) == "a,b,c", "serial systems keep their order under runParallel");
    }

    // --- 13. runParallel on an empty scheduler returns rather than hanging. ---
    {
        Scheduler s;
        World w;
        JobSystem jobs(2);
        s.runParallel(w, jobs);
        CHECK(true, "an empty parallel run returns");
    }

    if (g_fail == 0) std::printf("ecs scheduler: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
