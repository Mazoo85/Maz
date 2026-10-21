# GOD'S EYE VIEW — Step 2 (satellites) Implementation Plan

**Goal:** A SATS layer: the space stations and ~150 brightest satellites from CelesTrak, moving
smoothly in real time, tappable for name, height, speed and position.

**Spec:** `docs/superpowers/specs/2026-10-21-godview-design.md` · builds on
`docs/superpowers/plans/2026-10-21-godview-step1.md` (layer contract, global constraints).

## Decisions

- **Orbit maths:** `satellite.js` **5.0.0** (MIT), vendored as `godview/vendor/satellite.min.js`.
  5.x is the last release with a single-file UMD build, so the same file serves the page and the
  dependency-free Node tests. (6.x/7.x are ESM-only and parse OMM JSON; 5.x parses TLE text.)
- **Source:** CelesTrak GP in TLE format, two groups, refreshed every 2 h (CelesTrak's own guidance):
  `https://celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=tle` and
  `https://celestrak.org/NORAD/elements/gp.php?GROUP=visual&FORMAT=tle`.
  Either group alone is enough to show something; both failing is OFFLINE. Duplicates (the ISS is
  in both) are kept once, as a station.
- **Drawing:** globe.gl HTML markers (`htmlElementsData`), not points: satellites float at their
  real altitude, are easy to tap, and glide between once-a-second positions
  (`htmlTransitionDuration(1000)`). Markers hidden behind the Earth are faded and untappable.
- **Look:** stations amber with a label for the ISS and Tiangong; other satellites green.

## Layer contract additions (backward compatible)

```text
html      array    floating markers: { layer, item, lat, lng, altitude, color, label|null }
                   (objects keep their identity between ticks; positions are updated in place)
tick()    optional called once a second by the app while the layer is on; recomputes positions
```

The app owns the one-second clock (and skips it while the page is hidden). Every layer without
`html` or `tick` is unaffected.

## Tasks

1. **`js/lib/sats.js` (`GV_SATS`)** — `FEEDS`, `parseTle(text)` (name + two lines, column and
   checksum validated, malformed entries skipped), `checksum(line)`,
   `positionAt(satrec, date, satlib) -> {lat, lng, altKm, speedKmS} | null`.
   Tests: fixtures `celestrak-stations.txt` / `celestrak-visual.txt` (synthetic but valid elements,
   epoch 17 Oct 2026, one bad checksum, one truncated); ISS at a fixed time is 370–460 km up at
   7.5–7.8 km/s and within ±51.7° latitude; a garbage record gives `null`.
2. **`js/layers/sats-layer.js` (`GV_SATS_LAYER`)** — fetch both groups, merge and de-duplicate,
   build stable `html` items, `tick()` moves them, `describe()` gives name / altitude / speed
   (km/h and mph) / position / NORAD id with an N2YO link. Tests: counts, dedupe, station
   labels, one group down still live, both down OFFLINE, tick moves the ISS, stop clears.
3. **Page** — vendor script tag, HTML-marker channel and one-second ticker in `app.js`, marker
   CSS, SATS on by default. Browser test (CelesTrak answered from fixtures re-dated to a day
   before the run — pinning the page clock instead stops Playwright's page timers, and the
   satellites move on a timer): markers drawn, a real tap on the ISS marker opens its sheet, markers move,
   SATS OFFLINE when CelesTrak is down while QUAKES stays live.
4. **Docs** — godview README (layer row, vendored licence), hub blurb, root README row.
