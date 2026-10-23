# War Room 1936 — Stage 1 (map and clock) implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A standalone browser game at `warroom/` that shows the world of 1 January 1936 as
a three.js globe with every province, state and country, a running hourly clock with
weather, map modes, province/country inspection, save/load, and Blender-made counters,
all tested under Node and headless Chromium and gated by its own CI workflow.

**Architecture:** A deterministic plain-JS simulation (`warroom/js/sim/`) owns all state
and changes it only through commands; a three.js view (`warroom/js/view/`) draws a
sphere whose fragment shader resolves a province-id texture through a small lookup
texture, so map modes and ownership changes cost one tiny texture upload. Map data is
produced offline by a Python pipeline from Natural Earth, a 10-arc-minute elevation grid
and a Köppen climate raster; 3D pieces are produced offline by a Blender (bpy) script.
Both outputs are committed and rebuilt in CI.

**Tech Stack:** ES modules in the browser (served from GitHub Pages or any static
server), three.js 0.170.0 vendored, Node 22 for tests, Playwright with the box's
Chromium for the smoke test, Python 3.12 + numpy + Pillow + netCDF4 + kgcpy for the map
pipeline, Python 3.11 + `bpy` 5.0 for models.

## Global Constraints

- Standalone: no entry in `shared/projects.js`, no `shared/maz-nav.js`, no hub link,
  own workflow `.github/workflows/warroom.yml`. Not on the Maz engine.
- `warroom/js/sim/**` never imports from `view/`, never references `window`, `document`,
  `Math.random` or `Date.now`.
- All state in one plain JSON-serialisable object; every mutation through
  `applyCommand(state, cmd)`; the tick is deterministic for a given seed.
- Province-id texture 8192×4096 RGBA (id in R,G,B little-endian; A = 255 land, 128 lake,
  0 sea). Lookup textures 128×128 RGBA.
- Data files are JSON under `warroom/data/`; loader fails fast naming file and field.
- Tests are plain Node (`node warroom/tests/<name>.test.js`), same harness style as
  `earth/tests/earth-logic.test.js`. The Playwright smoke test is the only browser test.
- Original name, art and text throughout. No Paradox assets or text.
- `node scripts/check-links.mjs && node scripts/check-exchange.mjs` must pass before
  every push.
- Commit messages: imperative, prefixed `warroom:`; attribution lines from the session.

---

## File structure

