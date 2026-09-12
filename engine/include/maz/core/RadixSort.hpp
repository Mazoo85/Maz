#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

// maz::core::RadixSort — sort by an integer (or float) key in LINEAR time, O(n), instead of the O(n log n)
// of a comparison sort. A renderer sorts thousands of draw calls every frame by a packed 32/64-bit sort key
// (layer << depth << material) to batch state and draw front-to-back; a particle system sorts by camera
// distance for correct alpha blending; an ECS sorts entities by a packed archetype key. At those sizes, run
// every frame, the difference between n·log n and n comparisons is real. Radix sort achieves it by bucketing
// on one byte of the key at a time (a stable counting sort per byte, least-significant byte first), so after
// 4 passes (32-bit) or 8 passes (64-bit) the array is fully ordered — with NO key comparisons at all. The
// key-plus-payload variant (radixSortByKey) is STABLE: items with equal keys keep their original order, which
// is what makes it safe to sort by successive keys. Floats are handled via the standard order-preserving bit
// transform, so depth sorting "just works" including negatives. Header-only, std-only, deterministic. Godot
// has no radix sort; this is the workhorse behind fast per-frame ordering.
namespace maz::core {

namespace detail {

// Stable LSD radix sort of `items`, keyed by the parallel `keys` array (unsigned integral). Both are
// reordered in place. 256 buckets, one byte per pass; result lands back in `items` because sizeof(K) is even.
template <class T, class K>
void radixByKeys(std::vector<T>& items, std::vector<K>& keys) {
    static_assert(std::is_unsigned_v<K>, "radix key must be an unsigned integer type");
    const std::size_t n = items.size();
    if (n < 2) {
        return;
    }
    std::vector<T> tmp(n);
    std::vector<K> ktmp(n);
    constexpr int kBytes = static_cast<int>(sizeof(K));
    for (int b = 0; b < kBytes; ++b) {
        const int shift = b * 8;
        std::size_t count[256] = {0};
        for (std::size_t i = 0; i < n; ++i) {
            ++count[static_cast<std::size_t>((keys[i] >> shift) & K{0xff})];
        }
        std::size_t sum = 0;
        for (std::size_t j = 0; j < 256; ++j) {
            const std::size_t c = count[j];
            count[j] = sum;
            sum += c;
        }
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t bucket = static_cast<std::size_t>((keys[i] >> shift) & K{0xff});
            const std::size_t pos = count[bucket]++;
            tmp[pos] = items[i];
            ktmp[pos] = keys[i];
        }
        items.swap(tmp);
        keys.swap(ktmp);
    }
}

} // namespace detail

// Sort an unsigned-integer array ascending, in place.
template <class UInt>
void radixSort(std::vector<UInt>& a) {
    static_assert(std::is_unsigned_v<UInt> && std::is_integral_v<UInt>, "radixSort needs unsigned integers");
    std::vector<UInt> keys = a;
    detail::radixByKeys(a, keys);
}

// Sort arbitrary items ascending by an unsigned-integer key extracted with keyOf(item). STABLE — equal keys
// keep their original relative order.
template <class T, class KeyFn>
void radixSortByKey(std::vector<T>& items, KeyFn keyOf) {
    using K = std::remove_cv_t<std::remove_reference_t<decltype(keyOf(items[0]))>>;
    static_assert(std::is_unsigned_v<K>, "radixSortByKey key must be an unsigned integer type");
    const std::size_t n = items.size();
    if (n < 2) {
        return;
    }
    std::vector<K> keys(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = keyOf(items[i]);
    }
    detail::radixByKeys(items, keys);
}

// The order-preserving float<->uint32 transform used for radix-sorting floats (including negatives):
// non-negative floats get their sign bit set; negative floats are fully inverted. This makes the raw bit
// pattern compare in the same order as the float value.
inline std::uint32_t floatSortKey(float f) {
    std::uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    return (u & 0x80000000u) ? ~u : (u | 0x80000000u);
}
inline float floatFromSortKey(std::uint32_t v) {
    const std::uint32_t u = (v & 0x80000000u) ? (v & 0x7fffffffu) : ~v;
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

// Sort a float array ascending, in place (handles negatives and zero; NaN is not supported).
inline void radixSortFloats(std::vector<float>& a) {
    const std::size_t n = a.size();
    if (n < 2) {
        return;
    }
    std::vector<std::uint32_t> keys(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = floatSortKey(a[i]);
    }
    radixSort(keys);
    for (std::size_t i = 0; i < n; ++i) {
        a[i] = floatFromSortKey(keys[i]);
    }
}

} // namespace maz::core
