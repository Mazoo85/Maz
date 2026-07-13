// Unit tests for maz::core::Bitset<N> — the word-backed, fixed-size ECS
// component-signature mask. Exercises per-bit and whole-set mutation, counts/
// predicates, set algebra (& | ^ ~) plus subset containment, equality, the fast
// set-bit iteration (findFirstSet/findNextSet/forEachSetBit), and the tail-mask
// invariant for N not a multiple of 64. Pure C++, no GPU/display.

#include "maz/core/Bitset.hpp"

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

static_assert([]{ Bitset<8> b; b.set(0); b.set(3); return b.count(); }() == 2u, "constexpr count");

int main() {
    // --- BASIC SET / TEST / CLEAR / FLIP -------------------------------------
    {
        Bitset<128> b;
        b.set(0);
        b.set(64);
        b.set(127);
        check(b.test(0) && b.test(64) && b.test(127) && !b.test(1) && !b.test(63), "set/test across words");
        check(b.count() == 3, "count after three sets");
        b.clear(64);
        check(!b.test(64) && b.count() == 2, "clear drops a bit");
        b.flip(5);
        check(b.test(5), "flip sets an unset bit");
        b.flip(5);
        check(!b.test(5), "flip clears a set bit");
    }

    // --- WORD BOUNDARY --------------------------------------------------------
    {
        Bitset<128> b;
        b.set(63);
        check(!b.test(64), "bit 63 does not leak into bit 64");
        b.set(64);
        check(b.test(63) && b.test(64) && b.count() == 2, "adjacent bits straddle the word boundary");
    }

    // --- COUNT / ANY / ALL / NONE --------------------------------------------
    {
        Bitset<128> b;
        check(b.none() && !b.any() && b.count() == 0 && !b.all(), "empty predicates");
        b.set(1);
        b.set(70);
        check(b.any() && b.count() == 2 && !b.all(), "populated predicates");
        b.reset();
        check(b.none(), "reset clears all bits");
    }

    // --- WHOLE SET / FLIP / RESET --------------------------------------------
    {
        Bitset<128> b;
        b.set();
        check(b.count() == 128 && b.all(), "whole set fills every bit");
        b.flip();
        check(b.count() == 0 && b.none(), "whole flip of a full set empties it");
        b.set(3);
        b.reset();
        check(b.none(), "reset after a single set");
    }

    // --- & | ^ ~ CONTAINS -----------------------------------------------------
    {
        Bitset<128> a;
        a.set(1);
        a.set(2);
        a.set(3);
        Bitset<128> c;
        c.set(2);
        c.set(3);
        c.set(4);

        Bitset<128> aAndC = a & c;
        check(aAndC.count() == 2 && aAndC.test(2) && aAndC.test(3) && !aAndC.test(1) && !aAndC.test(4), "AND keeps the intersection");

        Bitset<128> aOrC = a | c;
        check(aOrC.count() == 4 && aOrC.test(1) && aOrC.test(4), "OR keeps the union");

        Bitset<128> aXorC = a ^ c;
        check(aXorC.count() == 2 && aXorC.test(1) && aXorC.test(4) && !aXorC.test(2), "XOR keeps the symmetric difference");

        Bitset<128> na = ~a;
        check(na.count() == 128 - 3 && !na.test(1) && na.test(0), "NOT inverts every bit");

        Bitset<128> sup;
        sup.set(1);
        sup.set(2);
        sup.set(3);
        sup.set(4);
        Bitset<128> sub;
        sub.set(2);
        sub.set(3);
        check(sup.contains(sub), "superset contains subset");

        Bitset<128> notsup;
        notsup.set(1);
        notsup.set(2);
        Bitset<128> big;
        big.set(1);
        big.set(2);
        big.set(3);
        check(!notsup.contains(big), "non-superset does not contain");
        check(a.contains(a), "a contains itself");

        Bitset<128> empty;
        check(a.contains(empty), "everything contains the empty set");
    }

    // --- == / != --------------------------------------------------------------
    {
        Bitset<128> x;
        x.set(5);
        x.set(70);
        Bitset<128> y;
        y.set(5);
        y.set(70);
        check(x == y, "equal masks compare equal");
        y.set(9);
        check(x != y, "differing masks compare unequal");
    }

    // --- SET-BIT ITERATION ----------------------------------------------------
    {
        Bitset<128> b;
        const std::size_t exp[5] = {3, 40, 63, 64, 99};
        for (auto e : exp) {
            b.set(e);
        }
        std::size_t out[8];
        std::size_t n = 0;
        b.forEachSetBit([&](std::size_t idx) {
            if (n < 8) {
                out[n] = idx;
            }
            ++n;
        });
        bool ok = (n == 5);
        for (std::size_t i = 0; i < 5 && ok; ++i) {
            if (out[i] != exp[i]) {
                ok = false;
            }
        }
        check(ok, "forEachSetBit yields set bits in ascending order");
        check(b.findFirstSet() == 3, "findFirstSet returns the lowest set bit");
        check(b.findNextSet(4) == 40, "findNextSet skips to the next set bit");
        check(b.findNextSet(64) == 64, "findNextSet is inclusive of the from index");
        check(b.findNextSet(100) == 128, "findNextSet past the last returns N");

        Bitset<128> e2;
        check(e2.findFirstSet() == 128, "findFirstSet on empty returns N");
    }

    // --- TAIL MASKING GATE (N % 64 != 0) -------------------------------------
    {
        Bitset<100> b;
        b.set();
        check(b.count() == 100 && b.all(), "whole set on N=100 fills exactly 100 bits, not 128");
        {
            std::size_t cnt = 0;
            std::size_t maxIdx = 0;
            b.forEachSetBit([&](std::size_t idx) {
                ++cnt;
                if (idx > maxIdx) {
                    maxIdx = idx;
                }
            });
            check(cnt == 100 && maxIdx == 99, "no set bit at or beyond index 100");
        }

        Bitset<100> f;
        f.flip();
        check(f.count() == 100, "whole flip from empty leaves the tail zero");

        Bitset<100> e;
        Bitset<100> ne = ~e;
        check(ne.count() == 100 && ne.all(), "NOT of empty fills exactly N bits");

        Bitset<100> t;
        t.set(99);
        check(t.findFirstSet() == 99 && t.findNextSet(99) == 99 && t.findNextSet(100) == 100, "iteration honors the top valid index");
        t.clear(99);
        check(t.findFirstSet() == 100, "findFirstSet on cleared returns N");

        Bitset<10> s;
        s.set();
        check(s.count() == 10 && s.all(), "whole set on a single tail-only word");
        {
            std::size_t cnt = 0;
            std::size_t mx = 0;
            s.forEachSetBit([&](std::size_t idx) {
                ++cnt;
                if (idx > mx) {
                    mx = idx;
                }
            });
            check(cnt == 10 && mx == 9, "no set bit beyond index 9 for N=10");
        }
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
