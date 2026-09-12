// tests/io/lzw.cpp — verifies LZW compression round-trips exactly (io Lzw.hpp).
// Ground truths, deterministic (seeded LCG for payloads, no <random>, no clock):
//   * the airtight guarantee: decompress(compress(x)) == x for EVERY input — empty, single byte, all-same
//     runs, small-alphabet repetitive data, full-256 random noise, and inputs large enough to fill and
//     FREEZE the 65536-entry dictionary;
//   * highly repetitive data actually shrinks (ratio < 1);
//   * a hand-built example round-trips.
#include "maz/io/Lzw.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

static bool roundTrips(const std::vector<std::uint8_t>& in) {
    return maz::io::lzwDecompress(maz::io::lzwCompress(in)) == in;
}

int main() {
    // --- 1. Degenerate + tiny inputs. ---
    {
        CHECK(roundTrips({}), "empty round-trips");
        CHECK(maz::io::lzwCompress({}).empty(), "empty compresses to empty");
        CHECK(roundTrips({42}), "single byte round-trips");
        CHECK(roundTrips({0, 0, 0, 0, 0, 0, 0, 0}), "run of zeros round-trips");
        CHECK(roundTrips({1, 2, 3, 4, 5}), "short distinct sequence round-trips");
    }

    // --- 2. Randomized round-trips across alphabets and sizes. ---
    {
        Lcg rng{0x12CE5u};
        bool ok = true;
        for (int trial = 0; trial < 1500 && ok; ++trial) {
            const int len = static_cast<int>(rng.next() % 800u);
            const int alpha = 1 + static_cast<int>(rng.next() % 256u); // 1..256 distinct byte values
            std::vector<std::uint8_t> in;
            in.reserve(static_cast<std::size_t>(len));
            for (int i = 0; i < len; ++i) {
                in.push_back(static_cast<std::uint8_t>(rng.next() % static_cast<std::uint32_t>(alpha)));
            }
            if (!roundTrips(in)) ok = false;
        }
        CHECK(ok, "random payloads (varied length + alphabet) round-trip exactly");
    }

    // --- 3. Full-entropy noise (worst case for compression, still exact). ---
    {
        Lcg rng{0xF00Du};
        std::vector<std::uint8_t> noise;
        for (int i = 0; i < 5000; ++i) noise.push_back(static_cast<std::uint8_t>(rng.next() & 0xFFu));
        CHECK(roundTrips(noise), "5000 bytes of full-range noise round-trip");
    }

    // --- 4. Large repetitive input: fills + freezes the dictionary, and actually compresses. ---
    {
        std::vector<std::uint8_t> big;
        Lcg rng{0xABCDu};
        // ~300k bytes with lots of structure so the dictionary saturates past 65536 entries.
        const std::uint8_t pattern[] = {7, 7, 7, 3, 9, 9, 1, 2, 3, 4, 5, 5, 5, 0};
        for (int i = 0; i < 300000; ++i) {
            if ((rng.next() & 7u) == 0u) big.push_back(static_cast<std::uint8_t>(rng.next() & 0xFFu));
            else big.push_back(pattern[static_cast<std::size_t>(i) % (sizeof(pattern))]);
        }
        CHECK(roundTrips(big), "300k structured bytes round-trip (dictionary freeze path exercised)");

        std::vector<std::uint8_t> repetitive(20000, 0);
        for (std::size_t i = 0; i < repetitive.size(); ++i)
            repetitive[i] = static_cast<std::uint8_t>("MAZ_ENGINE_"[i % 11]);
        const std::vector<std::uint8_t> packed = maz::io::lzwCompress(repetitive);
        CHECK(roundTrips(repetitive), "repetitive text round-trips");
        CHECK(packed.size() < repetitive.size(), "repetitive data actually compresses (ratio < 1)");
    }

    if (g_fail == 0) {
        std::printf("lzw: OK — round-trip (empty/tiny/random/noise/large-freeze), compression ratio.\n");
        return 0;
    }
    std::printf("lzw: %d failure(s).\n", g_fail);
    return 1;
}