```
warroom/
  index.html                     page shell, importmap, script tags
  README.md
  css/warroom.css                phone-first layout, light/dark via prefers-color-scheme
  vendor/three/three.module.js   from three@0.170.0 (build/three.module.js)
  vendor/three/OrbitControls.js  from examples/jsm/controls, import 'three' via importmap
  vendor/three/GLTFLoader.js     from examples/jsm/loaders (+ BufferGeometryUtils.js)
  vendor/three/LICENSE
  js/main.js                     boot
  js/sim/rng.js                  xoshiro128** seeded RNG, per-stream
  js/sim/clock.js                hours ↔ date, tick scheduling
  js/sim/state.js                newGame(data, seed), reserved sections
  js/sim/commands.js             validateCommand / applyCommand
  js/sim/map.js                  province/state/region lookups, neighbours, BFS
  js/sim/weather.js              daily weather per strategic region
  js/sim/tick.js                 tick(state): fixed subsystem order
  js/sim/save.js                 toSave / fromSave with version + migrations
  js/sim/modifiers.js            modifier table scaffold (read by weather, grows later)
  js/data/load.js                fetch + schema checks (browser) / fs (node)
  js/data/schema.js              tiny schema checker
  js/view/globe.js               sphere, textures, shader, picking
  js/view/camera.js              OrbitControls wrapper, zoom levels, phone gestures
  js/view/mapmodes.js            id → colour per mode
  js/view/overlays.js            Blender counters, instanced
  js/view/panels/topbar.js
  js/view/panels/province.js
  js/view/panels/country.js
  js/view/panels/search.js
  js/view/panels/saves.js
  js/view/input.js               pointer/touch → selection
  data/map/provinces.png         id map
  data/map/provinces.json        [{id,name,type,terrain,state,region,coastal,vp,x,y,lat,lon}]
  data/map/adjacency.json        [[neighbour ids...], ...] indexed by province id, with
                                 river flags in rivers.json
  data/map/states.json           [{id,name,provinces,owner,controller,cores,category,
                                 manpower,resources,infrastructure,capital_province}]
  data/map/regions.json          strategic regions [{id,name,type,provinces,climate}]
  data/map/climate.json          per region climate zone
  data/common/countries.json     [{tag,name,adjective,color,capital_state,ideology,
                                 leader,flag}]
  data/common/terrain.json       terrain types with modifiers (used from Stage 4)
  data/common/weather.json       weather types and transition tables per climate
  data/scenarios/1936.json       start date, armies per country (counter placement)
  models/*.glb, models/manifest.json
  blender/build_models.py
  blender/build_relief.py
  pipeline/build_map.py          orchestrates the steps below
  pipeline/ne.py                 Natural Earth download + load (cached in pipeline/cache/)
  pipeline/raster.py             polygon rasterisation to the id grid (Pillow)
  pipeline/states.py             merge/split admin regions into states
  pipeline/provinces.py          Poisson seeds + nearest-seed assignment, land and sea
  pipeline/terrain.py            elevation + Köppen + regions → terrain
  pipeline/adjacency.py          neighbours, coasts, rivers, straits
  pipeline/owners_1936.json      modern country → 1936 owner, with per-region overrides
  pipeline/straits.json          declared sea crossings
  pipeline/terrain_overrides.json
  pipeline/vp_1936.json          victory points and capitals
  pipeline/requirements.txt
  tests/harness.js               test/assert/eq/near helpers (copied style from earth)
  tests/rng.test.js
  tests/clock.test.js
  tests/state.test.js
  tests/commands.test.js
  tests/map.test.js              data integrity: adjacency symmetric, reachability
  tests/weather.test.js
  tests/save.test.js
  tests/determinism.test.js
  tests/data.test.js             every data file loads against schema; references resolve
  tests/smoke.playwright.mjs     headless Chromium: globe renders, click → panel
  tests/serve.mjs                tiny static server for the smoke test
```

---

### Task 1: Scaffold, vendor three.js, CI, README

**Files:**
- Create: `warroom/index.html`, `warroom/css/warroom.css`, `warroom/README.md`,
  `warroom/vendor/three/*`, `warroom/js/main.js` (placeholder boot that logs),
  `.github/workflows/warroom.yml`, `warroom/tests/harness.js`
- Modify: nothing outside `warroom/` and `.github/workflows/`

**Interfaces:**
- Produces: `tests/harness.js` exporting `{test, assert, eq, near, done}` (CommonJS-free:
  ES module; `done()` prints the summary and sets `process.exitCode`).
- Produces: `index.html` with `<script type="importmap">{"imports":{"three":"./vendor/three/three.module.js","three/addons/":"./vendor/three/"}}</script>` and `<script type="module" src="./js/main.js">`.

- [ ] **Step 1: Copy three.js from the npm tarball** (`npm pack three@0.170.0`, copy
  `build/three.module.js`, `examples/jsm/controls/OrbitControls.js`,
  `examples/jsm/loaders/GLTFLoader.js`, `examples/jsm/utils/BufferGeometryUtils.js`,
  `LICENSE`). Rewrite GLTFLoader's `'../utils/BufferGeometryUtils.js'` import to
  `'./BufferGeometryUtils.js'`.
- [ ] **Step 2: Write `index.html`** with the importmap, a `<canvas id="globe">`, a
  `<header id="topbar">`, `<aside id="panel">`, `<nav id="mapmodes">`, a `<div id="boot">`
  loading message, and the module script.
- [ ] **Step 3: Write `tests/harness.js`** (`test`, `assert`, `eq`, `near`, `done`).
- [ ] **Step 4: Write `.github/workflows/warroom.yml`** with jobs `logic` (node tests),
  `smoke` (Playwright), `pipeline` and `models` (added by Tasks 5 and 8; add the jobs then).
- [ ] **Step 5: `node scripts/check-links.mjs && node scripts/check-exchange.mjs`** pass.
- [ ] **Step 6: Commit** `warroom: scaffold the standalone page, vendor three.js, CI`.

### Task 2: Seeded RNG and clock

