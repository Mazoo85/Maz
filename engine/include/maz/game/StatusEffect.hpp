#pragma once

#include <cstddef>
#include <vector>

// maz::game status-effect system — the timed buff / debuff layer behind poison, regeneration, haste, and
// burning. Each effect has a type id, a remaining duration, a stack count, and an optional periodic
// interval; `update(dt)` counts every effect down, fires a "tick" each time an effect's interval elapses
// (the moment a damage-over-time or heal-over-time effect should act), and drops effects whose duration
// runs out. The ticks are reported back with the effect's current stack count, so the game multiplies
// poison damage by stacks without tracking timers itself. Re-applying an effect follows a stack policy —
// refresh the timer, add a stack (up to a cap), or keep the existing one. This is DISTINCT from the stat
// modifier stack (untimed, in Stat.hpp) and the ability cooldown bank (one gating timer per ability, in
// Cooldown.hpp): status effects are the durational on-entity effects that expire and pulse. Godot ships no
// status-effect system — games hand-roll it every time — so this is a beyond-Godot gameplay utility.
// Header-only, std-only, deterministic.
namespace maz::game {

enum class StackMode {
    Refresh, // re-apply resets duration and replaces the stack count
    Add,     // re-apply adds stacks (up to maxStacks) and refreshes duration
    Keep,    // re-apply is ignored while the effect is already active
};

struct StatusEffect {
    int id = -1;           // effect type id (poison, haste, ...)
    double remaining = 0.0; // seconds left
    int stacks = 1;         // current stack count
    double interval = 0.0;  // seconds between ticks; <= 0 means no periodic ticks
    double sinceTick = 0.0; // internal: time accumulated toward the next tick
};

// One update()'s worth of ticks for a single effect.
struct StatusTick {
    int id = -1;
    int stacks = 0; // stack count at tick time (multiply per-stack damage/heal by this)
    int ticks = 0;  // how many interval boundaries elapsed this update
};

class StatusEffectSystem {
public:
    StatusEffectSystem() = default;

    // Apply effect `id` for `duration` seconds. `interval > 0` makes it tick that often. On re-apply the
    // StackMode decides how it combines; `maxStacks` caps StackMode::Add. Non-positive duration or a
    // zero/negative stack count is ignored. Returns the effect's resulting stack count (0 if not applied).
    int apply(int id, double duration, int stacks = 1, double interval = 0.0,
              StackMode mode = StackMode::Refresh, int maxStacks = 99) {
        if (id < 0 || duration <= 0.0 || stacks < 1) return 0;
        StatusEffect* e = find(id);
        if (e == nullptr) {
            m_effects.push_back(StatusEffect{id, duration, stacks, interval, 0.0});
            return stacks;
        }
        switch (mode) {
            case StackMode::Refresh:
                e->remaining = duration;
                e->stacks = stacks;
                e->interval = interval;
                break;
            case StackMode::Add:
                e->stacks += stacks;
                if (e->stacks > maxStacks) e->stacks = maxStacks;
                e->remaining = duration;
                e->interval = interval;
                break;
            case StackMode::Keep:
                break; // leave the running effect untouched
        }
        return e->stacks;
    }

    // Advance every effect by `dt` seconds. Returns a StatusTick for each effect that ticked at least once
    // this update. Effects whose duration reaches zero are removed (after reporting any ticks). A
    // non-positive dt is a no-op.
    std::vector<StatusTick> update(double dt) {
        std::vector<StatusTick> out;
        if (dt <= 0.0) return out;
        for (StatusEffect& e : m_effects) {
            int ticks = 0;
            if (e.interval > 0.0) {
                e.sinceTick += dt;
                while (e.sinceTick >= e.interval) {
                    e.sinceTick -= e.interval;
                    ++ticks;
                }
            }
            e.remaining -= dt;
            if (ticks > 0) out.push_back(StatusTick{e.id, e.stacks, ticks});
        }
        for (std::size_t i = m_effects.size(); i-- > 0;) {
            if (m_effects[i].remaining <= 0.0) m_effects.erase(m_effects.begin() + static_cast<long>(i));
        }
        return out;
    }

    bool has(int id) const { return find(id) != nullptr; }
    int stacksOf(int id) const {
        const StatusEffect* e = find(id);
        return e == nullptr ? 0 : e->stacks;
    }
    double remainingOf(int id) const {
        const StatusEffect* e = find(id);
        return e == nullptr ? 0.0 : e->remaining;
    }

    // Remove an effect outright (a cleanse / dispel). Returns true if it was present.
    bool remove(int id) {
        for (std::size_t i = 0; i < m_effects.size(); ++i) {
            if (m_effects[i].id == id) {
                m_effects.erase(m_effects.begin() + static_cast<long>(i));
                return true;
            }
        }
        return false;
    }

    void clear() { m_effects.clear(); }
    std::size_t activeCount() const { return m_effects.size(); }
    const std::vector<StatusEffect>& effects() const { return m_effects; }

private:
    StatusEffect* find(int id) {
        for (StatusEffect& e : m_effects)
            if (e.id == id) return &e;
        return nullptr;
    }
    const StatusEffect* find(int id) const {
        for (const StatusEffect& e : m_effects)
            if (e.id == id) return &e;
        return nullptr;
    }

    std::vector<StatusEffect> m_effects;
};

} // namespace maz::game
