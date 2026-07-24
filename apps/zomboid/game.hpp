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
var g_medkits = [];     # object pool for dropped health pickups
var g_ammo = [];        # object pool for dropped ammo pickups
var g_spits = [];       # object pool for spitter acid globs (enemy ranged projectiles)
var g_powerups = [];    # object pool for timed power-up pickups (rapid-fire / damage / shield)
var g_crates = [];      # object pool for periodic supply-crate care packages
var g_mines = [];       # object pool for deployable proximity mines
var g_sentries = [];    # object pool for deployable auto-turret sentries
var g_fires = [];       # object pool for molotov fire patches (burning ground)
var g_acid = [];        # object pool for spitter acid puddles (caustic ground hazard)
var g_barrels = [];     # explosive barrels scattered in the arena (shoot to detonate)
var g_shake = 0;        # screen-shake magnitude; decays every frame

var g_score = 0;
var g_kills = 0;
var g_wave = 0;

# Salvage economy: kills drop cash the survivor banks and spends mid-fight on ammo, grenades, or a heal
# (keys 6/7/8). A light between-the-action shop that rewards racking up kills.
var g_cash = 0;

# Flips to false the moment the survivor takes any health/armor damage during the current wave.
# Clearing a wave with it still true earns a "flawless wave" bonus. Reset to true at each wave start.
var g_wave_clean = true;

# Wave mutator: from wave 3 on, each wave rolls a random modifier that reshapes the whole horde for
# that wave, for run-to-run variety. 0 none, 1 feral (faster), 2 hulking (tougher), 3 frenzy (more of
# them). Applied to every zombie as it spawns; the survivor sees the active modifier on the HUD.
var g_mutator = 0;

# Kill-streak combo: fast, unbroken kills build a score multiplier that decays if you stop killing.
var g_combo = 0;         # current streak length
var g_mult = 1;          # score multiplier from the streak (1 + one per 5 kills, capped at 5)
var g_combo_timer = 0;   # seconds since the last kill
var g_combo_window = 2.5;
var g_streak_rewards = 0; # count of killstreak milestones (every 10th unbroken kill) hit this run

# Day/night cycle. g_phase runs 0..g_day_len and wraps; the back half is night, when the horde
# hunts faster and bites harder. Advanced once per frame by the survivor so the whole world shares
# one clock.
var g_phase = 0;
var g_day_len = 60;

func is_night() {
    return g_phase >= (g_day_len / 2);
}

