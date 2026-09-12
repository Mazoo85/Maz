// tests/core/objectpool.cpp — verifies the recycling object pool (core::ObjectPool<T>).
// Ground truths: acquire grows capacity and raises activeCount; get() is mutable; release lowers activeCount
// and marks the slot inactive; the next acquire REUSES the freed slot (no capacity growth) — the defining
// pooling property; recycled slots keep their prior value (pooling reuses storage); a reference from get()
// stays valid as the pool grows (deque-backed); double/out-of-range release is a safe no-op; capacity settles
// at the high-water mark of concurrently-live objects; reset() frees all but keeps capacity; clear() drops it.
#include "maz/core/ObjectPool.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::ObjectPool;

struct Bullet {
    float x = 0, y = 0;
    int id = -1;
};

int main() {
    // --- 1. Acquire grows capacity + active count; get() is mutable. ---
    {
        ObjectPool<Bullet> pool;
        CHECK(pool.capacity() == 0 && pool.activeCount() == 0, "starts empty");
        const std::size_t a = pool.acquire();
        const std::size_t b = pool.acquire();
        CHECK(pool.capacity() == 2 && pool.activeCount() == 2, "two acquires -> capacity 2, active 2");
        pool.get(a).id = 10;
        pool.get(b).id = 20;
        CHECK(pool.get(a).id == 10 && pool.get(b).id == 20, "get() stores per-slot state");
        CHECK(pool.isActive(a) && pool.isActive(b), "both slots active");
    }

    // --- 2. Release then acquire REUSES the freed slot (no new capacity). ---
    {
        ObjectPool<Bullet> pool;
        const std::size_t a = pool.acquire();
        const std::size_t b = pool.acquire();
        pool.release(a);
        CHECK(pool.activeCount() == 1 && !pool.isActive(a), "release lowers active count, marks inactive");
        const std::size_t c = pool.acquire();
        CHECK(c == a, "acquire recycles the just-freed slot index");
        CHECK(pool.capacity() == 2, "recycling did NOT grow capacity");
        CHECK(pool.activeCount() == 2, "active count back to 2");
        (void)b;
    }

    // --- 3. Recycled slot keeps its prior value (pooling reuses storage, doesn't reset). ---
    {
        ObjectPool<Bullet> pool;
        const std::size_t a = pool.acquire();
        pool.get(a).id = 777;
        pool.release(a);
        const std::size_t r = pool.acquire();
        CHECK(r == a && pool.get(r).id == 777, "recycled slot retains its previous value");
    }

    // --- 4. A reference from get() stays valid as the pool grows. ---
    {
        ObjectPool<Bullet> pool;
        const std::size_t a = pool.acquire();
        Bullet& ref = pool.get(a);
        ref.id = 42;
        for (int i = 0; i < 1000; ++i) pool.acquire(); // force many growths
        CHECK(ref.id == 42, "reference into the pool survives growth (stable storage)");
        CHECK(pool.get(a).id == 42, "and get(a) still reads it");
    }

    // --- 5. Double / out-of-range release is a safe no-op. ---
    {
        ObjectPool<Bullet> pool;
        const std::size_t a = pool.acquire();
        pool.release(a);
        const std::size_t before = pool.activeCount();
        pool.release(a);      // already free
        pool.release(9999);   // out of range
        CHECK(pool.activeCount() == before, "double/out-of-range release does not corrupt the count");
        const std::size_t r = pool.acquire();
        CHECK(r == a && pool.activeCount() == 1, "free list not double-counted (single slot recycled)");
    }

    // --- 6. Capacity settles at the high-water mark of concurrent objects. ---
    {
        ObjectPool<Bullet> pool;
        std::vector<std::size_t> live;
        // Churn: repeatedly acquire up to 8 live, then release down to 2, many rounds.
        for (int round = 0; round < 50; ++round) {
            while (live.size() < 8) live.push_back(pool.acquire());
            while (live.size() > 2) { pool.release(live.back()); live.pop_back(); }
        }
        CHECK(pool.capacity() == 8, "capacity never exceeds the peak concurrent count (8)");
        CHECK(pool.activeCount() == live.size(), "active count matches the live set");
    }

    // --- 7. reset() frees all but keeps capacity; clear() drops it. ---
    {
        ObjectPool<Bullet> pool;
        for (int i = 0; i < 5; ++i) pool.acquire();
        pool.reset();
        CHECK(pool.activeCount() == 0 && pool.capacity() == 5, "reset: active 0, capacity kept");
        const std::size_t r = pool.acquire();
        CHECK(pool.capacity() == 5, "acquire after reset recycles (no growth)");
        (void)r;
        pool.clear();
        CHECK(pool.activeCount() == 0 && pool.capacity() == 0, "clear drops storage");
    }

    if (g_fail == 0) {
        std::printf("objectpool: OK — acquire/release, slot reuse, retained state, stable refs, bounded capacity.\n");
        return 0;
    }
    std::printf("objectpool: %d failure(s).\n", g_fail);
    return 1;
}
