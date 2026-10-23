# War Room 1936 — design

*A grand-strategy game of the Second World War in the style of Hearts of Iron IV, as a
standalone browser game, with its 3D pieces made in Blender.*

Date: 23 October 2026. Status: Stage 1 built on branch `claude/warroom-stage1`; the design as a whole awaits Cody's review.

## 1. What this is

**War Room 1936** (folder `warroom/`) is a clone of Hearts of Iron IV in scope: the same
genre, the same shape of game, the same systems. You pick any country of the world in
January 1936, run its politics, economy, research and armed forces hour by hour, and fight
the Second World War as it happened or as you make it happen.

It is built the way every recent project in this repo is built:

- A **standalone browser app** served from the GitHub Pages site
  (`https://mazoo85.github.io/Maz/warroom/`). Opens on a phone or a computer. No account,
  no install. It is *not* part of MAZ ARCADE (no hub link, no `shared/` scripts, its own
  CI workflow), and it is *not* built on the Maz engine, per the rule at the top of
  `CLAUDE.md`.
- **three.js** draws the world as a globe. It is vendored into the repo so the game has no
  network dependency and can be run and screenshotted headlessly in CI and in this box.
- **Blender** makes every 3D piece: the division, fleet and air-wing counters, ship, tank
  and plane models, building markers, and the relief of the globe. They are built by a
  Blender Python script, not by hand, so the models are reproducible and live in git as
  the script plus the exported `.glb` files.
- The whole simulation is **plain JavaScript with no browser dependency**, so it runs under
  Node for tests and for the AI to play thousands of games unattended.

### Names and ownership

Hearts of Iron is Paradox Interactive's trademark and their files are copyrighted. This
game copies the *genre and mechanics*, which are not protectable, and nothing else:

- Its own name, art, music, icons, text and data files. Nothing is extracted from HoI4.
- Historical facts (which countries existed, who led them, how many factories and divisions
  they had, which technologies they had) are facts, drawn from encyclopaedic sources.
- Flags are redrawn as simple SVGs from their public-domain descriptions.
- Country-specific "focus" content is written fresh with its own names and structure.

### Decisions already made

| Decision | Choice | Why |
| --- | --- | --- |
| Engine | Browser, three.js, vendored | Cody's choice. Runs on phone from Pages; fully testable here. |
| Players | Single-player first; built for multiplayer later | Everything the player does is a *command* applied to a deterministic simulation, which is exactly what lockstep multiplayer needs. Stage 9 adds the lobby and relay. |
| Map | The real world, 1936, as a globe | It is a clone. Province shapes come from Natural Earth (public domain). |
| Start dates | 1 January 1936 and 1 September 1939 | As HoI4. 1939 is a later stage because it needs a second full set of country data. |
| Language | English only | Localisation keys from day one so more can follow. |
| Scripting | JSON data files plus a small effect/trigger language in JSON | Mods and focus trees are data, not code, as in HoI4; but no custom parser to maintain. |

## 2. Everything in Hearts of Iron, and which stage builds it

Every system of Hearts of Iron IV (base game plus the systems its major expansions added
that players consider part of the game today) is listed here, each assigned to a stage.
**Each stage is a playable game on its own** and is designed, planned and built as its
own project, in order. Nothing is left out; the stage number says *when*, not *whether*.

### 2.1 Map and world — Stage 1

- Globe with **land, sea and lake provinces** (~12,000 land, ~2,500 sea), grouped into
  **states** (~1,000, cut from real-world first-level administrative regions) and
  **strategic regions** (air regions over land, naval regions over sea).
- Per province: terrain (plains, forest, hills, mountain, urban, jungle, marsh, desert,
  ocean, lake), coastal flag, river crossings on edges, victory points, adjacency, and a
  stable integer id. Per state: owner, controller, core owners, category (which sets
  building slots), infrastructure, buildings, resources, manpower, local supplies.
- **Weather and climate**: climate zones from a Köppen grid; daily weather per strategic
  region (clear, rain, storm, snow, blizzard, sandstorm, mud, frost) with combat, air and
  movement effects. Day and night.
- **Clock**: the game advances one hour per tick. Speeds 1 to 5 and pause. Dates run from
  1 January 1936 to 31 December 1948 (the game continues past that if the player wants).
- **Map modes**: political, terrain, states, supply, resistance, naval, air, strategic,
  diplomacy, victory points, weather, construction, resources, manpower, infrastructure.
- **Selection and inspection**: click a province to see its state and owner; click a
  country to see its summary. Search by place name.
