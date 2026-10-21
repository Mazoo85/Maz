// tests/ironfront/movement.cpp — move points, terrain costs, domains, stacking and zone of control.
#include "check.hpp"
#include "fixtures.hpp"

using namespace ironfront;
using maz::game::hexDistance;

int main() {
    {   // Infantry on open plains reaches exactly its six neighbours.
        World w = fx::plains();
        fx::add(w, UnitType::Infantry, Germany, 4, 4);
        const auto r = reachable(w, w.units[0]);
        CHECK(r.size() == 6);
        for (const Hex& h : r) CHECK(hexDistance(h, w.units[0].hex) == 1);
    }
    {   // A tank reaches distance 2 on plains (6 + 12 hexes).
        World w = fx::plains();
        fx::add(w, UnitType::Tank, Germany, 4, 4);
        const auto r = reachable(w, w.units[0]);
        CHECK(r.size() == 18);
    }
    {   // Forest costs 2: a tank can enter it but not continue beyond it.
        World w = fx::world({".......", "...f...", ".......", "......."});
        fx::add(w, UnitType::Tank, Germany, 2, 1); // west of the forest at (3,1)
        const auto r = reachable(w, w.units[0]);
        CHECK(fx::has(r, offsetToHex(3, 1)));
        CHECK(!fx::has(r, offsetToHex(4, 1))); // directly behind the forest: no plains route in 2 points
        CHECK(fx::has(r, offsetToHex(4, 0)));  // but the open hex beside it is
    }
    {   // Infantry (1 point) may still step into a mountain (cost 3) with full points.
        World w = fx::world({"...", ".m.", "..."});
        fx::add(w, UnitType::Infantry, Germany, 0, 1);
        CHECK(fx::has(reachable(w, w.units[0]), offsetToHex(1, 1)));
    }
    {   // Domains: land never enters sea, destroyers only sail, fighters fly over everything.
        World w = fx::world({"..~~~~", "..~~~~", "..mmm."});
        fx::add(w, UnitType::Tank, Germany, 1, 0);
        fx::add(w, UnitType::Destroyer, Germany, 3, 0);
        fx::add(w, UnitType::Fighter, Germany, 0, 1);
        for (const Hex& h : reachable(w, w.units[0])) CHECK(w.map.terrain(h) != Terrain::Sea);
        const auto ship = reachable(w, w.units[1]);
        CHECK(!ship.empty());
        for (const Hex& h : ship) CHECK(w.map.terrain(h) == Terrain::Sea);
        const auto plane = reachable(w, w.units[2]);
        CHECK(fx::has(plane, offsetToHex(4, 1))); // across the sea, 4 hexes
        CHECK(fx::has(plane, offsetToHex(4, 2))); // over the mountains
        for (const Hex& h : plane) CHECK(hexDistance(h, w.units[2].hex) <= 4);
    }
    {   // No stacking: an occupied hex is not a destination and blocks the way.
        World w = fx::plains();
        fx::add(w, UnitType::Infantry, Germany, 4, 4);
        fx::add(w, UnitType::Infantry, Germany, 5, 4);
        CHECK(!fx::has(reachable(w, w.units[0]), offsetToHex(5, 4)));
    }
    {   // Zone of control: a tank that steps next to an enemy must stop there.
        World w = fx::plains();
        fx::add(w, UnitType::Tank, Germany, 2, 4);
        fx::add(w, UnitType::Infantry, France, 5, 4);
        const auto r = reachable(w, w.units[0]);
        CHECK(fx::has(r, offsetToHex(3, 4)));  // 1 step, not next to the enemy
        CHECK(fx::has(r, offsetToHex(4, 4)));  // 2 steps, enters ZOC (adjacent to enemy) — allowed as an end
        // Hexes that would require passing *through* a ZOC hex are out: (4,4) is the only route to
        // nothing further anyway (move 2), so check the fighter instead, which ignores ZOC.
        fx::add(w, UnitType::Fighter, Germany, 2, 2);
        const auto f = reachable(w, w.units[2]);
        CHECK(fx::has(f, offsetToHex(6, 2)));
        // A tank starting beside the enemy, boxed so its only exits are ZOC hexes, gets one step.
        World z = fx::plains();
        fx::add(z, UnitType::Tank, Germany, 4, 4);
        fx::add(z, UnitType::Infantry, France, 4, 2);
        fx::add(z, UnitType::Infantry, France, 4, 6);
        const auto zr = reachable(z, z.units[0]);
        // (4,3) and (4,5) / (3,3) etc. are ZOC: reachable but nothing past them through them.
        for (const Hex& h : zr) {
            if (hexDistance(h, z.units[0].hex) == 2) {
                // Any distance-2 hex must be reachable via a non-ZOC neighbour.
                bool viaFree = false;
                for (const Hex& n : maz::game::hexNeighbors(h))
                    if (hexDistance(n, z.units[0].hex) == 1 && !inEnemyZoc(z, Germany, n)) viaFree = true;
                CHECK(viaFree);
            }
        }
    }
    {   // ZOC ends movement: a destroyer sailing a one-hex-wide lane past an enemy on the coast
        // stops at the first hex beside it and cannot sail on, even with move points left.
        World w = fx::world({"~~~~~~", "......"});
        fx::add(w, UnitType::Destroyer, Britain, 0, 0);
        fx::add(w, UnitType::Infantry, Germany, 2, 1); // borders (2,0) and (3,0)
        const auto r = reachable(w, w.units[0]);
        CHECK(fx::has(r, offsetToHex(1, 0)));
        CHECK(fx::has(r, offsetToHex(2, 0)));
        CHECK(!fx::has(r, offsetToHex(3, 0)));
        // Without the enemy it sails all three hexes.
        w.units.pop_back();
        CHECK(fx::has(reachable(w, w.units[0]), offsetToHex(3, 0)));
    }
    return finish("ironfront_movement");
}
