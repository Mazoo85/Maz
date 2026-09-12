// tests/core/radixsort.cpp — verifies linear-time radix sorting (core RadixSort.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * radixSort(uint32) and radixSort(uint64) match std::sort on thousands of random arrays;
//   * radixSortByKey is STABLE — equal keys keep their original order (matches std::stable_sort by key);
//   * radixSortFloats matches std::sort including negatives, zero, and +/- values;
//   * edge cases (empty, single, already-sorted, reverse) are handled.
#include "maz/core/RadixSort.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::core::radixSort;
using maz::core::radixSortByKey;
using maz::core::radixSortFloats;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::uint64_t next64() { return (static_cast<std::uint64_t>(next()) << 32) | next(); }
};

int main() {
    Lcg rng{0x2AD1705u};

    // --- 1. uint32 vs std::sort. ---
    {
        bool ok = true;
        for (int trial = 0; trial < 3000 && ok; ++trial) {
            const std::size_t n = rng.next() % 300u;
            std::vector<std::uint32_t> a(n), ref(n);
            for (std::size_t i = 0; i < n; ++i) { a[i] = rng.next(); ref[i] = a[i]; }
            radixSort(a);
            std::sort(ref.begin(), ref.end());
            if (a != ref) ok = false;
        }
        CHECK(ok, "radixSort(uint32) matches std::sort");
    }

    // --- 2. uint64 vs std::sort. ---
    {
        bool ok = true;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            const std::size_t n = rng.next() % 200u;
            std::vector<std::uint64_t> a(n), ref(n);
            for (std::size_t i = 0; i < n; ++i) { a[i] = rng.next64(); ref[i] = a[i]; }
            radixSort(a);
            std::sort(ref.begin(), ref.end());
            if (a != ref) ok = false;
        }
        CHECK(ok, "radixSort(uint64) matches std::sort");
    }

    // --- 3. Stability: equal keys keep original order. ---
    {
        struct Item { std::uint32_t key; int seq; };
        bool ok = true;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            const std::size_t n = rng.next() % 200u;
            std::vector<Item> a(n), ref(n);
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint32_t k = rng.next() % 16u; // small key domain -> many ties
                a[i] = Item{k, static_cast<int>(i)};
                ref[i] = a[i];
            }
            radixSortByKey(a, [](const Item& it) { return it.key; });
            std::stable_sort(ref.begin(), ref.end(),
                             [](const Item& x, const Item& y) { return x.key < y.key; });
            for (std::size_t i = 0; i < n && ok; ++i)
                if (a[i].key != ref[i].key || a[i].seq != ref[i].seq) ok = false;
        }
        CHECK(ok, "radixSortByKey is stable (matches std::stable_sort by key)");
    }

    // --- 4. Floats including negatives / zero. ---
    {
        bool ok = true;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            const std::size_t n = rng.next() % 200u;
            std::vector<float> a(n), ref(n);
            for (std::size_t i = 0; i < n; ++i) {
                const float f = (static_cast<float>(static_cast<std::int32_t>(rng.next())) / 65536.0f);
                a[i] = f; ref[i] = f;
            }
            radixSortFloats(a);
            std::sort(ref.begin(), ref.end());
            if (a != ref) ok = false;
        }
        CHECK(ok, "radixSortFloats matches std::sort with negatives and zero");
    }

    // --- 5. Edge cases. ---
    {
        std::vector<std::uint32_t> empty;
        radixSort(empty);
        CHECK(empty.empty(), "empty array stays empty");

        std::vector<std::uint32_t> one{42u};
        radixSort(one);
        CHECK(one.size() == 1 && one[0] == 42u, "single element unchanged");

        std::vector<std::uint32_t> sorted{1u, 2u, 3u, 4u, 5u};
        radixSort(sorted);
        CHECK((sorted == std::vector<std::uint32_t>{1u, 2u, 3u, 4u, 5u}), "already sorted stays sorted");

        std::vector<std::uint32_t> rev{5u, 4u, 3u, 2u, 1u};
        radixSort(rev);
        CHECK((rev == std::vector<std::uint32_t>{1u, 2u, 3u, 4u, 5u}), "reverse gets sorted");

        std::vector<float> fedge{0.0f, -0.0f, -1.5f, 1.5f, -100.0f, 100.0f, 0.25f};
        std::vector<float> fref = fedge;
        radixSortFloats(fedge);
        std::sort(fref.begin(), fref.end());
        CHECK(fedge == fref, "float edge values sort correctly");
    }

    if (g_fail == 0) {
        std::printf("radixsort: OK — uint32/uint64 vs std::sort, stability, floats, edge cases.\n");
        return 0;
    }
    std::printf("radixsort: %d failure(s).\n", g_fail);
    return 1;
}