**Files:** `js/sim/rng.js`, `js/sim/clock.js`, `tests/rng.test.js`, `tests/clock.test.js`

**Interfaces (produces):**
```js
// rng.js — xoshiro128** on four uint32 words stored in a plain array (serialisable)
export function seedRng(seed /* string|number */) → [a,b,c,d]
export function nextU32(s) → uint32   // mutates s
export function nextFloat(s) → [0,1)
export function nextInt(s, n) → 0..n-1
export function pick(s, array)
// clock.js — hour 0 == 1936-01-01 00:00
export const START = { year: 1936, month: 1, day: 1 };
export function hourToDate(hour) → { year, month, day, hour, dayOfYear, weekday }
export function dateToHour({year, month, day, hour=0}) → integer
export function formatDate(hour) → "1 Jan 1936, 00:00"
export function isNewDay(hour) / isNewMonth(hour) / isNewYear(hour)
export const SPEED_HOURS_PER_SECOND = [0, 1, 2, 6, 12, 24*2];  // index = speed 0..5
```
- [ ] Test: same seed → same sequence; different seeds differ; `nextInt(s, 6)` over 6000
  draws has each face between 850 and 1150; `pick` returns elements.
- [ ] Test: `hourToDate(0)` is 1 Jan 1936 00:00; leap day 29 Feb 1936 exists;
  `dateToHour(hourToDate(h)) === h` for h in a spread of values up to 1948;
  `formatDate(24*31)` is "1 Feb 1936, 00:00".
- [ ] Implement, run, commit `warroom: seeded rng and game clock`.

### Task 3: State, commands, tick, save

**Files:** `js/sim/state.js`, `js/sim/commands.js`, `js/sim/tick.js`, `js/sim/save.js`,
`js/sim/modifiers.js`, tests for each, `tests/determinism.test.js`

**Interfaces (produces):**
```js
// state.js
export function newGame(data, { seed, scenario }) → state
// state shape (reserved empty sections keep later saves compatible):
// { version: 1, seed, scenario: '1936', hour: 0, speed: 0, paused: true,
//   rng: { weather: [..], events: [..] },
//   countries: { [tag]: { tag, name, color, capital, states: [ids], ... } },
//   states: { [id]: { owner, controller, cores: [tags], buildings: {}, ... } },
//   provinces: { controller: Uint16-like array as plain array },
//   regions: { [id]: { weather: 'clear', weatherDays: 0 } },
//   economy: {}, research: {}, politics: {}, diplomacy: {}, land: {}, naval: {}, air: {},
//   intel: {}, log: [] }
// commands.js
export function validateCommand(state, cmd) → null | string
export function applyCommand(state, cmd) → void (throws if invalid)
// Stage 1 commands: {type:'set_speed', speed:0..5}, {type:'pause'}, {type:'resume'},
//                   {type:'select_country', tag} (sets state.player; validated tag exists)
// tick.js
export function tick(state, data) → void    // one hour
export function advance(state, data, hours) → void
// save.js
export const SAVE_VERSION = 1;
export function toSave(state) → { version, state }   (plain object)
export function fromSave(obj, data) → state           (runs migrations, validates version)
// modifiers.js
export function buildModifiers(state, data) → { [tag]: { [key]: number } }  (empty for now)
```
- [ ] Tests: newGame builds every country from `data.countries`, every state has an
  owner that exists; invalid commands return reasons; `tick` increments hour;
  determinism: two `newGame` with seed 'x' advanced 24*30 hours serialise identically,
  and seed 'y' differs in `regions` weather; `fromSave(toSave(s))` deep-equals `s`;
  a save with `version: 99` throws a message containing "newer".
- [ ] Commit `warroom: game state, commands, tick and save`.

### Task 4: Map pipeline — states and land provinces

**Files:** `pipeline/requirements.txt`, `pipeline/ne.py`, `pipeline/raster.py`,
`pipeline/states.py`, `pipeline/provinces.py`, `pipeline/build_map.py` (first version),
`pipeline/owners_1936.json`, `data/common/countries.json`

