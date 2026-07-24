#pragma once

#include <cmath>
#include <string>

#include "maz/scene/SceneTree.hpp"

// ZOMBOID — a native top-down twin-stick zombie SHOOTER built entirely on the Maz Engine. Its whole
// simulation — the survivor, aiming and firing, bullets, zombie health and death, endless escalating
// waves, loot and the day/night cycle — is written in maz::script and driven through a
// scene::SceneTree, exactly the way you'd author a game in Godot. This header holds the game's script
// program and a scene builder so both the playable app and the unit tests share one source of truth.
//
// Because a script can only move nodes that already exist (it cannot spawn or free them), bullets and
// zombies are OBJECT POOLS: the scene builder pre-creates a fixed pool of each, and the scripts
// activate / recycle them. Firing grabs a dormant bullet; a cleared wave revives dormant zombies.
// That keeps 100% of the game rules in deterministic, headless-testable script — the app layer only
// reads node state each frame and draws it.
namespace zomboid {

// The complete game logic, in maz::script. Shared globals wire the objects together with zero host
// coupling: g_player (the survivor), g_bullets / g_zombies (the pools), g_director (the wave spawner),
// and the score / wave / clock the whole world reads.
inline const char* scripts() {
    return R"MAZ(
var g_player = nil;
var g_director = nil;
var g_bullets = [];     # object pool: every Bullet appends itself here in _ready
var g_zombies = [];     # object pool: every Zombie appends itself here in _ready
var g_particles = [];   # object pool for impact / blood particles (juice)
var g_grenades = [];    # object pool for thrown grenades
var g_shake = 0;        # screen-shake magnitude; decays every frame

var g_score = 0;
var g_kills = 0;
var g_wave = 0;

# Day/night cycle. g_phase runs 0..g_day_len and wraps; the back half is night, when the horde
# hunts faster and bites harder. Advanced once per frame by the survivor so the whole world shares
# one clock.
var g_phase = 0;
var g_day_len = 60;

func is_night() {
    return g_phase >= (g_day_len / 2);
}

# Aggression multiplier: 1.0 by day, ramps to 1.7 at night, so dusk gets tense.
func danger() {
    if (is_night() == false) { return 1.0; }
    return 1.7;
}

# Spawn up to `count` impact particles at (x, y) from the shared pool. kind 0 = spark, 1 = blood.
func emit(x, y, count, kind) {
    var i = 0;
    var n = len(g_particles);
    var spawned = 0;
    while (i < n and spawned < count) {
        var p = g_particles[i];
        if (p.active == false) {
            p.ignite(x, y, kind);
            spawned = spawned + 1;
        }
        i = i + 1;
    }
}

# The survivor the player controls. The app sets aim_x/aim_y (a unit vector toward the mouse) and the
# `firing` flag (mouse held) each frame; the survivor owns the fire cadence and pulls bullets from the
# pool, so the entire weapon behaviour is deterministic and testable headless.
class Survivor {
    var health = 100;
    var max_health = 100;
    var hunger = 0;
    var alive = true;
    var food = 3;
    var loot_collected = 0;

    var aim_x = 1;
    var aim_y = 0;
    var firing = false;
    var shots = 0;

    # Weapon state (set by set_weapon): 0 = pistol, 1 = shotgun, 2 = SMG.
    var weapon = 0;
    var fire_rate = 6;      # shots per second (base * rate_mult)
    var fire_cd = 0;
    var damage = 25;        # damage per bullet (base * dmg_mult)
    var base_fr = 6;        # per-weapon base fire rate / damage, before upgrade multipliers
    var base_dmg = 25;
    var dmg_mult = 1.0;     # upgrade multipliers, grow between waves
    var rate_mult = 1.0;
    var upgrades = 0;       # number of between-wave upgrades applied
    var pellets = 1;        # bullets per shot (shotgun fires several)
    var spread = 0;         # random aim jitter per pellet, radians
    var bullet_speed = 70;

    # Ammo, per weapon index [pistol, shotgun, smg]: rounds in the magazine, spare rounds in reserve,
    # magazine capacity, and reload time (seconds). Firing a shot spends one magazine round.
    var mags = [12, 6, 30];
    var reserves = [48, 24, 90];
    var mag_sizes = [12, 6, 30];
    var reload_times = [1.2, 1.8, 2.0];
    var reloading = false;
    var reload_t = 0;
    var cur_ammo = 12;       # convenience mirrors of the active weapon for the HUD
    var cur_reserve = 48;
    var is_reloading = false;
    var grenades = 3;        # thrown-explosive count

    func _ready() { g_player = self; self.set_weapon(0); }

    func _process(dt) {
        # Screen shake always eases back toward rest, even on the death screen.
        g_shake = g_shake - dt * 4.0;
        if (g_shake < 0) { g_shake = 0; }
        if (self.alive == false) { return; }

        # Advance the shared world clock (survivor owns it).
        g_phase = g_phase + dt;
        if (g_phase >= g_day_len) { g_phase = g_phase - g_day_len; }

        # Survival pressure: hunger creeps up; at max hunger, health drains.
        self.hunger = self.hunger + dt * 1.5;
        if (self.hunger > 100) { self.hunger = 100; }
        if (self.hunger >= 100) { self.health = self.health - dt * 3; }

        # Weapon cadence + ammo + reload.
        self.fire_cd = self.fire_cd - dt;
        if (self.reloading) {
            self.reload_t = self.reload_t - dt;
            if (self.reload_t <= 0) { self.finish_reload(); }
        } else {
            if (self.firing and self.fire_cd <= 0) {
                if (self.mags[self.weapon] > 0) {
                    self.do_shoot();
                    self.mags[self.weapon] = self.mags[self.weapon] - 1;
                    self.fire_cd = 1.0 / self.fire_rate;
                    if (self.mags[self.weapon] <= 0) { self.start_reload(); }
                } else {
                    self.start_reload();
                }
            }
        }
        self.cur_ammo = self.mags[self.weapon];
        self.cur_reserve = self.reserves[self.weapon];
        self.is_reloading = self.reloading;

        if (self.health <= 0) { self.health = 0; self.alive = false; }
    }

    # Configure the active weapon. 0 = pistol (accurate), 1 = shotgun (spread pellets, slow),
    # 2 = SMG (fast, weaker, slight spread).
    func set_weapon(i) {
        self.weapon = i;
        if (i == 1) {
            self.base_fr = 1.6;
            self.base_dmg = 14;
            self.pellets = 6;
            self.spread = 0.28;
            self.bullet_speed = 58;
        } else {
            if (i == 2) {
                self.base_fr = 12;
                self.base_dmg = 11;
                self.pellets = 1;
                self.spread = 0.06;
                self.bullet_speed = 82;
            } else {
                self.weapon = 0;
                self.base_fr = 6;
                self.base_dmg = 25;
                self.pellets = 1;
                self.spread = 0;
                self.bullet_speed = 70;
            }
        }
        self.apply_mults();
        self.fire_cd = 0;
        self.reloading = false;
        self.reload_t = 0;
        self.is_reloading = false;
        self.cur_ammo = self.mags[self.weapon];
        self.cur_reserve = self.reserves[self.weapon];
    }

    # Fold the upgrade multipliers onto the active weapon's base stats.
    func apply_mults() {
        self.fire_rate = self.base_fr * self.rate_mult;
        self.damage = self.base_dmg * self.dmg_mult;
    }

    # Apply the next between-wave upgrade, cycling: +damage, +fire rate, +max health (heal), +ammo.
    func apply_upgrade() {
        var k = self.upgrades % 4;
        if (k == 0) {
            self.dmg_mult = self.dmg_mult + 0.2;
        } else {
            if (k == 1) {
                self.rate_mult = self.rate_mult + 0.15;
            } else {
                if (k == 2) {
                    self.max_health = self.max_health + 25;
                    self.health = self.max_health;
                } else {
                    self.reserves[0] = self.reserves[0] + 36;
                    self.reserves[1] = self.reserves[1] + 12;
                    self.reserves[2] = self.reserves[2] + 60;
                }
            }
        }
        self.upgrades = self.upgrades + 1;
        self.apply_mults();
    }

    # Fire the whole shot: one bullet per pellet, each jittered within the weapon's spread.
    func do_shoot() {
        var ax = self.aim_x;
        var ay = self.aim_y;
        var m = sqrt(ax * ax + ay * ay);
        if (m <= 0.0001) { return; }
        ax = ax / m;
        ay = ay / m;
        var p = 0;
        while (p < self.pellets) {
            self.fire_one(ax, ay);
            p = p + 1;
        }
    }

    # Launch one bullet from the pool along (ax, ay) rotated by a random spread offset.
    func fire_one(ax, ay) {
        var a = randf_range(0 - self.spread, self.spread);
        var ca = cos(a);
        var sa = sin(a);
        var dx = ax * ca - ay * sa;
        var dy = ax * sa + ay * ca;
        var i = 0;
        var n = len(g_bullets);
        while (i < n) {
            var b = g_bullets[i];
            if (b.active == false) {
                b.fire(self.node.x, self.node.y, dx, dy, self.bullet_speed, self.damage);
                self.shots = self.shots + 1;
                return;
            }
            i = i + 1;
        }
    }

    # Begin reloading the active weapon (if not already, has reserve, and isn't full).
    func start_reload() {
        if (self.reloading) { return; }
        var w = self.weapon;
        if (self.reserves[w] <= 0) { return; }
        if (self.mags[w] >= self.mag_sizes[w]) { return; }
        self.reloading = true;
        self.reload_t = self.reload_times[w];
    }

    # Move rounds from reserve into the magazine (up to capacity) and end the reload.
    func finish_reload() {
        var w = self.weapon;
        var need = self.mag_sizes[w] - self.mags[w];
        var take = need;
        if (take > self.reserves[w]) { take = self.reserves[w]; }
        self.mags[w] = self.mags[w] + take;
        self.reserves[w] = self.reserves[w] - take;
        self.reloading = false;
    }

    # Manual reload (bound to R in the app).
    func reload() { self.start_reload(); }

    # Throw a grenade along the aim vector, if any are left.
    func throw_grenade() {
        if (self.grenades <= 0) { return; }
        var ax = self.aim_x;
        var ay = self.aim_y;
        var m = sqrt(ax * ax + ay * ay);
        if (m <= 0.0001) { return; }
        ax = ax / m;
        ay = ay / m;
        var i = 0;
        var n = len(g_grenades);
        while (i < n) {
            var g = g_grenades[i];
            if (g.active == false) {
                g.throw_at(self.node.x, self.node.y, ax, ay);
                self.grenades = self.grenades - 1;
                return;
            }
            i = i + 1;
        }
    }

    func eat() {
        if (self.food > 0) {
            self.food = self.food - 1;
            self.hunger = self.hunger - 40;
            if (self.hunger < 0) { self.hunger = 0; }
        }
    }

    func collect(kind) {
        self.food = self.food + 1;
        self.loot_collected = self.loot_collected + 1;
        # Loot is also an ammo crate: top up every weapon's reserve and a grenade.
        self.reserves[0] = self.reserves[0] + 24;
        self.reserves[1] = self.reserves[1] + 8;
        self.reserves[2] = self.reserves[2] + 40;
        self.grenades = self.grenades + 1;
    }

    func take_damage(dmg) {
        self.health = self.health - dmg;
        if (self.health <= 0) { self.health = 0; self.alive = false; }
    }
}

# A pooled bullet. Dormant until fired; then it flies straight, expires after max_life, and on contact
# with a live zombie deals damage and returns itself to the pool.
class Bullet {
    var active = false;
    var vx = 0;
    var vy = 0;
    var life = 0;
    var max_life = 2.0;
    var damage = 25;
    var hit_radius = 1.6;

    func _ready() { g_bullets.append(self); }

    func fire(px, py, dirx, diry, speed, dmg) {
        self.node.x = px;
        self.node.y = py;
        self.vx = dirx * speed;
        self.vy = diry * speed;
        self.damage = dmg;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.node.x = self.node.x + self.vx * dt;
        self.node.y = self.node.y + self.vy * dt;
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }

        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var rr = self.hit_radius + z.radius;
                if (dx * dx + dy * dy <= rr * rr) {
                    z.take_damage(self.damage);
                    self.active = false;
                    return;
                }
            }
            i = i + 1;
        }
    }
}

