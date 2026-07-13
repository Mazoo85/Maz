// Unit tests for maz::core::Handle + Pool<T>. Exercises the generational
// stale-reference guard: after a slot is destroyed and recycled, the old
// handle must be rejected while the new one resolves. Pure C++, no GPU/display.

#include "maz/core/Pool.hpp"

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
    // --- BASIC CREATE / GET ---------------------------------------------------
    {
        Pool<int> p;
        Handle h = p.create(42);
        check(p.valid(h), "create returns a valid handle");
        check(p.get(h) != nullptr && *p.get(h) == 42, "get returns the stored value");
        check(!p.valid(Handle::null()), "null handle is not valid");
        check(p.get(Handle::null()) == nullptr, "get(null) returns nullptr");
    }

    // --- DISTINCT HANDLES / SIZE ---------------------------------------------
    {
        Pool<int> p;
        check(p.size() == 0, "empty pool size == 0");
        Handle a = p.create(10);
        Handle b = p.create(20);
        check(a != b, "two creates give distinct handles");
        check(p.get(a) != nullptr && p.get(b) != nullptr && *p.get(a) != *p.get(b),
              "two creates give distinct objects");
        check(p.size() == 2, "size() tracks the live count");
    }

    // --- DESTROY --------------------------------------------------------------
    {
        Pool<int> p;
        Handle h = p.create(7);
        check(p.size() == 1, "size() == 1 before destroy");
        check(p.destroy(h), "destroy returns true for a live handle");
        check(!p.valid(h), "handle invalid after destroy");
        check(p.get(h) == nullptr, "get(h) == nullptr after destroy");
        check(!p.destroy(h), "second destroy(h) returns false");
        check(p.size() == 0, "size() decrements after destroy");
    }

    // --- SLOT REUSE + STALE (KEY gate) ---------------------------------------
    {
        Pool<int> p;
        Handle a = p.create(111);
        p.destroy(a);
        Handle b = p.create(222);
        check(b.index == a.index, "freed slot is reused (same index)");
        check(b.generation != a.generation, "reused slot has a different generation");
        check(!p.valid(a), "old handle is stale after reuse");
        check(p.get(a) == nullptr, "get(stale) returns nullptr");
        check(p.valid(b), "new handle is valid");
        check(p.get(b) != nullptr && *p.get(b) == 222, "new handle resolves to new value");
    }

    // --- OUT-OF-RANGE / GARBAGE HANDLE ---------------------------------------
    {
        Pool<int> p;
        p.create(1);
        Handle garbage{9999u, 1u};
        check(!p.valid(garbage), "out-of-range handle is not valid");
        check(p.get(garbage) == nullptr, "get(out-of-range) returns nullptr (no crash)");
    }

    // --- MANY CREATE/DESTROY CYCLES ------------------------------------------
    {
        Pool<int> p;
        Handle handles[100];
        for (int i = 0; i < 100; ++i) {
            handles[i] = p.create(i);
        }
        check(p.size() == 100, "100 live objects after 100 creates");
        for (int i = 0; i < 100; i += 2) {
            p.destroy(handles[i]);  // destroy the even-index ones
        }
        bool oddOk = true;
        bool evenStale = true;
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0) {
                if (p.valid(handles[i]) || p.get(handles[i]) != nullptr) {
                    evenStale = false;
                }
            } else {
                const int* v = p.get(handles[i]);
                if (!p.valid(handles[i]) || v == nullptr || *v != i) {
                    oddOk = false;
                }
            }
        }
        check(oddOk, "odd-index handles stay valid with the right value");
        check(evenStale, "even-index handles are stale (invalid, get nullptr)");
        check(p.size() == 50, "size() == 50 after destroying half");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        Pool<int> p;
        Handle a = p.create(5);
        Handle b = p.create(6);
        p.clear();
        check(p.size() == 0, "size() == 0 after clear()");
        check(!p.valid(a) && !p.valid(b), "previously-live handles invalid after clear()");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