- **Save and load** to the browser's storage and to a downloadable file.

### 2.2 Buildings — Stage 2

Infrastructure, civilian factory, military factory, naval dockyard, air base, naval base,
land fort, coastal fort, anti-air, radar station, synthetic refinery, fuel silo, rocket site,
nuclear reactor, supply hub, railway (province-edge building), and later the special-project
facilities of Stage 7. Each has a cost, a state slot rule, a damage model, and repair.

### 2.3 Economy and production — Stage 2

- **Factories** split between consumer goods (set by economy law and modifiers), trade,
  construction and production lines.
- **Construction queue** with priority ordering, per-state costs, infrastructure bonus.
- **Production lines** per equipment type: assigned factories, production efficiency with
  growth and retention, output to the stockpile, line priority.
- **Equipment**: infantry equipment, support equipment, artillery, anti-tank, anti-air,
  motorized, mechanized, armoured cars, light/medium/heavy/super-heavy/modern tanks and
  their tank-destroyer, SPG and SPAA variants, amphibious tanks, trucks, trains, railway
  guns, fighters, heavy fighters, CAS, naval bombers, tactical bombers, strategic bombers,
  transport planes, scout planes, jet variants, rockets, nuclear bombs, destroyers, light
  cruisers, heavy cruisers, battlecruisers, battleships, carriers, submarines, convoys.
  Each with stats, cost, resources and a tech line of successive models.
- **Resources**: oil, aluminium, rubber, tungsten, steel, chromium; from states (with
  excavation tech), synthetic refineries, and trade. **Fuel** produced from oil, stored in
  silos, consumed by motorised and armoured units, ships and planes.
- **Trade**: offer/import by resource through other countries, paid in civilian factories,
  carried by convoys; trade law sets how much is exported.
- **Stockpile** of equipment; **lend-lease**; **licensed production** of another country's
  designs; **equipment conversion** (upgrading old equipment on a line).
- **Variants and designers** (Stage 7): tank, ship and plane designers with modules;
  refits; **military industrial organisations** that improve designs over time.

### 2.4 Research — Stage 2

Technology trees with dates, prerequisites, ahead-of-time penalty, research slots and
bonuses: infantry, support companies, armour, artillery, land doctrines (mobile warfare,
superior firepower, grand battle plan, mass assault), air technology, air doctrines, naval
technology, naval doctrines, engineering (radar, encryption/decryption, computing,
rocketry, nuclear), industry (construction, production, excavation, synthetic fuel,
concentrated/dispersed industry), and the special projects of Stage 7.

### 2.5 Politics — Stage 3

- **Political power** income and spending. **Stability** and **war support** with their
  effects and their events.
- **Ideologies**: democratic, fascist, communist, non-aligned; party popularity, drift,
  elections, referendums, civil wars, coups, ideology change.
- **Laws**: conscription, economy, trade; **advisors**: political, theorists, chiefs of
  army/navy/air, high command, military industrial design companies.
- **National spirits** (ideas) with modifiers and durations.
- **National focus trees**: a generic tree for every country (Stage 3) and country-specific
  trees for the majors and key minors (Stage 8), with prerequisites, mutual exclusion,
  branches, and historical AI paths.
- **Decisions and missions** with costs, timers and categories; **events** with triggers,
  options and delays; **news events**.
- **Leaders and rulers**: country leaders per ideology with traits; **generals**,
  **field marshals**, **admirals** with skill levels, traits, experience and promotion.
- **Subjects**: puppet, integrated puppet, dominion, collaboration government, with autonomy
  levels and the actions that change them; releasing nations; formable nations; cosmetic
  tags; government in exile.
- **Capitulation** by victory points, and the **surrender** of factions.

### 2.6 Diplomacy — Stage 5

Opinions and relation modifiers; improve relations; guarantee independence; non-aggression
pacts; military access; docking rights; **factions** (create, invite, join, leave, leader);
**justify war goals** against a state or a country with time, cost and **world tension**;
declare war; call allies; join wars; **threat** and tension thresholds that gate what
democracies may do; lend-lease; volunteers; expeditionary forces; attachés; boost party
popularity; stage coups; embargoes; **peace conferences** with war score, bidding for
states, puppeting, liberating, demilitarising and reparations; white peace; conditional
surrender.

### 2.7 Land warfare — Stage 4

- **Divisions** built from **templates**: battalions of each type in combat width columns,
  support companies, template editing costs army experience; equipment and manpower
  requirements; priority; special forces cap.
