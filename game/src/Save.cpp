// Save/load for the ZOMBOID sim. Regenerates the (deterministic) tile map on
// load and restores every piece of mutable state, including the RNG stream, so a
// loaded game continues bit-identically to one that was never saved.
#include "zomboid/Save.hpp"

#include "zomboid/Sim.hpp"

namespace zb {

namespace {
constexpr uint32_t kMagic = 0x5642535Au; // "ZBSV" little-endian
constexpr uint32_t kVersion = 1;

void writeInv(Writer& w, const std::vector<InvSlot>& inv) {
    w.u32(static_cast<uint32_t>(inv.size()));
    for (const auto& s : inv) {
        w.str(s.id);
        w.i32(s.qty);
        w.i32(s.durab);
        w.boolean(s.hasDurab);
    }
}
} // namespace

std::vector<uint8_t> Sim::saveState() const {
    std::vector<uint8_t> out;
    Writer w(out);
    w.u32(kMagic);
    w.u32(kVersion);

    // RNG stream + meta clock.
    w.u64(m_rng.state());
    w.u64(m_rng.inc());
    w.f32(m_dayTime);
    w.i32(m_dayCount);
    w.i32(m_kills);
    w.f32(m_waveTimer);
    w.boolean(m_started);

    // Player.
    const Player& p = m_player;
    w.f32(p.pos.x);
    w.f32(p.pos.y);
    w.f32(p.dir);
    w.f32(p.speed);
    w.f32(p.radius);
    w.f32(p.health);
    w.f32(p.hunger);
    w.f32(p.thirst);
    w.f32(p.fatigue);
    w.f32(p.mood);
    w.f32(p.infection);
    w.boolean(p.infected);
    w.f32(p.attackCd);
    w.f32(p.attackImmune);
    w.f32(p.hurtFlash);
    w.i32(p.slots);
    w.str(p.weapon);
    w.boolean(p.flashlight);
    w.boolean(p.dead);
    writeInv(w, p.inv);

    // Zombies.
    w.u32(static_cast<uint32_t>(m_zombies.size()));
    for (const auto& z : m_zombies) {
        w.f32(z.pos.x);
        w.f32(z.pos.y);
        w.f32(z.dir);
        w.f32(z.health);
        w.f32(z.max);
        w.f32(z.speed);
        w.boolean(z.sprinter);
        w.f32(z.radius);
        w.boolean(z.dead);
        w.f32(z.knock);
        w.f32(z.kx);
        w.f32(z.ky);
        w.f32(z.groan);
        w.f32(z.wander);
        w.f32(z.wt);
        w.u8(static_cast<uint8_t>(z.tint));
    }

    // Bullets.
    w.u32(static_cast<uint32_t>(m_bullets.size()));
    for (const auto& b : m_bullets) {
        w.f32(b.pos.x);
        w.f32(b.pos.y);
        w.f32(b.vx);
        w.f32(b.vy);
        w.f32(b.dmg);
        w.f32(b.life);
    }

    // Corpses.
    w.u32(static_cast<uint32_t>(m_corpses.size()));
    for (const auto& c : m_corpses) {
        w.f32(c.pos.x);
        w.f32(c.pos.y);
        w.f32(c.t);
    }

    // Opened flags for the (deterministically regenerated) containers.
    w.u32(static_cast<uint32_t>(m_world.containers.size()));
    for (const auto& c : m_world.containers) w.boolean(c.opened);

    // Message log.
    w.u32(static_cast<uint32_t>(m_messages.size()));
    for (const auto& m : m_messages) {
        w.str(m.text);
        w.f32(m.life);
    }

    return out;
}

bool Sim::loadState(const std::vector<uint8_t>& data) {
    Reader r(data);
    if (r.u32() != kMagic) return false;
    if (r.u32() != kVersion) return false;

    // Parse into locals first; only commit to *this if the whole blob is valid.
    const uint64_t rngState = r.u64();
    const uint64_t rngInc = r.u64();

    // Rebuild the deterministic world; container opened flags are restored below.
    World world = generateWorld();

    Player p;
    float dayTime = r.f32();
    int dayCount = r.i32();
    int kills = r.i32();
    float waveTimer = r.f32();
    bool started = r.boolean();

    p.pos.x = r.f32();
    p.pos.y = r.f32();
    p.dir = r.f32();
    p.speed = r.f32();
    p.radius = r.f32();
    p.health = r.f32();
    p.hunger = r.f32();
    p.thirst = r.f32();
    p.fatigue = r.f32();
    p.mood = r.f32();
    p.infection = r.f32();
    p.infected = r.boolean();
    p.attackCd = r.f32();
    p.attackImmune = r.f32();
    p.hurtFlash = r.f32();
    p.slots = r.i32();
    p.weapon = r.str();
    p.flashlight = r.boolean();
    p.dead = r.boolean();
    {
        const uint32_t n = r.u32();
        if (!r.ok() || n > 4096) return false;
        p.inv.reserve(n);
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            InvSlot s;
            s.id = r.str();
            s.qty = r.i32();
            s.durab = r.i32();
            s.hasDurab = r.boolean();
            p.inv.push_back(std::move(s));
        }
    }

