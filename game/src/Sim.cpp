#include "zomboid/Sim.hpp"

#include <algorithm>
#include <cmath>

namespace zb {

Sim::Sim(uint64_t seed) : m_rng(seed) {}

void Sim::newGame() {
    m_world = generateWorld();

    m_player = Player{};
    m_player.pos = Vec2{m_world.spawn.x + 0.5f, m_world.spawn.y + 0.5f};
    m_player.speed = 4.6f;
    m_player.radius = 0.35f;
    m_player.health = 100.0f;
    m_player.mood = 70.0f;
    m_player.slots = 8;
    m_player.weapon = "fists";

    // Starting kit.
    addItem("branch", 1);
    addItem("water", 1);
    addItem("granola", 1);
    addItem("bandage", 2);
    addItem("map", 1);
    m_player.weapon = "branch";

    m_zombies.clear();
    m_bullets.clear();
    m_corpses.clear();
    m_messages.clear();

    m_dayTime = 8 * 60;
    m_dayCount = 1;
    m_kills = 0;
    m_waveTimer = 0.0f;

    pushMsg("Welcome to ANCHORAGE. Survive.");
    pushMsg("The dead walk on 5th Avenue...");

    // Initial horde near downtown.
    for (int i = 0; i < 40; i++) spawnZombie(true);
    m_started = true;
}

// =====================================================================
//  INVENTORY
// =====================================================================
InvSlot* Sim::findSlot(const std::string& id) {
    for (auto& s : m_player.inv)
        if (s.id == id) return &s;
    return nullptr;
}

bool Sim::addItem(const std::string& id, int qty) {
    if (qty <= 0) qty = 1;
    const ItemDef& base = itemDef(id);
    if (base.stack > 0) {
        if (InvSlot* ex = findSlot(id)) {
            ex->qty += qty;
            return true;
        }
    }
    if (static_cast<int>(m_player.inv.size()) >= m_player.slots) {
        pushMsg("Inventory full!");
        return false;
    }
    InvSlot entry;
    entry.id = id;
    entry.qty = qty;
    if (base.type == ItemType::Weapon) {
        entry.durab = base.durab;
        entry.hasDurab = true;
    }
    m_player.inv.push_back(std::move(entry));
    return true;
}

void Sim::removeOne(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_player.inv.size())) return;
    InvSlot& s = m_player.inv[static_cast<size_t>(idx)];
    s.qty--;
    if (s.qty <= 0) m_player.inv.erase(m_player.inv.begin() + idx);
}

void Sim::useSlot(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_player.inv.size())) return;
    const std::string id = m_player.inv[static_cast<size_t>(idx)].id;
    const ItemDef& base = itemDef(id);

    switch (base.type) {
    case ItemType::Weapon:
        m_player.weapon = id;
        pushMsg("Equipped " + base.name);
        return;
    case ItemType::Food:
        m_player.hunger = std::max(0.0f, m_player.hunger - base.hunger);
        if (base.mood != 0.0f) m_player.mood = clampf(m_player.mood + base.mood, 0, 100);
        pushMsg("Ate " + base.name);
        removeOne(idx);
        return;
    case ItemType::Drink:
        m_player.thirst = std::max(0.0f, m_player.thirst - base.thirst);
        if (base.fatigue != 0.0f)
            m_player.fatigue = std::max(0.0f, m_player.fatigue + base.fatigue);
        if (base.mood != 0.0f) m_player.mood = clampf(m_player.mood + base.mood, 0, 100);
        pushMsg("Drank " + base.name);
        removeOne(idx);
        return;
    case ItemType::Med:
        m_player.health = clampf(m_player.health + base.heal, 0, 100);
        if (base.cureInfection && m_player.infected) {
            m_player.infected = false;
            m_player.infection = 0.0f;
            pushMsg("Infection cured!");
        }
        pushMsg("Used " + base.name);
        removeOne(idx);
        return;
    case ItemType::Misc:
        if (id == "flashlight") m_player.flashlight = !m_player.flashlight;
        return;
    }
}

