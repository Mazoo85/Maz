#pragma once

#include <cstddef>
#include <unordered_map>

// maz::game ability cooldown manager — the timer bank behind spell cooldowns, dashes, the global cooldown,
// and any "you can't do that yet" gate. Abilities are keyed by an integer id; `tryUse` fires an ability
// and puts it on cooldown in one call (returning false if it is still recharging), `tick` advances every
// active timer by the frame delta, and `isReady` / `remaining` / `fraction` drive the greyed-out button
// and the radial sweep. Cooldowns that reach zero are dropped, so an idle manager holds nothing. Godot
// ships Timer nodes but no ability-cooldown abstraction — games wire this up by hand every time — so this
// is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

class CooldownManager {
public:
    CooldownManager() = default;

    // Put ability `id` on cooldown for `duration` seconds, resetting any timer already running. A
    // non-positive duration clears the cooldown (the ability is immediately ready).
    void start(int id, double duration) {
        if (duration <= 0.0) {
            m_timers.erase(id);
            return;
        }
        m_timers[id] = Timer{duration, duration};
    }

    // If the ability is ready, put it on cooldown for `duration` and return true; otherwise return false
    // and leave the running cooldown untouched. The canonical "use ability" call.
    bool tryUse(int id, double duration) {
        if (!isReady(id)) return false;
        start(id, duration);
        return true;
    }

    // Advance every active cooldown by `dt` seconds (non-positive dt is ignored). Timers that reach zero
    // are removed, so the ability becomes ready.
    void tick(double dt) {
        if (dt <= 0.0) return;
        for (auto it = m_timers.begin(); it != m_timers.end();) {
            it->second.remaining -= dt;
            if (it->second.remaining <= 0.0)
                it = m_timers.erase(it);
            else
                ++it;
        }
    }

    // True when the ability is off cooldown (no active timer).
    bool isReady(int id) const { return m_timers.find(id) == m_timers.end(); }

    // Seconds left on the cooldown, or 0 if ready.
    double remaining(int id) const {
        const auto it = m_timers.find(id);
        return it == m_timers.end() ? 0.0 : it->second.remaining;
    }

    // Fraction [0,1] of the cooldown still to go: 1.0 right after use, shrinking to 0.0 when ready — the
    // fill amount for a radial cooldown sweep. Returns 0 for a ready ability.
    double fraction(int id) const {
        const auto it = m_timers.find(id);
        if (it == m_timers.end() || it->second.duration <= 0.0) return 0.0;
        const double f = it->second.remaining / it->second.duration;
        return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    }

    // Shorten a running cooldown by `seconds` (a haste / cooldown-reduction effect); clamps at 0 and frees
    // the ability if it hits zero. No effect on a ready ability or a non-positive amount.
    void reduce(int id, double seconds) {
        if (seconds <= 0.0) return;
        const auto it = m_timers.find(id);
        if (it == m_timers.end()) return;
        it->second.remaining -= seconds;
        if (it->second.remaining <= 0.0) m_timers.erase(it);
    }

    // Clear one ability's cooldown (make it ready immediately).
    void reset(int id) { m_timers.erase(id); }

    // Clear every cooldown.
    void clear() { m_timers.clear(); }

    // Number of abilities currently on cooldown.
    std::size_t activeCount() const { return m_timers.size(); }

private:
    struct Timer {
        double remaining = 0.0;
        double duration = 0.0;
    };
    std::unordered_map<int, Timer> m_timers;
};

} // namespace maz::game