- **Recruit and deploy**: training queue, training levels, deployment location.
- **Units** on the map: organisation, strength, equipment, experience, entrenchment, supply,
  attrition, movement by terrain, rivers, infrastructure and weather.
- **Commanders**: generals command armies of up to 24 divisions (more with traits), field
  marshals command army groups; skill in attack, defence, planning and logistics; traits
  earned by what they do; promotion.
- **Battle planner**: front lines, offensive lines with arrows, fallback lines, garrison
  areas, naval invasion orders, paradrop orders; planning bonus; execute/halt; manual
  orders for individual units; strategic redeployment; exercises.
- **Combat**: combat width by terrain, soft and hard attack against defence or
  breakthrough, armour against piercing, organisation and strength damage, tactics chosen
  by commander skill, terrain and river penalties, air superiority and close air support,
  encirclement, retreat, reinforcement and overrun; **naval invasions** and
  **paradrops**; **nuclear strikes** (Stage 7); **railway guns** and **special forces**
  (marines, mountaineers, paratroopers); **cavalry** and **garrison** units.
- **Supply**: supply hubs, railways (levels), ports, trucks and horses, motorisation,
  supply range and hub capacity, air supply, attrition when out of supply, naval supply
  over sea, the supply map mode. (Stage 4 brings the full system; Stage 1 has only the
  map data for it.)
- **Occupation**: occupation laws, resistance and compliance per state, garrison demands,
  sabotage, collaboration governments (Stage 7), partisan uprisings.

### 2.8 Naval warfare — Stage 6

Fleets, task forces, ships with stats (attack by gun type, torpedo, anti-air, armour,
HP, speed, range, visibility, detection); missions (patrol, strike force, convoy escort,
convoy raid, naval invasion support, minelaying, minesweeping, hold); naval combat with
screening, positioning, detection and retreat; naval supremacy per region; convoys and
convoy raiding; naval bases, repair, fuel; admirals; refits; the ship designer (Stage 7);
mines; carrier air wings; shore bombardment; naval superiority for invasions.

### 2.9 Air warfare — Stage 6

Air wings on air bases and carriers; air regions; missions (air superiority, interception,
close air support, strategic bombing, naval strike, port strike, logistics strike,
paradrop, air supply, reconnaissance, naval mining, naval patrol); air combat and ground
attack; air superiority effects on land combat and movement; anti-air and radar; aces;
pilot experience; the plane designer (Stage 7); nuclear and rocket delivery (Stage 7).

### 2.10 Intelligence and special projects — Stage 7

Intelligence agency with upgrades; operatives; operations (steal blueprints, build
intelligence networks, sabotage, coup support, propaganda, infiltration of army/navy/air/
civilian); decryption and codebreaking with its combat bonus; counter-intelligence;
collaboration governments. Special projects: nuclear bombs, rockets and jets, advanced
designs; the tank/ship/plane designers; military industrial organisations; raids.

### 2.11 Artificial intelligence — Stage 5 (core), refined in every later stage

Every country not played by a person is run by the AI: economy (what to build and
produce), research order, politics (historical or random focus paths, laws, advisors),
diplomacy (factions, guarantees, war goals, peace demands), strategic war plans per
front, naval and air deployment, difficulty settings and AI personalities. The AI uses the
same command interface as the player, so it can play a whole game headless under Node.

### 2.12 The player's interface — every stage

The top bar (political power, stability, war support, factories, manpower, convoys, fuel,
date and speed); the country panel with tabs for politics, research, diplomacy, trade,
construction, production, recruit and deploy, logistics, army, navy, air, decisions and
focus; the outliner; the ledger; alerts; tooltips on everything; province, state, unit,
fleet and wing panels; the battle planner; the peace conference; the new-game screen
(country, difficulty, rules, historical AI); save/load; settings; hot keys; phone layout.

### 2.13 Content — Stage 8

Country-specific focus trees, events, decisions, leaders and portraits for Germany, the
United Kingdom, France, Italy, the Soviet Union, Japan, the United States, and the key
minors (Poland, Spain, China, Nationalist and Communist, Yugoslavia, Romania, Hungary,
Czechoslovakia, Finland, the Netherlands, Belgium, Sweden, Turkey, Greece, Canada,
Australia, India, South Africa, Mexico, Brazil, Argentina), the Spanish and Chinese civil
wars, the historical event chain of 1936-45, the 1939 start, formable nations, and music.

### 2.14 Multiplayer and mods — Stage 9

