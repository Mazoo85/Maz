// tests/game/elo.cpp — verifies the Elo rating system (game Elo.hpp).
// Ground truths, hand-computed, deterministic:
//   * equal ratings -> 0.5 expected score each; expectedScore(A,B)+expectedScore(B,A)==1;
//   * a 400-point lead gives exactly 10/11 (~0.909) expected score;
//   * an equal-rating win moves each player by exactly K/2 (winner +, loser -);
//   * the exchange is zero-sum: the winner gains exactly what the loser loses (with a shared K);
//   * the underdog gains more for the same win than the favorite would;
//   * a draw between unequal players nudges the lower-rated up and the higher-rated down;
//   * the provisional K-factor is higher than the established one.
#include "maz/game/Elo.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::eloDelta;
using maz::game::eloExpectedScore;
using maz::game::eloKFactor;
using maz::game::EloPair;
using maz::game::eloPlay;
using maz::game::eloUpdate;

static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Equal ratings -> 50/50, and expectations sum to 1. ---
    {
        CHECK(near(eloExpectedScore(1500.0f, 1500.0f), 0.5f), "equal ratings give 0.5 expected score");
        CHECK(near(eloExpectedScore(1600.0f, 1400.0f) + eloExpectedScore(1400.0f, 1600.0f), 1.0f),
              "expected scores of the two sides sum to 1");
    }

    // --- 2. A 400-point lead -> 10/11. ---
    {
        CHECK(near(eloExpectedScore(1800.0f, 1400.0f), 10.0f / 11.0f, 1e-3f),
              "a 400-point advantage is a 10/11 expected score");
    }

    // --- 3. Equal-rating win moves each by K/2. ---
    {
        const float k = 32.0f;
        const float winnerNew = eloUpdate(1500.0f, 1500.0f, 1.0f, k);
        const float loserNew = eloUpdate(1500.0f, 1500.0f, 0.0f, k);
        CHECK(near(winnerNew, 1500.0f + k * 0.5f), "equal-rating winner gains K/2");
        CHECK(near(loserNew, 1500.0f - k * 0.5f), "equal-rating loser drops K/2");
    }

    // --- 4. Zero-sum exchange with a shared K. ---
    {
        const EloPair r = eloPlay(1650.0f, 1500.0f, 1.0f, 24.0f); // A (favorite) wins
        const float total0 = 1650.0f + 1500.0f;
        CHECK(near(r.a + r.b, total0, 1e-3f), "total rating is conserved (winner gains what loser loses)");
        CHECK(r.a > 1650.0f && r.b < 1500.0f, "the winner rises and the loser falls");
    }

    // --- 5. Underdog gains more than the favorite would for the same win. ---
    {
        const float underdogGain = eloDelta(1400.0f, 1700.0f, 1.0f, 32.0f); // beats a stronger player
        const float favoriteGain = eloDelta(1700.0f, 1400.0f, 1.0f, 32.0f); // beats a weaker player
        CHECK(underdogGain > favoriteGain, "beating a stronger opponent is worth more");
        CHECK(underdogGain > 0.0f && favoriteGain > 0.0f, "both wins gain some rating");
    }

    // --- 6. Draw between unequal players. ---
    {
        const float lowNew = eloUpdate(1400.0f, 1700.0f, 0.5f, 32.0f);  // underdog draws
        const float highNew = eloUpdate(1700.0f, 1400.0f, 0.5f, 32.0f); // favorite draws
        CHECK(lowNew > 1400.0f, "the lower-rated player gains from a draw");
        CHECK(highNew < 1700.0f, "the higher-rated player loses from a draw");
    }

    // --- 7. Provisional K is higher than established K. ---
    {
        CHECK(eloKFactor(5) > eloKFactor(100), "new players have a larger (more volatile) K-factor");
    }

    if (g_fail == 0) {
        std::printf("elo: OK — equal 50/50, 400pt=10/11, K/2 swing, zero-sum, underdog bonus, draw, "
                    "provisional K.\n");
        return 0;
    }
    std::printf("elo: %d failure(s).\n", g_fail);
    return 1;
}
