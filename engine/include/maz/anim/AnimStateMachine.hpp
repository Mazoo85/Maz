#pragma once

#include <functional>
#include <string>
#include <vector>

namespace maz::anim {

// ---- Animation state machine -------------------------------------------------------------------
// Godot's AnimationNodeStateMachine: a graph of named states with CROSS-FADING transitions. Each state
// here carries an integer payload (a clip index, or a blend-space id — so a state can itself be a blend
// space, giving "a state machine over blend spaces"). The machine tracks the current state, runs timed
// cross-fades on transitions, and reports the active state(s) with weights that sum to 1 — the exact
// same shape as a blend space's weights, so the output feeds straight into anim::blendPosesWeighted and
// the two compose (state weight x blend-space weight). Transitions fire on an explicit travel(name) or
// when a per-transition condition() returns true. Pure logic (no GPU/clips), so it unit-tests headless.

class AnimStateMachine {
public:
    struct Active {
        int id;       // the active state's payload
        float weight; // 0..1; the active entries sum to 1
    };

    int addState(const std::string& name, int payload) {
        m_states.push_back({name, payload});
        return static_cast<int>(m_states.size()) - 1;
    }

    // A directed transition from -> to with a cross-fade of `fadeSeconds` (0 = instant). `cond`, if set,
    // auto-fires the transition when it returns true and `from` is the current state.
    void addTransition(const std::string& from, const std::string& to, float fadeSeconds,
                       std::function<bool()> cond = {}) {
        m_trans.push_back({find(from), find(to), fadeSeconds, std::move(cond)});
    }

    void setStart(const std::string& name) {
        m_current = find(name);
        m_transitioning = false;
    }

    // Request a transition to `name` now (uses a defined from->to fade if one exists, else instant).
    void travel(const std::string& name) {
        const int to = find(name);
        if (to < 0 || (to == m_current && !m_transitioning)) {
            return;
        }
        float fade = 0.0f;
        for (const Trans& t : m_trans) {
            if (t.from == m_current && t.to == to) {
                fade = t.fade;
                break;
            }
        }
        begin(to, fade);
    }

    void update(float dt) {
        if (m_transitioning) {
            m_elapsed += dt;
            if (m_elapsed >= m_fade) {
                m_current = m_to;
                m_transitioning = false;
            }
            return;
        }
        // Take the first outgoing transition from the current state whose condition fires.
        for (const Trans& t : m_trans) {
            if (t.from == m_current && t.cond && t.cond()) {
                begin(t.to, t.fade);
                break;
            }
        }
    }

    // The active state(s) and their blend weights (sum to 1). During a cross-fade this is the from-state
    // fading out and the to-state fading in; otherwise it's just the current state at full weight.
    std::vector<Active> active() const {
        std::vector<Active> out;
        if (m_states.empty()) {
            return out;
        }
        if (m_transitioning && m_fade > 1e-6f) {
            float t = m_elapsed / m_fade;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            out.push_back({payload(m_from), 1.0f - t});
            out.push_back({payload(m_to), t});
        } else {
            out.push_back({payload(m_current), 1.0f});
        }
        return out;
    }

    bool transitioning() const { return m_transitioning; }
    // The state the machine is in (or heading toward mid-transition).
    int current() const { return m_transitioning ? m_to : m_current; }
    std::string currentName() const {
        const int s = current();
        return (s >= 0 && s < static_cast<int>(m_states.size())) ? m_states[static_cast<size_t>(s)].name
                                                                 : std::string();
    }

private:
    struct State {
        std::string name;
        int payload;
    };
    struct Trans {
        int from, to;
        float fade;
        std::function<bool()> cond;
    };

    int find(const std::string& n) const {
        for (size_t i = 0; i < m_states.size(); ++i) {
            if (m_states[i].name == n) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    int payload(int s) const {
        return (s >= 0 && s < static_cast<int>(m_states.size())) ? m_states[static_cast<size_t>(s)].payload
                                                                 : 0;
    }
    void begin(int to, float fade) {
        m_from = m_current;
        m_to = to;
        m_fade = fade;
        m_elapsed = 0.0f;
        m_transitioning = fade > 1e-6f;
        if (!m_transitioning) {
            m_current = to; // instant switch
        }
    }

    std::vector<State> m_states;
    std::vector<Trans> m_trans;
    int m_current = 0;
    bool m_transitioning = false;
    int m_from = 0, m_to = 0;
    float m_fade = 0.0f, m_elapsed = 0.0f;
};

} // namespace maz::anim
