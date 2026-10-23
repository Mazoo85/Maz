# Velocity — an Asphalt-style arcade racer in the browser

**Date:** 2026-10-23
**Status:** approved design, awaiting implementation plan

## What it is

A fast arcade racing game in the spirit of Asphalt Legends: one city circuit,
seven AI opponents, three laps, drifting, nitro and jumps. It runs in any
up-to-date browser with no build step, is designed for a phone first, and
also plays on a computer with the keyboard.

It is a **standalone** project in `velocity/`, like `mandala/` and `earth/`.
It is not part of MAZ ARCADE: no hub link, no entry in `shared/projects.js`,
no `shared/maz-nav.js`, and its own CI workflow. It is not built on the Maz
C++ engine.

Live address once merged: `https://mazoo85.github.io/Maz/velocity/`. On a
phone, *Add to Home Screen* opens it like an app.

## The game

### The race

- **Track:** "Harbor Loop", one city circuit of roughly 3 km. Wide streets,
  a tunnel lit by neon signs, a long sweep along the harbor front, one jump
  ramp, and several chicanes. The track is a closed loop with no shortcuts.
- **Format:** 3 laps against 7 AI cars. Grid start with a 3-2-1 countdown.
- **Positions:** live race position, lap counter, lap time and total time.
- **Finish:** crossing the line on lap 3 ends the player's race. Results show
  the finishing place, total time, best lap, and whether it beat the saved
  best time.

### Driving

- The car **accelerates on its own** (Asphalt-style). Holding the right side
  of the screen or the up key gives full throttle; releasing it coasts.
- **Steering:** tilt the phone (device orientation) or hold the left/right
  on-screen arrows. On a computer: arrow keys or A/D. The player can switch
  between tilt and arrows in the pause menu; arrows are the default so the
  game works without a permission prompt.
- **Drift:** hold the brake/drift button (or Shift) while steering to slide
  through a corner. Drifting keeps more speed than braking and fills the
  nitro bar.
- **Nitro:** tap NITRO (or Space) to burn nitro for a burst of speed with
  speed lines and camera shake. The bar fills from drifting, near-misses
  with other cars, and air time off the ramp.
- **Jump:** the ramp launches the car; it is airborne for a moment, cannot
  steer, and lands with a bounce.
- **Collisions:** scraping a wall slows the car and throws sparks. A hard
  head-on hit into a wall, or being rammed hard, "wrecks" the car: a short
  slow-motion crash, then it respawns on the track facing forward at a low
  speed. AI cars can also be wrecked by the player.

### Cars

Four cars to choose from before the race, each a different balance of top
speed, acceleration, handling and nitro strength. Names and looks are
stylised, not real brands:

| Car | Character |
|---|---|
| Comet | Balanced all-rounder (the default). |
| Brawler | Highest top speed, slow to turn, strong nitro. |
| Dart | Quickest acceleration and sharpest handling, lowest top speed. |
| Drifter | Longest, most controllable drifts; nitro fills fastest. |

No upgrades, unlocks or currency in this version.

### AI opponents

Seven opponents, each driving a racing line round the track with its own
skill (how closely it follows the ideal line and how late it brakes) and
aggression (how willing it is to block or bump). Mild rubber-banding keeps
the pack close: opponents far ahead of the player ease off a little, and
opponents far behind push a little harder. They use nitro on straights.

### Screens

1. **Title:** game name, *Race*, *Best time*, a mute button.
2. **Choose your car:** the four cars with their stat bars; tap to pick.
3. **Race:** 3D view from a chase camera behind the car, with a HUD showing
   position (e.g. *3rd / 8*), lap (*2 / 3*), speed, the nitro bar, and the
   current lap time. Touch controls appear on phones; a pause button is in
   a corner.
4. **Pause:** resume, restart, controls (tilt or arrows), sound, quit.
5. **Results:** finishing place, total time, best lap, *New best!* if it
   beat the record, and *Race again* / *Change car* / *Title*.

### Looks and sound

- Low-poly stylised city at sunset: warm sky gradient, flat-shaded blocks
  for buildings, neon signs in the tunnel, water along the harbor.
- Speed lines and camera shake during nitro; sparks when scraping walls;
  a brief slow-motion on a wreck.
- Simple synthesised engine sound whose pitch follows speed, a nitro whoosh
  and a crash thump, all made with the Web Audio API. A mute button on
  every screen; the choice is remembered.
- Respects light and dark system theme on the menu screens.

### What is saved

In `localStorage`, on that device only: best race time, best lap, chosen car,
control scheme (tilt or arrows) and mute state. Nothing leaves the browser.

## How it is built

### Files