# Aggression multiplier driving horde speed + bite damage. Smoothly ramps from 1.0 at dawn/midday up
# to 1.7 at midnight and back down, so tension builds through dusk and eases at first light instead of
# snapping on at a hard boundary.
func danger() {
    var t = g_phase / g_day_len;                     # 0..1 across a full day
    var night = (1.0 - cos(t * 6.2831853)) / 2.0;    # 0 at midday/dawn, 1 at midnight
    return 1.0 + 0.7 * night;
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
    var armor = 0;          # body-armor plate: a depletable damage buffer bought from the shop
    var armor_max = 100;
    var hunger = 0;
    var alive = true;
    var food = 3;
    var loot_collected = 0;
    var regen_timer = 0;   # seconds since last damage; after a delay the survivor slowly heals
    var time_survived = 0; # seconds alive this run (for the end-of-run summary)
    var hits = 0;          # bullets that connected (paired with `shots` for accuracy)
    var crate_timer = 30;  # seconds until the next supply-crate care package drops

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
    # Temporary power-up buff (from pooled pickups): kind -1 none, 0 rapid-fire, 1 damage, 2 shield,
    # 3 piercing rounds.
    var buff_kind = -1;
    var buff_timer = 0;
    var buff_fr = 1.0;      # temporary fire-rate / damage multipliers layered over the upgrades
    var buff_dmg = 1.0;
    var pierce_shots = false;  # while a piercing power-up is active, bullets punch through zombies
    var adrenaline = false; # last-stand surge: fire faster while critically wounded (<25% health)
    var crit_chance = 0.15; # chance a shot lands a critical hit for bonus damage
    var crit_mult = 2.0;    # critical-hit damage multiplier
    var pellets = 1;        # bullets per shot (shotgun fires several)
    var spread = 0;         # random aim jitter per pellet, radians
    var bullet_speed = 70;

    # Ammo, per weapon index [pistol, shotgun, smg, railgun, flamethrower]: rounds in the magazine,
    # spare rounds in reserve, magazine capacity, and reload time (seconds). Firing spends one round;
    # the flamethrower burns fuel per tick, so it carries a big tank.
    var mags = [12, 6, 30, 5, 100];
    var reserves = [48, 24, 90, 20, 200];
    var mag_sizes = [12, 6, 30, 5, 100];
    var reload_times = [1.2, 1.8, 2.0, 2.5, 2.2];
    var reloading = false;
    var reload_t = 0;
    var cur_ammo = 12;       # convenience mirrors of the active weapon for the HUD
    var cur_reserve = 48;
    var is_reloading = false;
    var grenades = 3;        # thrown-explosive count
    var mines = 2;           # deployable proximity-mine stock
    var sentries = 1;        # deployable auto-turret stock
    var molotovs = 2;        # thrown firebomb stock
    # Overcharge ultimate: kills fill the meter; when full, detonate wipes the field.
    var ult = 0;
    var ult_max = 25;
    var ult_ready = false;
    # Kill-milestone rewards: every `bonus_step` kills grant a grenade and a small heal.
    var kills = 0;
    var next_bonus = 25;
    var bonus_step = 25;
    # Second wind: a stored revive charge. Lethal damage is cancelled — the survivor bursts back with
    # half health, brief invulnerability, and a nova that clears the crowd. Earned again every 50 kills.
    var revives = 1;
    var next_revive_at = 50;
    # Evasive dodge-roll: a quick directional burst with brief invulnerability, then a cooldown.
    var dash_cd = 0;         # seconds until the dodge is ready again
    var dash_cd_max = 4.0;
    var dash_time = 0;       # remaining dodge-burst movement duration
    var dash_vx = 0;
    var dash_vy = 0;
    var iframes = 0;         # invulnerability window granted by the dodge
    var dash_speed = 46;
    var dash_dmg = 40;       # a dash shoulder-checks zombies it passes through for this damage
    var dash_hits = [];      # zombies already struck this dash (one shove each, not per-frame)
    # Close-quarters melee shove: a last-resort swing that damages and knocks back adjacent zombies.
    var melee_cd = 0;
    var melee_cd_max = 1.1;
    var melee_range = 4.5;
    var melee_dmg = 55;

    func _ready() { g_player = self; self.set_weapon(0); }

    # Called once per zombie kill: charges the ultimate and hands out milestone rewards.
    func on_kill() {
        self.kills = self.kills + 1;
        self.add_ult(1);
        if (self.kills >= self.next_bonus) {
            self.next_bonus = self.next_bonus + self.bonus_step;
            self.grenades = self.grenades + 1;
            self.heal(15);
        }
        if (self.kills >= self.next_revive_at) {
            self.next_revive_at = self.next_revive_at + 50;
            if (self.revives < 3) { self.revives = self.revives + 1; }
        }
    }

    # Cash in a second-wind charge: cancel death, burst back to half health with emergency i-frames,
    # and detonate a nova that damages and knocks back the surrounding crowd to buy breathing room.
    func second_wind() {
        self.revives = self.revives - 1;
        self.health = self.max_health * 0.5;
        self.alive = true;
        self.iframes = 2.0;     # emergency invulnerability window
        self.hunger = 0;        # relieve the hunger pressure too
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= 100.0) {   # nova radius 10
                    var m = sqrt(d2);
                    if (m < 0.01) { m = 0.01; }
                    z.hit_knockback(dx / m, dy / m, 8.0);
                    z.take_damage(150);
                }
            }
            i = i + 1;
        }
        emit(self.node.x, self.node.y, 40, 1);
        g_shake = 3.0;
    }

    # Add ultimate charge (one per kill) until the meter is full.
    func add_ult(n) {
        if (self.ult_ready) { return; }
        self.ult = self.ult + n;
        if (self.ult >= self.ult_max) {
            self.ult = self.ult_max;
            self.ult_ready = true;
        }
    }

    # Unleash the charged ultimate: a screen-wide blast that hammers every live zombie, then resets.
    func detonate() {
        if (self.ult_ready == false) { return; }
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) { z.take_damage(500); }
            i = i + 1;
        }
        emit(self.node.x, self.node.y, 40, 1);
        g_shake = 3.0;
        self.ult = 0;
        self.ult_ready = false;
        # A brief invulnerability window as the blast goes off, so the ultimate is a true panic button —
        # you're safe for the moment it takes to clear the field, even mid-swarm.
        if (self.iframes < 1.0) { self.iframes = 1.0; }
    }

    func _process(dt) {
        # Screen shake always eases back toward rest, even on the death screen.
        g_shake = g_shake - dt * 4.0;
        if (g_shake < 0) { g_shake = 0; }
        # Combo decays if you stop killing.
        g_combo_timer = g_combo_timer + dt;
        if (g_combo_timer > g_combo_window) {
            g_combo = 0;
            g_mult = 1;
        }
        if (self.alive == false) { return; }

        # Dodge-roll timers: cooldown recharges, i-frames tick down, and an active
        # dodge carries the survivor in a short burst.
        if (self.dash_cd > 0) { self.dash_cd = self.dash_cd - dt; }
        if (self.melee_cd > 0) { self.melee_cd = self.melee_cd - dt; }
        if (self.iframes > 0) { self.iframes = self.iframes - dt; }
        if (self.dash_time > 0) {
            self.dash_time = self.dash_time - dt;
            self.node.x = self.node.x + self.dash_vx * dt;
            self.node.y = self.node.y + self.dash_vy * dt;
            # Offensive dodge: shoulder-check every zombie the roll passes through — one shove each,
            # knocking it back and dealing dash damage, so the dodge doubles as a way to bulldoze out.
            var di = 0;
            var dn = len(g_zombies);
            while (di < dn) {
                var dz = g_zombies[di];
                if (dz.alive and self.dash_struck(dz) == false) {
                    var ddx = dz.node.x - self.node.x;
                    var ddy = dz.node.y - self.node.y;
                    var dr = 2.5 + dz.radius;
                    if (ddx * ddx + ddy * ddy <= dr * dr) {
                        var dm = sqrt(ddx * ddx + ddy * ddy);
                        if (dm < 0.01) { dm = 0.01; }
                        dz.hit_knockback(ddx / dm, ddy / dm, 5.0);
                        dz.take_damage(self.dash_dmg);
                        dz.stagger(0.4);   # bulldozing through a body flinches it, like the melee shove
                        self.dash_hits.append(dz);
                    }
                }
                di = di + 1;
            }
        }

        # Advance the shared world clock (survivor owns it).
        g_phase = g_phase + dt;
        if (g_phase >= g_day_len) { g_phase = g_phase - g_day_len; }

        # Power-up buff countdown: when it lapses, strip the temporary multipliers.
        if (self.buff_kind >= 0) {
            self.buff_timer = self.buff_timer - dt;
            if (self.buff_timer <= 0) {
                self.buff_kind = -1;
                self.buff_timer = 0;
                self.buff_fr = 1.0;
                self.buff_dmg = 1.0;
                self.pierce_shots = false;
                self.apply_mults();
            }
        }

        # Last-stand adrenaline: critically wounded (<25% health) makes the survivor fire faster.
        var low = false;
        if (self.health <= self.max_health * 0.25) { low = true; }
        if (low != self.adrenaline) {
            self.adrenaline = low;
            self.apply_mults();
        }

        # Run timer for the end-of-run summary (advances only while alive).
        self.time_survived = self.time_survived + dt;

        # Periodic supply-crate care package: drops on the ring around the survivor.
        self.crate_timer = self.crate_timer - dt;
        if (self.crate_timer <= 0) {
            self.crate_timer = 30;
            var cang = randf_range(0, 6.2831853);
            var crad = 14 + randf_range(0, 8);
            drop_crate(self.node.x + cos(cang) * crad, self.node.y + sin(cang) * crad);
        }

        # Out-of-combat regeneration: stay unharmed for a few seconds and health slowly recovers.
        self.regen_timer = self.regen_timer + dt;
        if (self.regen_timer > 5.0 and self.health < self.max_health) {
            self.health = self.health + dt * 4.0;
            if (self.health > self.max_health) { self.health = self.max_health; }
        }

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
                # Overflow power-up (kind 6): while active, fire freely — no ammo spent, no reloads.
                var infinite = false;
                if (self.buff_kind == 6 and self.buff_timer > 0) { infinite = true; }
                if (infinite) {
                    self.do_shoot();
                    self.fire_cd = 1.0 / self.fire_rate;
                } else {
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
        }
        self.cur_ammo = self.mags[self.weapon];
        self.cur_reserve = self.reserves[self.weapon];
        self.is_reloading = self.reloading;

        if (self.health <= 0) {
            if (self.revives > 0) { self.second_wind(); }
            else { self.health = 0; self.alive = false; }
        }
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
                if (i == 3) {
                    # Railgun: slow, high-damage, pierces a whole line of zombies (hitscan beam).
                    self.base_fr = 2;
                    self.base_dmg = 40;
                    self.pellets = 1;
                    self.spread = 0;
                    self.bullet_speed = 120;
                } else {
                    if (i == 4) {
                        # Flamethrower: rapid, very short range; low direct damage but sets zombies
                        # alight so the burn does the real work. No projectiles — a cone in front.
                        self.base_fr = 14;
                        self.base_dmg = 5;
                        self.pellets = 1;
                        self.spread = 0;
                        self.bullet_speed = 40;
                    } else {
                        self.weapon = 0;
                        self.base_fr = 6;
                        self.base_dmg = 25;
                        self.pellets = 1;
                        self.spread = 0;
                        self.bullet_speed = 70;
                    }
                }
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
        var adr = 1.0;
        if (self.adrenaline) { adr = 1.5; }   # last-stand surge
        self.fire_rate = self.base_fr * self.rate_mult * self.buff_fr * adr;
        self.damage = self.base_dmg * self.dmg_mult * self.buff_dmg;
    }

    # Roll this shot's damage: usually the base, occasionally a critical hit for bonus damage.
    func shot_damage() {
        if (randf() < self.crit_chance) { return self.damage * self.crit_mult; }
        return self.damage;
    }

    # Activate a timed power-up buff picked up from the field. A new pickup refreshes the timer.
    func grant_powerup(kind) {
        self.buff_kind = kind;
        self.buff_timer = 8.0;
        self.buff_fr = 1.0;
        self.buff_dmg = 1.0;
        self.pierce_shots = false;
        if (kind == 0) { self.buff_fr = 2.2; }   # rapid fire
        if (kind == 1) { self.buff_dmg = 2.2; }  # double damage
        if (kind == 3) { self.pierce_shots = true; }  # piercing rounds
        # Cryo nova (kind 4): an instant panic button — chills every zombie on the field so a swarm
        # crawls while you reposition. One-shot on pickup rather than a sustained buff.
        if (kind == 4) {
            var i = 0;
            var n = len(g_zombies);
            while (i < n) {
                var z = g_zombies[i];
                if (z.alive) { z.apply_slow(4.0); }
                i = i + 1;
            }
            emit(self.node.x, self.node.y, 24, 0);   # frost burst
        }
        # Vampiric (kind 5): a sustained buff — no instant effect. While it lasts, each kinetic hit
        # (bullet / railgun beam) siphons a little health back, handled where those hits land.
        # Overflow (kind 6): infinite ammo / no reloads while active. Cancel any in-progress reload so
        # you can fire immediately, and top the current magazine for a clean look on the HUD.
        if (kind == 6) {
            self.reloading = false;
            self.mags[self.weapon] = self.mag_sizes[self.weapon];
        }
        self.apply_mults();
    }

    # True while a Vampiric (kind 5) buff is active — kinetic hits leech health back to the survivor.
    func lifesteal_active() {
        if (self.buff_kind == 5) {
            if (self.buff_timer > 0) { return true; }
        }
        return false;
    }

    # Apply the next between-wave upgrade, cycling: +damage, +fire rate, +max health (heal), +ammo,
    # +crit chance.
    func apply_upgrade() {
        var k = self.upgrades % 5;
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
                    if (k == 3) {
                        self.reserves[0] = self.reserves[0] + 36;
                        self.reserves[1] = self.reserves[1] + 12;
                        self.reserves[2] = self.reserves[2] + 60;
                        self.reserves[3] = self.reserves[3] + 10;
                        self.reserves[4] = self.reserves[4] + 120;
                    } else {
                        self.crit_chance = self.crit_chance + 0.05;
                    }
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
        if (self.weapon == 3) { self.railgun_fire(ax, ay); return; }
        if (self.weapon == 4) { self.flamethrower_fire(ax, ay); return; }
        var p = 0;
        while (p < self.pellets) {
            self.fire_one(ax, ay);
            p = p + 1;
        }
    }

    # Railgun: a piercing hitscan beam. Damages every zombie whose body straddles the aim ray in
    # front of the survivor (all in one shot), then spawns a harmless fast tracer bullet for the visual.
    func railgun_fire(ax, ay) {
        var beam = 1.2;
        var dmg = self.shot_damage();   # one crit roll for the whole beam
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var rx = z.node.x - self.node.x;
                var ry = z.node.y - self.node.y;
                var t = rx * ax + ry * ay;        # distance along the beam
                if (t >= 0) {
                    var px = rx - t * ax;         # perpendicular offset from the beam
                    var py = ry - t * ay;
                    var rr = beam + z.radius;
                    if (px * px + py * py <= rr * rr) {
                        z.take_damage(dmg);
                        z.apply_bleed(1);   # the beam lacerates too
                        if (self.lifesteal_active()) { self.heal(1.0); }   # Vampiric leech, per body
                        self.hits = self.hits + 1;   # railgun beam connections count too
                    }
                }
            }
            i = i + 1;
        }
        # Visual tracer: a pierce bullet flies through everything and just expires (no extra damage).
        var j = 0;
        var mb = len(g_bullets);
        while (j < mb) {
            var b = g_bullets[j];
            if (b.active == false) {
                b.fire(self.node.x, self.node.y, ax, ay, self.bullet_speed, 0);
                b.pierce = true;
                self.shots = self.shots + 1;
                return;
            }
            j = j + 1;
        }
        self.shots = self.shots + 1;
    }

    # Flamethrower: no projectiles — a short cone of fire in front of the survivor. Every live zombie
    # inside the cone takes a little direct damage and is set alight, so the lingering burn does the
    # heavy lifting. Devastating against packs at close range, useless at distance.
    func flamethrower_fire(ax, ay) {
        var range = 11.0;
        var dmg = self.shot_damage();
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var rx = z.node.x - self.node.x;
                var ry = z.node.y - self.node.y;
                var d = sqrt(rx * rx + ry * ry);
                if (d <= range) {
                    var dot = 1.0;                          # a body right on top counts as in-cone
                    if (d > 0.01) { dot = (rx * ax + ry * ay) / d; }
                    if (dot > 0.6) {                        # within a ~53-degree cone of the aim
                        z.take_damage(dmg);
                        z.ignite(1.4, 16);
                        self.hits = self.hits + 1;
                    }
                }
            }
            i = i + 1;
        }
        emit(self.node.x + ax * 3.0, self.node.y + ay * 3.0, 4, 2);   # flame lick at the nozzle
        self.shots = self.shots + 1;
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
                b.fire(self.node.x, self.node.y, dx, dy, self.bullet_speed, self.shot_damage());
                if (self.pierce_shots) { b.pierce_left = 2; }  # punch through up to 2 extra zombies
                if (self.weapon == 1) { b.falloff = true; }    # shotgun pellets fade with travel
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

    # Hurl a molotov in the aim direction: it lands a fixed distance ahead and leaves a burning fire
    # patch that ignites any zombie standing in it. Consumes one from the stock. Returns true if thrown.
    func throw_molotov() {
        if (self.molotovs <= 0) { return false; }
        var ax = self.aim_x;
        var ay = self.aim_y;
        var m = sqrt(ax * ax + ay * ay);
        if (m <= 0.0001) { return false; }
        ax = ax / m;
        ay = ay / m;
        var tx = self.node.x + ax * 9.0;   # landing point ahead of the survivor
        var ty = self.node.y + ay * 9.0;
        var i = 0;
        var n = len(g_fires);
        while (i < n) {
            var f = g_fires[i];
            if (f.active == false) {
                f.ignite_ground(tx, ty);
                self.molotovs = self.molotovs - 1;
                return true;
            }
            i = i + 1;
        }
        return false;
    }

    # Deploy a proximity mine at the survivor's feet: it arms after a short delay, then detonates when
    # a zombie steps near. Consumes one from the stock. Returns true if one was placed.
    func place_mine() {
        if (self.mines <= 0) { return false; }
        var i = 0;
        var n = len(g_mines);
        while (i < n) {
            var m = g_mines[i];
            if (m.active == false) {
                m.arm(self.node.x, self.node.y);
                self.mines = self.mines - 1;
                return true;
            }
            i = i + 1;
        }
        return false;
    }

    # Deploy an auto-turret sentry at the survivor's feet: it auto-fires at nearby zombies for a short
    # lifetime, then powers down. Consumes one from the stock. Returns true if one was placed.
    func place_sentry() {
        if (self.sentries <= 0) { return false; }
        var i = 0;
        var n = len(g_sentries);
        while (i < n) {
            var s = g_sentries[i];
            if (s.active == false) {
                s.deploy(self.node.x, self.node.y);
                self.sentries = self.sentries - 1;
                return true;
            }
            i = i + 1;
        }
        return false;
    }

    func eat() {
        if (self.food > 0) {
            self.food = self.food - 1;
            self.hunger = self.hunger - 40;
            if (self.hunger < 0) { self.hunger = 0; }
        }
    }

    # Restore health from a medkit, capped at the current max.
    func heal(amount) {
        self.health = self.health + amount;
        if (self.health > self.max_health) { self.health = self.max_health; }
    }

    func collect(kind) {
        self.food = self.food + 1;
        self.loot_collected = self.loot_collected + 1;
        # Loot is also an ammo crate: top up every weapon's reserve and a grenade.
        self.reserves[0] = self.reserves[0] + 24;
        self.reserves[1] = self.reserves[1] + 8;
        self.reserves[2] = self.reserves[2] + 40;
        self.reserves[3] = self.reserves[3] + 6;
        self.reserves[4] = self.reserves[4] + 80;
        self.grenades = self.grenades + 1;
    }

    # Grab a dropped ammo box: tops up the active weapon's reserve, with a little for the others.
    func collect_ammo() {
        self.reserves[self.weapon] = self.reserves[self.weapon] + self.mag_sizes[self.weapon] * 2;
        var i = 0;
        while (i < 5) {
            if (i != self.weapon) { self.reserves[i] = self.reserves[i] + 4; }
            i = i + 1;
        }
    }

    # Spend banked salvage on a mid-fight purchase: 0 = ammo refill, 1 = grenade, 2 = heal. Returns
    # true if the survivor could afford it (and the buy landed), false if too poor.
    func buy(kind) {
        if (self.alive == false) { return false; }
        var cost = 50;
        if (kind == 1) { cost = 40; }
        if (kind == 2) { cost = 60; }
        if (kind == 3) { cost = 80; }
        if (g_cash < cost) { return false; }
        g_cash = g_cash - cost;
        if (kind == 0) { self.collect_ammo(); }
        if (kind == 1) { self.grenades = self.grenades + 1; }
        if (kind == 2) { self.heal(40); }
        if (kind == 3) { self.armor = self.armor_max; }   # strap on a fresh armor plate
        emit(self.node.x, self.node.y, 8, 0);
        return true;
    }

    # Grab a supply-crate care package: a big refill of ammo, grenades, and health.
    func collect_crate() {
        self.heal(50);
        self.grenades = self.grenades + 2;
        self.mines = self.mines + 1;
        self.sentries = self.sentries + 1;
        self.molotovs = self.molotovs + 1;
        self.reserves[0] = self.reserves[0] + 48;
        self.reserves[1] = self.reserves[1] + 16;
        self.reserves[2] = self.reserves[2] + 90;
        self.reserves[3] = self.reserves[3] + 15;
        self.reserves[4] = self.reserves[4] + 160;
    }

    # Evasive dodge-roll in a direction: a quick burst of movement plus brief invulnerability,
    # then a cooldown. Returns true if the dodge fired, false if unavailable (cooling down / no aim).
    func dash(dirx, diry) {
        if (self.alive == false) { return false; }
        if (self.dash_cd > 0) { return false; }
        var m = sqrt(dirx * dirx + diry * diry);
        if (m < 0.01) { return false; }
        self.dash_vx = dirx / m * self.dash_speed;
        self.dash_vy = diry / m * self.dash_speed;
        self.dash_time = 0.22;
        self.iframes = 0.35;
        self.dash_cd = self.dash_cd_max;
        self.dash_hits = [];   # fresh strike list for this dash
        return true;
    }

    # Has this dash already shoulder-checked zombie `z`? (so each body is struck once per dash).
    func dash_struck(z) {
        var i = 0;
        var n = len(self.dash_hits);
        while (i < n) {
            if (self.dash_hits[i] == z) { return true; }
            i = i + 1;
        }
        return false;
    }

    # Close-quarters melee shove: a heavy swing that damages and knocks back every zombie in a short
    # radius. Free (no ammo) but on a short cooldown — a last resort when a zombie is on top of you.
    # Returns the number of zombies struck, or -1 if it was on cooldown.
    func melee() {
        if (self.alive == false) { return -1; }
        if (self.melee_cd > 0) { return -1; }
        self.melee_cd = self.melee_cd_max;
        var hit = 0;
        var executed = 0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= self.melee_range * self.melee_range) {
                    var m = sqrt(d2);
                    if (m < 0.01) { m = 0.01; }
                    z.hit_knockback(dx / m, dy / m, 6.0);
                    # Execute: a melee against a badly-wounded non-boss (under 30% health) finishes it
                    # outright, rewarding cleanup; otherwise it's a normal heavy swing.
                    if (z.kind != 3 and z.health <= z.max_health * 0.3) {
                        z.take_damage(z.health + 1000);
                        executed = executed + 1;
                    } else {
                        z.take_damage(self.melee_dmg);
                        z.stagger(0.4);   # a shove reliably flinches even a brute — a create-space button
                    }
                    hit = hit + 1;
                }
            }
            i = i + 1;
        }
        # Landing an execute refunds most of the melee cooldown, so cleaning up stragglers chains fast.
        if (executed > 0) { self.melee_cd = self.melee_cd_max * 0.35; }
        emit(self.node.x, self.node.y, 14, 2);
        g_shake = 1.2;
        return hit;
    }

    func take_damage(dmg) {
        # Dodge-roll invulnerability: i-frames make the survivor untouchable mid-roll.
        if (self.iframes > 0) { return; }
        # An active shield power-up soaks all incoming damage.
        if (self.buff_kind == 2 and self.buff_timer > 0) { return; }
        self.regen_timer = 0;   # taking a hit resets the out-of-combat heal delay
        g_wave_clean = false;   # a landed hit spoils a flawless-wave run (even if armor eats it)
        var d = dmg;
        # Body armor is a depletable buffer: it takes the hit first, and only the overflow past a
        # spent plate bleeds through to health (bought from the shop, key 9).
        if (self.armor > 0) {
            self.armor = self.armor - d;
            emit(self.node.x, self.node.y, 3, 0);   # sparks off the plate
            if (self.armor >= 0) { return; }        # fully absorbed
            d = 0 - self.armor;                     # remainder past the broken plate
            self.armor = 0;
        }
        self.health = self.health - d;
        if (self.health <= 0) {
            if (self.revives > 0) { self.second_wind(); return; }
            self.health = 0;
            self.alive = false;
        }
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
    var pierce = false;    # a railgun tracer flies through zombies without dealing contact damage
    var pierce_left = 0;   # piercing power-up: extra zombies this bullet can punch through and keep going
    var hit_list = [];     # zombies already struck (so a piercing bullet doesn't re-hit the same body)
    var falloff = false;   # shotgun pellets lose damage the farther they've flown (point-blank identity)

    func _ready() { g_bullets.append(self); }

    func fire(px, py, dirx, diry, speed, dmg) {
        self.node.x = px;
        self.node.y = py;
        self.vx = dirx * speed;
        self.vy = diry * speed;
        self.damage = dmg;
        self.life = self.max_life;
        self.pierce = false;
        self.pierce_left = 0;
        self.hit_list = [];
        self.falloff = false;
        self.active = true;
    }

    # This bullet's damage at its current age. Plain rounds hit flat; a shotgun pellet (falloff) hits
    # hardest fresh out of the barrel and fades toward 40% by the end of its flight — a point-blank ramp.
    func effective_damage() {
        if (self.falloff == false) { return self.damage; }
        var frac = self.life / self.max_life;   # 1.0 just-fired → 0.0 at the end of its life
        if (frac > 1.0) { frac = 1.0; }
        if (frac < 0.4) { frac = 0.4; }
        return self.damage * frac;
    }

    # True unless this bullet has already struck zombie `z` on an earlier frame (piercing bookkeeping).
    func not_hit(z) {
        var i = 0;
        var n = len(self.hit_list);
        while (i < n) {
            if (self.hit_list[i] == z) { return false; }
            i = i + 1;
        }
        return true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.node.x = self.node.x + self.vx * dt;
        self.node.y = self.node.y + self.vy * dt;
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        if (self.pierce) { return; }   # railgun tracer: no contact damage, just flies on

        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive and self.not_hit(z)) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var rr = self.hit_radius + z.radius;
                if (dx * dx + dy * dy <= rr * rr) {
                    var spd = sqrt(self.vx * self.vx + self.vy * self.vy);
                    if (spd > 0.001) { z.hit_knockback(self.vx / spd, self.vy / spd, 0.6); }
                    z.take_damage(self.effective_damage());
                    z.apply_bleed(1);   # kinetic round tears a bleeding wound
                    if (g_player != nil) {
                        g_player.hits = g_player.hits + 1;
                        if (g_player.lifesteal_active()) { g_player.heal(2.0); }   # Vampiric leech
                    }
                    self.hit_list.append(z);
                    # A piercing round spends one pierce and flies on; a normal round stops here.
                    if (self.pierce_left > 0) {
                        self.pierce_left = self.pierce_left - 1;
                        return;
                    }
                    self.active = false;
                    return;
                }
            }
            i = i + 1;
        }
        # Explosive barrels are shootable too: a hit chips their hull and pops them at zero.
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var b = g_barrels[bi];
            if (b.active) {
                var bdx = b.node.x - self.node.x;
                var bdy = b.node.y - self.node.y;
                var brr = self.hit_radius + b.radius;
                if (bdx * bdx + bdy * bdy <= brr * brr) {
                    b.take_damage(self.effective_damage());
                    if (g_player != nil) { g_player.hits = g_player.hits + 1; }
                    self.active = false;
                    return;
                }
            }
            bi = bi + 1;
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
                var d2 = dx * dx + dy * dy;
                var cr = self.blast_radius + 2.0;    # chill reaches a bit past the kill radius
                if (d2 <= cr * cr) { z.apply_slow(2.5); }
                if (d2 <= self.blast_radius * self.blast_radius) { z.take_damage(self.blast_dmg); }
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

