// tests/core/murmur3.cpp — verifies MurmurHash3 x86_32 (core Murmur3.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * the canonical published test vectors match byte-for-byte (this is the strong correctness proof —
//     these are the values every conformant MurmurHash3 implementation produces);
//   * hashing is deterministic and changes with the seed;
//   * a one-byte change scatters the hash (avalanche) — no clustering of similar inputs;
//   * hashing over a large distinct-key set produces very few collisions.
#include "maz/core/Murmur3.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

using maz::core::murmur3_32;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. Canonical test vectors (PMurHash / SMHasher reference). ---
    {
        CHECK(murmur3_32("", 0) == 0x00000000u, "\"\" seed 0");
        CHECK(murmur3_32("", 1) == 0x514E28B7u, "\"\" seed 1");
        CHECK(murmur3_32("", 0xffffffffu) == 0x81F16F39u, "\"\" seed 0xffffffff");
        const std::uint32_t s = 0x9747b28cu;
        CHECK(murmur3_32("a", s) == 0x7FA09EA6u, "\"a\"");
        CHECK(murmur3_32("ab", s) == 0x74875592u, "\"ab\"");
        CHECK(murmur3_32("abc", s) == 0xC84A62DDu, "\"abc\"");
        CHECK(murmur3_32("abcd", s) == 0xF0478627u, "\"abcd\"");
        CHECK(murmur3_32("aaaa", s) == 0x5A97808Au, "\"aaaa\"");
        CHECK(murmur3_32("aaa", s) == 0x283E0130u, "\"aaa\"");
        CHECK(murmur3_32("aa", s) == 0x5D211726u, "\"aa\"");
        CHECK(murmur3_32("Hello, world!", s) == 0x24884CBAu, "\"Hello, world!\"");
        CHECK(murmur3_32("The quick brown fox jumps over the lazy dog", s) == 0x2FA826CDu, "pangram");
    }

    // --- 2. Determinism + seed sensitivity. ---
    {
        CHECK(murmur3_32("maz", 42) == murmur3_32("maz", 42), "deterministic");
        CHECK(murmur3_32("maz", 42) != murmur3_32("maz", 43), "different seed -> different hash");
    }

    // --- 3. Avalanche: a one-byte change scatters the output. ---
    {
        const std::uint32_t a = murmur3_32("player_health", 0);
        const std::uint32_t b = murmur3_32("player_healthy", 0); // one extra byte
        // Count differing bits; a good hash flips roughly half of the 32.
        int diff = 0;
        std::uint32_t x = a ^ b;
        while (x) { diff += static_cast<int>(x & 1u); x >>= 1; }
        CHECK(diff >= 8, "a small input change flips many output bits");
    }

    // --- 4. Low collision rate over many distinct keys. ---
    {
        std::set<std::uint32_t> seen;
        int collisions = 0;
        for (int i = 0; i < 20000; ++i) {
            const std::string key = "entity_" + std::to_string(i);
            const std::uint32_t h = murmur3_32(key, 0);
            if (!seen.insert(h).second) ++collisions;
        }
        // With 20k keys in a 2^32 space, expected accidental collisions ~ 0.05; allow a tiny slack.
        CHECK(collisions <= 2, "distinct keys hash with almost no collisions");
    }

    if (g_fail == 0) {
        std::printf("murmur3: OK — canonical vectors, determinism, seed, avalanche, low collisions.\n");
        return 0;
    }
    std::printf("murmur3: %d failure(s).\n", g_fail);
    return 1;
}
