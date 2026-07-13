#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>

#include "maz/core/Assert.hpp"

namespace maz::core {

// A word-backed, fixed-size bitset of N bits — the ECS component-signature-mask
// primitive (each bit == one component type; an entity's set components form a
// mask, a system's required components another, and matching is a subset test).
// VALUE-ADD over std::bitset: fast set-bit ITERATION (findFirstSet/findNextSet/
// forEachSetBit) built on the C++20 <bit> intrinsics, so a signature can be
// walked component-by-component without probing every index.
// INVARIANT: the unused high bits of the last word (indices at or above N) are
// held at ZERO at all times; maskTail() re-establishes this after any op that
// can dirty them — set() (whole), whole-flip(), and operator~. NOT thread-safe.
template <std::size_t N>
class Bitset {
    static_assert(N >= 1, "Bitset requires N >= 1");

  public:
    static constexpr std::size_t kBitsPerWord = 64;
    static constexpr std::size_t kWordCount   = (N + kBitsPerWord - 1) / kBitsPerWord;  // ceil div
    static constexpr std::size_t kTailBits    = N % kBitsPerWord;  // 0 == last word fully used

    // TAIL MASK — the crux. When kTailBits==0 the last word is FULLY used, so the
    // mask is all-ones; we MUST NOT compute (1<<64) (UB). The ternary guards it:
    // the else-branch shifts only by kTailBits in [1,63].
    static constexpr std::uint64_t kLastWordMask = (kTailBits == 0) ? ~std::uint64_t{0} : ((std::uint64_t{1} << kTailBits) - 1);

  private:
    std::uint64_t m_words[kWordCount] = {};  // zero-init

    constexpr void maskTail() { m_words[kWordCount - 1] &= kLastWordMask; }  // applied after set()/whole-flip()/~ (the only tail-dirtying ops)

    static constexpr std::size_t   wordOf(std::size_t i) { return i / kBitsPerWord; }
    static constexpr std::uint64_t bitOf (std::size_t i) { return std::uint64_t{1} << (i % kBitsPerWord); }  // i%64 < 64 => shift safe

  public:
    // per-bit (bounds-checked)
    constexpr void set  (std::size_t i) { MAZ_ASSERT(i < N, "Bitset::set index out of range");   m_words[wordOf(i)] |=  bitOf(i); }
    constexpr void clear(std::size_t i) { MAZ_ASSERT(i < N, "Bitset::clear index out of range"); m_words[wordOf(i)] &= ~bitOf(i); }
    constexpr void flip (std::size_t i) { MAZ_ASSERT(i < N, "Bitset::flip index out of range");  m_words[wordOf(i)] ^=  bitOf(i); }
    constexpr bool test (std::size_t i) const { MAZ_ASSERT(i < N, "Bitset::test index out of range"); return (m_words[wordOf(i)] & bitOf(i)) != 0; }
    constexpr bool operator[](std::size_t i) const { return test(i); }