**Algorithm:**
1. `ne.py`: download (if not cached under `pipeline/cache/`, gitignored) the geojson files
   `ne_10m_admin_1_states_provinces`, `ne_10m_admin_0_countries`, `ne_10m_lakes`,
   `ne_10m_rivers_lake_centerlines`, `ne_10m_populated_places`,
   `ne_10m_geography_regions_polys`, `ne_10m_glaciated_areas` from
   `https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/`.
2. `raster.py`: `rasterize(features, width=8192, height=4096, key)` → `numpy.int32`
   array, equirectangular (x = (lon+180)/360*W, y = (90-lat)/180*H), drawn with
   `PIL.ImageDraw.Draw(Image.new('I', ...)).polygon(..., fill=id)` largest-area first so
   enclaves overwrite their hosts.
3. `states.py`: start from admin-1 regions; **merge** any region with fewer than 1,200
   land pixels (~2,900 km² at the equator) into its largest same-country neighbour
   (repeat until none remain, keeping islands as their own state if no neighbour);
   **split** any region with more than 60,000 pixels into k-means halves repeatedly;
   number states 1..N in a stable order (by adm0_a3 then name). Owner from
   `owners_1936.json`: `{"by_adm0": {"DEU":"GER", ...}, "by_adm1_code": {"POL-1234":"GER"},
   "cores": {"GER": ["DEU", ...], ...}}`; `by_adm1_code` wins over `by_adm0`.
4. `provinces.py`: per state, seeds = max(1, round(pixels / 2,000)) capped at 40,
   placed by Poisson disc on the state's pixels (seeded RNG, seed = state id); each land
   pixel in the state is assigned to the nearest seed (vectorised with numpy per state
   bounding box). Province ids 1..N land, then sea. Sea: a seed lattice of 4° cells
   (denser, 2°, within 10° of coasts), nearest-seed over sea pixels; lakes from the lakes
   layer rasterised at ≥ 50 pixels become lake provinces, smaller lakes are land.
   Each province gets its centroid pixel and lat/lon.
5. `build_map.py` writes `data/map/provinces.png`, `provinces.json`, `states.json` and
   prints counts. Target counts: 900–1,200 states, 10,000–14,000 land provinces,
   2,000–3,000 sea provinces.
- [ ] Test (`tests/map.test.js`, part 1): province ids are contiguous; every land
  province has a state; every state has ≥ 1 province; the PNG dimensions are 8192×4096
  (decode PNG in Node with a minimal inflate — use `zlib.inflateSync` and the PNG filter
  unrolling in `tests/png.js`); counts within targets.
- [ ] Commit `warroom: map pipeline builds states and provinces from Natural Earth`.

### Task 5: Map pipeline — terrain, sea regions, adjacency, victory points

**Files:** `pipeline/terrain.py`, `pipeline/adjacency.py`, `pipeline/regions.py`,
`pipeline/straits.json`, `pipeline/terrain_overrides.json`, `pipeline/vp_1936.json`,
outputs `data/map/adjacency.json`, `rivers.json`, `regions.json`, `climate.json`,
`data/common/terrain.json`; extend `tests/map.test.js`; add CI job `pipeline`

**Algorithm:**
- Terrain per land province from the mean/slope of the 10-arc-minute elevation grid
  (`earth-topography-10arcmin.nc` from fatiando-data, cached) sampled at the province's
  pixels: slope ≥ 60 m/10' → mountain; ≥ 25 → hills; else by Köppen class from kgcpy's
  raster (12960×6480): `Af/Am` → jungle, `BWh/BWk` → desert, `Dfc/Dfd/ET/EF` with
  wetlands → marsh, `Cf*/Df*` with forest-likelihood (latitude 45–65 or Köppen D) →
  forest, else plains; `Wetlands`/`Delta` regions from the geography layer → marsh;
  populated places with population ≥ 500,000 in 1936-era (use `POP_MAX` ≥ 1,500,000
  modern as proxy) make their province urban; `terrain_overrides.json` last.
- Adjacency: 4-neighbour pixel scan collects province pairs (land-land, land-sea,
  sea-sea); `straits.json` adds declared crossings (Dover, Gibraltar, Bosporus,
  Øresund, Messina, Tsushima, Kerch, …) as land-land links flagged `strait`. River
  crossings: a land-land edge is `river` if the rasterised river centreline (scale rank
  ≤ 6) passes through the shared border pixels.
