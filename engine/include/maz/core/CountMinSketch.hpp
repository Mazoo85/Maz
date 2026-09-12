#pragma once

#include "maz/core/Murmur3.hpp" // murmur3_32

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// maz::core::CountMinSketch — estimate HOW MANY TIMES each item appeared in a stream, using a small fixed
// table instead of one counter per distinct key. Where HyperLogLog answers "how many DIFFERENT items?", a
// Count-Min sketch answers "how often did THIS item occur?" — approximately, in O(1) memory that does not
// grow with the number of distinct keys. It is the standard tool for finding "heavy hitters" cheaply:
// which item is being spammed in chat, which network source is flooding packets, which ability is used most,
// which asset is requested most — without a hash map that balloons to millions of entries. The structure is
// d rows of w counters; each item is hashed d different ways (one column per row) and those d counters are
// bumped; the estimate is the MINIMUM of the item's d counters. Because different keys can collide into the
// same counter, the estimate can only ever be TOO HIGH, never too low — and taking the min across rows makes
// large overestimates rare. Registers add elementwise, so per-shard sketches merge for free. Header-only,
// std-only, deterministic. Godot has no frequency sketch.
namespace maz::core {

class CountMinSketch {
public:
    // width = counters per row (accuracy); depth = number of rows / independent hashes (confidence).
    CountMinSketch(std::size_t width = 2048, std::size_t depth = 4)
        : m_w(width < 1 ? 1 : width), m_d(depth < 1 ? 1 : depth) {
        m_table.assign(m_w * m_d, 0);
    }

    // Record `count` occurrences of `key`.
    void add(std::string_view key, std::uint64_t count = 1) {
        for (std::size_t i = 0; i < m_d; ++i) {
            m_table[i * m_w + col(key, i)] += count;
        }
        m_total += count;
    }

    // Estimated number of occurrences of `key` (>= the true count; never an underestimate).
    std::uint64_t estimate(std::string_view key) const {
        std::uint64_t best = UINT64_MAX;
        for (std::size_t i = 0; i < m_d; ++i) {
            best = std::min(best, m_table[i * m_w + col(key, i)]);
        }
        return (m_d == 0) ? 0 : best;
    }

    // Fold another sketch (same dimensions) in; the result estimates the combined stream.
    void merge(const CountMinSketch& other) {
        if (other.m_w != m_w || other.m_d != m_d) {
            return;
        }
        for (std::size_t i = 0; i < m_table.size(); ++i) {
            m_table[i] += other.m_table[i];
        }
        m_total += other.m_total;
    }

    std::uint64_t total() const { return m_total; }
    std::size_t width() const { return m_w; }
    std::size_t depth() const { return m_d; }
    void clear() {
        std::fill(m_table.begin(), m_table.end(), std::uint64_t{0});
        m_total = 0;
    }

private:
    std::size_t col(std::string_view key, std::size_t row) const {
        // A distinct seed per row gives (near-)independent hash functions.
        const std::uint32_t seed = static_cast<std::uint32_t>(row) * 0x9E3779B1u + 0x85EBCA6Bu;
        return static_cast<std::size_t>(murmur3_32(key, seed)) % m_w;
    }

    std::size_t m_w;
    std::size_t m_d;
    std::uint64_t m_total = 0;
    std::vector<std::uint64_t> m_table;
};

} // namespace maz::core
