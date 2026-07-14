#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <utility>  // std::move
#include <vector>

#include "maz/core/Assert.hpp"

// A timer / callback scheduler advanced by update(dt) — the Godot SceneTreeTimer
// analog. after(delay, cb) fires a one-shot callback once, delay seconds after it
// was scheduled; every(interval, cb) fires repeatedly with catch-up — a single
// large dt spanning N whole intervals fires the callback N times, carrying the
// leftover remainder into the next tick. cancel(id) stops a still-pending timer.
// Callbacks are std::function<void()>, so a capturing lambda, a free function, or
// a maz::core::Delegate all compose. Timers fire in registration order within a
// tick.
//
// Safe under re-entrancy: a callback may cancel any timer (including itself) and
// may schedule new timers. Cancel is a deactivate-not-erase, so a timer cancelled
// from inside another callback this same tick never fires — the post-update sweep
// removes it. Newly scheduled timers are DEFERRED: while update() runs, adds route
// to m_pending and are merged into m_timers only after the loop, so they do NOT
// fire this tick and m_timers is never resized mid-loop (indexing m_timers[i] into
// a non-reallocating vector stays valid across a callback). interval MUST be > 0
// so the repeating catch-up loop strictly advances and always terminates.
//
// NOT thread-safe. Pause / time-scale and a SceneTree binding are future
// refinements (not built here).

namespace maz::core {

// A stable handle to a scheduled timer. value 0 is the invalid/null id.
struct TimerId {
    std::uint64_t value = 0;

    constexpr bool valid() const { return value != 0; }
    constexpr bool operator==(const TimerId& o) const { return value == o.value; }
};

class TimerManager {
  public:
    using Callback = std::function<void()>;

    // Schedules a one-shot timer that fires cb once, delay seconds from now.
    // delay must be >= 0 and cb non-null. Returns a handle usable with cancel().
    // Scheduled from inside a callback: deferred to the next update (see class doc).
    TimerId after(float delay, Callback cb) {
        MAZ_ASSERT(delay >= 0.0f, "TimerManager::after: negative delay");
        MAZ_ASSERT(cb != nullptr, "TimerManager::after: null callback");
        const std::uint64_t id = m_nextId++;
        Timer t{ id, delay, delay, /*repeating*/ false, /*active*/ true, std::move(cb) };
        if (m_updating) {
            m_pending.push_back(std::move(t));
        } else {
            m_timers.push_back(std::move(t));
        }
        return TimerId{ id };
    }

    // Schedules a repeating timer that fires cb every interval seconds. interval
    // must be > 0 (guards the catch-up loop against an infinite loop) and cb
    // non-null. Returns a handle usable with cancel().
    TimerId every(float interval, Callback cb) {
        MAZ_ASSERT(interval > 0.0f, "TimerManager::every: interval must be > 0");
        MAZ_ASSERT(cb != nullptr, "TimerManager::every: null callback");
        const std::uint64_t id = m_nextId++;
        Timer t{ id, interval, interval, /*repeating*/ true, /*active*/ true, std::move(cb) };
        if (m_updating) {
            m_pending.push_back(std::move(t));
        } else {
            m_timers.push_back(std::move(t));
        }
        return TimerId{ id };
    }

    // Cancels the timer with the given id. Searches both live and pending timers;
    // an already-inactive or unknown id is a no-op. Deactivates rather than
    // erases, so cancelling from inside a callback this same tick is safe — the
    // cancelled timer will not fire and is removed by the post-update sweep.
    // Returns true if a still-active timer was found and deactivated.
    bool cancel(TimerId id) {
        if (!id.valid()) { return false; }
        for (Timer& t : m_timers) {
            if (t.id == id.value && t.active) {
                t.active = false;
                return true;
            }
        }
        for (Timer& t : m_pending) {
            if (t.id == id.value && t.active) {
                t.active = false;
                return true;
            }
        }
        return false;
    }

    // Advances all timers by dt seconds, firing any that come due. dt must be >= 0.
    // Re-entrant update() is a programmer error. One-shot timers fire once then
    // deactivate; repeating timers fire once per whole interval elapsed (catch-up).
    // Callbacks may cancel or schedule timers (see class doc for the semantics).
    void update(float dt) {
        MAZ_ASSERT(dt >= 0.0f, "TimerManager::update: negative dt");
        MAZ_ASSERT(!m_updating, "TimerManager::update: re-entrant update");
        m_updating = true;

        // Iterate BY INDEX: adds route to m_pending while m_updating, so m_timers
        // is never resized during the loop and m_timers[i] stays valid across a
        // callback. Re-read m_timers[i].active before each fire — an earlier
        // callback this tick may have cancelled it.
        for (std::size_t i = 0; i < m_timers.size(); ++i) {
            if (!m_timers[i].active) { continue; }
            m_timers[i].remaining -= dt;

            if (!m_timers[i].repeating) {
                if (m_timers[i].active && m_timers[i].remaining <= 0.0f) {
                    m_timers[i].cb();
                    m_timers[i].active = false;  // one-shot done
                }
            } else {
                // Catch-up: fire once per whole interval elapsed. interval > 0 is
                // asserted, so remaining strictly increases each iteration and the
                // loop terminates. Re-check active each iteration — a callback may
                // cancel this very timer.
                while (m_timers[i].active && m_timers[i].remaining <= 0.0f) {
                    m_timers[i].remaining += m_timers[i].interval;
                    m_timers[i].cb();
                }
            }
        }

        m_updating = false;

        // Merge timers scheduled during this update, THEN sweep inactive ones (so a
        // pending timer cancelled before merge is swept too).
        if (!m_pending.empty()) {
            m_timers.insert(m_timers.end(),
                            std::make_move_iterator(m_pending.begin()),
                            std::make_move_iterator(m_pending.end()));
            m_pending.clear();
        }
        std::erase_if(m_timers, [](const Timer& t) { return !t.active; });
    }

    // Number of live (active) timers across both live and pending sets.
    std::size_t activeCount() const {
        std::size_t n = 0;
        for (const Timer& t : m_timers) { if (t.active) { ++n; } }
        for (const Timer& t : m_pending) { if (t.active) { ++n; } }
        return n;
    }

    // Removes all timers. Must not be called from inside a callback (re-entrant).
    void clear() {
        MAZ_ASSERT(!m_updating, "TimerManager::clear: called during update");
        m_timers.clear();
        m_pending.clear();
    }

    bool empty() const { return activeCount() == 0; }

  private:
    struct Timer {
        std::uint64_t id;
        float interval;   // seconds between fires (reused for repeats)
        float remaining;  // seconds until the next fire
        bool repeating;
        bool active;
        Callback cb;
    };

    std::vector<Timer> m_timers;   // live timers, ticked by update()
    std::vector<Timer> m_pending;  // scheduled during an update, merged afterwards
    std::uint64_t m_nextId = 1;    // 0 is reserved for the null TimerId
    bool m_updating = false;       // true while inside update()
};

} // namespace maz::core
