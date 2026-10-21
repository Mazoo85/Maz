# GOD'S EYE VIEW

A live 3D globe in the spy-console style, made for phones. Spin the Earth with a
finger, pinch to zoom, tap anything for its details.

**Open it:** [godview/](./) — on a phone, use your browser's *Add to Home Screen*
and it opens full screen like an app.

## Layers

| Layer | Source | Refresh | Licence |
|---|---|---|---|
| 〰 Earthquakes | [USGS](https://earthquake.usgs.gov/earthquakes/feed/v1.0/geojson.php), every quake of the past day | 60 s | U.S. public domain |
| 🛰 Satellites | [CelesTrak](https://celestrak.org/NORAD/elements/) space stations + ~150 brightest, moved every second by the SGP4 orbit model | orbits every 2 h | U.S. government data; CelesTrak asks for credit |
| ✈ Planes | [adsb.lol](https://adsb.lol) (ODbL) around the view; [OpenSky Network](https://opensky-network.org) if adsb.lol fails | 15 s, and when you stop spinning somewhere new | ODbL / OpenSky non-commercial; credited on screen |
| ☁ Weather | Rain: [RainViewer](https://www.rainviewer.com/api.html) radar, newest frame · Clouds: [NASA GIBS](https://gibs.earthdata.nasa.gov) VIIRS photo of yesterday (UTC) | rain 10 min · clouds hourly | Both ask for credit, shown on screen while WEATHER is on |

Stations glow amber (the ISS and Tiangong are labelled), other satellites green.
Tap one for its height, speed and position, and a link to its live track.

WEATHER is off until you tap it (it is the biggest download). Rain shows wherever
countries run weather radar; the clouds are a whole-world photo of the day before,
with land and sea filtered out so only cloud is left. The button shows the radar
frame's time.

We acknowledge the use of imagery provided by services from NASA's Global Imagery
Browse Services (GIBS), part of NASA's Earth Science Data and Information System
(ESDIS).

PLANES is off until you tap it. It shows up to 300 aircraft within 250 nautical
miles (460 km) of the middle of the screen — the free flight services only answer
for one area at a time — and asks again whenever you stop spinning the globe
somewhere new. Between updates each plane flies on along its heading at its
speed, so they move smoothly. Planes on the ground are dimmed.

Cameras and ships are planned — see the
[design](../docs/superpowers/specs/2026-10-21-godview-design.md).

If a source stops answering, its button says **OFFLINE** and it retries on its
own; the rest of the globe keeps working.

## How it is built

A static page — no server, no account, no API key. The globe is
[globe.gl](https://github.com/vasturiano/globe.gl) 2.46.2 (MIT) from jsDelivr.
Country outlines are Natural Earth 1:110m (public domain) via
[world-atlas](https://github.com/topojson/world-atlas) (ISC), converted once by
`tools/build-countries.cjs` into `data/countries.geojson`.

Orbits are worked out by [satellite.js](https://github.com/shashwatak/satellite-js)
5.0.0 (MIT, `vendor/satellite.LICENSE.md`), kept in `vendor/` rather than loaded from a
CDN: 5.x is its last release built as a single file, so the page and the
dependency-free Node tests use the very same copy.

- `js/lib/` — pure logic: feed parsing, the poller, formatting
- `js/layers/` — one file per layer, all on the same small contract
- `js/app.js` — the only file that touches the page or the globe

## Tests

    node godview/tests/godview-logic.test.js        # no dependencies
    npm --prefix music/tests install                # once, for Playwright
    node godview/tests/godview-browser.test.js      # phone-size Chromium

The browser test answers USGS and CelesTrak from `tests/fixtures/`, and NASA and
RainViewer with small pictures it draws itself, and the flight services from
saved samples, so it never depends on today's earthquakes, orbits, weather or
traffic. The satellite fixtures are synthetic
but valid elements; the browser test re-dates them to a day before each run. Set `GLOBE_JS` to a local copy of
`globe.gl.min.js` to run it with no internet.

Inspired by [God's Eye View](https://github.com/bilawalsidhu/gods-eye-view) by
Bilawal Sidhu (MIT); no code is shared with it.
