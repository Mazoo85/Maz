// tests/io/rangecoder.cpp — verifies arithmetic (range) coding round-trips (io RangeCoder.hpp).
// Ground truths, deterministic (seeded LCG for streams/models, no <random>, no clock):
//   * the airtight guarantee: rangeDecode(rangeEncode(s, freq), freq) == s for EVERY symbol stream and model
//     — across thousands of random alphabets, frequency tables, and lengths;
//   * skewed distributions actually compress below the fixed-width size (ceil(log2 alphabet) bits/symbol);
//   * degenerate cases (empty stream, single-symbol alphabet) round-trip.
#include "maz/io/RangeCoder.hpp"

#include <cmath>
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

int main() {
    // --- 1. Randomized round-trips over many alphabets/models/lengths. ---
    {
        Lcg rng{0x2C0DEu};
        bool ok = true;
        for (int t = 0; t < 3000 && ok; ++t) {
            const std::uint32_t alpha = 1 + rng.next() % 32u;
            std::vector<std::uint32_t> freq(alpha);
            std::uint32_t tot = 0;
            for (std::uint32_t& f : freq) {
                f = 1u + rng.next() % 40u;
                tot += f;
            }
            if (tot > (1u << 16)) continue; // model must fit the coder's total budget
            const std::uint32_t len = rng.next() % 500u;
            std::vector<std::uint32_t> syms(len);
            for (std::uint32_t& v : syms) v = rng.next() % alpha;
            const std::vector<std::uint8_t> enc = maz::io::rangeEncode(syms, freq);
            const std::vector<std::uint32_t> dec = maz::io::rangeDecode(enc, freq);
            if (dec != syms) ok = false;
        }
        CHECK(ok, "encode/decode round-trips exactly for random streams and models");
    }

    // --- 2. Skewed distribution compresses below fixed width. ---
    {
        // Alphabet of 4, but symbol 0 dominates -> ~ well under 2 bits/symbol.
        std::vector<std::uint32_t> freq{1000u, 10u, 10u, 5u};
        Lcg rng{0x5C0FFu};
        std::vector<std::uint32_t> syms(4000);
        for (std::uint32_t& v : syms) {
            const std::uint32_t r = rng.next() % 1025u;
            v = r < 1000u ? 0u : (r < 1010u ? 1u : (r < 1020u ? 2u : 3u));
        }
        const std::vector<std::uint8_t> enc = maz::io::rangeEncode(syms, freq);
        const std::vector<std::uint32_t> dec = maz::io::rangeDecode(enc, freq);
        CHECK(dec == syms, "skewed stream round-trips");
        // Fixed width would be 2 bits/symbol = 1000 bytes; range coder should be far smaller.
        CHECK(enc.size() < 700, "skewed distribution compresses well below fixed-width 2 bits/symbol");
    }

    // --- 3. Degenerate cases. ---
    {
        std::vector<std::uint32_t> freq{3u, 5u, 2u};
        const std::vector<std::uint32_t> empty;
        CHECK(maz::io::rangeDecode(maz::io::rangeEncode(empty, freq), freq).empty(), "empty stream round-trips");

        std::vector<std::uint32_t> oneAlpha{7u};
        std::vector<std::uint32_t> allZero(50, 0u); // only symbol 0 exists
        CHECK(maz::io::rangeDecode(maz::io::rangeEncode(allZero, oneAlpha), oneAlpha) == allZero,
              "single-symbol alphabet round-trips");
    }

    if (g_fail == 0) {
        std::printf("rangecoder: OK — random round-trips, skewed compression, degenerate cases.\n");
        return 0;
    }
    std::printf("rangecoder: %d failure(s).\n", g_fail);
    return 1;
}
