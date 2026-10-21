#pragma once

#include "ironfront/Game.hpp"

#include "maz/game/UtilityAI.hpp"

#include <climits>
#include <queue>
#include <utility>
#include <optional>
#include <vector>

// Iron Front — the computer opponent. One step at a time (so the app can animate each move), it
// looks at every unit of the country whose turn it is, scores that unit's options with
// maz::game::UtilityAction and takes the single best action:
//   capture  — step onto a city it doesn't own (capitals first)
//   attack   — hit a target, favouring damage dealt, kills and low counter-damage
//   advance  — close distance to the nearest goal (enemy/neutral cities for armies, enemy units for
//              ships and planes), preferring hexes it can strike from next
//   retreat  — a battered unit under threat falls back toward its own cities
//   hold     — do nothing; a unit whose best option is to hold is finished for the turn
// Every action marks the unit moved or attacked, so a turn always ends (at most two steps per unit).
namespace ironfront {

struct AiAction {
    enum Kind { Move, Attack } kind = Move;
    int unitId = 0;
    Hex target;
};

namespace ai_detail {

using maz::game::Consideration;
using maz::game::UtilityAction;

constexpr int kFar = INT_MAX / 4;

// Travel cost from the nearest goal hex (Dijkstra) over hexes `u` could ever enter: one per hex,
// except that land units pay kSeaStep per sea hex so they ship out only when water is the way.
constexpr int kSeaStep = 3;

struct DistanceField {
    const Map* map = nullptr;
    std::vector<int> d;
    int at(const Hex& h) const {
        if (!map->inBounds(h)) return kFar;
        int c = 0, r = 0;
        hexToOffset(h, c, r);
        return d[static_cast<std::size_t>(r * map->width() + c)];
    }
};

inline DistanceField distanceField(const World& w, const Unit& u, const std::vector<Hex>& goals) {
    DistanceField f;
    f.map = &w.map;
    f.d.assign(static_cast<std::size_t>(w.map.width() * w.map.height()), kFar);
    using Item = std::pair<int, Hex>;
    auto cmp = [](const Item& x, const Item& y) { return x.first > y.first; };
    std::priority_queue<Item, std::vector<Item>, decltype(cmp)> q(cmp);
    auto set = [&](const Hex& h, int v) {
        int c = 0, r = 0;
        hexToOffset(h, c, r);
        f.d[static_cast<std::size_t>(r * w.map.width() + c)] = v;
    };
    const bool land = u.st().domain == Domain::Land;
    for (const Hex& g : goals)
        if (w.map.inBounds(g) && f.at(g) != 0) {
            set(g, 0);
            q.push({0, g});
        }
    while (!q.empty()) {
        const auto [dist, h] = q.top();
        q.pop();
        if (dist > f.at(h)) continue;
        for (const Hex& n : maz::game::hexNeighbors(h)) {
            if (!w.map.inBounds(n)) continue;
            int step = 1;
            if (land) {
                step = w.map.terrain(n) == Terrain::Sea ? kSeaStep : 1;
            } else if (moveCost(w, u, n) < 0) {
                continue;
            }
            if (dist + step >= f.at(n)) continue;
            set(n, dist + step);
            q.push({dist + step, n});
        }
    }
    return f;
}

inline std::vector<Hex> goalsFor(const Game& g, const Unit& u) {
    std::vector<Hex> goals;
    if (u.st().domain == Domain::Land) {
        for (const City& c : g.world.map.cities())
            if (c.owner != u.owner) goals.push_back(c.hex);
    } else {
        for (const Unit& e : g.world.units)
            if (e.owner != u.owner) goals.push_back(e.hex);
    }
    return goals;
}

inline Consideration linear(float input) {
    Consideration c;
    c.input = input;
    return c;
}

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// Expected outcome of `a` attacking `d` at a neutral roll.
inline UtilityAction scoreAttack(const World& w, const Unit& a, const Unit& d) {
    const int dealt = std::min(d.hp, computeDamage(w, a, d, 1.0f, false));
    int taken = 0;
    if (dealt < d.hp && canTarget(w, d, a)) {
        Unit hurt = d;
        hurt.hp -= dealt;
        taken = computeDamage(w, hurt, a, 1.0f, true);
    }
    UtilityAction act;
    act.weight = dealt >= d.hp ? 1.4f : 1.0f;
    if (w.map.cityAt(d.hex)) act.weight += 0.2f;
    act.considerations.push_back(linear(0.25f + 0.75f * static_cast<float>(dealt) / static_cast<float>(d.hp)));
    act.considerations.push_back(linear(1.0f - 0.8f * clamp01(static_cast<float>(taken) / static_cast<float>(a.hp))));
    return act;
}

inline bool threatened(const World& w, const Unit& u) {
    for (const Unit& e : w.units)
        if (e.owner != u.owner && maz::game::hexDistance(e.hex, u.hex) <= 2) return true;
    return false;
}

struct Candidate {
    AiAction action;
    UtilityAction utility;
};

inline void unitCandidates(const Game& g, const Unit& u, std::vector<Candidate>& out) {
    const World& w = g.world;
    if (!u.attacked)
        for (const Unit& e : w.units)
            if (canTarget(w, u, e)) out.push_back({{AiAction::Attack, u.id, e.hex}, scoreAttack(w, u, e)});
    if (u.moved || u.attacked) return;

    const std::vector<Hex> moves = reachable(w, u);
    if (moves.empty()) return;
    const bool retreating = u.hp * 100 < u.st().maxHp * 35 && threatened(w, u);
    std::vector<Hex> goals;
    if (retreating) {
        for (const City& c : w.map.cities())
            if (c.owner == u.owner) goals.push_back(c.hex);
    } else {
        goals = goalsFor(g, u);
    }
    const DistanceField field = distanceField(w, u, goals);
    const DistanceField& toHome = field;
    const DistanceField& toGoal = field;
    const float span = static_cast<float>(u.st().move);

    for (const Hex& h : moves) {
        UtilityAction act;
        const City* city = w.map.cityAt(h);
        if (u.st().domain == Domain::Land && city && city->owner != u.owner) {
            act.weight = city->capital && city->owner != kNeutral ? 3.0f : 2.0f;
        } else if (retreating) {
            const float gain = static_cast<float>(toHome.at(u.hex) - toHome.at(h)) / span;
            if (gain <= 0.0f) continue;
            act.weight = 0.9f;
            act.considerations.push_back(linear(clamp01(gain)));
        } else {
            const int from = toGoal.at(u.hex), to = toGoal.at(h);
            if (from >= kFar || to >= kFar) continue;
            const float gain = static_cast<float>(from - to) / span;
            // Could this unit strike something from there next?
            Unit there = u;
            there.hex = h;
            bool strike = false;
            for (const Unit& e : w.units)
                if (canTarget(w, there, e)) strike = true;
            if (gain <= 0.0f && !strike) continue;
            act.weight = 0.5f + (strike ? 0.3f : 0.0f);
            act.considerations.push_back(linear(0.3f + 0.7f * clamp01(gain)));
            // Prefer defensible ground slightly.
            act.considerations.push_back(linear(0.7f + 0.3f * clamp01((terrainDefense(w, there) - 1.0f) / 0.75f)));
        }
        out.push_back({{AiAction::Move, u.id, h}, act});
    }
}

} // namespace ai_detail

// The single best action for one unit, or nullopt when it has nothing worth doing.
inline std::optional<AiAction> bestActionFor(const Game& g, const Unit& u) {
    std::vector<ai_detail::Candidate> cands;
    ai_detail::unitCandidates(g, u, cands);
    if (cands.empty()) return std::nullopt;
    std::vector<maz::game::UtilityAction> utils;
    utils.reserve(cands.size());
    for (const auto& c : cands) utils.push_back(c.utility);
    float best = 0.0f;
    const int i = maz::game::selectBestAction(utils, &best);
    if (i < 0 || best < 0.05f) return std::nullopt;
    return cands[static_cast<std::size_t>(i)].action;
}

inline bool applyAiAction(Game& g, const AiAction& a) {
    return a.kind == AiAction::Attack ? g.attack(a.unitId, a.target) : g.move(a.unitId, a.target);
}

// One country's turn, played a unit at a time in id order: each unit moves and/or attacks until it
// has nothing worth doing, then the next unit goes. Remembers finished units so each step only
// evaluates the unit in hand. Start a fresh AiTurn for every country turn.
class AiTurn {
public:
    // The next action, or nullopt when the whole country is done.
    std::optional<AiAction> next(const Game& g) {
        if (g.isOver()) return std::nullopt;
        for (const Unit& u : g.world.units) {
            if (u.owner != g.current || isDone(u.id)) continue;
            if (auto a = bestActionFor(g, u)) return a;
            m_done.push_back(u.id);
        }
        return std::nullopt;
    }
    // Applies one step; false when the turn is over. A refused action retires that unit.
    bool step(Game& g) {
        const auto a = next(g);
        if (!a) return false;
        if (!applyAiAction(g, *a)) retire(a->unitId);
        return true;
    }
    // Takes a unit out of this turn (e.g. after the caller applied a step that was refused).
    void retire(int unitId) { m_done.push_back(unitId); }

private:
    bool isDone(int id) const {
        for (int d : m_done)
            if (d == id) return true;
        return false;
    }
    std::vector<int> m_done;
};

// Plays the current country's whole turn and passes play on.
inline void playAiTurn(Game& g) {
    AiTurn t;
    while (t.step(g)) {
    }
    g.endTurn();
}

} // namespace ironfront
