#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// maz::core hashing — CRC-32 (ISO-HDLC, the zip/PNG polynomial) and SHA-256, the checksums games use
// for asset integrity, save-file validation, content-addressed caches, and network message digests.
// This is Godot's HashingContext / crc32 territory. Both are exact, standard algorithms (verified
// against the published test vectors), header-only and dependency-free. CRC-32 is a fast non-crypto
// checksum; SHA-256 is the cryptographic digest.
namespace maz::core {

// CRC-32/ISO-HDLC (reflected, poly 0xEDB88320, init/xorout 0xFFFFFFFF) — matches zip, PNG, gzip.
inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t seed = 0) {
    std::uint32_t crc = ~seed;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}
inline std::uint32_t crc32(const std::string& s) {
    return crc32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

namespace detail {
inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
} // namespace detail

// SHA-256 digest (32 bytes) of a byte buffer.
inline std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len) {
    static const std::uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    // Message + padding: 0x80, then zeros, then 64-bit big-endian bit length, to a 64-byte multiple.
    std::vector<std::uint8_t> msg(data, data + len);
    const std::uint64_t bitLen = static_cast<std::uint64_t>(len) * 8u;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    using detail::rotr;
    for (std::size_t off = 0; off < msg.size(); off += 64) {
        std::uint32_t w[64];
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t j = off + i * 4;
            w[i] = (static_cast<std::uint32_t>(msg[j]) << 24) |
                   (static_cast<std::uint32_t>(msg[j + 1]) << 16) |
                   (static_cast<std::uint32_t>(msg[j + 2]) << 8) |
                   static_cast<std::uint32_t>(msg[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6],
                      hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::array<std::uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i) {
        out[static_cast<std::size_t>(i * 4)] = static_cast<std::uint8_t>((h[i] >> 24) & 0xFF);
        out[static_cast<std::size_t>(i * 4 + 1)] = static_cast<std::uint8_t>((h[i] >> 16) & 0xFF);
        out[static_cast<std::size_t>(i * 4 + 2)] = static_cast<std::uint8_t>((h[i] >> 8) & 0xFF);
        out[static_cast<std::size_t>(i * 4 + 3)] = static_cast<std::uint8_t>(h[i] & 0xFF);
    }
    return out;
}
inline std::array<std::uint8_t, 32> sha256(const std::string& s) {
    return sha256(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

// Lowercase hex of an arbitrary digest.
inline std::string toHex(const std::uint8_t* data, std::size_t len) {
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(hexd[(data[i] >> 4) & 0xF]);
        out.push_back(hexd[data[i] & 0xF]);
    }
    return out;
}

// Lowercase hex string of a SHA-256 digest (Godot's HashingContext.finish() -> hex).
inline std::string sha256Hex(const std::string& s) {
    const auto d = sha256(s);
    return toHex(d.data(), d.size());
}

// HMAC-SHA256 (RFC 2104) — keyed message authentication, the primitive behind signed save files,
// tamper-proof network messages and API tokens. Godot's Crypto.hmac_digest(HASH_SHA256, ...). Built on
// the SHA-256 above; verified against the RFC 4231 test vectors. Block size is 64 bytes.
inline std::array<std::uint8_t, 32> hmacSha256(const std::uint8_t* key, std::size_t keyLen,
                                               const std::uint8_t* msg, std::size_t msgLen) {
    constexpr std::size_t kBlock = 64;
    std::array<std::uint8_t, kBlock> k0{}; // zero-padded key
    if (keyLen > kBlock) {
        const auto kh = sha256(key, keyLen); // long keys are hashed first
        for (std::size_t i = 0; i < kh.size(); ++i) {
            k0[i] = kh[i];
        }
    } else {
        for (std::size_t i = 0; i < keyLen; ++i) {
            k0[i] = key[i];
        }
    }

    std::vector<std::uint8_t> inner;
    inner.reserve(kBlock + msgLen);
    for (std::size_t i = 0; i < kBlock; ++i) {
        inner.push_back(static_cast<std::uint8_t>(k0[i] ^ 0x36u)); // ipad
    }
    inner.insert(inner.end(), msg, msg + msgLen);
    const auto innerHash = sha256(inner.data(), inner.size());

    std::vector<std::uint8_t> outer;
    outer.reserve(kBlock + innerHash.size());
    for (std::size_t i = 0; i < kBlock; ++i) {
        outer.push_back(static_cast<std::uint8_t>(k0[i] ^ 0x5cu)); // opad
    }
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    return sha256(outer.data(), outer.size());
}
inline std::array<std::uint8_t, 32> hmacSha256(const std::string& key, const std::string& msg) {
    return hmacSha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(),
                      reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size());
}
inline std::string hmacSha256Hex(const std::string& key, const std::string& msg) {
    const auto d = hmacSha256(key, msg);
    return toHex(d.data(), d.size());
}

} // namespace maz::core
