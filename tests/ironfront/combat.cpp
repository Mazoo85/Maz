// tests/ironfront/combat.cpp — the damage formula, terrain, flanking, counters and targeting rules.
#include "check.hpp"
#include "fixtures.hpp"
#include "ironfront/Combat.hpp"

using namespace ironfront;

int main() {
    {   // Base case: infantry vs infantry on plains = round(4 x 10 / 14) = 3.
        World w = fx::plains();
        fx::add(w, UnitType::Infantry, Germany, 3, 4);
        fx::add(w, UnitType::Infantry, France, 4, 4);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 3);
    }
    {   // Flanking: tank base 5; +15% per extra friend beside the target, capped at 3 friends.
        World w = fx::plains();
        fx::add(w, UnitType::Tank, Germany, 3, 4);      // attacker, west of the target
        fx::add(w, UnitType::Infantry, France, 4, 4);   // target
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 5);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.1f, false) == 6); // a lucky roll: 5.5 -> 6
        fx::add(w, UnitType::Infantry, Germany, 5, 4);  // 1 friend: 5.75 -> 6
        CHECK(flankingFriends(w, w.units[0], w.units[1]) == 1);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 6);
        fx::add(w, UnitType::Infantry, Germany, 4, 3);
        fx::add(w, UnitType::Infantry, Germany, 4, 5);  // 3 friends: 7.25 -> 7
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 7);
        fx::add(w, UnitType::Infantry, Germany, 3, 3);
        fx::add(w, UnitType::Infantry, Germany, 3, 5);  // 5 friends: still capped at 3
        CHECK(flankingFriends(w, w.units[0], w.units[1]) == 3);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 7);
        // Counter-attacks never get the flanking bonus.
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, true) == 5);
    }
    {   // A weakened attacker hits less: a half-strength tank deals 2.5 -> 3 instead of 5.
        World w = fx::plains();
        Unit& t = fx::add(w, UnitType::Tank, Germany, 3, 4);
        t.hp = 6;
        fx::add(w, UnitType::Infantry, France, 4, 4);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 3);
        // Minimum damage is 1: a 1-HP infantry attacking still scratches.
        w.units[0].type = UnitType::Infantry;
        w.units[0].hp = 1;
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 1);
    }
    {   // Terrain multipliers.
        World w = fx::world({"....", ".fm.", "...."});
        w.map.cities().push_back(City{"Testburg", offsetToHex(0, 0), France, false});
        Unit& inf = fx::add(w, UnitType::Infantry, France, 0, 0);
        CHECK(terrainDefense(w, inf) == 1.75f); // infantry in a city
        inf.type = UnitType::Tank;
        CHECK(terrainDefense(w, w.units[0]) == 1.5f);
        Unit& forest = fx::add(w, UnitType::Infantry, France, 1, 1);
        CHECK(terrainDefense(w, forest) == 1.5f);
        forest.type = UnitType::Artillery;
        CHECK(terrainDefense(w, w.units[1]) == 1.25f);
        Unit& mtn = fx::add(w, UnitType::Infantry, France, 2, 1);
        CHECK(terrainDefense(w, mtn) == 1.5f);
        Unit& plane = fx::add(w, UnitType::Fighter, France, 2, 1);
        plane.hex = offsetToHex(2, 1); // aircraft ignore terrain
        CHECK(terrainDefense(w, plane) == 1.0f);
    }
    {   // Tanks fighting into mountains: 70 / (10 + 4 x 1.5) x 0.75 = 3.28 -> 3.
        World w = fx::world({"...", ".m.", "..."});
        fx::add(w, UnitType::Tank, Germany, 0, 1);
        fx::add(w, UnitType::Infantry, France, 1, 1);
        CHECK(computeDamage(w, w.units[0], w.units[1], 1.0f, false) == 3);
    }
    {   // Artillery from 2 hexes: no counter. From 1 hex: the defender hits back.
        World w = fx::plains();
        const int art = fx::add(w, UnitType::Artillery, Germany, 2, 4).id;
        const int inf = fx::add(w, UnitType::Infantry, France, 4, 4).id;
        CHECK(canTarget(w, *w.unitById(art), *w.unitById(inf)));
        CombatResult r = resolveAttack(w, art, inf, 1.0f, 1.0f);
        CHECK(r.damageDealt == 5 && r.damageTaken == 0);
        CHECK(w.unitById(inf)->hp == 5);
        w.unitById(art)->hex = offsetToHex(3, 4);
        r = resolveAttack(w, art, inf, 1.0f, 1.0f);
        // Adjacent now, but the second 5-damage strike destroys the 5-HP infantry, so no counter.
        CHECK(r.defenderDestroyed && r.damageTaken == 0);
        CHECK(w.unitById(inf) == nullptr); // removed from the world
        // A fresh adjacent target survives the strike and counters.
        const int inf2 = fx::add(w, UnitType::Infantry, France, 4, 4).id;
        w.unitById(inf2)->hp = 10;
        r = resolveAttack(w, art, inf2, 1.0f, 1.0f);
        CHECK(r.damageDealt == 5);
        // Counter from a 5/10 infantry: 4 x 0.5 x 10 / (10 + 2) = 1.67 -> 2.
        CHECK(r.damageTaken == 2);
        CHECK(w.unitById(art)->hp == 6);
    }
    {   // Targeting rules.
        World w = fx::world({"~~~~", "...."});
        const Unit ship2 = fx::add(w, UnitType::Destroyer, Britain, 1, 0); // copy: the vector grows
        fx::add(w, UnitType::Infantry, Germany, 1, 1);
        fx::add(w, UnitType::Artillery, Germany, 2, 1);
        fx::add(w, UnitType::Fighter, Germany, 0, 1);
        fx::add(w, UnitType::Infantry, Britain, 3, 1);
        const Unit& inf = w.units[1];
        const Unit& art = w.units[2];
        const Unit& fig = w.units[3];
        CHECK(computeDamage(w, w.units[0], inf, 1.1f, false) == 4); // 5x10/(10+4)x1.1 = 3.93 -> 4
        CHECK(!canTarget(w, inf, ship2)); // infantry can't hit a ship
        CHECK(canTarget(w, art, ship2));  // artillery can
        CHECK(canTarget(w, fig, ship2));  // and so can aircraft
        CHECK(canTarget(w, ship2, inf));  // a ship can shell the coast
        CHECK(!canTarget(w, inf, inf));   // no friendly fire
        CHECK(!canTarget(w, inf, w.units[4])); // out of range (distance 2)
        CHECK(canTarget(w, art, w.units[4]));  // artillery's range 2 covers it
    }
    {   // Transports are easy prey and harmless.
        World w = fx::world({"~~~~", "...."});
        const int boat = fx::add(w, UnitType::Tank, Britain, 1, 0).id;   // a tank at sea
        const int ship = fx::add(w, UnitType::Destroyer, Germany, 2, 0).id;
        const int inf = fx::add(w, UnitType::Infantry, Germany, 1, 1).id;
        const int art = fx::add(w, UnitType::Artillery, Germany, 2, 1).id;
        const Unit& b = *w.unitById(boat);
        CHECK(embarked(w, b));
        CHECK(!canTarget(w, b, *w.unitById(ship)));  // can't attack from a transport
        CHECK(!canTarget(w, b, *w.unitById(inf)));
        CHECK(!canTarget(w, *w.unitById(inf), b));   // infantry can't reach it on the water
        CHECK(canTarget(w, *w.unitById(art), b));
        CHECK(canTarget(w, *w.unitById(ship), b));
        // Destroyer vs transport: 5 x 10 / (10 + 1 x 1.0) = 4.55 -> 5, and no counter.
        CHECK(computeDamage(w, *w.unitById(ship), b, 1.0f, false) == 5);
        const CombatResult r = resolveAttack(w, ship, boat, 1.0f, 1.0f);
        CHECK(r.damageDealt == 5 && r.damageTaken == 0);
    }
    return finish("ironfront_combat");
}
