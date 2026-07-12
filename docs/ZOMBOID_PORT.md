# ZOMBOID: ANCHORAGE — native port (Maz Engine, Phase 13)

The flagship game for Maz Engine is a native port of the browser reference game
(`index.html` + `js/`). This document tracks how the port is structured and what remains.

## Structure

```
game/
  include/zomboid/
    Math.hpp     Vec2 + clamp/lerp/dist/normAngle (engine-agnostic, no GLM)
    Rng.hpp      PCG32 deterministic RNG (replaces JS Math.random())
    Tiles.hpp    Tile enum + isSolidTile
    World.hpp    Anchorage data (avenues/streets/buildings) + World + generateWorld()
    Items.hpp    ItemDef DB + themed loot tables + rollLoot()
    Sim.hpp      Player/Zombie/Bullet/Corpse + the deterministic Sim
  src/           World.cpp · Items.cpp · Sim.cpp
apps/zomboid/    headless autopilot driver (proof-of-life; prints a run summary)
tests/           zomboid_tests.cpp (worldgen, determinism, needs, combat, loot, day/night)
```

The `zomboid` library is **deliberately free of SDL3/Vulkan/GLM**: the whole simulation
compiles and unit-tests headless on any C++20 toolchain, so gameplay logic is verified in CI
without a GPU or display. Rendering, input, and audio are the *app's* job and bridge onto the
same `Sim` — gameplay code never touches the engine backend.

## Design mapping (browser → native)

| Browser (`js/`)                     | Native (`game/`)                                  |
|-------------------------------------|---------------------------------------------------|
| `world.js` data tables              | `World.cpp` `avenues()/streets()/buildings()`     |
| `worldgen.js` `generate()`          | `World.cpp` `generateWorld()`                     |
| `items.js` items + `rollLoot`       | `Items.cpp` `itemDef()` + `rollLoot()`            |
| `game.js` fixed-step sim            | `Sim.cpp` `Sim::step()` (fixed `kStep = 1/60`)    |
| `Math.random()` (unseeded)          | `zb::Rng` PCG32 (seeded, reproducible)            |
| keyboard/mouse event handlers       | `zb::Input` (per-tick intent struct)              |
| canvas rendering / WebAudio         | *(not in the sim — belongs to the render layer)*  |

Purely-visual state from the reference (particles, float-texts, screen shake, CRT/scanline
post-fx) is intentionally excluded from the sim; it will live in the render layer.

## What's done

- Deterministic, fixed-timestep simulation with the full survival loop: five decaying needs
  (health/hunger/thirst/fatigue/mood), infection, natural regen.
- Anchorage tile worldgen: water/greenbelt, avenue+street grid, 37 named landmark buildings
  with doors, parking aprons, and richness-scaled loot containers.
- Item database and themed weighted loot tables (grocery/hospital/police/hardware/…).
- Combat: melee arc with weapon durability + breakage; ranged (pistol single, shotgun spread)
  with ammo consumption; bullets vs. tiles and zombies.
- Zombie AI: sight/flashlight aggro, wander, knockback, contact damage + bite infection chance;
  sprinters. Day/night cycle drives denser/faster night waves up to a day-scaled cap.

## What's next

1. **Render bridge** — a `Camera2D` + sprite/tilemap batch on the Vulkan renderer (Phase 3),
   drawing tiles, entities, and a HUD from `Sim` state.
2. **Input bridge** — map SDL keyboard/mouse (Phase 1 action-mapping) into `zb::Input`.
3. **Audio** — chiptune SFX/music via the Phase 7 audio module.
4. **Save/load** — serialize `Sim` state (Phase 10).

Run it now:

```
./build/bin/zomboid --ticks 3600 --seed 7   # 60 game-seconds of headless autopilot
ctest --test-dir build -R zomboid            # run the sim unit tests
```
