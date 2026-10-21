# GOD'S EYE VIEW — Step 3 (weather) Implementation Plan

**Goal:** A WEATHER layer: live rain radar and the day's cloud cover drawn onto the globe's
surface, in the console colours, toggled from the layer bar.

**Spec:** `docs/superpowers/specs/2026-10-21-godview-design.md` · builds on the step 1 and 2 plans
(layer contract, global constraints).

## Decisions

- **Clouds:** NASA Worldview Snapshots, one global 2048×1024 equirectangular image of the
  previous UTC day's VIIRS (NOAA-21) true-colour mosaic:
  `https://wvs.earthdata.nasa.gov/api/v1/snapshot?REQUEST=GetSnapshot&LAYERS=VIIRS_NOAA21_CorrectedReflectance_TrueColor&CRS=EPSG:4326&TIME=<YYYY-MM-DD>&BBOX=-90,-180,90,180&WIDTH=2048&HEIGHT=1024&FORMAT=image/jpeg`.
  Keyless and browser-direct (documented as CORS `*` by the God's Eye View project, which uses
  the same service from the browser). Only clouds are kept: bright, unsaturated pixels become
  pale console-green with alpha on a ramp; land, sea and the black gaps between orbits drop out.
  Daily, so it is labelled as such; refetched hourly in case a newer day has been published.
- **Rain:** RainViewer's public radar (keyless, browser-direct): `weather-maps.json` gives the
  newest frame, whose 256 px tiles at zoom 2 (4×4, Web Mercator) are stitched and reprojected
  onto the same 2048×1024 equirectangular grid. Refreshed every 10 minutes (RainViewer's frame
  cadence). Coverage is wherever national radar networks exist.
- **Either source alone is shown; both failing is OFFLINE.**
- **Drawing:** the two are baked with the dark globe colour into one texture, set as the
  globe image (`globeImageUrl`) while WEATHER is on; the plain dark material returns when off.
  Country outlines, markers and rings sit above it unchanged. No extra download at boot:
  WEATHER is **off by default** (it is the heaviest layer).
- **Button:** shows the radar frame's time (e.g. `06:10 UTC`), or `CLOUDS` if only clouds loaded.
- **Credits:** a one-line credit strip above the layer bar while WEATHER is on:
  "Clouds: NASA GIBS / VIIRS · Rain: RainViewer" (both ask for credit).

## Layer contract additions (backward compatible)

```text
texture   optional { width, height, data: Uint8ClampedArray (RGBA) } | null — painted onto the
                   globe surface while the layer is on
summary   optional string shown on the button instead of the count
credit    optional string shown in the credit strip while the layer is on
```

## Tasks

1. **`js/lib/weather.js` (`GV_WEATHER`)** — pure pixel maths, no DOM: `cloudsUrl(date)`,
   `RAIN_INDEX_URL`, `rainTileUrl(host, path, z, x, y)`, `latestRainFrame(index)`,
   `cloudAlpha(r, g, b)`, `mercatorY(lat, size)`, `stitch(tiles, n, tileSize)`,
   `composite({ width, height, clouds, rain, rainSize })`. Tests on tiny synthetic images:
   white cloud kept, blue sea/green land/black gap dropped, rain lands at the right latitude
   after reprojection, nothing drawn beyond ±85°, dark base where there is nothing.
2. **`js/layers/weather-layer.js` (`GV_WEATHER_LAYER`)** — fetch clouds and rain (injected
   `fetchJson`, `loadPixels(url)`), composite, expose `texture` / `summary` / `credit`;
   clouds hourly and rain every 10 min on one poller (10 min; the clouds image is only
   re-downloaded when its date changes or an hour has passed). Tests: both, only clouds, only
   rain, both down OFFLINE, stop clears the texture.
3. **Page** — `loadPixels` (Image with `crossOrigin='anonymous'` → canvas → `getImageData`),
   texture → canvas → `globeImageUrl`, credit strip, summary on the button, WEATHER off by
   default. Browser test with both sources answered from generated fixture images: turning
   WEATHER on paints the globe (texture present, globe image set), button shows the radar time,
   credit strip visible; turning it off restores the plain globe; both down shows OFFLINE while
   QUAKES/SATS carry on.
4. **Docs** — godview README row and credits, hub blurb, root README row.