# A pooled thrown grenade: flies from the survivor along the aim, slows, and after a short fuse
# explodes — damaging every zombie inside the blast radius and throwing off blood/shake. Great for
# clearing a cluster, but limited in supply (topped up by loot).
class Grenade {
    var active = false;
    var vx = 0;
    var vy = 0;
    var fuse = 0;
    var blast_radius = 5.0;
    var blast_dmg = 60;

    func _ready() { g_grenades.append(self); }

    func throw_at(x, y, dx, dy) {
        self.node.x = x;
        self.node.y = y;
        var spd = 28;
        self.vx = dx * spd;
        self.vy = dy * spd;
        self.fuse = 0.9;
        self.active = true;
    }

    func explode() {
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= self.blast_radius * self.blast_radius) {
                    z.take_damage(self.blast_dmg);
                }
            }
            i = i + 1;
        }
        emit(self.node.x, self.node.y, 24, 1);
        g_shake = g_shake + 1.8;
        if (g_shake > 3.0) { g_shake = 3.0; }
        self.active = false;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.node.x = self.node.x + self.vx * dt;
        self.node.y = self.node.y + self.vy * dt;
        self.vx = self.vx * 0.92;
        self.vy = self.vy * 0.92;
        self.fuse = self.fuse - dt;
        if (self.fuse <= 0) { self.explode(); }
    }
}

