#pragma once

#include "ironfront/Combat.hpp"
#include "ironfront/MapEurope.hpp"
#include "ironfront/Movement.hpp"

#include "maz/core/Pcg32.hpp"

#include <array>
#include <cstdint>
#include <vector>

// Iron Front — one game in progress: the world, whose turn it is, and the rules that change it.
// Countries play in a fixed order (Germany, UK, France, USSR, Italy), skipping eliminated ones. Each
// unit may move once and attack once per turn; attacking ends its movement. Moving a land unit onto
// a hex claims it, onto a city captures it, and onto an enemy capital eliminates that country: its
// units disband and its cities and land pass to the captor. Last country standing wins.
//
// The state is plain public data so Save.hpp can write and restore it exactly.
namespace ironfront {

struct Game {
    World world;
    int player = Germany;
    int current = Germany;
    int turn = 1;
    std::array<bool, CountryCount> alive{true, true, true, true, true};
    int nextId = 1;
    uint64_t seed = 1;
    uint64_t rolls = 0; // combat rolls drawn so far; each roll is derived from (seed, index)

    static Game fromScenario(const Scenario& s, int playerCountry, uint64_t rngSeed) {
        Game g;
        g.world.map = s.map;
        g.player = playerCountry;
        g.seed = rngSeed;
        for (const UnitSpawn& sp : s.units) {
            Unit u;
            u.id = g.nextId++;
            u.type = sp.type;
            u.owner = sp.owner;
            u.hex = offsetToHex(sp.col, sp.row);
            u.hp = stats(sp.type).maxHp;
            g.world.units.push_back(u);
        }
        return g;
    }
    static Game newGame(int playerCountry, uint64_t rngSeed) { return fromScenario(europe1939(), playerCountry, rngSeed); }

    // --- queries ---------------------------------------------------------------------------
    int aliveCount() const {
        int n = 0;
        for (bool a : alive) n += a ? 1 : 0;
        return n;
    }
    int winner() const {
        if (aliveCount() != 1) return -1;
        for (int c = 0; c < CountryCount; ++c)
            if (alive[static_cast<std::size_t>(c)]) return c;
        return -1;
    }
    // player == kNeutral is a spectator game (all AI), which only ends with one power left.
    bool playerLost() const { return player >= 0 && !alive[static_cast<std::size_t>(player)]; }
    bool playerWon() const { return winner() == player; }
    bool isOver() const { return playerLost() || winner() >= 0; }

    const City* capitalOf(int country) const {
        for (const City& c : world.map.cities())
            if (c.capital && c.owner == country) return &c;
        return nullptr;
    }
    int cityCount(int country) const {
        int n = 0;
        for (const City& c : world.map.cities()) n += c.owner == country ? 1 : 0;
        return n;
    }

    bool canAct(const Unit& u) const { return u.owner == current && !isOver(); }

    std::vector<Hex> moveTargets(int unitId) const {
        const Unit* u = world.unitById(unitId);
        if (!u || !canAct(*u) || u->moved || u->attacked) return {};
        return reachable(world, *u);
    }
    std::vector<Hex> attackTargets(int unitId) const {
        const Unit* u = world.unitById(unitId);
        if (!u || !canAct(*u) || u->attacked) return {};
        std::vector<Hex> out;
        for (const Unit& e : world.units)
            if (canTarget(world, *u, e)) out.push_back(e.hex);
        return out;
    }

    // --- actions ---------------------------------------------------------------------------
    bool move(int unitId, const Hex& to) {
        Unit* u = world.unitById(unitId);
        if (!u || !canAct(*u) || u->moved || u->attacked || !canReach(world, *u, to)) return false;
        u->hex = to;
        u->moved = true;
        if (u->st().domain == Domain::Land && !atSea(world, *u)) claim(u->owner, to);
        return true;
    }

    bool attack(int unitId, const Hex& target, CombatResult* out = nullptr) {
        Unit* u = world.unitById(unitId);
        Unit* d = world.unitAt(target);
        if (!u || !d || !canAct(*u) || u->attacked || !canTarget(world, *u, *d)) return false;
        u->attacked = true;
        u->moved = true; // attacking ends the unit's movement
        const float r1 = nextRoll();
        const float r2 = nextRoll();
        const CombatResult res = resolveAttack(world, unitId, d->id, r1, r2);
        if (out) *out = res;
        return true;
    }

    void endTurn() {
        if (isOver()) return;
        for (Unit& u : world.units)
            if (u.owner == current) u.moved = u.attacked = false;
        for (int step = 1; step <= CountryCount; ++step) {
            const int next = (current + step) % CountryCount;
            if (!alive[static_cast<std::size_t>(next)]) continue;
            if (next <= current) ++turn; // wrapped past the last country: a new round
            current = next;
            return;
        }
    }

    float nextRoll() {
        maz::core::Pcg32 r(seed, rolls++);
        return r.rangef(0.9f, 1.1f);
    }

private:
    void claim(int country, const Hex& h) {
        world.map.setOwner(h, country);
        City* c = world.map.cityAt(h);
        if (!c || c->owner == country) return;
        const int loser = c->owner;
        c->owner = country;
        if (c->capital && loser != kNeutral) eliminate(loser, country);
    }

    void eliminate(int loser, int captor) {
        alive[static_cast<std::size_t>(loser)] = false;
        world.units.erase(std::remove_if(world.units.begin(), world.units.end(),
                                         [&](const Unit& u) { return u.owner == loser; }),
                          world.units.end());
        for (City& c : world.map.cities())
            if (c.owner == loser) c.owner = captor;
        for (int& o : world.map.ownerGrid())
            if (o == loser) o = captor;
    }
};

} // namespace ironfront
