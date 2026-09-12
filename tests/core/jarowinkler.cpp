// tests/core/jarowinkler.cpp — verifies Jaro & Jaro-Winkler similarity (core JaroWinkler.hpp).
// Ground truths, deterministic (published reference values from the Winkler literature):
//   * identical strings score 1; empty/edge cases behave;
//   * jaro("MARTHA","MARHTA") = 0.9444..., jaroWinkler = 0.9611...;
//   * jaro("DIXON","DICKSONX") = 0.7666..., jaroWinkler = 0.8133...;
//   * jaro("DWAYNE","DUANE") = 0.8222..., jaroWinkler = 0.8400;
//   * scores are symmetric and stay within [0,1];
//   * the Winkler prefix boost only raises the score (never below Jaro), and a "did you mean" ranking
//     picks the intended correction.
#include "maz/core/JaroWinkler.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b) { return std::fabs(a - b) < 1e-4; }

int main() {
    using maz::core::jaro;
    using maz::core::jaroWinkler;

    // --- 1. Identity and empties. ---
    {
        CHECK(near(jaro("hello", "hello"), 1.0), "identical strings -> jaro 1");
        CHECK(near(jaroWinkler("hello", "hello"), 1.0), "identical strings -> jaroWinkler 1");
        CHECK(near(jaro("", ""), 1.0), "two empties -> 1");
        CHECK(near(jaro("abc", ""), 0.0), "one empty -> 0");
        CHECK(near(jaro("abc", "xyz"), 0.0), "no common chars -> 0");
    }

    // --- 2. Published reference values. ---
    {
        CHECK(near(jaro("MARTHA", "MARHTA"), 0.944444), "jaro MARTHA/MARHTA");
        CHECK(near(jaroWinkler("MARTHA", "MARHTA"), 0.961111), "jaroWinkler MARTHA/MARHTA");
        CHECK(near(jaro("DIXON", "DICKSONX"), 0.766667), "jaro DIXON/DICKSONX");
        CHECK(near(jaroWinkler("DIXON", "DICKSONX"), 0.813333), "jaroWinkler DIXON/DICKSONX");
        CHECK(near(jaro("DWAYNE", "DUANE"), 0.822222), "jaro DWAYNE/DUANE");
        CHECK(near(jaroWinkler("DWAYNE", "DUANE"), 0.84), "jaroWinkler DWAYNE/DUANE");
    }

    // --- 3. Symmetry and range. ---
    {
        const char* words[] = {"kitten", "sitting", "flaw", "lawn", "MARTHA", "MARHTA", "", "a"};
        bool sym = true, range = true;
        for (const char* x : words) {
            for (const char* y : words) {
                const double d1 = jaroWinkler(x, y);
                const double d2 = jaroWinkler(y, x);
                if (!near(d1, d2)) sym = false;
                if (d1 < -1e-9 || d1 > 1.0 + 1e-9) range = false;
            }
        }
        CHECK(sym, "jaroWinkler is symmetric");
        CHECK(range, "jaroWinkler stays within [0,1]");
    }

    // --- 4. Winkler prefix boost never lowers the score. ---
    {
        const char* pairs[][2] = {{"jones", "johnson"}, {"abcvxz", "abcxyz"}, {"prefix", "prefab"}};
        bool ok = true;
        for (auto& p : pairs)
            if (jaroWinkler(p[0], p[1]) + 1e-12 < jaro(p[0], p[1])) ok = false;
        CHECK(ok, "the prefix boost never drops below plain Jaro");
    }

    // --- 5. "Did you mean" ranking picks the intended command. ---
    {
        const std::string typed = "attak";
        const std::vector<std::string> commands = {"attack", "defend", "retreat", "status", "inventory"};
        std::string best;
        double bestScore = -1.0;
        for (const std::string& c : commands) {
            const double s = jaroWinkler(typed, c);
            if (s > bestScore) {
                bestScore = s;
                best = c;
            }
        }
        CHECK(best == "attack", "'attak' best-matches 'attack'");
        CHECK(bestScore > 0.9, "the intended match scores highly");
    }

    if (g_fail == 0) {
        std::printf("jarowinkler: OK — identity, reference values, symmetry/range, prefix boost, ranking.\n");
        return 0;
    }
    std::printf("jarowinkler: %d failure(s).\n", g_fail);
    return 1;
}
