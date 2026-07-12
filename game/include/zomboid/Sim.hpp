// ZOMBOID: ANCHORAGE — deterministic gameplay simulation (ported from js/game.js).
//
// Engine-agnostic: no rendering, audio, or platform code. Drive it with a fixed
// timestep and an Input struct; read the public state to render or test. This is
// the native Maz port of the browser reference game's simulation core.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "zomboid/Items.hpp"
#include "zomboid/Math.hpp"
#include "zomboid/Rng.hpp"
#include "zomboid/World.hpp"

namespace zb {

constexpr float kStep = 1.0f / 60.0f;   // fixed simulation timestep
constexpr float kTimeScale = 28.0f;     // game-minutes per real second
constexpr float kDayLen = 24.0f * 60.0f; // minutes in a day

// One inventory stack.
struct InvSlot {
    std::string id;
    int qty = 1;
    int durab = kInfiniteDurab;
    bool hasDurab = false;
};

struct Player {
    Vec2 pos{0, 0};
    float dir = 0.0f;
    float speed = 4.6f;
    float radius = 0.35f;

    float health = 100.0f;
    float hunger = 0.0f;
    float thirst = 0.0f;
    float fatigue = 0.0f;
    float mood = 70.0f;
    float infection = 0.0f;
    bool infected = false;

    float attackCd = 0.0f;
    float attackImmune = 0.0f;
    float hurtFlash = 0.0f;

    std::vector<InvSlot> inv;
    int slots = 8;
    std::string weapon = "fists";
    bool flashlight = false;
    bool dead = false;
};

enum class ZombieTint { GreenA, GreenB, Sprinter };

struct Zombie {
    Vec2 pos{0, 0};
    float dir = 0.0f;
    float health = 0.0f;
    float max = 0.0f;
    float speed = 0.0f;
    bool sprinter = false;
    float radius = 0.35f;
    bool dead = false;
    float knock = 0.0f;
    float kx = 0.0f, ky = 0.0f;
    float groan = 0.0f;
    float wander = 0.0f;
    float wt = 0.0f;
    ZombieTint tint = ZombieTint::GreenA;
};

struct Bullet {
    Vec2 pos{0, 0};
    float vx = 0.0f, vy = 0.0f;
    float dmg = 0.0f;
    float life = 0.0f;
};

struct Corpse {
    Vec2 pos{0, 0};
    float t = 0.0f;
};

struct LogMessage {
    std::string text;
    float life = 6.0f;
};

// Per-tick player intent. Movement axes are in [-1,1]; aim is a world-tile point.
struct Input {
    float moveX = 0.0f;
    float moveY = 0.0f;
    Vec2 aim{0, 0};          // world-tile coordinates the player faces
    bool attackHeld = false; // held fire (matches mouse.down)
    bool attackPressed = false; // edge fire (matches click/space)
    bool interact = false;   // E — loot/interact
    bool reload = false;     // R
    int useSlot = -1;        // 0..7, or -1
    bool toggleFlashlight = false; // F
};

class Sim {
public:
    explicit Sim(uint64_t seed = 1);

    // (Re)start a fresh game. Regenerates the world and starting kit.
    void newGame();

    // Advance one fixed step (kStep seconds) with the given input.
    void step(const Input& in);

    // --- read-only accessors for rendering / tests ---
    const World& world() const { return m_world; }
    const Player& player() const { return m_player; }
    const std::vector<Zombie>& zombies() const { return m_zombies; }
    const std::vector<Bullet>& bullets() const { return m_bullets; }
    const std::vector<Corpse>& corpses() const { return m_corpses; }
    const std::vector<LogMessage>& messages() const { return m_messages; }

    int kills() const { return m_kills; }
    int day() const { return m_dayCount; }
    float dayTime() const { return m_dayTime; }
    bool isNight() const { return m_dayTime < 6 * 60 || m_dayTime > 21 * 60; }
    int aliveZombies() const;
    // 0 = full day, ~0.78 = deep night (drives the render darkness overlay).
    float darknessAlpha() const;

    // --- save / load ---
    // Serialize the full mutable game state (RNG stream, clock, player+inventory,
    // zombies, bullets, corpses, opened containers) to a versioned byte blob.
    std::vector<uint8_t> saveState() const;
    // Restore from a blob produced by saveState(). Returns false (leaving the sim
    // unchanged) if the data is corrupt, truncated, or a version/shape mismatch.
    bool loadState(const std::vector<uint8_t>& data);

    // Actions (public so a UI or test can invoke them directly).
    bool addItem(const std::string& id, int qty);
    void useSlot(int idx);
    void attack();
    bool tryInteract();
    void reloadWeapon();

private:
    void spawnZombie(bool downtown);
    void updatePlayer(const Input& in, float dt);
    void updateZombies(float dt);
    void updateBullets(float dt);
    void updateWorld(float dt);
    void moveEntity(Vec2& pos, float radius, float mvx, float mvy);
    bool collides(float x, float y, float r) const;
    void damageZombie(Zombie& z, float dmg, float ang);
    void killZombie(Zombie& z);
    void hurtPlayer(float dmg);
    void removeOne(int idx);
    void pushMsg(const std::string& text);
    InvSlot* findSlot(const std::string& id);

    Rng m_rng;
    World m_world;
    Player m_player;
    std::vector<Zombie> m_zombies;
    std::vector<Bullet> m_bullets;
    std::vector<Corpse> m_corpses;
    std::vector<LogMessage> m_messages;

    float m_dayTime = 8 * 60; // minutes; start 08:00
    int m_dayCount = 1;
    int m_kills = 0;
    float m_waveTimer = 0.0f;
    bool m_started = false;
};

} // namespace zb
