#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace maz::core {

// Time-based scheduling: the "do this later" and "do this on a beat" primitive nearly all gameplay
// needs — spawn a wave every few seconds, fire a callback after a delay, run a cooldown, drive a
// scripted sequence. Two pieces:
//   * Scheduler — fire-and-forget timers: after(delay) runs a callback once; every(interval, count)
//     runs it repeatedly (a finite count or forever); cancel() stops a pending one by handle. update()
//     advances all timers and fires whatever came due, catching up if a big dt spans several
//     intervals, and staying safe when a callback schedules or cancels timers mid-update.
//   * Sequence — an ordered script of steps played over time: wait(seconds), call(fn), and
//     span(duration, fn(progress 0..1)) for animated stretches; optionally loop. Built on the same
//     fixed-step dt the rest of the engine runs on, so it's fully deterministic. Header-only.

class Scheduler {
public:
    using Handle = uint32_t;
    using Callback = std::function<void()>;
    static constexpr Handle kInvalid = 0;

    // Run cb once after `delay` seconds.
    Handle after(double delay, Callback cb) { return add(delay, delay, 1, std::move(cb)); }

    // Run cb every `interval` seconds. repeats < 0 = forever; otherwise fire exactly `repeats` times.
    Handle every(double interval, Callback cb, int repeats = -1) {
        return add(interval, interval, repeats, std::move(cb));
    }

    // Cancel a pending timer. Returns true if it was live.
    bool cancel(Handle h) {
        for (Timer& t : m_timers) {
            if (t.id == h && t.alive) {
                t.alive = false;
                return true;
            }
        }
        return false;
    }

    void clear() { m_timers.clear(); }

    // Number of live timers.
    std::size_t count() const {
        std::size_t n = 0;
        for (const Timer& t : m_timers)
            if (t.alive) ++n;
        return n;
    }

    // Advance time and fire due callbacks. Callbacks are invoked AFTER stepping every timer, so a
    // callback may safely add or cancel timers without disturbing this update.
    void update(double dt) {
        std::vector<Callback> fired;
        const std::size_t n = m_timers.size(); // snapshot: timers added during firing wait for next tick
        for (std::size_t i = 0; i < n; ++i) {
            Timer& t = m_timers[i];
            if (!t.alive) continue;
            t.remaining -= dt;
            while (t.alive && t.remaining <= 0.0) {
                fired.push_back(t.cb);
                const bool more = (t.repeats < 0) || (t.repeats > 1);
                if (t.repeats > 0) --t.repeats;
                if (more)
                    t.remaining += t.interval;
                else
                    t.alive = false;
            }
        }
        // Drop dead timers before firing, so count() inside a callback is accurate.
        m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(),
                                      [](const Timer& t) { return !t.alive; }),
                       m_timers.end());
        for (Callback& cb : fired)
            if (cb) cb();
    }

private:
    struct Timer {
        Handle id;
        double remaining;
        double interval;
        int repeats; // remaining fire count; <0 = infinite
        Callback cb;
        bool alive;
    };

    Handle add(double remaining, double interval, int repeats, Callback cb) {
        const Handle id = m_next++;
        if (m_next == kInvalid) m_next = 1;
        m_timers.push_back(Timer{id, remaining, interval, repeats, std::move(cb), true});
        return id;
    }

    std::vector<Timer> m_timers;
    Handle m_next = 1;
};

// An ordered, time-driven script of steps.
class Sequence {
public:
    // Wait for `seconds` before the next step.
    Sequence& wait(double seconds) {
        m_steps.push_back(Step{StepType::Wait, seconds, nullptr, nullptr});
        return *this;
    }
    // Fire a callback instantly, then advance.
    Sequence& call(std::function<void()> fn) {
        m_steps.push_back(Step{StepType::Call, 0.0, std::move(fn), nullptr});
        return *this;
    }
    // Drive fn(progress) from 0 to 1 over `duration` seconds (fn is called with 1.0 exactly once at
    // the end).
    Sequence& span(double duration, std::function<void(float)> fn) {
        m_steps.push_back(Step{StepType::Span, duration, nullptr, std::move(fn)});
        return *this;
    }
    // Restart from the top when the last step finishes.
    Sequence& loop(bool on = true) {
        m_loop = on;
        return *this;
    }

    void reset() {
        m_index = 0;
        m_elapsed = 0.0;
        m_done = false;
    }

    bool done() const { return m_done; }
    std::size_t index() const { return m_index; }

    void update(double dt) {
        if (m_done || m_steps.empty()) return;
        double budget = dt;
        // Advance through as many steps as this dt covers. Instantaneous Call steps always run when
        // reached (even with zero budget left), so a `wait(1).call(...)` fires on an exact-1s step.
        while (!m_done) {
            Step& s = m_steps[m_index];
            if (s.type == StepType::Call) {
                if (s.call) s.call();
                advance();
                continue;
            }
            if (budget <= 0.0) break;
            if (s.type == StepType::Wait) {
                m_elapsed += budget;
                if (m_elapsed >= s.duration) {
                    budget = m_elapsed - s.duration; // carry leftover into the next step
                    advance();
                } else {
                    budget = 0.0;
                }
            } else { // Span
                m_elapsed += budget;
                if (m_elapsed >= s.duration) {
                    if (s.span) s.span(1.0f);
                    budget = m_elapsed - s.duration;
                    advance();
                } else {
                    const float p = s.duration > 0.0 ? static_cast<float>(m_elapsed / s.duration)
                                                     : 1.0f;
                    if (s.span) s.span(p);
                    budget = 0.0;
                }
            }
        }
    }

private:
    enum class StepType { Wait, Call, Span };
    struct Step {
        StepType type;
        double duration;
        std::function<void()> call;
        std::function<void(float)> span;
    };

    void advance() {
        m_elapsed = 0.0;
        ++m_index;
        if (m_index >= m_steps.size()) {
            if (m_loop)
                m_index = 0;
            else
                m_done = true;
        }
    }

    std::vector<Step> m_steps;
    std::size_t m_index = 0;
    double m_elapsed = 0.0;
    bool m_loop = false;
    bool m_done = false;
};

} // namespace maz::core
