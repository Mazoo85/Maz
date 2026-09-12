#pragma once

#include <cstddef>
#include <vector>

// maz::core::ShuffleBag<T> — a "deal from a bag" randomizer that gives FAIR, clump-free randomness. Unlike an
// independent weighted roll (which can hand you the same result five times in a row, or leave one option
// starved for a long stretch), a shuffle bag holds one token per desired outcome, deals them in random order,
// and only refills-and-reshuffles once the bag is empty — so over each cycle every outcome appears EXACTLY
// its intended number of times, and the worst-case drought is bounded. This is the Tetris "7-bag" piece
// randomizer, and exactly what you want for enemy-type spawns, music-playlist shuffle, card decks, or random
// events that should feel fair rather than streaky. It also (by default) avoids handing you the same value
// twice across a refill boundary, so no back-to-back repeats when the outcomes are distinct. Complements the
// engine's AliasTable (fast independent weighted draws) and ReservoirSampler (streaming sample) — this is the
// without-replacement, cycle-fair option. Any RNG with an inclusive `range(int lo, int hi)` works (e.g.
// core::Pcg32). Header-only, std-only, fully deterministic for a given seed.
namespace maz::core {

template <typename T>
class ShuffleBag {
public:
    explicit ShuffleBag(bool avoidImmediateRepeat = true) : m_avoidRepeat(avoidImmediateRepeat) {}

    // Add `count` copies of `item` to the bag's contents (its per-cycle frequency).
    void add(const T& item, int count = 1) {
        for (int i = 0; i < count; ++i) m_contents.push_back(item);
    }

    std::size_t totalCount() const { return m_contents.size(); }  // items dealt per full cycle
    std::size_t remaining() const { return m_current.size(); }    // items left in the current cycle
    bool empty() const { return m_contents.empty(); }

    void clear() {
        m_contents.clear();
        m_current.clear();
        m_hasLast = false;
    }

    // Deal the next item. Refills and reshuffles when the current cycle is exhausted. Returns a default-
    // constructed T if the bag has no contents. `rng.range(lo, hi)` must return an inclusive integer in [lo, hi].
    template <typename Rng>
    T next(Rng& rng) {
        bool freshCycle = false;
        if (m_current.empty()) {
            m_current = m_contents;
            freshCycle = true;
        }
        if (m_current.empty()) return T{}; // nothing was ever added

        const std::size_t n = m_current.size();
        std::size_t i = static_cast<std::size_t>(rng.range(0, static_cast<int>(n) - 1));

        // At a cycle boundary, avoid repeating the previously-dealt value if a different one is available.
        if (freshCycle && m_avoidRepeat && m_hasLast && m_current[i] == m_last) {
            for (std::size_t k = 1; k < n; ++k) {
                const std::size_t j = (i + k) % n;
                if (!(m_current[j] == m_last)) {
                    i = j;
                    break;
                }
            }
        }

        T item = m_current[i];
        m_current[i] = m_current[n - 1]; // swap-remove keeps the draw uniform
        m_current.pop_back();
        m_last = item;
        m_hasLast = true;
        return item;
    }

private:
    std::vector<T> m_contents; // master multiset: one entry per outcome instance
    std::vector<T> m_current;  // what's left to deal this cycle
    T m_last{};
    bool m_hasLast = false;
    bool m_avoidRepeat = true;
};

} // namespace maz::core
