// tests/core/segmenttree.cpp — verifies dynamic range queries + point updates (core SegmentTree.hpp).
// Ground truths, deterministic (checked against brute-force scans over the same array):
//   * sum tree: range sums match a linear scan, before and after point updates;
//   * min tree and max tree: range extrema match a linear scan, before and after updates;
//   * queryAll matches the whole-array reduction; get() reads the current leaf;
//   * empty/invalid ranges and an over-long upper bound behave as documented;
//   * a randomized (deterministic LCG) update/query stress test agrees with brute force every step.
#include "maz/core/SegmentTree.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct MinOp { int operator()(int a, int b) const { return b < a ? b : a; } };
struct MaxOp { int operator()(int a, int b) const { return b > a ? b : a; } };

// Brute-force references over a plain vector.
static long bruteSum(const std::vector<int>& v, std::size_t l, std::size_t r) {
    long s = 0;
    for (std::size_t i = l; i <= r; ++i) s += v[i];
    return s;
}
static int bruteMin(const std::vector<int>& v, std::size_t l, std::size_t r) {
    int m = std::numeric_limits<int>::max();
    for (std::size_t i = l; i <= r; ++i) m = v[i] < m ? v[i] : m;
    return m;
}
static int bruteMax(const std::vector<int>& v, std::size_t l, std::size_t r) {
    int m = std::numeric_limits<int>::min();
    for (std::size_t i = l; i <= r; ++i) m = v[i] > m ? v[i] : m;
    return m;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

int main() {
    using maz::core::SegmentTree;

    // --- 1. Sum tree matches brute force, before and after updates. ---
    {
        std::vector<int> v{5, 3, 8, 1, 9, 2, 7, 4};
        SegmentTree<int> st(v, 0);
        CHECK(st.query(0, 7) == bruteSum(v, 0, 7), "full-range sum");
        CHECK(st.query(2, 5) == bruteSum(v, 2, 5), "interior sum");
        CHECK(st.query(3, 3) == v[3], "single-element sum");
        CHECK(st.queryAll() == bruteSum(v, 0, 7), "queryAll == full sum");
        v[4] = -6;
        st.update(4, -6);
        CHECK(st.get(4) == -6, "get reflects the update");
        CHECK(st.query(0, 7) == bruteSum(v, 0, 7), "sum after update");
        CHECK(st.query(4, 6) == bruteSum(v, 4, 6), "range spanning the updated index");
    }

    // --- 2. Min and max trees. ---
    {
        std::vector<int> v{5, 3, 8, 1, 9, 2, 7, 4};
        SegmentTree<int, MinOp> mn(v, std::numeric_limits<int>::max());
        SegmentTree<int, MaxOp> mx(v, std::numeric_limits<int>::min());
        CHECK(mn.query(0, 7) == bruteMin(v, 0, 7), "range min");
        CHECK(mx.query(0, 7) == bruteMax(v, 0, 7), "range max");
        CHECK(mn.query(1, 4) == 1, "min of {3,8,1,9} is 1");
        CHECK(mx.query(5, 7) == 7, "max of {2,7,4} is 7");
        v[3] = 12;
        mn.update(3, 12);
        mx.update(3, 12);
        CHECK(mn.query(1, 4) == bruteMin(v, 1, 4), "min after raising the former minimum");
        CHECK(mx.query(0, 7) == 12, "max after update is the new peak");
    }

    // --- 3. Edge cases. ---
    {
        SegmentTree<int> empty(std::size_t{0}, 0);
        CHECK(empty.query(0, 5) == 0, "query on empty tree returns identity");
        CHECK(empty.queryAll() == 0, "queryAll on empty tree returns identity");
        std::vector<int> v{10, 20, 30};
        SegmentTree<int> st(v, 0);
        CHECK(st.query(2, 1) == 0, "inverted range returns identity");
        CHECK(st.query(5, 9) == 0, "range past the end returns identity");
        CHECK(st.query(1, 100) == bruteSum(v, 1, 2), "over-long upper bound is clamped");
        SegmentTree<int> one(std::size_t{1}, 0);
        one.update(0, 42);
        CHECK(one.query(0, 0) == 42 && one.queryAll() == 42, "single-element tree works");
    }

    // --- 4. Randomized stress vs brute force. ---
    {
        const std::size_t n = 64;
        std::vector<int> v(n, 0);
        SegmentTree<int> st(v, 0);
        SegmentTree<int, MinOp> mn(v, std::numeric_limits<int>::max());
        Lcg rng{0xDEADBEEFu};
        for (std::size_t i = 0; i < n; ++i) {
            const int val = static_cast<int>(rng.next() % 1000u) - 500;
            v[i] = val;
            st.update(i, val);
            mn.update(i, val);
        }
        bool ok = true;
        for (int iter = 0; iter < 3000; ++iter) {
            const std::size_t a = rng.next() % n;
            const std::size_t b = rng.next() % n;
            const std::size_t l = a < b ? a : b;
            const std::size_t r = a < b ? b : a;
            if (st.query(l, r) != bruteSum(v, l, r)) ok = false;
            if (mn.query(l, r) != bruteMin(v, l, r)) ok = false;
            // Occasionally mutate a cell.
            if ((rng.next() & 3u) == 0u) {
                const std::size_t idx = rng.next() % n;
                const int val = static_cast<int>(rng.next() % 1000u) - 500;
                v[idx] = val;
                st.update(idx, val);
                mn.update(idx, val);
            }
        }
        CHECK(ok, "randomized sum/min queries agree with brute force through interleaved updates");
    }

    if (g_fail == 0) {
        std::printf("segmenttree: OK — sum/min/max range queries, point updates, edges, random stress.\n");
        return 0;
    }
    std::printf("segmenttree: %d failure(s).\n", g_fail);
    return 1;
}
