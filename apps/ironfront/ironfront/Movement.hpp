#pragma once

#include "ironfront/Map.hpp"
#include "ironfront/Units.hpp"

#include <algorithm>
#include <vector>

// Iron Front — units on the map and where they can move. Movement spends move points by terrain
// (plains/city 1, forest 2, mountain 3), never stacks two units in a hex, and obeys zone of control:
// a land or sea unit that steps next to an enemy ground/naval unit must stop there. Fighters fly:
// every hex costs 1 and zone of control does not apply.
namespace ironfront {

struct Unit {
    int id = 0;
    UnitType type = UnitType::Infantry;
    int owner = kNeutral;
    Hex hex;
    int hp = 0;
    bool moved = false;
    bool attacked = false;

    const UnitStats& st() const { return stats(type); }
};

struct World {
    Map map;
    std::vector<Unit> units;

    Unit* unitAt(const Hex& h) {
        for (Unit& u : units)
            if (u.hex == h) return &u;
        return nullptr;
    }
    const Unit* unitAt(const Hex& h) const {
        for (const Unit& u : units)
            if (u.hex == h) return &u;
        return nullptr;
    }
    Unit* unitById(int id) {
        for (Unit& u : units)
            if (u.id == id) return &u;
        return nullptr;
    }
    const Unit* unitById(int id) const {
        for (const Unit& u : units)
            if (u.id == id) return &u;
        return nullptr;
    }
};

// Cost for `u` to enter `to`, ignoring other units; -1 when the unit can never go there.
inline int moveCost(const World& w, const Unit& u, const Hex& to) {
    if (!w.map.inBounds(to)) return -1;
    const Terrain t = w.map.terrain(to);
    switch (u.st().domain) {
        case Domain::Air: return 1;
        case Domain::Sea: return t == Terrain::Sea ? 1 : -1;
        case Domain::Land: break;
    }
    switch (t) {
        case Terrain::Plains: return 1;
        case Terrain::Forest: return 2;
        case Terrain::Mountain: return 3;
        case Terrain::Sea: return -1;
    }
    return -1;
}

// True when `h` is next to an enemy (of `owner`) unit that exerts zone of control (any non-air unit).
inline bool inEnemyZoc(const World& w, int owner, const Hex& h) {
    for (const Hex& n : maz::game::hexNeighbors(h)) {
        const Unit* e = w.unitAt(n);
        if (e && e->owner != owner && e->st().domain != Domain::Air) return true;
    }
    return false;
}

// Every hex `u` can end its move on this turn (excluding its own hex), by Dijkstra over move points.
inline std::vector<Hex> reachable(const World& w, const Unit& u) {
    struct Node {
        Hex hex;
        int left; // move points remaining on arrival
        bool stop; // movement must end here (zone of control)
    };
    const int full = u.st().move;
    const bool flies = u.st().domain == Domain::Air;
    std::vector<Node> best{{u.hex, full, false}}; // best[i] = best arrival found so far
    std::vector<Node> open{best[0]};
    auto find = [&](const Hex& h) -> Node* {
        for (Node& n : best)
            if (n.hex == h) return &n;
        return nullptr;
    };

    while (!open.empty()) {
        // Expand the open node with the most move points left (Dijkstra on remaining budget).
        auto it = std::max_element(open.begin(), open.end(),
                                   [](const Node& a, const Node& b) { return a.left < b.left; });
        const Node cur = *it;
        open.erase(it);
        if (cur.stop || cur.left <= 0) continue;
        for (const Hex& n : maz::game::hexNeighbors(cur.hex)) {
            const int cost = moveCost(w, u, n);
            if (cost < 0 || w.unitAt(n)) continue;
            int left = cur.left - cost;
            if (left < 0) {
                // A unit with full move points may always take one step, ending its move.
                if (!(cur.hex == u.hex && cur.left == full)) continue;
                left = 0;
            }
            const bool stop = !flies && inEnemyZoc(w, u.owner, n);
            if (stop) left = 0;
            Node* seen = find(n);
            if (seen && seen->left >= left) continue;
            if (seen) {
                seen->left = left;
                seen->stop = stop;
            } else {
                best.push_back({n, left, stop});
            }
            open.push_back({n, left, stop});
        }
    }

    std::vector<Hex> out;
    for (const Node& n : best)
        if (n.hex != u.hex) out.push_back(n.hex);
    return out;
}

inline bool canReach(const World& w, const Unit& u, const Hex& to) {
    for (const Hex& h : reachable(w, u))
        if (h == to) return true;
    return false;
}

} // namespace ironfront
