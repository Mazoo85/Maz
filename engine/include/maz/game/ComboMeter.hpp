#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::game combo meter — the score-chain tracker behind arcade multipliers, fighting-game combo counters,
// and rhythm-game streaks. Each `hit` bumps the combo count and refreshes a countdown; if the countdown
// runs out (or the game calls `breakCombo` on a miss), the chain resets to zero. The current count maps
// through configurable tiers to a score MULTIPLIER, and `scoreFor` applies it to a base point value. The
// best combo reached is remembered for an end-of-run "max combo" stat. Purely time-driven, no rendering.
// Godot ships no combo/score-chain system — games hand-roll it every time — so this is a beyond-Godot
// gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

struct ComboTier {
    int minCount = 0;         // combo count at or above which...
    double multiplier = 1.0;  // ...this multiplier applies
};

class ComboMeter {
public:
    // `timeout`: seconds a combo survives without a hit before it breaks (clamped to >= 0). The default
    // tiers give 1x below 5, 1.5x at 5, 2x at 10, 3x at 25, 4x at 50.
    explicit ComboMeter(double timeout = 2.0) : m_timeout(timeout < 0.0 ? 0.0 : timeout) {
        m_tiers = {{0, 1.0}, {5, 1.5}, {10, 2.0}, {25, 3.0}, {50, 4.0}};
    }

    // Replace the multiplier tiers (sorted ascending internally). An empty set falls back to a flat 1x.
    void setTiers(std::vector<ComboTier> tiers) {
        m_tiers = std::move(tiers);
        std::sort(m_tiers.begin(), m_tiers.end(),
                  [](const ComboTier& a, const ComboTier& b) { return a.minCount < b.minCount; });
    }

    // Register `n` hits: grows the combo and refreshes the countdown. Non-positive n is ignored.
    void hit(int n = 1) {
        if (n <= 0) return;
        m_count += n;
        if (m_count > m_max) m_max = m_count;
        m_timer = m_timeout;
    }

    // Advance the countdown by `dt`; when it expires the combo breaks (resets to 0). No-op if inactive or
    // dt is non-positive.
    void update(double dt) {
        if (m_count <= 0 || dt <= 0.0) return;
        m_timer -= dt;
        if (m_timer <= 0.0) {
            m_count = 0;
            m_timer = 0.0;
        }
    }

    // Force the combo to break (a missed note / dropped chain).
    void breakCombo() {
        m_count = 0;
        m_timer = 0.0;
    }

    int count() const { return m_count; }
    int maxCount() const { return m_max; }
    bool active() const { return m_count > 0; }
    double timeRemaining() const { return m_timer; }

    // Score multiplier for the current combo (the highest tier whose minCount <= count; 1.0 otherwise).
    double multiplier() const {
        double m = 1.0;
        for (const ComboTier& t : m_tiers)
            if (m_count >= t.minCount) m = t.multiplier;
        return m;
    }

    // `base` points scaled by the current multiplier, rounded.
    long long scoreFor(long long base) const {
        return static_cast<long long>(std::llround(static_cast<double>(base) * multiplier()));
    }

    // Clear the current combo (keeps the max-combo record).
    void reset() {
        m_count = 0;
        m_timer = 0.0;
    }

    // Clear everything including the max-combo record.
    void resetAll() {
        m_count = 0;
        m_timer = 0.0;
        m_max = 0;
    }

private:
    double m_timeout;
    double m_timer = 0.0;
    int m_count = 0;
    int m_max = 0;
    std::vector<ComboTier> m_tiers;
};

} // namespace maz::game
