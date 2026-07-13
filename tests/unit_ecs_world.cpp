// Unit tests for maz::ecs::World — the minimal ECS entity/component runtime.
// Exercises generational entity recycle (a recycled index at a new generation
// must reject the stale handle and inherit NO stale component), type-erased
// per-type component stores with signature/store sync, and single-component
// each<T> iteration. Pure C++, no GPU/display.

#include "maz/ecs/World.hpp"

#include <cstdint>
#include <cstdio>

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
struct Missing {};

} // namespace

int main() {
    // --- CREATE / ALIVE / DISTINCT -------------------------------------------
    {
        World w;
        Entity a = w.create();
        Entity b = w.create();
        check(w.alive(a) && w.alive(b), "created entities are alive");
        check(a != b, "two creates give distinct entities");
        check(w.entityCount() == 2, "entityCount() tracks the live count");
        check(!w.alive(Entity::null()), "null entity is not alive");
    }

    // --- ADD / GET / HAS -----------------------------------------------------
    {
        World w;
        Entity a = w.create();
        Entity b = w.create();
        w.add<int>(a, 42);
        check(w.has<int>(a), "has<int>(a) after add");
        check(w.get<int>(a) != nullptr && *w.get<int>(a) == 42, "get<int>(a) resolves to stored value");
        check(!w.has<int>(b) && w.get<int>(b) == nullptr, "b has no int component");
        check(w.componentCount<int>() == 1, "componentCount<int>() == 1");
    }

    // --- SECOND TYPE INDEPENDENT ---------------------------------------------
    {
        World w;
        Entity a = w.create();
        Entity b = w.create();
        w.add<int>(a, 42);
        w.add<Position>(a, Position{1, 2});
        check(w.has<Position>(a) && w.has<int>(a), "a has both Position and int");
        check(w.get<Position>(a)->x == 1 && w.get<Position>(a)->y == 2, "Position value stored correctly");
        w.add<Position>(b, Position{3, 4});
        check(w.componentCount<Position>() == 2 && w.componentCount<int>() == 1, "per-type counts independent");
        check(!w.has<int>(b) && w.has<Position>(b), "b has Position but not int");
    }

    // --- REMOVE --------------------------------------------------------------
    {
        World w;
        Entity a = w.create();
        w.add<int>(a, 42);
        w.add<Position>(a, Position{1, 2});
        w.remove<int>(a);
        check(!w.has<int>(a) && w.get<int>(a) == nullptr && w.componentCount<int>() == 0, "int removed cleanly");
        check(w.has<Position>(a), "Position survives int removal");
    }

    // --- MUTATE VIA GET + ADD-RETURNS-REF ------------------------------------
    {
        World w;
        Entity a = w.create();
        w.add<Position>(a, Position{1, 2});
        *w.get<Position>(a) = Position{7, 8};
        check(w.get<Position>(a)->x == 7 && w.get<Position>(a)->y == 8, "mutate component in place via get");
        int& r = w.add<int>(a, 5);
        r = 6;
        check(*w.get<int>(a) == 6, "add returns a mutable reference to the stored component");
    }

    // --- DESTROY -------------------------------------------------------------
    {
        World w;
        Entity a = w.create();
        (void)a;
        Entity c = w.create();
        w.add<int>(c, 99);
        std::size_t n = w.entityCount();
        w.destroy(c);
        check(!w.alive(c), "destroyed entity is not alive");
        check(w.get<int>(c) == nullptr, "get on destroyed entity returns nullptr");
        check(w.entityCount() == n - 1, "entityCount decrements on destroy");
    }

    // --- RECYCLE + STALE (KEY gate) ------------------------------------------
    {
        World w;
        Entity x = w.create();
        std::uint32_t xi = x.index;
        w.add<int>(x, 111);
        w.destroy(x);
        Entity y = w.create();
        check(y.index == xi, "freed index is reused");
        check(y.generation != x.generation, "recycled index has a different generation");
        check(!w.alive(x), "stale handle is dead after recycle");
        check(w.get<int>(x) == nullptr, "get(stale) returns nullptr");
        check(!w.has<int>(y), "recycled entity inherits no stale component");
        check(w.get<int>(y) == nullptr, "get on recycled entity returns nullptr");
    }

    // --- each<T> -------------------------------------------------------------
    {
        World w2;
        Entity e1 = w2.create();
        Entity e2 = w2.create();
        Entity e3 = w2.create();
        w2.add<int>(e1, 10);
        w2.add<int>(e3, 30);
        int sum = 0;
        int cnt = 0;
        bool sawE2 = false;
        w2.each<int>([&](Entity ent, int& v) {
            ++cnt;
            sum += v;
            if (ent.index == e2.index) { sawE2 = true; }
        });
        check(cnt == 2 && sum == 40 && !sawE2, "each<int> visits only int-holders");
        int mcnt = 0;
        w2.each<Missing>([&](Entity, Missing&) { ++mcnt; });
        check(mcnt == 0, "each over a never-used type visits nothing");
    }

    // --- MANY ----------------------------------------------------------------
    {
        World w3;
        Entity es[100];
        for (std::size_t i = 0; i < 100; ++i) {
            es[i] = w3.create();
            w3.add<int>(es[i], static_cast<int>(i));
        }
        check(w3.entityCount() == 100 && w3.componentCount<int>() == 100, "100 entities each with an int");
        {
            long s = 0;
            w3.each<int>([&](Entity, int& v) { s += v; });
            check(s == 4950, "each<int> sums 0..99 == 4950");
        }
        for (std::size_t i = 0; i < 100; i += 2) {
            w3.destroy(es[i]);  // destroy the even-index ones
        }
        check(w3.entityCount() == 50 && w3.componentCount<int>() == 50, "half destroyed leaves 50");
        {
            int c2 = 0;
            long s2 = 0;
            w3.each<int>([&](Entity, int& v) { ++c2; s2 += v; });
            check(c2 == 50 && s2 == 2500, "each<int> over survivors sums the odds 1..99 == 2500");
        }
    }

    // --- view: 2-component visits exactly the "both" set --------------------
    {
        World w;
        Entity eAB = w.create(); w.add<int>(eAB, 7); w.add<Position>(eAB, Position{1, 2});
        Entity eA  = w.create(); w.add<int>(eA, 9);
        Entity eB  = w.create(); w.add<Position>(eB, Position{5, 6});
        Entity eNone = w.create(); (void)eNone;
        int count = 0; bool onlyEAB = true; bool valuesOk = false;
        w.view<int, Position>([&](Entity ent, int& n, Position& p) {
            ++count;
            if (ent.index != eAB.index) { onlyEAB = false; }
            if (ent.index == eAB.index && n == 7 && p.x == 1 && p.y == 2) { valuesOk = true; }
        });
        check(count == 1 && onlyEAB, "view<int,Position> visits only the both-holder");
        check(valuesOk, "view passes the correct component values by reference");
        (void)eA; (void)eB;
    }

    // --- view: bigger mixed set, expected computed via a shadow -------------
    {
        World w;
        Entity es[12]; bool expectBoth[12] = {};
        int expectedCount = 0;
        for (std::size_t i = 0; i < 12; ++i) {
            es[i] = w.create();
            bool hasInt = (i % 2 == 0);
            bool hasPos = (i % 3 == 0);
            if (hasInt) { w.add<int>(es[i], static_cast<int>(i)); }
            if (hasPos) { w.add<Position>(es[i], Position{static_cast<int>(i), 0}); }
            if (hasInt && hasPos) { expectBoth[i] = true; ++expectedCount; }
        }
        int count = 0; bool allHadBoth = true;
        w.view<int, Position>([&](Entity ent, int&, Position&) {
            ++count;
            bool matched = false;
            for (std::size_t i = 0; i < 12; ++i) { if (es[i].index == ent.index) { matched = expectBoth[i]; break; } }
            if (!matched) { allHadBoth = false; }
        });
        check(count == expectedCount && allHadBoth, "view<int,Position> visits exactly the both-subset");
    }

    // --- view: 3-component ------------------------------------------------
    {
        World w;
        Entity eFull = w.create(); w.add<int>(eFull, 1); w.add<Position>(eFull, Position{1,1}); w.add<Velocity>(eFull, Velocity{2,3});
        Entity eNoVel = w.create(); w.add<int>(eNoVel, 1); w.add<Position>(eNoVel, Position{0,0});
        Entity eNoPos = w.create(); w.add<int>(eNoPos, 1); w.add<Velocity>(eNoPos, Velocity{9,9});
        int count = 0; bool onlyFull = true;
        w.view<int, Position, Velocity>([&](Entity ent, int&, Position&, Velocity& v) {
            ++count;
            if (ent.index != eFull.index || v.dx != 2 || v.dy != 3) { onlyFull = false; }
        });
        check(count == 1 && onlyFull, "view<int,Position,Velocity> visits only the full-set entity");
        (void)eNoVel; (void)eNoPos;
    }

    // --- view: mutable refs write through ---------------------------------
    {
        World w;
        Entity eAB = w.create(); w.add<int>(eAB, 5); w.add<Position>(eAB, Position{10, 20});
        Entity eA  = w.create(); w.add<int>(eA, 5);  // must NOT be mutated (no Position)
        w.view<int, Position>([](Entity, int& n, Position& p) { n += 100; p.x += 1; });
        check(*w.get<int>(eAB) == 105 && w.get<Position>(eAB)->x == 11 && w.get<Position>(eAB)->y == 20, "view mutations write through to storage");
        check(*w.get<int>(eA) == 5, "view leaves non-matching entities untouched");
    }

    // --- view: order independence (int-lead vs Position-lead) --------------
    {
        World w;
        Entity eAB1 = w.create(); w.add<int>(eAB1, 1); w.add<Position>(eAB1, Position{0,0});
        Entity eAB2 = w.create(); w.add<int>(eAB2, 2); w.add<Position>(eAB2, Position{0,0});
        Entity eA   = w.create(); w.add<int>(eA, 3);
        Entity eB   = w.create(); w.add<Position>(eB, Position{0,0});
        bool seenA[64] = {}; bool seenB[64] = {}; int cA = 0; int cB = 0;
        w.view<int, Position>([&](Entity ent, int&, Position&) { seenA[ent.index] = true; ++cA; });
        w.view<Position, int>([&](Entity ent, Position&, int&) { seenB[ent.index] = true; ++cB; });
        bool same = (cA == cB);
        for (int i = 0; i < 64; ++i) { if (seenA[i] != seenB[i]) { same = false; } }
        check(cA == 2 && same, "view<int,Position> and view<Position,int> match the same set");
        (void)eAB1; (void)eAB2; (void)eA; (void)eB;
    }

    // --- view: no match -----------------------------------------------------
    {
        World w;
        Entity a = w.create(); w.add<int>(a, 1);
        Entity b = w.create(); w.add<Velocity>(b, Velocity{1,1});
        int count = 0;
        w.view<int, Velocity>([&](Entity, int&, Velocity&) { ++count; });
        check(count == 0, "view over a combo no entity has visits nothing");
        (void)a; (void)b;
    }

    // --- view: never-added type (non-lead and lead) ------------------------
    {
        World w;
        Entity a = w.create(); w.add<int>(a, 1);
        int c1 = 0; int c2 = 0;
        w.view<int, Missing>([&](Entity, int&, Missing&) { ++c1; });   // Missing never added (non-lead)
        w.view<Missing, int>([&](Entity, Missing&, int&) { ++c2; });   // Missing as lead, never added -> early return
        check(c1 == 0 && c2 == 0, "view with a never-added type (either position) visits nothing, no crash");
        (void)a;
    }

    // --- view: destroy excludes --------------------------------------------
    {
        World w;
        Entity eAB = w.create(); w.add<int>(eAB, 1); w.add<Position>(eAB, Position{0,0});
        int before = 0;
        w.view<int, Position>([&](Entity, int&, Position&) { ++before; });
        check(before == 1, "view visits the live both-holder before destroy");
        w.destroy(eAB);
        int after = 0;
        w.view<int, Position>([&](Entity, int&, Position&) { ++after; });
        check(after == 0, "view no longer visits a destroyed entity");
    }

    // --- view: recycled index with partial components not wrongly matched --
    {
        World w;
        Entity x = w.create(); w.add<int>(x, 1); w.add<Position>(x, Position{0,0});
        w.destroy(x);
        Entity y = w.create();          // recycles x's index
        w.add<int>(y, 2);               // int only — no Position this time
        check(y.index == x.index, "recycled index reused (precondition for this check)");
        int count = 0;
        w.view<int, Position>([&](Entity, int&, Position&) { ++count; });
        check(count == 0, "recycled entity with only some components is not matched by the full view");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
