# Maz

This repo holds three things:

1. **Maz Engine** — a native **C++20 + Vulkan + SDL3** game engine, 2D-first but architected so
   3D drops in later. See **[`docs/ROADMAP.md`](docs/ROADMAP.md)** for the full build plan (the
   "massive list") and **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** for the design.
2. **ZOMBOID: ANCHORAGE** — the browser game below, which is both a design reference and the
   engine's eventual flagship port target (roadmap Phase 13).
3. **MadLibs Story Forge** — a zero-dependency browser app in
   **[`madlibs/`](madlibs/)** that randomly forges story ideas (broken into
   scene beats) to seed storyboards and scripts. See
   **[`madlibs/README.md`](madlibs/README.md)**.

> **Codebase memory:** this repo is set up with a
> [codebase-memory MCP server](docs/CODEBASE_MEMORY.md) that gives Claude persistent,
> cross-session memory about the project. It starts automatically in Claude Code — no setup needed.

## Building Maz Engine

Requires a C++20 compiler, CMake ≥ 3.24, the **Vulkan SDK** (loader + `glslangValidator`), and
on Linux the X11 dev packages. SDL3 and GLM are fetched automatically.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/sandbox                 # window + animated clear color (needs a GPU + display)
./build/bin/sandbox --headless      # CI: init, run, exit cleanly with no display/GPU
ctest --test-dir build              # headless smoke test
```

The current milestone is **M0** — the walking skeleton: a window, a fixed-timestep loop, a
Vulkan clear-screen renderer, and clean shutdown. It degrades gracefully with no GPU/display so
it runs in CI.

---

# ZOMBOID: ANCHORAGE

A browser-playable, **Project Zomboid–style** open-world zombie survival game,
rendered as a **1990s SEGA 16-bit arcade title** and drenched in **neon**. The
map is a tile-built replica of **downtown & midtown Anchorage, Alaska** — real
avenues, real streets, and real building names.

No build step, no dependencies. It's pure HTML5 Canvas + vanilla JavaScript.

```
open index.html        # double-click it, or:
python3 -m http.server  # then visit http://localhost:8000
```

---

## The vibe

- **SEGA-90s presentation** — a "NEON GENESIS ARCADE presents" boot screen with a
  faux "SEEE-GAAA" jingle, a synthwave **PRESS START** title with neon sun and
  perspective grid, chunky 16-bit drop-shadow logotype.
- **Neon everything** — hot-pink / cyan / purple / acid-green HUD bars, glowing
  doorways, neon zombie eyes, muzzle flashes, scanlines + CRT vignette overlay.
- **Chiptune audio** — WebAudio square/triangle/saw blips and a looping
  Genesis-style bassline. Toggle with **L**.

## Survival (the Zomboid part)

Five decaying needs drive everything:

| Stat | Drains from | Fix it with |
|------|-------------|-------------|
| **Health**   | bites, starvation, dehydration, infection | bandages, painkillers, first-aid kits |
| **Fed**      | time | canned salmon, moose jerky, Moose's Tooth pizza, chips |
| **Hydro**    | time (fastest) | bottled water, soda, Kaladi coffee |
| **Energy**   | running, time | coffee, energy drinks |
| **Mood**     | exhaustion, low health | eating well, surviving |

- **Bites can infect you.** Infection climbs, drains health, and is only cured by
  **antibiotics** (loot the hospitals).
- **Day/night cycle.** Nights go dark (use **F** for the flashlight) and spawn
  faster, denser hordes — including **neon-pink sprinters**.
- **Loot** the glowing orange crates inside buildings with **E**. Loot tables are
  themed: police stations have guns & ammo, hospitals have meds, hardware stores
  have axes & crowbars, groceries have food.
- **8-slot hotbar inventory**, melee weapons with durability, and two firearms
  (M9 pistol, Mossberg 500) with ammo.

## Controls

| Key | Action |
|-----|--------|
| **WASD / Arrows** | Move |
| **Mouse** | Aim |
| **Left-click / Space** | Attack / fire |
| **E** | Loot crate / interact |
| **1–8** | Use / equip hotbar slot |
| **R** | Reload firearm |
| **Tab** | Inventory |
| **M** | Anchorage tactical map |
| **F** | Flashlight |
| **P** | Pause |
| **L** | Mute / unmute audio |

## The Anchorage map

A 160×140 tile grid. **Cook Inlet** to the west, the **Chugach** treeline to the
east, **Ship Creek** and the **Alaska Railroad Depot** along the north.

**Avenues (E–W):** Ship Creek · 1st · 3rd · 4th · 5th · 6th · 9th · Delaney Park
Strip · 15th · Northern Lights Blvd · Benson Blvd · Tudor Rd.

**Streets (N–S):** L · K · I · G · E · C · A · Cordova · Gambell · Ingra · Lake
Otis Pkwy · Boniface Pkwy.

**Named buildings include:** Hotel Captain Cook · 4th Avenue Theatre · Egan
Convention Center · Dena'ina Center · Alaska Center for the Performing Arts ·
Anchorage Museum · Z.J. Loussac Library · Snow City Cafe · Glacier Brewhouse ·
49th State Brewing · Nordstrom (5th Ave Mall) · Carrs · Fred Meyer · Title Wave
Books · REI · Sullivan Arena · Merrill Field · Providence & Alaska Native &
Alaska Regional Hospitals · Moose's Tooth · Bear Tooth · Chilkoot Charlie's ·
Spenard Builders Supply · Dimond Center · University of Alaska Anchorage ·
Anchorage Police Dept · and more. You spawn at **Town Square Park (5th & C)**.

## Project layout

```
index.html        # shell + script load order
css/style.css     # arcade-cabinet styling, scanlines
js/world.js       # Anchorage data: avenues, streets, named buildings
js/worldgen.js    # bakes data into a tile grid + loot containers
js/items.js       # item definitions + themed loot tables
js/audio.js       # WebAudio chiptune SFX + music
js/game.js        # engine: states, input, sim, combat, rendering, HUD
```

> A note on "exact replica": this is an affectionate, playable homage built from
> scratch — original code and art, with Anchorage's real geography and place
> names recreated as a game world. It is not Project Zomboid's code or assets.

## Tools

- [`scraper/`](scraper) — **maz-scrape**, a general-purpose, recipe-driven scraper
  for static HTML pages. Point it at a YAML recipe (field → CSS selector); it
  crawls (following pagination/links), extracts records, and writes JSONL/CSV/SQLite,
  politely by default. See [scraper/README.md](scraper/README.md).
