# Terra Desk for iPhone

**Your 3D globe, live.** Ships, aircraft, rocket launches, the aurora, volcanoes, lightning, 8,700 battles, old maps
laid over today's, the Moon and the planets, the ocean floor, road trips, hiking trails and a 3D map builder, on one
globe you spin with your finger.

**Open it:** `https://mazoo85.github.io/Maz/terradesk/`

**Put it on your iPhone's home screen**

1. Open the address above in **Safari** (it has to be Safari for this).
2. Tap the **Share** button (the square with an arrow pointing up).
3. Scroll down and tap **Add to Home Screen**, then **Add**.
4. Open **Terra Desk** from its new icon. It opens full screen, like an app.

The ☰ button at the top left opens the layers and tools. Tap the globe for a place's coordinates and height or depth.

A standalone app: it is not part of MAZ ARCADE. It is the phone version of the Terra Desk desktop program, with the
same layers and code; the Android version is a separate download.

## What works where

| Layer | On iPhone |
|---|---|
| Aurora, Launches, Volcanoes, Battles, Lightning, Old Maps, Other Worlds, Ocean Floor, Road Trip, Trails, Map Builder | Works in Safari, straight from each service. |
| Aircraft Spotter | Planes near where you are looking, through the GOD'S EYE VIEW planes relay ([`godview/relay/`](../godview/relay/README.md)), because the flight services refuse web pages. The worldwide military list is not available on iPhone yet. |
| Ship Watch | Needs your own free aisstream.io key (☰ → Settings → Keys), kept only on your phone. aisstream.io may refuse connections from web pages; if it does, Ship Watch says so. |

**Saving** (road trips, trails, map builder files) opens the iPhone share sheet: choose **Save to Files**.

**Offline:** after the first open, the app, the globe library and the maps you have looked at are kept on the phone,
so it opens without signal; live layers need the internet.

## How it is built

The same page as the desktop and Android apps (CesiumJS 1.147.0 from jsDelivr, the layers in `js/layers/`), with
`js/td-web.js` doing in the page what the desktop's main process did: it asks only Terra Desk's own data services,
one request at a time per service with polite gaps, keeps answers for a while, falls back to Overpass mirrors and to
the last answer when the signal drops, and keeps settings and keys in the phone's own storage. `sw.js` is the offline
helper.

Bundled: the battle and volcano lists (`data/`, the same lists Earth Explorer uses), the Global Force Registry
aircraft list, and the Solar System Scope 2K maps of Earth, the Moon, the planets and the Sun (CC BY 4.0) in `assets/`.

## Tests

```sh
node terradesk/tests/terradesk-logic.test.mjs            # no dependencies, seconds
npm --prefix music/tests install && npx --prefix music/tests playwright install webkit chromium
node terradesk/tests/terradesk-browser.test.cjs          # the page as an iPhone in WebKit and a phone in Chromium
BASE_URL=https://mazoo85.github.io/Maz/terradesk/ node terradesk/tests/terradesk-browser.test.cjs   # the live site
node terradesk/tests/probe-services.mjs                  # which outside services answer the page
```

CI runs all of it in `.github/workflows/terradesk.yml`.
