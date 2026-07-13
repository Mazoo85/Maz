// Unit tests for maz::core::SmallVector<T, N>. Exercises inline storage, the
// spill to heap once growth exceeds N, move-on-grow relocation, the two-branch
// move ctor / move assign (heap-steal vs element-wise inline move), and manual
// element lifetime — the Tracked s_live==0 checks are the leak/double-free
// detectors. Pure C++, no GPU/display.

#include "maz/core/SmallVector.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>  // std::move

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// File-local lifetime-counting element: every ctor/dtor bumps a static counter
// so a test can assert nothing leaks (s_live) and account for every construction
// (s_dtor == s_ctor + s_copy + s_move).
struct Tracked {
    inline static int s_ctor = 0, s_dtor = 0, s_copy = 0, s_move = 0, s_live = 0;
    static void reset() { s_ctor = 0; s_dtor = 0; s_copy = 0; s_move = 0; s_live = 0; }
    int v = 0;
    bool movedFrom = false;
    Tracked() { ++s_ctor; ++s_live; }
    explicit Tracked(int x) : v(x) { ++s_ctor; ++s_live; }
    Tracked(const Tracked& o) : v(o.v) { ++s_copy; ++s_live; }
    Tracked(Tracked&& o) noexcept : v(o.v) { ++s_move; ++s_live; o.movedFrom = true; }
    ~Tracked() { ++s_dtor; --s_live; }
    // No assignment ops: the container only placement-constructs + destroys elements.
};

} // namespace

