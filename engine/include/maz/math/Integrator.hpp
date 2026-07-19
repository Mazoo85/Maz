#pragma once

// maz::math numerical ODE integrators — advance a continuous system state by dt given its derivative.
// The engine's physics uses semi-implicit Euler (fast, stable enough for contacts); these are the
// accurate general-purpose integrators for smooth continuous motion where Euler drifts: orbital and
// n-body motion, spring/pendulum simulation, projectiles with air drag, and any custom equation of
// motion. RK4 (classic 4th-order Runge-Kutta) is the workhorse — its error shrinks ~16x each time the
// step halves. Generic over any State type that supports `State + State` and `State * float` (plain
// float, a vec, or a small phase-space struct), with the derivative supplied as a callable
// deriv(state, t). Godot exposes no general integrator to gameplay code, so this is a beyond-Godot
// numerics utility. Header-only, std-only, deterministic.
namespace maz::math {

// One classic RK4 step: returns the state advanced from time t by dt. `deriv(state, t)` gives dState/dt.
template <typename State, typename Deriv>
State integrateRK4(const State& state, float t, float dt, Deriv deriv) {
    const State k1 = deriv(state, t);
    const State k2 = deriv(state + k1 * (dt * 0.5f), t + dt * 0.5f);
    const State k3 = deriv(state + k2 * (dt * 0.5f), t + dt * 0.5f);
    const State k4 = deriv(state + k3 * dt, t + dt);
    return state + (k1 + k2 * 2.0f + k3 * 2.0f + k4) * (dt / 6.0f);
}

// One midpoint (RK2) step — cheaper, 2nd-order accurate.
template <typename State, typename Deriv>
State integrateRK2(const State& state, float t, float dt, Deriv deriv) {
    const State k1 = deriv(state, t);
    const State k2 = deriv(state + k1 * (dt * 0.5f), t + dt * 0.5f);
    return state + k2 * dt;
}

// One explicit-Euler step — 1st-order, for reference/comparison.
template <typename State, typename Deriv>
State integrateEuler(const State& state, float t, float dt, Deriv deriv) {
    return state + deriv(state, t) * dt;
}

// Advance `state` by `steps` RK4 steps of size dt starting at t0. Returns the final state.
template <typename State, typename Deriv>
State integrateRK4Steps(State state, float t0, float dt, int steps, Deriv deriv) {
    float t = t0;
    for (int i = 0; i < steps; ++i) {
        state = integrateRK4(state, t, dt, deriv);
        t += dt;
    }
    return state;
}

} // namespace maz::math
