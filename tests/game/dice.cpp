// tests/game/dice.cpp — verifies the dice-notation parser/roller (game::parseDice / rollDice / minRoll / maxRoll /
// averageRoll). Ground truths: well-formed strings parse to the right count/sides/modifier (case-insensitive 'd',
// optional count, +/- modifier); malformed strings are rejected; min/max/average match the closed-form bounds; a
// roll with a seeded RNG yields the right number of dice, each face in [1,sides], a total within [min,max], and is
// reproducible for the same seed. Pure CPU, deterministic.
#include "maz/game/Dice.hpp"
#include "maz/core/Pcg32.hpp"

#include <cmath>
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz;

int main() {
    // --- 1. Well-formed parses. ---
    {
        const game::DiceSpec a = game::parseDice("2d6+3");
        CHECK(a.valid && a.count == 2 && a.sides == 6 && a.modifier == 3, "2d6+3 parses");
        const game::DiceSpec b = game::parseDice("d20");
        CHECK(b.valid && b.count == 1 && b.sides == 20 && b.modifier == 0, "d20 -> 1 die, no modifier");
        const game::DiceSpec c = game::parseDice("4d8-1");
        CHECK(c.valid && c.count == 4 && c.sides == 8 && c.modifier == -1, "4d8-1 negative modifier");
        const game::DiceSpec e = game::parseDice(" 10D10 ");
        CHECK(e.valid && e.count == 10 && e.sides == 10, "uppercase D and surrounding spaces");
    }

    // --- 2. Malformed strings are rejected. ---
    {
        const char* bad[] = {"", "6", "d", "2d", "2x6", "2d6+", "2d6-", "2d6 x", "0d6", "2d0", "-2d6", "2d6+2extra"};
        for (const char* s : bad) CHECK(!game::parseDice(s).valid, "malformed dice string is rejected");
    }

    // --- 3. Distribution bounds match the closed form. ---
    {
        const game::DiceSpec d = game::parseDice("2d6+3");
        CHECK(game::minRoll(d) == 5, "2d6+3 min = 2*1+3 = 5");
        CHECK(game::maxRoll(d) == 15, "2d6+3 max = 2*6+3 = 15");
        CHECK(std::fabs(game::averageRoll(d) - 10.0) < 1e-9, "2d6+3 average = 2*3.5+3 = 10");
        const game::DiceSpec v; // default-invalid
        CHECK(game::minRoll(v) == 0 && game::maxRoll(v) == 0 && game::averageRoll(v) == 0.0, "invalid spec -> 0 bounds");
    }

    // --- 4. Rolls: right count, each face in range, total within bounds, reproducible by seed. ---
    {
        const game::DiceSpec d = game::parseDice("4d6+2");
        core::Pcg32 rng(12345u, 67u);
        for (int trial = 0; trial < 200; ++trial) {
            const game::RollResult r = game::rollDice(d, rng);
            CHECK(r.rolls.size() == 4, "roll produces `count` dice");
            bool inFace = true;
            for (int f : r.rolls) if (f < 1 || f > 6) inFace = false;
            CHECK(inFace, "every die face is within [1, sides]");
            CHECK(r.total >= game::minRoll(d) && r.total <= game::maxRoll(d), "total within [min, max]");
        }
        // Same seed reproduces the exact sequence.
        core::Pcg32 r1(999u, 1u), r2(999u, 1u);
        const game::RollResult a = game::rollDice(d, r1);
        const game::RollResult b = game::rollDice(d, r2);
        CHECK(a.total == b.total && a.rolls == b.rolls, "same seed -> identical roll");
    }

    // --- 5. Convenience parse+roll overload reports the parsed spec and rejects bad input. ---
    {
        core::Pcg32 rng(7u, 7u);
        game::DiceSpec spec;
        const game::RollResult ok = game::rollDice(std::string("3d4"), rng, &spec);
        CHECK(spec.valid && ok.rolls.size() == 3, "convenience overload rolls a valid expression");
        game::DiceSpec bad;
        const game::RollResult none = game::rollDice(std::string("nonsense"), rng, &bad);
        CHECK(!bad.valid && none.total == 0 && none.rolls.empty(), "convenience overload rejects a bad expression");
    }

    if (g_fail == 0) {
        std::printf("dice: OK — parse (valid/invalid), min/max/average, ranged & reproducible rolls, convenience.\n");
        return 0;
    }
    std::printf("dice: %d failure(s).\n", g_fail);
    return 1;
}