int main() {
    // --- DEFAULT --------------------------------------------------------------
    {
        SmallVector<int, 4> v;
        check(v.size() == 0 && v.empty() && v.capacity() == 4 && v.isInline()
                  && SmallVector<int, 4>::inlineCapacity() == 4,
              "default is empty, inline, capacity N");
    }

    // --- FILL TO N INLINE -----------------------------------------------------
    {
        SmallVector<int, 4> v;
        for (int i = 0; i < 4; ++i) { v.push_back(i); }
        check(v.size() == 4 && v.capacity() == 4 && v.isInline(), "N pushes stay inline");
        bool ok = true;
        for (std::size_t i = 0; i < v.size(); ++i) { if (v[i] != static_cast<int>(i)) { ok = false; } }
        check(ok, "inline values are correct");
        check(v.data() == v.begin() && v.end() == v.begin() + 4, "data/begin/end parity");
    }

    // --- SPILL PAST N ---------------------------------------------------------
    {
        SmallVector<int, 4> v;
        for (int i = 0; i < 5; ++i) { v.push_back(i); }
        check(v.size() == 5 && v.capacity() >= 8 && v.capacity() > 4 && !v.isInline(),
              "spilling past N grows to heap");
        bool ok = true;
        for (std::size_t i = 0; i < 5; ++i) { if (v[i] != static_cast<int>(i)) { ok = false; } }
        check(ok, "move-on-grow preserved data");
    }

    // --- OVER-ALIGNED ELEMENT (heap storage honors alignof(T)) ----------------
    {
        struct alignas(32) Over { int x = 0; };
        // Inline storage first: alignas(T) on the inline buffer must honor it.
        SmallVector<Over, 2> v;
        v.push_back(Over{1});
        check(v.isInline(), "over-aligned stays inline below N");
        check(reinterpret_cast<std::uintptr_t>(v.data()) % 32 == 0, "inline storage is 32-aligned");
        // Force a spill: the aligned ::operator new path must return 32-aligned memory.
        v.push_back(Over{2});
        v.push_back(Over{3});
        check(!v.isInline(), "over-aligned spills to heap past N");
        check(reinterpret_cast<std::uintptr_t>(v.data()) % 32 == 0, "heap storage is 32-aligned");
        check(v[0].x == 1 && v[2].x == 3, "over-aligned values intact across spill");
    }

    // --- TRACKED NO-LEAK BELOW N ----------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 4> v;
            v.emplace_back(1);
            v.emplace_back(2);
            v.emplace_back(3);
            check(Tracked::s_live == 3, "3 live inline");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (inline)");
        check(Tracked::s_dtor == Tracked::s_ctor + Tracked::s_copy + Tracked::s_move,
              "every construction is balanced by a destruction");
    }

    // --- TRACKED SPILL MOVE-ON-GROW -------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 2> v;
            v.emplace_back(1);
            v.emplace_back(2);
            check(v.isInline(), "2 elements stay inline");
            int m0 = Tracked::s_move;
            v.emplace_back(3);
            check(Tracked::s_move - m0 == 2, "grow relocates 2 existing elements by move");
            check(!v.isInline() && v.size() == 3 && Tracked::s_live == 3, "spilled, 3 live");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (spilled)");
    }

    // --- POP_BACK / CLEAR -----------------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 2> v;
            v.emplace_back(1);
            v.emplace_back(2);
            v.emplace_back(3);
            std::size_t cap = v.capacity();
            v.pop_back();
            check(v.size() == 2 && Tracked::s_live == 2, "pop_back destroys one");
            v.clear();
            check(v.size() == 0 && Tracked::s_live == 0 && v.capacity() == cap && !v.isInline(),
                  "clear destroys all but keeps heap capacity");
            v.emplace_back(9);
            check(v.size() == 1, "reusable after clear");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (pop/clear)");
    }

    // --- COPY CTOR + COPY ASSIGN ----------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 2> a;
            a.emplace_back(10);
            a.emplace_back(20);
            a.emplace_back(30);
            int c0 = Tracked::s_copy;
            SmallVector<Tracked, 2> b(a);
            check(Tracked::s_copy - c0 == 3 && b.size() == 3 && b[0].v == 10 && b[2].v == 30,
                  "copy ctor deep-copies all elements");
            a[0].v = 99;
            check(b[0].v == 10, "copy is independent of source");
            SmallVector<Tracked, 2> c;
            c = a;
            check(c.size() == 3 && c[1].v == 20 && c[0].v == 99, "copy assign deep-copies");

            SmallVector<Tracked, 4> d;
            d.emplace_back(7);
            d.emplace_back(8);
            SmallVector<Tracked, 4> e(d);
            check(e.size() == 2 && e[1].v == 8 && d.isInline() && e.isInline(),
                  "inline-source copy stays inline");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (copy)");
    }

    // --- MOVE CTOR FROM HEAP (steal, no element moves) ------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 2> a;
            a.emplace_back(1);
            a.emplace_back(2);
            a.emplace_back(3);
            check(!a.isInline(), "source spilled to heap");
            int m0 = Tracked::s_move;
            SmallVector<Tracked, 2> b(std::move(a));
            check(Tracked::s_move - m0 == 0 && b.size() == 3 && !b.isInline() && b[2].v == 3,
                  "heap move ctor steals block without element moves");
            check(a.size() == 0 && a.isInline(), "moved-from heap source is empty-inline");
            check(Tracked::s_live == 3, "no extra elements created");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (heap move ctor)");
    }

    // --- MOVE CTOR FROM INLINE (element-wise) ---------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 4> a;
            a.emplace_back(1);
            a.emplace_back(2);
            a.emplace_back(3);
            check(a.isInline(), "source stays inline");
            int m0 = Tracked::s_move;
            SmallVector<Tracked, 4> b(std::move(a));
            check(Tracked::s_move - m0 == 3 && b.size() == 3 && b[2].v == 3 && a.size() == 0,
                  "inline move ctor moves each element");
            check(Tracked::s_live == 3, "3 live after inline move");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (inline move ctor)");
    }

    // --- MOVE ASSIGN BOTH BRANCHES --------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 2> dst;
            dst.emplace_back(1);
            dst.emplace_back(2);
            dst.emplace_back(3);
            SmallVector<Tracked, 2> src;
            src.emplace_back(4);
            src.emplace_back(5);
            src.emplace_back(6);
            dst = std::move(src);
            check(dst.size() == 3 && dst[0].v == 4 && src.size() == 0, "move assign from heap source");

            SmallVector<Tracked, 4> dst2;
            dst2.emplace_back(1);
            SmallVector<Tracked, 4> src2;
            src2.emplace_back(7);
            src2.emplace_back(8);
            dst2 = std::move(src2);
            check(dst2.size() == 2 && dst2[1].v == 8 && src2.size() == 0, "move assign from inline source");
        }
        check(Tracked::s_live == 0 && Tracked::s_dtor == Tracked::s_ctor + Tracked::s_copy + Tracked::s_move,
              "no leak / balanced after move assign");
    }

    // --- RESERVE --------------------------------------------------------------
    {
        SmallVector<int, 4> v;
        v.reserve(100);
        check(v.capacity() >= 100 && !v.isInline() && v.size() == 0, "reserve spills and grows capacity");
        v.push_back(5);
        check(v.size() == 1 && v[0] == 5, "usable after reserve");
    }

    // --- ITERATORS / DATA PARITY ----------------------------------------------
    {
        SmallVector<int, 4> v;
        for (int i = 0; i < 6; ++i) { v.push_back(i); }
        int s1 = 0;
        for (int x : v) { s1 += x; }
        int s2 = 0;
        for (std::size_t i = 0; i < v.size(); ++i) { s2 += v[i]; }
        check(s1 == s2 && s1 == 15, "range-for and index sums agree");
        bool ok = true;
        for (std::size_t i = 0; i < v.size(); ++i) { if (v.data()[i] != v[i]) { ok = false; } }
        check(ok, "data()[i] matches operator[]");
    }

    // --- EMPLACE_BACK IN PLACE ------------------------------------------------
    {
        Tracked::reset();
        {
            SmallVector<Tracked, 4> v;
            int ct0 = Tracked::s_ctor, c0 = Tracked::s_copy, mv0 = Tracked::s_move;
            v.emplace_back(42);
            check(Tracked::s_ctor - ct0 == 1 && Tracked::s_copy - c0 == 0 && Tracked::s_move - mv0 == 0
                      && v.back().v == 42,
                  "emplace_back constructs in place, no copy/move");
        }
        check(Tracked::s_live == 0, "all destroyed after scope (emplace)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
