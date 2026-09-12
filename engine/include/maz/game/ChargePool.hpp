#pragma once

// maz::game charge pool — an ability with MULTIPLE stored uses that recharge one at a time, the "2 dashes /
// 3 grenades / 4 blinks" mechanic from Overwatch, MOBAs, and modern action games. It is the step up from a
// single on/off `CooldownManager` timer: a pool holds up to `maxCharges`, each `tryUse` spends one, and a
// shared recharge clock refills them back one by one. Spending a charge while others are still recharging
// does NOT reset the in-progress timer — the next charge keeps ticking in — which is exactly how these
// systems feel to play. Drives the pip counter and the radial fill on the ability button (`charges` /
// `fraction`). Deterministic, header-only, std-only. Godot ships Timer nodes but no charge abstraction.
namespace maz::game {

class ChargePool {
  public:
    ChargePool() = default;
    ChargePool(int maxCharges, double rechargeTime)
        : m_max(maxCharges < 0 ? 0 : maxCharges),
          m_recharge(rechargeTime < 0.0 ? 0.0 : rechargeTime),
          m_charges(m_max) {}

    int charges() const { return m_charges; }
    int max() const { return m_max; }
    bool isFull() const { return m_charges >= m_max; }
    bool canUse() const { return m_charges > 0; }

    // Spend one charge; returns false (and changes nothing) if none are available. When spent from a FULL
    // pool the recharge clock starts fresh; when spent mid-recharge the in-progress timer keeps its elapsed.
    bool tryUse() {
        if (m_charges <= 0) {
            return false;
        }
        --m_charges;
        return true;
    }

    // Advance the recharge clock by `dt` seconds, adding charges as it crosses each recharge interval (a big
    // dt can restore several). Non-positive dt and a full pool are no-ops; a zero recharge time refills fully.
    void tick(double dt) {
        if (dt <= 0.0 || m_charges >= m_max) {
            return;
        }
        if (m_recharge <= 0.0) {
            m_charges = m_max;
            m_elapsed = 0.0;
            return;
        }
        m_elapsed += dt;
        while (m_elapsed >= m_recharge && m_charges < m_max) {
            m_elapsed -= m_recharge;
            ++m_charges;
        }
        if (m_charges >= m_max) {
            m_elapsed = 0.0; // full: idle the clock (leftover time is not banked)
        }
    }

    // Progress in [0,1] toward the NEXT charge; 0 when the pool is full or the recharge time is zero. This
    // is the radial fill for the "recharging" pip.
    double fraction() const {
        if (m_charges >= m_max || m_recharge <= 0.0) {
            return 0.0;
        }
        return m_elapsed / m_recharge;
    }

    // Grant `n` charges immediately (clamped to the pool). Reaching full idles the recharge clock.
    void add(int n) {
        m_charges += n;
        if (m_charges > m_max) m_charges = m_max;
        if (m_charges < 0) m_charges = 0;
        if (m_charges >= m_max) m_elapsed = 0.0;
    }

    void refill() {
        m_charges = m_max;
        m_elapsed = 0.0;
    }
    void drain() { m_charges = 0; } // recharge clock keeps its elapsed and continues refilling on tick

  private:
    int m_max = 0;
    double m_recharge = 0.0;
    int m_charges = 0;
    double m_elapsed = 0.0;
};

} // namespace maz::game
