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

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
