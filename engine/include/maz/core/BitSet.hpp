#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::core::BitSet — a DYNAMIC bit set (resizable, word-packed). std::bitset is fixed-size and
// std::vector<bool> lacks the set-algebra and fast scanning games actually want; BitSet stores bits 64
// to a word and gives the operations that matter: test/set/reset/flip, whole-set and/or/xor/not,
// popcount, any/none/all, and O(1)-per-hit iteration over just the set bits (findFirst/findNext). It is
// the natural backing for entity flag sets, per-frame "visited/dirty" marks, tile occupancy grids, and
// arbitrary-width collision layer masks — anywhere a plain int mask runs out of bits. Header-only,
// std-only (C++20 <bit> popcount/countr_zero). Godot exposes only fixed 32-bit masks.
namespace maz::core {

class BitSet {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    BitSet() = default;
    explicit BitSet(std::size_t bits, bool value = false)
        : m_size(bits), m_words((bits + 63) / 64, value ? ~0ull : 0ull) {
        if (value) {
            trim();
        }
    }

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    // Grow (new bits are 0) or shrink (dropped bits vanish, tail is re-trimmed).
    void resize(std::size_t bits, bool value = false) {
        const std::size_t oldSize = m_size;
        m_words.resize((bits + 63) / 64, value ? ~0ull : 0ull);
        m_size = bits;
        if (value && bits > oldSize) {
            // Fill the newly exposed bits of the (previously partial) last old word.
            for (std::size_t i = oldSize; i < bits && (i % 64) != 0; ++i) {
                m_words[i >> 6] |= (1ull << (i & 63));
            }
        }
        trim();
    }

    void set(std::size_t i) { m_words[i >> 6] |= (1ull << (i & 63)); }
    void reset(std::size_t i) { m_words[i >> 6] &= ~(1ull << (i & 63)); }
    void flip(std::size_t i) { m_words[i >> 6] ^= (1ull << (i & 63)); }
    void set(std::size_t i, bool v) { v ? set(i) : reset(i); }
    bool test(std::size_t i) const { return (m_words[i >> 6] >> (i & 63)) & 1ull; }
    bool operator[](std::size_t i) const { return test(i); }

    void setAll() {
        for (std::uint64_t& w : m_words) {
            w = ~0ull;
        }
        trim();
    }
    void resetAll() {
        for (std::uint64_t& w : m_words) {
            w = 0ull;
        }
    }
    void flipAll() {
        for (std::uint64_t& w : m_words) {
            w = ~w;
        }
        trim();
    }

    // Number of set bits (hardware popcount per word; tail bits are kept 0 so no masking is needed).
    std::size_t count() const {
        std::size_t c = 0;
        for (const std::uint64_t w : m_words) {
            c += static_cast<std::size_t>(std::popcount(w));
        }
        return c;
    }
    bool any() const {
        for (const std::uint64_t w : m_words) {
            if (w != 0ull) {
                return true;
            }
        }
        return false;
    }
    bool none() const { return !any(); }
    bool all() const { return count() == m_size; }

    // Set-algebra with a same-sized BitSet (mismatched sizes are a no-op guarded by min-word count).
    BitSet& operator&=(const BitSet& o) {
        const std::size_t n = m_words.size() < o.m_words.size() ? m_words.size() : o.m_words.size();
        for (std::size_t i = 0; i < n; ++i) {
            m_words[i] &= o.m_words[i];
        }
        for (std::size_t i = n; i < m_words.size(); ++i) {
            m_words[i] = 0ull; // bits beyond the other set are cleared by AND
        }
        return *this;
    }
    BitSet& operator|=(const BitSet& o) {
        const std::size_t n = m_words.size() < o.m_words.size() ? m_words.size() : o.m_words.size();
        for (std::size_t i = 0; i < n; ++i) {
            m_words[i] |= o.m_words[i];
        }
        trim();
        return *this;
    }
    BitSet& operator^=(const BitSet& o) {
        const std::size_t n = m_words.size() < o.m_words.size() ? m_words.size() : o.m_words.size();
        for (std::size_t i = 0; i < n; ++i) {
            m_words[i] ^= o.m_words[i];
        }
        trim();
        return *this;
    }
    BitSet operator~() const {
        BitSet r(*this);
        r.flipAll();
        return r;
    }

    // First set bit at or after `from`, or npos. Scans word-at-a-time with countr_zero, so iterating
    // the set bits costs O(set bits + words), not O(total bits).
    std::size_t findNext(std::size_t from) const {
        if (from >= m_size) {
            return npos;
        }
        std::size_t wi = from >> 6;
        std::uint64_t w = m_words[wi] & (~0ull << (from & 63));
        while (true) {
            if (w != 0ull) {
                const std::size_t idx = (wi << 6) + static_cast<std::size_t>(std::countr_zero(w));
                return idx < m_size ? idx : npos;
            }
            if (++wi >= m_words.size()) {
                return npos;
            }
            w = m_words[wi];
        }
    }
    std::size_t findFirst() const { return findNext(0); }

    bool operator==(const BitSet& o) const { return m_size == o.m_size && m_words == o.m_words; }
    bool operator!=(const BitSet& o) const { return !(*this == o); }

private:
    // Zero the unused high bits of the final word so count()/any()/== stay correct.
    void trim() {
        const std::size_t rem = m_size & 63;
        if (rem != 0 && !m_words.empty()) {
            m_words.back() &= (1ull << rem) - 1ull;
        }
    }

    std::size_t m_size = 0;
    std::vector<std::uint64_t> m_words;
};

inline BitSet operator&(BitSet a, const BitSet& b) { return a &= b; }
inline BitSet operator|(BitSet a, const BitSet& b) { return a |= b; }
inline BitSet operator^(BitSet a, const BitSet& b) { return a ^= b; }

} // namespace maz::core
