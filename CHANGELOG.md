# Changelog

All notable changes to the Maz Engine are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); the engine version is defined in
`engine/include/maz/core/Version.hpp` (`maz::engineVersion()`) and mirrored in the top-level
`CMakeLists.txt`.

## [Unreleased]

### Flagship game
- **ZOMBOID** (`apps/zomboid`) — a top-down twin-stick zombie **shooter** whose entire simulation is
  written in `maz::script` and driven on a `scene::SceneTree`. Mouse-aim + hold-to-fire pulls rounds
  from an object-pooled bullet system (a script can't spawn nodes, so bullets and zombies are fixed
  pools the scripts activate/recycle); zombies have health and die; a wave Director spawns endless,
  escalating hordes on a ring; loot, hunger→health survival pressure, and the day/night rage ramp
  remain. Score / wave / kills HUD, a hurt-tint on wounded zombies, an aim tracer and crosshair.
  100% of the rules are verified headless in CI; the render app steps a deterministic fixed timestep
  under `--headless`/`--frames` (a 30 s autopilot run reaches wave 4 / 36 kills with no GPU).
- **Weapons** — four switchable guns (1/2/3/4): pistol (accurate), shotgun (6-pellet spread, slow),
  SMG (fast, weaker), and a **railgun** — a slow, high-damage **piercing hitscan beam** that damages
  an entire line of zombies in a single shot (with its own magazine/reserve/reload), spawning a
  fly-through tracer for the visual. Each gun has its own fire rate, damage, spread and bullet speed;
  the shotgun fires a real pellet burst in one shot. Verified headless (`zomboid_sim`): a railgun shot
  pierces three zombies lined up along the aim at once while sparing a body off the beam. Verified headless (`tests/zomboid/sim.cpp`, ctest
  `zomboid_sim`): pools build, wave 1 auto-spawns, firing respects each weapon's cadence, the shotgun
  emits a 6-bullet burst, the SMG out-shoots the pistol over a second, a bullet kills a zombie and
  scores, cleared waves escalate, and survival/loot still hold. Kept as its own fast-compiling target
  so game iterations verify in a second instead of rebuilding the full unit suite.
- **Elite champions** — from wave 2 the Director occasionally crowns a non-boss zombie as an elite:
  2.5× health, a speed bump, 3× score, and a guaranteed medkit on death (plus an extra blood
  flourish). They read at a glance — the app draws them larger and gold-tinted — turning a routine
  zombie into a high-value, high-risk target worth chasing. Verified headless (`zomboid_sim`):
  `make_elite` more than doubles a zombie's health and score, and killing an elite always drops a
  medkit (where an ordinary kill only sometimes does).
- **Boss ground slam** — the boss now has a special attack on top of its bite: every ~4 seconds it
  slams the ground, emitting a radial shockwave (particle burst + heavy screen-shake) that deals a
  flat 25 damage to the survivor if they are within ~10 units. Because the boss is slow, the slam is
  what punishes standing next to it — you have to keep circling. Verified headless (`zomboid_sim`): a
  boss slamming with a survivor 6 units away (inside the shockwave but outside bite range) drains
  their health, while an identical boss 30 units away leaves them untouched.
- **Enemy variety** — four zombie kinds with distinct stats, sprites and sizes: walkers (baseline),
  runners (fast, fragile), brutes (slow, tanky, big, hard-hitting) and a boss that leads every 5th
  wave. The Director mixes kinds into each wave (runners from wave 2, brutes from wave 3); tougher
  kinds award more score, and bullet hit tests use each body's radius so big enemies are easier to
  hit. Verified headless: a forced wave 5 spawns a boss (HP > 300) plus a genuine runner/brute/walker
  mix with runners out-pacing and brutes out-tanking walkers, and a brute survives a single pistol
  shot. Autopilot 45 s run reaches wave 5 (a boss wave).
- **Bullet knockback** — a bullet shoves the zombie it hits back along the shot's direction (a new
  `hit_knockback` on the zombie), giving fire tactile weight and a crowd-control lever — the shotgun's
  burst can stall a rush. Heavy bodies (brutes and bosses, by radius) shrug most of it off. Verified
  headless (`zomboid_sim`): a knockback impulse pushes a walker along the travel direction while a
  brute barely moves.
- **Impact juice** — a script-side particle pool (sparks on every hit, a blood burst on a kill) and a
  decaying screen-shake value that the camera reads (bigger kicks for brutes and bosses), plus an
  app-side muzzle flash. The simulation core is verified headless (`zomboid_sim`): the particle pool
  builds, a kill emits particles and raises `g_shake`, and both ease back to rest within a couple of
  seconds. The muzzle flash and camera shake render on the app side (owner-visible on real hardware).
