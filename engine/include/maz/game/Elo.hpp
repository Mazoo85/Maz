#pragma once

#include <cmath>

// maz::game Elo rating — the ranking + matchmaking math behind ranked ladders, leaderboards, and
// difficulty matching (Arpad Elo's system, as used by chess and virtually every competitive game).
//
// Each competitor carries a single number (the rating). Before a match, the ratings predict the
// probability each side wins; after it, both ratings move by an amount that depends on how SURPRISING
// the result was — beating a much stronger opponent gains a lot, beating a much weaker one gains almost
// nothing, and the exchange is zero-sum (the winner gains exactly what the loser drops). The K-factor
// controls volatility: large K (new/provisional players) moves ratings fast, small K (established
// players) keeps them stable. Use it for ranked matchmaking, seeding brackets, or scaling AI difficulty
// to a player's measured skill. Pure value math, header-only, deterministic — unit-tested against the
// exact Elo formulas and the conservation (zero-sum) property.
namespace maz::game {

// Probability that a player rated `ratingA` beats a player rated `ratingB` (their "expected score",
// 0..1). Draws count as half; expectedScore(A,B) + expectedScore(B,A) == 1 exactly.
inline float eloExpectedScore(float ratingA, float ratingB) {
    return 1.0f / (1.0f + std::pow(10.0f, (ratingB - ratingA) / 400.0f));
}

// New rating for a player after one game: rating + K * (actualScore - expectedScore). `actualScore` is
// 1 for a win, 0.5 for a draw, 0 for a loss. `k` is the K-factor (volatility).
inline float eloUpdate(float rating, float opponentRating, float actualScore, float k = 32.0f) {
    return rating + k * (actualScore - eloExpectedScore(rating, opponentRating));
}

// The rating change (delta) a player receives for a result — eloUpdate minus the starting rating.
inline float eloDelta(float rating, float opponentRating, float actualScore, float k = 32.0f) {
    return k * (actualScore - eloExpectedScore(rating, opponentRating));
}

struct EloPair {
    float a = 0.0f;
    float b = 0.0f;
};

// Update BOTH players from a single game in one call. `scoreA` is A's result (1 win / 0.5 draw / 0
// loss); B's is 1 - scoreA. With a shared K the total rating is conserved (zero-sum). Returns the new
// ratings.
inline EloPair eloPlay(float ratingA, float ratingB, float scoreA, float k = 32.0f) {
    EloPair out;
    out.a = eloUpdate(ratingA, ratingB, scoreA, k);
    out.b = eloUpdate(ratingB, ratingA, 1.0f - scoreA, k);
    return out;
}

// A reasonable K-factor schedule: high while a player is provisional (few games), then settling to a
// stable value — mirrors how real ladders treat newcomers vs veterans.
inline float eloKFactor(int gamesPlayed, float provisionalK = 40.0f, float establishedK = 20.0f,
                        int provisionalGames = 30) {
    return gamesPlayed < provisionalGames ? provisionalK : establishedK;
}

} // namespace maz::game
