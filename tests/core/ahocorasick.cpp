// tests/core/ahocorasick.cpp — verifies multi-pattern matching (core AhoCorasick.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a handful of overlapping patterns ("he","she","his","hers") in "ushers" match exactly where expected;
//   * containsAny / countMatches agree with the full match list;
//   * empty patterns are ignored; matching before build() is a no-op;
//   * on thousands of random texts + random small-alphabet pattern sets, findAll() returns EXACTLY the same
//     set of (begin,end,pattern) matches as a brute-force per-pattern std::string::find scan — overlaps,
//     patterns that are suffixes of others, and repeats all included.
#include "maz/core/AhoCorasick.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using maz::core::AhoCorasick;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

struct M {
    std::size_t begin, end;
    int pat;
    bool operator<(const M& o) const {
        if (begin != o.begin) return begin < o.begin;
        if (end != o.end) return end < o.end;
        return pat < o.pat;
    }
    bool operator==(const M& o) const { return begin == o.begin && end == o.end && pat == o.pat; }
};

// Reference: for each pattern, find all (overlapping) occurrences with std::string::find.
static std::vector<M> brute(const std::vector<std::string>& pats, const std::string& text) {
    std::vector<M> out;
    for (int p = 0; p < static_cast<int>(pats.size()); ++p) {
        const std::string& pat = pats[static_cast<std::size_t>(p)];
        if (pat.empty()) continue;
        std::size_t pos = text.find(pat, 0);
        while (pos != std::string::npos) {
            out.push_back(M{pos, pos + pat.size(), p});
            pos = text.find(pat, pos + 1); // overlapping occurrences
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

int main() {
    // --- 1. Classic textbook case. ---
    {
        AhoCorasick ac;
        const int he = ac.addPattern("he");
        const int she = ac.addPattern("she");
        const int his = ac.addPattern("his");
        const int hers = ac.addPattern("hers");
        ac.build();
        const auto ms = ac.findAll("ushers");
        // "ushers": she@[1,4), he@[2,4), hers@[2,6)
        std::vector<M> got;
        for (const auto& m : ms) got.push_back(M{m.begin, m.end, m.pattern});
        std::sort(got.begin(), got.end());
        std::vector<M> want{{1, 4, she}, {2, 4, he}, {2, 6, hers}};
        std::sort(want.begin(), want.end());
        CHECK(got == want, "ushers matches she/he/hers at the right spots");
        CHECK(his >= 0 && ac.containsAny("this is his"), "containsAny finds an embedded pattern");
        CHECK(!ac.containsAny("xyzzy"), "containsAny false when nothing matches");
        CHECK(ac.countMatches("ushers") == 3, "countMatches agrees with findAll size");
    }

    // --- 2. Empty patterns ignored; pre-build matching is a no-op. ---
    {
        AhoCorasick ac;
        CHECK(ac.addPattern("") == -1, "empty pattern ignored");
        ac.addPattern("ab");
        CHECK(ac.findAll("abab").empty(), "no matches before build()");
        ac.build();
        CHECK(ac.countMatches("abab") == 2, "two matches of 'ab' in 'abab' after build");
        CHECK(ac.patternCount() == 1, "only the non-empty pattern was stored");
    }

    // --- 3. Randomized cross-check against brute force. ---
    {
        Lcg rng{0xACAC01u};
        bool ok = true;
        long totalMatches = 0;
        for (int trial = 0; trial < 4000 && ok; ++trial) {
            const int alpha = 2 + static_cast<int>(rng.next() % 3u); // alphabet 'a'..('a'+1..3): forces overlaps
            const int np = 1 + static_cast<int>(rng.next() % 6u);    // 1..6 patterns
            std::vector<std::string> pats;
            AhoCorasick ac;
            for (int p = 0; p < np; ++p) {
                const int len = 1 + static_cast<int>(rng.next() % 4u); // 1..4 chars
                std::string s;
                for (int k = 0; k < len; ++k)
                    s.push_back(static_cast<char>('a' + static_cast<int>(rng.next() % static_cast<std::uint32_t>(alpha))));
                pats.push_back(s);
                ac.addPattern(s);
            }
            ac.build();

            const int tlen = static_cast<int>(rng.next() % 30u); // 0..29
            std::string text;
            for (int k = 0; k < tlen; ++k)
                text.push_back(static_cast<char>('a' + static_cast<int>(rng.next() % static_cast<std::uint32_t>(alpha))));

            std::vector<M> got;
            for (const auto& m : ac.findAll(text)) got.push_back(M{m.begin, m.end, m.pattern});
            std::sort(got.begin(), got.end());
            const std::vector<M> want = brute(pats, text);
            if (got != want) { ok = false; break; }
            totalMatches += static_cast<long>(got.size());
        }
        CHECK(ok, "findAll matches brute force across random pattern sets and texts");
        CHECK(totalMatches > 1000, "the random trials actually produced many matches");
    }

    if (g_fail == 0) {
        std::printf("ahocorasick: OK — textbook case, containsAny, empties, 4000-trial brute-force parity.\n");
        return 0;
    }
    std::printf("ahocorasick: %d failure(s).\n", g_fail);
    return 1;
}
