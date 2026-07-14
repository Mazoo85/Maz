// Unit tests for maz::core sampling helpers (Sampling.hpp) built on the iter4
// PCG Rng: weightedIndex (loot-table draw proportional to non-negative weights,
// single-non-zero-weight is RNG-independent, zero-weight never picked, seed
// determinism, loose proportionality), shuffle (permutation invariant, seed
// determinism, empty/size-1 no-op), sampleWithoutReplacement (distinct+in-range,
// k==n full permutation, k==0 empty, determinism), and gaussian (seed-determinism,
// finite, stddev==0 returns mean). Pure C++, no GPU/display.

#include "maz/core/Sampling.hpp"

#include <cstdio>
#include <vector>
#include <algorithm>  // std::sort, std::find
#include <cmath>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// True if every element of v is distinct.
bool allDistinct(std::vector<std::size_t> v) {
    std::sort(v.begin(), v.end());
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i - 1] == v[i]) { return false; }
    }
    return true;
}

} // namespace

int main() {
    // --- weightedIndex single non-zero (EXACT, RNG-independent) --------------
    {
        Rng r(12345);
        std::vector<float> w = {0.0f, 0.0f, 1.0f, 0.0f};
        bool always2 = true;
        for (int i = 0; i < 20; ++i) {
            if (weightedIndex(r, w) != 2) { always2 = false; }
        }
        check(always2, "weightedIndex {0,0,1,0} always returns 2 regardless of RNG");

        Rng r2(999);
        std::vector<float> w2 = {1.0f, 0.0f, 0.0f};
        bool always0 = true;
        for (int i = 0; i < 20; ++i) {
            if (weightedIndex(r2, w2) != 0) { always0 = false; }
        }
        check(always0, "weightedIndex {1,0,0} always returns 0");

        Rng r3(7);
        std::vector<float> w3 = {0.0f, 0.0f, 0.0f, 5.0f};
        bool always3 = true;
        for (int i = 0; i < 20; ++i) {
            if (weightedIndex(r3, w3) != 3) { always3 = false; }
        }
        check(always3, "weightedIndex {0,0,0,5} always returns 3");
    }

    // --- weightedIndex two-way determinism -----------------------------------
    {
        Rng a(7);
        Rng b(7);
        std::vector<float> w = {1.0f, 1.0f};
        bool same = true;
        bool inRange = true;
        for (int i = 0; i < 10; ++i) {
            std::size_t ia = weightedIndex(a, w);
            std::size_t ib = weightedIndex(b, w);
            if (ia != ib) { same = false; }
            if (ia > 1) { inRange = false; }
        }
        check(same, "weightedIndex same seed -> identical draws");
        check(inRange, "weightedIndex {1,1} results are all 0 or 1");
    }

    // --- weightedIndex proportionality (loose statistical) -------------------
    {
        Rng r(2024);
        std::vector<float> w = {1.0f, 0.0f, 0.0f, 9.0f};
        int count[4] = {0, 0, 0, 0};
        for (int i = 0; i < 10000; ++i) {
            count[weightedIndex(r, w)]++;
        }
        check(count[3] > count[0], "weightedIndex: heavy index 3 selected more than light index 0");
        check(count[1] == 0 && count[2] == 0, "weightedIndex: zero-weight indices never selected");
    }

    // --- shuffle is a permutation --------------------------------------------
    {
        std::vector<int> v = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        Rng r(123);
        shuffle(r, v);
        check(v.size() == 10, "shuffle preserves size");
        std::vector<int> sorted = v;
        std::sort(sorted.begin(), sorted.end());
        std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        check(sorted == expected, "shuffle result is a permutation (nothing lost/duplicated)");
    }

    // --- shuffle seed-determinism --------------------------------------------
    {
        std::vector<int> va = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::vector<int> vb = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        Rng a(555);
        Rng b(555);
        shuffle(a, va);
        shuffle(b, vb);
        check(va == vb, "shuffle same seed -> identical permutation");

        std::vector<int> vc = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        Rng c(556);
        shuffle(c, vc);
        check(vc != va, "shuffle different seed -> different permutation (probabilistic)");
    }

    // --- shuffle edge --------------------------------------------------------
    {
        std::vector<int> empty;
        Rng r(1);
        shuffle(r, empty);
        check(empty.empty(), "shuffle empty vector is a no-op");

        std::vector<int> one = {42};
        shuffle(r, one);
        check(one.size() == 1 && one[0] == 42, "shuffle size-1 vector is a no-op");
    }

    // --- sampleWithoutReplacement distinct + in range ------------------------
    {
        Rng r(9);
        std::vector<std::size_t> s = sampleWithoutReplacement(r, 10, 4);
        check(s.size() == 4, "sampleWithoutReplacement returns k elements");
        bool inRange = true;
        for (std::size_t e : s) {
            if (e >= 10) { inRange = false; }
        }
        check(inRange, "sampleWithoutReplacement elements are in [0, n)");
        check(allDistinct(s), "sampleWithoutReplacement elements are distinct");
    }

    // --- sample k==n permutation ---------------------------------------------
    {
        Rng r(9);
        std::vector<std::size_t> s = sampleWithoutReplacement(r, 6, 6);
        check(s.size() == 6, "sample k==n returns n elements");
        std::vector<std::size_t> sorted = s;
        std::sort(sorted.begin(), sorted.end());
        std::vector<std::size_t> expected = {0, 1, 2, 3, 4, 5};
        check(sorted == expected, "sample k==n is a full permutation");

        std::vector<std::size_t> empty = sampleWithoutReplacement(r, 6, 0);
        check(empty.empty(), "sample k==0 returns an empty vector");
    }

    // --- sample determinism --------------------------------------------------
    {
        Rng a(31);
        Rng b(31);
        std::vector<std::size_t> sa = sampleWithoutReplacement(a, 20, 5);
        std::vector<std::size_t> sb = sampleWithoutReplacement(b, 20, 5);
        check(sa == sb, "sampleWithoutReplacement same seed -> identical sample");

        Rng c(32);
        std::vector<std::size_t> sc = sampleWithoutReplacement(c, 20, 5);
        check(sc != sa, "sampleWithoutReplacement different seed -> different sample (probabilistic)");
    }

    // --- gaussian determinism + finite ---------------------------------------
    {
        Rng a(1);
        Rng b(1);
        float ga = gaussian(a);
        float gb = gaussian(b);
        check(ga == gb, "gaussian same seed -> bit-identical sample");
        check(std::isfinite(ga), "gaussian sample is finite");

        Rng c(1);
        float g0 = gaussian(c, 5.0f, 0.0f);
        check(g0 == 5.0f, "gaussian with stddev 0 returns exactly the mean");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
