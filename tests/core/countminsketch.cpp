// tests/core/countminsketch.cpp — verifies the Count-Min Sketch (core CountMinSketch.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * the estimate is NEVER below the true count (the core one-sided-error guarantee) — checked for every
//     key over a known stream;
//   * with a wide-enough table the estimates are exact or very close for the vast majority of keys;
//   * heavy hitters are recovered accurately, and the overestimate is bounded by ~epsilon*total;
//   * an absent key estimates a small value; empty sketch estimates 0;
//   * merging two sketches sums the per-key counts; estimation is deterministic.
#include "maz/core/CountMinSketch.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

using maz::core::CountMinSketch;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. Never-underestimate + accuracy over a known stream. ---
    {
        CountMinSketch cms(4096, 5);
        std::map<std::string, std::uint64_t> truth;
        // 500 keys, key i occurs (i+1) times -> a skewed distribution with clear heavy hitters.
        for (int i = 0; i < 500; ++i) {
            const std::string k = "k" + std::to_string(i);
            const std::uint64_t n = static_cast<std::uint64_t>(i + 1);
            cms.add(k, n);
            truth[k] += n;
        }
        bool neverUnder = true;
        int exact = 0;
        std::uint64_t worstOver = 0;
        for (const auto& kv : truth) {
            const std::uint64_t est = cms.estimate(kv.first);
            if (est < kv.second) neverUnder = false;
            if (est == kv.second) ++exact;
            worstOver = std::max(worstOver, est - kv.second);
        }
        CHECK(neverUnder, "estimate is never below the true count (one-sided error)");
        CHECK(exact >= 450, "with a wide table the vast majority of keys are exact");
        // total = sum 1..500 = 125250; the CM bound is ~ e/w * total ~ 83. Allow generous slack.
        CHECK(worstOver < 400, "the overestimate stays small relative to the stream total");
    }

    // --- 2. Heavy hitter dominates and is recovered closely. ---
    {
        CountMinSketch cms(2048, 4);
        cms.add("boss", 100000);
        for (int i = 0; i < 3000; ++i) cms.add("mob" + std::to_string(i), 1);
        const std::uint64_t est = cms.estimate("boss");
        CHECK(est >= 100000, "heavy hitter not underestimated");
        CHECK(est - 100000 < 100000 / 20, "heavy hitter recovered within a few percent");
    }

    // --- 3. Absent key + empty sketch. ---
    {
        CountMinSketch cms(4096, 5);
        for (int i = 0; i < 100; ++i) cms.add("present" + std::to_string(i), 3);
        // An absent key can only be inflated by collisions; with a wide table it stays small.
        CHECK(cms.estimate("definitely_absent_key_xyz") <= 12, "absent key estimates a small value");

        CountMinSketch empty(1024, 4);
        CHECK(empty.estimate("anything") == 0, "empty sketch estimates 0");
    }

    // --- 4. Merge sums per-key counts. ---
    {
        CountMinSketch a(2048, 4), b(2048, 4);
        a.add("x", 40);
        b.add("x", 60);
        b.add("y", 25);
        a.merge(b);
        CHECK(a.estimate("x") >= 100 && a.estimate("x") < 100 + 20, "merge sums counts for shared key");
        CHECK(a.estimate("y") >= 25 && a.estimate("y") < 25 + 20, "merge brings in the other sketch's keys");
        CHECK(a.total() == 125, "merged total is the sum of stream totals");
    }

    // --- 5. Determinism. ---
    {
        CountMinSketch a(1024, 4), b(1024, 4);
        for (int i = 0; i < 2000; ++i) {
            const std::string k = "e" + std::to_string(i % 300);
            a.add(k);
            b.add(k);
        }
        bool same = true;
        for (int i = 0; i < 300; ++i) {
            const std::string k = "e" + std::to_string(i);
            if (a.estimate(k) != b.estimate(k)) same = false;
        }
        CHECK(same, "same stream -> identical estimates");
    }

    if (g_fail == 0) {
        std::printf("countminsketch: OK — one-sided error, accuracy, heavy hitter, absent, merge, determinism.\n");
        return 0;
    }
    std::printf("countminsketch: %d failure(s).\n", g_fail);
    return 1;
}