Lobby, invites, hot-join, lockstep simulation over a relay (Cody's Cloudflare relay from
`godview/` is the model), desync detection, host migration, and a mod loader that
overlays data files.

## 3. Approaches considered

**A. Flat map, 2D canvas, no 3D.** Fastest to build and lightest on a phone, and most
grand-strategy games are flat. But it leaves Blender with nothing meaningful to do, and
Cody asked for Blender.

**B. Globe in three.js, province identity carried by a texture, Blender for the pieces
and relief (chosen).** The world is one sphere. A single "province id" texture tells the
shader which province every pixel belongs to, and a small lookup texture gives each
province its current colour, so changing map mode or ownership costs nothing. Borders,
selection, fog and highlights are drawn in the shader. This is how Paradox's own map
works. The 3D counters, models and relief come from Blender as glTF. Scales to 15,000
provinces on a phone; the simulation never touches three.js.

**C. Full 3D terrain with every province as its own mesh.** Prettiest, but 15,000
meshes and their borders are too heavy for a phone, and building and editing them is a
full-time job. Not worth it for a strategy map viewed from orbit.

B is the recommendation and the design below assumes it.

## 4. Architecture

```
warroom/
  index.html            the game page (one page; panels are DOM, the map is WebGL)
  css/                  phone-first layout, light and dark
  js/
    main.js             boots: loads data, builds the sim, builds the view, runs the loop
    sim/                THE SIMULATION — pure JS, no DOM, no three.js, deterministic
      state.js          the whole game state as one plain object (serialisable)
      clock.js          hours, days, months; the tick scheduler
      rng.js            seeded random numbers (xoshiro); one stream per subsystem
      commands.js       every player/AI action as {type, ...} and its validator/applier
      map.js            provinces, states, regions, adjacency, path-finding
      weather.js
      economy.js        factories, construction, production, resources, trade, fuel
      research.js
      politics.js       power, stability, war support, laws, ideas, focus, decisions, events
      diplomacy.js      opinion, factions, war goals, tension, peace conferences
      land.js           templates, divisions, commanders, orders, movement, combat, supply
      naval.js
      air.js
      intel.js
      ai/               one file per concern: economy, research, politics, diplomacy, war
      effects.js        the JSON effect/trigger mini-language (modifiers, conditions)
      save.js           serialise and restore
    view/               THE VIEW — three.js globe, DOM panels, input
      globe.js          sphere, province-id texture, lookup texture, shader, picking
      camera.js         orbit, zoom levels, phone gestures
      overlays.js       counters, arrows, fronts, battles, weather, from Blender glTF
      panels/           one file per panel (topbar, country, province, unit, ...)
      mapmodes.js
      input.js
    data/               loader + schema checks for the JSON data
  data/                 GAME DATA (JSON, generated or authored; never edited by code)
    map/                provinces.png (id map), provinces.json, states.json, regions.json,
                        adjacency.json, terrain.json, climate.json
    countries/          one file per country: 1936 state (and 1939 in Stage 8)
    common/             techs, equipment, buildings, laws, ideologies, templates, traits,
                        focus trees, decisions, events, localisation
  models/               Blender outputs: *.glb committed, plus a manifest
  blender/
    build_models.py     bpy script: builds every model procedurally and exports glTF
    build_relief.py     bakes the globe's relief/normal map
  pipeline/             Python: Natural Earth + climate + elevation -> data/map/*
  vendor/               three.module.js, GLTFLoader.js, OrbitControls.js (pinned versions)
  tests/                plain Node tests, no browser; plus one Playwright smoke test
  README.md
```

**Rules that keep it sane:**

1. `sim/` never imports from `view/`, never touches `document`, `window` or three.js.
   `node -e "import('./warroom/js/sim/state.js')"` must work. Every test of game rules is
   a sim test.
2. All state lives in one plain object; no classes with hidden fields, no closures holding
   state. `JSON.parse(JSON.stringify(state))` is a valid save.
3. Every change to state goes through a **command** (`commands.js`): validate, then apply.
   The view only *sends* commands. The AI only *sends* commands. Multiplayer will only
   *relay* commands. Replays are the command log plus the seed.
4. The tick is deterministic: same state, same commands, same seed, same result, on every
   machine. No `Math.random`, no `Date.now`, no iteration over object keys whose order
   can differ (use arrays or sorted keys).
5. Data files are validated against a schema at load, with one clear error naming the file
   and field. A broken data file never produces a mysterious crash later.
