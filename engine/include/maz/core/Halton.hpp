#pragma once

#include <cstdint>
#include <utility>

// maz::core::Halton — the Halton low-discrepancy (quasi-random) sequence. Unlike a pseudo-random
// generator (Pcg32/Random), which can clump and leave gaps, a Halton sequence fills space EVENLY: each
// new point lands in the biggest remaining hole. That makes it the right tool for procedural scatter
// (trees, rocks, stars) that should look spread out rather than blotchy, for anti-aliasing / temporal
// jitter sample offsets, and for evenly probing a search space. Each coordinate is the van der Corput
// radical inverse of the sample index in a chosen (prime) base — deterministic and stateless, so the
// i-th point is always the same. Values lie in [0,1). Godot has no low-discrepancy sequence. Header-only.
namespace maz::core {

// The van der Corput radical inverse of `index` in `base`: write index in base-`base`, then mirror the
// digits around the decimal point. Deterministic, in [0,1). base < 2 returns 0.
inline float radicalInverse(std::uint32_t index, std::uint32_t base) {
    if (base < 2) {
        return 0.0f;
    }
    const float invBase = 1.0f / static_cast<float>(base);
    float invBaseN = invBase;
    float result = 0.0f;
    while (index > 0) {
        const std::uint32_t digit = index % base;
        result += static_cast<float>(digit) * invBaseN;
        index /= base;
        invBaseN *= invBase;
    }
    return result;
}

// The 1D Halton value for a sample index in a given base (a synonym for radicalInverse).
inline float halton(std::uint32_t index, std::uint32_t base) { return radicalInverse(index, base); }

// A 2D Halton point in the unit square, using coprime bases (2,3 by default) so the two axes do not
// correlate. Ideal for scatter positions and sample offsets.
inline std::pair<float, float> halton2D(std::uint32_t index, std::uint32_t baseX = 2,
                                        std::uint32_t baseY = 3) {
    return {radicalInverse(index, baseX), radicalInverse(index, baseY)};
}

// A tiny stateful cursor for streaming successive Halton points without tracking the index by hand.
struct HaltonSequence {
    std::uint32_t base = 2;
    std::uint32_t index = 1; // sample 0 is the trivial 0.0; start at 1 for a useful first point

    explicit HaltonSequence(std::uint32_t base_ = 2, std::uint32_t start = 1)
        : base(base_), index(start) {}

    float next() { return radicalInverse(index++, base); }
};

} // namespace maz::core