- Regions: land strategic regions = states grouped by k-means into ~120 regions of
  ~100 land provinces, named after their largest state; naval regions = sea provinces
  grouped into ~80 regions named from a small ocean/sea name table by centroid. Climate
  per region = the modal Köppen group (tropical, arid, temperate, continental, polar).
- Victory points: `vp_1936.json` lists `{name, lat, lon, vp}` for capitals and
  historically significant cities; each is mapped to the province containing it and
  the province's `vp` set; `states.json` gets `capital_province`.
- [ ] Tests: adjacency symmetric; no land province without a land or sea neighbour;
  land is reachable from Berlin's province to Lisbon's, Moscow's, Cape Town's and
  Vladivostok's by land+strait edges; every state has a capital province with vp ≥ 1 for
  every country capital; terrain values are in the enum.
- [ ] CI job `pipeline`: `pip install -r warroom/pipeline/requirements.txt`,
  `python warroom/pipeline/build_map.py --out /tmp/map`, then
  `python warroom/pipeline/compare.py /tmp/map warroom/data/map` fails if any JSON
  differs or the PNG differs by more than 0.01% of pixels (allows library drift).
- [ ] Commit `warroom: terrain, regions, adjacency and victory points`.

### Task 6: Data loader, schemas, countries and scenario data

**Files:** `js/data/schema.js`, `js/data/load.js`, `data/common/countries.json`,
`data/common/weather.json`, `data/scenarios/1936.json`, `tests/data.test.js`

**Interfaces:**
```js
// schema.js
export function check(value, schema, path) → throws Error(`${path}: ${reason}`)
// schema is a nested object: {type:'object', props:{...}, required:[...]} | {type:'array', items}
//   | {type:'string'|'number'|'integer'|'boolean', enum?, min?, max?}
// load.js
export async function loadData(baseUrl, fetchImpl) → data
// data = { provinces, adjacency, rivers, states, regions, climate, countries, terrain,
//          weather, scenario, byTag: {tag → country}, provinceIdMap: Uint8Array? (no: the
//          view decodes the PNG; sim uses provinces.json only) }
// In Node, fetchImpl reads files; in the browser it is window.fetch.
```
- `countries.json`: one entry per 1936 country (≈ 75): tag (3 letters, our own set),
  name, adjective, colour (hex), capital_state (by state name lookup in pipeline output,
  resolved to id at load), ideology, leader name, a 2-colour flag description
  `{kind: 'tricolor'|'bicolor'|'cross'|'plain'|'canton', colors: [...]}` rendered as SVG by
  the view.
- `weather.json`: for each climate group, a daily transition table between weather
  states and the month-by-month bias (e.g. continental: snow likely Nov–Mar).
- `scenario 1936`: `{start_hour: 0, armies: {GER: [{state:'Brandenburg', divisions: 24}], ...}}`.
- [ ] Tests: every file loads; every state owner tag exists in countries; every
  country's capital resolves; every army state resolves; schema errors name the path.
- [ ] Commit `warroom: data loader, schemas, 1936 countries and scenario`.

### Task 7: Weather in the simulation

**Files:** `js/sim/weather.js`, `tests/weather.test.js`; wire into `tick.js`

```js
export function tickWeather(state, data, hour)  // runs when isNewDay(hour)
// for each region: roll nextFloat(state.rng.weather) against data.weather[climate][month][current]
// state.regions[id].weather ∈ {'clear','rain','storm','snow','blizzard','sandstorm','mud','frost'}
export function weatherAt(state, data, provinceId) → weather string
```
- [ ] Tests: after 365 days a continental region has seen snow and a tropical one has
  not; the distribution over 10 years for 'arid' has sandstorm between 3% and 15% of
  days; deterministic for a seed.
- [ ] Commit `warroom: daily weather per strategic region`.

### Task 8: Blender models

**Files:** `blender/build_models.py`, `blender/build_relief.py`, `blender/README.md`,
`models/*.glb`, `models/manifest.json`; CI job `models`

