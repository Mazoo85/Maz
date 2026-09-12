#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace maz::game::goap {

// Goal-Oriented Action Planning — a step beyond the behaviour tree. Instead of an author hand-wiring
// what an agent does, the agent is given a GOAL (a desired world-state) and a LIBRARY of actions, each
// with preconditions and effects, and it PLANS backward-optimally the cheapest sequence of actions that
// carries the current world from where it is to the goal. This is the classic F.E.A.R. AI technique; it
// gives emergent, re-plannable behaviour (drop in a new action and every agent can use it, no tree edits)
// that Godot ships no built-in equivalent for. Pure integer/graph search — deterministic, headless-
// testable, and it replays identically every run.
//
// The world is a set of boolean facts packed into a 64-bit word (bit i = fact i is true). A Condition is
// a PARTIAL state: `mask` marks which facts it constrains and `want` their required truth, so a goal or a
// precondition can care about three facts and ignore the other sixty-one. The planner is A* over world
// states: g = accumulated action cost, h = an admissible lower bound (you still need at least one more
// action, costing at least the cheapest action, whenever the goal is unmet), so the first plan it pops is
// guaranteed minimum-cost.

using State = std::uint64_t;

// Bit i as a mask — a tiny convenience so callers read `bit(Fact::HasWood)` instead of `1ull << 2`.
inline constexpr State bit(int i) {
    return State{1} << i;
}

// A partial world-state used for goals and action preconditions. `mask` = the facts this cares about,
// `want` = their required values. A full state `s` satisfies it when the cared-about bits match.
struct Condition {
    State mask = 0;
    State want = 0;

    // Require fact `i` to equal `value`. Chainable: Condition{}.require(0,true).require(2,false).
    Condition& require(int i, bool value) {
        mask |= bit(i);
        if (value) {
            want |= bit(i);
        } else {
            want &= ~bit(i);
        }
        return *this;
    }
};

inline bool satisfied(State s, const Condition& c) {
    return (s & c.mask) == (c.want & c.mask);
}

// One thing an agent can do. It is applicable only when `pre` holds; applying it forces the `set` facts
// true and the `clear` facts false (everything else untouched). `cost` is the search weight (time, risk…).
struct Action {
    std::string name;
    Condition pre;
    State set = 0;   // facts forced true
    State clear = 0; // facts forced false
    float cost = 1.0f;

    Action() = default;
    explicit Action(std::string n) : name(std::move(n)) {}

    Action& sets(int i) {
        set |= bit(i);
        return *this;
    }
    Action& clears(int i) {
        clear |= bit(i);
        return *this;
    }
    Action& needs(int i, bool value) {
        pre.require(i, value);
        return *this;
    }
    Action& withCost(float c) {
        cost = c;
        return *this;
    }
};

inline State apply(State s, const Action& a) {
    return (s | a.set) & ~a.clear;
}

// The result of a plan: `found` true if the goal is reachable, `steps` the indices (into the supplied
// action library) of the optimal action sequence in execution order, `cost` its total weight.
struct Plan {
    bool found = false;
    std::vector<int> steps;
    float cost = 0.0f;
};

// Plan the cheapest action sequence taking `start` to a state satisfying `goal`, using `library`.
// Returns an empty (found, cost 0) plan when the goal already holds. A* with an admissible heuristic, so
// the plan is optimal. `maxExpansions` bounds the search so a pathological library can't spin forever.
inline Plan plan(State start, const Condition& goal, const std::vector<Action>& library,
                 int maxExpansions = 100000) {
    Plan result;
    if (satisfied(start, goal)) {
        result.found = true;
        return result;
    }

    // Cheapest single action — an admissible per-step lower bound for the heuristic.
    float minCost = 1e30f;
    for (const Action& a : library) {
        if (a.cost < minCost) {
            minCost = a.cost;
        }
    }
    if (minCost > 1e29f) {
        minCost = 0.0f; // empty library: no progress possible, heuristic collapses to Dijkstra
    }

    struct Node {
        State state;
        float f; // g + h, the A* priority
    };
    struct Worse {
        bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
    };
    std::priority_queue<Node, std::vector<Node>, Worse> open;

    std::unordered_map<State, float> best;          // best known g per state
    std::unordered_map<State, std::pair<State, int>> came; // state -> (predecessor, action index)

    auto heuristic = [&](State s) { return satisfied(s, goal) ? 0.0f : minCost; };

    best[start] = 0.0f;
    open.push({start, heuristic(start)});

    int expansions = 0;
    while (!open.empty()) {
        const Node cur = open.top();
        open.pop();
        const State s = cur.state;
        const float g = best[s];
        // Stale queue entry (a cheaper path to `s` was found after this was pushed) — skip.
        if (cur.f > g + heuristic(s) + 1e-6f) {
            continue;
        }
        if (satisfied(s, goal)) {
            // Reconstruct the action sequence by walking predecessors back to the start.
            State node = s;
            while (node != start) {
                const auto& link = came[node];
                result.steps.push_back(link.second);
                node = link.first;
            }
            for (std::size_t i = 0, j = result.steps.size(); i < j / 2; ++i) {
                std::swap(result.steps[i], result.steps[j - 1 - i]);
            }
            result.found = true;
            result.cost = g;
            return result;
        }
        if (++expansions > maxExpansions) {
            break;
        }
        for (std::size_t ai = 0; ai < library.size(); ++ai) {
            const Action& a = library[ai];
            if (!satisfied(s, a.pre)) {
                continue;
            }
            const State ns = apply(s, a);
            if (ns == s) {
                continue; // action changes nothing here — no progress
            }
            const float ng = g + a.cost;
            const auto it = best.find(ns);
            if (it == best.end() || ng < it->second - 1e-6f) {
                best[ns] = ng;
                came[ns] = {s, static_cast<int>(ai)};
                open.push({ns, ng + heuristic(ns)});
            }
        }
    }
    return result; // found stays false — goal unreachable within the expansion budget
}

} // namespace maz::game::goap
