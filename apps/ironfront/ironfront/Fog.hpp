#pragma once

#include "ironfront/Movement.hpp"

#include "maz/game/FogOfWar.hpp"

// Iron Front — what a country can see. The map of Europe is common knowledge, so every hex starts
// Explored (terrain and borders show); only hexes within a unit's vision radius, or one hex around an
// owned city, are Visible, and enemy units are drawn only there. Recomputed from the world whenever
// it changes, so it never needs saving. FogOfWar is indexed by offset (col, row).
namespace ironfront {

inline void updateFog(const World& w, int country, maz::game::FogOfWar& fog) {
    if (fog.width() != w.map.width() || fog.height() != w.map.height()) {
        fog.resize(w.map.width(), w.map.height());
        fog.exploreAll();
    }
    fog.beginFrame();
    auto revealAround = [&](const Hex& centre, int radius) {
        for (const Hex& h : maz::game::hexRange(centre, radius)) {
            int c = 0, r = 0;
            hexToOffset(h, c, r);
            fog.reveal(c, r);
        }
    };
    for (const Unit& u : w.units)
        if (u.owner == country) revealAround(u.hex, u.st().vision);
    for (const City& c : w.map.cities())
        if (c.owner == country) revealAround(c.hex, 1);
}

inline bool hexVisible(const maz::game::FogOfWar& fog, const Hex& h) {
    int c = 0, r = 0;
    hexToOffset(h, c, r);
    return fog.isVisible(c, r);
}

inline bool unitVisible(const maz::game::FogOfWar& fog, const Unit& u) { return hexVisible(fog, u.hex); }

} // namespace ironfront
