#pragma once

#include <cstdint>
#include <type_traits>

namespace maz::core {

// Type-safe, zero-overhead bitflag wrapper over a SCOPED ENUM whose enumerators
// are single bits (1<<0, 1<<1, ...) — the Qt QFlags / vulkan-hpp Flags analog,
// and the maz::core answer to Godot's raw int/uint32 bitfields + manual masking.
// REQUIREMENT: the enum MUST have an UNSIGNED underlying type (the bit ops ~,<<
// are only well-defined on unsigned) — declare `enum class Perm : std::uint32_t
// { ... }`; a plain `enum class` defaults to signed int and intentionally fails
// the static_assert below. To compose two enumerators directly (E::A | E::B ->
// Flags<E>), opt in with MAZ_FLAGS_ENABLE(E) at the SAME namespace scope as the
// enum, right after its definition. NARROWING NOTE: for underlying types
// narrower than int (uint8_t/uint16_t) every ~,|,&,^ integer-promotes its
// operands to int, so the result must be narrowed back to U. The explicit
// static_cast<U>(...) on every op makes that narrowing intentional and portable
// — GCC/Clang happen not to warn here, but stricter settings and MSVC /W4 do,
// and the cast documents that the truncation (e.g. ~uint8_t{1} -> 0xFE) is the
// desired "complement within U's width".
template <class E>
class Flags {
    static_assert(std::is_enum_v<E>, "Flags<E> requires an enum type");

  public:
    using U = std::underlying_type_t<E>;
    static_assert(std::is_unsigned_v<U>, "Flags<E> requires an enum with an unsigned underlying type (e.g. enum class E : std::uint32_t)");

    constexpr Flags() = default;
    constexpr Flags(E e) : m_bits(static_cast<U>(e)) {}   // implicit, ergonomic
    static constexpr Flags fromRaw(U raw) { Flags f; f.m_bits = raw; return f; }

    constexpr Flags operator|(Flags o) const { return fromRaw(static_cast<U>(m_bits | o.m_bits)); }
    constexpr Flags operator&(Flags o) const { return fromRaw(static_cast<U>(m_bits & o.m_bits)); }
    constexpr Flags operator^(Flags o) const { return fromRaw(static_cast<U>(m_bits ^ o.m_bits)); }
    constexpr Flags operator~() const { return fromRaw(static_cast<U>(~m_bits)); }  // ~ promotes small U to int -> cast back

    constexpr Flags& operator|=(Flags o) { m_bits = static_cast<U>(m_bits | o.m_bits); return *this; }
    constexpr Flags& operator&=(Flags o) { m_bits = static_cast<U>(m_bits & o.m_bits); return *this; }
    constexpr Flags& operator^=(Flags o) { m_bits = static_cast<U>(m_bits ^ o.m_bits); return *this; }

    constexpr bool operator==(Flags o) const { return m_bits == o.m_bits; }
    constexpr bool operator!=(Flags o) const { return m_bits != o.m_bits; }

    // True iff ALL bits of e are set (membership for a single-bit e; an all-of
    // test for a multi-bit e). Note: has(e) with e == 0 (e.g. a None = 0
    // enumerator) is always true, since the empty bit set is trivially contained.
    constexpr bool has(E e) const { return (m_bits & static_cast<U>(e)) == static_cast<U>(e); }
    constexpr bool test(E e) const { return has(e); }
    constexpr bool hasAny(Flags o) const { return (m_bits & o.m_bits) != 0; }
    constexpr bool hasAll(Flags o) const { return (m_bits & o.m_bits) == o.m_bits; }
    constexpr bool any() const { return m_bits != 0; }
    constexpr bool none() const { return m_bits == 0; }
    explicit constexpr operator bool() const { return m_bits != 0; }

    constexpr Flags& set(E e)    { m_bits = static_cast<U>(m_bits | static_cast<U>(e)); return *this; }
    constexpr Flags& clear(E e)  { m_bits = static_cast<U>(m_bits & static_cast<U>(~static_cast<U>(e))); return *this; }  // double-~/cast
    constexpr Flags& toggle(E e) { m_bits = static_cast<U>(m_bits ^ static_cast<U>(e)); return *this; }

    constexpr U value() const { return m_bits; }

  private:
    U m_bits = 0;   // single member => sizeof(Flags<E>) == sizeof(U), zero overhead
};

} // namespace maz::core

// Opt-in free operators so `E::A | E::B` (both enumerators) yields Flags<E>.
// Invoke MAZ_FLAGS_ENABLE(YourEnum) at the SAME namespace scope as YourEnum,
// right after its definition; ADL then finds these for E-typed operands.
// `E OP flags` (enum literal left of a Flags) is intentionally NOT provided.
// User writes the trailing semicolon.
#define MAZ_FLAGS_ENABLE(E) \
    inline constexpr ::maz::core::Flags<E> operator|(E a, E b) { return ::maz::core::Flags<E>(a) | ::maz::core::Flags<E>(b); } \
    inline constexpr ::maz::core::Flags<E> operator&(E a, E b) { return ::maz::core::Flags<E>(a) & ::maz::core::Flags<E>(b); } \
    inline constexpr ::maz::core::Flags<E> operator~(E a) { return ~::maz::core::Flags<E>(a); }

namespace maz::core::detail {
enum class FlagsSelfTest : std::uint32_t { A = 1u << 0, B = 1u << 1, C = 1u << 2 };
MAZ_FLAGS_ENABLE(FlagsSelfTest)
static_assert(sizeof(Flags<FlagsSelfTest>) == sizeof(std::uint32_t), "zero-overhead: sizeof == underlying");
static_assert(Flags<FlagsSelfTest>{}.none(), "default is empty");
static_assert((FlagsSelfTest::A | FlagsSelfTest::B).value() == 3u, "constexpr enum|enum via macro");
static_assert((FlagsSelfTest::A | FlagsSelfTest::B).has(FlagsSelfTest::A), "constexpr has()");
static_assert(!(FlagsSelfTest::A | FlagsSelfTest::B).has(FlagsSelfTest::C), "constexpr !has()");
static_assert(Flags<FlagsSelfTest>::fromRaw(5u).value() == 5u, "fromRaw round-trip");
} // namespace maz::core::detail
