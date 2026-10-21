#pragma once

#include "ironfront/Movement.hpp"

#include <algorithm>
#include <cmath>

// Iron Front — who can attack whom, and how hard. The damage formula is the spec's:
//
//   flank  = 1 + 0.15 x min(3, attacker's friends adjacent to the defender)     (1 for counters)
//   damage = max(1, round(attack x hp/maxHp x flank x roll x 10 / (10 + defense x T)))
//
// with T the defender's terrain multiplier and roll in [0.9, 1.1]. Tanks fighting from or into
// mountains deal x0.75. A surviving defender that can reach the attacker hits back once.
namespace ironfront {

inline bool canTarget(const World& w, const Unit& a, const Unit& d) {
    if (a.owner == d.owner) return false;
    const int dist = maz::game::hexDistance(a.hex, d.hex);
    if (dist < 1 || dist > a.st().range) return false;
    // Ships at sea can only be hit by artillery, other ships and aircraft.
    if (d.st().domain == Domain::Sea && a.st().domain == Domain::Land && a.type != UnitType::Artillery)
        return false;
    (void)w;
    return true;
}

inline float terrainDefense(const World& w, const Unit& d) {
    if (d.st().domain == Domain::Air) return 1.0f;
    const bool infantry = d.type == UnitType::Infantry;
    if (w.map.cityAt(d.hex)) return infantry ? 1.75f : 1.5f;
    switch (w.map.terrain(d.hex)) {
        case Terrain::Forest: return infantry ? 1.5f : 1.25f;
        case Terrain::Mountain: return 1.5f;
        case Terrain::Plains:
        case Terrain::Sea: break;
    }
    return 1.0f;
}

inline int flankingFriends(const World& w, const Unit& a, const Unit& d) {
    int n = 0;
    for (const Hex& h : maz::game::hexNeighbors(d.hex)) {
        const Unit* f = w.unitAt(h);
        if (f && f->id != a.id && f->owner == a.owner) ++n;
    }
    return std::min(3, n);
}

inline int computeDamage(const World& w, const Unit& a, const Unit& d, float roll, bool counter) {
    const float flank = counter ? 1.0f : 1.0f + 0.15f * static_cast<float>(flankingFriends(w, a, d));
    const float health = static_cast<float>(a.hp) / static_cast<float>(a.st().maxHp);
    float dmg = static_cast<float>(a.st().attack) * health * flank * roll * 10.0f /
                (10.0f + static_cast<float>(d.st().defense) * terrainDefense(w, d));
    if (a.type == UnitType::Tank &&
        (w.map.terrain(a.hex) == Terrain::Mountain || w.map.terrain(d.hex) == Terrain::Mountain))
        dmg *= 0.75f;
    return std::max(1, static_cast<int>(std::lround(dmg)));
}

struct CombatResult {
    int damageDealt = 0;
    int damageTaken = 0;
    bool defenderDestroyed = false;
    bool attackerDestroyed = false;
};

// Resolves `attackerId` striking `defenderId` (caller has checked canTarget). `roll` and
// `counterRoll` are the random swings in [0.9, 1.1]. Destroyed units are removed from the world.
inline CombatResult resolveAttack(World& w, int attackerId, int defenderId, float roll, float counterRoll) {
    CombatResult res;
    Unit* a = w.unitById(attackerId);
    Unit* d = w.unitById(defenderId);
    if (!a || !d) return res;
    res.damageDealt = std::min(d->hp, computeDamage(w, *a, *d, roll, false));
    d->hp -= res.damageDealt;
    if (d->hp <= 0) {
        res.defenderDestroyed = true;
    } else if (canTarget(w, *d, *a)) {
        res.damageTaken = std::min(a->hp, computeDamage(w, *d, *a, counterRoll, true));
        a->hp -= res.damageTaken;
        res.attackerDestroyed = a->hp <= 0;
    }
    w.units.erase(std::remove_if(w.units.begin(), w.units.end(), [](const Unit& u) { return u.hp <= 0; }),
                  w.units.end());
    return res;
}

} // namespace ironfront
