#include "Sim.hpp"

#include <algorithm>
#include <cmath>

namespace shooter {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;

inline float len(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float dist2(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

Sim::Sim(uint32_t seed) : rng_(seed) { reset(); }

void Sim::reset() {
    state_ = GameState::Playing;
    playerPos_ = {0.0f, 0.0f};
    playerAngle_ = 0.0f;
    playerHp_ = playerMaxHp_;
    fireCooldown_ = 0.0f;
    invuln_ = 0.0f;
    wave_ = 0;
    kills_ = 0;
    score_ = 0;
    zombiesToSpawn_ = 0;
    spawnTimer_ = 0.0f;
    zombies_.clear();
    bullets_.clear();
    particles_.clear();
    pickups_.clear();
    startWave(1);
}

void Sim::startWave(int n) {
    wave_ = n;
    zombiesToSpawn_ = static_cast<int>(5.0f + static_cast<float>(n) * 3.2f);
    spawnTimer_ = 0.0f;
}

void Sim::spawnZombie() {
    const float ang = rng_.range(0.0f, kTau);
    const float dst = rng_.range(720.0f, 980.0f); // spawn just off-screen around the player
    Zombie z;
    z.pos = {playerPos_.x + std::cos(ang) * dst, playerPos_.y + std::sin(ang) * dst};
    z.wobble = rng_.range(0.0f, kTau);

    const float w = static_cast<float>(wave_);
    const float roll = rng_.nextUnit();
    if (wave_ >= 5 && roll > 0.90f) {
        z.type = ZombieType::Brute;
        z.radius = 26.0f;
        z.speed = rng_.range(38.0f, 52.0f);
        z.hp = 16.0f + w * 2.0f;
        z.damage = 20.0f;
        z.score = 40;
    } else if (wave_ >= 3 && roll < 0.16f) {
        z.type = ZombieType::Runner;
        z.radius = 12.0f;
        z.speed = rng_.range(135.0f, 165.0f) + w;
        z.hp = 2.0f + std::floor(w * 0.3f);
        z.damage = 6.0f;
        z.score = 15;
    } else {
        z.type = ZombieType::Walker;
        z.radius = 15.0f;
        z.speed = rng_.range(55.0f, 78.0f) + w * 2.2f;
        z.hp = 3.0f + std::floor(w * 0.4f);
        z.damage = 8.0f;
        z.score = 10;
    }
    z.maxHp = z.hp;
    zombies_.push_back(z);
}

void Sim::spawnBlood(Vec2 p, float dir, int count) {
    for (int i = 0; i < count; ++i) {
        const float a = dir + rng_.range(-0.9f, 0.9f);
        const float sp = rng_.range(60.0f, 300.0f);
        Particle pt;
        pt.pos = p;
        pt.vel = {std::cos(a) * sp, std::sin(a) * sp};
        pt.life = rng_.range(0.3f, 0.7f);
        pt.maxLife = 0.7f;
        pt.radius = rng_.range(1.5f, 3.6f);
        pt.r = 184;
        pt.g = 30;
        pt.b = 30;
        particles_.push_back(pt);
    }
}

void Sim::spawnMuzzle(Vec2 p, float dir) {
    for (int i = 0; i < 4; ++i) {
        const float a = dir + rng_.range(-0.35f, 0.35f);
        const float sp = rng_.range(120.0f, 260.0f);
        Particle pt;
        pt.pos = p;
        pt.vel = {std::cos(a) * sp, std::sin(a) * sp};
        pt.life = 0.12f;
        pt.maxLife = 0.12f;
        pt.radius = rng_.range(1.5f, 3.0f);
        pt.r = 255;
        pt.g = 215;
        pt.b = 106;
        particles_.push_back(pt);
    }
}

void Sim::killZombie(std::size_t index) {
    Zombie& z = zombies_[index];
    ++kills_;
    score_ += z.score;
    spawnBlood(z.pos, rng_.range(0.0f, kTau), 16);
    // Health drops: a small base chance, boosted when the player is hurting.
    const bool lowHp = playerHp_ < 35.0f;
    if (rng_.nextUnit() < 0.09f || (lowHp && rng_.nextUnit() < 0.25f)) {
        Pickup pk;
        pk.pos = z.pos;
        pk.life = 12.0f;
        pickups_.push_back(pk);
    }
    z.dead = true;
}

void Sim::update(float dt, const Input& in) {
    if (state_ == GameState::Dead) {
        if (in.restart) {
            reset();
        }
        return;
    }

    // ----- Movement -----
    Vec2 mv = in.move;
    float mlen = len(mv);
    if (mlen > 1.0f) { mv.x /= mlen; mv.y /= mlen; mlen = 1.0f; }
    playerPos_.x += mv.x * playerSpeed_ * dt;
    playerPos_.y += mv.y * playerSpeed_ * dt;
    playerPos_.x = clampf(playerPos_.x, -kWorld, kWorld);
    playerPos_.y = clampf(playerPos_.y, -kWorld, kWorld);

    // ----- Aim + fire -----
    bool firing = false;
    if (in.aiming && (in.aim.x != 0.0f || in.aim.y != 0.0f)) {
        playerAngle_ = std::atan2(in.aim.y, in.aim.x);
        firing = in.firing;
    } else if (mlen > 0.0f) {
        playerAngle_ = std::atan2(mv.y, mv.x); // face travel direction when not aiming
    }

    fireCooldown_ -= dt;
    if (firing && fireCooldown_ <= 0.0f) {
        fireCooldown_ = fireRate_;
        const float spread = rng_.range(-0.05f, 0.05f);
        const float a = playerAngle_ + spread;
        Bullet b;
        const float muzzle = playerRadius_ + 6.0f;
        b.pos = {playerPos_.x + std::cos(playerAngle_) * muzzle,
                 playerPos_.y + std::sin(playerAngle_) * muzzle};
        b.vel = {std::cos(a) * 900.0f, std::sin(a) * 900.0f};
        b.life = 0.9f;
        bullets_.push_back(b);
        spawnMuzzle(b.pos, playerAngle_);
    }

    // ----- Spawn pacing -----
    if (zombiesToSpawn_ > 0) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f) {
            spawnZombie();
            --zombiesToSpawn_;
            const float base = 0.85f - static_cast<float>(wave_) * 0.04f;
            spawnTimer_ = base < 0.18f ? 0.18f : base;
        }
    } else if (zombies_.empty()) {
        startWave(wave_ + 1);
    }

    // ----- Bullets -----
    for (Bullet& b : bullets_) {
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.life -= dt;
        if (b.life <= 0.0f) { b.dead = true; continue; }
        for (std::size_t j = 0; j < zombies_.size(); ++j) {
            Zombie& z = zombies_[j];
            if (z.dead) continue;
            const float rr = z.radius + b.radius;
            if (dist2(b.pos, z.pos) < rr * rr) {
                z.hp -= b.damage;
                z.hitFlash = 0.08f;
                spawnBlood(b.pos, std::atan2(b.vel.y, b.vel.x), 5);
                b.dead = true;
                if (z.hp <= 0.0f) killZombie(j);
                break;
            }
        }
    }
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
                                  [](const Bullet& b) { return b.dead; }),
                   bullets_.end());

    // ----- Zombies: chase, separate, bite -----
    for (std::size_t i = 0; i < zombies_.size(); ++i) {
        Zombie& z = zombies_[i];
        if (z.dead) continue;
        const float ang = std::atan2(playerPos_.y - z.pos.y, playerPos_.x - z.pos.x);
        z.wobble += dt * 6.0f;
        const float wob = std::sin(z.wobble) * 0.25f;
        z.pos.x += std::cos(ang + wob) * z.speed * dt;
        z.pos.y += std::sin(ang + wob) * z.speed * dt;
        if (z.hitFlash > 0.0f) z.hitFlash -= dt;

        // Crowd separation so the horde spreads instead of stacking into one dot.
        for (std::size_t j = 0; j < i; ++j) {
            Zombie& o = zombies_[j];
            if (o.dead) continue;
            const float rr = z.radius + o.radius;
            const float d2 = dist2(z.pos, o.pos);
            if (d2 < rr * rr && d2 > 0.01f) {
                const float d = std::sqrt(d2);
                const float push = (rr - d) * 0.5f;
                const float nx = (z.pos.x - o.pos.x) / d, ny = (z.pos.y - o.pos.y) / d;
                z.pos.x += nx * push; z.pos.y += ny * push;
                o.pos.x -= nx * push; o.pos.y -= ny * push;
            }
        }

        // Bite the player (respecting invulnerability frames).
        if (invuln_ <= 0.0f) {
            const float rr = z.radius + playerRadius_;
            if (dist2(z.pos, playerPos_) < rr * rr) {
                playerHp_ -= z.damage;
                invuln_ = 0.6f;
                spawnBlood(playerPos_, ang + kPi, 8);
                z.pos.x -= std::cos(ang) * 18.0f; // knock the biter back a touch
                z.pos.y -= std::sin(ang) * 18.0f;
                if (playerHp_ <= 0.0f) {
                    playerHp_ = 0.0f;
                    state_ = GameState::Dead;
                    return;
                }
            }
        }
    }
    zombies_.erase(std::remove_if(zombies_.begin(), zombies_.end(),
                                  [](const Zombie& z) { return z.dead; }),
                   zombies_.end());

    if (invuln_ > 0.0f) invuln_ -= dt;

    // ----- Pickups -----
    for (Pickup& p : pickups_) {
        p.life -= dt;
        p.bob += dt * 4.0f;
        const float rr = playerRadius_ + 16.0f;
        if (dist2(p.pos, playerPos_) < rr * rr) {
            playerHp_ = playerHp_ + 25.0f;
            if (playerHp_ > playerMaxHp_) playerHp_ = playerMaxHp_;
            p.life = -1.0f; // consumed
            for (int k = 0; k < 10; ++k) {
                Particle pt;
                pt.pos = p.pos;
                pt.vel = {rng_.range(-120.0f, 120.0f), rng_.range(-120.0f, 120.0f)};
                pt.life = 0.4f;
                pt.maxLife = 0.4f;
                pt.radius = rng_.range(2.0f, 4.0f);
                pt.r = 89; pt.g = 217; pt.b = 90;
                particles_.push_back(pt);
            }
        }
    }
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
                                  [](const Pickup& p) { return p.life <= 0.0f; }),
                   pickups_.end());

    // ----- Particles -----
    for (Particle& p : particles_) {
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= 0.9f;
        p.vel.y *= 0.9f;
        p.life -= dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());
}

} // namespace shooter
