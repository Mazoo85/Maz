# GOD'S EYE VIEW — Step 4 (planes) Implementation Plan

**Goal:** A PLANES layer: live aircraft around the part of the globe you are looking at, drawn
as small plane glyphs pointing along their heading, flying smoothly between updates, tappable
for callsign, height, speed, heading and type.

**Spec:** `docs/superpowers/specs/2026-10-21-godview-design.md` · builds on the step 1–3 plans.

## Decisions

- **Sources:** keyless, community ADS-B data, regional by design.
  - Primary: adsb.lol point API (ODbL) —
    `https://api.adsb.lol/v2/point/<lat>/<lon>/250` (radius in nautical miles, 250 max).
  - Fallback when the primary fails: OpenSky Network anonymous REST (non-commercial licence) —
    `https://opensky-network.org/api/states/all?lamin=..&lomin=..&lamax=..&lomax=..`, a box of
    the same size. Anonymous OpenSky has a small daily allowance, so it is only asked when
    adsb.lol has failed.
  - Neither is documented as allowing direct browser requests (God's Eye View proxies both).
    If both refuse a real phone, the button says OFFLINE and the options (a small relay) go to
    Cody rather than a key into the page.
- **Where:** the point under the camera centre at fetch time (`globe.pointOfView()`), every
  15 s, and again as soon as a drag/zoom ends somewhere new (more than 2° away).
- **Cap:** at most 300 aircraft, nearest the centre first, for phone speed. Positions older than
  60 s are dropped.
- **Motion:** each tick (1 s) moves every airborne aircraft along its track at its ground speed
  (dead reckoning), so they fly instead of jumping every 15 s.
- **Drawing:** HTML markers (as satellites), glyph `✈` rotated to the track, amber; aircraft on
  the ground dimmed. Altitude is real (feet → Earth radii).
- **Off by default** (heavy, and regional).
- **Credit:** "Planes: adsb.lol (ODbL)" or "Planes: OpenSky Network", whichever answered.

## Layer contract additions (backward compatible)

```text
html[].rotation   optional degrees clockwise from north; the app turns the marker to it
followsView       optional true: the app calls refresh() when the view settles somewhere new
opts.getView()    -> { lat, lng, altitude } given to layers that follow the view
```

## Tasks

1. **`js/lib/planes.js` (`GV_PLANES`)** — `adsbUrl(lat, lng)`, `openskyUrl(lat, lng)`,
   `parseAdsb(json)`, `parseOpensky(json)` → `Plane { hex, callsign, lat, lng, altFt|null,
   onGround, speedKt, track, type, reg }`, `distanceKm(a, b)`, `nearest(list, centre, cap)`,
   `advance(plane, seconds)` (great-circle step along the track). Tests with saved samples of
   both formats: junk and stale rows dropped, "ground" altitude, units converted (OpenSky m and
   m/s → ft and kt), cap keeps the nearest, advancing 60 s at 480 kt due east moves ~8 nm east.
2. **`js/layers/planes-layer.js` (`GV_PLANES_LAYER`)** — poller (15 s) asking adsb.lol around
   `getView()`, falling back to OpenSky; markers with rotation; `tick()` dead-reckons; describe
   (callsign, ALTITUDE ft/m, SPEED kt/mph, HEADING, AIRCRAFT type · reg, adsb.lol globe link);
   credit by source. Tests: primary used, fallback used when primary fails, both down OFFLINE,
   tick moves airborne planes only, view followed, stop clears.
3. **Page** — script tags, rotation applied to markers, `followsView` refresh on the controls'
   `end`, plane marker CSS, PLANES off by default. Browser test (both sources answered from the
   samples, one turned off at a time): count, markers, a tap opens the sheet, planes move,
   fallback credit, both down OFFLINE while the rest carry on.
4. **Docs** — README row and credits, hub blurb, root README row.
