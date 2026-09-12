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
    // The whole game's script, 173 KB of it. -Wpedantic's -Woverlength-strings objects that the
    // standard only requires a compiler to support 65,536 characters in one literal; clang raises
    // it and GCC does not, which is why it surfaced only on macOS. Every compiler the project
    // targets handles this literal — it is a portability floor, not a defect — and splitting a
    // script into concatenated chunks to satisfy the floor would put arbitrary seams through the
    // game's source for nothing. Silenced here, around this literal only.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
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
# them), 4 bulwark (shielded), 5 volatile (corpses leave acid), 6 regenerator (bodies self-heal),
# 7 relentless (no knockback), 8 savage (harder bites), 9 bloodthirsty (bites heal the biter).
# Applied to every zombie as it spawns / on contact; the survivor sees the active modifier on the HUD.
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

# Aggression multiplier driving horde speed + bite damage. Holds at 1.0 through the daylit first half,
# then climbs with the falling light across the night half up to 1.7 at the darkest hour before dawn,
# so the threat tracks exactly what the screen shows: bright means calm, dark means deadly. (The old
# cosine peaked at dusk while the screen was still fully bright and eased off as it got darkest — the
# reverse of the intent — so it's replaced with the same night ramp the renderer darkens by.)
func danger() {
    var t = g_phase / g_day_len;                     # 0..1 across a full day
    var night = 0.0;                                 # daylit first half: no aggression bonus
    if (t >= 0.5) { night = (t - 0.5) * 2.0; }       # night half: 0 at dusk, climbing to 1 before dawn
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
    var acid_slow = 0;      # >0 while standing in a spitter's acid puddle — movement is bogged down
    var crit_chance = 0.15; # chance a shot lands a critical hit for bonus damage
    var crit_mult = 2.0;    # critical-hit damage multiplier
    var move_mult = 1.0;    # permanent walk-speed multiplier, grown by the between-wave speed upgrade
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
    var perfect_timer = 0;   # active-reload reward: brief +30% damage after a perfectly-timed reload
    var cur_ammo = 12;       # convenience mirrors of the active weapon for the HUD
    var cur_reserve = 48;
    var is_reloading = false;
    var grenades = 3;        # thrown-explosive count
    var mines = 2;           # deployable proximity-mine stock
    var sentries = 1;        # deployable auto-turret stock
    var molotovs = 2;        # thrown firebomb stock
    var flame_cd = 0;        # throttle for the flamethrower's lingering ground-fire trail
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
        # A kill charges the ultimate — and kills landed on a hot combo streak charge it FASTER: the
        # higher the score multiplier, the more meter each kill banks (1x at ×1–2, 2x at ×3–4, 3x at ×5).
        # So keeping a chain alive earns the panic-button ultimate far more often — aggression pays off.
        var gain = 1 + int((g_mult - 1) * 0.5);
        self.add_ult(gain);
        # Kill momentum feeds mobility: every kill shaves a little off the dodge-roll cooldown, so
        # staying on the offensive keeps your escape ready. Chaining kills in a tight spot can refresh a
        # dodge just when you need it — aggression sustains defense.
        if (self.dash_cd > 0) {
            self.dash_cd = self.dash_cd - 0.3;
            if (self.dash_cd < 0) { self.dash_cd = 0; }
        }
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
            if (z.alive) {
                z.take_damage(500);
                # Anything too tough to be one-shot (a boss, a shielded bulwark, a beefy elite) is left
                # deep-frozen by the overcharge — so the ultimate also cryo-locks the survivors (and,
                # since a chilled caster is silenced, shuts their abilities down) while you regroup.
                if (z.alive) { z.apply_slow(3.0); }
            }
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
        # Combo decays if you stop killing — but a hard-won streak buys grace: the higher your
        # multiplier, the longer the window before it resets (base 2.5s up to +2s at ×5), so a big
        # combo is more resilient and worth pushing for.
        g_combo_timer = g_combo_timer + dt;
        var win = g_combo_window + (g_mult - 1) * 0.5;
        if (g_combo_timer > win) {
            g_combo = 0;
            g_mult = 1;
        }
        if (self.alive == false) { return; }

        # Dodge-roll timers: cooldown recharges, i-frames tick down, and an active
        # dodge carries the survivor in a short burst.
        # Last-stand mobility: while critically wounded (adrenaline active) the dodge-roll recharges 60%
        # faster, so your escape roll — and its i-frames — comes back sooner exactly when you're desperate.
        # It stacks with adrenaline's existing faster fire, +damage, and damage reduction, making the
        # sub-25%-health scramble a real comeback window rather than a slow death.
        if (self.dash_cd > 0) {
            var drecover = dt;
            if (self.adrenaline) { drecover = dt * 1.6; }
            self.dash_cd = self.dash_cd - drecover;
        }
        if (self.melee_cd > 0) { self.melee_cd = self.melee_cd - dt; }
        if (self.iframes > 0) { self.iframes = self.iframes - dt; }
        if (self.acid_slow > 0) { self.acid_slow = self.acid_slow - dt; }
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
            # Field Medic (kind 9): a sustained heal-over-time. It steadily mends the survivor for the
            # whole duration — even mid-combat, unlike the passive out-of-combat regen that any hit resets
            # and starvation suppresses. Distinct from Vampiric (heal only on landed hits) and the instant
            # medkit: this is reliable trickle sustain you can rely on while still fighting. Capped at full.
            if (self.buff_kind == 9 and self.buff_timer > 0 and self.health < self.max_health) {
                self.health = self.health + 6.0 * dt;
                if (self.health > self.max_health) { self.health = self.max_health; }
            }
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

        # Out-of-combat regeneration: stay unharmed for a few seconds and health slowly recovers — but
        # NOT on an empty stomach. While starving (hunger maxed) the regen is suppressed, so the hunger
        # drain is real pressure you must answer with a ration instead of just out-healing it by standing
        # still (previously the +4/s regen quietly cancelled the -3/s starve drain, nullifying hunger).
        self.regen_timer = self.regen_timer + dt;
        if (self.regen_timer > 5.0 and self.health < self.max_health and self.hunger < 100) {
            self.health = self.health + dt * 4.0;
            if (self.health > self.max_health) { self.health = self.max_health; }
        }

        # Survival pressure: hunger creeps up; at max hunger, health drains — UNLESS you're carrying a
        # ration, in which case you instinctively eat one rather than waste away. So starvation damage only
        # bites when your food is actually gone, not while a meal sits unused in your pack. (Eating drops
        # hunger by a chunk, so it won't re-trigger until hunger climbs back to max — a self-paced auto-feed
        # that stretches your rations, matching how the shop and pickups never squander a resource.)
        self.hunger = self.hunger + dt * 1.5;
        if (self.hunger > 100) { self.hunger = 100; }
        if (self.hunger >= 100) {
            if (self.food > 0) { self.eat(); }
            else { self.health = self.health - dt * 3; }
        }

        # Flamethrower ground-fire trail throttle ticks down (bounds how often it lays a fire patch).
        if (self.flame_cd > 0) {
            self.flame_cd = self.flame_cd - dt;
            if (self.flame_cd < 0) { self.flame_cd = 0; }
        }
        # Active-reload damage surge ticks down.
        if (self.perfect_timer > 0) {
            self.perfect_timer = self.perfect_timer - dt;
            if (self.perfect_timer < 0) { self.perfect_timer = 0; }
        }
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

    # Roll this shot's damage: usually the base, occasionally a critical hit for bonus damage. While
    # critically wounded, last-stand adrenaline also lends a desperation +30% to every shot — so a
    # cornered survivor hits back harder, turning a near-death moment into a real comeback window.
    func shot_damage() {
        var out = self.damage;
        if (randf() < self.crit_chance) { out = self.damage * self.crit_mult; }
        if (self.adrenaline) { out = out * 1.3; }
        # Active-reload surge: shots fired in the window after a perfectly-timed reload bite 30% harder.
        if (self.perfect_timer > 0) { out = out * 1.3; }
        return out;
    }

    # Activate a timed power-up buff picked up from the field. A new pickup refreshes the timer.
    func grant_powerup(kind) {
        # Cryo Nova (kind 4) is a ONE-SHOT panic button, not a sustained buff: fire the field-wide chill
        # immediately and RETURN without touching the buff slot. So grabbing a Cryo Nova never cancels an
        # active sustained buff (Rapid Fire, Berserk, Overflow, ...) the way it used to — the pickup used to
        # overwrite buff_kind/buff_timer and reset the fire-rate/damage multipliers, wiping your real buff
        # and parking you in an 8s do-nothing "cryo" state. (That phantom state also showed a bogus HUD
        # countdown.) The instant chill is the whole effect; the sustained cold aura is Frost Field (kind 7).
        if (kind == 4) {
            var i = 0;
            var n = len(g_zombies);
            while (i < n) {
                var z = g_zombies[i];
                if (z.alive) { z.apply_slow(4.0); }
                i = i + 1;
            }
            emit(self.node.x, self.node.y, 24, 0);   # frost burst
            return;
        }
        self.buff_kind = kind;
        self.buff_timer = 8.0;
        self.buff_fr = 1.0;
        self.buff_dmg = 1.0;
        self.pierce_shots = false;
        if (kind == 0) { self.buff_fr = 2.2; }   # rapid fire
        if (kind == 1) { self.buff_dmg = 2.2; }  # double damage
        if (kind == 3) { self.pierce_shots = true; }  # piercing rounds
        # Berserk (kind 8): a combined offensive surge — fire rate AND damage both jump at once, so
        # it's the "go loud" button (rapid boosts only rate, double-damage only damage; this is both).
        if (kind == 8) { self.buff_fr = 1.7; self.buff_dmg = 1.7; }
        # Vampiric (kind 5): a sustained buff — no instant effect. While it lasts, each kinetic hit
        # (bullet / railgun beam) siphons a little health back, handled where those hits land.
        # Overflow (kind 6): infinite ammo / no reloads while active. Cancel any in-progress reload so
        # you can fire immediately, and top the current magazine for a clean look on the HUD.
        if (kind == 6) {
            self.reloading = false;
            self.mags[self.weapon] = self.mag_sizes[self.weapon];
        }
        # Field Medic (kind 9): a sustained buff with no instant effect — the heal-over-time is applied
        # each frame in the buff countdown (see _process). Nothing to set up here beyond the timer above.
        self.apply_mults();
    }

    # True while a Vampiric (kind 5) buff is active — kinetic hits leech health back to the survivor.
    func lifesteal_active() {
        if (self.buff_kind == 5) {
            if (self.buff_timer > 0) { return true; }
        }
        return false;
    }

    # True while a Frost Field (kind 7) buff is active — every zombie crawls at half speed for the
    # duration (a sustained slow aura, unlike the one-shot cryo nova).
    func frost_active() {
        if (self.buff_kind == 7) {
            if (self.buff_timer > 0) { return true; }
        }
        return false;
    }

    # Apply the next between-wave upgrade, cycling: +damage, +fire rate, +max health (heal), +ammo,
    # +crit chance, +crit damage, +move speed.
    func apply_upgrade() {
        var k = self.upgrades % 8;
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
                        if (k == 4) {
                            self.crit_chance = self.crit_chance + 0.05;
                        } else {
                            if (k == 5) {
                                # k == 5: heavier critical hits — deepens the crit build so +crit-chance
                                # upgrades keep paying off with bigger spikes, not just more-frequent ones.
                                self.crit_mult = self.crit_mult + 0.25;
                            } else {
                                if (k == 6) {
                                    # k == 6: fleeter feet — a permanent walk-speed boost. Mobility is king
                                    # in a twin-stick survival game, so a faster survivor kites the horde,
                                    # reaches loot, and repositions out of hazards more easily.
                                    self.move_mult = self.move_mult + 0.08;
                                } else {
                                    # k == 7: quicker recovery — trims the dodge-roll cooldown (down to a
                                    # 0.6s floor), so the escape roll and its i-frames come back sooner. A
                                    # defensive/mobility pick distinct from raw walk speed.
                                    self.dash_cd_max = self.dash_cd_max - 0.3;
                                    if (self.dash_cd_max < 0.6) { self.dash_cd_max = 0.6; }
                                }
                            }
                        }
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
                        # Armor-piercing slug: the railgun shears any shield clean off before biting
                        # into health, so it's the definitive answer to armored zombies and Bulwark waves.
                        if (z.shield > 0) { z.shield = 0; emit(z.node.x, z.node.y, 6, 0); }
                        z.take_damage(dmg);
                        z.apply_bleed(1);   # the beam lacerates too
                        if (self.lifesteal_active()) { self.heal(1.0); }   # Vampiric leech, per body
                        self.hits = self.hits + 1;   # railgun beam connections count too
                    }
                }
            }
            i = i + 1;
        }
        # The beam pops any explosive barrel it passes through, just as an ordinary bullet does — the
        # railgun shouldn't be the one gun that can't shoot a barrel. Same ray test as the zombie sweep.
        var rbi = 0;
        var rbn = len(g_barrels);
        while (rbi < rbn) {
            var rb = g_barrels[rbi];
            if (rb.active) {
                var brx = rb.node.x - self.node.x;
                var bry = rb.node.y - self.node.y;
                var bt = brx * ax + bry * ay;
                if (bt >= 0) {
                    var bpx = brx - bt * ax;
                    var bpy = bry - bt * ay;
                    var brr = beam + rb.radius;
                    if (bpx * bpx + bpy * bpy <= brr * brr) { rb.take_damage(dmg); }
                }
            }
            rbi = rbi + 1;
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
        # The cone cooks explosive barrels too, so torching a barrel pops it — same as a molotov.
        var fbi = 0;
        var fbn = len(g_barrels);
        while (fbi < fbn) {
            var fb = g_barrels[fbi];
            if (fb.active) {
                var frx = fb.node.x - self.node.x;
                var fry = fb.node.y - self.node.y;
                var fbd = sqrt(frx * frx + fry * fry);
                if (fbd <= range) {
                    var fdot = 1.0;
                    if (fbd > 0.01) { fdot = (frx * ax + fry * ay) / fbd; }
                    if (fdot > 0.6) { fb.take_damage(dmg); }
                }
            }
            fbi = fbi + 1;
        }
        # The cone also flashes over any caustic puddle it sweeps across — same combustion a molotov's
        # fire triggers — so the flamethrower can weaponise a spitter's acid or a Volatile-horde pool.
        var fai = 0;
        var fan = len(g_acid);
        while (fai < fan) {
            var fa = g_acid[fai];
            if (fa.active) {
                var farx = fa.node.x - self.node.x;
                var fary = fa.node.y - self.node.y;
                var fad = sqrt(farx * farx + fary * fary);
                if (fad <= range) {
                    var fadot = 1.0;
                    if (fad > 0.01) { fadot = (farx * ax + fary * ay) / fad; }
                    if (fadot > 0.6) { fa.combust(); }
                }
            }
            fai = fai + 1;
        }
        # The flamethrower also lays a lingering ground-fire trail: on a short throttle it drops a fire
        # patch mid-cone, so sweeping the flamethrower paints burning ground that keeps denying the lane
        # after the stream stops (and, being fire, flashes over any acid it touches). Throttled so a
        # single free fire slot is enough and it doesn't starve the molotov pool.
        if (self.flame_cd <= 0) {
            if (light_fire(self.node.x + ax * 6.0, self.node.y + ay * 6.0)) { self.flame_cd = 0.6; }
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
        # The sidearm pistol (weapon 0) has an infinite reserve, so it can always reload — every other
        # weapon needs rounds in reserve. This guarantees the survivor is never left fully disarmed when
        # the power weapons run dry: fall back to the pistol and you always have something that fires.
        if (w != 0 and self.reserves[w] <= 0) { return; }
        if (self.mags[w] >= self.mag_sizes[w]) { return; }
        self.reloading = true;
        self.reload_t = self.reload_times[w];
    }

    # Move rounds from reserve into the magazine (up to capacity) and end the reload.
    func finish_reload() {
        var w = self.weapon;
        # The pistol's reserve is bottomless — a reload always tops the mag back to full without drawing
        # any pool down, so it can never be exhausted (only the reload time gates it).
        if (w == 0) {
            self.mags[w] = self.mag_sizes[w];
            self.reloading = false;
            return;
        }
        var need = self.mag_sizes[w] - self.mags[w];
        var take = need;
        if (take > self.reserves[w]) { take = self.reserves[w]; }
        self.mags[w] = self.mags[w] + take;
        self.reserves[w] = self.reserves[w] - take;
        self.reloading = false;
    }

    # Manual reload (bound to R in the app). Tapping R while already reloading is an ACTIVE RELOAD: hit
    # the tail-end timing window and the reload snaps shut instantly AND grants a brief +30% damage surge
    # (see shot_damage). Tap too early and nothing happens — no penalty, the normal reload just continues.
    func reload() {
        if (self.reloading) {
            var w = self.weapon;
            var full = self.reload_times[w];
            # The window is the last stretch of the reload (12%–40% of the timer remaining).
            if (self.reload_t <= full * 0.4 and self.reload_t >= full * 0.12) {
                self.finish_reload();
                self.perfect_timer = 4.0;
                emit(self.node.x, self.node.y, 8, 1);   # snap-reload flourish
            }
            return;
        }
        self.start_reload();
    }

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
        if (light_fire(tx, ty)) {
            self.molotovs = self.molotovs - 1;
            return true;
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

    # Eat a ration: drops hunger by a chunk. A ration is never squandered on a body that isn't hungry —
    # eating at zero hunger would spend the food for nothing (the -40 just clamps straight back to 0), so
    # decline it and keep the ration, the same "no wasted resource" rule the shop applies to a heal bought
    # at full health. Returns true only if a ration was actually consumed. (The auto-feed at max hunger
    # only ever calls this with hunger pegged at 100, so the guard changes nothing there; it just stops a
    # mistimed manual E-press from throwing a meal away.)
    func eat() {
        if (self.food <= 0) { return false; }
        if (self.hunger <= 0) { return false; }
        self.food = self.food - 1;
        self.hunger = self.hunger - 40;
        if (self.hunger < 0) { self.hunger = 0; }
        return true;
    }

    # Restore health from a medkit, capped at the current max.
    func heal(amount) {
        self.health = self.health + amount;
        if (self.health > self.max_health) { self.health = self.max_health; }
    }

    # Grabbing a medkit heals, and any surplus past full health is banked as bonus armor (up to the
    # plate cap) instead of being thrown away — so a kit scooped up at high health is never wasted.
    func take_medkit(amount) {
        self.health = self.health + amount;
        if (self.health > self.max_health) {
            var overflow = self.health - self.max_health;
            self.health = self.max_health;
            self.armor = self.armor + overflow;
            if (self.armor > self.armor_max) { self.armor = self.armor_max; }
        }
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
        if (kind == 4) { cost = 70; }
        # Refuse a wasted buy so hard-won salvage is never thrown away on a no-op: a heal at full health,
        # or a fresh plate when the current one isn't even scratched, is declined WITHOUT charging (the
        # app can read the false return to keep the cash and flag the buy as unavailable).
        if (kind == 2 and self.health >= self.max_health) { return false; }
        if (kind == 3 and self.armor >= self.armor_max) { return false; }
        # An ammo refill is a no-op while the pistol (weapon 0) is equipped — its reserve is bottomless, so
        # a purchased refill would just top up a pool that's never drawn down. Decline it without charging;
        # switch to a power weapon before restocking. (Every other weapon has a finite reserve worth refilling.)
        if (kind == 0 and self.weapon == 0) { return false; }
        if (g_cash < cost) { return false; }
        g_cash = g_cash - cost;
        if (kind == 0) { self.collect_ammo(); }
        if (kind == 1) { self.grenades = self.grenades + 1; }
        if (kind == 2) { self.heal(40); }
        if (kind == 3) { self.armor = self.armor_max; }   # strap on a fresh armor plate
        # Field kit: restocks the tactical gadgets in one buy — a mine, a sentry, and a molotov — so
        # the placement tools have a cash source between supply crates, not just crate luck.
        if (kind == 4) {
            self.mines = self.mines + 1;
            self.sentries = self.sentries + 1;
            self.molotovs = self.molotovs + 1;
        }
        emit(self.node.x, self.node.y, 8, 0);
        return true;
    }

    # Grab a supply-crate care package: a big refill of ammo, grenades, health — and rations. The crate is
    # the only RENEWABLE food source (the map's scattered ration pickups are one-time), so it's what keeps
    # the hunger clock answerable on a long run: without it, food would inevitably run dry and starvation
    # became an unavoidable death regardless of skill. A periodic care package includes a couple of rations,
    # turning hunger into a sustainable pressure (keep grabbing crates) rather than a slow guaranteed loss.
    func collect_crate() {
        self.heal(50);
        self.food = self.food + 2;
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
        self.acid_slow = 0;    # the burst of speed shakes the survivor free of a spitter's caustic bog
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
        # Landing an execute refunds most of the melee cooldown, so cleaning up stragglers chains fast,
        # and each finisher siphons a little life back (executioner's bloodthirst) — so wading in to
        # shove-execute a wounded pack is a genuine sustain button, not just a create-space one.
        if (executed > 0) {
            self.melee_cd = self.melee_cd_max * 0.35;
            self.heal(executed * 5.0);
        }
        # A melee shove also bats incoming spitter acid globs out of the air within reach — a defensive
        # read: time the swing to knock a glob down before it lands. A swatted glob is destroyed clean,
        # leaving NO caustic puddle (unlike letting it splat), so a well-timed shove fully denies it.
        var mi = 0;
        var mn = len(g_spits);
        while (mi < mn) {
            var sp = g_spits[mi];
            if (sp.active) {
                var sx = sp.node.x - self.node.x;
                var sy = sp.node.y - self.node.y;
                if (sx * sx + sy * sy <= self.melee_range * self.melee_range) {
                    sp.active = false;                       # batted out of the air — no splat, no puddle
                    emit(sp.node.x, sp.node.y, 3, 1);
                }
            }
            mi = mi + 1;
        }
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
        # Last-stand grit: while the desperation surge is up (critically wounded, <25% health), the
        # survivor doesn't just hit harder and faster — they also shrug off a quarter of incoming
        # damage, so the comeback window is a genuine fighting chance instead of a death spiral.
        # Applied before armor, so a plate soaks more real hits while the surge lasts, too.
        if (self.adrenaline) { d = d * 0.75; }
        # Body armor is a depletable buffer: it takes the hit first, and only the overflow past a
        # spent plate bleeds through to health (bought from the shop, key 9).
        if (self.armor > 0) {
            self.armor = self.armor - d;
            emit(self.node.x, self.node.y, 3, 0);   # sparks off the plate
            if (self.armor > 0) { return; }         # plate still has charge — hit fully soaked
            # The plate is now spent: this is the breaking hit whether it landed exactly on the plate's
            # last point (armor == 0, no bleed-through) or overflowed it. Fire the shatter on the break.
            d = 0 - self.armor;                     # remainder past the broken plate (0 if it emptied exactly)
            self.armor = 0;
            # The plate doesn't fail quietly — it SHATTERS: as it breaks it throws off a concussive
            # burst that shoves and staggers the surrounding horde, buying a breath of space at the exact
            # moment the survivor's armor gives out. A defensive payoff for having worn a plate into the
            # crush (fires only on the break, not on every hit the plate soaks).
            var ai = 0;
            var an = len(g_zombies);
            while (ai < an) {
                var az = g_zombies[ai];
                if (az.alive) {
                    var adx = az.node.x - self.node.x;
                    var ady = az.node.y - self.node.y;
                    var ad2 = adx * adx + ady * ady;
                    if (ad2 <= 25.0) {   # radius 5
                        var am = sqrt(ad2);
                        if (am < 0.01) { am = 0.01; }
                        az.hit_knockback(adx / am, ady / am, 5.0);
                        az.stagger(0.4);
                    }
                }
                ai = ai + 1;
            }
            emit(self.node.x, self.node.y, 16, 0);   # plate-shatter burst
            g_shake = g_shake + 1.0;
            if (g_shake > 3.0) { g_shake = 3.0; }
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

    # How hard this bullet shoves the zombie it hits. Plain rounds give a light 0.6 nudge; a shotgun
    # pellet (falloff) lands a heavy point-blank shove that fades with travel — so a shotgun to the face
    # bodily knocks a zombie back (its crowd-control identity), while pellets fired across the arena
    # barely budge it. Scales on the same freshness ramp as the pellet's damage.
    func knock_strength() {
        if (self.falloff == false) { return 0.6; }
        var frac = self.life / self.max_life;   # 1.0 just-fired → 0.0 at the end of its life
        if (frac > 1.0) { frac = 1.0; }
        if (frac < 0.4) { frac = 0.4; }
        return 3.5 * frac;
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
                    if (spd > 0.001) { z.hit_knockback(self.vx / spd, self.vy / spd, self.knock_strength()); }
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
        # Spitter globs are shootable out of the air: a bullet that catches an in-flight acid glob
        # destroys it clean (no puddle, like a well-timed melee swat), giving RANGED counterplay to the
        # horde's one ranged threat — snipe the glob before it lands instead of only dodging or swatting
        # it. A piercing round shears through and keeps going; a normal round is spent knocking it down.
        var si = 0;
        var sn = len(g_spits);
        while (si < sn) {
            var sp = g_spits[si];
            if (sp.active) {
                var sdx = sp.node.x - self.node.x;
                var sdy = sp.node.y - self.node.y;
                var srr = self.hit_radius + 1.0;   # glob is a small mid-air target
                if (sdx * sdx + sdy * sdy <= srr * srr) {
                    sp.active = false;                       # shot down — destroyed clean, no puddle
                    emit(sp.node.x, sp.node.y, 5, 1);
                    if (self.pierce_left > 0) {
                        self.pierce_left = self.pierce_left - 1;
                    } else {
                        self.active = false;
                        return;
                    }
                }
            }
            si = si + 1;
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
                if (d2 <= self.blast_radius * self.blast_radius) {
                    z.take_damage(self.blast_dmg);
                    # Concussive stun: the blast briefly roots survivors of it, so the grenade is crowd
                    # control as well as damage — and a staggered body takes the weak-point bonus, so
                    # follow-up fire into the reeling pack bites harder.
                    z.stagger(0.5);
                }
            }
            i = i + 1;
        }
        # The frag also sets off any explosive barrel in the blast, so a grenade lobbed at a barrel
        # chains into a far bigger explosion.
        var gbi = 0;
        var gbn = len(g_barrels);
        while (gbi < gbn) {
            var gb = g_barrels[gbi];
            if (gb.active) {
                var gbx = gb.node.x - self.node.x;
                var gby = gb.node.y - self.node.y;
                if (gbx * gbx + gby * gby <= self.blast_radius * self.blast_radius) { gb.take_damage(999); }
            }
            gbi = gbi + 1;
        }
        # Acid is volatile — a hard blast flashes it over, same as a naked flame or a mine. Any caustic
        # puddle caught in the frag's radius combusts, so a grenade lobbed onto a spitter's acid chains
        # into a fiery flash-over that also sets the surrounding pack alight.
        var gai = 0;
        var gan = len(g_acid);
        while (gai < gan) {
            var ga = g_acid[gai];
            if (ga.active) {
                var gax = ga.node.x - self.node.x;
                var gay = ga.node.y - self.node.y;
                if (gax * gax + gay * gay <= self.blast_radius * self.blast_radius) { ga.combust(); }
            }
            gai = gai + 1;
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
            g_player.take_medkit(self.heal);
            self.active = false;
        } else {
            # Magnetism: within a short radius the kit drifts toward the survivor. When the survivor is
            # critically wounded (last-stand adrenaline, under 25% health) the kit reaches out from twice
            # as far and drifts in faster — the one lifeline you're desperate for finds you in the
            # scramble, extending the adrenaline comeback the game already grants (faster fire, +30%
            # shot damage, quicker dodge, damage reduction) with a fifth desperation perk. Only medkits
            # get this reach; ammo and power-ups keep their normal pull, so it's a survival lifeline, not
            # a blanket loot magnet.
            var mag = 36.0;      # normal pull radius (6 units)
            var pull = 12.0;
            if (g_player.adrenaline) { mag = 144.0; pull = 20.0; }   # 12-unit reach, faster drift
            if (d2 <= mag) {
                var d = sqrt(d2);
                self.node.x = self.node.x + (dx / d) * pull * dt;
                self.node.y = self.node.y + (dy / d) * pull * dt;
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

# Wave-clear vacuum: sweep up every small drop still lying on the field so clearing a wave never strands a
# medkit, power-up, or ammo box during the lull before the next one. Returns how many were collected.
# (Supply crates are deliberately NOT vacuumed — they're a large care package you walk to, not a small
# drop that would otherwise expire unclaimed.)
func vacuum_pickups() {
    var collected = 0;
    if (g_player == nil) { return collected; }
    if (g_player.alive == false) { return collected; }
    var i = 0;
    var mn = len(g_medkits);
    while (i < mn) {
        var m = g_medkits[i];
        if (m.active) {
            g_player.take_medkit(m.heal);
            m.active = false;
            emit(m.node.x, m.node.y, 4, 0);
            collected = collected + 1;
        }
        i = i + 1;
    }
    var j = 0;
    var pn = len(g_powerups);
    while (j < pn) {
        var p = g_powerups[j];
        if (p.active) {
            g_player.grant_powerup(p.kind);
            p.active = false;
            emit(p.node.x, p.node.y, 4, 0);
            collected = collected + 1;
        }
        j = j + 1;
    }
    # Ammo boxes are small zombie drops too, so sweep them up on the clear rather than letting a box that
    # dropped late in the wave expire unclaimed during the lull — the same courtesy as medkits/power-ups.
    var a = 0;
    var an = len(g_ammo);
    while (a < an) {
        var ab = g_ammo[a];
        if (ab.active) {
            g_player.collect_ammo();
            ab.active = false;
            emit(ab.node.x, ab.node.y, 4, 0);
            collected = collected + 1;
        }
        a = a + 1;
    }
    return collected;
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
        # A mine's blast also cooks off any explosive barrel in range, so laying a mine beside a barrel
        # rigs a huge combined detonation — and the barrel's own blast chains on to more barrels. (Nearby
        # exploders are already set off above: the blast's 120 damage kills them, triggering their own
        # detonation, so mines daisy-chain through an exploder pack too.)
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var b = g_barrels[bi];
            if (b.active) {
                var bdx = b.node.x - self.node.x;
                var bdy = b.node.y - self.node.y;
                if (bdx * bdx + bdy * bdy <= self.blast_radius * self.blast_radius) { b.take_damage(999); }
            }
            bi = bi + 1;
        }
        # Acid is volatile — a naked flame flashes it over, and so does a hard blast. The mine's
        # detonation combusts any caustic puddle in range (a spitter's acid or a Volatile-horde pool),
        # so rigging a mine beside a puddle chains the trap into a bigger fiery flash-over that also
        # sets the surrounding pack alight. Rewards deliberate placement on the hazards already downrange.
        var ai = 0;
        var an = len(g_acid);
        while (ai < an) {
            var a = g_acid[ai];
            if (a.active) {
                var adx = a.node.x - self.node.x;
                var ady = a.node.y - self.node.y;
                if (adx * adx + ady * ady <= self.blast_radius * self.blast_radius) { a.combust(); }
            }
            ai = ai + 1;
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
        # Cluster-aware trigger. The blast radius (6) dwarfs the trigger ring (3), so popping for the
        # first lone straggler to clip the edge wastes most of the blast. Instead the mine holds for a
        # worthwhile catch: it detonates the instant TWO or more zombies are inside the trigger ring (a
        # cluster its blast can engulf), or the moment a single zombie steps point-blank onto it (half the
        # trigger radius) — so a lone walker in the lane still sets it off rather than strolling over a dud.
        var count = 0;
        var point_blank = false;
        var pb = self.trigger_range * 0.5;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= self.trigger_range * self.trigger_range) {
                    count = count + 1;
                    if (d2 <= pb * pb) { point_blank = true; }
                }
            }
            i = i + 1;
        }
        if (point_blank or count >= 2) { self.detonate(); }
    }
}

# Threat weight for a zombie of the given kind — higher means more dangerous / more worth spending a
# scarce shot on. Used by the sentry to focus-fire the target that matters instead of the nearest body:
# a boss or a summoner (which spawns endless reinforcements) or a healer (which undoes your damage)
# outranks a slow walker even when the walker is closer.
func threat_of(kind) {
    var t = 10;             # 0 walker / default — least dangerous
    if (kind == 1) { t = 15; }    # runner
    if (kind == 9) { t = 20; }    # leaper
    if (kind == 6) { t = 25; }    # splitter
    if (kind == 4) { t = 30; }    # exploder
    if (kind == 10) { t = 35; }   # bloater
    if (kind == 13) { t = 40; }   # warper
    if (kind == 5) { t = 45; }    # spitter (the horde's one ranged attacker)
    if (kind == 8) { t = 50; }    # armored
    if (kind == 11) { t = 55; }   # screamer (frenzies the pack)
    if (kind == 2) { t = 60; }    # brute
    if (kind == 12) { t = 75; }   # healer (undoes your damage)
    if (kind == 7) { t = 80; }    # summoner (calls endless reinforcements)
    if (kind == 3) { t = 100; }   # boss
    return t;
}

# A pooled auto-turret sentry. Dormant until the survivor deploys it; then it auto-fires a hitscan bolt
# at the highest-threat live zombie in range on a cadence (nearest breaks ties), for a limited lifetime,
# before powering down. A stationary ally that thins a lane while the survivor handles another.
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

    # When a sentry powers down — lifetime expired or magazine dry — it doesn't just wink out: it
    # self-destructs in a final blast, damaging and staggering the zombies around it. So a sentry
    # planted deep in the horde earns a farewell explosion, rewarding aggressive placement. Friendly to
    # the survivor (zombie-only blast), unlike an explosive barrel.
    func self_destruct() {
        self.active = false;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= 36.0) {   # radius 6
                    z.take_damage(50);
                    z.stagger(0.4);
                }
            }
            i = i + 1;
        }
        # The farewell blast is a real explosion, so — like a barrel, mine, grenade, or exploder death —
        # it cooks off any explosive barrel in range and flashes over any caustic puddle it overlaps. The
        # blast itself stays friendly to the survivor (it never damages the player directly); planting a
        # sentry next to a barrel just means its power-down chains into the barrel's own (double-edged)
        # blast. Completes the "every hard blast sets off volatile hazards" rule for the last explosion
        # that skipped it.
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var b = g_barrels[bi];
            if (b.active) {
                var bx = b.node.x - self.node.x;
                var by = b.node.y - self.node.y;
                if (bx * bx + by * by <= 36.0) { b.take_damage(999); }
            }
            bi = bi + 1;
        }
        var ai = 0;
        var an = len(g_acid);
        while (ai < an) {
            var a = g_acid[ai];
            if (a.active) {
                var ax = a.node.x - self.node.x;
                var ay = a.node.y - self.node.y;
                var rr = 6.0 + a.radius;
                if (ax * ax + ay * ay <= rr * rr) { a.combust(); }
            }
            ai = ai + 1;
        }
        emit(self.node.x, self.node.y, 20, 1);   # blast burst
        g_shake = g_shake + 1.2;
        if (g_shake > 3.0) { g_shake = 3.0; }
    }

    func _process(dt) {
        if (self.active == false) { return; }
        self.life = self.life - dt;
        if (self.life <= 0) { self.self_destruct(); return; }
        self.fire_cd = self.fire_cd - dt;
        if (self.fire_cd > 0) { return; }
        # Acquire the highest-threat live zombie in range and shoot it. A sentry's magazine is scarce, so
        # it focus-fires what matters (a boss, a summoner, a healer) rather than plinking whatever body is
        # merely nearest; among targets of equal threat it picks the closest, so it still finishes the
        # nearest of a like pack first.
        var r2 = self.range * self.range;
        var target = nil;
        var best_threat = -1;
        var best_d2 = 0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                var d2 = dx * dx + dy * dy;
                if (d2 <= r2) {
                    var th = threat_of(z.kind);
                    if (th > best_threat) {
                        best_threat = th; best_d2 = d2; target = z;
                    } else {
                        if (th == best_threat) {
                            if (d2 < best_d2) { best_d2 = d2; target = z; }
                        }
                    }
                }
            }
            i = i + 1;
        }
        if (target != nil) {
            target.take_damage(self.damage);
            emit(target.node.x, target.node.y, 3, 0);   # impact sparks on the target
            self.fire_cd = 1.0 / self.fire_rate;
            self.ammo = self.ammo - 1;                   # spend a bolt; it dies when the magazine is dry
            if (self.ammo <= 0) {
                self.self_destruct();                    # runs dry → goes out with a bang
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
        # Fire cooks off any explosive barrel caught in the patch, so a molotov thrown onto a barrel
        # detonates it after a moment — chaining the flames into a blast for extra area control.
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var b = g_barrels[bi];
            if (b.active) {
                var bdx = b.node.x - self.node.x;
                var bdy = b.node.y - self.node.y;
                if (bdx * bdx + bdy * bdy <= self.radius * self.radius) {
                    b.take_damage(self.burn_dps * dt);
                }
            }
            bi = bi + 1;
        }
        # Flame touching a caustic puddle flashes it over: an acid pool overlapping the fire combusts in
        # a violent burst (see AcidPool.combust). So a molotov thrown onto a spitter's acid — or onto a
        # Volatile-horde pool — converts that enemy hazard into a damaging fireball instead of terrain
        # you have to route around.
        var ai = 0;
        var an = len(g_acid);
        while (ai < an) {
            var a = g_acid[ai];
            if (a.active) {
                var adx = a.node.x - self.node.x;
                var ady = a.node.y - self.node.y;
                var rr = self.radius + a.radius;
                if (adx * adx + ady * ady <= rr * rr) { a.combust(); }
            }
            ai = ai + 1;
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
                        # Caustic sludge also bogs the survivor down, so an acid puddle is real area
                        # denial — you can't just tank the damage and hold your spot in it.
                        g_player.acid_slow = 0.5;
                    }
                }
            }
            # The sludge is caustic to the horde too: any zombie standing in it is bogged down and
            # crawls (the same chill-slow the cryo tools inflict), refreshed each tick so it lasts as
            # long as they wade through it. It deals them no bonus damage — already-dead flesh doesn't
            # bleed, so to actually HURT the pack you still have to burn the pool (combust). This turns
            # a spitter's own puddle into a double-edged battlefield: it punishes you for holding a
            # spot, but you can also kite the swarm through it to slow the whole pack — so spitters are
            # a threat that cuts both ways.
            var zi = 0;
            var zn = len(g_zombies);
            while (zi < zn) {
                var z = g_zombies[zi];
                if (z.alive) {
                    var zx = z.node.x - self.node.x;
                    var zy = z.node.y - self.node.y;
                    if (zx * zx + zy * zy <= self.radius * self.radius) { z.apply_slow(0.6); }
                }
                zi = zi + 1;
            }
        }
        self.puff = self.puff - dt;
        if (self.puff <= 0) { self.puff = 0.4; emit(self.node.x, self.node.y, 2, 1); }
    }

    # The caustic sludge is volatile: touch a naked flame to it and it flashes over in a single violent
    # combustion. Any zombie caught in (or near) the pool takes a burst of damage and is set alight, and
    # the puddle is spent in the flash. This turns a hazard the survivor normally has to avoid into an
    # offensive tool — molotov or flame-cone a spitter's puddle (or a Volatile-horde pool) to weaponise it.
    func combust() {
        if (self.active == false) { return; }
        self.active = false;
        emit(self.node.x, self.node.y, 18, 1);   # fiery flash-over
        var r = self.radius + 2.0;
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= r * r) {
                    z.take_damage(30);
                    if (z.alive) { z.ignite(2.0, 12); }
                }
            }
            i = i + 1;
        }
        # The flash-over is a violent combustion in its own right, so it cooks off any explosive barrel it
        # engulfs — just like a naked flame or a hard blast does. A spitter's puddle that happens to sit on
        # a barrel becomes a two-stage bomb: light the acid, the flash-over pops the barrel. Completes the
        # rule that every violent combustion (fire, blast, now the acid flash) can set a barrel off.
        var bi = 0;
        var bn = len(g_barrels);
        while (bi < bn) {
            var cb = g_barrels[bi];
            if (cb.active) {
                var bx = cb.node.x - self.node.x;
                var by = cb.node.y - self.node.y;
                if (bx * bx + by * by <= r * r) { cb.take_damage(999); }
            }
            bi = bi + 1;
        }
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

