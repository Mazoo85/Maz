# MAZ ARCADE

Everything in this repository, connected. The front door is
**[`index.html`](index.html)** — the MAZ ARCADE hub — which lists and links every
project below. Live on GitHub Pages:

### ▶ **https://mazoo85.github.io/Maz/**

Publishing is GitHub Pages' own **"deploy from a branch"** on the default branch,
serving the repo root — so every push to the default branch republishes the site,
with no workflow in the loop. The empty [`.nojekyll`](.nojekyll) file at the root
tells Pages to serve the tree verbatim rather than running Jekyll over it.

Every browser project also carries a small **MAZ pill in its top-left corner**:
tap it to jump straight to any other project, or back to the hub. Nothing here
is a dead end.

---

## What's inside

| | Project | What it is | Open it |
|---|---|---|---|
| 🎮 | **ZOMBOID: ANCHORAGE** | Open-world zombie survival across a tile-built replica of downtown Anchorage, drawn as a 1990s SEGA arcade title. Five decaying needs, day/night hordes, looting, firearms. | [play](zomboid/) · [docs](zomboid/README.md) |
| 🎮 | **NEON CELLS** | Roguelite action-platformer in the shape of Dead Cells: procedurally built biomes proved reachable before you enter them, permadeath, three scroll colours, weapons with rolled affixes, skills, mutations, two bosses, and blueprints that stay unlocked between runs. Plays on a controller, a keyboard or a phone. | [play](cells/) · [docs](cells/README.md) |
| 🎮 | **DEAD SECTOR** | Phone-first, top-down twin-stick zombie shooter in one self-contained HTML file. Dual touch joysticks, escalating waves. | [play](shooter/) · [docs](shooter/README.md) |
| 🎵 | **SONG FORGE** | Generative AI music maker: writes and plays complete songs — chords, bass, drums, arpeggio, melody — across 8 genres, with WAV and MIDI export. Offline, no API key. | [open](music/) · [docs](music/README.md) |
| 🎬 | **SCRIPT FORGE** | Type what your film is about and get the film: a formatted screenplay, a shot list, and an animated short — sets, camera, voices, a real SONG FORGE score — that plays in the page and downloads as an MP4 or WebM. | [open](film/) · [docs](film/README.md) |
| 🎲 | **NAME MAKER** | Press a button, get a name: one adjective, one noun, from a thousand of each. Installs to a phone home screen and works offline. | [open](namemaker/) · [docs](namemaker/README.md) |
| ✍️ | **MADLIBS STORY FORGE** | Randomly forges story ideas broken into scene beats, ready to seed a storyboard or script. | [open](madlibs/) · [docs](madlibs/README.md) |
| ⚙️ | **Maz Engine** | Native **C++20 + Vulkan + SDL3** game engine, 2D-first but architected so 3D drops in later. | [roadmap](docs/ROADMAP.md) · [architecture](docs/ARCHITECTURE.md) |
| 🕸️ | **maz-scrape** | Recipe-driven scraper for static HTML — point it at a YAML recipe, get JSONL/CSV/SQLite. | [docs](scraper/README.md) |
| 🤝 | **Maz Crew** | Runs a coding task through planner → coder → reviewer → tester, with human checkpoints. | [docs](crew/README.md) |
| ⚒️ | **The Forge** | The repo's own nightly loop: reads the roadmap, CI and `TODO` markers, picks one task, hands it to Maz Crew, and opens a draft PR. Verifies against every project declared to depend on what it changed. Never pushes to the default branch, never merges. | [docs](docs/FORGE.md) |

The list lives in **[`shared/projects.js`](shared/projects.js)** — one file, read by
both the hub and the in-app nav. Add a project there and it appears everywhere.

---

## How it's wired together

```
index.html              # the MAZ ARCADE hub — links to everything
shared/projects.js      # THE list of projects (hub + nav both read this)
shared/maz-nav.js       # the in-app nav pill, one <script> line per app
zomboid/  cells/  shooter/  music/  madlibs/  film/  namemaker/   # the browser projects
engine/   apps/  tests/  docs/           # Maz Engine (C++)
scraper/  crew/  forge/                  # Python tools
scripts/check-links.mjs # proves every link in the repo resolves
scripts/smoke-site.cjs  # drives the hub and every app in a real browser
```

Adding a project is three steps: drop its folder in, add an entry to
`shared/projects.js`, and put this one line before `</body>` in its HTML —

```html
<script src="../shared/maz-nav.js" data-current="your-project-id" defer></script>
```

`scripts/check-links.mjs` fails the build if you forget the last one.

---

## Checking it still hangs together

```
node scripts/check-links.mjs     # every link + the project manifest (no deps, instant)
node scripts/smoke-site.cjs      # boots the hub and every app in Chromium
node film/tests/film-logic.test.js   # SCRIPT FORGE's reader, writer, edit and exports
node cells/tests/cells-logic.test.js    # NEON CELLS' level reachability, combat maths and save
node cells/tests/cells-browser.test.js  # NEON CELLS itself: a real run in Chromium
```

The browser ones need Playwright once: `npm --prefix music/tests install`.

These all run automatically in **[Site CI](.github/workflows/site-ci.yml)** on
every push. **[All Checks](.github/workflows/all-checks.yml)** is the one button in the
Actions tab that runs every suite in the repo — engine, site, music, scraper and
crew — together.

| Workflow | Covers |
|---|---|
| [`ci.yml`](.github/workflows/ci.yml) | Maz Engine: builds under `-Werror`, headless ctest |
| [`site-ci.yml`](.github/workflows/site-ci.yml) | Links, the project manifest, SCRIPT FORGE logic, NEON CELLS logic + a real run, hub + every app in Chromium |
| [`music-ci.yml`](.github/workflows/music-ci.yml) | SONG FORGE composition logic + real-audio browser tests |
| [`scraper-ci.yml`](.github/workflows/scraper-ci.yml) | maz-scrape, offline (mocked transport) |
| [`crew-ci.yml`](.github/workflows/crew-ci.yml) | Maz Crew, offline (fake client) |
| [`all-checks.yml`](.github/workflows/all-checks.yml) | All of the above, on demand |

---

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
it runs in CI. See **[`docs/ROADMAP.md`](docs/ROADMAP.md)** for the full build plan and
**[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** for the design.

ZOMBOID: ANCHORAGE doubles as the engine's eventual flagship port target
(roadmap Phase 13).

---

## Running the browser projects locally

No build step, no dependencies. From the repo root:

```
python3 -m http.server        # then open http://localhost:8000
```

That serves the hub, and every project is one click from there.

> **Codebase memory:** this repo is set up with a
> [codebase-memory MCP server](docs/CODEBASE_MEMORY.md) that gives Claude persistent,
> cross-session memory about the project. It starts automatically in Claude Code — no setup needed.
