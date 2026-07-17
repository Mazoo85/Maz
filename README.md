# Maz

This repo holds two things:

1. **Maz Engine** — a native **C++20 + Vulkan + SDL3** game engine, 2D-first but architected so
   3D drops in later. See **[`docs/ROADMAP.md`](docs/ROADMAP.md)** for the full build plan (the
   "massive list") and **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** for the design.
2. **ZOMBOID: ANCHORAGE** — the browser game below, which is both a design reference and the
   engine's eventual flagship port target (roadmap Phase 13).

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

## CJC Music Station

**CJC Music Station** is a native music-production app (a DAW) built on the engine's audio/UI layers,
aimed at FL Studio–level capability. It's a full groovebox/DAW: program patterns, arrange them into a
song, play them with real synths and samples, mix and process with effects, automate parameters, save
the project, and bounce to WAV.

`maz::audio` provides:

- **Instruments** — a synthesized drum kit (`DrumVoice`, per-hit velocity, **choke groups**, per-channel **tuning**); **two** polyphonic
  synths (lead + bass, `SynthInstrument`) with **subtractive** (dual detuned oscillator + sub +
  noise + **unison** supersaw stack + a resonant state-variable **filter** with envelope), **FM**, and **wavetable** (a morphing
  single-cycle table scanned by an envelope-sweepable position) engines, plus **portamento/glide**;
  and a **`Sampler`** (WAV playback, pitch-shifted per note, with **reverse**, **loop**, **start-offset**, and an **amp envelope**).
- **Sequencing** — a `Sequencer` channel rack (step grid with per-step velocity, **probability**, and **ratcheting**), **two** `PianoRoll`
  lanes (lead + bass) with a **chord tool** (maj/min/7ths/sus) and **per-note probability**, a **resizable pattern length** (8–64 steps) with adjustable **steps-per-beat** (16ths, triplets, 32nds), **multiple named + clonable patterns + a playlist** to build songs, **swing**, **humanize**,
  an **arpeggiator** (up/down/up-down, 1–4 octave range), a **global transpose**, a **metronome** with **count-in**, and **sidechain** ducking.
- **Mixing** — per-channel volume/mute/solo/**pan**, **pan for the lead & bass buses**, true **stereo**, **per-bus insert strips**
  (drums / lead / bass, each with its own EQ + distortion + compressor + gain/mute), a master effect
  chain (parametric EQ, tilt EQ, low-pass, high-pass, distortion with selectable curves (soft/hard/fold/sine), tape saturation, bitcrusher, noise gate,
  compressor, chorus, flanger, phaser, delay (with ping-pong), reverb (with pre-delay), mid/side stereo
  widener, mono-bass maker, auto-pan),
  parallel **aux send/return buses** (reverb + delay), and a master **limiter** with an adjustable ceiling.
- **Modulation** — automation of filter/FM/reverb/master/delay-mix/distortion-drive, synced to the transport, from either an
  **LFO** or a drawn **automation clip** (FL-style breakpoint envelope, linearly interpolated + looped).
- **I/O** — an `AudioEngine` that drives an SDL3 device *or* renders offline, **project save/load**
  (`.cjc`), WAV **bounce**, **stem export**, **MIDI export + import** (`.mid` in and out), session **recording** (output + SDL
  mic/line input capture), and **plugin hosting**: a native `.so` ABI
  ([`PluginApi.h`](engine/include/maz/audio/PluginApi.h), see `plugins/example_tremolo`), the
  open [CLAP](https://cleveraudio.org) format (`ClapHost`, see `plugins/example_clap`), **and
  [VST3](https://steinbergmedia.github.io/vst3_doc/)** (`Vst3Host`, see `plugins/example_vst3`) —
  hosted against Steinberg's **MIT-licensed** `pluginterfaces` headers only (no GPL `public.sdk`).
- **GUI** — a Dear ImGui interface (channel rack, piano roll, synth, bass, mixer, automation,
  arrangement, transport) rendered via Vulkan; a CI test drives it under software Vulkan.

See the **CJC Music Station track** in [`docs/ROADMAP.md`](docs/ROADMAP.md). All three plugin formats
(native ABI, CLAP, VST3) and live audio-input recording are implemented and, except for touching a
real audio device, exercised by the headless CI tests.

```
./build/bin/daw --headless --song --fm --auto --seconds 8 --wav song.wav   # full arrangement (bounce)
./build/bin/daw --headless --beat --melody --sample pluck.wav --wav sampled.wav  # melody via sampler
./build/bin/daw --headless --save song.cjc         # save the demo project to a .cjc file
./build/bin/daw --headless --load song.cjc --wav out.wav   # load a project and render it
```

```
cmake --build build                                    # builds the `daw` app too
./build/bin/daw                                        # window: channel rack + piano roll + test tone
./build/bin/daw --headless --beat --melody --seconds 4 --wav song.wav   # drums + melody to a WAV
./build/bin/daw --headless --beat --seconds 2 --wav beat.wav            # just the demo drum groove
./build/bin/daw --headless --seconds 1 --freq 440 --wav tone.wav        # a single oscillator tone
```

Flags: `--beat` (demo drum pattern), `--melody` (demo piano-roll melody), `--fm` / `--wt` (FM or
wavetable lead engine), `--bpm N` (tempo), `--freq HZ` (oscillator pitch), `--seconds N` (offline
length), `--wav PATH` (write the render).
Headless mode needs no audio device, so CI verifies the synth, drums, and melodic sequencing.

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