void Sim::reloadWeapon() {
    const ItemDef& base = itemDef(m_player.weapon);
    if (!base.ranged) return;
    if (findSlot(base.ammo))
        pushMsg("Reloaded " + base.name);
    else
        pushMsg("No ammo for " + base.name);
}

// =====================================================================
//  INTERACTION / LOOTING
// =====================================================================
bool Sim::tryInteract() {
    Container* best = nullptr;
    float bd = 2.2f;
    for (auto& c : m_world.containers) {
        if (c.opened) continue;
        float d = length(static_cast<float>(c.x) + 0.5f - m_player.pos.x,
                         static_cast<float>(c.y) + 0.5f - m_player.pos.y);
        if (d < bd) {
            bd = d;
            best = &c;
        }
    }
    if (!best) {
        pushMsg("Nothing to interact with");
        return false;
    }
    best->opened = true;
    std::vector<Drop> drops = rollLoot(best->loot, best->kind, m_rng);
    if (drops.empty()) {
        pushMsg("Empty...");
        return true;
    }
    for (const auto& d : drops) addItem(d.id, d.qty);
    pushMsg("Looted " + best->name);
    return true;
}

// =====================================================================
//  COMBAT
// =====================================================================
void Sim::attack() {
    if (m_player.dead || m_player.attackCd > 0.0f) return;
    const ItemDef& base = itemDef(m_player.weapon);
    m_player.attackCd = base.speed;

    if (base.ranged) {
        InvSlot* ammo = findSlot(base.ammo);
        if (!ammo) {
            pushMsg("Click! No ammo (R to reload)");
            m_player.attackCd = 0.2f;
            return;
        }
        ammo->qty--;
        if (ammo->qty <= 0) {
            m_player.inv.erase(m_player.inv.begin() + (ammo - &m_player.inv[0]));
        }
        const float ang = m_player.dir;
        Bullet b;
        b.pos = m_player.pos;
        b.vx = std::cos(ang) * 22.0f;
        b.vy = std::sin(ang) * 22.0f;
        b.dmg = base.dmg;
        b.life = base.range / 22.0f * 1.4f;
        m_bullets.push_back(b);
        if (m_player.weapon == "shotgun") {
            for (int s = -2; s <= 2; s++) {
                if (s == 0) continue;
                const float a2 = ang + static_cast<float>(s) * 0.12f;
                Bullet p;
                p.pos = m_player.pos;
                p.vx = std::cos(a2) * 20.0f;
                p.vy = std::sin(a2) * 20.0f;
                p.dmg = base.dmg * 0.5f;
                p.life = 0.28f;
                m_bullets.push_back(p);
            }
        }
        return;
    }

    // Melee arc.
    const float ang = m_player.dir;
    bool hitAny = false;
    for (auto& z : m_zombies) {
        if (z.dead) continue;
        const float dx = z.pos.x - m_player.pos.x, dy = z.pos.y - m_player.pos.y;
        const float d = length(dx, dy);
        if (d > base.range + 0.4f) continue;
        const float za = std::atan2(dy, dx);
        const float diff = std::fabs(normAngle(za - ang));
        if (diff < 1.1f) {
            damageZombie(z, base.dmg, ang);
            hitAny = true;
        }
    }
    if (hitAny) {
        if (InvSlot* w = findSlot(m_player.weapon)) {
            if (w->hasDurab && base.durab != kInfiniteDurab) {
                w->durab--;
                if (w->durab <= 0) {
                    pushMsg(base.name + " broke!");
                    m_player.inv.erase(m_player.inv.begin() + (w - &m_player.inv[0]));
                    m_player.weapon = "fists";
                }
            }
        }
    }
}

void Sim::damageZombie(Zombie& z, float dmg, float ang) {
    z.health -= dmg;
    z.knock = 0.25f;
    z.kx = std::cos(ang);
    z.ky = std::sin(ang);
    if (z.health <= 0.0f) killZombie(z);
}

