#pragma once

#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

// maz::core::WeightedReservoir — select k items from a STREAM of weighted items in a single pass and O(k)
// memory, where each item's chance of being kept is proportional to its WEIGHT (the Efraimidis-Spirakis
// "A-Res" algorithm). This is the missing middle between the engine's two sampling tools: AliasTable does a
// weighted pick from a KNOWN, in-memory set, and ReservoirSampler picks k from a stream but treats every
// item EQUALLY. WeightedReservoir does both at once — stream in candidates you cannot all hold, each tagged
// with a weight, and keep k chosen in proportion to those weights. The natural tool for drawing N loot
// items from a generated pile weighted by rarity, sampling spawn points weighted by desirability, or keeping
// importance-weighted telemetry without unbounded memory. The trick: for an item of weight w draw a uniform
// u in (0,1) and give it key = u^(1/w); keep the k items with the largest keys (a size-k min-heap). Keys are
// compared in log space (log(u)/w) for numerical stability. Deterministic via an embedded splitmix64.
// Header-only, std-only. Godot ships no weighted reservoir sampler.
namespace maz::core {

template <class T>
class WeightedReservoir {
public:
    WeightedReservoir(std::size_t k, std::uint64_t seed)
        : m_k(k), m_rngState(seed + 0x9E3779B97F4A7C15ull) {}

    // Offer one item with a strictly-positive weight. Non-positive weights are ignored.
    void add(const T& item, double weight) {
        if (weight <= 0.0 || m_k == 0) {
            return;
        }
        double u = rand01();
        if (u <= 0.0) {
            u = 2.220446049250313e-16; // avoid log(0)
        }
        const double key = std::log(u) / weight; // larger (closer to 0) = more likely to survive
        if (m_heap.size() < m_k) {
            m_heap.push(Entry{key, item});
        } else if (key > m_heap.top().key) {
            m_heap.pop();
            m_heap.push(Entry{key, item});
        }
    }

    // The chosen items (up to k). Order is unspecified.
    std::vector<T> sample() const {
        std::vector<T> out;
        out.reserve(m_heap.size());
        std::priority_queue<Entry, std::vector<Entry>, Greater> copy = m_heap;
        while (!copy.empty()) {
            out.push_back(copy.top().item);
            copy.pop();
        }
        return out;
    }

    std::size_t size() const { return m_heap.size(); }
    std::size_t capacity() const { return m_k; }

    void clear() {
        std::priority_queue<Entry, std::vector<Entry>, Greater> empty;
        m_heap.swap(empty);
    }

private:
    struct Entry {
        double key;
        T item;
    };
    struct Greater {
        bool operator()(const Entry& a, const Entry& b) const { return a.key > b.key; } // min-heap on key
    };

    double rand01() {
        m_rngState += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = m_rngState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);
        return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
    }

    std::size_t m_k;
    std::uint64_t m_rngState;
    std::priority_queue<Entry, std::vector<Entry>, Greater> m_heap; // size-k min-heap on key
};

} // namespace maz::core
