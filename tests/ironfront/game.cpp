// tests/ironfront/game.cpp — turn flow, action rules, capture, elimination, winning and losing.
#include "check.hpp"
#include "ironfront/Game.hpp"

using namespace ironfront;

// Two countries on a strip: Germany west, France east, a neutral town in the middle.
//   cols: 0 G-capital  1 G  2 G  3 neutral town  4 F  5 F  6 F-capital
static Scenario duel() {
    Scenario s;
    s.map = Map::fromAscii({".......", "......."}, {"GGG-FFF", "GGG-FFF"});
    s.map.cities().push_back(City{"Westheim", offsetToHex(0, 0), Germany, true});
    s.map.cities().push_back(City{"Mittel", offsetToHex(3, 0), kNeutral, false});
    s.map.cities().push_back(City{"Ostville", offsetToHex(5, 1), France, false});
    s.map.cities().push_back(City{"Estcap", offsetToHex(6, 0), France, true});
    s.units.push_back({UnitType::Infantry, Germany, 2, 0});
    s.units.push_back({UnitType::Tank, Germany, 1, 1});
    s.units.push_back({UnitType::Infantry, France, 5, 0});
    return s;
}

static Game duelGame() {
    Game g = Game::fromScenario(duel(), Germany, 7);
    // Only two countries exist on this map.
    g.alive = {true, false, true, false, false};
    return g;
}

int main() {
    {   // Only the current country acts; move-then-attack is fine, attack-then-move is not.
        Game g = duelGame();
        CHECK(g.current == Germany && g.turn == 1);
        const int ger = 1, tank = 2, fra = 3;
        CHECK(!g.move(fra, offsetToHex(4, 0)));  // not France's turn
        CHECK(g.moveTargets(fra).empty());
        CHECK(g.move(ger, offsetToHex(3, 0)));    // captures the neutral town
        CHECK(g.world.map.cityAt(offsetToHex(3, 0))->owner == Germany);
        CHECK(g.world.map.owner(offsetToHex(3, 0)) == Germany);
        CHECK(!g.move(ger, offsetToHex(2, 0)));   // already moved
        CHECK(g.attackTargets(ger).empty());      // France's infantry is 2 hexes away
        CHECK(g.move(tank, offsetToHex(3, 1)));   // the tank advances alongside
        g.endTurn();
        CHECK(g.current == France && g.turn == 1);
        // France attacks the town's garrison, which then cannot be followed by a move.
        CHECK(g.move(fra, offsetToHex(4, 0)));
        CHECK(!g.attackTargets(fra).empty());
        CombatResult r;
        CHECK(g.attack(fra, offsetToHex(3, 0), &r));
        CHECK(r.damageDealt > 0);
        CHECK(!g.attack(fra, offsetToHex(3, 0)));  // one attack per turn
        g.endTurn();
        CHECK(g.current == Germany && g.turn == 2); // eliminated/absent countries are skipped
        // Flags reset at the start of a new turn: the German infantry may act again.
        CHECK(!g.moveTargets(ger).empty() || !g.attackTargets(ger).empty());
        // Attack first, then moving is refused.
        if (!g.attackTargets(ger).empty()) {
            CHECK(g.attack(ger, g.attackTargets(ger)[0]));
            CHECK(!g.move(ger, offsetToHex(2, 0)));
        }
    }
    {   // Capturing an enemy capital eliminates the country and transfers everything; last one wins.
        Game g = duelGame();
        g.world.units.erase(g.world.units.begin() + 2); // remove the French defender
        Unit* tank = g.world.unitById(2);
        tank->hex = offsetToHex(5, 1);                  // standing in Ostville, next to the capital
        g.world.map.cityAt(offsetToHex(5, 1))->owner = Germany;
        CHECK(g.move(2, offsetToHex(6, 0)));
        CHECK(!g.alive[France]);
        CHECK(g.world.map.cityAt(offsetToHex(6, 0))->owner == Germany);
        CHECK(g.world.map.owner(offsetToHex(4, 0)) == Germany);      // French land passes over
        CHECK(g.cityCount(France) == 0);
        CHECK(g.winner() == Germany && g.playerWon() && g.isOver());
        CHECK(!g.move(1, offsetToHex(1, 0)));                         // nothing moves after the end
    }
    {   // Losing: the player's capital falls.
        Game g = Game::fromScenario(duel(), France, 7);
        g.alive = {true, false, true, false, false};
        g.world.units.erase(g.world.units.begin());            // remove the German infantry
        g.world.unitById(3)->hex = offsetToHex(1, 0);          // French infantry beside Westheim
        // Germany's turn first; pass it.
        g.endTurn();
        CHECK(g.current == France);
        CHECK(g.move(3, offsetToHex(0, 0)));
        CHECK(!g.alive[Germany]);
        CHECK(g.world.unitById(2) == nullptr);                 // the German tank disbanded
        CHECK(g.playerWon());
        // And from Germany's side the same game is a loss.
        Game h = g;
        h.player = Germany;
        CHECK(h.playerLost() && h.isOver() && !h.playerWon());
    }
    {   // The full scenario starts sane.
        Game g = Game::newGame(Britain, 42);
        CHECK(g.player == Britain && g.current == Germany && g.aliveCount() == CountryCount);
        CHECK(!g.isOver());
        for (int c = 0; c < CountryCount; ++c) CHECK(g.capitalOf(c) != nullptr);
        // Rolls are deterministic in (seed, index) and stay in range.
        Game a = Game::newGame(Britain, 42), b = Game::newGame(Britain, 42);
        for (int i = 0; i < 100; ++i) {
            const float x = a.nextRoll();
            CHECK(x == b.nextRoll());
            CHECK(x >= 0.9f && x <= 1.1f);
        }
        // A full round passes through every country and back to Germany at turn 2.
        for (int i = 0; i < CountryCount; ++i) g.endTurn();
        CHECK(g.current == Germany && g.turn == 2);
    }
    return finish("ironfront_game");
}
