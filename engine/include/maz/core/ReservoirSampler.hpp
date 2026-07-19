#pragma once

#include "maz/core/Pcg32.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::core::ReservoirSampler — uniform random selection of k items from a stream of UNKNOWN length,
// in a single pass and O(k) memory (Vitter's "algorithm R"). You feed items in one at a time and, at any
// moment, hold k of them chosen so that every item seen so far had an equal chance of being kept — no
// need to store or even count the stream up front. That is the right tool for picking N random spawn
// points from a candidate stream, sampling a handful of events from a firehose, choosing random loot
// from a generated pile, or keeping a representative subset of telemetry without unbounded memory.
// Deterministic from a caller-supplied Pcg32 (replays / lockstep). Godot has no reservoir sampler.
// Header-only.
namespace maz::core {

template <typename T>
class ReservoirSampler {
public:
    ReservoirSampler(std::size_t k, Pcg32& rng) : m_k(k), m_rng(&rng) { m_reservoir.reserve(k); }

    // Feed one item from the stream. Keeps it with the correct probability so the reservoir stays a
    // uniform sample of everything offered so far.
    void offer(const T& item) {
        ++m_seen;
        if (m_reservoir.size() < m_k) {
            m_reservoir.push_back(item);
            return;
        }
        if (m_k == 0) {
            return;
        }
        // Replace a random slot with probability k/seen: pick j in [0, seen); if it lands in the
        // reservoir, overwrite that slot.
        const std::uint32_t j = m_rng->nextBounded(static_cast<std::uint32_t>(m_seen));
        if (j < static_cast<std::uint32_t>(m_k)) {
            m_reservoir[j] = item;
        }
    }

    const std::vector<T>& samples() const { return m_reservoir; }
    std::size_t size() const { return m_reservoir.size(); }
    std::size_t seen() const { return m_seen; }
    std::size_t capacity() const { return m_k; }

    void reset() {
        m_reservoir.clear();
        m_seen = 0;
    }

private:
    std::size_t m_k;
    Pcg32* m_rng;
    std::vector<T> m_reservoir;
    std::size_t m_seen = 0;
};

// Convenience: reservoir-sample k items from a whole vector in one call.
template <typename T>
inline std::vector<T> reservoirSample(const std::vector<T>& items, std::size_t k, Pcg32& rng) {
    ReservoirSampler<T> s(k, rng);
    for (const T& it : items) {
        s.offer(it);
    }
    return s.samples();
}

} // namespace maz::core
