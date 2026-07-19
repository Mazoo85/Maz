#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// maz::core::BloomFilter — a space-efficient probabilistic set: it answers "have I definitely NOT seen
// this?" with certainty and "might I have seen this?" with a small, tunable false-positive chance,
// using a fraction of the memory a real set of the keys would cost. It never reports a false negative
// (anything added always tests present), so it is the ideal fast pre-filter: a "visited" marker for
// the millions of cells/chunks in a huge procedural world, a duplicate-suppressor for events or
// network packets, or a cheap gate in front of an expensive exact lookup ("skip the disk/db hit when
// the Bloom filter says it's certainly absent"). Uses k hash probes derived from a single std::hash by
// double-hashing (h1 + i*h2), so it works for any std::hash-able key. Build one directly by bit-count +
// probe-count, or via optimal() from an expected item count and target false-positive rate.
// Header-only, std-only. Godot has no Bloom filter.
namespace maz::core {

template <typename T, typename Hash = std::hash<T>>
class BloomFilter {
public:
    // Direct construction: a bit array of `bitCount` bits probed by `numHashes` hash functions.
    BloomFilter(std::size_t bitCount, std::size_t numHashes)
        : m_numBits(bitCount == 0 ? 1 : bitCount),
          m_k(numHashes == 0 ? 1 : numHashes),
          m_words((m_numBits + 63) / 64, 0) {}

    // Size a filter for `expectedItems` entries at target false-positive rate `fpRate` (in (0,1)),
    // using the classic optimal m = -n ln p / (ln2)^2 bits and k = (m/n) ln2 probes.
    static BloomFilter optimal(std::size_t expectedItems, double fpRate) {
        const double n = static_cast<double>(expectedItems == 0 ? 1 : expectedItems);
        const double p = (fpRate > 0.0 && fpRate < 1.0) ? fpRate : 0.01;
        const double ln2 = 0.6931471805599453;
        const double m = std::ceil(-(n * std::log(p)) / (ln2 * ln2));
        std::size_t bits = m > 1.0 ? static_cast<std::size_t>(m) : 1;
        const double kf = std::round((m / n) * ln2);
        std::size_t k = kf > 1.0 ? static_cast<std::size_t>(kf) : 1;
        return BloomFilter(bits, k);
    }

    void add(const T& key) {
        std::uint64_t h1 = 0;
        std::uint64_t h2 = 0;
        probes(key, h1, h2);
        for (std::size_t i = 0; i < m_k; ++i) {
            const std::uint64_t idx = (h1 + static_cast<std::uint64_t>(i) * h2) % m_numBits;
            m_words[static_cast<std::size_t>(idx >> 6)] |= (std::uint64_t{1} << (idx & 63));
        }
        ++m_added;
    }

    // false => the key was definitely never added. true => probably added (small false-positive rate).
    bool possiblyContains(const T& key) const {
        std::uint64_t h1 = 0;
        std::uint64_t h2 = 0;
        probes(key, h1, h2);
        for (std::size_t i = 0; i < m_k; ++i) {
            const std::uint64_t idx = (h1 + static_cast<std::uint64_t>(i) * h2) % m_numBits;
            if ((m_words[static_cast<std::size_t>(idx >> 6)] & (std::uint64_t{1} << (idx & 63))) == 0) {
                return false;
            }
        }
        return true;
    }

    void clear() {
        for (std::uint64_t& w : m_words) {
            w = 0;
        }
        m_added = 0;
    }

    std::size_t numBits() const { return m_numBits; }
    std::size_t numHashes() const { return m_k; }
    std::size_t addedCount() const { return m_added; } // number of add() calls (with duplicates)

    // Count of set bits.
    std::size_t setBits() const {
        std::size_t c = 0;
        for (std::uint64_t w : m_words) {
            c += static_cast<std::size_t>(std::popcount(w));
        }
        return c;
    }
    // Fraction of bits set, in [0,1].
    double fillRatio() const { return static_cast<double>(setBits()) / static_cast<double>(m_numBits); }

    // Estimate of the number of DISTINCT items added, from the fill ratio (Swamidass-Baldi estimator).
    double approxItemCount() const {
        const double x = static_cast<double>(setBits());
        const double m = static_cast<double>(m_numBits);
        const double k = static_cast<double>(m_k);
        if (x <= 0.0) {
            return 0.0;
        }
        if (x >= m) {
            return m / k; // saturated
        }
        return -(m / k) * std::log(1.0 - x / m);
    }

    // Union with another filter of identical geometry (both must share numBits and numHashes).
    // Returns false and does nothing if the geometries differ.
    bool merge(const BloomFilter& other) {
        if (other.m_numBits != m_numBits || other.m_k != m_k) {
            return false;
        }
        for (std::size_t i = 0; i < m_words.size(); ++i) {
            m_words[i] |= other.m_words[i];
        }
        m_added += other.m_added;
        return true;
    }

private:
    static std::uint64_t mix(std::uint64_t x) { // splitmix64 finalizer
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }

    void probes(const T& key, std::uint64_t& h1, std::uint64_t& h2) const {
        h1 = static_cast<std::uint64_t>(m_hash(key));
        h2 = mix(h1) | 1ULL; // odd step keeps the probe sequence well-distributed
    }

    std::size_t m_numBits;
    std::size_t m_k;
    std::vector<std::uint64_t> m_words;
    std::size_t m_added = 0;
    Hash m_hash{};
};

} // namespace maz::core
