#pragma once

#include <algorithm> // std::min, std::max

// maz::game weapon magazine — the ammo model every shooter needs: a current MAGAZINE (rounds loaded and
// ready to fire), a RESERVE pool of spare ammo, and a timed RELOAD that refills the magazine from the
// reserve. Firing consumes one round; when the magazine runs dry (or on a manual "tactical" reload of a
// partial magazine) `reload` starts a timer during which the weapon cannot fire, and when it completes it
// transfers as many rounds as fit from the reserve into the magazine. This is deliberately distinct from
// the engine's other resource gates: `CooldownManager` is a binary ready/not-ready timer keyed by id, and
// `ChargePool` is N interchangeable charges on a shared auto-recharge clock — a magazine instead has TWO
// coupled pools (loaded vs reserve) and an explicit, interruptible reload step, which is what makes gun
// ammo feel like gun ammo. Godot ships no ammo abstraction; games hand-roll it every time. Reserve can be
// made effectively infinite for arcade weapons. Header-only, std-only, deterministic.
namespace maz::game {

class Magazine {
public:
    // A weapon with a magazine holding `magSize` rounds (starts fully loaded), `reserve` spare rounds, and
    // a reload taking `reloadTime` seconds. `magSize` is clamped to >= 1, the rest to >= 0.
    explicit Magazine(int magSize = 30, int reserve = 0, double reloadTime = 2.0)
        : m_magSize(magSize < 1 ? 1 : magSize),
          m_rounds(m_magSize),
          m_reserve(reserve < 0 ? 0 : reserve),
          m_reloadTime(reloadTime < 0.0 ? 0.0 : reloadTime) {}

    // ---- firing ----
    // True when a shot can be fired right now: a round is chambered and no reload is in progress.
    bool canFire() const { return m_rounds > 0 && !m_reloading; }
    bool isEmpty() const { return m_rounds == 0; }
    bool isFull() const { return m_rounds >= m_magSize; }

    // Fire one round if possible: consumes it and returns true; otherwise returns false (empty click, or
    // busy reloading) and changes nothing.
    bool tryFire() {
        if (!canFire()) return false;
        --m_rounds;
        return true;
    }

    // ---- reloading ----
    // Begin a reload. Succeeds (returns true) only when not already reloading, the magazine is not full,
    // and there is reserve ammo to draw from (infinite reserve always qualifies). A zero reload time
    // completes immediately in this call.
    bool reload() {
        if (m_reloading || isFull()) return false;
        if (!m_infinite && m_reserve <= 0) return false;
        if (m_reloadTime <= 0.0) {
            finishReload();
            return true;
        }
        m_reloading = true;
        m_reloadTimer = m_reloadTime;
        return true;
    }

    // Advance a reload in progress by `dt` seconds; completes (transferring ammo) when the timer elapses.
    // Non-positive dt and "not reloading" are no-ops.
    void tick(double dt) {
        if (!m_reloading || dt <= 0.0) return;
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0) finishReload();
    }

    // Abort an in-progress reload without loading any ammo (e.g. interrupted to fire or swap weapons).
    void cancelReload() {
        m_reloading = false;
        m_reloadTimer = 0.0;
    }

    bool isReloading() const { return m_reloading; }
    // Reload completion in [0,1] (0 at the start, 1 when done) — drives the reload bar; 0 when not reloading.
    double reloadProgress() const {
        if (!m_reloading || m_reloadTime <= 0.0) return 0.0;
        const double p = 1.0 - m_reloadTimer / m_reloadTime;
        return p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
    }

    // ---- reserve management ----
    void addReserve(int n) {
        if (n > 0) m_reserve += n;
    }
    // Make the reserve effectively infinite (arcade weapon): reloads always top the magazine to full and
    // never deplete the reserve. Pass false to return to a finite reserve.
    void setInfiniteReserve(bool infinite) { m_infinite = infinite; }
    bool hasInfiniteReserve() const { return m_infinite; }

    // ---- queries ----
    int count() const { return m_rounds; }        // rounds currently in the magazine
    int capacity() const { return m_magSize; }     // magazine size
    int reserve() const { return m_reserve; }      // spare rounds (meaningless when infinite)
    // Total ammo the weapon can still fire before running completely dry (loaded + reserve); a large
    // sentinel when the reserve is infinite.
    int total() const { return m_infinite ? kInfiniteTotal : m_rounds + m_reserve; }
    // Magazine fill in [0,1] — drives the ammo pip row / counter.
    float fraction() const { return static_cast<float>(m_rounds) / static_cast<float>(m_magSize); }

    static constexpr int kInfiniteTotal = 1 << 30;

private:
    void finishReload() {
        const int space = m_magSize - m_rounds;
        const int loaded = m_infinite ? space : std::min(space, m_reserve);
        m_rounds += loaded;
        if (!m_infinite) m_reserve -= loaded;
        m_reloading = false;
        m_reloadTimer = 0.0;
    }

    int m_magSize;
    int m_rounds;
    int m_reserve;
    double m_reloadTime;
    double m_reloadTimer = 0.0;
    bool m_reloading = false;
    bool m_infinite = false;
};

} // namespace maz::game
