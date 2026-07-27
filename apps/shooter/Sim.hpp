#pragma once

// DEAD SECTOR — gameplay simulation (engine-independent).
//
// This is the pure, deterministic core of the top-down zombie shooter: player, zombies, bullets,
// particles, pickups, wave pacing, and all collision/spawn rules. It has NO dependency on the
// renderer, the window, Vulkan, or GLM — it takes an Input each fixed step and advances state.
// That keeps it unit-testable with no GPU/display (see tests/unit_shooter.cpp) and lets the app
// layer (apps/shooter/main.cpp) do nothing but translate this state into sprite draws.
//
// Units are world pixels; +X is right, +Y is down (matching the engine's top-left sprite origin).
// Randomness comes from a small seeded xorshift RNG so a given seed + input stream is reproducible.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace shooter {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Deterministic 32-bit xorshift RNG. Seeded, so simulation runs (and tests) are reproducible.
class Rng {
public:
    explicit Rng(uint32_t seed = 0x1234567u) : state_(seed ? seed : 0x1234567u) {}
    void seed(uint32_t s) { state_ = s ? s : 0x1234567u; }

    uint32_t nextU32() {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }
    // Uniform float in [0, 1).
    float nextUnit() { return static_cast<float>(nextU32() >> 8) / static_cast<float>(1u << 24); }
    // Uniform float in [lo, hi).
    float range(float lo, float hi) { return lo + (hi - lo) * nextUnit(); }

private:
    uint32_t state_;
};

enum class ZombieType : uint8_t { Walker, Runner, Brute };

struct Zombie {
    Vec2 pos;
    float radius = 15.0f;
    float speed = 60.0f;
    float hp = 3.0f;
    float maxHp = 3.0f;
    float damage = 8.0f;
    int score = 10;
    ZombieType type = ZombieType::Walker;
    float hitFlash = 0.0f; // >0 for a few frames after being shot (drawn white)
    float wobble = 0.0f;   // shambling gait phase
    bool dead = false;
};

struct Bullet {
    Vec2 pos;
    Vec2 vel;
    float life = 0.9f;
    float radius = 4.0f;
    float damage = 2.0f;
    bool dead = false;
};

struct Particle {
    Vec2 pos;
    Vec2 vel;
    float life = 0.5f;
    float maxLife = 0.5f;
    float radius = 2.5f;
    uint8_t r = 255, g = 255, b = 255;
};

struct Pickup {
    Vec2 pos;
    float life = 12.0f; // despawns if not collected
    float bob = 0.0f;
};

enum class GameState : uint8_t { Playing, Dead };

// One fixed-step's worth of intent, produced by the app from keyboard/mouse (or by a test).
struct Input {
    Vec2 move;             // desired move direction; magnitude in [0,1] scales speed
    Vec2 aim;              // aim direction (need not be normalized)
    bool aiming = false;   // whether `aim` is meaningful this step
    bool firing = false;   // hold to auto-fire
    bool restart = false;  // request restart from the death screen
};

class Sim {
public:
    static constexpr float kWorld = 2200.0f; // arena half-extent; player clamps to [-kWorld, kWorld]

    explicit Sim(uint32_t seed = 0x1234567u);

    // Restart a fresh run (wave 1, full health). Called on construction and on restart.
    void reset();

    // Advance the whole simulation by one fixed step. `dt` is seconds (e.g. 1/60).
    void update(float dt, const Input& in);

    // --- read-only state for rendering / tests ---
    GameState state() const { return state_; }
    Vec2 playerPos() const { return playerPos_; }
    float playerAngle() const { return playerAngle_; }
    float playerHp() const { return playerHp_; }
    float playerMaxHp() const { return playerMaxHp_; }
    float playerRadius() const { return playerRadius_; }
    float playerInvuln() const { return invuln_; }
    int wave() const { return wave_; }
    int kills() const { return kills_; }
    int score() const { return score_; }
    int enemiesRemaining() const { return static_cast<int>(zombies_.size()) + zombiesToSpawn_; }

    const std::vector<Zombie>& zombies() const { return zombies_; }
    const std::vector<Bullet>& bullets() const { return bullets_; }
    const std::vector<Particle>& particles() const { return particles_; }
    const std::vector<Pickup>& pickups() const { return pickups_; }

private:
    void startWave(int n);
    void spawnZombie();
    void killZombie(std::size_t index);
    void spawnBlood(Vec2 p, float dir, int count);
    void spawnMuzzle(Vec2 p, float dir);

    Rng rng_;
    GameState state_ = GameState::Playing;

    Vec2 playerPos_;
    float playerAngle_ = 0.0f;
    float playerHp_ = 100.0f;
    float playerMaxHp_ = 100.0f;
    float playerRadius_ = 16.0f;
    float playerSpeed_ = 250.0f;
    float fireCooldown_ = 0.0f;
    float fireRate_ = 0.14f;
    float invuln_ = 0.0f;

    int wave_ = 0;
    int kills_ = 0;
    int score_ = 0;
    int zombiesToSpawn_ = 0;
    float spawnTimer_ = 0.0f;

    std::vector<Zombie> zombies_;
    std::vector<Bullet> bullets_;
    std::vector<Particle> particles_;
    std::vector<Pickup> pickups_;
};

} // namespace shooter
