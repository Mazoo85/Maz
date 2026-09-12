#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

// maz::core::simulatedAnnealing — a general-purpose optimizer for hard problems where you cannot enumerate
// every option: it searches for a state that minimises an "energy" (cost) function by wandering the state
// space, always accepting improvements but ALSO accepting worse states with a probability that shrinks as a
// "temperature" cools. That controlled willingness to go uphill early lets it escape local minima that a
// pure hill-climb would get stuck in — the reason it solves layout, scheduling, tour (TSP-style), puzzle,
// and procedural-placement problems that have no closed-form answer. It is generic: you supply the State
// type, an `energy(state)` cost, and a `neighbour(state, rand01)` that returns a slightly-mutated copy, plus
// a cooling schedule. Deterministic given a seed (embedded splitmix64 — no <random>, no clock), so a level
// or layout generated this way is reproducible. Header-only, std-only. Godot ships no optimizer.
namespace maz::core {

template <class State>
struct AnnealResult {
    State state;         // best state found
    double energy = 0.0; // its energy (cost)
};

// Minimise energy() over the reachable state space. neighbour(state, rand01) must return a mutated copy;
// rand01 is a callable returning a double in [0,1). Geometric cooling from startTemp down to endTemp.
template <class State, class Energy, class Neighbour>
AnnealResult<State> simulatedAnnealing(State initial, Energy energy, Neighbour neighbour, int iterations,
                                       double startTemp, double endTemp, std::uint64_t seed) {
    State cur = std::move(initial);
    double curE = energy(cur);
    State best = cur;
    double bestE = curE;

    if (iterations < 1) {
        return AnnealResult<State>{std::move(best), bestE};
    }
    if (startTemp <= 0.0) {
        startTemp = 1e-6;
    }
    if (endTemp <= 0.0) {
        endTemp = 1e-9;
    }

    std::uint64_t rngState = seed + 0x9E3779B97F4A7C15ull;
    auto rand01 = [&rngState]() -> double {
        rngState += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = rngState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);
        return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
    };

    const double ratio = endTemp / startTemp;
    for (int i = 0; i < iterations; ++i) {
        const double frac = iterations > 1 ? static_cast<double>(i) / static_cast<double>(iterations - 1)
                                           : 1.0;
        const double temp = startTemp * std::pow(ratio, frac);

        State cand = neighbour(cur, rand01);
        const double candE = energy(cand);
        const double dE = candE - curE;
        if (dE <= 0.0 || rand01() < std::exp(-dE / temp)) {
            cur = std::move(cand);
            curE = candE;
            if (curE < bestE) {
                best = cur;
                bestE = curE;
            }
        }
    }
    return AnnealResult<State>{std::move(best), bestE};
}

} // namespace maz::core