# A pooled acid glob lobbed by a spitter zombie. Flies toward where the survivor stood when it was
# fired (so it can be side-stepped), then splashes on arrival — damaging the survivor only if they are
# still near the impact point. This is the horde's one ranged threat, so a spitter must be prioritized.
class Spit {
    var active = false;
    var vx = 0;
    var vy = 0;
    var fuse = 0;
    var splash = 2.6;
    var dmg = 12;

    func _ready() { g_spits.append(self); }

    func launch(x, y, tx, ty) {
        self.node.x = x;
        self.node.y = y;
        var dx = tx - x;
        var dy = ty - y;
        var d = sqrt(dx * dx + dy * dy);
        if (d < 0.001) { d = 0.001; }
        var spd = 24;
        self.vx = (dx / d) * spd;
        self.vy = (dy / d) * spd;
        self.fuse = d / spd;   # lands roughly where the survivor was at launch
        self.active = true;
    }

    func splat() {
        if (g_player != nil) {
            if (g_player.alive) {
                var dx = g_player.node.x - self.node.x;
                var dy = g_player.node.y - self.node.y;
                if (dx * dx + dy * dy <= self.splash * self.splash) {
                    g_player.take_damage(self.dmg);
                }
            }
        }
        emit(self.node.x, self.node.y, 8, 1);
        leave_acid(self.node.x, self.node.y);   # the glob leaves a caustic puddle where it lands
        self.active = false;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.node.x = self.node.x + self.vx * dt;
        self.node.y = self.node.y + self.vy * dt;
        self.fuse = self.fuse - dt;
        if (self.fuse <= 0) { self.splat(); }
    }
}