# A pooled impact particle: a short-lived speck that flies out from a hit and fades. Sparks (kind 0) are
# fast and brief; blood (kind 1) is redder, slower, and lingers a touch longer. Pure juice.
class Particle {
    var active = false;
    var vx = 0;
    var vy = 0;
    var life = 0;
    var max_life = 0.5;
    var kind = 0;

    func _ready() { g_particles.append(self); }

    func ignite(x, y, k) {
        self.node.x = x;
        self.node.y = y;
        self.kind = k;
        var ang = randf_range(0, 6.2831853);
        var spd = 0;
        if (k == 1) {
            spd = randf_range(6, 20);
            self.max_life = randf_range(0.3, 0.7);
        } else {
            spd = randf_range(10, 30);
            self.max_life = randf_range(0.12, 0.3);
        }
        self.vx = cos(ang) * spd;
        self.vy = sin(ang) * spd;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.node.x = self.node.x + self.vx * dt;
        self.node.y = self.node.y + self.vy * dt;
        self.vx = self.vx * 0.9;
        self.vy = self.vy * 0.9;
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; }
    }
}

# A pooled zombie. Dormant (alive == false) until the Director spawns it into a wave; then it walks at
# the survivor and bites on a cooldown. Killed by bullets; on death it awards score and goes dormant
# so the Director can recycle it next wave.
class Zombie {
    var alive = false;
    var kind = 0;          # 0 walker, 1 runner, 2 brute, 3 boss
    var health = 30;
    var max_health = 30;
    var speed = 15;
    var damage = 6;
    var radius = 1.0;      # body radius (bullet hit test + draw size)
    var attack_range = 1.2;
    var score_value = 10;
    var cooldown = 0;

