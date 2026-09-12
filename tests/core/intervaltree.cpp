// tests/core/intervaltree.cpp — verifies the interval tree (core IntervalTree.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a small hand-built set answers point and range queries with exactly the expected payloads;
//   * touching/closed endpoints are inclusive (low <= x <= high);
//   * on thousands of random interval sets, queryPoint and queryOverlap return EXACTLY the same set of
//     payloads as a brute-force linear scan (order-independent);
//   * empty tree and single interval behave.
#include "maz/core/IntervalTree.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::core::IntervalTree;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

static std::vector<int> sorted(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    return v;
}

int main() {
    // --- 1. Hand-built set. ---
    {
        IntervalTree<int> t;
        t.insert(1.0, 5.0, 0);   // [1,5]
        t.insert(3.0, 8.0, 1);   // [3,8]
        t.insert(6.0, 10.0, 2);  // [6,10]
        t.insert(12.0, 15.0, 3); // [12,15]
        t.build();
        CHECK(sorted(t.queryPoint(4.0)) == (std::vector<int>{0, 1}), "point 4 hits [1,5] and [3,8]");
        CHECK(sorted(t.queryPoint(7.0)) == (std::vector<int>{1, 2}), "point 7 hits [3,8] and [6,10]");
        CHECK(t.queryPoint(11.0).empty(), "point 11 hits nothing");
        CHECK(sorted(t.queryOverlap(5.0, 6.0)) == (std::vector<int>{0, 1, 2}), "range [5,6] hits three");
        CHECK(sorted(t.queryOverlap(0.0, 20.0)) == (std::vector<int>{0, 1, 2, 3}), "wide range hits all");
        CHECK(t.queryOverlap(-5.0, 0.5).empty(), "range fully left hits none");
    }

    // --- 2. Inclusive endpoints. ---
    {
        IntervalTree<int> t;
        t.insert(2.0, 4.0, 7);
        t.build();
        CHECK(sorted(t.queryPoint(2.0)) == (std::vector<int>{7}), "low endpoint inclusive");
        CHECK(sorted(t.queryPoint(4.0)) == (std::vector<int>{7}), "high endpoint inclusive");
        CHECK(t.queryPoint(4.0001).empty(), "just past high excluded");
        CHECK(sorted(t.queryOverlap(4.0, 9.0)) == (std::vector<int>{7}), "touching range overlaps");
    }

    // --- 3. Randomized vs brute force. ---
    {
        Lcg r{0x117E5u};
        bool ok = true;
        long totalHits = 0;
        for (int trial = 0; trial < 3000 && ok; ++trial) {
            const int n = static_cast<int>(r.next() % 60u);
            std::vector<double> lo(static_cast<std::size_t>(n)), hi(static_cast<std::size_t>(n));
            IntervalTree<int> t;
            for (int i = 0; i < n; ++i) {
                const double a = static_cast<double>(r.next() % 100u);
                const double b = a + static_cast<double>(r.next() % 30u);
                lo[static_cast<std::size_t>(i)] = a;
                hi[static_cast<std::size_t>(i)] = b;
                t.insert(a, b, i);
            }
            t.build();

            // A few point + range queries per set, checked against a linear scan.
            for (int q = 0; q < 6 && ok; ++q) {
                const double qa = static_cast<double>(r.next() % 130u) - 5.0;
                const double qb = qa + static_cast<double>(r.next() % 20u);

                std::vector<int> bruteP, bruteR;
                for (int i = 0; i < n; ++i) {
                    const double L = lo[static_cast<std::size_t>(i)], H = hi[static_cast<std::size_t>(i)];
                    if (L <= qa && qa <= H) bruteP.push_back(i);
                    if (L <= qb && H >= qa) bruteR.push_back(i);
                }
                if (sorted(t.queryPoint(qa)) != sorted(bruteP)) { ok = false; break; }
                if (sorted(t.queryOverlap(qa, qb)) != sorted(bruteR)) { ok = false; break; }
                totalHits += static_cast<long>(bruteP.size() + bruteR.size());
            }
        }
        CHECK(ok, "interval tree matches brute force across random sets and queries");
        CHECK(totalHits > 2000, "the random trials actually produced many hits");
    }

    // --- 4. Empty + single. ---
    {
        IntervalTree<int> t;
        t.build();
        CHECK(t.empty() && t.queryPoint(0.0).empty(), "empty tree queries empty");
        t.insert(5.0, 5.0, 42); // degenerate point interval
        t.build();
        CHECK(sorted(t.queryPoint(5.0)) == (std::vector<int>{42}), "degenerate [5,5] contains 5");
        CHECK(t.queryPoint(5.5).empty(), "degenerate interval excludes other points");
    }

    if (g_fail == 0) {
        std::printf("intervaltree: OK — hand set, inclusive ends, 3000-trial brute-force parity, edges.\n");
        return 0;
    }
    std::printf("intervaltree: %d failure(s).\n", g_fail);
    return 1;
}