# Activate a dormant acid glob from the pool, launched from (x, y) toward (tx, ty).
func launch_spit(x, y, tx, ty) {
    var i = 0;
    var n = len(g_spits);
    while (i < n) {
        var s = g_spits[i];
        if (s.active == false) {
            s.launch(x, y, tx, ty);
            return;
        }
        i = i + 1;
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

# A pooled health pickup dropped by a dying zombie. Sits on the ground for a while, blinking; walk over
# it to heal. Expires if left too long. Recycled from the pool.
class Medkit {
    var active = false;
    var life = 0;
    var max_life = 12;
    var heal = 40;
    var pickup_range = 2.2;

    func _ready() { g_medkits.append(self); }

    func place(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var d2 = dx * dx + dy * dy;
        if (d2 <= self.pickup_range * self.pickup_range) {
            g_player.heal(self.heal);
            self.active = false;
        } else {
            # Magnetism: within a short radius the kit drifts toward the survivor.
            if (d2 <= 36.0) {
                var d = sqrt(d2);
                self.node.x = self.node.x + (dx / d) * 12.0 * dt;
                self.node.y = self.node.y + (dy / d) * 12.0 * dt;
            }
        }
    }
}

# Activate a dormant medkit from the pool at (x, y) — called when a zombie drops one.
func drop_medkit(x, y) {
    var i = 0;
    var n = len(g_medkits);
    while (i < n) {
        var m = g_medkits[i];
        if (m.active == false) {
            m.place(x, y);
            return;
        }
        i = i + 1;
    }
}

# A pooled ammo box dropped by a dying zombie. Walk over it (or let it drift in) to top up reserves.
class Ammo {
    var active = false;
    var life = 0;
    var max_life = 12;
    var pickup_range = 2.2;

    func _ready() { g_ammo.append(self); }

    func place(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var d2 = dx * dx + dy * dy;
        if (d2 <= self.pickup_range * self.pickup_range) {
            g_player.collect_ammo();
            self.active = false;
        } else {
            # Magnetism: within a short radius the box drifts toward the survivor.
            if (d2 <= 36.0) {
                var d = sqrt(d2);
                self.node.x = self.node.x + (dx / d) * 12.0 * dt;
                self.node.y = self.node.y + (dy / d) * 12.0 * dt;
            }
        }
    }
}

# Activate a dormant ammo box from the pool at (x, y) — called when a zombie drops one.
func drop_ammo(x, y) {
    var i = 0;
    var n = len(g_ammo);
    while (i < n) {
        var a = g_ammo[i];
        if (a.active == false) {
            a.place(x, y);
            return;
        }
        i = i + 1;
    }
}

# A pooled power-up pickup dropped rarely by a dying zombie. Walk over it to gain a timed buff:
# kind 0 = rapid fire, 1 = double damage, 2 = shield (temporary invulnerability). Blinks near expiry.
class Powerup {
    var active = false;
    var kind = 0;
    var life = 0;
    var max_life = 14;
    var pickup_range = 2.2;

    func _ready() { g_powerups.append(self); }

    func place(x, y, k) {
        self.node.x = x;
        self.node.y = y;
        self.kind = k;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var d2 = dx * dx + dy * dy;
        if (d2 <= self.pickup_range * self.pickup_range) {
            g_player.grant_powerup(self.kind);
            self.active = false;
        } else {
            # Magnetism: within a short radius the power-up drifts toward the survivor.
            if (d2 <= 36.0) {
                var d = sqrt(d2);
                self.node.x = self.node.x + (dx / d) * 12.0 * dt;
                self.node.y = self.node.y + (dy / d) * 12.0 * dt;
            }
        }
    }
}

# Activate a dormant power-up of kind k from the pool at (x, y).
func drop_powerup(x, y, k) {
    var i = 0;
    var n = len(g_powerups);
    while (i < n) {
        var p = g_powerups[i];
        if (p.active == false) {
            p.place(x, y, k);
            return;
        }
        i = i + 1;
    }
}

# A pooled supply crate: a periodic care package. Sits on the ground for a while; walk over it for a
# big refill of ammo, grenades, and health. Expires if ignored, then recycles.
class Crate {
    var active = false;
    var life = 0;
    var max_life = 20;
    var pickup_range = 2.4;

    func _ready() { g_crates.append(self); }

    func place(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        if (dx * dx + dy * dy <= self.pickup_range * self.pickup_range) {
            g_player.collect_crate();
            self.active = false;
        }
    }
}

# Activate a dormant supply crate from the pool at (x, y).
func drop_crate(x, y) {
    var i = 0;
    var n = len(g_crates);
    while (i < n) {
        var c = g_crates[i];
        if (c.active == false) {
            c.place(x, y);
            return;
        }
        i = i + 1;
    }
}

# A pooled proximity mine. Dormant until the survivor deploys it; arms after a short delay (so you
# don't blow yourself up placing it), then detonates the moment a live zombie steps within trigger
# range — a heavy blast that damages, knocks back, and chills everything in the blast radius.
class Mine {
    var active = false;
    var armed = false;
    var arm_delay = 0;
    var trigger_range = 3.0;
    var blast_radius = 6.0;
    var blast_dmg = 120;

    func _ready() { g_mines.append(self); }

    func arm(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.arm_delay = 0.6;   # brief safety fuse before it can trigger
        self.armed = false;
        self.active = true;
    }

    func detonate() {
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= self.blast_radius * self.blast_radius) {
                    var m = sqrt(d2);
                    if (m < 0.01) { m = 0.01; }
                    z.hit_knockback(dx / m, dy / m, 5.0);
                    z.apply_slow(2.0);
                    z.take_damage(self.blast_dmg);
                }
            }
            i = i + 1;
        }
        emit(self.node.x, self.node.y, 28, 1);
        g_shake = g_shake + 2.2;
        if (g_shake > 3.0) { g_shake = 3.0; }
        self.active = false;
        self.armed = false;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        if (self.arm_delay > 0) {
            self.arm_delay = self.arm_delay - dt;
            if (self.arm_delay <= 0) { self.armed = true; }
            return;
        }
        # Once armed, detonate if any live zombie is within trigger range.
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= self.trigger_range * self.trigger_range) {
                    self.detonate();
                    return;
                }
            }
            i = i + 1;
        }
    }
}

# A pooled auto-turret sentry. Dormant until the survivor deploys it; then it auto-fires a hitscan bolt
# at the nearest live zombie in range on a cadence, for a limited lifetime, before powering down. A
# stationary ally that thins a lane while the survivor handles another.
class Sentry {
    var active = false;
    var life = 0;
    var max_life = 12;
    var fire_cd = 0;
    var fire_rate = 3.0;     # bolts per second
    var range = 16.0;
    var damage = 22;
    var ammo = 25;           # limited magazine — burns out fast against a dense pack
    var ammo_max = 25;

    func _ready() { g_sentries.append(self); }

    func deploy(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.fire_cd = 0;
        self.ammo = self.ammo_max;
        self.active = true;
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        self.fire_cd = self.fire_cd - dt;
        if (self.fire_cd > 0) { return; }
        # Acquire the nearest live zombie in range and shoot it.
        var best = self.range * self.range;
        var target = nil;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= best) { best = d2; target = z; }
            }
            i = i + 1;
        }
        if (target != nil) {
            target.take_damage(self.damage);
            emit(target.node.x, target.node.y, 3, 0);   # impact sparks on the target
            self.fire_cd = 1.0 / self.fire_rate;
            self.ammo = self.ammo - 1;                   # spend a bolt; it dies when the magazine is dry
            if (self.ammo <= 0) {
                self.active = false;
                emit(self.node.x, self.node.y, 8, 0);    # spark-out puff when the sentry runs dry
            }
        }
    }
}