6. Each `sim/*.js` file owns one subsystem and exposes `tick<Name>(state, hour)` plus the
   commands it handles. Files that pass ~800 lines are split.

### 4.1 Data flow

```
Natural Earth, climate, elevation --pipeline/--> data/map/*      (offline, committed)
blender/build_models.py            --bpy-------> models/*.glb    (offline, committed)
hand-authored JSON                 ------------> data/countries, data/common

browser:  main.js --loads--> data/*  --> sim.newGame(scenario, seed) --> state
          view renders state; input -> command -> sim.apply(state, cmd)
          requestAnimationFrame: while (speed > 0 && time budget) sim.tick(state)
          view diff: changed provinces -> lookup texture; changed units -> overlays
          autosave: every in-game month -> localStorage; export -> JSON download
```

### 4.2 The simulation tick

One tick is one hour. Within an hour, subsystems run in a fixed order so results are
reproducible: clock → weather (daily) → supply → movement → combat → air → naval → economy
(daily) → research (daily) → politics (daily) → diplomacy (daily) → AI (daily, countries
staggered across the day so no single hour stalls) → events. Each subsystem reads the
state and writes to it; none of them call each other. Cross-subsystem effects go through
**modifiers**: a flat table built once per day from ideas, laws, techs, advisors, weather and
terrain, read everywhere (`mod(state, country, 'research_speed')`).

### 4.3 Commands

A command is `{type, country, ...fields}`. `commands.validate(state, cmd)` returns `null` or
a reason string the UI shows. `commands.apply(state, cmd)` mutates state. The full list
grows stage by stage; each stage's plan enumerates its commands with their validators.
Stage 1 has: set speed, pause, select (view-only, not a command), save, load, new game.

### 4.4 The globe (view)

- A sphere with an equirectangular **province-id texture** (8192×4096 RGBA; id in RGB,
  flags in A) produced by the pipeline. 8192×4096 is ~2 px per 10 km at the equator, the
  same order as HoI4's 5632×2048 flat map, and within every phone's texture limits.
- A **lookup texture** of 128×128 RGBA (16,384 slots) mapping id → colour for the current
  map mode, plus a second one for per-province flags (selected, hovered, fog, battle).
  Map mode changes rewrite this texture; nothing else.
- The fragment shader samples the id, looks up colour, compares neighbour ids to draw
  province and state borders (thin and thick), blends a relief/normal map baked in
  Blender, and darkens the night side.
- **Picking**: raycast to the sphere → lat/lon → read the id from a CPU copy of the id map.
- **Overlays**: instanced glTF counters from Blender for divisions, fleets and wings;
  front lines and arrows as line geometry on the sphere; weather as sprites per region.
- **Camera**: orbit controls with zoom levels that switch detail (whole globe → continent →
  state → province) and one-finger drag, pinch and two-finger tilt on phones.

### 4.5 Blender

`blender/build_models.py` runs under `bpy` (Blender as a Python module, installed with
`pip install bpy`, pinned to the 5.0 series; in this box it installs into a Python 3.11 virtual environment and exports glTF in under a second per model) or under
a full Blender (`blender -b -P build_models.py`). It builds each model from primitives and
modifiers in code, names it, applies a simple palette material, and exports one `.glb` per
model into `models/`, plus `models/manifest.json` listing names, bounds and pivot. Models
(all low-poly, flat shaded, read from orbit):

- counters: `division`, `division_armoured`, `division_motorised`, `division_special`,
  `fleet`, `air_wing`, each a plinth with a raised symbol;
- pieces: `infantry`, `tank_light`, `tank_medium`, `tank_heavy`, `truck`, `artillery`,
  `fighter`, `bomber`, `destroyer`, `cruiser`, `battleship`, `carrier`, `submarine`,
  `convoy`;
- markers: `factory`, `dockyard`, `airbase`, `naval_base`, `fort`, `radar`, `supply_hub`,
  `capital`, `victory_point`, `battle`, `nuke`;
- `build_relief.py` turns the elevation grid into the globe's relief normal map.

CI installs `bpy`, runs both scripts into a temporary folder, and fails if any model listed
in the manifest is missing or fails to export, so the committed models can always be rebuilt.

### 4.6 The map pipeline

`pipeline/build_map.py` (Python 3, numpy; no other dependencies) does, offline:

1. Rasterise Natural Earth 10m first-level admin regions (4,596 polygons) and country
   polygons to the 8192×4096 grid; merge tiny regions into neighbours and split huge ones
   so each **state** is a sensible size (target ~1,000 states).
