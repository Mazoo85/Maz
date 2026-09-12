// tests/core/shufflebag.cpp — verifies the fair "deal from a bag" randomizer (core::ShuffleBag<T>).
// Ground truths: each cycle deals every outcome EXACTLY its intended count (cycle fairness), so over K cycles
// each item appears exactly K*count times; a single cycle is a permutation of the contents; weighted counts
// are honoured per cycle; remaining()/totalCount() track the state; with avoid-immediate-repeat on and all
// distinct outcomes there are NEVER two identical draws in a row (including across refills); the sequence is
// deterministic for a fixed seed; and an empty bag is safe. Checked against the without-replacement definition.
#include "maz/core/ShuffleBag.hpp"
#include "maz/core/Pcg32.hpp"

#include <cstdio>
#include <map>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::Pcg32;
using maz::core::ShuffleBag;

int main() {
    // --- 1. Cycle fairness: each of A,B,C appears exactly once per 3 draws; K cycles -> K each. ---
    {
        ShuffleBag<int> bag;
        bag.add(1);
        bag.add(2);
        bag.add(3);
        CHECK(bag.totalCount() == 3, "three items in the bag");
        Pcg32 rng(42, 7);
        const int cycles = 500;
        std::map<int, int> counts;
        for (int c = 0; c < cycles; ++c) {
            std::map<int, int> thisCycle;
            for (int k = 0; k < 3; ++k) {
                const int v = bag.next(rng);
                ++counts[v];
                ++thisCycle[v];
            }
            CHECK(thisCycle[1] == 1 && thisCycle[2] == 1 && thisCycle[3] == 1, "each cycle is a permutation");
        }
        CHECK(counts[1] == cycles && counts[2] == cycles && counts[3] == cycles, "each item appears K times total");
    }

    // --- 2. Weighted counts honoured per cycle (A x2, B x1). ---
    {
        ShuffleBag<char> bag;
        bag.add('A', 2);
        bag.add('B', 1);
        CHECK(bag.totalCount() == 3, "weighted bag has 3 tokens");
        Pcg32 rng(1, 1);
        for (int c = 0; c < 300; ++c) {
            int a = 0, b = 0;
            for (int k = 0; k < 3; ++k) {
                const char v = bag.next(rng);
                if (v == 'A') ++a; else if (v == 'B') ++b;
            }
            CHECK(a == 2 && b == 1, "A twice, B once every cycle");
        }
    }

    // --- 3. remaining()/totalCount() track the cycle. ---
    {
        ShuffleBag<int> bag;
        bag.add(10);
        bag.add(20);
        Pcg32 rng(9, 9);
        CHECK(bag.remaining() == 0, "starts with an empty current cycle");
        bag.next(rng);
        CHECK(bag.remaining() == 1, "one left after the first draw");
        bag.next(rng);
        CHECK(bag.remaining() == 0, "cycle exhausted after the second draw");
        bag.next(rng);
        CHECK(bag.remaining() == 1 && bag.totalCount() == 2, "next draw refilled the cycle");
    }

    // --- 4. Avoid-immediate-repeat: distinct outcomes never repeat back-to-back (incl. across refills). ---
    {
        ShuffleBag<int> bag(true);
        for (int i = 0; i < 6; ++i) bag.add(i); // 6 distinct outcomes
        Pcg32 rng(2024, 3);
        int prev = -1;
        bool anyRepeat = false;
        for (int k = 0; k < 6000; ++k) {
            const int v = bag.next(rng);
            if (v == prev) anyRepeat = true;
            prev = v;
        }
        CHECK(!anyRepeat, "no two identical draws in a row when repeats are avoidable");
    }

    // --- 5. Determinism: same seed -> same sequence. ---
    {
        ShuffleBag<int> a, b;
        for (int i = 0; i < 5; ++i) { a.add(i); b.add(i); }
        Pcg32 ra(77, 2), rb(77, 2);
        bool same = true;
        for (int k = 0; k < 200; ++k)
            if (a.next(ra) != b.next(rb)) same = false;
        CHECK(same, "identical seed -> identical deal order");
    }

    // --- 6. Empty bag is safe. ---
    {
        ShuffleBag<int> bag;
        Pcg32 rng(1, 1);
        CHECK(bag.empty() && bag.next(rng) == 0, "empty bag returns a default value without crashing");
    }

    if (g_fail == 0) {
        std::printf("shufflebag: OK — cycle fairness, weighted counts, state tracking, no repeats, deterministic.\n");
        return 0;
    }
    std::printf("shufflebag: %d failure(s).\n", g_fail);
    return 1;
}
