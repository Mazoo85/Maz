// tests/core/diff.cpp — verifies the sequence diff / LCS (core Diff.hpp).
// Ground truths, deterministic (fixed + seeded-LCG sequences, no <random>, no clock):
//   * KNOWN LCS LENGTH: LCS("ABCBDAB","BDCAB") has length 4;
//   * SUBSEQUENCE (airtight): the LCS is a subsequence of BOTH inputs;
//   * EDIT SCRIPT RECONSTRUCTS (airtight): filtering diff to {Keep,Delete} reproduces A and {Keep,Insert}
//     reproduces B, for many random sequence pairs;
//   * KEEP COUNT == LCS LENGTH: the number of Keep entries equals the LCS length (minimal edit);
//   * EDGE CASES: identical -> all Keep; disjoint -> LCS empty, only Delete+Insert; empty inputs.
#include "maz/core/Diff.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using maz::core::DiffOp;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static std::vector<char> vec(const std::string& s) { return std::vector<char>(s.begin(), s.end()); }
static std::string str(const std::vector<char>& v) { return std::string(v.begin(), v.end()); }

// Is `sub` a subsequence of `seq`?
static bool isSubsequence(const std::vector<char>& sub, const std::vector<char>& seq) {
    std::size_t k = 0;
    for (std::size_t i = 0; i < seq.size() && k < sub.size(); ++i) {
        if (seq[i] == sub[k]) {
            ++k;
        }
    }
    return k == sub.size();
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 32); }
};

int main() {
    using namespace maz::core;

    // --- 1. Known LCS length + subsequence property. ---
    {
        const auto a = vec("ABCBDAB"), b = vec("BDCAB");
        const auto lcs = longestCommonSubsequence(a, b);
        CHECK(lcs.size() == 4, "LCS(ABCBDAB, BDCAB) has length 4");
        CHECK(isSubsequence(lcs, a) && isSubsequence(lcs, b), "the LCS is a subsequence of both inputs");
    }

    // --- 2. Edit script reconstructs A and B; Keep count equals LCS length. Random pairs. ---
    {
        Lcg rng{0xD1F0u};
        int tested = 0;
        for (int t = 0; t < 3000; ++t) {
            // Short strings over a tiny alphabet so common subsequences are frequent.
            std::vector<char> a, b;
            const int na = static_cast<int>(rng.next() % 9), nb = static_cast<int>(rng.next() % 9);
            for (int i = 0; i < na; ++i) {
                a.push_back(static_cast<char>('a' + rng.next() % 4));
            }
            for (int i = 0; i < nb; ++i) {
                b.push_back(static_cast<char>('a' + rng.next() % 4));
            }
            const auto d = diff(a, b);
            std::vector<char> rebuiltA, rebuiltB;
            int keeps = 0;
            for (const auto& e : d) {
                if (e.op == DiffOp::Keep) {
                    rebuiltA.push_back(e.value);
                    rebuiltB.push_back(e.value);
                    ++keeps;
                } else if (e.op == DiffOp::Delete) {
                    rebuiltA.push_back(e.value);
                } else {
                    rebuiltB.push_back(e.value);
                }
            }
            CHECK(rebuiltA == a, "Keep+Delete reproduces A");
            CHECK(rebuiltB == b, "Keep+Insert reproduces B");
            CHECK(keeps == static_cast<int>(longestCommonSubsequence(a, b).size()),
                  "the number of Keep entries equals the LCS length");
            ++tested;
        }
        CHECK(tested == 3000, "all random pairs were exercised");
    }

    // --- 3. Edge cases. ---
    {
        const auto s = vec("hello world");
        const auto same = diff(s, s);
        bool allKeep = true;
        for (const auto& e : same) {
            if (e.op != DiffOp::Keep) {
                allKeep = false;
            }
        }
        CHECK(allKeep && same.size() == s.size(), "identical inputs produce all-Keep");
        CHECK(str(longestCommonSubsequence(s, s)) == "hello world", "LCS of a sequence with itself is itself");

        const auto da = vec("abc"), db = vec("xyz");
        CHECK(longestCommonSubsequence(da, db).empty(), "disjoint inputs have empty LCS");
        int del = 0, ins = 0, keep = 0;
        for (const auto& e : diff(da, db)) {
            del += e.op == DiffOp::Delete;
            ins += e.op == DiffOp::Insert;
            keep += e.op == DiffOp::Keep;
        }
        CHECK(del == 3 && ins == 3 && keep == 0, "disjoint diff is 3 deletes + 3 inserts");

        CHECK(diff(std::vector<char>{}, std::vector<char>{}).empty(), "empty vs empty is an empty diff");
        CHECK(diff(std::vector<char>{}, vec("ab")).size() == 2, "empty vs 'ab' is two inserts");
    }

    if (g_fail == 0) {
        std::printf("diff: OK — known LCS, subsequence, reconstruction, keep-count, edge cases.\n");
        return 0;
    }
    std::printf("diff: %d failure(s).\n", g_fail);
    return 1;
}