    func _ready() { g_zombies.append(self); }

    # Revive as a plain walker with explicit hp/speed (used by tests to park a target).
    func spawn_at(x, y, hp, spd) {
        self.node.x = x;
        self.node.y = y;
        self.kind = 0;
        self.health = hp;
        self.max_health = hp;
        self.speed = spd;
        self.damage = 6;
        self.radius = 1.0;
        self.attack_range = 1.2;
        self.score_value = 10;
        self.cooldown = 0;
        self.alive = true;
    }

    # Director call: revive as kind k with wave-w-scaled stats.
    #   0 walker - baseline    1 runner - fast/fragile
    #   2 brute  - slow/tanky/big/hard-hitting    3 boss - huge, every 5th wave
    func spawn(x, y, k, w) {
        self.node.x = x;
        self.node.y = y;
        self.kind = k;
        self.cooldown = 0;
        self.alive = true;
        if (k == 1) {
            self.health = 14 + w * 4;
            self.speed = 22 + w;
            self.damage = 4;
            self.radius = 0.8;
            self.attack_range = 1.0;
            self.score_value = 8;
        } else {
            if (k == 2) {
                self.health = 80 + w * 20;
                self.speed = 8;
                self.damage = 16;
                self.radius = 1.8;
                self.attack_range = 1.8;
                self.score_value = 25;
            } else {
                if (k == 3) {
                    self.health = 400 + w * 60;
                    self.speed = 7;
                    self.damage = 30;
                    self.radius = 3.0;
                    self.attack_range = 2.5;
                    self.score_value = 200;
                } else {
                    self.health = 25 + w * 8;
                    self.speed = 13 + w;
                    self.damage = 6;
                    self.radius = 1.0;
                    self.attack_range = 1.2;
                    self.score_value = 10;
                }
            }
        }
        if (self.speed > 30) { self.speed = 30; }
        self.max_health = self.health;
    }

