#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// maz::core MurmurHash3 (x86_32) — a fast, well-distributed NON-cryptographic hash (Austin Appleby, public
// domain), the default workhorse for hash tables, bloom filters, feature flags, and stable content/asset IDs.
// The engine already has cryptographic digests (SHA-1/SHA-256) and CRC32, but those are the wrong tool for
// hashing map keys millions of times a frame: SHA is far too slow, and CRC32 has poor avalanche (similar
// inputs cluster). MurmurHash3 is built for exactly this — a few multiplies and rotates per 4 bytes, strong
// mixing so one-bit input changes scatter the whole output, and a `seed` so you can derive independent hash
// functions (e.g. the k hashes a bloom filter needs). It is also a de-facto interchange standard: this
// implementation reproduces the canonical published test vectors byte-for-byte, so hashes computed here match
// those from other tools and languages. Header-only, std-only, deterministic. Godot exposes only its own
// String.hash and hash_djb2.
namespace maz::core {

namespace detail {
inline std::uint32_t murmurRotl(std::uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}
} // namespace detail

// 32-bit MurmurHash3 of `len` bytes with an optional seed.
inline std::uint32_t murmur3_32(const std::uint8_t* data, std::size_t len, std::uint32_t seed = 0) {
    const std::uint32_t c1 = 0xcc9e2d51u;
    const std::uint32_t c2 = 0x1b873593u;
    std::uint32_t h = seed;

    const std::size_t nblocks = len / 4;
    for (std::size_t i = 0; i < nblocks; ++i) {
        const std::size_t j = i * 4;
        std::uint32_t k = static_cast<std::uint32_t>(data[j]) |
                          (static_cast<std::uint32_t>(data[j + 1]) << 8) |
                          (static_cast<std::uint32_t>(data[j + 2]) << 16) |
                          (static_cast<std::uint32_t>(data[j + 3]) << 24);
        k *= c1;
        k = detail::murmurRotl(k, 15);
        k *= c2;
        h ^= k;
        h = detail::murmurRotl(h, 13);
        h = h * 5u + 0xe6546b64u;
    }

    // Tail.
    const std::uint8_t* tail = data + nblocks * 4;
    std::uint32_t k1 = 0;
    switch (len & 3u) {
    case 3:
        k1 ^= static_cast<std::uint32_t>(tail[2]) << 16;
        [[fallthrough]];
    case 2:
        k1 ^= static_cast<std::uint32_t>(tail[1]) << 8;
        [[fallthrough]];
    case 1:
        k1 ^= static_cast<std::uint32_t>(tail[0]);
        k1 *= c1;
        k1 = detail::murmurRotl(k1, 15);
        k1 *= c2;
        h ^= k1;
        break;
    default:
        break;
    }

    // Finalization mix.
    h ^= static_cast<std::uint32_t>(len);
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

inline std::uint32_t murmur3_32(std::string_view s, std::uint32_t seed = 0) {
    return murmur3_32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size(), seed);
}

} // namespace maz::core