```
velocity/
  index.html                 the page, loads Three.js from the CDN then the modules
  README.md                  what it is, how to play, how to run the tests
  css/style.css              the look of the menus and HUD
  js/track.js                the circuit: centreline points, widths, banking, ramp
  js/physics.js              car motion: throttle, steering, drift, nitro, collisions
  js/ai.js                   opponent driving and rubber-banding
  js/race.js                 laps, positions, countdown, finish, results, records
  js/scene.js                builds the 3D world and cars with Three.js, draws frames
  js/app.js                  screens, touch/keyboard/tilt input, HUD, sound
  tests/velocity-logic.test.js   plain-Node tests for track, physics, ai, race
.github/workflows/velocity.yml   runs the tests on pushes that touch velocity/
```

### Dependencies

- **Three.js** from `https://cdn.jsdelivr.net/npm/three@<pinned version>/`,
  loaded with a plain `<script>` tag the way `earth/index.html` loads
  MapLibre. The version is pinned and at least two weeks old.
- Nothing else. No bundler, no package manager, no build step.

### Module boundaries

Each logic module is a plain script that works both in the browser (as a
global) and in Node (via `module.exports`), the pattern `earth/js/geo.js`
and `mandala/js/mandala.js` use. None of `track`, `physics`, `ai` or `race`
touches the DOM or Three.js, so they are tested in plain Node.

- **`track.js`** — holds Harbor Loop as an ordered list of centreline points,
  each with a road width, banking angle and flags (tunnel, ramp). It answers:
  the nearest point on the centreline to a position, the distance along the
  lap (for positions and lap counting), the road direction and width there,
  and whether a position is off the road (for wall collisions). It also
  gives the eight grid start slots.
- **`physics.js`** — one car's state (position, heading, speed, lateral slip,
  nitro, airborne, wrecked) and a `step(car, input, track, dt)` that moves it
  one frame. Includes car-to-car collision resolution between two car states.
  Car stats come from a table of the four cars.
- **`ai.js`** — given an opponent's state, the track and the player's distance
  along the lap, produces the same input shape a human would (throttle,
  steer, drift, nitro). Skill and aggression are per-opponent numbers.
- **`race.js`** — owns the list of cars, the countdown, lap counting (using
  track distance so a car cannot cheat by driving backwards over the line),
  position ordering, finish detection, and the results. Also reads and
  writes the saved best times through an injected storage object so tests
  can pass in a fake.
- **`scene.js`** — turns the track into meshes (road, kerbs, walls, buildings,
  tunnel, water, ramp, sky), creates one mesh per car, keeps the chase camera
  behind the player, and renders. Reads car states; never changes them.
- **`app.js`** — the game loop (fixed-timestep physics, variable-rate render),
  screens, input gathering (touch zones, keyboard, device orientation), the
  HUD, and Web Audio sound.

### Data flow each frame

input (app) → player input + AI inputs (ai) → physics step for every car
(physics, using track) → race update (race: laps, positions, finish) →
render (scene) and HUD (app).

### Errors and edge cases

- If Three.js fails to load (no internet on first open), the page shows a
  plain message saying the game needs an internet connection the first time
  it loads, instead of a blank screen.
- If device orientation is unavailable or denied, steering falls back to the
  on-screen arrows and the tilt option is greyed out.
- If `localStorage` is unavailable (private browsing), the game still plays;
  records simply are not kept.
- The game pauses itself when the tab is hidden and resumes with a 3-2-1
  countdown so a phone call does not lose the race.
- A car that leaves the road entirely (a physics bug or an extreme bounce) is
  snapped back onto the road by the respawn logic after one second.

### Performance

Target 60 fps on a mid-range phone: low-poly meshes, flat shading, one
directional light plus ambient, no shadow maps on phones, merged static
geometry for the city, and a capped device-pixel-ratio of 2.

### Testing

`node velocity/tests/velocity-logic.test.js`, no dependencies, covers:

- **track:** distance along the lap increases round the circuit and wraps at
  the line; nearest-point lookup; a point beyond the road width counts as
  off-road; eight distinct grid slots on the road.
- **physics:** auto-acceleration reaches but does not exceed top speed;
  steering turns the heading; drifting keeps more speed than braking and
  fills nitro; nitro raises speed above top speed and drains; the ramp makes
  the car airborne and it lands; a hard wall hit wrecks and respawns; two
  overlapping cars are pushed apart.
- **ai:** an opponent on the track produces inputs that keep it on the road
  over a full lap; rubber-banding eases off when far ahead.
- **race:** countdown ends before cars move; laps count only when crossing
  the line forwards; positions order by laps then distance; the player
  finishing ends the race with a correct place; best time is saved only when
  beaten.

CI: `.github/workflows/velocity.yml` runs that file on pushes and pull
requests that touch `velocity/` or the workflow. The repo-wide
`scripts/check-links.mjs` and `scripts/check-exchange.mjs` gates already run
on every push and must pass.

## Out of scope for this version

Multiple tracks, a career or events, car upgrades and unlocks, currency,
multiplayer, leaderboards, licensed cars or music, and joining MAZ ARCADE.