- **Ammo + reload** — each weapon has a magazine, a reserve, a capacity and a reload time (pistol
  12/48, shotgun 6/24, SMG 30/90). A shot spends one round; emptying the magazine auto-reloads (or
  press R), and switching weapons cancels a reload. Loot doubles as an ammo crate, topping up every
  weapon's reserve. The HUD shows `AMMO mag / reserve` and a RELOADING indicator. Verified headless
  (`zomboid_sim`): firing drains the magazine and an auto-reload refills it from a diminished reserve;
  a weapon with an empty magazine and empty reserve fires exactly its last round then goes dry and
  cannot reload; and collecting loot raises the reserve.
- **End-of-run summary** — the death screen now reports the run: time survived (m:ss), shot accuracy
  (hits ÷ shots, capped at 100%), and total kills alongside the wave/score line. The survivor tracks
  `time_survived` (advances only while alive) and a `hits` counter incremented whenever a bullet or the
  railgun beam connects. Verified headless (`zomboid_sim`): the survive timer accrues ~1 s over 60
  ticks and firing into a parked target raises the hit counter.
- **Kill-milestone rewards** — every 25th kill hands the survivor a bonus grenade and a small heal,
  a steady sustained-play reward that runs alongside the between-wave upgrades. A single `on_kill` hook
  now drives kill tracking, the ultimate charge, and the milestone check. Verified headless
  (`zomboid_sim`): the 25th kill (and not the 24th) grants a grenade and heals, and the next milestone
  advances to 50.
- **Critical hits** — every shot rolls against a crit chance (15% base) for bonus damage (×2), across
  all four weapons (the railgun rolls once for the whole beam). Crit chance is a permanent stat the
  survivor can grow, and the HUD shows the live percentage. Verified headless (`zomboid_sim`): with the
  chance forced to 1.0 a shot deals exactly base×crit-mult, and forced to 0.0 it deals base.
- **Between-wave upgrades** — surviving a wave grants a permanent upgrade, cycling +20% damage, +15%
  fire rate, +25 max health (with a full heal), an ammo top-up, and +5% crit chance. Damage and fire rate are applied
  as multipliers over each weapon's base stats, so the whole arsenal scales together as the run goes
  on. The HUD shows the upgrade count and current DMG/RATE multipliers. Verified headless
  (`zomboid_sim`): each `apply_upgrade` raises the matching stat in turn, and clearing wave 1 hands out
  the first upgrade exactly when wave 2 opens (none before).
- **Chill / slow status + shatter** — grenade blasts now also chill every zombie caught in a slightly
  wider radius, leaving survivors crawling at 40% speed for ~2.5 s (a new `slow_timer` + `apply_slow`
  on the zombie, folded into its movement), and a **chilled body takes 50% extra damage** from any hit
  — a freeze-then-shred combo that turns grenades into a damage multiplier as well as crowd control.
  Chilled zombies read as icy blue in the app. Verified headless (`zomboid_sim`): a chilled walker
  advances less than 60% as far per tick as an unimpaired one, and takes ~1.5× the damage from an
  identical hit.
- **Grenades** — a throwable, object-pooled explosive (start with 3, press G): it flies along the aim,
  slows, and after a short fuse detonates, damaging every zombie inside the blast radius plus a burst
  of blood and screen-shake. Loot tops one up. The HUD shows the grenade count. Verified headless
  (`zomboid_sim`): the pool builds, throwing spends exactly one and arms a grenade, a detonation in the
  middle of a three-zombie cluster kills all three at once, throwing with none left does nothing, and
  loot refills a grenade.
- **Combo multiplier** — fast, unbroken kills build a score multiplier (+1× every 5 kills in the
  streak, capped at 5×) that decays after ~2.5 s without a kill, rewarding aggressive play. Each kill
  scores `score_value × multiplier`. The HUD flashes the live combo. Verified headless (`zomboid_sim`)
  against a hand-computed total: five back-to-back walker kills reach a 5-streak / 2× and score exactly
  60 (four at 1× + one at 2×), and the streak resets to 1× after the window elapses with no kills.
- **Persistent high score + restart** — the best run (best wave + best score) is saved to the platform
  pref dir via `core::KeyValueStore` and shown on the HUD; on death the game saves a new best (flagging
  "NEW BEST!") and offers "PRESS ENTER TO RESTART", which rebuilds a fresh SceneTree for a clean run.
  Verified headless (`zomboid_sim`): `beatsBest` ranks runs correctly (score primary, wave tiebreak),
  and a best written through `KeyValueStore.save()` reloads intact.
