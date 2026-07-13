// Unit tests for maz::core::Flags<E> — the type-safe bitflag wrapper over a
// scoped enum. Exercises composition (member + macro free ops), queries
// (has/hasAny/hasAll), mutators (set/clear/toggle), equality, raw round-trip,
// zero-overhead sizeof, and the small-underlying (uint8_t) -Wconversion path.
// Pure C++, no GPU/display.

#include "maz/core/Flags.hpp"

#include <cstdint>
#include <cstdio>
#include <type_traits>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

enum class Perm  : std::uint32_t { None = 0, Read = 1u << 0, Write = 1u << 1, Exec = 1u << 2 };
enum class Small : std::uint8_t  { A = 1u << 0, B = 1u << 1, C = 1u << 2 };
MAZ_FLAGS_ENABLE(Perm)
MAZ_FLAGS_ENABLE(Small)

// file-scope constexpr proofs:
static_assert((Perm::Read | Perm::Write).value() == 3u, "constexpr flag combine");
constexpr Flags<Perm> kCf = Perm::Read | Perm::Exec;
static_assert(kCf.has(Perm::Exec), "constexpr has");
static_assert(sizeof(Flags<Perm>) == sizeof(std::uint32_t), "zero overhead u32");
static_assert(sizeof(Flags<Small>) == sizeof(std::uint8_t), "zero overhead u8");

} // namespace

int main() {
    // --- CONSTRUCTION ---------------------------------------------------------
    {
        Flags<Perm> f;
        check(f.none() && !f.any() && f.value() == 0u && !bool(f), "default is empty");
        Flags<Perm> r(Perm::Read);
        check(r.has(Perm::Read) && !r.has(Perm::Write) && r.value() == 1u && bool(r), "single-bit construct");
    }

    // --- COMBINE (MEMBER OP) --------------------------------------------------
    {
        Flags<Perm> rw = Flags<Perm>(Perm::Read) | Perm::Write;
        check(rw.has(Perm::Read) && rw.has(Perm::Write) && !rw.has(Perm::Exec) && rw.value() == 3u, "member operator| combines");
    }

    // --- MACRO FREE OP --------------------------------------------------------
    {
        auto rw2 = Perm::Read | Perm::Write;
        static_assert(std::is_same_v<decltype(rw2), Flags<Perm>>, "macro yields Flags<Perm>");
        check(rw2.value() == 3u, "enum|enum via macro");
        auto all3 = Perm::Read | Perm::Write | Perm::Exec;
        check(all3.value() == 7u, "three-way enum|enum");
    }

    // --- & ^ ~ ----------------------------------------------------------------
    {
        Flags<Perm> rw = Flags<Perm>(Perm::Read) | Perm::Write;
        check((rw & Flags<Perm>(Perm::Read)).value() == 1u, "operator& masks");
        check((rw ^ Flags<Perm>(Perm::Read)).value() == 2u, "operator^ toggles");
        Flags<Perm> nr = ~Flags<Perm>(Perm::Read);
        check(!nr.has(Perm::Read) && nr.has(Perm::Write) && nr.value() == static_cast<std::uint32_t>(~1u), "operator~ inverts");
    }

    // --- has / hasAny / hasAll ------------------------------------------------
    {
        Flags<Perm> rw = Flags<Perm>(Perm::Read) | Perm::Write;
        check(!rw.hasAny(Flags<Perm>(Perm::Exec)), "hasAny false when disjoint");
        check(rw.hasAny(Perm::Read | Perm::Exec), "hasAny true when overlapping");
        check(rw.hasAll(Perm::Read | Perm::Write), "hasAll true when superset");
        check(!rw.hasAll(Perm::Read | Perm::Exec), "hasAll false when missing bit");
    }

    // --- set / clear / toggle -------------------------------------------------
    {
        Flags<Perm> m;
        m.set(Perm::Read).set(Perm::Write);
        check(m.value() == 3u, "set chains");
        m.clear(Perm::Read);
        check(m.value() == 2u, "clear removes bit");
        m.toggle(Perm::Exec);
        check(m.value() == 6u, "toggle sets bit");
        m.toggle(Perm::Exec);
        check(m.value() == 2u, "toggle clears bit");
    }

    // --- EQUALITY -------------------------------------------------------------
    {
        check(Flags<Perm>(Perm::Read | Perm::Write) == (Perm::Write | Perm::Read), "== is order-independent");
        check(Flags<Perm>(Perm::Read | Perm::Write) != Flags<Perm>(Perm::Read), "!= detects difference");
    }

    // --- RAW ROUND-TRIP -------------------------------------------------------
    {
        auto fr = Flags<Perm>::fromRaw(5u);
        check(fr.value() == 5u && fr.has(Perm::Read) && fr.has(Perm::Exec) && !fr.has(Perm::Write), "fromRaw preserves bits");
    }

    // --- SIZEOF (ZERO OVERHEAD) -----------------------------------------------
    {
        check(sizeof(Flags<Perm>) == sizeof(std::uint32_t), "sizeof == u32");
        check(sizeof(Flags<Small>) == sizeof(std::uint8_t), "sizeof == u8");
    }

    // --- SMALL uint8 (-Wconversion path) --------------------------------------
    {
        Flags<Small> s = Small::A | Small::C;
        check(s.value() == std::uint8_t{5}, "u8 combine");
        Flags<Small> ns = ~s;
        check(!ns.has(Small::A) && ns.has(Small::B), "u8 operator~ clean");
        // Assert the exact full-width complement: ~0x05 truncated to uint8 == 0xFA
        // (proves the ~ result is narrowed to U's width, not left as a wide int).
        check(ns.value() == std::uint8_t{0xFA}, "u8 operator~ exact value");
        s.set(Small::B);
        check(s.value() == std::uint8_t{7}, "u8 set");
        s.clear(Small::A);
        check(s.value() == std::uint8_t{6}, "u8 clear");
        s.toggle(Small::A);
        check(s.value() == std::uint8_t{7}, "u8 toggle");
    }

    // --- CONSTEXPR USE AT RUNTIME ---------------------------------------------
    {
        check(kCf.has(Perm::Exec), "constexpr value has() at runtime");
        check(kCf.value() == 5u, "constexpr value() at runtime");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
