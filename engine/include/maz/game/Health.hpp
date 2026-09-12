#pragma once

// maz::game health component — the hit-point pool behind health bars, death, and "you can't hurt me right
// now" invulnerability frames. It holds current / max HP, applies `takeDamage` and `heal` (both clamped, so
// HP never goes below 0 or above max), and reports the fraction for a health bar plus `isDead` / `isFull`.
// After a hit it can grant a window of invulnerability (the classic post-damage i-frames) during which
// further damage is ignored, and `update(dt)` ticks that window down and applies optional passive
// regeneration. Pairs directly with the M486 damage resolver (feed its result into `takeDamage`). Godot
// ships no health system — games hand-roll it every time — so this is a beyond-Godot gameplay utility.
// Header-only, std-only, deterministic.
namespace maz::game {

class Health {
public:
    // Start at full `maxHp` (clamped to >= 1).
    explicit Health(double maxHp = 100.0) : m_max(maxHp < 1.0 ? 1.0 : maxHp), m_current(m_max) {}

    double current() const { return m_current; }
    double max() const { return m_max; }
    double fraction() const { return m_max > 0.0 ? m_current / m_max : 0.0; }
    bool isDead() const { return m_current <= 0.0; }
    bool isFull() const { return m_current >= m_max; }
    bool isInvulnerable() const { return m_invuln > 0.0; }
    double invulnerabilityRemaining() const { return m_invuln; }

    // Apply `amount` damage. Returns the HP actually lost (0 if dead, invulnerable, or amount <= 0). A hit
    // that connects starts the on-hit invulnerability window (if one is configured).
    double takeDamage(double amount) {
        if (amount <= 0.0 || isDead() || isInvulnerable()) return 0.0;
        const double applied = amount < m_current ? amount : m_current;
        m_current -= applied;
        if (m_current < 0.0) m_current = 0.0;
        if (m_onHitInvuln > 0.0) m_invuln = m_onHitInvuln;
        return applied;
    }

    // Restore `amount` HP (capped at max). Returns the HP actually gained (0 if dead or amount <= 0).
    double heal(double amount) {
        if (amount <= 0.0 || isDead()) return 0.0;
        const double room = m_max - m_current;
        const double gained = amount < room ? amount : room;
        m_current += gained;
        if (m_current > m_max) m_current = m_max;
        return gained;
    }

    // Advance the invulnerability window and apply passive regen.
    void update(double dt) {
        if (dt <= 0.0) return;
        if (m_invuln > 0.0) {
            m_invuln -= dt;
            if (m_invuln < 0.0) m_invuln = 0.0;
        }
        if (m_regen > 0.0 && !isDead() && !isFull()) heal(m_regen * dt);
    }

    // Set the max HP. `healToFull` refills to the new max; otherwise current is clamped down if needed.
    void setMax(double m, bool healToFull = false) {
        m_max = m < 1.0 ? 1.0 : m;
        if (healToFull) m_current = m_max;
        else if (m_current > m_max) m_current = m_max;
    }
    // Set current HP directly (clamped to [0, max]).
    void setCurrent(double c) { m_current = c < 0.0 ? 0.0 : (c > m_max ? m_max : c); }

    void kill() { m_current = 0.0; }
    // Revive to `hp` HP (default -1 = full); clears any invulnerability.
    void revive(double hp = -1.0) {
        m_current = hp < 0.0 ? m_max : (hp > m_max ? m_max : hp);
        m_invuln = 0.0;
    }

    // Seconds of invulnerability granted automatically after each connecting hit (0 disables).
    void setOnHitInvulnerability(double seconds) { m_onHitInvuln = seconds < 0.0 ? 0.0 : seconds; }
    // Grant an invulnerability window right now (keeps the longer of current / new).
    void grantInvulnerability(double seconds) {
        if (seconds > m_invuln) m_invuln = seconds;
    }
    // Passive HP regenerated per second (0 disables).
    void setRegen(double perSecond) { m_regen = perSecond < 0.0 ? 0.0 : perSecond; }

private:
    double m_max;
    double m_current;
    double m_invuln = 0.0;      // remaining invulnerability window
    double m_onHitInvuln = 0.0; // window granted per hit
    double m_regen = 0.0;       // HP per second
};

} // namespace maz::game
