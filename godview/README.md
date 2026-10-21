# GOD'S EYE VIEW

A live 3D globe in the spy-console style, made for phones. Spin the Earth with a
finger, pinch to zoom, tap anything for its details.

**Open it:** [godview/](./) — on a phone, use your browser's *Add to Home Screen*
and it opens full screen like an app.

## Layers

| Layer | Source | Refresh | Licence |
|---|---|---|---|
| 〰 Earthquakes | [USGS](https://earthquake.usgs.gov/earthquakes/feed/v1.0/geojson.php), every quake of the past day | 60 s | U.S. public domain |

Satellites, weather, planes, cameras and ships are planned — see the
[design](../docs/superpowers/specs/2026-10-21-godview-design.md).

If a source stops answering, its button says **OFFLINE** and it retries on its
own; the rest of the globe keeps working.

## How it is built

A static page — no server, no account, no API key. The globe is
[globe.gl](https://github.com/vasturiano/globe.gl) 2.46.2 (MIT) from jsDelivr.
Country outlines are Natural Earth 1:110m (public domain) via
[world-atlas](https://github.com/topojson/world-atlas) (ISC), converted once by
`tools/build-countries.cjs` into `data/countries.geojson`.

- `js/lib/` — pure logic: feed parsing, the poller, formatting
- `js/layers/` — one file per layer, all on the same small contract
- `js/app.js` — the only file that touches the page or the globe

## Tests

    node godview/tests/godview-logic.test.js        # no dependencies
    npm --prefix music/tests install                # once, for Playwright
    node godview/tests/godview-browser.test.js      # phone-size Chromium

The browser test answers the USGS feed from `tests/fixtures/usgs-sample.json`,
so it never depends on today's earthquakes. Set `GLOBE_JS` to a local copy of
`globe.gl.min.js` to run it with no internet.

Inspired by [God's Eye View](https://github.com/bilawalsidhu/gods-eye-view) by
Bilawal Sidhu (MIT); no code is shared with it.
