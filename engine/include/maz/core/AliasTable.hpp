#pragma once

#include "maz/core/Pcg32.hpp"

#include <cstddef>
#include <vector>

// maz::core::AliasTable — Vose's ALIAS METHOD for O(1) weighted random selection. Pcg32::weighted
// picks an index proportional to its weight in O(n) per draw, rebuilding the running total every time;
// that is fine for an occasional roll but wasteful for a table sampled thousands of times a frame
// (particle spawns, procedural scatter, big loot/encounter tables). AliasTable pays an O(n) build ONCE
// to precompute two small tables, after which every sample is a single random column + one coin flip —
// constant time regardless of table size. Deterministic: draws come from a caller-supplied Pcg32, so
// the same seed reproduces the same sequence (replays / lockstep). Negative weights count as 0. Godot's
// rand_weighted is the O(n) form; the build-once alias table is a beyond-Godot utility. Header-only.
namespace maz::core {

class AliasTable {
public:
    AliasTable() = default;
    explicit AliasTable(const std::vector<float>& weights) { build(weights); }

    // Precompute the alias tables from `weights` (Vose's algorithm). Safe to call again to rebuild.
    void build(const std::vector<float>& weights) {
        const std::size_t n = weights.size();
        m_prob.assign(n, 0.0f);
        m_alias.assign(n, 0);
        if (n == 0) {
            return;
        }

        // Scale weights so the average is 1 (scaled[i] = w_i * n / total). Non-positive weights -> 0.
        double total = 0.0;
        for (const float w : weights) {
            if (w > 0.0f) {
                total += static_cast<double>(w);
            }
        }
        std::vector<float> scaled(n, 0.0f);
        if (total <= 0.0) {
            // Degenerate: no positive weight. Fall back to a uniform table so sampling still works.
            for (std::size_t i = 0; i < n; ++i) {
                scaled[i] = 1.0f;
            }
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                const double w = weights[i] > 0.0f ? static_cast<double>(weights[i]) : 0.0;
                scaled[i] = static_cast<float>(w * static_cast<double>(n) / total);
            }
        }

        std::vector<std::size_t> small;
        std::vector<std::size_t> large;
        small.reserve(n);
        large.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (scaled[i] < 1.0f) {
                small.push_back(i);
            } else {
                large.push_back(i);
            }
        }

        while (!small.empty() && !large.empty()) {
            const std::size_t s = small.back();
            small.pop_back();
            const std::size_t l = large.back();
            large.pop_back();
            m_prob[s] = scaled[s];
            m_alias[s] = l;
            scaled[l] = (scaled[l] + scaled[s]) - 1.0f;
            if (scaled[l] < 1.0f) {
                small.push_back(l);
            } else {
                large.push_back(l);
            }
        }
        // Leftovers get probability 1 (any tiny float drift settles here).
        while (!large.empty()) {
            m_prob[large.back()] = 1.0f;
            large.pop_back();
        }
        while (!small.empty()) {
            m_prob[small.back()] = 1.0f;
            small.pop_back();
        }
    }

    std::size_t size() const { return m_prob.size(); }
    bool empty() const { return m_prob.empty(); }

    // Draw an index in [0, size()) with probability proportional to its weight, in O(1). Returns -1 for
    // an empty table. Uses `rng` for both the column pick and the coin flip.
    int sample(Pcg32& rng) const {
        const std::size_t n = m_prob.size();
        if (n == 0) {
            return -1;
        }
        const std::size_t col = static_cast<std::size_t>(rng.range(0, static_cast<int>(n) - 1));
        return rng.nextFloat() < m_prob[col] ? static_cast<int>(col)
                                             : static_cast<int>(m_alias[col]);
    }

private:
    std::vector<float> m_prob;        // per-column acceptance probability
    std::vector<std::size_t> m_alias; // per-column fallback index
};

} // namespace maz::core
