# GOD'S EYE VIEW (`godview/`) — design

**Date:** 2026-10-21 · **Status:** approved in conversation, awaiting spec review

## What it is

A phone-first, browser-only live intelligence globe for MAZ ARCADE, in the spirit of
[bilawalsidhu/gods-eye-view](https://github.com/bilawalsidhu/gods-eye-view) (MIT). We build our
own; we do not copy that project's code. It is served by the existing Pages workflow at
`https://mazoo85.github.io/Maz/godview/` and gets a tile on the hub (`index.html`).

## Decisions made

| Question | Decision |
|---|---|
| Layers | Planes, satellites, earthquakes, weather, cameras, ships (all six) |
| Look | "Spy control room": dark globe, glowing green country outlines, amber/green markers, console type |
| Globe tech | A ready-made globe library loaded from a pinned CDN URL (`globe.gl`, built on three.js) |
| Hosting | Static page on GitHub Pages. No server of our own, no API keys in the page |

## Screen and interaction

- **Globe** fills the screen. One-finger drag spins it, pinch zooms. Works with mouse too.
- **Top bar:** `GOD'S EYE VIEW` title, live UTC clock, a `LIVE` light that blinks on each data refresh.
- **Bottom bar:** six large toggle buttons (min 44 px tap target), each with a live count, e.g.
  `PLANES 4,812`. A layer whose source is failing shows greyed out with `OFFLINE`.
- **Tapping a marker** opens a bottom sheet with its details (flight callsign/altitude/speed;
  satellite name/height/speed; quake magnitude/age/place; camera image).
- Respects phone safe areas (notch, home bar); installable to the home screen (web app manifest).
- Default on first load: earthquakes and satellites on; heavier layers (planes, cameras) off until tapped.

## Layers and sources

All keyless and free. Each must be readable directly by a web page (CORS); this is verified on a
real phone at each step because the build box cannot reach these hosts.

| Layer | Source | Refresh | Notes |
|---|---|---|---|
| Earthquakes | USGS GeoJSON feed (past day) | 60 s | Pulsing ring, radius scaled by magnitude |
| Satellites | CelesTrak GP data (stations + brightest, ~200 objects) | fetched once / 2 h | Positions propagated in the page every frame (SGP4) |
| Weather | RainViewer radar + NASA GIBS cloud imagery | 10 min | Semi-transparent overlay |
| Planes | adsb.lol point API, OpenSky as fallback | 15 s | Only the area in view is fetched, capped for phone speed |
| Cameras | Public DOT / city camera feeds that permit browser access | 60 s still images | Keep only feeds that pass the CORS check |
| Ships | AISStream (needs a free key) | — | Expected to be blocked from browsers. If so, stop and bring options back to Cody (e.g. a small relay server) rather than putting a key in a public page |

## Structure

```
godview/
  index.html        page shell, top bar, bottom bar, bottom sheet
  css/godview.css   spy-console theme, safe areas
  js/app.js         boots the globe, owns the layer registry and the UI
  js/layers/*.js    one file per layer: quakes, sats, weather, planes, cameras, ships
  js/lib/*.js       pure logic: feed parsing, orbit math, formatting, retry/backoff
  tests/*.test.js   plain Node tests, like film/ and music/
  README.md
```

Every layer implements the same small interface — `start()`, `stop()`, `refresh()`, a `status`
(`live` / `loading` / `offline`) and a `count` — so `app.js` treats them all the same and one
failing layer cannot break another. Fetch failures back off and retry on their own.

Pure logic (parsing each feed into plain points, SGP4 propagation, unit formatting, the
backoff timer) lives in `js/lib/` with no DOM or globe dependency, so it is unit-tested in Node.

## Testing

- `node godview/tests/*.test.js` — feed parsers against saved sample responses, orbit maths
  against known positions, formatting, and the offline/backoff path.
- Load the page headless in Chromium (Playwright) at phone size: it renders, buttons fit and are
  tappable, a failed source shows `OFFLINE` without errors.
- Repo gates: `node scripts/check-links.mjs && node scripts/check-exchange.mjs`.
- Cody does one live check on a real phone after each build step.

## Build order

1. Globe, theme, bottom bar, earthquakes
2. Satellites
3. Weather
4. Planes
5. Cameras
6. Ships, or a check-in if blocked

## Out of scope (for now)

Photoreal 3D cities, voice control, search, accounts, any server of our own.
