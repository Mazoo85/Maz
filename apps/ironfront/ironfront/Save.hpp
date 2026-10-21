#pragma once

#include "ironfront/Game.hpp"

#include <optional>
#include <sstream>
#include <string>

// Iron Front — saving and loading a game in progress as plain text. The map itself always comes from
// the Europe 1939 scenario, so a save only records what changes: turn state, land and city owners,
// and the units. Version-tagged; anything malformed loads as nullopt rather than a broken game.
namespace ironfront {

constexpr int kSaveVersion = 1;

inline std::string serialize(const Game& g) {
    std::ostringstream o;
    o << "ironfront " << kSaveVersion << "\n";
    o << "state " << g.player << ' ' << g.current << ' ' << g.turn << ' ' << g.nextId << ' ' << g.seed << ' '
      << g.rolls << "\n";
    o << "alive";
    for (bool a : g.alive) o << ' ' << (a ? 1 : 0);
    o << "\nowners ";
    for (int v : g.world.map.ownerGrid()) o << static_cast<char>(v < 0 ? '-' : '0' + v);
    o << "\ncities";
    for (const City& c : g.world.map.cities()) o << ' ' << c.owner;
    o << "\nunits " << g.world.units.size() << "\n";
    for (const Unit& u : g.world.units)
        o << u.id << ' ' << static_cast<int>(u.type) << ' ' << u.owner << ' ' << u.hex.q << ' ' << u.hex.r << ' '
          << u.hp << ' ' << (u.moved ? 1 : 0) << ' ' << (u.attacked ? 1 : 0) << "\n";
    return o.str();
}

inline std::optional<Game> deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "ironfront" || version != kSaveVersion) return std::nullopt;

    Game g = Game::newGame(Germany, 1);
    auto validCountry = [](int c, bool allowNeutral) { return (allowNeutral && c == kNeutral) || (c >= 0 && c < CountryCount); };

    if (!(in >> tag) || tag != "state") return std::nullopt;
    if (!(in >> g.player >> g.current >> g.turn >> g.nextId >> g.seed >> g.rolls)) return std::nullopt;
    if (!validCountry(g.player, true) || !validCountry(g.current, false) || g.turn < 1) return std::nullopt;

    if (!(in >> tag) || tag != "alive") return std::nullopt;
    for (bool& a : g.alive) {
        int v = 0;
        if (!(in >> v)) return std::nullopt;
        a = v != 0;
    }

    std::string owners;
    if (!(in >> tag >> owners) || tag != "owners" || owners.size() != g.world.map.ownerGrid().size())
        return std::nullopt;
    for (std::size_t i = 0; i < owners.size(); ++i) {
        const char ch = owners[i];
        if (ch != '-' && (ch < '0' || ch >= '0' + CountryCount)) return std::nullopt;
        g.world.map.ownerGrid()[i] = ch == '-' ? kNeutral : ch - '0';
    }

    if (!(in >> tag) || tag != "cities") return std::nullopt;
    for (City& c : g.world.map.cities())
        if (!(in >> c.owner) || !validCountry(c.owner, true)) return std::nullopt;

    std::size_t count = 0;
    if (!(in >> tag >> count) || tag != "units" || count > 10000) return std::nullopt;
    g.world.units.clear();
    for (std::size_t i = 0; i < count; ++i) {
        Unit u;
        int type = 0, moved = 0, attacked = 0;
        if (!(in >> u.id >> type >> u.owner >> u.hex.q >> u.hex.r >> u.hp >> moved >> attacked)) return std::nullopt;
        if (type < 0 || type >= kUnitTypeCount || !validCountry(u.owner, false)) return std::nullopt;
        u.type = static_cast<UnitType>(type);
        if (!g.world.map.inBounds(u.hex) || u.hp <= 0 || u.hp > u.st().maxHp) return std::nullopt;
        u.moved = moved != 0;
        u.attacked = attacked != 0;
        g.world.units.push_back(u);
    }
    return g;
}

} // namespace ironfront
