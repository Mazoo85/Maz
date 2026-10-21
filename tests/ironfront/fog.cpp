// tests/ironfront/fog.cpp — vision radii, city sight, and remembered-but-not-live hexes.
#include "check.hpp"
#include "fixtures.hpp"
#include "ironfront/Fog.hpp"

using namespace ironfront;
using maz::game::FogOfWar;
using maz::game::Visibility;

static Visibility vis(const FogOfWar& f, int c, int r) { return f.visibility(c, r); }

int main() {
    World w = fx::plains(12);
    fx::add(w, UnitType::Infantry, Germany, 2, 4);  // vision 2
    fx::add(w, UnitType::Tank, France, 4, 4);       // 2 hexes away: seen
    fx::add(w, UnitType::Tank, France, 5, 4);       // 3 hexes away: hidden
    w.map.cities().push_back(City{"Watch", offsetToHex(10, 10), Germany, false});

    FogOfWar fog;
    updateFog(w, Germany, fog);
    CHECK(fog.width() == 12 && fog.height() == 12);
    CHECK(vis(fog, 2, 4) == Visibility::Visible);
    CHECK(unitVisible(fog, w.units[1]));
    CHECK(!unitVisible(fog, w.units[2]));
    CHECK(vis(fog, 5, 4) == Visibility::Explored);       // the map itself is known everywhere
    CHECK(vis(fog, 10, 10) == Visibility::Visible);      // an owned city sees itself...
    CHECK(vis(fog, 11, 10) == Visibility::Visible);      // ...and one hex around
    CHECK(vis(fog, 10, 8) == Visibility::Explored);      // but not two

    // The fighter's longer sight (4) spots the far tank; France's view is its own.
    fx::add(w, UnitType::Fighter, Germany, 1, 4);
    updateFog(w, Germany, fog);
    CHECK(unitVisible(fog, w.units[2]));
    FogOfWar fr;
    updateFog(w, France, fr);
    CHECK(unitVisible(fr, w.units[0]));
    CHECK(vis(fr, 10, 10) == Visibility::Explored);

    // Units leave: their old surroundings drop back to remembered.
    w.units[0].hex = offsetToHex(8, 8);
    w.units[3].hex = offsetToHex(8, 9);
    updateFog(w, Germany, fog);
    CHECK(vis(fog, 2, 4) == Visibility::Explored);
    CHECK(!unitVisible(fog, w.units[1]));
    CHECK(vis(fog, 8, 8) == Visibility::Visible);
    return finish("ironfront_fog");
}