    // whole-set
    constexpr void set()   { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] = ~std::uint64_t{0}; } maskTail(); }  // MASK
    constexpr void reset() { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] = 0; } }                              // no dirty tail
    constexpr void flip()  { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] = ~m_words[k]; } maskTail(); }        // MASK

    // counts / predicates
    constexpr std::size_t count() const { std::size_t c = 0; for (std::size_t k = 0; k < kWordCount; ++k) { c += static_cast<std::size_t>(std::popcount(m_words[k])); } return c; }
    constexpr bool any()  const { for (std::size_t k = 0; k < kWordCount; ++k) { if (m_words[k] != 0) { return true; } } return false; }
    constexpr bool none() const { return !any(); }
    constexpr bool all()  const { return count() == N; }

    // set algebra (word-wise). Tail is dirtied only by set()/whole-flip()/operator~
    // (all re-mask); &,|,^ of tail-clean operands stay tail-clean (0 op 0 == 0), and
    // == is safe since both operands are tail-clean.
    constexpr Bitset operator&(const Bitset& o) const { Bitset r; for (std::size_t k = 0; k < kWordCount; ++k) { r.m_words[k] = m_words[k] & o.m_words[k]; } return r; }
    constexpr Bitset operator|(const Bitset& o) const { Bitset r; for (std::size_t k = 0; k < kWordCount; ++k) { r.m_words[k] = m_words[k] | o.m_words[k]; } return r; }
    constexpr Bitset operator^(const Bitset& o) const { Bitset r; for (std::size_t k = 0; k < kWordCount; ++k) { r.m_words[k] = m_words[k] ^ o.m_words[k]; } return r; }
    constexpr Bitset operator~() const { Bitset r; for (std::size_t k = 0; k < kWordCount; ++k) { r.m_words[k] = ~m_words[k]; } r.maskTail(); return r; }  // ONLY op needing re-mask
    constexpr Bitset& operator&=(const Bitset& o) { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] &= o.m_words[k]; } return *this; }
    constexpr Bitset& operator|=(const Bitset& o) { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] |= o.m_words[k]; } return *this; }
    constexpr Bitset& operator^=(const Bitset& o) { for (std::size_t k = 0; k < kWordCount; ++k) { m_words[k] ^= o.m_words[k]; } return *this; }
    constexpr bool operator==(const Bitset& o) const { for (std::size_t k = 0; k < kWordCount; ++k) { if (m_words[k] != o.m_words[k]) { return false; } } return true; }
    constexpr bool operator!=(const Bitset& o) const { return !(*this == o); }

    // ECS system-match: this ⊇ other (every set bit of other is set here). contains(empty)==true.
    constexpr bool contains(const Bitset& o) const { for (std::size_t k = 0; k < kWordCount; ++k) { if ((m_words[k] & o.m_words[k]) != o.m_words[k]) { return false; } } return true; }

    // FAST SET-BIT ITERATION (never ctz(0) — countr_zero is only called on a proven-nonzero word)
    constexpr std::size_t findFirstSet() const {
        for (std::size_t k = 0; k < kWordCount; ++k) { if (m_words[k] != 0) { return k * kBitsPerWord + static_cast<std::size_t>(std::countr_zero(m_words[k])); } }
        return N;
    }
    constexpr std::size_t findNextSet(std::size_t from) const {
        if (from >= N) { return N; }
        std::size_t k = from / kBitsPerWord;
        std::uint64_t w = m_words[k] & (~std::uint64_t{0} << (from % kBitsPerWord));  // keep bits >= from%64; from%64 in [0,63] => shift safe
        if (w != 0) { return k * kBitsPerWord + static_cast<std::size_t>(std::countr_zero(w)); }
        for (++k; k < kWordCount; ++k) { if (m_words[k] != 0) { return k * kBitsPerWord + static_cast<std::size_t>(std::countr_zero(m_words[k])); } }
        return N;
    }
    template <class F> constexpr void forEachSetBit(F&& fn) const {
        for (std::size_t k = 0; k < kWordCount; ++k) {
            std::uint64_t w = m_words[k];
            while (w != 0) {
                std::size_t b = k * kBitsPerWord + static_cast<std::size_t>(std::countr_zero(w));
                fn(b);
                w &= (w - 1);  // clear lowest set bit
            }
        }
    }

    // observers
    static constexpr std::size_t size()      { return N; }
    static constexpr std::size_t wordCount() { return kWordCount; }
};

} // namespace maz::core

namespace maz::core::detail {
inline constexpr Bitset<8> kBitsetSelfTest = []{ Bitset<8> b; b.set(0); b.set(3); return b; }();
static_assert(kBitsetSelfTest.count() == 2, "constexpr count");
static_assert(kBitsetSelfTest.test(3) && !kBitsetSelfTest.test(1), "constexpr test");
static_assert(Bitset<100>::kLastWordMask == ((std::uint64_t{1} << 36) - 1), "tail mask (100%64==36)");
static_assert(Bitset<128>::kLastWordMask == ~std::uint64_t{0}, "full last word => all-ones mask");
static_assert(Bitset<64>::kWordCount == 1 && Bitset<100>::kWordCount == 2, "ceil-div word count");
} // namespace maz::core::detail