void Sim::killZombie(Zombie& z) {
    z.dead = true;
    m_kills++;
    m_corpses.push_back(Corpse{z.pos, 0.0f});
}

// =====================================================================
//  ZOMBIES
// =====================================================================
void Sim::spawnZombie(bool downtown) {
    Vec2 sp;
    if (downtown && m_rng.nextFloat() < 0.7f) {
        sp = Vec2{30.0f + m_rng.nextFloat() * 60.0f, 24.0f + m_rng.nextFloat() * 40.0f};
    } else {
        const SpawnPoint& base =
            m_world.spawnPoints[m_rng.below(static_cast<uint32_t>(m_world.spawnPoints.size()))];
        sp = Vec2{static_cast<float>(base.x) + (m_rng.nextFloat() - 0.5f) * 3.0f,
                  static_cast<float>(base.y) + (m_rng.nextFloat() - 0.5f) * 3.0f};
    }
    if (m_world.isSolid(static_cast<int>(std::floor(sp.x)), static_cast<int>(std::floor(sp.y))))
        return;
    // Don't spawn on top of the player.
    if (m_started && dist(sp, m_player.pos) < 12.0f) return;

    const float tough = m_rng.nextFloat();
    Zombie z;
    z.pos = sp;
    z.dir = m_rng.nextFloat() * kTwoPi;
    z.health = 30.0f + tough * 50.0f;
    z.max = z.health;
    z.sprinter = tough > 0.85f;
    z.speed = 1.1f + m_rng.nextFloat() * 1.4f + (z.sprinter ? 2.2f : 0.0f);
    z.radius = 0.35f;
    z.groan = m_rng.nextFloat() * 6.0f;
    z.wander = m_rng.nextFloat() * kTwoPi;
    z.wt = 0.0f;
    if (z.sprinter)
        z.tint = ZombieTint::Sprinter;
    else
        z.tint = m_rng.nextFloat() < 0.5f ? ZombieTint::GreenA : ZombieTint::GreenB;
    m_zombies.push_back(z);
}

void Sim::updateZombies(float dt) {
    for (auto& z : m_zombies) {
        if (z.dead) continue;
        z.groan -= dt;
        if (z.groan <= 0.0f) z.groan = 4.0f + m_rng.nextFloat() * 8.0f;

        const float dp = dist(z.pos, m_player.pos);
        const bool sees = dp < 11.0f || (m_player.flashlight && dp < 16.0f);
        float tx, ty;
        if (sees) {
            tx = m_player.pos.x;
            ty = m_player.pos.y;
        } else {
            z.wt -= dt;
            if (z.wt <= 0.0f) {
                z.wander = m_rng.nextFloat() * kTwoPi;
                z.wt = 1.0f + m_rng.nextFloat() * 3.0f;
            }
            tx = z.pos.x + std::cos(z.wander);
            ty = z.pos.y + std::sin(z.wander);
        }
        const float ang = std::atan2(ty - z.pos.y, tx - z.pos.x);
        z.dir = ang;
        const float spd = (sees ? z.speed : z.speed * 0.45f) *
                          ((m_player.flashlight && z.sprinter) ? 1.2f : 1.0f);

        float mvx = std::cos(ang) * spd * dt;
        float mvy = std::sin(ang) * spd * dt;
        if (z.knock > 0.0f) {
            mvx += z.kx * z.knock * 6.0f * dt;
            mvy += z.ky * z.knock * 6.0f * dt;
            z.knock -= dt;
        }
        moveEntity(z.pos, z.radius, mvx, mvy);

        if (dp < 0.7f && m_player.attackImmune <= 0.0f) {
            hurtPlayer(0.18f + (z.sprinter ? 0.12f : 0.0f));
        }
    }
    // Cull dead occasionally to keep the vector small.
    if (m_zombies.size() > 400) {
        m_zombies.erase(std::remove_if(m_zombies.begin(), m_zombies.end(),
                                       [](const Zombie& z) { return z.dead; }),
                        m_zombies.end());
    }
}