# A pooled molotov fire patch. Dormant until a molotov lands; then it burns for a few seconds,
# re-igniting any zombie standing inside its radius (the burn status deals the actual damage).
class FirePool {
    var active = false;
    var life = 0;
    var max_life = 5.0;
    var radius = 5.0;
    var burn_dps = 18;
    var puff = 0;   # timer for occasional flame particles

    func _ready() { g_fires.append(self); }

    func ignite_ground(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.puff = 0;
        self.active = true;
        emit(x, y, 18, 1);
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        # Keep every zombie in the patch alight (topping up burn_timer so they cook while they stand in it).
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                # A zombie in the flames both cooks (burn DoT) and stumbles (a brief slow), so a molotov
                # is area denial and crowd control — the fire holds a lane, not just chips health.
                if (dx * dx + dy * dy <= self.radius * self.radius) {
                    z.ignite(1.0, self.burn_dps);
                    z.apply_slow(0.5);
                }
            }
            i = i + 1;
        }
        self.puff = self.puff - dt;
        if (self.puff <= 0) { self.puff = 0.3; emit(self.node.x, self.node.y, 3, 1); }
    }
}

# A pooled acid puddle left where a spitter's glob lands: caustic ground that eats at the survivor while
# they stand in it, then dries up. Unlike molotov fire (which burns zombies), this is an enemy hazard —
# it punishes the survivor for holding a spot a spitter can reach, adding spatial pressure to spitters.
class AcidPool {
    var active = false;
    var life = 0;
    var max_life = 4.5;
    var radius = 3.2;
    var dps = 14;
    var tick = 0;     # accumulator so the burn lands in periodic ticks, not every frame
    var puff = 0;

    func _ready() { g_acid.append(self); }

    func splat_at(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.life = self.max_life;
        self.tick = 0;
        self.puff = 0;
        self.active = true;
        emit(x, y, 12, 1);
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.active = false; return; }
        self.tick = self.tick - dt;
        if (self.tick <= 0) {
            self.tick = 0.35;
            if (g_player != nil) {
                if (g_player.alive) {
                    var dx = g_player.node.x - self.node.x;
                    var dy = g_player.node.y - self.node.y;
                    if (dx * dx + dy * dy <= self.radius * self.radius) {
                        g_player.take_damage(self.dps * 0.35);
                    }
                }
            }
        }
        self.puff = self.puff - dt;
        if (self.puff <= 0) { self.puff = 0.4; emit(self.node.x, self.node.y, 2, 1); }
    }
}

# Activate a dormant acid puddle from the pool at (x, y).
func leave_acid(x, y) {
    var i = 0;
    var n = len(g_acid);
    while (i < n) {
        var a = g_acid[i];
        if (a.active == false) {
            a.splat_at(x, y);
            return;
        }
        i = i + 1;
    }
}

# A pooled explosive barrel scattered around the arena. Shoot it to pop it: a hefty blast that damages,
# knocks back and ignites every zombie nearby, and chain-reacts to other barrels in range. A one-shot
# environmental trap the survivor lures the horde onto.
class Barrel {
    var active = false;
    var hp = 30;
    var radius = 1.4;         # hittable body radius
    var blast_radius = 7.0;
    var blast_dmg = 90;

    func _ready() { g_barrels.append(self); }

    func place(x, y) {
        self.node.x = x;
        self.node.y = y;
        self.hp = 30;
        self.active = true;
    }

    func take_damage(dmg) {
        if (self.active == false) { return; }
        self.hp = self.hp - dmg;
        if (self.hp <= 0) { self.explode(); }
    }

    func explode() {
        self.active = false;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= self.blast_radius * self.blast_radius) {
                    var m = sqrt(d2);
                    if (m < 0.01) { m = 0.01; }
                    z.hit_knockback(dx / m, dy / m, 6.0);
                    z.take_damage(self.blast_dmg);
                    z.ignite(2.5, 12);
                }
            }
            i = i + 1;
        }
        # Chain-react to other barrels in range (self is already inactive, so no infinite loop).
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var b = g_barrels[bi];
            if (b.active and b != self) {
                var bx = b.node.x - self.node.x;
                var by = b.node.y - self.node.y;
                if (bx * bx + by * by <= self.blast_radius * self.blast_radius) { b.take_damage(999); }
            }
            bi = bi + 1;
        }
        emit(self.node.x, self.node.y, 30, 1);
        g_shake = g_shake + 2.5;
        if (g_shake > 3.0) { g_shake = 3.0; }
    }
}

# A pooled zombie. Dormant (alive == false) until the Director spawns it into a wave; then it walks at
# the survivor and bites on a cooldown. Killed by bullets; on death it awards score and goes dormant
# so the Director can recycle it next wave.
class Zombie {
    var alive = false;
    var kind = 0;          # 0 walker, 1 runner, 2 brute, 3 boss, 4 exploder, 5 spitter, 6 splitter,
                           #   7 summoner, 8 armored, 9 leaper
    var spawn_wave = 1;    # wave this zombie was spawned in (used to scale its splitlings)
    var health = 30;
    var max_health = 30;
    var speed = 15;
    var damage = 6;
    var radius = 1.0;      # body radius (bullet hit test + draw size)
    var attack_range = 1.2;
    var score_value = 10;
    var cooldown = 0;
    var slam_cd = 0;       # boss (kind 3) ground-slam special-attack timer
    var enraged = false;   # boss (kind 3): flips true when badly wounded — faster, slams twice as often
    var elite = false;     # "champion" modifier: much tankier, faster, worth far more
    var slow_timer = 0;    # while > 0 the zombie is chilled and crawls at reduced speed
    var burn_timer = 0;    # while > 0 the zombie is on fire, taking damage over time
    var burn_dps = 0;      # fire damage per second while burning
    var burn_tick = 0;     # accumulator so burn damage lands in periodic ticks, not every frame
    var summon_cd = 0;     # summoner (kind 7) reinforcement timer
    var summon_budget = 0; # summoner: remaining reinforcements it may call before it's spent
    var shield = 0;        # armored zombie (kind 8): damage pool that must be broken before health
    var leap_cd = 0;       # leaper (kind 9): cooldown before it can pounce again
    var leaping = 0;       # leaper: seconds remaining in the current pounce (flies along leap vector)
    var leap_vx = 0;       # leaper: stored pounce velocity locked in at the start of the lunge
    var leap_vy = 0;
    var bleed_stacks = 0;  # laceration stacks from kinetic rounds — each ticks damage over time
    var bleed_timer = 0;   # while > 0 the wound is open and bleeding; refreshed by fresh hits
    var bleed_tick = 0;    # accumulator so bleed damage lands in periodic ticks, not every frame
    var stagger_timer = 0; # brief flinch: a heavy single hit freezes the zombie where it stands
    var stagger_cd = 0;    # cooldown after a flinch so it can't be perpetually stun-locked
    var frenzy_timer = 0;  # while > 0 the zombie is whipped into a screamer's frenzy — moves faster

    func _ready() { g_zombies.append(self); }

    # Set this zombie alight for `dur` seconds at `dps` damage/second (strongest ignition wins).
    func ignite(dur, dps) {
        if (self.alive == false) { return; }
        if (dur > self.burn_timer) { self.burn_timer = dur; }
        if (dps > self.burn_dps) { self.burn_dps = dps; }
    }

    # Chill this zombie (e.g. caught in a grenade blast): it crawls slowly for `dur` seconds.
    func apply_slow(dur) {
        if (dur > self.slow_timer) { self.slow_timer = dur; }
    }

    # Whip this zombie into a frenzy (a screamer's shriek): it surges faster for `dur` seconds.
    func apply_frenzy(dur) {
        if (self.alive == false) { return; }
        if (dur > self.frenzy_timer) { self.frenzy_timer = dur; }
    }

    # Directly flinch this zombie for `dur` seconds — an ungated stagger used by reliable interrupts
    # like the melee shove (the take_damage stagger is threshold + cooldown gated). The boss is immune.
    func stagger(dur) {
        if (self.alive == false) { return; }
        if (self.kind == 3) { return; }
        if (dur > self.stagger_timer) { self.stagger_timer = dur; }
    }

    # Open a bleeding wound: kinetic rounds add laceration stacks that tick damage over time.
    # Stacks build with sustained fire (rewarding staying on-target) and cap so it can't run away.
    func apply_bleed(n) {
        if (self.alive == false) { return; }
        self.bleed_stacks = self.bleed_stacks + n;
        if (self.bleed_stacks > 5) { self.bleed_stacks = 5; }   # cap the stack
        self.bleed_timer = 3.0;                                 # fresh hits keep the wound open
    }

    # Shove this zombie along (dirx, diry) when shot. Heavy bodies (brutes/bosses) mostly resist it.
    func hit_knockback(dirx, diry, amount) {
        var k = amount;
        if (self.radius > 1.5) { k = amount * 0.25; }
        self.node.x = self.node.x + dirx * k;
        self.node.y = self.node.y + diry * k;
    }

