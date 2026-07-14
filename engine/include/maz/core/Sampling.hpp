#pragma once

#include <vector>
#include <cstddef>
#include <numeric>  // std::iota
#include <utility>  // std::swap
#include <cmath>    // std::sqrt, std::log, std::cos

#include "maz/core/Random.hpp"
#include "maz/core/Assert.hpp"

namespace maz::core {

// Random sampling helpers over maz::core::Rng: shuffle (in-place Fisher-Yates),
// weightedIndex (choose an index proportional to non-negative weights — a loot
// table), sampleWithoutReplacement (k distinct indices from [0,n) via partial
// Fisher-Yates), and gaussian (Box-Muller normal sample). All deterministic for
// a given Rng seed/sequence. Composes the iter4 Rng. NOT thread-safe (mutates the
// Rng). A cached second Box-Muller sample and reservoir sampling are future
// refinements.

// In-place Fisher-Yates shuffle. Empty/size-1 vectors are a no-op. Deterministic
// for a given Rng seed. Assumes v.size() <= INT_MAX (the index passes through
// rangeInt's int span).
template <typename T>
void shuffle(Rng& rng, std::vector<T>& v) {
    for (std::size_t i = v.size(); i > 1; --i) {
        // rangeInt is INCLUSIVE so [0, i-1] picks any index 0..i-1.
        std::size_t j = static_cast<std::size_t>(rng.rangeInt(0, static_cast<int>(i) - 1));
        std::swap(v[i - 1], v[j]);
    }
}

// Pick an index with probability proportional to its (non-negative) weight — a
// loot table. Requires a non-empty weight vector with a positive total. A single
// non-zero weight (e.g. [0,0,1,0]) always returns that index; a zero-weight entry
// is never selected.
inline std::size_t weightedIndex(Rng& rng, const std::vector<float>& weights) {
    MAZ_ASSERT(!weights.empty(), "weightedIndex: empty weights");
    float total = 0.0f;
    for (float w : weights) {
        MAZ_ASSERT(w >= 0.0f, "weightedIndex: negative weight");
        total += w;
    }
    MAZ_ASSERT(total > 0.0f, "weightedIndex: total weight must be > 0");
    float r = rng.rangeFloat(0.0f, total);
    float acc = 0.0f;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        acc += weights[i];
        if (r < acc) {
            return i;
        }
    }
    // Guards a floating-point edge where r rounds to exactly total; a zero-weight
    // entry never satisfies r < acc so is never selected here.
    return weights.size() - 1;
}

// Return k DISTINCT indices from [0, n), in selection order, via a partial
// Fisher-Yates over an index array (each of the first k slots gets a distinct
// index; k==n yields a full permutation; k==0 yields an empty vector). Requires
// k <= n. Note: n must be small enough that n-1-i fits an int (n <= INT_MAX),
// since rangeInt takes int bounds.
inline std::vector<std::size_t> sampleWithoutReplacement(Rng& rng, std::size_t n, std::size_t k) {
    MAZ_ASSERT(k <= n, "sampleWithoutReplacement: k must be <= n");
    if (k == 0) {
        return {};
    }
    std::vector<std::size_t> idx(n);
    std::iota(idx.begin(), idx.end(), std::size_t{0});
    for (std::size_t i = 0; i < k; ++i) {
        std::size_t j = i + static_cast<std::size_t>(rng.rangeInt(0, static_cast<int>(n - 1 - i)));
        std::swap(idx[i], idx[j]);
    }
    idx.resize(k);
    return idx;
}

// A normally-distributed sample via Box-Muller. Returns one sample (the second
// Box-Muller output is discarded for simplicity; a cached-second-sample variant
// is a future refinement). Deterministic for a given Rng seed.
inline float gaussian(Rng& rng, float mean = 0.0f, float stddev = 1.0f) {
    float u1 = rng.nextFloat();
    float u2 = rng.nextFloat();
    // Guard u1 away from 0 (log(0) == -inf).
    if (u1 < 1e-7f) {
        u1 = 1e-7f;
    }
    float mag = stddev * std::sqrt(-2.0f * std::log(u1));
    float z = mag * std::cos(2.0f * 3.14159265358979323846f * u2);
    return mean + z;
}

} // namespace maz::core
