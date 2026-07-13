// Unit tests for maz::core::LinearAllocator. Exercises the fixed-capacity
// bump-pointer arena: alignment of returned pointers, exhaustion leaving the
// cursor intact, marker()/rewindTo() stack rewind, and capacity-0 safety.
// Pure C++, no GPU/display.

#include "maz/core/LinearAllocator.hpp"

#include <cstdint>
#include <cstddef>
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
    // --- CONSTRUCT ------------------------------------------------------------
    {
        LinearAllocator a(64);
        check(a.capacity() == 64 && a.used() == 0 && a.remaining() == 64, "fresh arena is empty at full capacity");
    }

    // --- BASIC ALLOCATE -------------------------------------------------------
    {
        LinearAllocator a(64);
        void* p = a.allocate(16, 8);
        check(p != nullptr, "allocate(16,8) succeeds");
        check(reinterpret_cast<std::uintptr_t>(p) % 8 == 0, "returned pointer is 8-aligned");
        check(a.used() >= 16 && a.used() <= 16 + 7, "used() advanced by size plus at most alignment-1 padding");
        check(a.remaining() == a.capacity() - a.used(), "remaining() == capacity() - used()");
    }

    // --- ALIGNMENT PADDING ----------------------------------------------------
    {
        LinearAllocator a(128);
        a.allocate(1, 1);
        void* p = a.allocate(8, 16);
        check(p != nullptr && reinterpret_cast<std::uintptr_t>(p) % 16 == 0, "16-aligned allocation after a 1-byte alloc");
        check(a.used() >= 1 + 8, "padding pushed used() past the raw 9 bytes");
    }

    // --- SEQUENTIAL NON-OVERLAP -----------------------------------------------
    {
        LinearAllocator a(256);
        void* p0 = a.allocate(10, 1);
        void* p1 = a.allocate(20, 1);
        void* p2 = a.allocate(5, 1);
        auto u0 = reinterpret_cast<std::uintptr_t>(p0), u1 = reinterpret_cast<std::uintptr_t>(p1), u2 = reinterpret_cast<std::uintptr_t>(p2);
        check(p0 && p1 && p2, "three sequential allocations all succeed");
        check(u1 >= u0 + 10 && u2 >= u1 + 20, "each block starts past the previous block's end");
        check(u0 != u1 && u1 != u2, "sequential allocations have distinct addresses");
    }

    // --- EXHAUSTION / CURSOR INTACT -------------------------------------------
    {
        LinearAllocator s(32);
        void* p = s.allocate(32, 1);
        check(p != nullptr, "allocation that exactly fills the arena succeeds");
        std::size_t u = s.used();
        void* q = s.allocate(1);
        check(q == nullptr, "over-capacity allocation returns nullptr");
        check(s.used() == u, "failed allocation left the cursor unchanged");

        // A request that fails specifically because of ALIGNMENT PADDING must also
        // leave the cursor untouched (exercises the padding-driven nullptr path).
        LinearAllocator s2(32);
        s2.allocate(30, 1);            // base is >=16-aligned, so the cursor is now at offset 30
        std::size_t u2 = s2.used();
        void* q2 = s2.allocate(1, 16);  // needs 2 padding + 1 byte = 3 > 2 remaining -> nullptr
        check(q2 == nullptr, "allocation failing due to padding returns nullptr");
        check(s2.used() == u2, "padding-driven failure left the cursor unchanged");
    }

    // --- EXACT FIT ------------------------------------------------------------
    {
        LinearAllocator a(24);
        void* p = a.allocate(24, 1);
        check(p != nullptr, "exact-fit allocation succeeds");
        check(a.remaining() == 0, "remaining() == 0 after exact fit");
        check(a.allocate(1, 1) == nullptr, "no further allocation once full");
    }

    // --- RESET ----------------------------------------------------------------
    {
        LinearAllocator a(64);
        void* p0 = a.allocate(16, 8);
        a.reset();
        check(a.used() == 0 && a.remaining() == a.capacity(), "reset() rewinds to empty");
        void* p1 = a.allocate(16, 8);
        check(p1 == p0, "arena reused after reset — same address/alignment");
    }

    // --- MARKER / REWIND ------------------------------------------------------
    {
        LinearAllocator a(128);
        LinearAllocator::Marker m = a.marker();
        void* p0 = a.allocate(16, 1);
        a.allocate(16, 1);
        check(a.used() == m + 32, "used() advanced by exactly two 16-byte allocs");
        a.rewindTo(m);
        check(a.used() == m, "rewindTo(m) restores the saved offset");
        void* p1 = a.allocate(16, 1);
        check(p1 == p0, "storage after the marker is reused");

        // Nested markers rewind in stack order.
        LinearAllocator::Marker A = a.marker();
        a.allocate(8, 1);
        LinearAllocator::Marker B = a.marker();
        a.allocate(8, 1);
        a.rewindTo(B);
        check(a.used() == B, "rewindTo(B) restores the inner marker");
        a.rewindTo(A);
        check(a.used() == A, "rewindTo(A) restores the outer marker");
    }

    // --- TYPED ALLOCATE -------------------------------------------------------
    {
        LinearAllocator a(256);
        int* p = a.allocate<int>(4);
        check(p != nullptr, "typed allocate<int>(4) succeeds");
        check(reinterpret_cast<std::uintptr_t>(p) % alignof(int) == 0, "typed pointer respects alignof(int)");
        check(a.used() >= 4 * sizeof(int), "used() advanced by at least 4 ints");
        bool wrOk = true;
        for (int i = 0; i < 4; ++i) {
            p[i] = i;
        }
        for (int i = 0; i < 4; ++i) {
            if (p[i] != i) {
                wrOk = false;
            }
        }
        check(wrOk, "typed storage is writable and readable");
        int* q = a.allocate<int>();
        check(q != nullptr, "typed allocate<int>() defaults to count 1");
    }

    // --- CAPACITY 0 -----------------------------------------------------------
    {
        LinearAllocator z(0);
        check(z.capacity() == 0, "capacity-0 arena reports 0 capacity");
        check(z.allocate(1) == nullptr, "non-empty allocate on a 0-arena returns nullptr");
        z.allocate(0);
        check(z.used() == 0, "0-arena stays empty (no crash)");
    }

    // --- SIZE 0 ON A NORMAL ARENA ---------------------------------------------
    {
        LinearAllocator a(64);
        a.allocate(8, 8);
        std::size_t u = a.used();
        void* p = a.allocate(0, 8);
        (void)p;
        check(a.used() == u, "already-aligned zero-size allocate advances by padding only (none here)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