- `build_models.py`: `python build_models.py --out ../models` under bpy, or
  `blender -b -P build_models.py -- --out ../models`. Builds, per model name, with
  primitives, booleans and bevels: counters (`division`, `division_armoured`,
  `division_motorised`, `division_special`, `fleet`, `air_wing`), pieces (`infantry`,
  `tank_light`, `tank_medium`, `tank_heavy`, `truck`, `artillery`, `fighter`, `bomber`,
  `destroyer`, `cruiser`, `battleship`, `carrier`, `submarine`, `convoy`), markers
  (`factory`, `dockyard`, `airbase`, `naval_base`, `fort`, `radar`, `supply_hub`,
  `capital`, `victory_point`, `battle`, `nuke`). Each is 1 unit tall, flat-shaded, one
  material per colour slot named `Body`, `Accent`, `Base` (the view tints `Body` with the
  country colour). Exports `.glb` and writes `manifest.json`:
  `{ "<name>": { "file": "<name>.glb", "height": 1.0, "pivot": "base" } }`.
- `build_relief.py`: reads the elevation grid, writes `data/map/relief.png` (4096×2048,
  normal map from the gradient, sea flat).
- [ ] Test (Node, `tests/models.test.js`): every manifest entry's file exists, is a valid
  GLB (magic `glTF`, version 2, length matches), and under 200 KB.
- [ ] CI job `models`: `pip install bpy==5.0.*` on Python 3.11, build into `/tmp/models`,
  fail if any manifest name is missing.
- [ ] Commit `warroom: Blender-built counters, pieces and markers`.

### Task 9: Globe view

**Files:** `js/view/globe.js`, `js/view/camera.js`, `js/view/mapmodes.js`,
`js/view/input.js`, `js/main.js` (real boot)

- Decode `provinces.png` to an `ImageBitmap`; upload as `DataTexture`-equivalent
  (`THREE.Texture` with `NearestFilter`, no mipmaps, `flipY=false`); also draw it to an
  offscreen canvas once to get the `Uint8ClampedArray` for CPU picking.
- Lookup textures: `colorLUT` (128×128 RGBA, `NearestFilter`) and `flagLUT`; functions
  `setProvinceColor(id, rgb)`, `setProvinceFlags(id, bits)`, `commit()` (one
  `needsUpdate` per frame).
- Shader (GLSL ES 3): sample id at uv and at ±1 texel in x/y; decode id; if any neighbour
  id differs → border (state border when `stateOf[id]` differs — a third LUT holds the
  state id per province); colour = LUT[id] mixed with relief normal lighting; night side
  from the sun direction computed from `state.hour`; selected/hovered from flagLUT.
- Picking: `pick(ndcX, ndcY)` raycasts the sphere → `(lat, lon)` → pixel → id.
- `mapmodes.js`: `paint(mode, state, data, globe)` for `political`, `terrain`, `states`,
  `regions`, `weather`, `vp`; political uses `countries[owner].color`; terrain a fixed
  palette; states a hashed hue per state; weather a palette per weather type.
- `camera.js`: OrbitControls with `enablePan=false`, `minDistance=1.05`,
  `maxDistance=4`, damping, zoom-speed by distance; `zoomLevel()` ∈ {globe, continent,
  state, province}.
- [ ] Manual check in Chromium via Playwright screenshot (Task 12 automates it).
- [ ] Commit `warroom: three.js globe with province-id shader, picking and map modes`.

### Task 10: Panels, top bar, search, clock controls

**Files:** `js/view/panels/topbar.js`, `province.js`, `country.js`, `search.js`,
`css/warroom.css`

- Top bar: flag SVG + country name of `state.player` (or "Observer"), date, speed
  buttons 0–5 (keyboard: space pause, +/- speed), map mode buttons.
- Province panel: name, terrain, state (link), owner/controller flag+name, victory
  points, weather now, neighbours (clickable), region.
- Country panel: flag, name, leader, ideology, capital (link), number of states and
  provinces, list of states with cores, "Play as" button → `select_country` command.
- Search: name match over provinces, states and countries; Enter flies the camera.
- The render loop: `requestAnimationFrame` → up to 16 ms of `tick()` calls per frame
  according to `SPEED_HOURS_PER_SECOND[state.speed]` with an accumulator → repaint
  changed provinces only (`state.dirtyProvinces`, cleared after paint) → draw.
- [ ] Commit `warroom: top bar, province and country panels, search`.

### Task 11: Overlays from Blender counters; save/load UI