2. Seed each state with points in proportion to its area (Poisson disc), assign every
   land pixel to its nearest seed within its state → **land provinces** (~12,000).
3. Seed the ocean on a coarser lattice → **sea provinces** (~2,500), grouped into **naval
   regions**; lakes from the Natural Earth lakes layer.
4. Terrain per province from the elevation grid (slope → hills/mountain) and the Köppen
   climate grid (jungle, desert, marsh), urban from populated-places density, with a
   hand-override table (`pipeline/terrain_overrides.json`) for anything the rules get wrong.
5. Adjacency from pixel neighbours; river crossings from the rivers layer; coastal flags;
   straits as declared crossings.
6. Victory points and capitals from the populated-places layer (scaled population) with a
   hand table for the historical ones.
7. 1936 ownership from a hand table mapping each state to its 1936 owner and cores (the
   admin regions are modern, so the table is where Danzig, Sudetenland, Manchukuo, the
   colonial empires and so on are set right).
8. Writes `data/map/*` and the id texture. Deterministic: same inputs, same bytes.

The pipeline runs here and in CI; its outputs are committed so the game never needs it at
run time.

### 4.7 Save and load

A save is `{version, seed, scenario, state}` as JSON, gzip-compressed with the browser's
`CompressionStream`. Autosave to `localStorage` monthly and on page hide; manual save to a
downloadable `.warroom` file; load from either. Saves carry a version and a migration
table so a save from an earlier stage still loads.

### 4.8 Errors

- Data load errors: the game stops before the title screen with the file, field and
  reason in plain words.
- Sim errors: a thrown error in a tick pauses the game, keeps the last good state, logs the
  command log and tick number, and shows "Something went wrong at <date>; the game is
  paused and your last save is safe". Nothing is silently caught.
- Command errors: validation reasons shown as the tooltip on the disabled button, never a
  dialog.
- WebGL unavailable: a message that the device cannot draw the map and a link to the
  README's device notes.

### 4.9 Testing

- `warroom/tests/*.test.js`: plain Node, no dependencies, the same style as
  `earth/tests/`. Every sim module gets one; every command gets a validator test and an
  apply test; the tick is tested for determinism (two runs, same seed, same JSON).
- Data tests: every data file loads against its schema; every reference (tech id,
  equipment id, state id) resolves; the map has no province with zero neighbours and no
  unreachable land.
- A full-game test: the AI plays every country headless for a year of game time under
  Node, and the test fails on any thrown error or any country in an impossible state.
- One Playwright smoke test: load the page in headless Chromium, wait for the globe,
  click a known province, assert the panel shows its name, screenshot. Runs here and in
  CI.
- `node scripts/check-links.mjs && node scripts/check-exchange.mjs` before every push.
- CI workflow `.github/workflows/warroom.yml`: Node tests, pipeline rebuild check,
  Blender model rebuild check, Playwright smoke test.

## 5. Stage 1 in detail — the map and the clock

What is built first, and what "done" means for it.

**Deliverable:** open `warroom/` and see the Earth of 1 January 1936 as a globe, every
country in its colour, every province and state in place. Spin, zoom and tilt. Switch map
modes. Click a province: see its name, terrain, state, owner, victory points and
neighbours. Click a country name: see its capital, number of states, and core states.
Press play: the clock runs at five speeds, the night side moves, the weather changes
daily on the weather map mode. Save, reload the page, load, and it is the same hour.
Works on a phone. Division counters from Blender are placed on every historical 1936
army's home province (they do nothing yet) to prove the model pipeline end to end.

**Includes:** sections 2.1 and 4.1–4.9, the data loader and schema checks, the country
colour table and names for every 1936 country, the provinces/states/regions pipeline,
the Blender model pipeline with all counter and marker models, the camera, the top bar,
the province and country panels, map modes, save/load, tests, CI and README.

**Excludes:** every other system; they are Stages 2–9. The state object has reserved,
empty sections for them so saves need no restructuring later.

**Done when:** the Playwright test passes, the Node tests pass, the pipeline and Blender
builds reproduce the committed data, CI is green, and it runs on Cody's phone from the
Pages site.

## 6. What follows

After Cody reviews this document, the writing-plans skill turns Stage 1 into a step-by-step
implementation plan, and Stage 1 is built. Each later stage starts from a design pass of
its own (this document is the master list; each stage's spec refines its section into
rules, numbers and interface) and ends with a playable game.