- **Melee shove** — press F (or, on autopilot, when a zombie is point-blank) for a free close-range
  swing that damages and knocks back every zombie in a short radius, then goes on a ~1-second cooldown.
  It costs no ammo, so it's the last-resort answer when a walker is right on top of you or you're mid-
  reload. Verified headless (`zomboid_sim`): a swing strikes only the point-blank zombie (damaging and
  knocking it back), leaves a distant zombie untouched, sets the cooldown, and a second swing during
  cooldown is refused and deals no further damage.
- **Dodge-roll** — press SPACE (or, on autopilot, when a zombie crowds you) for an evasive burst in
  your movement/aim direction, with a brief window of invulnerability (i-frames) that ignores all
  damage mid-roll, then a ~4-second cooldown. The survivor ghosts translucent-blue while invulnerable.
  It rewards timing dodges through spitter volleys, exploder rushes, and boss slams. Verified headless
  (`zomboid_sim`): the dodge fires and sets i-frames + cooldown, a second dodge is refused until it
  cools down, damage taken mid-roll is fully ignored, the burst carries the survivor, and damage lands
  normally again once the i-frames lapse.
- **Supply crates** — every ~30 seconds a care package drops on the ring around the survivor (an
  object-pooled `Crate`); reach it before it expires for a big refill: heal 50, +2 grenades, and a
  generous ammo top-up for all four weapons. It gives a reason to reposition between fights and a
  periodic goal. Verified headless (`zomboid_sim`): the pool builds, a dropped crate becomes active,
  and collecting it grants the grenades, ammo, and heal.
- **Pickup magnetism** — medkits and power-ups within a short radius (~6 units) drift toward the
  survivor instead of sitting inert, so you scoop them up by getting close rather than standing exactly
  on them. Verified headless (`zomboid_sim`): a medkit 5 units away slides toward the survivor over a
  tick while one 20 units away stays put.
- **Health medkits** — a slain zombie has a ~12% chance to drop a medkit; walk over it to heal 40
  (capped at max health). Kits are an object pool, blink as they near expiry, and vanish if ignored,
  giving a reason to push into danger for a top-up. Verified headless (`zomboid_sim`): the pool builds,
  `drop_medkit` activates a kit, walking onto it heals and consumes it, the heal is capped at max, and
  an ignored kit expires.
- **Exploder zombies** — a fifth zombie kind (from wave 4): a fast, fragile suicide bomber that
  detonates when killed, dealing area-of-effect damage to the survivor if they are within ~5 units,
  so it must be shot from a distance rather than let close. Its blast now also catches **nearby
  zombies** (40 damage in the same radius, other exploders excluded so the chain stays bounded), so an
  exploder popped inside a pack can take the pack with it — a double-edged crowd tool. The blast throws
  extra particles and a bigger screen-shake, and the Director folds exploders into the mix on later
  waves. Verified headless (`zomboid_sim`): a detonating exploder hurts a nearby walker while a distant
  one is untouched. Verified headless
  (`zomboid_sim`): a spawned exploder is faster and more fragile than a walker, dying next to the
  survivor drains their health while an identical exploder dying far away does not, and a forced wave 4
  actually contains an exploder.
- **Spitter zombies** — a sixth zombie kind (from wave 5): the horde's one ranged threat. Instead of
  charging, a spitter halts at a distance and lobs object-pooled acid globs at where the survivor was
  standing, on a cooldown; a glob travels and splashes on arrival, hurting the survivor only if they
  are still near the impact — so it can be side-stepped, but a spitter left alone will chip you down
  and must be prioritized. The Director folds spitters into later waves. Verified headless
  (`zomboid_sim`): a spawned spitter holds its ground rather than closing to melee, a tick puts an acid
  glob in the air, and the glob travels and lands on a stationary survivor to drain their health.
- **Overcharge ultimate** — every kill charges a meter (25 kills to full); once ready, press Q to
  detonate a screen-wide blast that hammers every live zombie on the field for 500 damage — wiping
  ordinary hordes outright — then the meter resets. The HUD shows the charge and flashes "OVERCHARGE
  READY"; autopilot fires it the moment it fills. Verified headless (`zomboid_sim`): charging the meter
  to full sets the ready flag, a detonate clears a cluster of live zombies to zero, and the charge is
  consumed back to empty.