**Files:** `js/view/overlays.js`, `js/view/panels/saves.js`, `js/sim/save.js` (gzip
helpers live in view, sim stays pure)

- Load `models/manifest.json` and the counters with GLTFLoader; one `InstancedMesh` per
  model; place counters at each scenario army's state capital province on the sphere
  surface (`latLonToVector3`), tinted by owner, with a sprite digit of the division count;
  scaled with camera distance so they stay readable.
- Saves panel: Save (name), Load (list from `localStorage` keys `warroom:save:<name>`),
  Export (download `<name>.warroom`, gzip via `CompressionStream`), Import (file input).
  Autosave each in-game month to `warroom:autosave` and on `visibilitychange`.
- [ ] Commit `warroom: Blender counters on the globe; save, load, export, import`.

### Task 12: Playwright smoke test, CI wiring, README, final checks

**Files:** `tests/serve.mjs`, `tests/smoke.playwright.mjs`, `warroom/README.md`,
`.github/workflows/warroom.yml`

- `serve.mjs`: static server on an ephemeral port serving the repo root.
- Smoke: launch Chromium (`executablePath` from `PLAYWRIGHT_BROWSERS_PATH` when set),
  open `/warroom/`, wait for `#boot` to be hidden, assert no console errors, click the
  globe centre, assert `#panel` contains a province name, switch to terrain map mode,
  press play at speed 5 for 2 s and assert the date advanced, save and reload, screenshot
  to `warroom/tests/out/smoke.png` (gitignored).
- README in the style of `earth/README.md`: what it is, open it, what it does (table),
  how it is built (pipeline, Blender), tests, licence of data.
- [ ] `node scripts/check-links.mjs && node scripts/check-exchange.mjs`, all Node tests,
  smoke test green; push; CI green.
- [ ] Commit `warroom: smoke test, README and CI`.

## Self-review

- Spec coverage: §2.1 map (Tasks 4, 5, 9), weather (7), clock (2, 10), map modes (9),
  selection (10), save/load (3, 11); §4.1–4.9 architecture (1, 3, 6, 9, 12); §4.5
  Blender (8); §4.6 pipeline (4, 5); Stage 1 deliverable counters (11); phone (1, 10
  CSS); CI (1, 5, 8, 12). No gaps.
- Placeholders: none; every task names its files, interfaces and tests.
- Type consistency: `applyCommand`/`validateCommand` names match in Tasks 3 and 10;
  `tickWeather(state, data, hour)` in 7 matches the `tick(state, data)` signature in 3;
  LUT names `colorLUT`/`flagLUT` only in Task 9.

---

## Addendum (23 October, later): the game moved to Unreal Engine

Cody decided the game must be an Unreal game. The plan above was executed as the browser
preview; the same Stage 1 was then rebuilt as an Unreal Engine 5.5 project in
`warroom/unreal/` with these correspondences:

| Browser build | Unreal project |
| --- | --- |
| `js/sim/*` (JavaScript simulation) | `Source/WarRoomSim/` (C++20, no engine dependency; `Tests/sim_tests.cpp` runs with g++ and in CI) |
| `js/view/globe.js` (shader, LUTs, picking) | `GlobeActor` + `Shaders/Globe.ush` + `M_Globe` built by `Scripts/setup_content.py` |
| `js/view/camera.js` | `OrbitCameraPawn` |
| `js/view/input.js`, keyboard | `WarRoomPlayerController` + `Config/DefaultInput.ini` |
| `js/view/panels/*`, search, saves | `SWarRoomHUD` (Slate) + `WarRoomSubsystem` saves in `Saved/SaveGames` |
| `js/main.js` loop | `WarRoomSubsystem` (ticks by speed) + `WarRoomGameMode` (spawns globe, camera, HUD) |
| `models/*.glb` via GLTFLoader | imported once by the setup script as static meshes; `M_Counter` tints the Body slot |
| Playwright smoke test | Unreal automation tests `WarRoom.Sim.*` (editor); the Unreal build itself runs on Cody's PC |

Data (`warroom/data`) and models (`warroom/models`) are shared unchanged. What could not
be verified in the cloud box: compiling the Unreal module and running the editor. The
first build on a PC with Unreal 5.5 is the remaining step of Stage 1.