# Activate a dormant fire patch from the pool at (x, y). Returns true if a free patch was lit.
func light_fire(x, y) {
    var i = 0;
    var n = len(g_fires);
    while (i < n) {
        var f = g_fires[i];
        if (f.active == false) {
            f.ignite_ground(x, y);
            return true;
        }
        i = i + 1;
    }
    return false;
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
        # The blast is double-edged: a survivor caught in it takes half damage (still respecting dodge
        # i-frames, shield and armor) and is flung clear — so lure the horde onto a barrel, don't hug it.
        # A survivor mid-dodge (i-frames up) rides the blast out completely — no damage AND no fling —
        # just like a boss slam or a brute's blow; without the i-frame gate the knockback flung a
        # perfectly-dodged survivor even though take_damage had already spared them, contradicting the
        # "respecting dodge i-frames" promise right here in this comment.
        if (g_player != nil and g_player.alive) {
            var pdx = g_player.node.x - self.node.x;
            var pdy = g_player.node.y - self.node.y;
            var pd2 = pdx * pdx + pdy * pdy;
            if (pd2 <= self.blast_radius * self.blast_radius) {
                g_player.take_damage(self.blast_dmg * 0.5);
                var pm = sqrt(pd2);
                if (pm < 0.01) { pm = 0.01; }
                if (g_player.iframes <= 0) {
                    g_player.node.x = g_player.node.x + (pdx / pm) * 5.0;
                    g_player.node.y = g_player.node.y + (pdy / pm) * 5.0;
                }
            }
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
        # A hard blast flashes over volatile acid too: any caustic puddle in the barrel's radius combusts
        # at once (on top of the lingering fire it leaves), so a barrel popped next to a spitter's pool
        # sets off a big combined fireball.
        var cai = 0;
        var can = len(g_acid);
        while (cai < can) {
            var ca = g_acid[cai];
            if (ca.active) {
                var cax = ca.node.x - self.node.x;
                var cay = ca.node.y - self.node.y;
                if (cax * cax + cay * cay <= self.blast_radius * self.blast_radius) { ca.combust(); }
            }
            cai = cai + 1;
        }
        # The ruptured barrel spills burning fuel: it leaves a lingering fire patch where it stood, so a
        # popped barrel keeps denying that ground (and cooking anything that walks in) for a few seconds
        # after the blast — and, like any fire, it flashes over a caustic puddle it happens to overlap.
        light_fire(self.node.x, self.node.y);
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
    var slam_warn = 0;     # boss: telegraph wind-up counting down before a slam actually lands
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
    var leap_wind = 0;     # leaper: coil/telegraph timer — it crouches briefly before the pounce fires
    var leap_vx = 0;       # leaper: stored pounce velocity locked in at the start of the lunge
    var leap_vy = 0;
    var warp_cd = 0;       # warper (kind 13): cooldown between blinks toward the survivor
    var warp_warn = 0;     # warper: telegraph shimmer counting down before a blink actually fires
    var bleed_stacks = 0;  # laceration stacks from kinetic rounds — each ticks damage over time
    var bleed_timer = 0;   # while > 0 the wound is open and bleeding; refreshed by fresh hits
    var bleed_tick = 0;    # accumulator so bleed damage lands in periodic ticks, not every frame
    var stagger_timer = 0; # brief flinch: a heavy single hit freezes the zombie where it stands
    var stagger_cd = 0;    # cooldown after a flinch so it can't be perpetually stun-locked
    var frenzy_timer = 0;  # while > 0 the zombie is whipped into a screamer's frenzy — moves faster
    var scream_warn = 0;   # screamer (kind 11) shriek wind-up: telegraph beat before the shriek lands
    var mend_warn = 0;     # healer (kind 12) mend wind-up: telegraph beat before the heal pulse lands
    var summon_warn = 0;   # summoner (kind 7) call wind-up: telegraph beat before reinforcements arrive

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

    # Whip this zombie into a frenzy (a screamer's shriek): it surges faster for `dur` seconds. The boss
    # is immune — it's a self-contained fight tuned by its own enrage phase, so a screamer can't stack a
    # frenzy on top of enrage into an uncatchable speed. Same boss exemption as stagger/gib/overkill/heal.
    func apply_frenzy(dur) {
        if (self.alive == false) { return; }
        if (self.kind == 3) { return; }
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
        # Relentless horde (mutator 7): the whole wave plants its feet — no knockback at all. Your shoves,
        # dash-strikes, mine blasts, and shotgun push all stop moving them, so positioning-by-knockback is
        # off the table that wave and you must lean on damage, chills, staggers, and kiting instead.
        if (g_mutator == 7) { return; }
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
        # The reinforcement's kind scales with the run: an early summoner calls fodder walkers, but from
        # the mid-game (wave 5+) it calls faster RUNNERS instead — so a summoner left alive stays a real
        # threat deep into a run rather than trickling in walkers the player easily outpaces. Killing the
        # summoner (or interrupting its cast) is the answer at every stage, but it matters more late.
        var rk = 0;
        if (self.spawn_wave >= 5) { rk = 1; }
        while (i < n and made < cnt) {
            var z = g_zombies[i];
            if (z.alive == false and z != self) {
                var ang = randf_range(0, 6.2831853);
                z.spawn(self.node.x + cos(ang) * 2.0, self.node.y + sin(ang) * 2.0, rk, self.spawn_wave);
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
    # A gib shockwave that chains through the surrounding pack. Its force scales with how badly the
    # killing blow overkilled this body (`power` = killing-damage / max-health, always >= 1.5 here):
    # a monster hit — a point-blank shotgun, a railgun line, a double-damage crit — throws a bigger,
    # wider burst than a body that only just tipped over the overkill threshold. So overkilling deep
    # inside a crowd is rewarded with a proportionally deadlier chain, not a flat one.
    func overkill_burst(power) {
        var extra = power - 1.5;
        if (extra < 0) { extra = 0; }
        if (extra > 3.0) { extra = 3.0; }         # cap the runaway on absurd hits
        var burst_dmg = 25 + int(extra * 15);     # 25 at the threshold, up to 70 on a huge overkill
        var rad2 = 16.0 + extra * 6.0;            # radius 4 → up to ~5.8 as the burst grows
        var i = 0;
        var n = len(g_zombies);
        while (i < n) {
            var z = g_zombies[i];
            if (z.alive and z != self) {
                var dx = z.node.x - self.node.x;
                var dy = z.node.y - self.node.y;
                if (dx * dx + dy * dy <= rad2) { z.take_damage(burst_dmg); }
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
        self.slam_warn = 0;
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
        self.leap_wind = 0;
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
                                            if (k == 13) {
                                                # Warper: fragile teleporter that shambles slowly, then
                                                # blinks a big chunk of the way to the survivor — closing
                                                # gaps you thought were safe. Kill it fast before it ports
                                                # into your lap.
                                                self.health = 22 + w * 5;
                                                self.speed = 9;
                                                self.damage = 7;
                                                self.radius = 1.05;
                                                self.attack_range = 1.2;
                                                self.score_value = 26;
                                                self.warp_cd = 2.0;
                                                self.warp_warn = 0;
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
        }
        # Wave mutator reshapes the whole horde: feral bodies are faster, hulking ones tougher (frenzy
        # only affects how many spawn, handled by the director). Applied before the speed clamp so a
        # feral runner still tops out at the hard speed cap.
        if (g_mutator == 1) { self.speed = self.speed * 1.35; }
        if (g_mutator == 2) { self.health = self.health * 1.5; }
        # Bulwark horde: every zombie carries a damage-absorbing shield (like the armored kind), so the
        # whole wave must be broken down before it can be hurt — punishing weak, spread-out fire.
        if (g_mutator == 4) { self.shield = self.shield + 15 + w * 2; }
        # Savage horde: every bite lands harder (x1.6). The wave isn't faster or tougher, but a single
        # missed body punishes the survivor far more — rewards keeping the horde at range.
        if (g_mutator == 8) { self.damage = self.damage * 1.6; }
        if (self.speed > 30) { self.speed = 30; }
        self.max_health = self.health;
    }

    func take_damage(dmg) {
        if (self.alive == false) { return; }
        var d = dmg;
        if (self.slow_timer > 0) { d = d * 1.5; }  # chilled bodies are brittle — shatter bonus
        # Weak-point window: a staggered zombie is reeling and defenceless, so shots landed while it
        # flinches bite 40% deeper. Rewards following a melee shove or dash-strike (both stagger) with
        # fire — and stacks with the chill bonus for a very brittle target.
        if (self.stagger_timer > 0) { d = d * 1.4; }
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
            var payout = salvage + int(salvage * (g_mult - 1) / 2);
            # Night is deadlier, so it pays: kills after dusk bank 50% more salvage — a risk/reward for
            # holding out through the dark hours rather than playing it safe.
            if (is_night()) { payout = payout + int(payout / 2); }
            g_cash = g_cash + payout;
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
                        # The exploder's "bite" is its detonation, so a Savage horde (mutator 8) makes it
                        # blast 60% harder too — otherwise the exploder would be the one enemy a Savage wave
                        # left untouched, since it detonates for a flat amount rather than biting for its
                        # (Savage-scaled) contact damage like every other kind.
                        var eblast = 35.0;
                        if (g_mutator == 8) { eblast = eblast * 1.6; }
                        if (ex * ex + ey * ey <= 25.0) { g_player.take_damage(eblast); }
                    }
                }
                # The blast also catches nearby zombies, so an exploder shot inside a pack takes the pack
                # with it — and it now CHAIN-DETONATES other exploders in range, daisy-chaining a cluster
                # into one big string of blasts (a real reward for luring exploders together, but the
                # survivor eats every blast they're standing in, so it cuts both ways). The chain is
                # naturally bounded: self is already dead (alive == false) before this runs, so a detonated
                # exploder is skipped and can't re-trigger — same safe recursion the barrels use.
                var bi = 0;
                var bn = len(g_zombies);
                while (bi < bn) {
                    var oz = g_zombies[bi];
                    if (oz.alive and oz != self) {
                        var ozx = oz.node.x - self.node.x;
                        var ozy = oz.node.y - self.node.y;
                        if (ozx * ozx + ozy * ozy <= 25.0) {
                            if (oz.kind == 4) {
                                oz.take_damage(9999);   # chain-detonate the neighbouring exploder
                            } else {
                                # The incendiary blast burns survivors of the initial hit.
                                oz.take_damage(40);
                                oz.ignite(3.0, 10);
                            }
                        }
                    }
                    bi = bi + 1;
                }
                # The exploder's blast is a hard, incendiary one — so, like a mine, grenade, or barrel, it
                # cooks off any explosive barrel in range (chaining into the barrel's own detonation)...
                var ebi = 0;
                var ebn = len(g_barrels);
                while (ebi < ebn) {
                    var eb = g_barrels[ebi];
                    if (eb.active) {
                        var ebx = eb.node.x - self.node.x;
                        var eby = eb.node.y - self.node.y;
                        if (ebx * ebx + eby * eby <= 25.0) { eb.take_damage(999); }
                    }
                    ebi = ebi + 1;
                }
                # ...and flashes over any caustic puddle it overlaps (acid is volatile — any hard blast sets
                # it off), so an exploder popped on a spitter's pool or beside a barrel sets up the same
                # environmental chain reaction the player's own explosives do.
                var eai = 0;
                var ean = len(g_acid);
                while (eai < ean) {
                    var ea = g_acid[eai];
                    if (ea.active) {
                        var eax = ea.node.x - self.node.x;
                        var eay = ea.node.y - self.node.y;
                        if (eax * eax + eay * eay <= 25.0) { ea.combust(); }
                    }
                    eai = eai + 1;
                }
                emit(self.node.x, self.node.y, 20, 1); # blast burst
            }
            # A splitter bursts into two fast runners at its position — UNLESS it died burning: a body
            # cooking in fire is incinerated before it can rupture, so it spawns nothing. That gives fire
            # (a molotov, the flamethrower, or a spreading blaze) a specific job — burn the splitters to
            # stop them multiplying, rather than shooting them and doubling the problem.
            if (self.kind == 6 and self.burn_timer <= 0) {
                self.split_off(2);
                emit(self.node.x, self.node.y, 12, 1);
            }
            # A bloater ruptures on death — but WHAT it ruptures into depends on how it died. Normally its
            # gas-bag body bursts into a lingering toxic cloud (reusing the acid hazard), so a careless
            # close-range kill leaves you standing in poison. But if it dies BURNING, that volatile gas is
            # already alight: instead of a poison cloud it erupts into a fire patch, converting the enemy
            # hazard into one that cooks the horde. So torching a bloater denies its poison and hands you a
            # blaze instead — a clear job for the flamethrower / molotov, mirroring how fire flashes acid over.
            if (self.kind == 10) {
                if (self.burn_timer > 0) {
                    light_fire(self.node.x, self.node.y);
                } else {
                    leave_acid(self.node.x, self.node.y);
                }
                emit(self.node.x, self.node.y, 20, 1);
            }
            # Volatile Horde mutator: every body ruptures into a caustic pool where it falls, so the
            # arena steadily fills with hazard as you fight — camping a single kill-zone poisons the
            # ground under your own feet, forcing you to keep repositioning. Bosses are exempt, and
            # bloaters already leave one (so we skip them here to avoid stacking two puddles).
            if (g_mutator == 5 and self.kind != 3 and self.kind != 10) {
                leave_acid(self.node.x, self.node.y);
            }
            # Overkill: if the killing hit alone dwarfed this body's full health, it gibs in a chain
            # shockwave. Kinds with their own death behaviour are exempt — an exploder (4) and boss (3)
            # have bespoke blasts, and a splitter (6) bursts into runners, so a gib would just vaporise
            # the very splitlings it spawned this same frame.
            if (d >= self.max_health * 1.5 and self.kind != 4 and self.kind != 3 and self.kind != 6) {
                self.overkill_burst(d / self.max_health);
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
            # Fire contagion: a body that dies while burning passes the flames on — every nearby zombie
            # is set alight (at the same intensity it was burning), so fire chains through a packed horde
            # the way frost shatter chains a freeze. Torch one zombie in a tight crowd and the blaze can
            # cascade across the whole pack — the offensive mirror of the defensive frost spread.
            if (self.burn_timer > 0) {
                var yi = 0;
                var yn = len(g_zombies);
                while (yi < yn) {
                    var yz = g_zombies[yi];
                    if (yz.alive and yz != self) {
                        var ydx = yz.node.x - self.node.x;
                        var ydy = yz.node.y - self.node.y;
                        if (ydx * ydx + ydy * ydy <= 20.25) { yz.ignite(1.5, self.burn_dps); }  # radius 4.5
                    }
                    yi = yi + 1;
                }
                emit(self.node.x, self.node.y, 10, 1);
            }
            g_shake = g_shake + s;
            if (g_shake > 3.0) { g_shake = 3.0; }
            # Felling a boss — the wave leader — is a landmark kill, so it always drops a full care
            # package: a guaranteed medkit AND a guaranteed power-up (on top of the big score and cash).
            # A just reward for grinding down the hardest target on the field.
            if (self.kind == 3) {
                drop_medkit(self.node.x, self.node.y);
                var bpk = int(randf_range(0, 10));
                if (bpk > 9) { bpk = 9; }
                drop_powerup(self.node.x - 2.0, self.node.y, bpk);
                emit(self.node.x, self.node.y, 24, 1);   # triumphant burst
            }
            # A slain zombie sometimes drops a medkit; an elite always does, plus an extra flourish.
            if (self.elite) {
                drop_medkit(self.node.x, self.node.y);
                emit(self.node.x, self.node.y, 16, 1);
                # An elite's death releases a shockwave that knocks back and wounds the surrounding
                # crowd, clearing breathing room around the corpse (and the medkit it drops) — a reward
                # for felling it inside a pack.
                var ei = 0;
                var en = len(g_zombies);
                while (ei < en) {
                    var ez = g_zombies[ei];
                    if (ez.alive and ez != self) {
                        var edx = ez.node.x - self.node.x;
                        var edy = ez.node.y - self.node.y;
                        var ed2 = edx * edx + edy * edy;
                        if (ed2 <= 49.0) {   # radius 7
                            var em = sqrt(ed2);
                            if (em < 0.01) { em = 0.01; }
                            ez.hit_knockback(edx / em, edy / em, 5.0);
                            ez.take_damage(30);
                        }
                    }
                    ei = ei + 1;
                }
            } else {
                if (randf() < 0.12) { drop_medkit(self.node.x, self.node.y); }
            }
            # Rarely it drops a power-up instead (rapid-fire, damage, shield, piercing, cryo, vampiric,
            # overflow, frost field, berserk, field medic).
            if (randf() < 0.05) {
                var pk = int(randf_range(0, 10));
                if (pk > 9) { pk = 9; }
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
        # A sustained Frost Field aura chills every zombie on the field: refresh the standard chill STATUS
        # each frame it's up, not just a movement slow. That makes the field confer everything the chill
        # status does — bodies turn brittle (+50% shatter damage), frost-shatter on death, and every
        # ability that a chill shuts off (summoner call, screamer shriek, healer mend, warper blink,
        # leaper coil, spitter spit) is denied — exactly as the field is documented. (It used to only halve
        # movement, so the "hard answer to support-heavy waves" it advertises never actually fired.)
        if (g_player.frost_active()) { self.apply_slow(0.2); }
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
        # Regenerator horde (mutator 6): every body knits its wounds back shut over time (6% of its
        # max health per second), so chip damage bleeds away and you must commit real burst to a kill
        # rather than poking. But an actively-harmed body can't close its wounds: regen halts entirely
        # while the zombie is burning, bleeding, OR chilled — so fire, lacerating fire, and cryo are all
        # reliable hard counters at ANY wave (previously only a chill stopped it, and a light burn/bleed
        # had to out-damage a flat 6%/s that scales with the body's health — so it couldn't reliably beat
        # regen on a tanky late-wave body). Bosses are exempt (they already enrage-heal).
        if (g_mutator == 6 and self.kind != 3 and self.slow_timer <= 0 and self.burn_timer <= 0 and self.bleed_stacks <= 0 and self.health < self.max_health) {
            self.health = self.health + self.max_health * 0.06 * dt;
            if (self.health > self.max_health) { self.health = self.max_health; }
        }
        # Summoner (kind 7): periodically calls a reinforcement until its budget runs out — now with a
        # telegraph wind-up first, so you get a window to burst the back-liner (or chill/stagger it) before
        # the reinforcement lands. A chilled caster is silenced (cryo shuts down the whole back line), and a
        # chill OR a stagger during the wind-up fizzles the call — a melee shove or dash-strike interrupts
        # the cast just like it breaks a leaper's coil.
        if (self.kind == 7 and self.summon_budget > 0) {
            if (self.summon_warn > 0) {
                self.summon_warn = self.summon_warn - dt;
                if (self.slow_timer > 0 or self.stagger_timer > 0) {
                    self.summon_warn = 0;   # chilled OR staggered mid-wind-up → the call fizzles
                } else {
                    if (self.summon_warn <= 0) {
                        if (self.summon(1) > 0) {
                            self.summon_budget = self.summon_budget - 1;
                            emit(self.node.x, self.node.y, 8, 1);
                        }
                    }
                }
            } else {
                self.summon_cd = self.summon_cd - dt;
                if (self.summon_cd <= 0 and self.slow_timer <= 0) {
                    self.summon_warn = 0.6;                       # start the telegraph
                    self.summon_cd = 4.0;                         # reset now so it won't retrigger mid-tell
                    emit(self.node.x, self.node.y, 2, 1);         # tell puff — reinforcements incoming
                }
            }
        }
        # Screamer (kind 11): on a cooldown it shrieks, whipping every nearby zombie into a speed
        # frenzy. It's fragile, so silencing it early keeps the horde from surging — a priority target.
        if (self.kind == 11) {
            if (self.scream_warn > 0) {
                # Winding up: a telegraph beat before the shriek lands, so the survivor gets a window to
                # burst the fragile screamer down or chill it — either cuts the shriek off entirely
                # (kill it and _process never runs; a chill mid-tell makes it fizzle).
                self.scream_warn = self.scream_warn - dt;
                if (self.slow_timer > 0 or self.stagger_timer > 0) {
                    self.scream_warn = 0;   # chilled OR staggered mid-wind-up → the shriek fizzles
                } else {
                    if (self.scream_warn <= 0) {
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
            } else {
                self.cooldown = self.cooldown - dt;
                if (self.cooldown <= 0 and self.slow_timer <= 0) {   # a chilled screamer can't wind up
                    self.scream_warn = 0.6;                          # start the telegraph
                    self.cooldown = 5.0;                             # reset now so it won't retrigger mid-tell
                    emit(self.node.x, self.node.y, 2, 1);            # tell puff — the shriek is coming
                }
            }
        }
        # Healer (kind 12): on a cooldown it mends every wounded zombie in a radius, knitting a chunk of
        # health back (never past their max). Force-multiplying but fragile, so cull it before it undoes
        # your work. It never heals itself, keeping it a body you can burn down.
        if (self.kind == 12) {
            if (self.mend_warn > 0) {
                # Winding up a mend: a telegraph beat before the heal pulse lands, so you get a window to
                # kill the fragile healer (or chill it) before it undoes your chip damage on the pack.
                self.mend_warn = self.mend_warn - dt;
                if (self.slow_timer > 0 or self.stagger_timer > 0) {
                    self.mend_warn = 0;   # chilled OR staggered mid-wind-up → the mend fizzles
                } else {
                    if (self.mend_warn <= 0) {
                        var mended = 0;
                        var hi = 0;
                        var hn = len(g_zombies);
                        while (hi < hn) {
                            var hz = g_zombies[hi];
                            # The mend never touches the boss (kind 3): the wave leader is a self-contained
                            # fight with its own huge health bar and enrage phase, so letting a healer refund
                            # 25% of that pool would undo a hard-won grind and undercut the designed boss
                            # duel. The boss is exempt here just as it is from stagger, gib, overkill, and
                            # the Volatile mutator — a healer can still mend the surrounding pack, not the boss.
                            # A wound that's actively burning, bleeding, OR chilled can't be patched either:
                            # damage over time (and cold) holds the wound open against ALL healing, so a
                            # healer can't top up a body you've set alight, lacerated, or frozen — the exact
                            # same rule that stops a Regenerator body knitting itself shut (which halts on
                            # burn/bleed/chill alike). So torching the pack (molotov / flamethrower), raking
                            # it with kinetic fire, or freezing it (Cryo Nova / Frost Field) all shut the
                            # healer's mend off, giving damage-over-time AND cold a single unified anti-heal
                            # role across every heal source in the game.
                            if (hz.alive and hz != self and hz.kind != 3 and hz.burn_timer <= 0 and hz.bleed_stacks <= 0 and hz.slow_timer <= 0 and hz.health < hz.max_health) {
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
            } else {
                self.cooldown = self.cooldown - dt;
                if (self.cooldown <= 0 and self.slow_timer <= 0) {   # a chilled healer can't wind up
                    self.mend_warn = 0.6;                            # start the telegraph
                    self.cooldown = 4.0;                             # reset now so it won't retrigger mid-tell
                    emit(self.node.x, self.node.y, 2, 1);            # tell puff — a mend is coming
                }
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
        # (The Frost Field's movement slow now comes through the chill status above — set once per frame at
        # the top of _process via apply_slow — so it's no longer applied separately here.)
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
            # Enrage regeneration: in its second phase the boss's fury knits its wounds — it slowly heals
            # (2% of its huge health bar per second) so the climax rewards sustained pressure, not a leisurely
            # plink. But it obeys the same DoT/chill rule everything else does: a boss that's burning,
            # bleeding, or chilled can't heal, so keeping fire, laceration, or cold on it shuts the enrage-heal
            # off entirely. This is why a healer/Regenerator can't top the boss up either — its self-heal is
            # the boss's own affair, and DoT is the counter. Never past its max.
            if (self.enraged and self.burn_timer <= 0 and self.bleed_stacks <= 0 and self.slow_timer <= 0 and self.health < self.max_health) {
                self.health = self.health + self.max_health * 0.02 * dt;
                if (self.health > self.max_health) { self.health = self.max_health; }
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
            if (self.slam_warn > 0) {
                # Wind-up telegraph: the boss rears back for a beat before the slam lands, giving a
                # sharp survivor a window to dash or run clear of the radius before it hits.
                self.slam_warn = self.slam_warn - dt;
                if (self.slam_warn <= 0) {
                    if (dist <= 10.0) {
                        g_player.take_damage(25);
                        # Knockback: the shockwave hurls the survivor away from the boss, so a slam
                        # clears space instead of just chipping health — punishing standing too close.
                        # A dodging survivor (i-frames up) rides the slam out UNTOUCHED — no damage AND no
                        # knockback — exactly like a brute's heavy blow. Without the i-frame gate the slam
                        # still flung a perfectly-dodged survivor around, contradicting the "untouchable
                        # mid-roll" promise the dodge is built on and the tell's "dash clear" counterplay.
                        if (g_player.iframes <= 0 and dist > 0.01) {
                            g_player.node.x = g_player.node.x + (dx / dist) * 6.0;
                            g_player.node.y = g_player.node.y + (dy / dist) * 6.0;
                        }
                    }
                    emit(self.node.x, self.node.y, 28, 1); # shockwave burst
                    g_shake = g_shake + 2.5;
                    if (g_shake > 3.0) { g_shake = 3.0; }
                }
            } else {
                if (self.slam_cd <= 0) {
                    self.slam_cd = slam_gap;
                    self.slam_warn = 0.5;   # start the wind-up; the slam lands half a second later
                    emit(self.node.x, self.node.y, 29, 1);   # telegraph ring
                }
            }
        }
        if (self.kind == 5) {
            # Spitter: a ranged artillery unit that MAINTAINS its firing distance — it advances only until
            # inside spitting range, holds in the outer band, and backpedals if the survivor pushes well
            # inside that range, so it keeps a firing gap rather than letting you stroll straight up to it
            # (matching its "keeps its distance" role, like the summoner and the rest of the back line).
            # It's still fragile (no melee bite) and silenced by chill/stagger, so cornering it or freezing
            # it remains the counter — you just have to work past its kiting.
            var hold = self.attack_range * 0.6;
            if (dist > self.attack_range) {
                self.node.x = self.node.x + (dx / dist) * self.speed * aggro * sm * dt;
                self.node.y = self.node.y + (dy / dist) * self.speed * aggro * sm * dt;
            } else {
                if (dist < hold and dist > 0.01) {
                    self.node.x = self.node.x - (dx / dist) * self.speed * sm * dt;   # backpedal to keep range
                    self.node.y = self.node.y - (dy / dist) * self.speed * sm * dt;
                }
            }
            self.cooldown = self.cooldown - dt;
            # A chilled or staggered spitter can't lob — a frozen back-liner is silenced just like the
            # casters, and a flinching (rooted) one can't wind up a throw. So a Cryo Nova / Frost Field, or
            # a melee shove / dash-strike that reaches it, shuts the horde's one ranged attacker up too.
            if (dist <= self.attack_range and self.cooldown <= 0 and self.slow_timer <= 0
                and self.stagger_timer <= 0) {
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
            if (self.leap_wind > 0) {
                # Interrupt: a stagger (a melee shove, a dash-strike, a grenade's concussion) or a chill
                # landed during the coil breaks the pounce outright — the leaper uncoils harmlessly and
                # has to recover before it can wind up again. So the telegraph isn't only something to
                # juke sideways from: punish the tell and you deny the leap entirely (the same readable
                # counterplay the back-line casters already have).
                if (self.stagger_timer > 0 or self.slow_timer > 0) {
                    self.leap_wind = 0;
                    self.leap_cd = 1.5;    # brief recovery before it can coil again
                    emit(self.node.x, self.node.y, 4, 0);   # fizzle puff as the pounce collapses
                    return;
                }
                # Coiled: rooted for a beat, telegraphing the pounce so the survivor can juke sideways.
                # The lunge then commits toward wherever the survivor is when the wind-up finishes.
                self.leap_wind = self.leap_wind - dt;
                if (self.leap_wind <= 0) {
                    var lw = 32.0;   # pounce burst speed
                    # Guard the exact-overlap case: if the survivor walked onto the coiled (rooted) leaper
                    # so distance is ~0, dividing by it would make the lunge vector NaN and corrupt the
                    # leaper's position. Floor the distance like every other division in this file does.
                    var ld = dist;
                    if (ld < 0.01) { ld = 0.01; }
                    self.leap_vx = (dx / ld) * lw;
                    self.leap_vy = (dy / ld) * lw;
                    self.leaping = 0.32;
                    self.leap_cd = 3.0;
                    emit(self.node.x, self.node.y, 6, 0);   # dust puff on take-off
                }
                return;
            }
            self.leap_cd = self.leap_cd - dt;
            if (self.leap_cd <= 0 and self.slow_timer <= 0 and dist > self.attack_range and dist < 16.0) {
                self.leap_wind = 0.35;   # crouch and coil before springing
                emit(self.node.x, self.node.y, 2, 1);   # tell puff as it hunkers down
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
        # Screamer (11) and Healer (12): fragile BACK-LINE support, so like the summoner they hold their
        # distance — backing away when the survivor closes rather than shambling into melee — which forces
        # you to push through the horde (or pick them off at range) to silence them, matching their designed
        # role. Their shriek/mend works on the surrounding pack, not the survivor, so they never need to
        # close in. (Previously they walked straight into melee, making a "back-line" support trivial to
        # reach.) Their cast wind-up above still roots them; this only governs their between-cast movement.
        if (self.kind == 11 or self.kind == 12) {
            var bkeep = 12.0;
            if (dist < bkeep and dist > 0.01) {
                self.node.x = self.node.x - (dx / dist) * self.speed * sm * dt;   # retreat
                self.node.y = self.node.y - (dy / dist) * self.speed * sm * dt;
            } else {
                if (dist > bkeep + 4.0) {
                    self.node.x = self.node.x + (dx / dist) * self.speed * 0.5 * sm * dt; # drift in slowly
                    self.node.y = self.node.y + (dy / dist) * self.speed * 0.5 * sm * dt;
                }
            }
            return;
        }
        # Interrupt the blink: a stagger (a shove/dash/grenade concussion) or a chill landed during the
        # warper's shimmer tell cancels the teleport OUTRIGHT and puts it on cooldown — the same readable
        # counterplay the leaper's coil has. Without this the stagger only paused the tell (it resumed and
        # blinked the instant the flinch wore off); now punishing the tell denies the blink entirely.
        if (self.kind == 13 and self.warp_warn > 0 and (self.stagger_timer > 0 or self.slow_timer > 0)) {
            self.warp_warn = 0;
            self.warp_cd = 1.5;                       # brief recovery before it can charge another blink
            emit(self.node.x, self.node.y, 4, 0);     # fizzle shimmer as the phase collapses
        }
        # Warper (kind 13): between slow shambles it teleports, on a cooldown, half the way to the
        # survivor in a single instant — erasing distance a walker never could and appearing right on
        # top of you. It only blinks while there's real ground to cover, then walks the last stretch.
        if (self.kind == 13 and self.stagger_timer <= 0) {
            if (self.warp_warn > 0) {
                # Winding up: the warper shimmers in place for a beat before it phases, giving the
                # survivor a fair tell to shoot it or reposition. It's rooted during the tell, then
                # commits the blink toward wherever the survivor is when the wind-up finishes.
                self.warp_warn = self.warp_warn - dt;
                if (self.warp_warn <= 0 and dist > 4.0) {
                    self.node.x = self.node.x + (dx / dist) * (dist * 0.5);
                    self.node.y = self.node.y + (dy / dist) * (dist * 0.5);
                    self.warp_cd = 2.5;
                    emit(self.node.x, self.node.y, 12, 0);   # blink shimmer at the arrival point
                }
                return;   # rooted through the telegraph
            }
            self.warp_cd = self.warp_cd - dt;
            # A chilled warper is locked down — a frozen body can't phase, so cryo (a Cryo Nova or a
            # Frost Field) is a hard counter that pins it in place until the chill wears off.
            if (self.warp_cd <= 0 and dist > 8.0 and self.slow_timer <= 0) {
                self.warp_warn = 0.3;                        # start the telegraph shimmer
                emit(self.node.x, self.node.y, 2, 1);        # tell puff as it charges the blink
                return;                                      # root it the instant the tell begins
            }
        }
        if (dist > self.attack_range) {
            self.node.x = self.node.x + (dx / dist) * self.speed * aggro * sm * dt;
            self.node.y = self.node.y + (dy / dist) * self.speed * aggro * sm * dt;
        }
        self.cooldown = self.cooldown - dt;
        if (dist <= self.attack_range and self.cooldown <= 0) {
            # An exploder is a suicide bomber: the moment it reaches the survivor it detonates ON CONTACT
            # instead of biting, so you can't just tank or melee it point-blank — keep your distance and
            # pop it from range. take_damage(9999) kills it, triggering its own death blast, which catches
            # the adjacent survivor.
            if (self.kind == 4) { self.take_damage(9999); return; }
            g_player.take_damage(self.damage * aggro);
            # Bloodthirsty Horde (mutator 9): a bite doesn't just wound you — the zombie SIPHONS life from
            # the wound, healing itself. So letting the horde touch you actively repairs it: you can't win a
            # Bloodthirsty wave by trading hits, you have to not get bitten (kite, chill, knock back). The
            # leech only lands if the bite actually drew blood, so a bite you DODGE (i-frames) or SOAK on a
            # Shield power-up feeds the horde nothing — the same two conditions take_damage no-ops on. That
            # keeps dodge/shield as real counterplay instead of a leak that still heals the pack. The boss
            # is exempt like every other horde-wide modifier; capped at the biter's max health.
            var bit_home = g_player.iframes <= 0 and (g_player.buff_kind != 2 or g_player.buff_timer <= 0);
            if (g_mutator == 9 and self.kind != 3 and bit_home) {
                self.health = self.health + 8;
                if (self.health > self.max_health) { self.health = self.max_health; }
                emit(self.node.x, self.node.y, 4, 1);   # a small crimson leech flourish
            }
            # A Brute (kind 2) doesn't just bite — its heavy blow HURLS the survivor back, wrecking your
            # position and your aim. So a brute that reaches you is a real spacing threat, not just a
            # damage tick — you get thrown clear (maybe into the rest of the horde). A dodging survivor
            # (i-frames up) rides it out untouched.
            if (self.kind == 2 and g_player.iframes <= 0 and dist > 0.01) {
                g_player.node.x = g_player.node.x + (dx / dist) * 4.0;
                g_player.node.y = g_player.node.y + (dy / dist) * 4.0;
            }
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
    var clean_streak = 0;   # consecutive flawless waves — the cash reward escalates with the streak

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
        if (w >= 3) { g_mutator = int(randf_range(1, 10)); }
        if (g_mutator > 9) { g_mutator = 9; }
        var pool = len(g_zombies);
        var count = self.base + w * 2;
        # Frenzy mutator throws a bigger horde at the survivor.
        if (g_mutator == 3) { count = count + int(count / 2); }
        if (count > pool) { count = pool; }
        var cx = 0;
        var cy = 0;
        if (g_player != nil) { cx = g_player.node.x; cy = g_player.node.y; }
        # Replenish the arena's explosive barrels: from wave 2 on, a couple of spent barrels are restored
        # each wave at fresh ring positions around the survivor, so the "lure the horde onto a barrel"
        # playstyle stays viable through an endless run instead of drying up once the starting barrels are
        # all popped. Mirrors how supply crates keep gadgets, ammo, and rations flowing.
        if (w >= 2) {
            var restored = 0;
            var rbi = 0;
            var rbn = len(g_barrels);
            while (rbi < rbn and restored < 2) {
                var rb = g_barrels[rbi];
                if (rb.active == false) {
                    var bang = randf_range(0, 6.2831853);
                    var brad = 16.0 + randf_range(0, 10.0);
                    rb.place(cx + cos(bang) * brad, cy + sin(bang) * brad);
                    restored = restored + 1;
                }
                rbi = rbi + 1;
            }
        }
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
                        if (i % 15 == 0 and w >= 8) {
                            k = 13;
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
                # Sweep up any pickups still on the field so a cleared wave never strands a drop.
                vacuum_pickups();
                # Flawless wave: cleared without taking a single hit. Doubles the clear bonus, pays a
                # cash reward, and patches the survivor up a little — rewarding aggressive, clean play.
                self.last_clean = g_wave_clean;
                if (g_wave_clean) {
                    var fb = self.wave * 50;
                    g_score = g_score + fb;
                    self.last_bonus = self.last_bonus + fb;
                    # Flawless streak: each unbroken no-hit wave pays more cash than the last (25, 40,
                    # 55, ... capped at 100), so stringing perfect waves together is worth chasing.
                    self.clean_streak = self.clean_streak + 1;
                    var reward = 25 + (self.clean_streak - 1) * 15;
                    if (reward > 100) { reward = 100; }
                    g_cash = g_cash + reward;
                    if (g_player != nil) { g_player.heal(10.0); }
                } else {
                    self.clean_streak = 0;   # taking a hit this wave breaks the flawless streak
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
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// True when a run's (wave, score) beats the stored best as a single ranked pair — score is the primary
// key, wave breaks ties. (Used to RANK one run against another, e.g. leaderboard ordering.)
inline bool beatsBest(int wave, int score, int bestWave, int bestScore) {
    return score > bestScore || (score == bestScore && wave > bestWave);
}

// True when a finished run sets a new personal best on EITHER axis — a deeper wave OR a higher score.
// Best wave and best score are PERSISTED independently (the HUD shows them as two separate stats:
// "BEST WAVE n   SCORE n"), so reaching a new deepest wave counts as a new best even when the score
// didn't beat the record, and vice-versa. This is the persistence/"NEW BEST!" rule; beatsBest above is
// the single-pair ranking rule. Gating the save on beatsBest (score-primary) used to leave best_wave
// stale — a run that reached a new deepest wave but scored below the all-time best never saved its wave.
inline bool newPersonalBest(int wave, int score, int bestWave, int bestScore) {
    return wave > bestWave || score > bestScore;
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

// One-line plain-language effect for each wave mutator (1-9), shown under its codename on the HUD so a
// player learns what "SAVAGE HORDE" (etc.) actually does instead of guessing. Kept in lockstep with the
// mutator logic in start_wave()/spawn(): 1 feral, 2 hulking, 3 frenzy, 4 bulwark, 5 volatile,
// 6 regenerator, 7 relentless, 8 savage, 9 bloodthirsty. Index 0 (no mutator) and out-of-range return "".
inline const char* mutatorEffect(int m) {
    if (m == 1) return "faster zombies";
    if (m == 2) return "tougher zombies";
    if (m == 3) return "a bigger horde";
    if (m == 4) return "shielded zombies";
    if (m == 5) return "corpses leave acid";
    if (m == 6) return "zombies self-heal";
    if (m == 7) return "no knockback";
    if (m == 8) return "harder-hitting bites";
    if (m == 9) return "bites heal the horde";
    return "";
}

// Display name for an active power-up buff (buff_kind 0-9), shown on the HUD with its countdown so the
// player knows which buff is up — and when it's about to lapse — instead of reading it off a body tint.
// Matches grant_powerup(): 0 rapid fire, 1 double damage, 2 shield, 3 piercing, 4 cryo nova, 5 vampiric,
// 6 overflow, 7 frost field, 8 berserk, 9 field medic. The idle state (buff_kind -1) and out-of-range "".
inline const char* powerupName(int b) {
    if (b == 0) return "RAPID FIRE";
    if (b == 1) return "DOUBLE DAMAGE";
    if (b == 2) return "SHIELD";
    if (b == 3) return "PIERCING";
    if (b == 4) return "CRYO NOVA";
    if (b == 5) return "VAMPIRIC";
    if (b == 6) return "OVERFLOW";
    if (b == 7) return "FROST FIELD";
    if (b == 8) return "BERSERK";
    if (b == 9) return "FIELD MEDIC";
    return "";
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