    func take_damage(dmg) {
        if (self.alive == false) { return; }
        self.health = self.health - dmg;
        emit(self.node.x, self.node.y, 3, 0); # hit sparks
        if (self.health <= 0) {
            self.health = 0;
            self.alive = false;
            g_kills = g_kills + 1;
            g_score = g_score + self.score_value;
            emit(self.node.x, self.node.y, 10, 1); # blood burst on death
            var s = 0.5;
            if (self.kind == 2) { s = 1.0; }
            if (self.kind == 3) { s = 2.5; }
            g_shake = g_shake + s;
            if (g_shake > 3.0) { g_shake = 3.0; }
        }
    }

    func _process(dt) {
        if (self.alive == false) { return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var aggro = danger();
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var dist = sqrt(dx * dx + dy * dy);
        if (dist > self.attack_range) {
            self.node.x = self.node.x + (dx / dist) * self.speed * aggro * dt;
            self.node.y = self.node.y + (dy / dist) * self.speed * aggro * dt;
        }
        self.cooldown = self.cooldown - dt;
        if (dist <= self.attack_range and self.cooldown <= 0) {
            g_player.take_damage(self.damage * aggro);
            self.cooldown = 1.0;
        }
    }
}

# The wave director: when the field is clear, waits a short beat then spawns the next, larger, tougher
# wave by reviving pooled zombies on a ring around the survivor. Endless and escalating.
class Director {
    var wave = 0;
    var break_timer = 0;    # 0 at start so wave 1 begins immediately
    var base = 4;

    func _ready() { g_director = self; }

    func alive_count() {
        var c = 0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            if (g_zombies[i].alive) { c = c + 1; }
            i = i + 1;
        }
        return c;
    }

    func start_wave(w) {
        # Reward surviving the previous wave with a permanent upgrade.
        if (w >= 2 and g_player != nil) { g_player.apply_upgrade(); }
        var pool = len(g_zombies);
        var count = self.base + w * 2;
        if (count > pool) { count = pool; }
        var cx = 0;
        var cy = 0;
        if (g_player != nil) { cx = g_player.node.x; cy = g_player.node.y; }
        var boss = 0;
        if (w % 5 == 0) { boss = 1; }
        var i = 0;
        while (i < pool) {
            var z = g_zombies[i];
            if (i < count) {
                # Choose this spawn's kind: a boss leads every 5th wave, brutes from wave 3,
                # runners from wave 2, walkers otherwise.
                var k = 0;
                if (boss == 1 and i == 0) {
                    k = 3;
                } else {
                    if (i % 5 == 0 and w >= 3) {
                        k = 2;
                    } else {
                        if (i % 3 == 0 and w >= 2) {
                            k = 1;
                        } else {
                            k = 0;
                        }
                    }
                }
                var ang = 6.28318530718 * i / count;
                var r = 34 + randf_range(0, 10);
                z.spawn(cx + cos(ang) * r, cy + sin(ang) * r, k, w);
            }
            i = i + 1;
        }
    }

