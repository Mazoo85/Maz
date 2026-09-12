// tests/core/weightedreservoir.cpp — verifies weighted reservoir sampling (core WeightedReservoir.hpp).
// Ground truths, deterministic (seeded per trial; no <random>, no clock):
//   * k=1 over items with weights {1,2,3,4}: selection frequency is proportional to weight (~10/20/30/40%);
//   * k>=n returns every item;
//   * a hugely-dominant weight is almost always included in the sample;
//   * the same seed reproduces the same sample; non-positive weights are ignored;
//   * the reservoir never holds more than k items.
#include "maz/core/WeightedReservoir.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using maz::core::WeightedReservoir;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. k=1 selection frequency proportional to weight. ---
    {
        const double w[4] = {1.0, 2.0, 3.0, 4.0}; // total 10 -> expect 0.1, 0.2, 0.3, 0.4
        const int N = 40000;
        int counts[4] = {0, 0, 0, 0};
        for (int trial = 0; trial < N; ++trial) {
            WeightedReservoir<int> wr(1, static_cast<std::uint64_t>(trial) * 2654435761ull + 12345ull);
            for (int i = 0; i < 4; ++i) wr.add(i, w[i]);
            const std::vector<int> s = wr.sample();
            if (s.size() == 1) ++counts[s[0]];
        }
        bool ok = true;
        for (int i = 0; i < 4; ++i) {
            const double freq = static_cast<double>(counts[i]) / static_cast<double>(N);
            const double expected = w[i] / 10.0;
            if (freq < expected - 0.02 || freq > expected + 0.02) ok = false;
        }
        CHECK(ok, "k=1 selection frequency tracks the weights (10/20/30/40%)");
    }

    // --- 2. k >= n returns everything. ---
    {
        WeightedReservoir<int> wr(10, 1u);
        for (int i = 0; i < 5; ++i) wr.add(i, static_cast<double>(i + 1));
        CHECK(wr.size() == 5, "with k>=n all items are kept");
        std::vector<char> seen(5, 0);
        for (int c : wr.sample()) if (c >= 0 && c < 5) seen[static_cast<std::size_t>(c)] = 1;
        bool all = true;
        for (char s : seen) if (!s) all = false;
        CHECK(all, "the sample contains every item");
    }

    // --- 3. Dominant weight is almost always included. ---
    {
        int included = 0;
        const int N = 2000;
        for (int trial = 0; trial < N; ++trial) {
            WeightedReservoir<int> wr(2, static_cast<std::uint64_t>(trial) * 40503ull + 7ull);
            wr.add(0, 1000.0); // dominant
            for (int i = 1; i < 8; ++i) wr.add(i, 1.0);
            for (int c : wr.sample()) if (c == 0) ++included;
        }
        CHECK(included > static_cast<int>(0.97 * N), "the dominant-weight item is picked almost every time");
    }

    // --- 4. Determinism + non-positive weights ignored. ---
    {
        WeightedReservoir<int> a(3, 999u), b(3, 999u);
        for (int i = 0; i < 20; ++i) { a.add(i, static_cast<double>(i + 1)); b.add(i, static_cast<double>(i + 1)); }
        const std::vector<int> sa = a.sample(), sb = b.sample();
        CHECK(sa == sb, "same seed + same stream -> identical sample");

        WeightedReservoir<int> wr(5, 3u);
        wr.add(1, 0.0);
        wr.add(2, -4.0);
        wr.add(3, 2.0);
        CHECK(wr.size() == 1, "zero and negative weights are ignored");
    }

    // --- 5. Never exceeds k. ---
    {
        WeightedReservoir<int> wr(4, 5u);
        for (int i = 0; i < 1000; ++i) wr.add(i, 1.0);
        CHECK(wr.size() == 4, "reservoir holds at most k items");
    }

    if (g_fail == 0) {
        std::printf("weightedreservoir: OK — weight-proportional, k>=n, dominant, determinism, cap.\n");
        return 0;
    }
    std::printf("weightedreservoir: %d failure(s).\n", g_fail);
    return 1;
}
