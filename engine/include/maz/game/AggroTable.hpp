#pragma once

// maz::game aggro / threat table — the bookkeeping behind "which target does this enemy attack?" Every
// action a would-be target takes against the owner (damage dealt, healing done on an ally, a taunt) adds
// `threat`; the enemy attacks whoever holds the most. Two touches make it feel right instead of naive:
// sticky aggro (a challenger must exceed the current target's threat by a switch threshold — default 110%
// — before it steals aggro, so the enemy doesn't flip-flop every hit) and a taunt that forces the target
// immediately and tops up the taunter's threat so it holds. Optional per-second decay bleeds threat back
// toward zero for out-of-combat forgetting. Keyed by an int source id (an entity handle). Godot ships no
// aggro/threat system — enemy targeting is hand-rolled every game — so this is a beyond-Godot gameplay
// utility. Header-only, std-only, deterministic.
#include <cstddef>
#include <unordered_map>

namespace maz::game {

class AggroTable {
public:
    static constexpr int kNone = -1;

    AggroTable() = default;

    // Add `amount` threat from `sourceId` (negative reduces; threat never drops below 0). Re-evaluates the
    // current target with hysteresis, so a small hit won't yank aggro off the established target.
    void addThreat(int sourceId, double amount) {
        double& t = m_threat[sourceId];
        t += amount;
        if (t < 0.0) t = 0.0;
        reevaluate();
    }

    // Set a source's threat to an exact value (clamped to >= 0).
    void setThreat(int sourceId, double value) {
        m_threat[sourceId] = value < 0.0 ? 0.0 : value;
        reevaluate();
    }

    // Force `sourceId` to become the current target right now and top up its threat to at least the current
    // highest, so it stays the target through the next few evaluations (the classic taunt).
    void taunt(int sourceId) {
        const double top = topThreat();
        double& t = m_threat[sourceId];
        if (t < top) t = top;
        m_current = sourceId; // forced, bypassing the switch threshold
    }

    // Remove a source entirely (it died or fled). If it was the current target, a new one is chosen.
    void removeSource(int sourceId) {
        if (m_threat.erase(sourceId) > 0 && m_current == sourceId) {
            m_current = kNone;
            reevaluate();
        }
    }

    // Bleed threat toward 0 at the configured decay rate. Uniform subtraction preserves the ordering of the
    // survivors; entries that reach 0 stay at 0 (call removeSource to drop them).
    void update(double dt) {
        if (dt <= 0.0 || m_decayRate <= 0.0) return;
        const double drop = m_decayRate * dt;
        for (auto& kv : m_threat) {
            kv.second -= drop;
            if (kv.second < 0.0) kv.second = 0.0;
        }
        reevaluate();
    }

    // The source the enemy should attack, or kNone if the table is empty.
    int currentTarget() const { return m_current; }
    // The raw highest-threat source ignoring hysteresis (ties broken by lower id), or kNone if empty.
    int topThreatId() const {
        int best = kNone;
        double bestVal = 0.0;
        for (const auto& kv : m_threat) {
            if (best == kNone || kv.second > bestVal || (kv.second == bestVal && kv.first < best)) {
                best = kv.first;
                bestVal = kv.second;
            }
        }
        return best;
    }
    double topThreat() const {
        const int id = topThreatId();
        return id == kNone ? 0.0 : threatOf(id);
    }

    double threatOf(int sourceId) const {
        const auto it = m_threat.find(sourceId);
        return it == m_threat.end() ? 0.0 : it->second;
    }
    bool has(int sourceId) const { return m_threat.find(sourceId) != m_threat.end(); }
    std::size_t sourceCount() const { return m_threat.size(); }
    bool isEmpty() const { return m_threat.empty(); }

    void clear() {
        m_threat.clear();
        m_current = kNone;
    }

    // A challenger must exceed the current target's threat by this factor to steal aggro (>= 1; default
    // 1.1 = 110%). 1.0 means any lead flips the target immediately.
    void setSwitchThreshold(double factor) { m_switchThreshold = factor < 1.0 ? 1.0 : factor; }
    double switchThreshold() const { return m_switchThreshold; }
    // Threat bled per second by update() (0 disables decay).
    void setDecayRate(double perSecond) { m_decayRate = perSecond < 0.0 ? 0.0 : perSecond; }
    double decayRate() const { return m_decayRate; }

private:
    // Pick the current target: keep the established one unless a challenger beats it by the switch factor.
    void reevaluate() {
        const int top = topThreatId();
        if (top == kNone) {
            m_current = kNone;
            return;
        }
        if (m_current == kNone || m_threat.find(m_current) == m_threat.end()) {
            m_current = top;
            return;
        }
        if (top != m_current) {
            const double topVal = threatOf(top);
            const double curVal = threatOf(m_current);
            if (topVal > curVal * m_switchThreshold) m_current = top;
        }
    }

    std::unordered_map<int, double> m_threat;
    int m_current = kNone;
    double m_switchThreshold = 1.1;
    double m_decayRate = 0.0;
};

} // namespace maz::game
