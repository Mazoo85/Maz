#pragma once

// maz::game poise / posture (super-armor) — the stagger mechanic of Dark Souls, Sekiro, Elden Ring and
// most modern action games: a SECOND resource, separate from health, that governs whether a hit
// INTERRUPTS you. Each hit drains poise; while you have poise left you shrug off the stagger and keep your
// action (super-armor); when poise hits zero it BREAKS — you are staggered (open to a critical/riposte)
// for a fixed time, then recover. Poise regenerates on its own, but only after a brief lull since the last
// hit, so sustained pressure grinds it down (Sekiro's posture) while trading blows slowly lets it recover.
//
// This is deliberately distinct from `game::Health` (hit points, death, post-hit invulnerability frames,
// passive HP regen) and `game::StatusEffect` (timed buffs/debuffs): poise is not damage and never kills —
// it decides interruptibility, which is a separate axis every action game tracks and Godot ships nothing
// for. Purely time-driven, no rendering. Header-only, std-only, deterministic.
namespace maz::game {

struct PoiseParams {
    float maxPoise = 100.0f;   // full poise pool
    float staggerTime = 1.5f;  // seconds spent broken/staggered before recovering
    float regenDelay = 2.0f;   // seconds after the last hit before poise starts regenerating
    float regenRate = 40.0f;   // poise restored per second once regen resumes
};

class Poise {
public:
    Poise() = default;
    explicit Poise(const PoiseParams& p)
        : m_p(p), m_poise(p.maxPoise <= 0.0f ? 1.0f : p.maxPoise) {
        if (m_p.maxPoise <= 0.0f) m_p.maxPoise = 1.0f;
    }

    // Absorb a stagger hit. Returns true only on the frame poise BREAKS (so the caller plays the stagger /
    // opens a riposte window). While already broken the hit is absorbed (poise stays at zero, no re-break).
    // Any hit resets the regen delay, so sustained pressure prevents recovery.
    bool takeStagger(float amount) {
        if (m_broken) return false;         // already staggered — further hits do not re-trigger
        m_regenLeft = m_p.regenDelay;        // pressure resets the regen lull
        if (amount <= 0.0f) return false;
        m_poise -= amount;
        if (m_poise <= 0.0f) {
            m_poise = 0.0f;
            m_broken = true;
            m_brokenLeft = m_p.staggerTime;
            return true;
        }
        return false;
    }

    // Advance timers: count down a stagger (recovering to full poise when it ends), or — once the post-hit
    // lull has elapsed — regenerate poise toward the max. Non-positive dt is a no-op.
    void update(float dt) {
        if (dt <= 0.0f) return;
        if (m_broken) {
            m_brokenLeft -= dt;
            if (m_brokenLeft <= 0.0f) {
                m_broken = false;
                m_brokenLeft = 0.0f;
                m_poise = m_p.maxPoise; // stand back up at full poise
                m_regenLeft = 0.0f;
            }
            return;
        }
        if (m_regenLeft > 0.0f) {
            m_regenLeft -= dt;
            if (m_regenLeft < 0.0f) m_regenLeft = 0.0f;
        } else if (m_poise < m_p.maxPoise) {
            m_poise += m_p.regenRate * dt;
            if (m_poise > m_p.maxPoise) m_poise = m_p.maxPoise;
        }
    }

    void reset() {
        m_poise = m_p.maxPoise;
        m_broken = false;
        m_brokenLeft = 0.0f;
        m_regenLeft = 0.0f;
    }

    float poise() const { return m_poise; }
    float max() const { return m_p.maxPoise; }
    float fraction() const { return m_poise / m_p.maxPoise; } // drives the posture bar
    bool isBroken() const { return m_broken; }
    bool isFull() const { return m_poise >= m_p.maxPoise; }
    float staggerRemaining() const { return m_broken ? m_brokenLeft : 0.0f; }
    const PoiseParams& params() const { return m_p; }

private:
    PoiseParams m_p{};
    float m_poise = 100.0f;
    float m_brokenLeft = 0.0f;
    float m_regenLeft = 0.0f;
    bool m_broken = false;
};

} // namespace maz::game
