// tests/ironfront/map.cpp — hex addressing, ASCII parsing, and the Europe 1939 scenario's sanity.
#include "check.hpp"
#include "ironfront/MapEurope.hpp"

#include <set>
#include <utility>

using namespace ironfront;

int main() {
    // offset <-> axial round trip, and odd-r neighbours line up.
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c) {
            int c2 = -1, r2 = -1;
            hexToOffset(offsetToHex(c, r), c2, r2);
            CHECK(c2 == c && r2 == r);
        }
    // (1,1) is an odd row: its lower-right neighbour is (2,2), lower-left is (1,2).
    CHECK(maz::game::hexDistance(offsetToHex(1, 1), offsetToHex(2, 2)) == 1);
    CHECK(maz::game::hexDistance(offsetToHex(1, 1), offsetToHex(1, 2)) == 1);
    CHECK(maz::game::hexDistance(offsetToHex(1, 1), offsetToHex(0, 2)) == 2);

    // ASCII parsing.
    const Map m = Map::fromAscii({".fm~", "...."}, {"GUF ", "SI--"});
    CHECK(m.width() == 4 && m.height() == 2);
    CHECK(m.terrain(offsetToHex(0, 0)) == Terrain::Plains);
    CHECK(m.terrain(offsetToHex(1, 0)) == Terrain::Forest);
    CHECK(m.terrain(offsetToHex(2, 0)) == Terrain::Mountain);
    CHECK(m.terrain(offsetToHex(3, 0)) == Terrain::Sea);
    CHECK(m.owner(offsetToHex(0, 0)) == Germany && m.owner(offsetToHex(1, 0)) == Britain);
    CHECK(m.owner(offsetToHex(0, 1)) == Soviet && m.owner(offsetToHex(1, 1)) == Italy);
    CHECK(m.owner(offsetToHex(2, 1)) == kNeutral && m.owner(offsetToHex(3, 0)) == kNeutral);
    CHECK(!m.inBounds(offsetToHex(4, 0)) && !m.inBounds(offsetToHex(0, 2)));
    CHECK(m.terrain(offsetToHex(9, 9)) == Terrain::Sea); // out of bounds reads as sea

    // The scenario.
    const Scenario s = europe1939();
    CHECK(s.map.width() == 40 && s.map.height() == 40);
    std::set<std::pair<int, int>> cityHexes;
    for (const City& c : s.map.cities()) {
        CHECK(s.map.inBounds(c.hex));
        CHECK(s.map.terrain(c.hex) != Terrain::Sea);
        CHECK(s.map.owner(c.hex) == c.owner);
        CHECK(cityHexes.insert({c.hex.q, c.hex.r}).second); // no two cities share a hex
    }
    for (int country = 0; country < CountryCount; ++country) {
        int capitals = 0, others = 0, units = 0;
        for (const City& c : s.map.cities())
            if (c.owner == country) (c.capital ? capitals : others)++;
        for (const UnitSpawn& u : s.units)
            if (u.owner == country) ++units;
        CHECK(capitals == 1);
        CHECK(others >= 4 && others <= 8);
        CHECK(units >= 6 && units <= 10);
    }
    std::set<std::pair<int, int>> unitHexes;
    for (const UnitSpawn& u : s.units) {
        const Hex h = offsetToHex(u.col, u.row);
        CHECK(s.map.inBounds(h));
        CHECK(unitHexes.insert({u.col, u.row}).second); // one unit per hex
        if (stats(u.type).domain == Domain::Sea) {
            CHECK(s.map.terrain(h) == Terrain::Sea);
        } else {
            CHECK(s.map.terrain(h) != Terrain::Sea);
            CHECK(s.map.owner(h) == u.owner);
        }
    }
    return finish("ironfront_map");
}
