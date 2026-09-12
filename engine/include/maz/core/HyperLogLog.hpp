#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// maz::core::HyperLogLog — estimate how many DISTINCT items a stream contained using a few kilobytes of
// fixed memory, no matter how many billions of items flow through. Counting uniques exactly needs a set that
// grows with the data (megabytes for millions of distinct values); HyperLogLog answers "roughly how many
// different X did we see?" from a tiny fixed array of counters, trading a small, bounded error (standard
// error ~1.04/sqrt(m)) for O(1) memory. This is the standard tool for cardinality: distinct players online,
// distinct enemies a weapon has hit, distinct assets touched this session for telemetry, distinct chat
// authors — anywhere the count matters but storing every value would be wasteful. The trick: hash each item
// to 64 bits, use the top bits to pick one of m = 2^p registers, and record in that register the position of
// the leftmost 1-bit of the rest; a stream of n distinct items tends to produce a maximum leading-zero run
// of about log2(n), and averaging across registers (harmonic mean) sharpens the estimate. Registers merge by
// max, so per-shard sketches combine into a whole-stream one for free. Header-only, std-only, deterministic.
// Godot has no cardinality estimator.
namespace maz::core {

class HyperLogLog {
public:
    // p in [4,16] sets the register count m = 2^p (memory) and accuracy (~1.04/sqrt(m) relative error).
    explicit HyperLogLog(int p = 12) : m_p(p < 4 ? 4 : (p > 16 ? 16 : p)) {
        m_registers.assign(static_cast<std::size_t>(1) << static_cast<unsigned>(m_p), 0);
    }

    // Add an item by its 64-bit hash.
    void addHash(std::uint64_t x) {
        const unsigned p = static_cast<unsigned>(m_p);
        const std::size_t idx = static_cast<std::size_t>(x >> (64u - p));
        const std::uint64_t rest = x << p; // the bits below the index; leading zeros here drive rho
        const std::uint8_t rho = leadingRho(rest, 64u - p);
        if (rho > m_registers[idx]) {
            m_registers[idx] = rho;
        }
    }

    // Add an item by hashing its bytes (FNV-1a 64 + splitmix64 finalize for good bit spread).
    void add(std::string_view s) { addHash(hash64(s)); }

    // Estimated number of distinct items added.
    double estimate() const {
        const std::size_t m = m_registers.size();
        const double dm = static_cast<double>(m);
        double sum = 0.0;
        std::size_t zeros = 0;
        for (std::uint8_t r : m_registers) {
            sum += std::ldexp(1.0, -static_cast<int>(r)); // 2^-r
            if (r == 0) {
                ++zeros;
            }
        }
        double alpha;
        if (m == 16) {
            alpha = 0.673;
        } else if (m == 32) {
            alpha = 0.697;
        } else if (m == 64) {
            alpha = 0.709;
        } else {
            alpha = 0.7213 / (1.0 + 1.079 / dm);
        }
        double e = alpha * dm * dm / sum;
        // Small-range correction: linear counting when many registers are still empty.
        if (e <= 2.5 * dm && zeros > 0) {
            e = dm * std::log(dm / static_cast<double>(zeros));
        }
        return e;
    }

    // Fold another sketch (same p) into this one — the result estimates the UNION's cardinality.
    void merge(const HyperLogLog& other) {
        if (other.m_registers.size() != m_registers.size()) {
            return;
        }
        for (std::size_t i = 0; i < m_registers.size(); ++i) {
            if (other.m_registers[i] > m_registers[i]) {
                m_registers[i] = other.m_registers[i];
            }
        }
    }

    void clear() { std::fill(m_registers.begin(), m_registers.end(), static_cast<std::uint8_t>(0)); }
    int precision() const { return m_p; }
    std::size_t registerCount() const { return m_registers.size(); }

    // 64-bit byte hash used by add(); exposed so callers can hash once and reuse.
    static std::uint64_t hash64(std::string_view s) {
        std::uint64_t h = 1469598103934665603ull; // FNV-1a
        for (char c : s) {
            h ^= static_cast<std::uint8_t>(c);
            h *= 1099511628211ull;
        }
        h ^= h >> 30;
        h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 27;
        h *= 0x94D049BB133111EBull;
        h ^= h >> 31;
        return h;
    }

private:
    // Leading-zero-run rho in the top `bits` of `rest` (counting from the MSB), plus 1; capped at bits+1.
    static std::uint8_t leadingRho(std::uint64_t rest, unsigned bits) {
        std::uint8_t rho = 1;
        std::uint64_t mask = 0x8000000000000000ull; // bit 63
        unsigned counted = 0;
        while (counted < bits && (rest & mask) == 0) {
            ++rho;
            mask >>= 1;
            ++counted;
        }
        return rho;
    }

    int m_p;
    std::vector<std::uint8_t> m_registers;
};

} // namespace maz::core