void Sim::hurtPlayer(float dmg) {
    m_player.health -= dmg;
    m_player.hurtFlash = 0.25f;
    m_player.attackImmune = 0.5f;
    if (m_rng.nextFloat() < 0.12f) { // chance of infection bite
        if (!m_player.infected) {
            m_player.infected = true;
            pushMsg("You were bitten! Infection rising...");
        }
    }
    if (m_player.health <= 0.0f) m_player.dead = true;
}

// =====================================================================
//  BULLETS
// =====================================================================
void Sim::updateBullets(float dt) {
    for (auto& b : m_bullets) {
        b.pos.x += b.vx * dt;
        b.pos.y += b.vy * dt;
        b.life -= dt;
        if (m_world.isSolid(static_cast<int>(std::floor(b.pos.x)),
                            static_cast<int>(std::floor(b.pos.y)))) {
            b.life = 0.0f;
            continue;
        }
        for (auto& z : m_zombies) {
            if (z.dead) continue;
            if (length(z.pos.x - b.pos.x, z.pos.y - b.pos.y) < 0.5f) {
                damageZombie(z, b.dmg, std::atan2(b.vy, b.vx));
                b.life = 0.0f;
                break;
            }
        }
    }
    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
                                   [](const Bullet& b) { return b.life <= 0.0f; }),
                    m_bullets.end());
}

// =====================================================================
//  MOVEMENT & COLLISION
// =====================================================================
void Sim::moveEntity(Vec2& pos, float radius, float mvx, float mvy) {
    const float nx = pos.x + mvx;
    if (!collides(nx, pos.y, radius)) pos.x = nx;
    const float ny = pos.y + mvy;
    if (!collides(pos.x, ny, radius)) pos.y = ny;
}

bool Sim::collides(float x, float y, float r) const {
    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            const int tx = static_cast<int>(std::floor(x + static_cast<float>(ox) * r));
            const int ty = static_cast<int>(std::floor(y + static_cast<float>(oy) * r));
            if (m_world.isSolid(tx, ty)) {
                const float cx = clampf(x, static_cast<float>(tx), static_cast<float>(tx) + 1.0f);
                const float cy = clampf(y, static_cast<float>(ty), static_cast<float>(ty) + 1.0f);
                if (length(x - cx, y - cy) < r) return true;
            }
        }
    }
    return false;
}

// =====================================================================
//  PLAYER UPDATE
// =====================================================================
void Sim::updatePlayer(const Input& in, float dt) {
    float mx = in.moveX, my = in.moveY;
    if (mx != 0.0f || my != 0.0f) {
        const float l = length(mx, my);
        const float fatiguePenalty = m_player.fatigue > 70.0f ? 0.6f : 1.0f;
        const float sp = m_player.speed * fatiguePenalty * dt;
        moveEntity(m_player.pos, m_player.radius, (mx / l) * sp, (my / l) * sp);
        m_player.fatigue = std::min(100.0f, m_player.fatigue + dt * 0.6f);
    }

    // Aim toward the target point (skip if it coincides with the player).
    if (in.aim.x != m_player.pos.x || in.aim.y != m_player.pos.y) {
        m_player.dir = std::atan2(in.aim.y - m_player.pos.y, in.aim.x - m_player.pos.x);
    }

    if (in.attackHeld) attack();

    m_player.attackCd = std::max(0.0f, m_player.attackCd - dt);
    m_player.attackImmune = std::max(0.0f, m_player.attackImmune - dt);
    m_player.hurtFlash = std::max(0.0f, m_player.hurtFlash - dt);

    // Survival decay (scaled to game time).
    const float gm = dt * kTimeScale; // game-minutes elapsed
    m_player.hunger = std::min(100.0f, m_player.hunger + gm * 0.10f);
    m_player.thirst = std::min(100.0f, m_player.thirst + gm * 0.14f);
    m_player.fatigue = std::min(100.0f, m_player.fatigue + gm * 0.05f);

    // Consequences.
    if (m_player.hunger >= 100.0f || m_player.thirst >= 100.0f) m_player.health -= dt * 1.2f;
    if (m_player.fatigue >= 100.0f) m_player.mood = std::max(0.0f, m_player.mood - dt * 2.0f);
    if (m_player.infected) {
        m_player.infection = std::min(100.0f, m_player.infection + dt * 0.7f);
        m_player.health -= dt * (0.3f + m_player.infection / 120.0f);
        if (m_player.infection >= 100.0f) pushMsg("The infection takes you...");
    }
    // Slow natural regen when well-fed & rested.
    if (m_player.hunger < 60.0f && m_player.thirst < 60.0f && !m_player.infected &&
        m_player.health < 100.0f) {
        m_player.health = std::min(100.0f, m_player.health + dt * 0.4f);
    }
    m_player.mood =
        clampf(m_player.mood + (m_player.health > 50.0f ? dt * 0.1f : -dt * 0.2f), 0, 100);

    if (m_player.health <= 0.0f) m_player.dead = true;
}