- **Out-of-combat regeneration** — take no damage for ~5 seconds and the survivor slowly heals (4 HP/s)
  back toward full, rewarding disengaging and repositioning between fights; any hit resets the delay, so
  standing in the horde never heals you. Verified headless (`zomboid_sim`): health holds steady inside
  the post-hit delay window, climbs once the delay elapses, and never overshoots max health.
- **Adrenaline (last stand)** — dropping below 25% health triggers a passive fire-rate surge (×1.5),
  turning a near-death moment into a fighting chance instead of a slow bleed-out; the boost layers on
  top of the upgrade and power-up multipliers and drops away the instant you heal back above the
  threshold. The survivor pulses red-hot while it's active. Verified headless (`zomboid_sim`): crossing
  below 25% health flips on the adrenaline flag and lifts the fire rate, and healing back above it
  reverts both to baseline.
- **Power-up pickups** — a slain zombie rarely drops an object-pooled power-up; walk over it for a
  short timed buff: rapid fire (fire rate ×2.2), double damage (×2.2), or a shield (all incoming
  damage negated). Buffs layer over the permanent upgrade multipliers and cleanly revert when the
  ~8 s timer lapses; a fresh pickup refreshes it. The survivor is tinted by the active buff (blue for
  shield). Verified headless (`zomboid_sim`): the pool builds, collecting a rapid-fire kit multiplies
  the fire rate and it falls back to base once the window elapses, and a shield fully soaks a 50-damage
  hit.
- **Sound effects** — procedural SFX (`audio::Audio`) for gunfire (per-weapon pitch), zombie deaths,
  reloads, grenade/boss booms, wave starts, taking a bite, and dying. The app fires one-shots by
  watching simulation state change frame-to-frame — no script hooks needed — and resyncs on restart so
  a fresh run stays silent until it acts. Audio degrades gracefully with no device (headless smoke
  runs clean, exit 0); the sound is owner-audible on real hardware.

### Scripting & scene
- `maz::script` — a from-scratch, header-only scripting language (the GDScript competitor): values,
  collections, stdlib+RNG, closures, classes + inheritance + `super`, host binding + lifecycle,
  signals + deferred dispatch, safety budgets + stack traces, hot reload, gradual typing, modules +
  introspection + debugger hooks.
- `scene::SceneTree` / `SceneNode` — unified node hierarchy with 2D transform composition, attached
  scripts, groups, path lookup, and `get_node`/`has_node` script natives; plus `.tscn`-style
  text serialization (`scene::saveTree`/`loadTree`).

### Asset pipeline
- `core::AssetServer<T>` — asynchronous threaded loading with status/progress, dedup + ref-counting,
  and stamp-based reimport / hot reload (Godot's `ResourceLoader` threaded API).

### Infrastructure
- `platform::CrashHandler` — fatal-signal backtrace dump to stderr + a crash-log file, with symbol
  demangling (Godot's `CrashHandler`).
- `tools/package.sh` — one-command self-contained game export with a launch-verify step (Godot's
  "Export Project").
- API docs: `tools/gen_api_docs.py` → `docs/API.md` (144 headers) + a `Doxyfile`.
- `core::Telemetry` — opt-in, privacy-first analytics buffer (off by default, no built-in network
  transport, JSONL, no PII).

### Core systems
- `core::Replay<T>` — deterministic input record/replay with RLE serialization.
- `core::Checkpoints` — named save slots + a rolling rewind ring (rewind mechanics / rollback).
- `core::LinearArena` + `core::PoolAllocator` — frame arena + fixed-size pool allocators.
- `core::SmallVector<T,N>` + `core::SparseSet<T>` — inline-buffer vector + O(1) sparse set.
- `core::TypeDesc<T>` — minimal, type-safe reflection (member-pointer props + serialize).
- `core::DateTime` + `core::GameClock` — deterministic epoch↔calendar + in-game clock.
- `core::Version` — semantic version type (parse/compare) + build macros.

### Spatial & IO
- `game::Quadtree` + `game::Octree` — 2D/3D bounded-region spatial partitioning for broadphase
  culling and range queries.
- `io::VirtualFileSystem` — `res://` / `user://` scheme paths with normalization and a
  traversal-escape guard.

### Build & CI
- Cross-platform GitHub Actions CI (Linux build+test; macOS/Windows unit tests).
- Builds pass `-rdynamic` so crash backtraces resolve symbols.

---

_This project is under active development toward parity-or-better with the Godot game engine. Where a
capability can't be matched honestly (e.g. platform-specific packaging, GPU features requiring a real
device), it is documented as pending rather than claimed._