    # Call `cnt` reinforcement walkers: revive that many dormant pool zombies as kind-0 walkers around
    # this body. Used by the summoner (kind 7) on a timer. Returns how many it actually spawned.
    func summon(cnt) {
        var made = 0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n and made < cnt) {
            var z = g_zombies[i];
            if (z.alive == false and z != self) {
                var ang = randf_range(0, 6.2831853);
                z.spawn(self.node.x + cos(ang) * 2.0, self.node.y + sin(ang) * 2.0, 0, self.spawn_wave);
                made = made + 1;
            }
            i = i + 1;
        }
        return made;
    }

    # Burst into `cnt` fast runners: revive that many dormant pool zombies as kind-1 runners around
    # this body. Used by the splitter (kind 6) on death. Bounded by the free slots in the pool.
    func split_off(cnt) {
        var made = 0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n and made < cnt) {
            var z = g_zombies[i];
            if (z.alive == false and z != self) {
                var ang = randf_range(0, 6.2831853);
                z.spawn(self.node.x + cos(ang) * 1.6, self.node.y + sin(ang) * 1.6, 1, self.spawn_wave);
                made = made + 1;
            }
            i = i + 1;
        }
        return made;
    }

    # Overkill gib: a decisive killing blow bursts this body in a small shockwave that chips every
    # nearby zombie, so a heavy hit (railgun, crit) landed on a weakened pack chains through it.
    func overkill_burst() {
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive and z != self) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= 16.0) { z.take_damage(25); }   # radius 4
            }
            i = i + 1;
        }
        emit(self.node.x, self.node.y, 14, 1);   # gib burst
    }

    # Crown this zombie an elite: a tankier, faster, high-value champion that always drops a medkit.
    func make_elite() {
        self.elite = true;
        self.health = self.health * 2.5;
        self.max_health = self.health;
        self.speed = self.speed * 1.15;
        if (self.speed > 30) { self.speed = 30; }
        self.score_value = self.score_value * 3;
    }

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
        self.elite = false;
        self.slow_timer = 0;
        self.alive = true;
    }

    # Director call: revive as kind k with wave-w-scaled stats.
    #   0 walker - baseline    1 runner - fast/fragile
    #   2 brute  - slow/tanky/big/hard-hitting    3 boss - huge, every 5th wave
    func spawn(x, y, k, w) {
        self.node.x = x;
        self.node.y = y;
        self.kind = k;
        self.spawn_wave = w;
        self.cooldown = 0;
        self.slam_cd = 3.0;    # a boss's first ground slam lands a few seconds in
        self.enraged = false;
        self.elite = false;
        self.slow_timer = 0;
        self.burn_timer = 0;
        self.burn_dps = 0;
        self.burn_tick = 0;
        self.summon_cd = 4.0;    # a summoner's first reinforcement lands a few seconds in
        self.summon_budget = 0;
        if (k == 7) { self.summon_budget = 6; }
        self.shield = 0;
        if (k == 8) { self.shield = 50 + w * 8; }   # armored zombie's damage-absorbing shield
        self.leap_cd = 1.5;    # a leaper's first pounce comes a beat after it appears
        self.leaping = 0;
        self.leap_vx = 0;
        self.leap_vy = 0;
        self.bleed_stacks = 0;
        self.bleed_timer = 0;
        self.bleed_tick = 0;
        self.stagger_timer = 0;
        self.stagger_cd = 0;
        self.frenzy_timer = 0;
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
                    if (k == 4) {
                        self.health = 20 + w * 5;
                        self.speed = 18 + w;
                        self.damage = 3;
                        self.radius = 1.1;
                        self.attack_range = 1.2;
                        self.score_value = 15;
                    } else {
                        if (k == 5) {
                            # Spitter: keeps its distance (large attack_range) and lobs acid.
                            self.health = 30 + w * 6;
                            self.speed = 11 + w;
                            self.damage = 0;
                            self.radius = 1.0;
                            self.attack_range = 13;
                            self.score_value = 18;
                        } else {
                            if (k == 6) {
                                # Splitter: a bloated mid-tier that bursts into two fast runners on death.
                                self.health = 45 + w * 10;
                                self.speed = 10 + w;
                                self.damage = 8;
                                self.radius = 1.4;
                                self.attack_range = 1.5;
                                self.score_value = 22;
                            } else {
                                if (k == 7) {
                                    # Summoner: slow, tanky support that periodically calls reinforcements.
                                    self.health = 90 + w * 16;
                                    self.speed = 7;
                                    self.damage = 5;
                                    self.radius = 1.5;
                                    self.attack_range = 1.4;
                                    self.score_value = 40;
                                } else {
                                    if (k == 8) {
                                        # Armored: modest health behind a heavy damage-absorbing shield.
                                        self.health = 30 + w * 6;
                                        self.speed = 10 + w;
                                        self.damage = 7;
                                        self.radius = 1.2;
                                        self.attack_range = 1.3;
                                        self.score_value = 28;
                                    } else {
                                        if (k == 9) {
                                            # Leaper: light and quick, closes the gap in sudden pounces.
                                            self.health = 22 + w * 5;
                                            self.speed = 12;
                                            self.damage = 8;
                                            self.radius = 1.0;
                                            self.attack_range = 1.2;
                                            self.score_value = 16;
                                        } else {
                                            if (k == 10) {
                                                # Bloater: fat, slow, tanky — bursts into a toxic cloud
                                                # on death, so it's best popped at a distance.
                                                self.health = 70 + w * 14;
                                                self.speed = 6;
                                                self.damage = 10;
                                                self.radius = 1.7;
                                                self.attack_range = 1.7;
                                                self.score_value = 30;
                                            } else {
                                            if (k == 11) {
                                                # Screamer: fragile back-line support that periodically
                                                # shrieks, whipping nearby zombies into a speed frenzy.
                                                self.health = 30 + w * 6;
                                                self.speed = 9 + w;
                                                self.damage = 4;
                                                self.radius = 1.1;
                                                self.attack_range = 1.3;
                                                self.score_value = 35;
                                            } else {
                                            if (k == 12) {
                                                # Healer: a back-line medic that periodically knits the
                                                # wounds of nearby zombies. Fragile but force-multiplying,
                                                # so it's a priority kill before it undoes your damage.
                                                self.health = 34 + w * 7;
                                                self.speed = 8 + w;
                                                self.damage = 4;
                                                self.radius = 1.15;
                                                self.attack_range = 1.3;
                                                self.score_value = 40;
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
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        # Wave mutator reshapes the whole horde: feral bodies are faster, hulking ones tougher (frenzy
        # only affects how many spawn, handled by the director). Applied before the speed clamp so a
        # feral runner still tops out at the hard speed cap.
        if (g_mutator == 1) { self.speed = self.speed * 1.35; }
        if (g_mutator == 2) { self.health = self.health * 1.5; }
        if (self.speed > 30) { self.speed = 30; }
        self.max_health = self.health;
    }

    func take_damage(dmg) {
        if (self.alive == false) { return; }
        var d = dmg;
        if (self.slow_timer > 0) { d = dmg * 1.5; }  # chilled bodies are brittle — shatter bonus
        # An armored zombie's shield soaks damage first; only the overflow past a broken shield bleeds
        # through to its health, so it must be broken down before it can be killed.
        if (self.shield > 0) {
            self.shield = self.shield - d;
            emit(self.node.x, self.node.y, 3, 0); # shield sparks
            if (self.shield >= 0) { return; }     # fully absorbed
            d = 0 - self.shield;                  # remainder past the broken shield
            self.shield = 0;
        }
        self.health = self.health - d;
        emit(self.node.x, self.node.y, 3, 0); # hit sparks
        if (self.health <= 0) {
            self.health = 0;
            self.alive = false;
            g_kills = g_kills + 1;
            g_combo = g_combo + 1;
            g_combo_timer = 0;
            g_mult = 1 + int(g_combo / 5);
            if (g_mult > 5) { g_mult = 5; }
            g_score = g_score + self.score_value * g_mult;
            # Salvage scales with the streak multiplier: a base cut per kill, plus a combo bonus that
            # grows as the multiplier climbs (nothing extra at ×1, up to +200% at ×5). Killing fast pays.
            var salvage = 5 + int(self.score_value / 4);
            g_cash = g_cash + salvage + int(salvage * (g_mult - 1) / 2);
            # Killstreak milestones: every 10th unbroken kill pays a cash bounty, and every 20th also
            # patches the survivor up a little — rewarding sustained aggression before the combo decays.
            if (g_combo % 10 == 0) {
                g_cash = g_cash + 15;
                g_streak_rewards = g_streak_rewards + 1;
                if (g_combo % 20 == 0 and g_player != nil) { g_player.heal(8.0); }
                emit(self.node.x, self.node.y, 14, 1);   # milestone flourish
            }
            if (g_player != nil) { g_player.on_kill(); } # charges the ultimate + milestone rewards
            emit(self.node.x, self.node.y, 10, 1); # blood burst on death
            var s = 0.5;
            if (self.kind == 2) { s = 1.0; }
            if (self.kind == 3) { s = 2.5; }
            # An exploder detonates on death: area-of-effect damage to a nearby
            # survivor, so it must be shot from a distance.
            if (self.kind == 4) {
                s = 1.5;
                if (g_player != nil) {
                    if (g_player.alive) {
                        var ex = g_player.node.x - self.node.x;
                        var ey = g_player.node.y - self.node.y;
                        if (ex * ex + ey * ey <= 25.0) { g_player.take_damage(35); }
                    }
                }
                # The blast also catches nearby zombies (but not other exploders, to bound the chain),
                # so an exploder shot inside a pack takes the pack with it.
                var bi = 0;
                var bn = len(g_zombies);
                while (bi < bn) {
                    var oz = g_zombies[bi];
                    if (oz.alive and oz.kind != 4) {
                        var ozx = oz.node.x - self.node.x;
                        var ozy = oz.node.y - self.node.y;
                        # The incendiary blast burns survivors of the initial hit.
                        if (ozx * ozx + ozy * ozy <= 25.0) { oz.take_damage(40); oz.ignite(3.0, 10); }
                    }
                    bi = bi + 1;
                }
                emit(self.node.x, self.node.y, 20, 1); # blast burst
            }
            # A splitter bursts into two fast runners at its position.
            if (self.kind == 6) {
                self.split_off(2);
                emit(self.node.x, self.node.y, 12, 1);
            }
            # A bloater ruptures on death into a lingering toxic cloud (reusing the acid hazard), so a
            # careless close-range kill leaves you standing in poison.
            if (self.kind == 10) {
                leave_acid(self.node.x, self.node.y);
                emit(self.node.x, self.node.y, 20, 1);
            }
            # Overkill: if the killing hit alone dwarfed this body's full health (and it isn't an
            # exploder or boss, which have their own death behaviour), it gibs in a chain shockwave.
            if (d >= self.max_health * 1.5 and self.kind != 4 and self.kind != 3) {
                self.overkill_burst();
            }
            # Frost shatter: a chilled body killed while frozen bursts into an icy cloud that chills
            # nearby zombies — chaining the cryo-nova / grenade-slow into a spreading freeze.
            if (self.slow_timer > 0) {
                var ci = 0;
                var cn = len(g_zombies);
                while (ci < cn) {
                    var cz = g_zombies[ci];
                    if (cz.alive and cz != self) {
                        var cdx = cz.node.x - self.node.x;
                        var cdy = cz.node.y - self.node.y;
                        if (cdx * cdx + cdy * cdy <= 20.25) { cz.apply_slow(1.5); }   # radius 4.5
                    }
                    ci = ci + 1;
                }
                emit(self.node.x, self.node.y, 12, 0);
            }
            g_shake = g_shake + s;
            if (g_shake > 3.0) { g_shake = 3.0; }
            # A slain zombie sometimes drops a medkit; an elite always does, plus an extra flourish.
            if (self.elite) {
                drop_medkit(self.node.x, self.node.y);
                emit(self.node.x, self.node.y, 16, 1);
            } else {
                if (randf() < 0.12) { drop_medkit(self.node.x, self.node.y); }
            }
            # Rarely it drops a power-up instead (rapid-fire, damage, shield, piercing, cryo, vampiric,
            # overflow).
            if (randf() < 0.05) {
                var pk = int(randf_range(0, 7));
                if (pk > 6) { pk = 6; }
                drop_powerup(self.node.x, self.node.y, pk);
            }
            # And sometimes an ammo box, to keep reserves topped up between crates.
            if (randf() < 0.10) { drop_ammo(self.node.x, self.node.y); }
        }
        # Stagger: a heavy single blow (at least 40% of full health) that doesn't kill briefly roots
        # the zombie where it stands — a reward for big hits (shotgun point-blank, railgun, grenades,
        # crits). A cooldown stops rapid fire from stun-locking it, and the boss is immune.
        if (self.alive and self.kind != 3 and self.stagger_cd <= 0 and d >= self.max_health * 0.4) {
            self.stagger_timer = 0.35;
            self.stagger_cd = 1.2;
            emit(self.node.x, self.node.y, 3, 0);
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
        # Burning status: fire deals damage in periodic ticks (bounded so it doesn't spam per frame).
        if (self.burn_timer > 0) {
            self.burn_timer = self.burn_timer - dt;
            self.burn_tick = self.burn_tick - dt;
            if (self.burn_tick <= 0) {
                self.burn_tick = 0.25;
                self.take_damage(self.burn_dps * 0.25);
                if (self.alive == false) { return; }   # burned to death this tick
            }
            if (self.burn_timer <= 0) { self.burn_timer = 0; self.burn_dps = 0; }
        }
        # Bleeding status: open wounds from kinetic rounds tick damage in periodic bursts that scale
        # with the stack count, so a body raked with fire keeps hemorrhaging even after you stop shooting.
        if (self.bleed_stacks > 0) {
            self.bleed_timer = self.bleed_timer - dt;
            self.bleed_tick = self.bleed_tick - dt;
            if (self.bleed_tick <= 0) {
                self.bleed_tick = 0.4;
                self.take_damage(self.bleed_stacks * 1.0);   # 1 dmg per stack per tick
                if (self.alive == false) { return; }         # bled out this tick
            }
            if (self.bleed_timer <= 0) { self.bleed_timer = 0; self.bleed_stacks = 0; }
        }
        # Summoner (kind 7): periodically calls a reinforcement until its budget runs out.
        if (self.kind == 7 and self.summon_budget > 0) {
            self.summon_cd = self.summon_cd - dt;
            if (self.summon_cd <= 0) {
                self.summon_cd = 4.0;
                if (self.summon(1) > 0) {
                    self.summon_budget = self.summon_budget - 1;
                    emit(self.node.x, self.node.y, 8, 1);
                }
            }
        }
        # Screamer (kind 11): on a cooldown it shrieks, whipping every nearby zombie into a speed
        # frenzy. It's fragile, so silencing it early keeps the horde from surging — a priority target.
        if (self.kind == 11) {
            self.cooldown = self.cooldown - dt;
            if (self.cooldown <= 0) {
                self.cooldown = 5.0;
                var si = 0;
                var sn = len(g_zombies);
                while (si < sn) {
                    var oz = g_zombies[si];
                    if (oz.alive and oz != self) {
                        var sdx = oz.node.x - self.node.x;
                        var sdy = oz.node.y - self.node.y;
                        if (sdx * sdx + sdy * sdy <= 225.0) { oz.apply_frenzy(3.0); }  # radius 15
                    }
                    si = si + 1;
                }
                emit(self.node.x, self.node.y, 18, 1);   # shriek burst
            }
        }
        # Healer (kind 12): on a cooldown it mends every wounded zombie in a radius, knitting a chunk of
        # health back (never past their max). Force-multiplying but fragile, so cull it before it undoes
        # your work. It never heals itself, keeping it a body you can burn down.
        if (self.kind == 12) {
            self.cooldown = self.cooldown - dt;
            if (self.cooldown <= 0) {
                self.cooldown = 4.0;
                var mended = 0;
                var hi = 0;
                var hn = len(g_zombies);
                while (hi < hn) {
                    var hz = g_zombies[hi];
                    if (hz.alive and hz != self and hz.health < hz.max_health) {
                        var hdx = hz.node.x - self.node.x;
                        var hdy = hz.node.y - self.node.y;
                        if (hdx * hdx + hdy * hdy <= 196.0) {   # radius 14
                            hz.health = hz.health + hz.max_health * 0.25;
                            if (hz.health > hz.max_health) { hz.health = hz.max_health; }
                            mended = mended + 1;
                        }
                    }
                    hi = hi + 1;
                }
                if (mended > 0) { emit(self.node.x, self.node.y, 10, 0); }   # heal pulse (sparks)
            }
        }
        # Chill status: while slowed, the zombie crawls at 40% speed.
        self.slow_timer = self.slow_timer - dt;
        if (self.slow_timer < 0) { self.slow_timer = 0; }
        # Stagger status: a flinch roots the zombie completely for a fraction of a second, and its
        # cooldown ticks down so it can be staggered again once the window has passed.
        self.stagger_cd = self.stagger_cd - dt;
        if (self.stagger_cd < 0) { self.stagger_cd = 0; }
        self.stagger_timer = self.stagger_timer - dt;
        if (self.stagger_timer < 0) { self.stagger_timer = 0; }
        # Frenzy status: a screamer's shriek surges nearby zombies to a burst of speed for a few seconds.
        self.frenzy_timer = self.frenzy_timer - dt;
        if (self.frenzy_timer < 0) { self.frenzy_timer = 0; }
        var sm = 1.0;
        if (self.slow_timer > 0) { sm = 0.4; }
        if (self.frenzy_timer > 0) { sm = sm * 1.6; }   # whipped into a frenzy — surges faster
        if (self.stagger_timer > 0) { sm = 0.0; }   # flinching — rooted where it stands
        if (self.kind == 3) {
            # Boss enrage: once badly wounded (below 35% health) it flies into a rage for a climactic
            # second phase — permanently faster, and slamming twice as often. Triggers once.
            if (self.enraged == false and self.health <= self.max_health * 0.35) {
                self.enraged = true;
                self.speed = self.speed * 1.7;
                self.summon_cd = 4.0;      # first reinforcement wave lands a few seconds into the rage
                self.summon_budget = 4;    # calls the horde a handful of times before it's spent
                emit(self.node.x, self.node.y, 30, 1);   # rage burst
                g_shake = 3.0;
            }
            # Enraged second phase: the boss periodically bellows and calls the horde, spawning a pair
            # of runners until its reinforcement budget runs dry — the climax becomes a real scramble.
            if (self.enraged and self.summon_budget > 0) {
                self.summon_cd = self.summon_cd - dt;
                if (self.summon_cd <= 0) {
                    self.summon_cd = 6.0;
                    if (self.split_off(2) > 0) {
                        self.summon_budget = self.summon_budget - 1;
                        emit(self.node.x, self.node.y, 8, 1);   # summon burst
                    }
                }
            }
            # Boss ground slam: a periodic radial shockwave that hammers a nearby survivor, so
            # standing next to the boss is punished even though it lumbers slowly. Still bites below.
            var slam_gap = 4.0;
            if (self.enraged) { slam_gap = 2.0; }
            self.slam_cd = self.slam_cd - dt;
            if (self.slam_cd <= 0) {
                self.slam_cd = slam_gap;
                if (dist <= 10.0) { g_player.take_damage(25); }
                emit(self.node.x, self.node.y, 28, 1); # shockwave burst
                g_shake = g_shake + 2.5;
                if (g_shake > 3.0) { g_shake = 3.0; }
            }
        }
        if (self.kind == 5) {
            # Spitter: advance only until inside spitting range, then hold and lob acid on a cooldown.
            if (dist > self.attack_range) {
                self.node.x = self.node.x + (dx / dist) * self.speed * aggro * sm * dt;
                self.node.y = self.node.y + (dy / dist) * self.speed * aggro * sm * dt;
            }
            self.cooldown = self.cooldown - dt;
            if (dist <= self.attack_range and self.cooldown <= 0) {
                launch_spit(self.node.x, self.node.y, g_player.node.x, g_player.node.y);
                self.cooldown = 2.2;
            }
            return;
        }
        # Leaper (kind 9): between pounces it walks; on a ready cooldown at mid-range it winds up a
        # sudden lunge — a fast burst toward the survivor that closes distance far quicker than a walk.
        if (self.kind == 9) {
            if (self.leaping > 0) {
                self.leaping = self.leaping - dt;
                self.node.x = self.node.x + self.leap_vx * sm * dt;
                self.node.y = self.node.y + self.leap_vy * sm * dt;
                self.cooldown = self.cooldown - dt;
                if (dist <= self.attack_range and self.cooldown <= 0) {
                    g_player.take_damage(self.damage * aggro);
                    self.cooldown = 1.0;
                }
                return;
            }
            self.leap_cd = self.leap_cd - dt;
            if (self.leap_cd <= 0 and self.slow_timer <= 0 and dist > self.attack_range and dist < 16.0) {
                var ls = 32.0;   # pounce burst speed
                self.leap_vx = (dx / dist) * ls;
                self.leap_vy = (dy / dist) * ls;
                self.leaping = 0.32;
                self.leap_cd = 3.0;
                emit(self.node.x, self.node.y, 6, 0);   # dust puff on take-off
                return;
            }
        }
        # Summoner (kind 7): a back-line necromancer that keeps its distance — it backs away when the
        # survivor closes in rather than shambling into melee, holding a comfortable range from which it
        # keeps calling reinforcements. It must be chased down or picked off before it floods the field.
        if (self.kind == 7) {
            var keep = 14.0;
            if (dist < keep and dist > 0.01) {
                self.node.x = self.node.x - (dx / dist) * self.speed * sm * dt;   # retreat
                self.node.y = self.node.y - (dy / dist) * self.speed * sm * dt;
            } else {
                if (dist > keep + 4.0) {
                    self.node.x = self.node.x + (dx / dist) * self.speed * 0.5 * sm * dt; # drift in slowly
                    self.node.y = self.node.y + (dy / dist) * self.speed * 0.5 * sm * dt;
                }
            }
            return;
        }
        if (dist > self.attack_range) {
            self.node.x = self.node.x + (dx / dist) * self.speed * aggro * sm * dt;
            self.node.y = self.node.y + (dy / dist) * self.speed * aggro * sm * dt;
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
    var bonus_wave = 0;     # highest wave already awarded a clear bonus (avoids double-paying)
    var last_bonus = 0;     # the most recent clear bonus (for the HUD banner)
    var last_clean = false; # whether the most recent cleared wave was flawless (for the HUD banner)

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
        g_wave_clean = true;   # a fresh wave starts flawless until the survivor takes a hit
        # Roll this wave's mutator (from wave 3 on): a random modifier that reshapes the whole horde.
        g_mutator = 0;
        if (w >= 3) { g_mutator = int(randf_range(1, 4)); }
        if (g_mutator > 3) { g_mutator = 3; }
        var pool = len(g_zombies);
        var count = self.base + w * 2;
        # Frenzy mutator throws a bigger horde at the survivor.
        if (g_mutator == 3) { count = count + int(count / 2); }
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
                    if (i % 10 == 0 and w >= 8) {
                        k = 8;
                    } else {
                    if (i % 9 == 0 and w >= 7) {
                        k = 7;
                    } else {
                    if (i % 8 == 0 and w >= 6) {
                        k = 6;
                    } else {
                    if (i % 7 == 0 and w >= 4) {
                        k = 4;
                    } else {
                    if (i % 4 == 0 and w >= 5) {
                        k = 9;
                    } else {
                    if (i % 11 == 0 and w >= 6) {
                        k = 10;
                    } else {
                    if (i % 13 == 0 and w >= 7) {
                        k = 11;
                    } else {
                    if (i % 17 == 0 and w >= 9) {
                        k = 12;
                    } else {
                        if (i % 6 == 0 and w >= 5) {
                            k = 5;
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
                    }
                    }
                    }
                    }
                    }
                    }
                    }
                    }
                }
                var ang = 6.28318530718 * i / count;
                var r = 34 + randf_range(0, 10);
                z.spawn(cx + cos(ang) * r, cy + sin(ang) * r, k, w);
                # From wave 2, a non-boss zombie is occasionally crowned an elite champion.
                if (k != 3 and w >= 2 and randf() < 0.08) { z.make_elite(); }
            }
            i = i + 1;
        }
    }

    func _process(dt) {
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        if (self.alive_count() == 0) {
            # Clearing a wave (once one has actually started) awards a score bonus that scales with it.
            if (self.wave >= 1 and self.bonus_wave < self.wave) {
                self.bonus_wave = self.wave;
                self.last_bonus = self.wave * 50;
                g_score = g_score + self.last_bonus;
                # Flawless wave: cleared without taking a single hit. Doubles the clear bonus, pays a
                # cash reward, and patches the survivor up a little — rewarding aggressive, clean play.
                self.last_clean = g_wave_clean;
                if (g_wave_clean) {
                    var fb = self.wave * 50;
                    g_score = g_score + fb;
                    self.last_bonus = self.last_bonus + fb;
                    g_cash = g_cash + 25;
                    if (g_player != nil) { g_player.heal(10.0); }
                }
            }
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

// True when a run's (wave, score) beats the stored best — score is the primary key, wave breaks ties.
inline bool beatsBest(int wave, int score, int bestWave, int bestScore) {
    return score > bestScore || (score == bestScore && wave > bestWave);
}

// Grade a finished run S/A/B/C/D from how far it got (wave), how much it cleared (kills), and how
// cleanly it shot (accuracy %). A single composite score keeps the letter meaningful across playstyles.
// Returns a 0-4 tier: 0=D, 1=C, 2=B, 3=A, 4=S. runRankLetter() maps the tier to its letter.
inline int runRank(int wave, int kills, int accuracyPct) {
    if (accuracyPct < 0) accuracyPct = 0;
    if (accuracyPct > 100) accuracyPct = 100;
    const int points = wave * 120 + kills * 6 + accuracyPct * 4;
    if (points >= 1900) return 4; // S
    if (points >= 1300) return 3; // A
    if (points >= 800) return 2;  // B
    if (points >= 400) return 1;  // C
    return 0;                     // D
}

inline const char* runRankLetter(int tier) {
    if (tier <= 0) return "D";
    if (tier == 1) return "C";
    if (tier == 2) return "B";
    if (tier == 3) return "A";
    return "S";
}

// Pool / scene sizes. Public so the app and tests agree on how many sprites to expect.
constexpr int kBulletPool = 64;
constexpr int kZombiePool = 40;
constexpr int kParticlePool = 90;
constexpr int kGrenadePool = 8;
constexpr int kMedkitPool = 12;
constexpr int kAmmoPool = 8;
constexpr int kSpitPool = 24;
constexpr int kPowerupPool = 8;
constexpr int kCratePool = 2;
constexpr int kMinePool = 6;
constexpr int kSentryPool = 3;
constexpr int kFirePool = 4;
constexpr int kAcidPool = 8;
constexpr int kBarrelPool = 6;
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

    // Medkit pool — dormant health pickups zombies may drop.
    for (int i = 0; i < kMedkitPool; ++i) {
        maz::scene::SceneNode* m = tree.createChild(tree.root(), "Medkit" + std::to_string(i));
        m->setPosition(100000.0, 100000.0);
        m->addToGroup("medkits");
        tree.attachScript(*m, "Medkit");
    }

    // Ammo pool — dormant ammo boxes zombies may drop.
    for (int i = 0; i < kAmmoPool; ++i) {
        maz::scene::SceneNode* a = tree.createChild(tree.root(), "Ammo" + std::to_string(i));
        a->setPosition(100000.0, 100000.0);
        a->addToGroup("ammo");
        tree.attachScript(*a, "Ammo");
    }

    // Spit pool — dormant acid globs spitter zombies lob at the survivor.
    for (int i = 0; i < kSpitPool; ++i) {
        maz::scene::SceneNode* s = tree.createChild(tree.root(), "Spit" + std::to_string(i));
        s->setPosition(100000.0, 100000.0);
        s->addToGroup("spits");
        tree.attachScript(*s, "Spit");
    }

    // Power-up pool — dormant timed buffs zombies may rarely drop.
    for (int i = 0; i < kPowerupPool; ++i) {
        maz::scene::SceneNode* p = tree.createChild(tree.root(), "Powerup" + std::to_string(i));
        p->setPosition(100000.0, 100000.0);
        p->addToGroup("powerups");
        tree.attachScript(*p, "Powerup");
    }

    // Supply-crate pool — dormant care packages the survivor spawns periodically.
    for (int i = 0; i < kCratePool; ++i) {
        maz::scene::SceneNode* c = tree.createChild(tree.root(), "Crate" + std::to_string(i));
        c->setPosition(100000.0, 100000.0);
        c->addToGroup("crates");
        tree.attachScript(*c, "Crate");
    }

    // Proximity-mine pool — dormant until the survivor deploys one.
    for (int i = 0; i < kMinePool; ++i) {
        maz::scene::SceneNode* m = tree.createChild(tree.root(), "Mine" + std::to_string(i));
        m->setPosition(100000.0, 100000.0);
        m->addToGroup("mines");
        tree.attachScript(*m, "Mine");
    }

    // Auto-turret sentry pool — dormant until the survivor deploys one.
    for (int i = 0; i < kSentryPool; ++i) {
        maz::scene::SceneNode* s = tree.createChild(tree.root(), "Sentry" + std::to_string(i));
        s->setPosition(100000.0, 100000.0);
        s->addToGroup("sentries");
        tree.attachScript(*s, "Sentry");
    }

    // Molotov fire-patch pool — dormant until a molotov lands.
    for (int i = 0; i < kFirePool; ++i) {
        maz::scene::SceneNode* f = tree.createChild(tree.root(), "Fire" + std::to_string(i));
        f->setPosition(100000.0, 100000.0);
        f->addToGroup("fires");
        tree.attachScript(*f, "FirePool");
    }

    // Spitter acid-puddle pool — dormant until a spitter's glob lands.
    for (int i = 0; i < kAcidPool; ++i) {
        maz::scene::SceneNode* a = tree.createChild(tree.root(), "Acid" + std::to_string(i));
        a->setPosition(100000.0, 100000.0);
        a->addToGroup("acid");
        tree.attachScript(*a, "AcidPool");
    }

    // Explosive barrels — scattered around the arena, live from the start (shoot to detonate).
    for (int i = 0; i < kBarrelPool; ++i) {
        maz::scene::SceneNode* b = tree.createChild(tree.root(), "Barrel" + std::to_string(i));
        b->addToGroup("barrels");
        tree.attachScript(*b, "Barrel");
        const double ang = 6.2831853 * i / kBarrelPool + 0.4;
        const double rad = 18.0 + (i % 3) * 6.0;
        maz::script::Value bv = b->script();
        std::vector<maz::script::Value> a = {maz::script::Value::fromNum(std::cos(ang) * rad),
                                             maz::script::Value::fromNum(std::sin(ang) * rad)};
        tree.scripts().vm().callOn(bv, "place", a);
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
