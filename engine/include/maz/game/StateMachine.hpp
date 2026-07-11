#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace maz::game {

// A lightweight finite state machine keyed by an integer/enum state id — the decision layer that
// sits above steering/pathfinding (guard patrol -> chase -> return), and equally the backbone of
// game flow (menu/playing/paused) or animation states. Each state has optional onEnter/onUpdate/
// onExit callbacks. Transitions are guarded predicates evaluated every update(); "any" transitions
// fire from whatever state is current. Evaluation is deterministic: on each update the any-
// transitions are checked first (in registration order), then the current state's transitions, and
// the first guard that returns true wins. Header-only, no GPU/allocation beyond the callback lists.
template <typename StateId>
class StateMachine {
public:
    using Action = std::function<void()>;
    using UpdateFn = std::function<void(float dt)>;
    using Guard = std::function<bool()>;

    // Register a state. onUpdate runs each update() while current; onEnter/onExit fire on switches.
    void addState(StateId id, UpdateFn onUpdate = {}, Action onEnter = {}, Action onExit = {}) {
        m_states.push_back({id, std::move(onEnter), std::move(onUpdate), std::move(onExit)});
    }
    // A transition from `from` to `to` taken when `guard()` is true.
    void addTransition(StateId from, StateId to, Guard guard) {
        m_transitions.push_back({from, to, std::move(guard), false});
    }
    // A transition taken from ANY state when `guard()` is true (checked before per-state ones).
    void addAnyTransition(StateId to, Guard guard) {
        m_transitions.push_back({StateId{}, to, std::move(guard), true});
    }

    // Enter `id` as the initial state (fires its onEnter; does not run any onExit).
    void start(StateId id) {
        m_current = id;
        m_started = true;
        if (const State* s = find(id)) {
            if (s->onEnter) {
                s->onEnter();
            }
        }
    }

    // Evaluate transitions (possibly switching state), then run the current state's onUpdate.
    void update(float dt) {
        if (!m_started) {
            return;
        }
        // Any-transitions first, then the current state's own transitions; first true guard wins.
        for (int pass = 0; pass < 2; ++pass) {
            const bool wantAny = pass == 0;
            for (const Transition& tr : m_transitions) {
                if (tr.any != wantAny) {
                    continue;
                }
                if (!tr.any && !(tr.from == m_current)) {
                    continue;
                }
                if (tr.to == m_current) {
                    continue; // never self-transition
                }
                if (tr.guard && tr.guard()) {
                    switchTo(tr.to);
                    goto ran; // one transition per update, then run the (new) state's update
                }
            }
        }
    ran:
        if (const State* s = find(m_current)) {
            if (s->onUpdate) {
                s->onUpdate(dt);
            }
        }
    }

    StateId current() const { return m_current; }
    bool isIn(StateId id) const { return m_started && m_current == id; }
    // Number of state switches taken since start() (start itself is not counted). Useful for tests.
    uint32_t transitionCount() const { return m_switches; }

private:
    struct State {
        StateId id;
        Action onEnter;
        UpdateFn onUpdate;
        Action onExit;
    };
    struct Transition {
        StateId from;
        StateId to;
        Guard guard;
        bool any;
    };

    const State* find(StateId id) const {
        for (const State& s : m_states) {
            if (s.id == id) {
                return &s;
            }
        }
        return nullptr;
    }

    void switchTo(StateId to) {
        if (const State* from = find(m_current)) {
            if (from->onExit) {
                from->onExit();
            }
        }
        m_current = to;
        ++m_switches;
        if (const State* s = find(to)) {
            if (s->onEnter) {
                s->onEnter();
            }
        }
    }

    std::vector<State> m_states;
    std::vector<Transition> m_transitions;
    StateId m_current{};
    bool m_started = false;
    uint32_t m_switches = 0;
};

} // namespace maz::game
