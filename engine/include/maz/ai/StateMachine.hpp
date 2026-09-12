#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>  // std::move

#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// An event-driven finite state machine (the Godot gameplay state-machine analog).
// Register StringId-named states with optional onEnter/onUpdate(dt)/onExit
// callbacks, wire [from]+[event]->to transitions, then start(), fire(event) to
// transition (onExit of the old state runs, then onEnter of the new state; a
// self-transition to==current fires BOTH), and update(dt) to tick the current
// state. Composes iter7 maz::core::StringId + std::function, which holds a
// capturing lambda, a free function, or an iter19 maz::core::Delegate. Callbacks
// run at well-defined points: onExit is called while m_current still == the OLD
// state, and onEnter is called after m_current == the NEW state; do NOT fire/add
// from inside a callback in a way that reasons about mid-transition state. NOT
// thread-safe. Condition/guard-based transitions, hierarchical/nested states, and
// a transition history are future refinements (not built here).

namespace maz::ai {

class StateMachine {
  public:
    using Action = std::function<void()>;
    using UpdateAction = std::function<void(float)>;

    // Registers a state. Callbacks are optional (an empty std::function is never
    // called). name must be valid and not already registered.
    void addState(maz::core::StringId name, Action onEnter = {}, UpdateAction onUpdate = {}, Action onExit = {}) {
        MAZ_ASSERT(name.valid(), "StateMachine::addState: invalid state name");
        MAZ_ASSERT(!hasState(name), "StateMachine::addState: duplicate state");
        m_states.emplace(name, State{ std::move(onEnter), std::move(onUpdate), std::move(onExit) });
    }

    // Wires [from]+[event]->to. from and to must be registered states. A duplicate
    // (from,event) overwrites the target — last write wins.
    void addTransition(maz::core::StringId from, maz::core::StringId event, maz::core::StringId to) {
        MAZ_ASSERT(event.valid(), "StateMachine::addTransition: invalid event");
        MAZ_ASSERT(hasState(from) && hasState(to), "StateMachine::addTransition: from/to must be registered states");
        m_transitions[from][event] = to;
    }

    // Selects the initial state. Does NOT enter it — start() performs the entry.
    void setInitial(maz::core::StringId name) {
        MAZ_ASSERT(hasState(name), "StateMachine::setInitial: unknown state");
        m_initial = name;
    }

    // Enters the initial state (fires its onEnter). Requires a valid initial state
    // and that the machine is not already started.
    void start() {
        MAZ_ASSERT(m_initial.valid() && hasState(m_initial), "StateMachine::start: no valid initial state");
        MAZ_ASSERT(!m_started, "StateMachine::start: already started");
        m_current = m_initial;
        m_started = true;
        const State& s = m_states.at(m_current);
        if (s.onEnter) { s.onEnter(); }
    }

    // Fires an event. If a transition exists for (current,event): calls the old
    // state's onExit, moves to the target, calls the new state's onEnter, and
    // returns true. A self-transition (to==current) fires both onExit and onEnter.
    // With no matching transition: returns false, no state change, no callbacks.
    bool fire(maz::core::StringId event) {
        MAZ_ASSERT(m_started, "StateMachine::fire before start");
        auto fromIt = m_transitions.find(m_current);
        if (fromIt == m_transitions.end()) { return false; }
        auto eventIt = fromIt->second.find(event);
        if (eventIt == fromIt->second.end()) { return false; }
        // Capture the target first, then run onExit of the OLD current (m_current
        // still == old), assign, then run onEnter of the NEW current.
        const maz::core::StringId to = eventIt->second;
        const State& oldState = m_states.at(m_current);
        if (oldState.onExit) { oldState.onExit(); }
        m_current = to;
        const State& newState = m_states.at(m_current);
        if (newState.onEnter) { newState.onEnter(); }
        return true;
    }

    // Ticks the current state's onUpdate (if set) with dt. Must be started.
    void update(float dt) {
        MAZ_ASSERT(m_started, "StateMachine::update before start");
        const State& s = m_states.at(m_current);
        if (s.onUpdate) { s.onUpdate(dt); }
    }

    // The current state (invalid/default StringId if not started).
    maz::core::StringId current() const { return m_current; }

    bool hasState(maz::core::StringId name) const { return m_states.find(name) != m_states.end(); }

    bool started() const { return m_started; }

    // Returns to the un-started state. Does NOT call the current state's onExit
    // (it's an abrupt reset); a subsequent start() re-enters the initial state and
    // fires its onEnter.
    void reset() {
        m_started = false;
        m_current = maz::core::StringId{};
    }

    std::size_t stateCount() const { return m_states.size(); }

  private:
    // core/StringId.hpp specializes std::hash<StringId>, so no local hasher is needed.
    struct State {
        Action onEnter;
        UpdateAction onUpdate;
        Action onExit;
    };

    std::unordered_map<maz::core::StringId, State> m_states;
    // [from][event] = to
    std::unordered_map<maz::core::StringId, std::unordered_map<maz::core::StringId, maz::core::StringId>> m_transitions;
    maz::core::StringId m_initial;
    maz::core::StringId m_current;
    bool m_started = false;
};

} // namespace maz::ai
