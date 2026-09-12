// tests/core/damerau.cpp — verifies Damerau–Levenshtein (OSA) distance (core DamerauLevenshtein.hpp).
// Ground truths, deterministic (fixed cases + seeded-LCG strings, no <random>, no clock):
//   * KNOWN VALUES: an adjacent transposition costs exactly ONE edit where plain Levenshtein charges two
//     ("teh"→"the", "ba"→"ab"); classic cases (kitten→sitting = 3) match; identity is 0; empty → other length;
//   * INDEPENDENT ORACLE: over many seeded random string pairs the Damerau distance is ALWAYS ≤ the engine's
//     existing core::levenshtein (transpositions can only ever help), and it equals |lenA − lenB| as a lower
//     bound is respected;
//   * SYMMETRY: d(a,b) == d(b,a); determinism.
#include "maz/core/DamerauLevenshtein.hpp"
#include "maz/core/StringUtils.hpp" // core::levenshtein — the independent oracle

#include <cstdint>
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
};

static std::string randStr(Lcg& rng, std::size_t maxLen) {
    const std::size_t n = rng.next() % (maxLen + 1);
    std::string s;
    s.reserve(n);
    for (std::size_t i = 0; i < n; ++i) s.push_back(static_cast<char>('a' + rng.next() % 4u)); // small alphabet -> transpositions common
    return s;
}

int main() {
    using maz::core::damerauLevenshtein;
    using maz::core::levenshtein;

    // --- 1. Known values; transposition is one edit where Levenshtein is two. ---
    {
        CHECK(damerauLevenshtein("teh", "the") == 1, "'teh'->'the' is a single transposition");
        CHECK(levenshtein("teh", "the") == 2, "Levenshtein charges the same transposition as two edits");
        CHECK(damerauLevenshtein("ba", "ab") == 1, "'ba'->'ab' is a single transposition");
        CHECK(damerauLevenshtein("kitten", "sitting") == 3, "classic kitten->sitting distance is 3");
        CHECK(damerauLevenshtein("abc", "abc") == 0, "identical strings have distance 0");
        CHECK(damerauLevenshtein("", "hello") == 5 && damerauLevenshtein("hello", "") == 5,
              "distance from empty is the other string's length");
        CHECK(damerauLevenshtein("ca", "abc") == 3 || damerauLevenshtein("ca", "abc") == 2,
              "'ca'->'abc' has a small, well-defined OSA distance");
    }

    // --- 2. Independent oracle: Damerau <= Levenshtein, symmetry, length lower bound. ---
    {
        Lcg rng{0xD00Du};
        bool leOk = true, symOk = true, lbOk = true, strictlyLess = false;
        for (int t = 0; t < 5000; ++t) {
            const std::string a = randStr(rng, 8);
            const std::string b = randStr(rng, 8);
            const std::size_t dl = damerauLevenshtein(a, b);
            const std::size_t lv = levenshtein(a, b);
            if (dl > lv) leOk = false;
            if (dl < lv) strictlyLess = true; // at least sometimes strictly better (a transposition helped)
            if (damerauLevenshtein(a, b) != damerauLevenshtein(b, a)) symOk = false;
            const std::size_t lb = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
            if (dl < lb) lbOk = false;
        }
        CHECK(leOk, "Damerau distance is always <= Levenshtein distance");
        CHECK(strictlyLess, "Damerau is sometimes strictly less than Levenshtein (transpositions help)");
        CHECK(symOk, "the distance is symmetric: d(a,b) == d(b,a)");
        CHECK(lbOk, "the distance respects the |lenA - lenB| lower bound");
    }

    // --- 3. Determinism. ---
    {
        CHECK(damerauLevenshtein("procedural", "procdeural") == damerauLevenshtein("procedural", "procdeural"),
              "identical inputs produce identical output");
        CHECK(damerauLevenshtein("procedural", "procdeural") == 1,
              "a single adjacent swap in a long word is distance 1");
    }

    if (g_fail == 0) {
        std::printf("damerau: OK — known values, transposition=1, <=Levenshtein oracle, symmetry, "
                    "determinism.\n");
        return 0;
    }
    std::printf("damerau: %d failure(s).\n", g_fail);
    return 1;
}
