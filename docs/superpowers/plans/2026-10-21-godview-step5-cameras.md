# GOD'S EYE VIEW — Step 5 (cameras) Implementation Plan

**Goal:** A CAMERAS layer: public traffic and road-weather cameras as tappable markers; a tap
shows the camera's latest picture, refreshed every minute while it is open.

**Spec:** `docs/superpowers/specs/2026-10-21-godview-design.md` · builds on the step 1–4 plans.

## Decisions

- **Sources** — keyless open-data camera lists, each optional (any one is enough to be live):
  - **London** — TfL JamCams, `https://api.tfl.gov.uk/Place/Type/JamCam` (~900; frames on TfL's
    public S3 bucket). TfL Open Data terms, attribution required.
  - **Finland** — Fintraffic/Digitraffic weather cameras,
    `https://tie.digitraffic.fi/api/weathercam/v1/stations` (~800 stations; one marker per
    station, its first preset in collection; frames `https://weathercam.digitraffic.fi/<preset>.jpg`).
    CC BY 4.0, attribution required. No custom `Digitraffic-User` header: it would force a CORS
    preflight from the browser.
  - **California** — Caltrans CCTV for districts 4, 7, 11, 3 (SF Bay, LA, San Diego,
    Sacramento), `https://cwwp2.dot.ca.gov/data/d<N>/cctv/cctvStatusD<NN>.json`; in-service
    cameras only.
  - Left out on purpose: NYC DOT (needs a signed data agreement) and FL511 (personal use only).
- **Why this works from a page:** only the *lists* need the source to allow web pages (CORS);
  the pictures are plain `<img>` tags, which any page may show. A list a source refuses simply
  drops out.
- **Cap:** at most 300 markers, nearest the middle of the screen; re-chosen from the lists
  already in hand when the view settles somewhere new (`followsView`), without re-downloading.
  Lists are refetched every 15 minutes.
- **Look:** cyan dots. The sheet shows the picture (cache-busted), its source, and a link.
- **Off by default.** Credits name each source that answered.

## Layer contract additions (backward compatible)

```text
describe(item).image   optional picture URL; the sheet shows it and refreshes it every 60 s
```

## Tasks

1. **`js/lib/cameras.js` (`GV_CAMERAS`)** — `SOURCES` (id, label, urls, credit),
   `parseTfl`, `parseDigitraffic`, `parseCaltrans` → `Camera { id, name, lat, lng, image,
   source }`; unavailable/unplaced rows dropped; only https picture URLs kept.
2. **`js/layers/cameras-layer.js` (`GV_CAMERAS_LAYER`)** — fetch every source (allSettled),
   keep last-good lists, pick the nearest 300 to `getView()`, `refresh()` re-picks from cached
   lists when they are fresh; describe with image + source + link.
3. **Page** — sheet picture with a 60 s refresh while open; camera marker CSS; CAMERAS off by
   default. Browser test with the three lists from samples and a drawn PNG for every picture.
4. **Docs** — README row and credits, hub blurb, root README row.