    std::vector<Zombie> zombies;
    {
        const uint32_t n = r.u32();
        if (!r.ok() || n > 100000) return false;
        zombies.reserve(n);
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            Zombie z;
            z.pos.x = r.f32();
            z.pos.y = r.f32();
            z.dir = r.f32();
            z.health = r.f32();
            z.max = r.f32();
            z.speed = r.f32();
            z.sprinter = r.boolean();
            z.radius = r.f32();
            z.dead = r.boolean();
            z.knock = r.f32();
            z.kx = r.f32();
            z.ky = r.f32();
            z.groan = r.f32();
            z.wander = r.f32();
            z.wt = r.f32();
            z.tint = static_cast<ZombieTint>(r.u8());
            zombies.push_back(z);
        }
    }

    std::vector<Bullet> bullets;
    {
        const uint32_t n = r.u32();
        if (!r.ok() || n > 100000) return false;
        bullets.reserve(n);
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            Bullet b;
            b.pos.x = r.f32();
            b.pos.y = r.f32();
            b.vx = r.f32();
            b.vy = r.f32();
            b.dmg = r.f32();
            b.life = r.f32();
            bullets.push_back(b);
        }
    }

    std::vector<Corpse> corpses;
    {
        const uint32_t n = r.u32();
        if (!r.ok() || n > 100000) return false;
        corpses.reserve(n);
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            Corpse c;
            c.pos.x = r.f32();
            c.pos.y = r.f32();
            c.t = r.f32();
            corpses.push_back(c);
        }
    }

    {
        const uint32_t n = r.u32();
        // Must match the regenerated world exactly, or the save is incompatible.
        if (!r.ok() || n != world.containers.size()) return false;
        for (uint32_t i = 0; i < n; i++) world.containers[i].opened = r.boolean();
    }

    std::vector<LogMessage> messages;
    {
        const uint32_t n = r.u32();
        if (!r.ok() || n > 4096) return false;
        messages.reserve(n);
        for (uint32_t i = 0; i < n && r.ok(); i++) {
            LogMessage m;
            m.text = r.str();
            m.life = r.f32();
            messages.push_back(std::move(m));
        }
    }

    if (!r.ok()) return false;

    // Commit.
    m_rng.setState(rngState, rngInc);
    m_world = std::move(world);
    m_player = std::move(p);
    m_zombies = std::move(zombies);
    m_bullets = std::move(bullets);
    m_corpses = std::move(corpses);
    m_messages = std::move(messages);
    m_dayTime = dayTime;
    m_dayCount = dayCount;
    m_kills = kills;
    m_waveTimer = waveTimer;
    m_started = started;
    return true;
}

} // namespace zb
