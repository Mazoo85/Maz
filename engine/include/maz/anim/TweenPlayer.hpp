#pragma once

#include "maz/anim/Tween.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace maz::anim {

// Tween sequencer / property animator — Godot's SceneTreeTween (create_tween + tween_property/
// tween_interval/tween_callback + parallel + set_loops). anim::Tween is a single time-cursor that
// interpolates ONE from→to over one duration; this composes many of those into a CHOREOGRAPHY: a list
// of steps that run one after another, any of which may run in PARALLEL with its neighbours, with
// delays and callbacks interleaved and the whole thing optionally looping. Each property step is bound
// to a value (a `void(float)` setter) that it writes every update, so one player animates a dot's x,
// then its y, while a second grows its radius — all advanced by a single update(dt). Pure logic
// (no GPU/allocation beyond the step list), so it unit-tests headlessly and runs deterministically
// under the fixed timestep.
//
// Model (mirrors Godot): the player holds an ordered list of GROUPS; groups run sequentially, and the
// tweeners inside a group run in parallel. `append*` starts a new group; `parallel*` adds to the
// current (last) group. A group's duration is its longest tweener; a tweener shorter than its group
// holds at its end value for the remainder.

struct Tweener {
    enum class Kind { Property, Interval, Callback };
    Kind kind = Kind::Property;
    float duration = 0.0f;
    Ease easing = Ease::Linear;
    std::function<void(float)> setter; // Property: written every update with the eased value
    float from = 0.0f;
    float to = 0.0f;
    std::function<void()> callback; // Callback: fired once when its group is entered/completed
};

class TweenPlayer {
public:
    // --- Builders (chainable) -----------------------------------------------------------------------
    // Append a property tween as a NEW sequential step: interpolate `setter` from→to over `dur`.
    TweenPlayer& appendProperty(std::function<void(float)> setter, float from, float to, float dur,
                                Ease easing = Ease::Linear) {
        m_groups.push_back({makeProperty(std::move(setter), from, to, dur, easing)});
        return *this;
    }
    // Add a property tween to the CURRENT step so it runs in parallel with the others there.
    TweenPlayer& parallelProperty(std::function<void(float)> setter, float from, float to, float dur,
                                  Ease easing = Ease::Linear) {
        if (m_groups.empty()) {
            m_groups.emplace_back();
        }
        m_groups.back().push_back(makeProperty(std::move(setter), from, to, dur, easing));
        return *this;
    }
    // A pure delay before the next step.
    TweenPlayer& appendInterval(float dur) {
        Tweener t;
        t.kind = Tweener::Kind::Interval;
        t.duration = dur;
        m_groups.push_back({std::move(t)});
        return *this;
    }
    // Fire `fn` once, sequenced after the previous step.
    TweenPlayer& appendCallback(std::function<void()> fn) {
        Tweener t;
        t.kind = Tweener::Kind::Callback;
        t.duration = 0.0f;
        t.callback = std::move(fn);
        m_groups.push_back({std::move(t)});
        return *this;
    }
    // Number of times the whole sequence plays; <= 0 means loop forever.
    TweenPlayer& setLoops(int loops) {
        m_loops = loops;
        return *this;
    }

    // --- Runtime ------------------------------------------------------------------------------------
    void update(float dt) {
        if (m_done || m_groups.empty()) {
            return;
        }
        m_groupTime += dt;
        int guard = 0;
        for (;;) {
            if (++guard > 4096) { // degenerate all-zero-duration infinite loop backstop
                m_done = true;
                return;
            }
            if (m_current >= m_groups.size()) {
                ++m_loopsDone;
                if (m_loops > 0 && m_loopsDone >= m_loops) {
                    m_done = true;
                    return;
                }
                m_current = 0; // restart the sequence (loop)
            }
            const float gd = groupDuration(m_groups[m_current]);
            if (m_groupTime < gd) {
                break; // still inside the current group
            }
            finalizeGroup(m_current); // snap properties to their end + fire callbacks
            m_groupTime -= gd;
            ++m_current;
        }
        if (!m_done && m_current < m_groups.size()) {
            applyGroup(m_current, m_groupTime);
        }
    }

    bool finished() const { return m_done; }
    std::size_t stepCount() const { return m_groups.size(); }

    // Total time of one pass through the sequence (sum of group durations).
    float totalDuration() const {
        float sum = 0.0f;
        for (const auto& g : m_groups) {
            sum += groupDuration(g);
        }
        return sum;
    }

    void reset() {
        m_current = 0;
        m_groupTime = 0.0f;
        m_loopsDone = 0;
        m_done = false;
    }

private:
    static Tweener makeProperty(std::function<void(float)> setter, float from, float to, float dur,
                                Ease easing) {
        Tweener t;
        t.kind = Tweener::Kind::Property;
        t.duration = dur;
        t.easing = easing;
        t.setter = std::move(setter);
        t.from = from;
        t.to = to;
        return t;
    }

    static float groupDuration(const std::vector<Tweener>& group) {
        float d = 0.0f;
        for (const Tweener& t : group) {
            if (t.duration > d) {
                d = t.duration;
            }
        }
        return d;
    }

    // Write each property to its eased value at `localTime` (each tweener clamped to its own duration).
    static void applyGroup(std::vector<Tweener>& group, float localTime) {
        for (Tweener& t : group) {
            if (t.kind == Tweener::Kind::Property && t.setter) {
                const float tt = t.duration > 0.0f ? localTime / t.duration : 1.0f;
                t.setter(t.from + (t.to - t.from) * ease(t.easing, tt));
            }
        }
    }

    // Snap properties to their end value and fire callbacks when a group completes.
    static void finalizeGroup(std::vector<Tweener>& group) {
        for (Tweener& t : group) {
            if (t.kind == Tweener::Kind::Property && t.setter) {
                t.setter(t.to);
            } else if (t.kind == Tweener::Kind::Callback && t.callback) {
                t.callback();
            }
        }
    }

    void applyGroup(std::size_t gi, float localTime) { applyGroup(m_groups[gi], localTime); }
    void finalizeGroup(std::size_t gi) { finalizeGroup(m_groups[gi]); }

    std::vector<std::vector<Tweener>> m_groups;
    std::size_t m_current = 0;
    float m_groupTime = 0.0f;
    int m_loops = 1;
    int m_loopsDone = 0;
    bool m_done = false;
};

} // namespace maz::anim