// =====================================================================
//  WORLD / TIME UPDATE
// =====================================================================
void Sim::updateWorld(float dt) {
    m_dayTime += dt * kTimeScale;
    if (m_dayTime >= kDayLen) {
        m_dayTime -= kDayLen;
        m_dayCount++;
        pushMsg("Day " + std::to_string(m_dayCount) + " in Anchorage");
    }

    const bool night = isNight();
    m_waveTimer -= dt;
    if (m_waveTimer <= 0.0f) {
        m_waveTimer = night ? 1.4f : 3.5f;
        const int alive = aliveZombies();
        const int cap = 60 + m_dayCount * 25;
        if (alive < cap) {
            const int n = night ? 4 : 2;
            for (int i = 0; i < n; i++) spawnZombie(false);
        }
    }

    for (auto& c : m_corpses) c.t += dt;
    if (m_corpses.size() > 120)
        m_corpses.erase(m_corpses.begin(),
                        m_corpses.begin() + static_cast<long>(m_corpses.size() - 120));

    for (auto& m : m_messages) m.life -= dt;
    m_messages.erase(std::remove_if(m_messages.begin(), m_messages.end(),
                                    [](const LogMessage& m) { return m.life <= 0.0f; }),
                     m_messages.end());
}

// =====================================================================
//  STEP + HELPERS
// =====================================================================
void Sim::step(const Input& in) {
    if (!m_started || m_player.dead) return;

    // Discrete (edge-triggered) actions.
    if (in.toggleFlashlight) m_player.flashlight = !m_player.flashlight;
    if (in.useSlot >= 0) useSlot(in.useSlot);
    if (in.interact) tryInteract();
    if (in.reload) reloadWeapon();
    if (in.attackPressed) attack();

    updatePlayer(in, kStep);
    updateZombies(kStep);
    updateBullets(kStep);
    updateWorld(kStep);
}

int Sim::aliveZombies() const {
    int n = 0;
    for (const auto& z : m_zombies)
        if (!z.dead) n++;
    return n;
}

float Sim::darknessAlpha() const {
    const float t = m_dayTime;
    if (t < 4 * 60) return 0.78f;
    if (t < 7 * 60) return lerpf(0.78f, 0.0f, (t - 4 * 60) / (3 * 60));
    if (t < 19 * 60) return 0.0f;
    if (t < 22 * 60) return lerpf(0.0f, 0.78f, (t - 19 * 60) / (3 * 60));
    return 0.78f;
}

void Sim::pushMsg(const std::string& text) {
    m_messages.push_back(LogMessage{text, 6.0f});
    if (m_messages.size() > 30) m_messages.erase(m_messages.begin());
}

} // namespace zb
