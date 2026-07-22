// tests/core/graycode.cpp — verifies Gray code + Hamming distance (core GrayCode.hpp).
// Ground truths, deterministic:
//   * known small Gray codes: gray(0..7) = 0,1,3,2,6,7,5,4;
//   * grayEncode / grayDecode round-trip for every value in a wide range and several widths;
//   * the DEFINING property: consecutive Gray codes differ by exactly one bit (Hamming distance 1);
//   * graySequence(bits) is a permutation of [0, 2^bits) with adjacent Hamming distance 1;
//   * hammingDistance basics (self=0, full flip, single bit) and symmetry.
#include "maz/core/GrayCode.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    using maz::core::grayDecode;
    using maz::core::grayEncode;
    using maz::core::graySequence;
    using maz::core::hammingDistance;

    // --- 1. Known small values. ---
    {
        const std::uint32_t expected[8] = {0, 1, 3, 2, 6, 7, 5, 4};
        bool ok = true;
        for (std::uint32_t i = 0; i < 8; ++i)
            if (grayEncode(i) != expected[i]) ok = false;
        CHECK(ok, "gray(0..7) = 0,1,3,2,6,7,5,4");
    }

    // --- 2. Round trip over a wide range and multiple widths. ---
    {
        bool ok = true;
        for (std::uint32_t n = 0; n < 200000u; ++n)
            if (grayDecode(grayEncode(n)) != n) ok = false;
        CHECK(ok, "grayDecode(grayEncode(n)) == n for 0..199999 (uint32)");

        bool ok8 = true;
        for (std::uint32_t v = 0; v < 256u; ++v) {
            const std::uint8_t b = static_cast<std::uint8_t>(v);
            if (grayDecode(grayEncode(b)) != b) ok8 = false;
        }
        CHECK(ok8, "round trip holds for every uint8 value");

        bool ok64 = true;
        const std::uint64_t samples[5] = {0ull, 1ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull,
                                          0x0123456789ABCDEFull};
        for (std::uint64_t s : samples)
            if (grayDecode(grayEncode(s)) != s) ok64 = false;
        CHECK(ok64, "round trip holds for uint64 edge values");
    }

    // --- 3. Consecutive Gray codes differ by exactly one bit. ---
    {
        bool ok = true;
        for (std::uint32_t n = 0; n < 100000u; ++n) {
            const std::uint32_t g0 = grayEncode(n);
            const std::uint32_t g1 = grayEncode(n + 1u);
            if (hammingDistance(g0, g1) != 1) ok = false;
        }
        CHECK(ok, "adjacent Gray codes are exactly one bit apart");
    }

    // --- 4. graySequence: permutation + adjacency. ---
    {
        const int bits = 8;
        const std::vector<std::uint32_t> seq = graySequence(bits);
        CHECK(seq.size() == 256u, "graySequence(8) has 2^8 entries");
        // Permutation of [0,256): every value appears exactly once.
        std::vector<char> seen(256, 0);
        bool perm = true;
        for (std::uint32_t g : seq) {
            if (g >= 256u || seen[g]) perm = false;
            else seen[g] = 1;
        }
        CHECK(perm, "graySequence(8) is a permutation of 0..255");
        // Adjacent entries differ by one bit (and the sequence is cyclic: last->first too).
        bool adj = true;
        for (std::size_t i = 1; i < seq.size(); ++i)
            if (hammingDistance(seq[i - 1], seq[i]) != 1) adj = false;
        if (hammingDistance(seq.back(), seq.front()) != 1) adj = false;
        CHECK(adj, "graySequence(8) neighbours (and the wrap) are one bit apart");
    }

    // --- 5. hammingDistance basics. ---
    {
        CHECK(hammingDistance(0xFFu, 0xFFu) == 0, "distance to self is 0");
        CHECK(hammingDistance(std::uint8_t{0x00}, std::uint8_t{0xFF}) == 8, "full flip is 8 bits");
        CHECK(hammingDistance(0b1010u, 0b1000u) == 1, "one differing bit");
        CHECK(hammingDistance(0b1100u, 0b0011u) == 4, "four differing bits");
        CHECK(hammingDistance(12345u, 67890u) == hammingDistance(67890u, 12345u), "symmetric");
    }

    if (g_fail == 0) {
        std::printf("graycode: OK — known values, round trip, one-bit adjacency, permutation, hamming.\n");
        return 0;
    }
    std::printf("graycode: %d failure(s).\n", g_fail);
    return 1;
}
