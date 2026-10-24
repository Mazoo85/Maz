# Velocity

**An arcade street racer in your browser.** One sunset city circuit, seven
rivals, three laps. Drift through the corners to fill your nitro, then burn it
on the straights. Scrape a wall and you lose speed; hit one head-on and you're
wrecked for a couple of seconds. Hit the ramp flat out and fly.

**Play it:** [velocity/](./index.html) — on the live site,
`https://mazoo85.github.io/Maz/velocity/`. On a phone, use your browser's
*Add to Home Screen* and it opens like an app.

No account, no install and no build step. It needs an internet connection
the first time it loads, to fetch its 3D engine ([Three.js](https://threejs.org/)).
A standalone app: it is not part of MAZ ARCADE and does not use the Maz engine.

## How to play

| | Phone | Keyboard |
|---|---|---|
| **Go** | The car accelerates on its own. | Same. |
| **Steer** | ◀ ▶ buttons, or tilt the phone (*Steering: tilt* on the title screen). | Arrow keys or A / D. |
| **Drift / brake** | Hold **DRIFT** while steering to slide round a corner. Hold it with no steering to brake. | Shift, or the down arrow / S. |
| **Nitro** | Tap **NITRO**. One tap burns for a moment; tap again to keep burning. | Space. |
| **Pause** | The **II** button. | P or Escape. |

The nitro bar fills when you drift, when you pass another car very close
(a *near miss*), and when you land a jump.

## The cars

| Car | Character |
|---|---|
| **Comet** | The all-rounder. |
| **Brawler** | Highest top speed and the strongest nitro, but it turns like a boat. |
| **Dart** | First off the line and sharpest in the corners; lowest top speed. |
| **Drifter** | Slides the longest and fills its nitro fastest. |

## The track

**Harbor Loop**, about 3 km: the start/finish straight along the harbor, a
long sweep round the headland, a tight chicane, a neon-lit tunnel, the jump, and a
run of fast corners back to the water.

Your best race time and best lap are kept in your browser on that device
(localStorage). Nothing leaves your browser.

## Files

| File | What it is |
|---|---|
| `js/track.js` | Harbor Loop and the maths for "where on the track am I". |
| `js/physics.js` | How a car moves: throttle, steering, drift, nitro, the ramp, walls and crashes. |
| `js/ai.js` | How the seven rivals drive. |
| `js/race.js` | Laps, positions, the countdown, the finish, results and the best-time record. |
| `js/scene.js` | The 3D city and the cars, drawn with Three.js. |
| `js/app.js` | The screens, controls, HUD, sound and game loop. |
| `tests/velocity-logic.test.js` | `node velocity/tests/velocity-logic.test.js`. Runs in CI via `.github/workflows/velocity.yml`. |
