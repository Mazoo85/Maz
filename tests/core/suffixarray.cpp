// tests/core/suffixarray.cpp — verifies the suffix array + LCP (core SuffixArray.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * the classic "banana" suffix array matches the known answer;
//   * over thousands of random small-alphabet strings, the suffix array equals a brute-force lexicographic
//     sort of all suffixes, and the LCP array matches a direct longest-common-prefix computation;
//   * contains()/count() agree with a brute-force scan (std::string::find) for present and absent patterns;
//   * longestRepeatedLength() equals the true longest repeated substring length;
//   * empty string is handled.
#include "maz/core/SuffixArray.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using maz::core::SuffixArray;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

// Brute-force suffix array: sort all suffix start indices by the suffix string.
static std::vector<int> bruteSA(const std::string& s) {
    std::vector<int> sa(s.size());
    for (int i = 0; i < static_cast<int>(s.size()); ++i) sa[static_cast<std::size_t>(i)] = i;
    std::sort(sa.begin(), sa.end(), [&s](int a, int b) { return s.substr(static_cast<std::size_t>(a)) <
                                                                s.substr(static_cast<std::size_t>(b)); });
    return sa;
}

static int commonPrefix(const std::string& s, int a, int b) {
    int h = 0;
    const int n = static_cast<int>(s.size());
    while (a + h < n && b + h < n && s[static_cast<std::size_t>(a + h)] == s[static_cast<std::size_t>(b + h)]) ++h;
    return h;
}

// Brute-force count of overlapping occurrences of `pat` in `s`.
static int bruteCount(const std::string& s, const std::string& pat) {
    if (pat.empty()) return static_cast<int>(s.size()) + 1;
    int c = 0;
    std::size_t pos = s.find(pat, 0);
    while (pos != std::string::npos) { ++c; pos = s.find(pat, pos + 1); }
    return c;
}

int main() {
    // --- 1. Known example. ---
    {
        SuffixArray sa("banana");
        // Suffixes sorted: a(5), ana(3), anana(1), banana(0), na(4), nana(2)
        const std::vector<int> want{5, 3, 1, 0, 4, 2};
        CHECK(sa.suffixArray() == want, "banana suffix array matches the known answer");
        CHECK(sa.contains("ana") && sa.count("ana") == 2, "'ana' occurs twice in banana");
        CHECK(!sa.contains("xyz"), "'xyz' is absent");
        CHECK(sa.longestRepeatedLength() == 3, "longest repeated substring in banana is 'ana' (len 3)");
    }

    // --- 2. Randomized cross-check against brute force. ---
    {
        Lcg rng{0x5A0FAu};
        bool saOk = true, lcpOk = true, searchOk = true, lrsOk = true;
        for (int trial = 0; trial < 3000 && saOk && lcpOk && searchOk && lrsOk; ++trial) {
            const int alpha = 2 + static_cast<int>(rng.next() % 3u); // tiny alphabet -> many repeats
            const int len = static_cast<int>(rng.next() % 40u);
            std::string s;
            for (int i = 0; i < len; ++i)
                s.push_back(static_cast<char>('a' + static_cast<int>(rng.next() % static_cast<std::uint32_t>(alpha))));

            SuffixArray sa(s);
            if (sa.suffixArray() != bruteSA(s)) { saOk = false; break; }

            // LCP check.
            const auto& sav = sa.suffixArray();
            const auto& lcp = sa.lcpArray();
            int trueLrs = 0;
            for (std::size_t i = 1; i < sav.size(); ++i) {
                const int cp = commonPrefix(s, sav[i - 1], sav[i]);
                if (lcp[i] != cp) { lcpOk = false; break; }
                trueLrs = std::max(trueLrs, cp);
            }
            if (sa.longestRepeatedLength() != trueLrs) { lrsOk = false; break; }

            // Search a few patterns (present substrings + random ones).
            for (int q = 0; q < 4; ++q) {
                std::string pat;
                if (!s.empty() && (rng.next() & 1u)) {
                    const int a = static_cast<int>(rng.next() % static_cast<std::uint32_t>(s.size()));
                    const int b = a + 1 + static_cast<int>(rng.next() % 4u);
                    pat = s.substr(static_cast<std::size_t>(a),
                                   static_cast<std::size_t>(std::min(b, static_cast<int>(s.size())) - a));
                } else {
                    const int pl = 1 + static_cast<int>(rng.next() % 4u);
                    for (int i = 0; i < pl; ++i)
                        pat.push_back(static_cast<char>('a' + static_cast<int>(rng.next() % static_cast<std::uint32_t>(alpha + 1))));
                }
                const bool has = s.find(pat) != std::string::npos;
                if (sa.contains(pat) != has || sa.count(pat) != bruteCount(s, pat)) { searchOk = false; break; }
            }
        }
        CHECK(saOk, "suffix array matches a brute-force suffix sort");
        CHECK(lcpOk, "LCP array matches direct common-prefix computation");
        CHECK(searchOk, "contains()/count() match a brute-force std::string::find scan");
        CHECK(lrsOk, "longestRepeatedLength() equals max(LCP)");
    }

    // --- 3. Empty string. ---
    {
        SuffixArray sa("");
        CHECK(sa.size() == 0 && sa.contains("") && !sa.contains("a"), "empty string handled");
    }

    if (g_fail == 0) {
        std::printf("suffixarray: OK — banana, brute-force SA/LCP parity, search, longest-repeat, empty.\n");
        return 0;
    }
    std::printf("suffixarray: %d failure(s).\n", g_fail);
    return 1;
}
