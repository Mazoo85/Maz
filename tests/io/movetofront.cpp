// tests/io/movetofront.cpp — verifies Move-To-Front coding (io MoveToFront.hpp).
// Ground truths, deterministic (seeded LCG + BWT pipeline, no <random>, no clock):
//   * AIRTIGHT ROUND-TRIP: mtfDecode(mtfEncode(x)) == x over random and structured data;
//   * a run of one symbol encodes to a leading index then zeros (the run collapses);
//   * PIPELINE: applied to a BWT of repetitive data, MTF yields a stream dominated by small values / zeros
//     (the low-entropy property that makes the following entropy coder effective);
//   * empty input round-trips;
//   * determinism.
#include "maz/io/MoveToFront.hpp"
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

static std::vector<std::uint8_t> bytes(const std::string& t) { return std::vector<std::uint8_t>(t.begin(), t.end()); }

int main() {
    // --- 1. Round-trip random + structured. ---
    {
        Lcg rng{0x4D7Fu};
        bool ok = true;
        for (int t = 0; t < 200; ++t) {
            const std::size_t n = rng.next() % 300u;
            std::vector<std::uint8_t> x(n);
            for (std::size_t i = 0; i < n; ++i) x[i] = static_cast<std::uint8_t>(rng.next() % 12u);
            if (maz::io::mtfDecode(maz::io::mtfEncode(x)) != x) ok = false;
        }
        CHECK(ok, "mtfDecode(mtfEncode(x)) reconstructs x for random data");
        const auto txt = bytes("the quick brown fox jumps over the lazy dog");
        CHECK(maz::io::mtfDecode(maz::io::mtfEncode(txt)) == txt, "text round-trips exactly");
    }

    // --- 2. A run collapses to a leading index then zeros. ---
    {
        std::vector<std::uint8_t> run(20, 200);
        const auto e = maz::io::mtfEncode(run);
        CHECK(e[0] == 200, "the first symbol of a run encodes to its byte value (identity table)");
        bool restZero = true;
        for (std::size_t i = 1; i < e.size(); ++i)
            if (e[i] != 0) restZero = false;
        CHECK(restZero, "the rest of a same-symbol run encodes to zeros");
    }

    // --- 3. BWT -> MTF pipeline yields mostly-small values. ---
    {
        std::string rep;
        for (int i = 0; i < 60; ++i) rep += "abcdabcd";
        const auto x = bytes(rep);
        const auto b = maz::io::bwtEncode(x);
        const auto m = maz::io::mtfEncode(b.data);
        int zeros = 0, small = 0;
        for (std::uint8_t v : m) {
            if (v == 0) ++zeros;
            if (v < 4) ++small;
        }
        CHECK(zeros > static_cast<int>(m.size()) / 2, "BWT+MTF produces a majority of zeros on repetitive data");
        CHECK(small > static_cast<int>(m.size()) * 3 / 4, "BWT+MTF is dominated by small values");
        // And the whole pipeline is reversible.
        const auto back = maz::io::bwtDecode(maz::io::mtfDecode(m), b.primaryIndex);
        CHECK(back == x, "the BWT+MTF pipeline round-trips");
    }

    // --- 4. Empty. ---
    {
        const std::vector<std::uint8_t> empty;
        CHECK(maz::io::mtfEncode(empty).empty() && maz::io::mtfDecode(empty).empty(), "empty input round-trips");
    }

    // --- 5. Determinism. ---
    {
        const auto x = bytes("determinism determinism determinism");
        CHECK(maz::io::mtfEncode(x) == maz::io::mtfEncode(x), "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("movetofront: OK — round-trip, run collapse, BWT+MTF pipeline, empty, determinism.\n");
        return 0;
    }
    std::printf("movetofront: %d failure(s).\n", g_fail);
    return 1;
}
