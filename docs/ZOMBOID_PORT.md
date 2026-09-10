# ZOMBOID: ANCHORAGE — native port (Maz Engine, Phase 13)

The flagship game for Maz Engine is a native port of the browser reference game
(`index.html` + `js/`). This document tracks how the port is structured and what remains.

## Structure

```
game/
  include/zomboid/
    Math.hpp     Vec2 + clamp/lerp/dist/normAngle (engine-agnostic, no GLM)
    Rng.hpp      PCG32 deterministic RNG (replaces JS Math.random())
    audio/Audio.hpp  headless chiptune synth: SFX + music -> WAV
    Tiles.hpp    Tile enum + isSolidTile
    World.hpp    Anchorage data (avenues/streets/buildings) + World + generateWorld()
    Items.hpp    ItemDef DB + themed loot tables + rollLoot()
    Sim.hpp      Player/Zombie/Bullet/Corpse + the deterministic Sim
  src/           World.cpp · Items.cpp · Sim.cpp
    Save.hpp     bounds-checked binary Writer/Reader (bit-exact floats)
    render/
      Framebuffer.hpp  CPU RGB framebuffer (fill/blend/outline/circle/line)
      Font.hpp         5x7 bitmap font + drawText (HUD/labels)
      SoftRenderer.hpp software reference rasterizer: renderScene() + renderWorldMap()
  src/           World.cpp · Items.cpp · Sim.cpp · Save.cpp · Audio.cpp · SoftRenderer.cpp
apps/zomboid/    headless autopilot driver; --render (PNG, Png.hpp) · --save/--load · --screen
apps/zomboid-audio/ renders chiptune SFX / music / demo to WAV (headless)
apps/zomboid-tui/ PLAYABLE terminal front-end: keyboard -> zb::Input, framebuffer
                 rendered as truecolor half-block ANSI + text HUD (POSIX/termios)
tests/           zomboid_tests.cpp (worldgen, determinism, needs, combat, loot,
                 save/load, render, day/night) + zomboid-tui --once smoke
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

The sim maintains the reference's gameplay-adjacent effect state too: blood/muzzle **particles**,
floating **damage numbers** / loot pickups, and **screen shake** (`kick`) — fed from combat and
looting and advanced in the update loop, so they save/load and stay deterministic. The frame-level
CRT/scanline/vignette post-fx and the menu screens are pure render decoration and live only in the
render layer.

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

## Rendering

A dependency-free **software reference rasterizer** (`zomboid/render/`) draws a `Sim` frame into a
CPU `Framebuffer` using the neon palette — tiles + detailing, loot containers, corpses, zombies
(with eyes/health bars), the player + facing line, bullets, blood/muzzle particles, floating damage
numbers, the night darkness overlay, and a full
**HUD** (five stat bars with labels, the day/clock/kills/z-alive panel, equipped weapon, an 8-slot
hotbar, and the message log) drawn with a built-in 5x7 bitmap font (`Font.hpp`), finished with a
**CRT post-pass** (scanlines + vignette) and screen-shake camera jitter — plus the neon **title /
pause / death screens** (`renderTitleScreen`/`renderPauseScreen`/`renderDeathScreen`) and a
whole-world overview (`renderWorldMap`). The `zomboid` driver's `--render out.png` writes it via a tiny built-in
PNG encoder (`apps/zomboid/Png.hpp`), so you can *see* the port with no GPU:

```
./build/bin/zomboid --seed 7 --render frame.png --tile 20 --width 900 --height 680
./build/bin/zomboid --seed 7 --render map.png --map     # tactical overview
```

This is deterministic and unit-tested, and doubles as the seed for golden-image render tests
(Phase 12). It also defines the exact draw intent the Vulkan sprite batch will reproduce on the GPU.

## Play it (terminal)

`apps/zomboid-tui` is the first *playable* build: it puts the terminal in raw mode, maps the
keyboard into `zb::Input`, runs the Sim in a real-time fixed-step loop, and draws each frame as
truecolor half-block ANSI (two pixels per character cell) with a text HUD — no GPU or window.

```
./build/bin/zomboid-tui          # play: WASD move, SPACE attack, E loot, 1-8 use, F light, N new, Q quit
./build/bin/zomboid-tui --once   # headless: render one autopilot frame and exit (CI smoke)
```

## Sound (done, headless)

`zomboid/audio/Audio.hpp` ports the reference WebAudio chiptune: oscillator blips (square /
triangle / saw / sine) with an attack-decay envelope and pitch slide, plus filtered noise bursts.
All 14 SFX and the looping bass+lead city music render into PCM buffers with no audio device and out
to WAV, deterministically (seeded RNG), so the sound layer is testable in CI. `apps/zomboid-audio`
writes them:

```
./build/bin/zomboid-audio --sfx gun gun.wav      # one effect
./build/bin/zomboid-audio --music 40 city.wav    # the loop
./build/bin/zomboid-audio --demo demo.wav        # every SFX over the music bed
```

A real-time audio device (SDL/miniaudio, Phase 7) can stream these same recipes later.

## What's next

1. **Vulkan render bridge** — a `Camera2D` + sprite/tilemap batch on the Vulkan renderer (Phase 3)
   reproducing the software rasterizer's draw intent on the GPU, plus a HUD and text rendering.
2. **Input bridge** — map SDL keyboard/mouse (Phase 1 action-mapping) into `zb::Input`.
3. **Real-time audio** — stream the (already ported) SFX/music recipes through the Phase 7 audio
   device (SDL/miniaudio); the synthesis itself is done (see Sound above).

## Save / load (done)

`Sim::saveState()` serializes all mutable state — RNG stream, clock, player + inventory, zombies,
bullets, corpses, and opened-container flags — to a versioned binary blob; `loadState()` regenerates
the deterministic tile map and restores the rest, rejecting corrupt/truncated/mismatched data
without mutating the sim. Floats are copied bit-exact, so a loaded game continues *identically* to
one that was never saved (proven by the continuation test and via the CLI: a 600-tick run equals a
400-tick run that is saved, reloaded, and continued 200 more).

```
./build/bin/zomboid --seed 5 --ticks 400 --save mid.sav
./build/bin/zomboid --load mid.sav --ticks 200          # continues from the save
```

Run it now:

```
./build/bin/zomboid --ticks 3600 --seed 7   # 60 game-seconds of headless autopilot
ctest --test-dir build -R zomboid            # run the sim unit tests
```
