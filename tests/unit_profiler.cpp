// Unit tests for maz::core::Profiler + ScopedTimer. Exercises named-scope timing
// aggregation (count/total/min/max), the first-sample min-seeding rule, scope
// isolation, reset, map iteration, and RAII timer smoke behavior. Pure C++, no
// GPU/display; timing asserts stay loose (ordering only, no exact-ns checks).

#include "maz/core/Profiler.hpp"

#include <cstdint>
#include <cstdio>

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
    // --- EMPTY ----------------------------------------------------------------
    {
        Profiler p;
        check(p.scopeCount() == 0, "empty profiler has zero scopes");
        check(p.get("x"_sid) == nullptr, "get on unknown scope returns nullptr");
        check(!p.has("x"_sid), "has on unknown scope returns false");
    }

    // --- SINGLE RECORD --------------------------------------------------------
    {
        Profiler p;
        p.record("frame"_sid, 100);
        const Stat* s = p.get("frame"_sid);
        check(s != nullptr, "get returns a Stat after record");
        check(s->count == 1 && s->totalNs == 100 && s->minNs == 100 && s->maxNs == 100,
              "single sample seeds count/total/min/max");
        check(p.scopeCount() == 1, "one scope after a single record");
    }

    // --- MULTIPLE SAMPLES, SAME SCOPE (KEY min-seeding gate) ------------------
    {
        Profiler p;
        p.record("a"_sid, 100);
        p.record("a"_sid, 50);
        p.record("a"_sid, 200);
        p.record("a"_sid, 75);
        const Stat* s = p.get("a"_sid);
        check(s->count == 4 && s->totalNs == 425 && s->minNs == 50 && s->maxNs == 200,
              "four samples aggregate (min==50 gates the min-seeding rule)");
    }

    // --- MIN-SEEDING EDGE (descending samples) -------------------------------
    {
        Profiler p;
        p.record("b"_sid, 500);
        p.record("b"_sid, 10);
        const Stat* s = p.get("b"_sid);
        check(s->minNs == 10 && s->maxNs == 500, "min tracks a later smaller sample");
    }

    // --- TWO DISTINCT SCOPES ARE ISOLATED ------------------------------------
    {
        Profiler p;
        p.record("a"_sid, 10);
        p.record("b"_sid, 20);
        p.record("a"_sid, 5);
        check(p.scopeCount() == 2, "two distinct names give two scopes");
        check(p.get("a"_sid)->count == 2 && p.get("a"_sid)->totalNs == 15 && p.get("a"_sid)->minNs == 5,
              "scope a aggregates independently");
        check(p.get("b"_sid)->count == 1 && p.get("b"_sid)->totalNs == 20,
              "scope b aggregates independently");
    }

    // --- EQUAL SAMPLES --------------------------------------------------------
    {
        Profiler p;
        p.record("c"_sid, 42);
        p.record("c"_sid, 42);
        p.record("c"_sid, 42);
        const Stat* s = p.get("c"_sid);
        check(s->count == 3 && s->totalNs == 126 && s->minNs == 42 && s->maxNs == 42,
              "equal samples keep min==max==sample");
    }

    // --- RESET ----------------------------------------------------------------
    {
        Profiler p;
        p.record("a"_sid, 1);
        p.record("b"_sid, 2);
        p.reset();
        check(p.scopeCount() == 0 && p.get("a"_sid) == nullptr, "reset clears all scopes");
        p.record("d"_sid, 7);
        check(p.get("d"_sid)->count == 1, "profiler is usable after reset");
    }

    // --- ITERATION VIA stats() -----------------------------------------------
    {
        Profiler p;
        p.record("a"_sid, 1);
        p.record("b"_sid, 2);
        p.record("b"_sid, 3);
        p.record("c"_sid, 4);
        uint64_t sum = 0;
        for (const auto& kv : p.stats()) {
            sum += kv.second.count;
        }
        check(sum == 4, "stats() iteration sees all four samples across scopes");
        check(p.stats().size() == p.scopeCount() && p.scopeCount() == 3,
              "stats() size matches scopeCount() == 3");
    }

    // --- ScopedTimer SMOKE (loose) -------------------------------------------
    {
        Profiler p;
        {
            ScopedTimer t(p, "scope"_sid);
            volatile unsigned long spin = 0;
            for (int i = 0; i < 100000; ++i) {
                spin += static_cast<unsigned long>(i);
            }
            (void)spin;
        }
        const Stat* s = p.get("scope"_sid);
        check(s != nullptr, "ScopedTimer records its scope on destruction");
        check(s->count == 1, "ScopedTimer records exactly one sample");
        check(s->maxNs >= s->minNs, "recorded max is >= min");
    }

    // --- ScopedTimer RIGHT SCOPE + ACCUMULATION ------------------------------
    {
        Profiler p;
        {
            ScopedTimer t(p, "scope"_sid);
            volatile int x = 0;
            (void)x;
        }
        {
            ScopedTimer t(p, "other"_sid);
            volatile int y = 0;
            (void)y;
        }
        check(p.get("scope"_sid) != nullptr && p.get("scope"_sid)->count == 1,
              "first timer records the scope name");
        check(p.get("other"_sid) != nullptr && p.get("other"_sid)->count == 1,
              "second timer records the other name");
        {
            ScopedTimer t(p, "scope"_sid);
            volatile int z = 0;
            (void)z;
        }
        check(p.get("scope"_sid)->count == 2, "re-timing a scope accumulates the count");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
