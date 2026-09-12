#pragma once

#include <cstddef>
#include <vector>

// maz::math compensated summation — add up many floating-point numbers WITHOUT the rounding drift that plain
// left-to-right addition accumulates. When a running total grows large, adding a small value loses low bits
// to rounding; over thousands of adds (mixing audio samples, accumulating forces or particle contributions,
// summing analytics/metrics, integrating over a frame) the error piles up. Kahan's algorithm carries a
// separate "compensation" term that captures the lost low bits and feeds them back on the next add, and
// Neumaier's refinement also handles the case where the next value is LARGER than the running sum — together
// they give a result close to what you'd get in much higher precision, at ~4 extra flops per element. Use
// KahanSum as a drop-in accumulator, or compensatedSum for a one-shot total. Header-only, std-only,
// deterministic. (Godot has no compensated-summation utility.)
namespace maz::math {

// A running accumulator that keeps a Neumaier compensation term. `value()` returns the corrected total.
template <class T = float>
struct KahanSum {
    T sum = T(0);
    T c = T(0); // running compensation (lost low-order bits)

    void add(T x) {
        const T t = sum + x;
        // Neumaier: pick the larger magnitude as the "big" operand so the small bits are the ones recovered.
        if ((sum < 0 ? -sum : sum) >= (x < 0 ? -x : x)) {
            c += (sum - t) + x;
        } else {
            c += (x - t) + sum;
        }
        sum = t;
    }

    void reset() {
        sum = T(0);
        c = T(0);
    }

    // The corrected total (running sum plus the accumulated compensation).
    T value() const { return sum + c; }
};

// One-shot compensated total of a range of values (Neumaier). Empty range returns 0.
template <class T = float>
inline T compensatedSum(const std::vector<T>& xs) {
    KahanSum<T> acc;
    for (const T& x : xs) {
        acc.add(x);
    }
    return acc.value();
}

} // namespace maz::math
