// Unit tests for maz::core::SparseSet<T>. Exercises the sparse-set component
// map: O(1) insert/remove/contains/get, the swap-and-pop remove (including the
// d==last self-remove case), and dense-iteration parity between keys()/values().
// A fixed-seed churn/fuzz loop cross-checks against a plain shadow array. Pure
// C++, no GPU/display.

#include "maz/core/SparseSet.hpp"
#include "maz/core/Random.hpp"

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
        SparseSet<int> set;
        check(set.size() == 0, "empty set size == 0");
        check(set.empty(), "empty set empty() == true");
        check(!set.contains(0), "empty set does not contain 0");
        check(!set.contains(99999), "empty set does not contain 99999 (no crash)");
        check(set.get(5) == nullptr, "get(5) == nullptr on empty set");
        check(!set.remove(5), "remove(5) == false on empty set");
    }

    // --- INSERT -> GET --------------------------------------------------------
    {
        SparseSet<int> set;
        check(set.insert(10, 100), "insert(10,100) returns true (new key)");
        check(set.contains(10), "contains(10) after insert");
        check(set.get(10) != nullptr && *set.get(10) == 100, "get(10) resolves to 100");
        check(set.size() == 1, "size() == 1 after one insert");
    }

    // --- OVERWRITE ------------------------------------------------------------
    {
        SparseSet<int> set;
        set.insert(10, 100);
        check(!set.insert(10, 999), "insert(10,999) returns false (overwrite)");
        check(set.get(10) != nullptr && *set.get(10) == 999, "get(10) resolves to 999 after overwrite");
        check(set.size() == 1, "size() stays 1 after overwrite");
    }

    // --- NON-CONTIGUOUS / LARGE KEYS ------------------------------------------
    {
        SparseSet<int> set;
        set.insert(0, 7);
        set.insert(1000, 8);
        set.insert(50, 9);
        bool ok = set.contains(0) && set.get(0) != nullptr && *set.get(0) == 7
               && set.contains(1000) && set.get(1000) != nullptr && *set.get(1000) == 8
               && set.contains(50) && set.get(50) != nullptr && *set.get(50) == 9;
        check(ok, "non-contiguous/large keys all resolve to their values");
        check(set.size() == 3, "size() == 3 with three spread-out keys");
        check(!set.contains(999), "contains(999) == false (gap between keys)");
    }

    // --- REMOVE MIDDLE (swap-and-pop KEY gate) --------------------------------
    {
        SparseSet<int> set;
        for (int k = 1; k <= 5; ++k) {
            set.insert(static_cast<uint32_t>(k), k * 10);
        }
        check(set.remove(2), "remove(2) returns true");
        check(!set.contains(2) && set.get(2) == nullptr, "key 2 gone after remove");
        bool survivorsOk = true;
        const uint32_t survivors[] = {1u, 3u, 4u, 5u};
        for (uint32_t k : survivors) {
            if (!set.contains(k) || set.get(k) == nullptr || *set.get(k) != static_cast<int>(k) * 10) {
                survivorsOk = false;
            }
        }
        check(survivorsOk, "survivors {1,3,4,5} still resolve to k*10 after remove(2)");
        check(set.size() == 4, "size() == 4 after removing one of five");
    }

    // --- REMOVE LAST (d == last self-remove) ----------------------------------
    {
        SparseSet<int> set;
        set.insert(1, 10);
        set.insert(2, 20);
        set.insert(3, 30);
        check(set.remove(3), "remove(3) (the last dense element) returns true");
        check(!set.contains(3), "key 3 gone after remove");
        check(set.contains(1) && set.get(1) != nullptr && *set.get(1) == 10, "key 1 intact after remove(last)");
        check(set.contains(2) && set.get(2) != nullptr && *set.get(2) == 20, "key 2 intact after remove(last)");
        check(set.size() == 2, "size() == 2 after removing the last element");
    }

    // --- REMOVE ALL / RE-INSERT -----------------------------------------------
    {
        SparseSet<int> set;
        for (int k = 1; k <= 5; ++k) {
            set.insert(static_cast<uint32_t>(k), k * 10);
        }
        bool drainOk = true;
        int expectedSize = 5;
        for (uint32_t k = 1u; k <= 5u; ++k) {
            if (!set.remove(k)) {
                drainOk = false;
            }
            --expectedSize;
            if (static_cast<int>(set.size()) != expectedSize) {
                drainOk = false;
            }
            // Every not-yet-removed key must still resolve.
            for (uint32_t j = k + 1u; j <= 5u; ++j) {
                if (!set.contains(j) || set.get(j) == nullptr || *set.get(j) != static_cast<int>(j) * 10) {
                    drainOk = false;
                }
            }
        }
        check(drainOk, "removing keys one by one decrements size and keeps survivors valid");
        check(set.size() == 0 && set.empty(), "set is empty after draining all keys");
        check(set.insert(42, 4200), "insert(42,4200) after drain returns true (new key)");
        check(set.get(42) != nullptr && *set.get(42) == 4200, "get(42) resolves to 4200 after re-insert");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        SparseSet<int> set;
        set.insert(1, 11);
        set.insert(2, 22);
        set.insert(1000, 1010);
        set.clear();
        check(set.size() == 0, "size() == 0 after clear()");
        check(set.empty(), "empty() == true after clear()");
        bool allGone = !set.contains(1) && !set.contains(2) && !set.contains(1000);
        check(allGone, "no keys remain after clear()");
        check(set.insert(5, 500), "insert(5,500) after clear returns true");
        check(set.get(5) != nullptr && *set.get(5) == 500, "get(5) resolves to 500 after clear + insert");
    }

    // --- DENSE-ITERATION PARITY -----------------------------------------------
    {
        SparseSet<int> set;
        set.insert(3, 30);
        set.insert(7, 70);
        set.insert(11, 110);
        set.insert(4, 40);
        set.insert(9, 90);
        set.remove(7);
        set.remove(3);
        bool parityOk = true;
        for (std::size_t i = 0; i < set.keys().size(); ++i) {
            uint32_t k = set.keys()[i];
            if (set.get(k) != &set.values()[i] || *set.get(k) != set.values()[i]) {
                parityOk = false;
            }
        }
        check(parityOk, "get(keys()[i]) is &values()[i] with matching value (dense parity)");
        check(set.keys().size() == set.values().size() && set.keys().size() == set.size(),
              "keys()/values()/size() agree on the live count");
    }

    // --- CHURN / FUZZ vs SHADOW -----------------------------------------------
    {
        SparseSet<int> set;
        Rng rng(12345u);
        const uint32_t kN = 256;
        bool present[256] = {};
        int shadowVal[256] = {};
        bool fuzzOk = true;
        for (int i = 0; i < 5000; ++i) {
            uint32_t key = static_cast<uint32_t>(rng.rangeInt(0, static_cast<int>(kN) - 1));
            if (i % 2 == 0) {
                int v = static_cast<int>(key) * 3 + i;
                set.insert(key, v);
                present[key] = true;
                shadowVal[key] = v;
            } else {
                set.remove(key);
                present[key] = false;
            }
            fuzzOk &= (set.contains(key) == present[key]);
            if (present[key]) {
                fuzzOk &= (set.get(key) != nullptr && *set.get(key) == shadowVal[key]);
            } else {
                fuzzOk &= (set.get(key) == nullptr);
            }
        }
        int liveCount = 0;
        for (uint32_t k = 0; k < kN; ++k) {
            if (present[k]) {
                ++liveCount;
            }
        }
        fuzzOk &= (liveCount == static_cast<int>(set.size()));
        for (std::size_t i = 0; i < set.keys().size(); ++i) {
            fuzzOk &= (set.get(set.keys()[i]) != nullptr && *set.get(set.keys()[i]) == set.values()[i]);
        }
        check(fuzzOk, "5000-iter churn stays consistent with the shadow array + dense parity");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
