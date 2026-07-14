// Unit tests for maz::core::LruCache<K,V> — a fixed-capacity least-recently-used
// cache. Exercises put/get basics, LRU eviction over capacity, the load-bearing
// promotion semantics (get promotes to MRU -> a different victim; peek does NOT
// promote), put-on-existing updating the value and promoting, erase/clear/empty
// bookkeeping, the capacity-1 edge, and string-valued get returning a live,
// mutable pointer into storage. Pure C++, no GPU/display.

#include "maz/core/LruCache.hpp"

#include <cstdio>
#include <string>

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
    // --- put/get basic -------------------------------------------------------
    {
        LruCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);
        check(c.get(1) != nullptr && *c.get(1) == 10, "get(1)==10");
        check(c.get(2) != nullptr && *c.get(2) == 20, "get(2)==20");
        check(c.size() == 2, "size==2");
        check(c.capacity() == 2, "capacity==2");
        check(c.contains(1) && c.contains(2), "contains 1 and 2");
        check(c.get(99) == nullptr, "get(99) miss -> nullptr");
    }

    // --- evict LRU over capacity ---------------------------------------------
    {
        LruCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);
        c.put(3, 30);  // overflow: 1 is LRU -> evicted
        check(c.size() == 2, "size==2 after overflow");
        check(c.contains(1) == false, "1 evicted (was LRU)");
        check(c.contains(2) && c.contains(3), "2 and 3 present");
        check(c.get(2) != nullptr && *c.get(2) == 20, "get(2)==20");
        check(c.get(3) != nullptr && *c.get(3) == 30, "get(3)==30");
    }

    // --- get promotes -> different victim ------------------------------------
    {
        LruCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);
        (void)c.get(1);  // promotes 1 to MRU -> 2 becomes LRU
        c.put(3, 30);    // evicts 2, not 1
        check(c.contains(2) == false, "2 evicted (get promoted 1)");
        check(c.contains(1) == true, "1 survives (was promoted)");
        check(c.contains(3) == true, "3 present");
    }

    // --- put existing updates value + promotes -------------------------------
    {
        LruCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);
        c.put(1, 99);  // update 1, promotes it to MRU
        check(c.size() == 2, "size==2 (no growth on update)");
        check(c.get(1) != nullptr && *c.get(1) == 99, "get(1)==99 (updated)");
        c.put(3, 30);  // 2 is LRU (1 was just promoted) -> evicted
        check(c.contains(2) == false, "2 evicted after update-promote");
        check(c.contains(1) == true, "1 survives");
    }

    // --- peek does NOT promote -----------------------------------------------
    {
        LruCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);
        (void)c.peek(1);  // read without promoting -> 1 stays LRU
        c.put(3, 30);     // evicts 1 (peek didn't save it)
        check(c.contains(1) == false, "1 evicted (peek did not promote)");
        check(c.contains(2) == true, "2 present");
        check(c.contains(3) == true, "3 present");
        check(c.peek(99) == nullptr, "peek(99) miss -> nullptr");
    }

    // --- erase / clear / empty -----------------------------------------------
    {
        LruCache<int, int> c(3);
        c.put(1, 10);
        c.put(2, 20);
        c.put(3, 30);
        check(c.erase(1) == true, "erase(1) -> true");
        check(c.contains(1) == false, "1 gone after erase");
        check(c.size() == 2, "size decremented after erase");
        check(c.erase(99) == false, "erase(99) absent -> false");
        c.clear();
        check(c.empty() == true, "empty after clear");
        check(c.size() == 0, "size==0 after clear");
        check(c.get(2) == nullptr, "get after clear -> nullptr");
    }

    // --- capacity 1 edge -----------------------------------------------------
    {
        LruCache<int, int> c(1);
        c.put(1, 10);
        c.put(2, 20);  // 1 evicted immediately
        check(c.size() == 1, "size==1 at capacity 1");
        check(c.contains(1) == false, "1 evicted at capacity 1");
        check(c.get(2) != nullptr && *c.get(2) == 20, "get(2)==20");
    }

    // --- string values + get pointer mutation --------------------------------
    {
        LruCache<int, std::string> c(2);
        c.put(1, "a");
        check(c.get(1) != nullptr && *c.get(1) == "a", "string get(1)==a");
        *c.get(1) = "z";  // mutate through the live pointer
        check(c.get(1) != nullptr && *c.get(1) == "z", "string get(1)==z after mutation");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
