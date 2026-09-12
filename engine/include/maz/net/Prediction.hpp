#pragma once

#include "maz/net/Reliability.hpp" // seqGreaterThan

#include <cstdint>
#include <deque>

// maz::net client-side prediction + server reconciliation — what makes a networked game feel
// instant despite round-trip latency. Instead of waiting for the server to confirm each move, the
// client PREDICTS: it applies its own input locally the moment it's pressed and keeps a history of
// every still-unacknowledged input. When an authoritative snapshot arrives (the true state AFTER
// some input sequence the server processed), the client RECONCILES: it snaps to that authoritative
// state and re-simulates every input the server hasn't seen yet — so a correct prediction is
// invisible and a misprediction is smoothly caught up. This is the Valve/Gaffer client-prediction
// model, the same idea Godot's high-level multiplayer leaves to the game; here it's a reusable,
// deterministic, unit-tested primitive. Templated on your State, Input, and a pure step function
// `State step(const State&, const Input&)`. No sockets or clocks — the caller drives the tick.
namespace maz::net {

template <typename State, typename Input>
class PredictionBuffer {
  public:
    PredictionBuffer() = default;
    explicit PredictionBuffer(const State& initial) : m_predicted(initial) {}

    // Apply an input the client just issued (tagged with the sequence number it was sent under):
    // advance the predicted state and remember the input until the server acknowledges it. Returns
    // the new predicted state to render immediately — no latency waiting on the server.
    template <typename StepFn>
    State applyInput(uint16_t seq, const Input& in, StepFn&& step) {
        m_predicted = step(m_predicted, in);
        m_pending.push_back({seq, in});
        return m_predicted;
    }

    // Fold in an authoritative server state that reflects the world AFTER input `ackedSeq` was
    // processed: drop every pending input up to and including that sequence, snap to `state`, then
    // re-simulate the inputs the server hasn't seen yet. If the prediction was right the result is
    // unchanged; if it was wrong this is the correction. Returns the reconciled "now" state.
    template <typename StepFn>
    State reconcile(uint16_t ackedSeq, const State& state, StepFn&& step) {
        while (!m_pending.empty() && !seqGreaterThan(m_pending.front().seq, ackedSeq)) {
            m_pending.pop_front();
        }
        m_predicted = state;
        for (const Cmd& c : m_pending) {
            m_predicted = step(m_predicted, c.input);
        }
        return m_predicted;
    }

    const State& predicted() const { return m_predicted; }
    void setState(const State& s) { m_predicted = s; }

    // Number of inputs issued but not yet acknowledged by the server.
    std::size_t pendingCount() const { return m_pending.size(); }
    bool hasPending() const { return !m_pending.empty(); }
    void clear() { m_pending.clear(); }

  private:
    struct Cmd {
        uint16_t seq;
        Input input;
    };
    std::deque<Cmd> m_pending;
    State m_predicted{};
};

} // namespace maz::net
