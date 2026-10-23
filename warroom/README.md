# War Room 1936

**A grand-strategy game of the Second World War, on a globe, in your browser.** Pick any
country of January 1936 and run it hour by hour. This is Stage 1 of a game built in the
style of Hearts of Iron IV: the whole world of 1936, every province and state in place, a
running clock with weather, and the pieces that the later stages (economy, politics,
armies, navies, air forces, diplomacy and AI) will move. The master design, listing every
system and the stage that builds it, is in
[`docs/superpowers/specs/2026-10-23-warroom-design.md`](../docs/superpowers/specs/2026-10-23-warroom-design.md).

**Open it:** [warroom/](./index.html) — on the live site,
`https://mazoo85.github.io/Maz/warroom/`. Works on a phone or a computer. No account, no
install. To run it from a checkout, serve the repository root (it loads its data files, so
it cannot run from a bare `file://` URL):

```sh
node warroom/tests/serve.mjs          # then open http://localhost:8777/warroom/
```

A standalone app: it is not part of MAZ ARCADE, and it is not built on the Maz engine.

## What it does

| | |
|---|---|
| **The world of 1936** | 12,000 land provinces in 1,300 states, 3,000 sea provinces, 80 countries with their 1936 borders: East Prussia German, Lwów Polish, Manchukuo, the colonial empires, the Chinese warlords. Drawn as a globe: drag to spin, pinch or scroll to zoom. |
| **Map modes** | Political, terrain, states, strategic regions, today's weather, victory points. |
| **Tap anything** | A province shows its terrain, state, owner, victory points, region, weather and neighbours (a `~` marks a river crossing). A state shows its owner, cores, capital, manpower and infrastructure. A country shows its leader, ideology, capital and every state, with *Play as* to take it over. |
| **Find a place** | Type a city, state or country name in the box at the top. |
| **The clock** | Five speeds and pause (space, 1–5, + and −). One tick is one hour. The night side moves with the sun; the weather changes every day, region by region, by climate and season. |
| **Counters** | The historical armies of 1936 stand at each capital as 3D counters made in Blender, tinted by country. They do nothing yet; Stage 4 moves them. |
| **Saves** | Named saves in the browser, an autosave every game month and whenever you leave the page, and a `.warroom` file you can download and open on another device. |

## How it is built

- **`js/sim/`** — the simulation: plain JavaScript, no browser in it. One state object,
  changed only through commands, advanced one hour per tick, deterministic for a seed.
  Everything the later stages add goes here.
- **`js/view/`** — the globe (three.js, vendored) and the panels. The world is one sphere;
  a province-id texture says which province every pixel belongs to and a small lookup
  texture gives each province its colour, so a map mode change costs one tiny upload.
- **`pipeline/`** — Python that cuts the world from public data: Natural Earth's
  first-level regions become states (merged where tiny, split where huge), provinces are
  grown inside them, the sea is tiled, terrain comes from a 10-arc-minute elevation grid,
  the Köppen climate map and Natural Earth's physical regions, and a hand table gives every
  modern region its 1936 owner. `python pipeline/build_map.py` rebuilds `data/` in about
  two minutes; CI checks that the committed data still matches.
- **`blender/`** — `build_models.py` builds every counter, vehicle, ship, plane and
  building marker from primitives in Blender (as the `bpy` module or a full Blender) and
  exports them to `models/*.glb`. CI rebuilds them too.
- **`data/`** — the generated map and the authored tables (countries, weather, terrain,
  the 1936 scenario). JSON, checked against a schema when the game starts.

## Tests

```sh
for t in warroom/tests/*.test.js; do node "$t"; done     # simulation, data and model checks
node warroom/tests/smoke.playwright.mjs                   # headless Chromium: needs `npm install playwright`
```

The smoke test opens the page, checks that the globe draws, taps a province, switches map
modes, runs the clock, saves and reloads, and leaves screenshots in `warroom/tests/out/`.

## Sources and licences

Map shapes, rivers, lakes, towns and physical regions: [Natural Earth](https://www.naturalearthdata.com/)
(public domain). Elevation: ETOPO1 as repackaged by
[fatiando-data](https://github.com/fatiando-data/earth-topography-10arcmin) (CC-BY). Climate:
the Köppen-Geiger map of Beck et al. via [kgcpy](https://pypi.org/project/kgcpy/). Historical
facts (borders, leaders, army sizes) are facts; the name, art, text and data files of this
game are its own. three.js is MIT-licensed (see `vendor/three/LICENSE`).
