# ZOMBOID — Player's Guide

A top-down twin-stick zombie survival shooter, and the flagship game built on the Maz engine.
Its entire ruleset is written in `maz::script` and driven on a `scene::SceneTree`; the app in
`apps/zomboid/` renders it and reads your input. You survive an endless, escalating siege — how many
waves can you last, and how high a score can you bank?

Run it: `./bin/zomboid` (or `./bin/zomboid --demo` to watch the autopilot show off every weapon).

---

## Controls

| Action | Key(s) |
|---|---|
| Move | **W A S D** (or arrow keys) |
| Aim | **Mouse** |
| Fire (hold) | **Left Mouse Button** |
| Switch weapon | **1** pistol · **2** shotgun · **3** SMG · **4** railgun · **5** flamethrower |
| Reload | **R** |
| Dodge roll (dash) | **Space** (+ a movement key for direction) |
| Melee shove | **F** |
| Throw grenade | **G** |
| Throw molotov | **X** |
| Lay proximity mine | **T** |
| Deploy auto-sentry | **Y** |
| Overcharge ultimate | **Q** (when the meter is full) |
| Eat a ration | **E** |
| **Shop** — buy ammo ($50) | **6** |
| **Shop** — buy grenade ($40) | **7** |
| **Shop** — buy heal ($60) | **8** |
| **Shop** — buy armor plate ($80) | **9** |
| Restart (after death) | **Enter** |
| Quit | **Esc** |

---

## Weapons

- **Pistol** — accurate, reliable, unlimited-ish reserve. Your default.
- **Shotgun** — a spread of pellets; devastating up close, weak at range.
- **SMG** — high rate of fire, low per-shot damage, a slight spread.
- **Railgun** — slow, high-damage hitscan beam that **pierces a whole line** of zombies in one shot.
- **Flamethrower** — no bullets; a short **cone of fire** that sets everything in it alight. Melts
  close packs, useless at distance.

Each weapon has its own magazine and reserve ammo; firing spends the magazine, **R** reloads it.
Between waves you earn permanent upgrades (+damage, +fire-rate, +max-health, +ammo, +crit-chance),
cycling automatically.

## The survivor's kit

- **Dodge roll (Space)** — a fast dash with brief **invincibility**. It's also *offensive*: you
  shoulder-check zombies you roll through, knocking them back and hurting them — dash *into* a pinch to
  bulldoze free.
- **Melee (F)** — a free heavy shove on a short cooldown. **Executes** any badly-wounded (<30% health)
  non-boss outright, and an execute refunds most of the cooldown — chain them to clean up stragglers.
- **Grenade (G)** / **Molotov (X)** — a thrown frag / a lingering fire patch.
- **Mine (T)** / **Sentry (Y)** — a proximity mine, and a stationary auto-turret that thins a lane.
- **Overcharge (Q)** — a screen-wide ultimate blast. Kills charge the meter; unleash it when full.
- **Second Wind** — a stored revive: lethal damage is cancelled once, bursting you back to half health
  with a crowd-clearing nova. Earned again every 50 kills.
- **Body armor** — a bought plate that soaks damage before your health; buy a fresh one with **9**.
- **Hunger** — you slowly get hungry; eat a ration (**E**) before it starts costing you health.

## The salvage economy

Every kill banks **cash**. Spend it mid-fight without pausing at the shop hotkeys (**6–9**): ammo,
grenades, a heal, or an armor plate. Hoard for a panic heal, or stay stocked on offense — your call.

---

## The enemies

Eleven zombie types join the horde as the waves climb:

| Kind | Enemy | Behaviour |
|---|---|---|
| 0 | **Walker** | The baseline shambler. |
| 1 | **Runner** | Fast, fragile; swarms you. |
| 2 | **Brute** | Slow, tanky, hits hard. |
| 3 | **Boss** | Huge bullet-sponge that leads every 5th wave; ground-slams. **Enrages** below 35% health — faster, slams twice as often. |
| 4 | **Exploder** | Detonates on death — shoot it from a distance. |
| 5 | **Spitter** | Hangs back and lobs acid that leaves a caustic **puddle** on the ground. |
| 6 | **Splitter** | Bursts into two runners when killed. |
| 7 | **Summoner** | Periodically calls in reinforcements. |
| 8 | **Armored** | Modest health behind a heavy damage-absorbing **shield** — break it down first. |
| 9 | **Leaper** | Light and quick; closes the gap in sudden **pounces**. |
| 10 | **Bloater** | Fat, slow, tanky; ruptures into a **toxic cloud** on death — kill it at range. |

Some zombies spawn as **elites** — bigger, tankier, worth far more, and they always drop a medkit.

## Power-ups

Rarely dropped by the slain, grabbed off the ground for a short buff:

- **Rapid Fire** — doubles fire rate.
- **Double Damage** — doubles damage.
- **Shield** — brief total immunity.
- **Piercing Rounds** — your bullets punch through several zombies.
- **Cryo Nova** — instantly chills *every* zombie on the field — a panic button when swarmed.

## Combat systems worth knowing

- **Combo multiplier** — fast, unbroken kills build a score multiplier (up to ×5) that decays if you
  stop killing.
- **Overkill gibs** — a hit far bigger than a zombie's health bursts it in a shockwave that can chain
  through a weakened pack.
- **Chilled = brittle** — frozen zombies take extra damage, and killing one while it's frozen
  **shatters** it into a spreading freeze.
- **Explosive barrels** — rusty barrels are scattered around the arena; shoot one to pop a big blast,
  and lure the horde onto them.

## The siege

- **Endless waves** — a Director spawns each wave larger and tougher, on a ring around you. Clear a
  wave for a score bonus and a permanent upgrade.
- **Day / night** — a smooth threat ramp: the horde hunts faster and bites harder toward midnight,
  easing at dawn. Watch the **THREAT** readout.
- **Wave mutators** — from wave 3, each wave rolls a random modifier shown on the HUD: **Feral**
  (faster), **Hulking** (tougher), or **Frenzied** (more of them). No two runs feel the same.

At the end of a run you're graded (**D** through **S**) on wave reached, kills, and accuracy, and your
best wave + score persist between sessions.

---

*This guide describes the game as shipped in `apps/zomboid`. Controls are read in `apps/zomboid/main.cpp`;
all gameplay rules live in the `maz::script` program inside `apps/zomboid/game.hpp`.*
