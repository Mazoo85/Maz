// Unit tests for maz::ecs::SystemScheduler — the ordered ECS system scheduler.
// Exercises registration-order run, world mutation from a system, repeated
// run() calls, remove/enable/disable/has/size/clear, and a flagship movement
// system driving World::view end-to-end. Pure C++, no GPU/display.

#include "maz/ecs/SystemScheduler.hpp"
#include "maz/core/StringId.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace maz::ecs;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

struct Position { int x; int y; };
struct Velocity { int dx; int dy; };

} // namespace

int main() {
    // --- REGISTRATION ORDER --------------------------------------------------
    {
        World w;
        SystemScheduler s;
        std::vector<int> order;
        s.add("a", [&](World&){ order.push_back(1); });
        s.add("b", [&](World&){ order.push_back(2); });
        s.run(w);
        check(order.size() == 2 && order[0] == 1 && order[1] == 2, "systems run in registration order");
    }

    // --- SYSTEM MUTATES WORLD ------------------------------------------------
    {
        World w;
        SystemScheduler s;
        s.add("spawn", [](World& world){ Entity e = world.create(); world.add<int>(e, 42); });
        s.run(w);
        check(w.entityCount() == 1 && w.componentCount<int>() == 1, "system created an entity+component");
    }

    // --- MULTIPLE RUNS -------------------------------------------------------
    {
        World w;
        SystemScheduler s;
        int counter = 0;
        s.add("inc", [&](World&){ counter += 5; });
        s.run(w);
        s.run(w);
        check(counter == 10, "system runs each run() call");
    }

    // --- REMOVE --------------------------------------------------------------
    {
        World w;
        SystemScheduler s;
        std::vector<int> log;
        s.add("a", [&](World&){ log.push_back(1); });
        s.add("b", [&](World&){ log.push_back(2); });
        s.add("c", [&](World&){ log.push_back(3); });
        check(s.remove("b"), "remove existing returns true");
        check(s.size() == 2, "size drops after remove");
        log.clear();
        s.run(w);
        check(log.size() == 2 && log[0] == 1 && log[1] == 3, "removed system does not run, order preserved");
        check(!s.remove("zzz"), "remove nonexistent returns false");
    }

    // --- ENABLE / DISABLE ----------------------------------------------------
    {
        World w;
        SystemScheduler s;
        std::vector<int> log;
        s.add("a", [&](World&){ log.push_back(1); });
        s.add("b", [&](World&){ log.push_back(2); });
        check(s.setEnabled("b", false), "setEnabled existing returns true");
        check(!s.isEnabled("b") && s.isEnabled("a"), "disabled b, a still enabled");
        log.clear();
        s.run(w);
        check(log.size() == 1 && log[0] == 1, "disabled system skipped");
        check(s.setEnabled("b", true), "re-enable returns true");
        log.clear();
        s.run(w);
        check(log.size() == 2, "re-enabled system runs");
        check(!s.isEnabled("nope"), "absent isEnabled false");
        check(!s.setEnabled("nope", true), "absent setEnabled false");
    }

    // --- HAS / SIZE / CLEAR --------------------------------------------------
    {
        World w;
        SystemScheduler s;
        s.add("a", [](World&){});
        check(s.has("a") && !s.has("b") && s.size() == 1, "has/size after single add");
        s.clear();
        check(s.size() == 0 && !s.has("a"), "clear empties the scheduler");
        int ran = 0;
        s.add("x", [&](World&){ ++ran; });
        s.clear();
        s.run(w);
        check(ran == 0, "cleared scheduler runs nothing");
    }

    // --- DUPLICATE-NAME ADD is a MAZ_ASSERT (programmer error), not exercised here.

    // --- FLAGSHIP: MOVEMENT SYSTEM (whole stack end-to-end) ------------------
    {
        World w;
        SystemScheduler s;
        Entity a = w.create(); w.add<Position>(a, Position{0, 0}); w.add<Velocity>(a, Velocity{1, 2});
        Entity b = w.create(); w.add<Position>(b, Position{10, 10}); w.add<Velocity>(b, Velocity{-1, 0});
        Entity c = w.create(); w.add<Position>(c, Position{5, 5});  // Position only, no Velocity
        s.add("movement", [](World& world){ world.view<Position, Velocity>([](Entity, Position& p, Velocity& v){ p.x += v.dx; p.y += v.dy; }); });
        s.run(w);
        s.run(w);  // two ticks
        check(w.get<Position>(a)->x == 2 && w.get<Position>(a)->y == 4, "entity a moved by 2*velocity");
        check(w.get<Position>(b)->x == 8 && w.get<Position>(b)->y == 10, "entity b moved by 2*velocity");
        check(w.get<Position>(c)->x == 5 && w.get<Position>(c)->y == 5, "Position-only entity untouched (no Velocity)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
