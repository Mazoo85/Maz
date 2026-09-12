#pragma once

#include <cstddef>
#include <vector>

// maz::game wave spawner — the enemy-wave director behind tower-defense, survival, and horde modes. Waves
// run in sequence: each wave waits a start delay, then releases its enemies one at a time on a fixed
// interval, and the NEXT wave begins only once the current wave is fully spawned AND every enemy is dead
// (the game reports kills). `update(dt)` returns the enemy-type ids to spawn this tick, `reportKilled`
// feeds the clear condition, and the spawner tracks the current wave, the alive count, and whether all
// waves are finished. Purely time- and event-driven, no rendering — the caller turns each returned id into
// an actual enemy. Godot ships no wave/spawner system — games hand-roll it every time — so this is a
// beyond-Godot gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

struct Wave {
    int enemyType = 0;      // id handed back for each spawn in this wave
    int count = 1;          // how many enemies this wave releases
    double interval = 1.0;  // seconds between spawns
    double startDelay = 0.0; // seconds to wait before the wave begins
};

class WaveSpawner {
public:
    WaveSpawner() = default;

    void addWave(const Wave& w) { m_waves.push_back(w); }
    void addWave(int enemyType, int count, double interval = 1.0, double startDelay = 0.0) {
        m_waves.push_back(Wave{enemyType, count, interval, startDelay});
    }

    std::size_t waveCount() const { return m_waves.size(); }
    const std::vector<Wave>& waves() const { return m_waves; }

    // Begin the first wave. No-op if there are no waves.
    void start() {
        if (m_waves.empty()) {
            m_phase = Phase::Done;
            return;
        }
        m_index = 0;
        enterWave();
    }

    // Advance by `dt` seconds and return the enemy-type ids to spawn this tick (usually 0 or 1, more if dt
    // spans several intervals). A non-positive dt returns nothing.
    std::vector<int> update(double dt) {
        std::vector<int> out;
        if (dt <= 0.0 || m_phase == Phase::Idle || m_phase == Phase::Done) return out;
        double remaining = dt;
        while (remaining > 0.0 && m_phase != Phase::Done && m_phase != Phase::Clearing) {
            const Wave& w = m_waves[static_cast<std::size_t>(m_index)];
            if (m_phase == Phase::Delay) {
                if (m_delay > remaining) {
                    m_delay -= remaining;
                    remaining = 0.0;
                } else {
                    remaining -= m_delay;
                    m_delay = 0.0;
                    m_phase = Phase::Spawning;
                    m_spawnTimer = 0.0;
                }
            } else { // Spawning
                if (m_spawned >= w.count) {
                    m_phase = Phase::Clearing;
                    maybeAdvance();
                    break;
                }
                if (m_spawnTimer <= 0.0) {
                    out.push_back(w.enemyType);
                    ++m_spawned;
                    m_spawnTimer = w.interval;
                    if (m_spawned >= w.count) {
                        m_phase = Phase::Clearing;
                        maybeAdvance();
                        break;
                    }
                } else if (m_spawnTimer > remaining) {
                    m_spawnTimer -= remaining;
                    remaining = 0.0;
                } else {
                    remaining -= m_spawnTimer;
                    m_spawnTimer = 0.0;
                }
            }
        }
        return out;
    }

    // Report `n` enemies of the current wave killed. When the wave is fully spawned and all its enemies are
    // dead, the next wave (or completion) begins.
    void reportKilled(int n = 1) {
        if (m_phase == Phase::Idle || m_phase == Phase::Done || n <= 0) return;
        m_killed += n;
        const int c = m_waves[static_cast<std::size_t>(m_index)].count;
        if (m_killed > c) m_killed = c;
        maybeAdvance();
    }

    int currentWave() const { return m_index; }         // 0-based; equals waveCount() when finished
    bool running() const { return m_phase != Phase::Idle && m_phase != Phase::Done; }
    bool finished() const { return m_phase == Phase::Done; }
    bool isSpawning() const { return m_phase == Phase::Spawning; }
    int spawnedInWave() const { return m_spawned; }
    int aliveCount() const { return m_spawned - m_killed; }
    // Enemies of the current wave not yet released.
    int remainingInWave() const {
        if (!running()) return 0;
        return m_waves[static_cast<std::size_t>(m_index)].count - m_spawned;
    }

    void reset() {
        m_index = 0;
        m_phase = Phase::Idle;
        m_delay = 0.0;
        m_spawnTimer = 0.0;
        m_spawned = 0;
        m_killed = 0;
    }

    void clear() {
        m_waves.clear();
        reset();
    }

private:
    enum class Phase { Idle, Delay, Spawning, Clearing, Done };

    void enterWave() {
        const Wave& w = m_waves[static_cast<std::size_t>(m_index)];
        m_spawned = 0;
        m_killed = 0;
        m_spawnTimer = 0.0;
        m_delay = w.startDelay;
        m_phase = Phase::Delay;
        if (w.count <= 0) {
            // Empty wave: nothing to spawn, clears at once.
            m_phase = Phase::Clearing;
            maybeAdvance();
        }
    }

    void maybeAdvance() {
        if (m_phase != Phase::Clearing) return;
        const int c = m_waves[static_cast<std::size_t>(m_index)].count;
        if (m_spawned >= c && m_killed >= c) {
            ++m_index;
            if (m_index >= static_cast<int>(m_waves.size())) {
                m_phase = Phase::Done;
            } else {
                enterWave();
            }
        }
    }

    std::vector<Wave> m_waves;
    int m_index = 0;
    Phase m_phase = Phase::Idle;
    double m_delay = 0.0;
    double m_spawnTimer = 0.0;
    int m_spawned = 0;
    int m_killed = 0;
};

} // namespace maz::game