    func _process(dt) {
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        if (self.alive_count() == 0) {
            self.break_timer = self.break_timer - dt;
            if (self.break_timer <= 0) {
                self.wave = self.wave + 1;
                g_wave = self.wave;
                self.start_wave(self.wave);
                self.break_timer = 3.0;
            }
        } else {
            self.break_timer = 3.0;
        }
    }
}

# A pickup the survivor can collect for food/rations. Recycles via a `taken` flag.
class Loot {
    var kind = "ration";
    var taken = false;
    var pickup_range = 2.5;

    func _process(dt) {
        if (self.taken) { return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var dist = sqrt(dx * dx + dy * dy);
        if (dist <= self.pickup_range) {
            self.taken = true;
            g_player.collect(self.kind);
        }
    }
}
)MAZ";
}

// Pool / scene sizes. Public so the app and tests agree on how many sprites to expect.
constexpr int kBulletPool = 64;
constexpr int kZombiePool = 40;
constexpr int kParticlePool = 90;
constexpr int kGrenadePool = 8;
constexpr int kLootCount = 3;

// Build the starting scene: a survivor at the origin, a wave Director, a pool of dormant zombies and
// bullets, and some loot. Returns the survivor node so the app/test can read its stats and drive it.
inline maz::scene::SceneNode* buildScene(maz::scene::SceneTree& tree) {
    tree.loadScripts(scripts());

    maz::scene::SceneNode* survivor = tree.createChild(tree.root(), "Survivor");
    survivor->setPosition(0, 0);
    survivor->addToGroup("player");
    tree.attachScript(*survivor, "Survivor");

    // Bullet pool — dormant, parked off-field until fired.
    for (int i = 0; i < kBulletPool; ++i) {
        maz::scene::SceneNode* b = tree.createChild(tree.root(), "Bullet" + std::to_string(i));
        b->setPosition(100000.0, 100000.0);
        b->addToGroup("bullets");
        tree.attachScript(*b, "Bullet");
    }

    // Particle pool — dormant sparks/blood the game ignites on impacts.
    for (int i = 0; i < kParticlePool; ++i) {
        maz::scene::SceneNode* p = tree.createChild(tree.root(), "Particle" + std::to_string(i));
        p->setPosition(100000.0, 100000.0);
        p->addToGroup("particles");
        tree.attachScript(*p, "Particle");
    }

    // Grenade pool — dormant thrown explosives.
    for (int i = 0; i < kGrenadePool; ++i) {
        maz::scene::SceneNode* g = tree.createChild(tree.root(), "Grenade" + std::to_string(i));
        g->setPosition(100000.0, 100000.0);
        g->addToGroup("grenades");
        tree.attachScript(*g, "Grenade");
    }

    // Zombie pool — dormant; the Director revives them wave by wave.
    for (int i = 0; i < kZombiePool; ++i) {
        maz::scene::SceneNode* z = tree.createChild(tree.root(), "Zombie" + std::to_string(i));
        z->setPosition(100000.0, 100000.0);
        z->addToGroup("zombies");
        tree.attachScript(*z, "Zombie");
    }

    // The wave director (invisible controller node).
    maz::scene::SceneNode* director = tree.createChild(tree.root(), "Director");
    tree.attachScript(*director, "Director");

    // Scattered loot.
    for (int i = 0; i < kLootCount; ++i) {
        maz::scene::SceneNode* l = tree.createChild(tree.root(), "Loot" + std::to_string(i));
        l->setPosition(-10.0 + i * 10.0, 12.0);
        l->addToGroup("loot");
        tree.attachScript(*l, "Loot");
    }

    return survivor;
}

} // namespace zomboid
