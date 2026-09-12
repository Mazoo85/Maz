// tests/io/bwt.cpp — verifies the Burrows-Wheeler Transform (io Bwt.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * AIRTIGHT ROUND-TRIP: bwtDecode(bwtEncode(x)) == x for random blocks and structured text of many sizes;
//   * CLUSTERING: on repetitive input the BWT output has MORE adjacent-equal bytes than the input — the
//     property that makes it compressible;
//   * empty and single-byte inputs round-trip;
//   * determinism.
#include "maz/io/Bwt.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
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

static std::vector<std::uint8_t> bytes(const std::string& t) {
    return std::vector<std::uint8_t>(t.begin(), t.end());
}
static int runs(const std::vector<std::uint8_t>& v) {
    int r = 0;
    for (std::size_t i = 1; i < v.size(); ++i)
        if (v[i] == v[i - 1]) ++r;
    return r;
}

int main() {
    // --- 1. Round-trip random blocks of many sizes. ---
    {
        Lcg rng{0xB147u};
        bool ok = true;
        int trials = 0;
        for (int t = 0; t < 200; ++t) {
            const std::size_t n = 1u + rng.next() % 300u;
            std::vector<std::uint8_t> x(n);
            for (std::size_t i = 0; i < n; ++i) x[i] = static_cast<std::uint8_t>(rng.next() % 8u); // small alphabet -> repeats
            const auto e = maz::io::bwtEncode(x);
            const auto d = maz::io::bwtDecode(e.data, e.primaryIndex);
            if (d != x) ok = false;
            ++trials;
        }
        CHECK(trials == 200 && ok, "bwtDecode(bwtEncode(x)) reconstructs x for random blocks");
    }

    // --- 2. Round-trip structured text. ---
    {
        const char* texts[] = {"banana", "mississippi", "abracadabra",
                               "the quick brown fox jumps over the lazy dog"};
        bool ok = true;
        for (const char* t : texts) {
            const auto x = bytes(t);
            const auto e = maz::io::bwtEncode(x);
            if (maz::io::bwtDecode(e.data, e.primaryIndex) != x) ok = false;
        }
        CHECK(ok, "structured text round-trips exactly");
    }

    // --- 3. Clustering: BWT increases adjacent-equal runs on repetitive input. ---
    {
        std::string rep;
        for (int i = 0; i < 40; ++i) rep += "abcdabcd";
        const auto x = bytes(rep);
        const auto e = maz::io::bwtEncode(x);
        CHECK(runs(e.data) > runs(x), "BWT clusters symbols: more adjacent-equal bytes than the input");
        CHECK(maz::io::bwtDecode(e.data, e.primaryIndex) == x, "the repetitive block still round-trips");
    }

    // --- 4. Edge cases. ---
    {
        const std::vector<std::uint8_t> empty;
        const auto ee = maz::io::bwtEncode(empty);
        CHECK(ee.data.empty() && maz::io::bwtDecode(ee.data, ee.primaryIndex).empty(), "empty input round-trips");
        const std::vector<std::uint8_t> one{42};
        const auto eo = maz::io::bwtEncode(one);
        CHECK(maz::io::bwtDecode(eo.data, eo.primaryIndex) == one, "single byte round-trips");
    }

    // --- 5. Determinism. ---
    {
        const auto x = bytes("determinism check determinism check");
        const auto a = maz::io::bwtEncode(x);
        const auto b = maz::io::bwtEncode(x);
        CHECK(a.data == b.data && a.primaryIndex == b.primaryIndex, "identical inputs produce identical transforms");
    }

    if (g_fail == 0) {
        std::printf("bwt: OK — round-trip random/text, clustering, edge cases, determinism.\n");
        return 0;
    }
    std::printf("bwt: %d failure(s).\n", g_fail);
    return 1;
}
