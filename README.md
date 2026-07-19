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

- **Instruments** — a synthesized drum kit (`DrumVoice` — kick/snare/hats/clap/**tom**/**cowbell**/**rimshot**/**crash cymbal**/**ride cymbal**/**shaker**/**clave**/**tambourine**/**conga**/**woodblock**/**bongo**, **per-channel selectable** per row, per-hit velocity, **choke groups**, per-channel **tuning** + **decay** + **drive/saturation** + **flam** + **pitch-envelope depth / "punch"** + **pitch-envelope time** (together shape the kick/tom pitch sweep — flat sub → snappy click → long 808 boom) + **tone** (a per-drum low-pass to darken bright hits, FL channel-filter style) + a snare **snap** (noise-vs-tone balance, from tom-like body to a crisp wire crack)); **two** polyphonic
  synths (lead + bass, `SynthInstrument`) with **subtractive** (a **three-oscillator** stack — five selectable shapes per oscillator (sine / square / saw / triangle / **trapezoid** — a clipped triangle, warmer than a square yet brighter than a triangle) + detuned 2nd osc + **coarse-tunable** 2nd & 3rd oscs (+ **fine-detune** on osc3 for a fatter beating stack) + **ring mod** + sub +
  noise + a selectable-waveform, **-1/-2 octave sub** + **unison** supersaw stack + **2nd & 3rd oscillators each with their own selectable waveform** (3xOSC-style, or linked to the primary), a resonant state-variable **filter** — **low-pass/high-pass/band-pass/notch** selectable — with envelope + **velocity sensitivity** + **key tracking** + a **cutoff LFO** (wobble/auto-wah, with a selectable **shape** — sine/square/saw/triangle/trapezoid, so the wobble can step, gate, or ramp — **tempo-syncable** to note divisions for rhythmic filter gating) + a **tremolo/amp LFO** (with a selectable **shape** — sine/square/saw/triangle/trapezoid, so a square gives a hard on/off gate; also **tempo-syncable** for a synth trance-gate) + a **tempo-syncable vibrato** (pitch LFO lockable to note divisions, with a selectable **shape** — sine/square/saw/triangle/trapezoid, so a square gives a two-pitch trill) + a **dedicated filter envelope** (its own ADSR sweeping the cutoff, bipolar depth in Hz) + **filter drive** (pre-filter tanh overdrive for analog growl)), **FM** (with operator **feedback** + **velocity→index** for touch-sensitive brightness), and **wavetable** (a morphing
  single-cycle table with **selectable frames**, scanned by an envelope- **and LFO**- **and velocity**-sweepable position) engines, plus **portamento/glide** (with a **legato-only** mode — glide just on overlapping notes), **analog drift** (per-note detune for analog warmth), **start-phase randomization** (each note begins its oscillators at a random phase for analog-style variation), **vibrato** (with an **onset delay** that fades it in on sustained notes), **tremolo** (amplitude LFO), a **pitch envelope**, **oscillator hard sync**, **pulse width (PWM)** with a **PWM LFO** (lush sweeping duty cycle), a per-instrument **octave** shift, and a **mono** (single-voice) mode;
  and a **`Sampler`** (WAV playback, pitch-shifted per note, with **reverse**, **loop** (incl. **ping-pong** bounce and an adjustable **loop region** — sustain just the body — with a **loop crossfade** to smooth the seam), a **beat slicer** (map N equal slices across the keyboard, Slicex-style), **start-offset**, a full **amp ADSR envelope** (attack/decay/sustain/release volume shaping — plucks, swells, held pads), a **velocity→volume sensitivity** (from fully dynamic down to velocity-independent one-shots), a resonant **low-pass filter** (per-voice cutoff + resonance, with its own **filter envelope** sweeping the cutoff and **velocity → cutoff** for touch-sensitive brightness), a **pitch envelope** (each note starts up to ±36 st away and slides to its true pitch — zaps, risers, 808 glides), a **monophonic mode** (last-note priority for tight mono sampled bass/leads), **fine tune**, **normalize**, and **fade in/out**).
- **Sequencing** — a `Sequencer` channel rack (step grid with per-step velocity, **probability**, and **ratcheting**), **two** `PianoRoll`
  lanes (lead + bass) with a **chord tool** (maj/min/7ths/sus/6ths/9ths/**dim7**/**m7♭5**), **per-note probability**, **per-note fine tune** (micro-detune in cents), **per-note roll/ratchet** (a note re-strikes 1–8 times across its first step for melodic rolls/stutters — Shift+scroll a note), **quantize** (adjustable strength), **scale-snap** (snap notes to major/minor/modes/pentatonic/blues/whole-tone/chromatic), **strum** (roll stacked chords), **legato** (glue notes to the next), **invert** (mirror the melody around a pivot), **reverse** (flip the rhythm in time), **humanize** (randomize velocity + timing), **velocity ramp** (linear crescendo/decrescendo across the phrase), **chop** (split notes into evenly-spaced repeats — rolls/stutters), **arpeggiate** (bake a chord into a printed up/down/up-down arpeggio of notes), **transpose** (shift all notes by semitones), **shift** (nudge the whole phrase earlier/later in time, wrapping), **stretch** (time-scale the phrase faster/slower), **gate** (scale note lengths for staccato/legato without moving starts), and **duplicate** (repeat the phrase), **per-step pitch** (a channel-rack graph-editor pitch row — Ctrl+scroll a step to tune that hit ±24 semitones), **per-step micro-timing nudge** (Ctrl+Shift+scroll to push a single hit up to 95% of its slot later for a laid-back, behind-the-beat feel — a finer-grained cousin of swing), per-channel **step-row rotate** (shift a groove around the bar) and **Euclidean fill** (evenly distributed pulses), a **resizable pattern length** (8–64 steps) with adjustable **steps-per-beat** (16ths, triplets, 32nds), **multiple named + clonable patterns + a playlist** (looping or play-once, with an optional **loop region** to cycle a section) to build songs, **per-pattern swing** (each pattern grooves on its own), **humanize**,
  an **arpeggiator** (up/down/up-down/random/as-played/**chord** (rhythmic full-chord stabs), 1–4 octave range, adjustable **gate length** for staccato/legato, and a **rate** of 1–8 steps per note to run slower than the grid), a **global transpose**, a **metronome** (with an adjustable **level**) with **count-in**, and **sidechain** ducking (with a selectable trigger channel, plus **attack** and release times for an instant hard pump or a softer rounded duck).
- **Mixing** — per-channel volume/mute/solo/**pan**, **pan for the lead & bass buses**, true **stereo**, **per-bus insert strips**
  (drums / lead / bass, each with its own **noise gate** + high-pass + **transient shaper** (per-bus attack/sustain punch) + EQ + distortion + compressor + gain/pan/mute/**solo** + **per-bus reverb & delay aux sends**), a master effect
  chain (parametric EQ (low shelf + **two** independent sweepable mid bells + high shelf), tilt EQ (with an adjustable **pivot** frequency), aural exciter (high-band harmonic enhancer), low-pass, high-pass, distortion with selectable curves (soft/hard/fold/sine/**tube** — asymmetric, even-harmonic warmth) + a post **tone** low-pass + an **output-level** trim (drive hard, then gain-stage the result back down), tape saturation (with **wow & flutter** pitch wobble), ring modulator, bitcrusher (bit-depth + sample-rate crush with a post **tone** low-pass to smooth the aliasing fizz, Squeeze-style), noise gate (with hold + a **sidechain/key high-pass** so booming lows don't false-open it),
  compressor (soft-knee, parallel/dry-wet mix, **sidechain high-pass** so lows don't pump it, live **gain-reduction meter**), 3-band multiband compressor (Maximus-style — independent low/mid/high threshold + ratio over two crossovers, exact-reconstruction band split), transient shaper (attack/sustain designer), de-esser (frequency-selective high-band compression), chorus (with **feedback** for a deeper, flanger-edged voice + a **stereo width** control from mono to extra-wide), flanger (with an **invert** switch for the hollow through-zero flange), and phaser (all three **tempo-syncable**; the phaser has a selectable **stage count** for deeper sweeps + a **stereo** mode that offsets the L/R sweeps 90° apart for a wide swirl), delay (with ping-pong + damping + a feedback **low-cut** for dub/analog echoes + **modulation** for warm BBD/tape pitch wobble + **ducking** so the echoes bloom in the gaps of a loud dry signal + **tempo sync** to note divisions), stereo/dual delay (independent L/R times, each **tempo-syncable** to its own note division, with feedback **damping** + **low-cut** tone + a **ping-pong** mode that cross-routes the feedback so echoes bounce L↔R), reverb (with pre-delay + width + freeze + ducking + wet-tail low-cut/high-cut tone + a **gated** mode for the classic 80s cut-off tail), mid/side stereo
  widener, mono-bass maker, sub-bass generator (synthesizes an octave-down tone tracking the input's low end — club sub for weak kicks/basslines), auto-pan (with **tempo sync** to note divisions for rhythmic panning + a selectable LFO **shape** — sine/triangle/**square** for a trance-gate-style hard L↔R ping-pong), envelope filter / auto-wah (up **or downward**-sweeping, with a **dry/wet mix** for a parallel wah that keeps the body), comb resonator (with **damping** to mellow the metallic ring), tremolo / trance-gate (with a selectable **shape** — sine/square/triangle/saw for smooth, hard-gated, ramped, or fading rhythms — and **tempo sync** to note divisions), formant (vowel) filter (5 vowels + a continuous **vowel morph** for talkbox A→E→I→O→U sweeps), mid/side stereo widener (with **bass mono** — keeps lows centered while widening the highs), Haas stereo enhancer (widens even a mono source), utility (gain/phase-invert/mono), brickwall look-ahead limiter/maximizer (input gain + ceiling + release, guaranteed-ceiling output, live **gain-reduction meter**), soft/hard clipper (instantaneous zero-latency ceiling — drive + hardness morphs from tanh saturation to a flat-top clip, for loudness)),
  parallel **aux send/return buses** (reverb + delay), a master **balance/pan**, and a master **limiter** with an adjustable ceiling.
- **Modulation** — automation of master-EQ-cutoff/FM/reverb/master/delay-mix/distortion-drive/stereo-width/**synth-filter-cutoff**/**synth-filter-resonance**/**bass-filter-cutoff**/**bass-filter-resonance**/**delay-feedback**/**reverb-size**/**bitcrusher-mix**/**lead-bus volume**/**lead-bus pan**/**bass-bus volume**/**bass-bus pan**/**drum-bus volume**/**drum-bus pan**/**reverb-send**/**delay-send**/**master-pan** (the classic lead sweep + acid Q builds + bus volume rides on every bus + drum drops/builds + auto-pan + parallel-send risers/throws + whole-mix auto-pan), synced to the transport, from either an
  **LFO** (optionally **tempo-synced** to note divisions from 4 bars down to 1/8) or a drawn **automation clip** (FL-style breakpoint envelope, linearly interpolated + looped).
- **I/O** — an `AudioEngine` that drives an SDL3 device *or* renders offline, **project save/load**
  (`.cjc`), WAV **bounce**, **stem export** (per-bus drums/lead/bass WAVs through their track inserts, pre-master, in one pass), **MIDI export + import** (`.mid` in and out), session **recording** (output + SDL
  mic/line input capture), and **plugin hosting**: a native `.so` ABI
  ([`PluginApi.h`](engine/include/maz/audio/PluginApi.h), see `plugins/example_tremolo`), the
  open [CLAP](https://cleveraudio.org) format (`ClapHost`, see `plugins/example_clap`), **and
  [VST3](https://steinbergmedia.github.io/vst3_doc/)** (`Vst3Host`, see `plugins/example_vst3`) —
  hosted against Steinberg's **MIT-licensed** `pluginterfaces` headers only (no GPL `public.sdk`).
- **GUI** — a Dear ImGui interface (channel rack, piano roll, synth, bass, mixer with a **master peak/RMS level meter**, automation,
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
