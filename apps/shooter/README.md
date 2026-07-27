# DEAD SECTOR (Maz Engine)

A top-down zombie shooter built natively on the **Maz Engine** — it renders through the engine's
2D sprite path (`Renderer::uploadTexture` + `drawSprite`), reads keyboard/mouse via `platform::Input`,
and runs on the engine's fixed-timestep `Clock`. This is the native counterpart to the browser game
in [`../../shooter/`](../../shooter/); same design, engine-driven.

> **Desktop build.** The engine has no touch input, so this version is keyboard + mouse. For phone
> play, use the browser build instead.

## Controls

| Action | Input |
| --- | --- |
| Move | `WASD` / arrow keys |
| Aim | mouse |
| Fire | hold left mouse button (or `SPACE`) |
| Retry (when dead) | `SPACE` / `ENTER` |
| Quit | `ESC` |

## Build & run

Part of the top-level CMake build (needs the Vulkan SDK + SDL3, per the repo README):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/shooter                 # play (needs a GPU + display)
./build/bin/shooter --headless --frames 20   # CI: run + exit cleanly, no display
```

## Layout

- **`Sim.hpp` / `Sim.cpp`** — the gameplay simulation: player, zombies (walker/runner/brute), bullets,
  particles, pickups, wave pacing, and all collision/spawn rules. **Engine-independent** (no renderer,
  window, Vulkan, or GLM) and deterministic via a seeded RNG, so it's unit-tested with no GPU/display
  (see [`../../tests/unit_shooter.cpp`](../../tests/unit_shooter.cpp)).
- **`Font.hpp`** — a tiny 5×7 bitmap font (the engine has no text renderer); baked into an RGBA atlas
  at startup and drawn as sprite sub-rects for the HUD.
- **`main.cpp`** — the app/presentation layer: window + renderer setup, procedural textures (a soft
  disc, a solid pixel, the font atlas), camera follow, HUD, and the draw loop. Contains no gameplay
  rules — it only turns input into a `shooter::Input` and paints the `shooter::Sim` state.

## Tests

- `unit_shooter` — pure-logic checks on the simulation (waves, combat, death/restart, scoring).
- `shooter_headless_smoke` — boots the app headless for a few frames and requires a clean exit,
  exercising texture upload + the whole draw path wiring with no GPU.
