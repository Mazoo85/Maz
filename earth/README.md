# Earth Explorer

**Your own Google Earth.** A 3D globe of satellite photos that you can spin,
zoom from space down to a single street, and tilt to see mountains and city
buildings in 3D. Search for anywhere, save your own places with notes, and
measure distances and areas.

**Open it:** [earth/](./index.html) — on the live site,
`https://mazoo85.github.io/Maz/earth/`.

**Install it as an app.** iPhone/iPad: open it in Safari, tap *Share*, then
*Add to Home Screen*. Android or a computer with Chrome/Edge: *Install as an
app* in Layers, or the install icon in the address bar. It then opens full
screen from its own icon, like any other app.

No account, no key and no build step: open `index.html` in any up-to-date
browser. A standalone app: it is not part of MAZ ARCADE.

## What it does

| | |
|---|---|
| **Globe** | The whole Earth in satellite photos, with an atmosphere round the edge. It turns slowly when you are zoomed out (switch that off in Layers). Drag to spin, pinch or scroll to zoom, two-finger drag or right-drag to tilt and turn. |
| **Search** | Type a place, an address or coordinates such as `48.8584, 2.2945` and press search. Pick a result and the map flies there; *Save this place* keeps it. |
| **3D** | Mountains rise when you tilt the view (the **3D** button tilts it for you). Zoom into a city to see its buildings standing up. *Make mountains taller* exaggerates the height up to 3×. |
| **Real 3D** | Real buildings, trees and bridges in photo detail, from Google's Photorealistic 3D Tiles. Opens where the map is looking and hands the view back when you press *Back to map*. Needs a free Google key of your own (below). |
| **My places** | *Drop a pin* and tap the map, or *Pin the middle*, or right-click with a mouse. A name is suggested from OpenStreetMap; add your own notes. Tap a place in the list to fly to it. Drag a new pin to move it before saving. |
| **Measure** | *Distance*: tap points along a route. *Area*: tap round the edge of a field, a lake or a country. Lines follow the curve of the Earth, so long distances are right. |
| **Units** | Kilometres or miles, in Layers. |
| **Share** | The view is kept in the page address, so *Copy a link to this view* sends someone exactly what you are looking at. |
| **Me** | Flies to where you are (your browser asks first). |

Your places are kept in your browser on that device. **Save to a file** in
My places writes `my-places.json`; **Open a file** on another device reads it
back in without saving anything twice. The first visit shows three example
places (marked *Example*), which you can delete.

## Setting up Real 3D

The first time you press **Real 3D**, the app shows these steps:

1. Open the [Map Tiles API page](https://console.cloud.google.com/apis/library/tile.googleapis.com), sign in and press **Enable**. Google asks you to set up billing the first time.
2. Open [Credentials](https://console.cloud.google.com/apis/credentials), press **Create credentials**, then **API key**.
3. Edit the key. Under *Application restrictions* choose **Websites** and add `mazoo85.github.io/*`. Under *API restrictions* choose **Map Tiles API**. Save.
4. Paste the key into the app.

The key is kept in that browser only, never in this repository. Google counts
each opening of the 3D view as one visit (one "root tileset" request) and
gives a free allowance every month; see the [Map Tiles API billing
page](https://developers.google.com/maps/documentation/tile/usage-and-billing)
for the current numbers. To be certain of never paying, set a daily limit
under [Quotas](https://console.cloud.google.com/apis/api/tile.googleapis.com/quotas).
The key can be changed or removed from **Layers → Real 3D key**.

The 3D view is drawn by [CesiumJS](https://cesium.com/platform/cesiumjs/)
1.145.0, downloaded only the first time you open it. It is a separate view
over the map rather than part of it, because Google's terms limit showing its
tiles together with other map data. There is no search inside it.

## Where the map comes from

All free, with no sign-up. Credits show in the corner of the map.

| | Source |
|---|---|
| Satellite photos | [Esri World Imagery](https://www.arcgis.com/home/item.html?id=10df2279f9684e4a9f6a7f08febac2a9) |
| Mountains | [AWS Terrain Tiles](https://registry.opendata.aws/terrain-tiles/) |
| Place names, borders, roads, buildings | [OpenStreetMap](https://www.openstreetmap.org/copyright), served by [OpenFreeMap](https://openfreemap.org) |
| Real 3D cities | [Google Photorealistic 3D Tiles](https://developers.google.com/maps/documentation/tile/3d-tiles), with your own key |
| Search and name suggestions | [OpenStreetMap Nominatim](https://nominatim.org) — asked only when you press search or drop a pin |
| The 3D map itself | [MapLibre GL JS](https://maplibre.org) 5.24.0 |

## Files

| | |
|---|---|
| `index.html` | The page. |
| `css/earth.css` | The look. |
| `js/app.js` | The map, search, places, measuring and layers. |
| `js/geo.js` | The maths (distances, areas, curved lines), number formatting and the saved-places file. No browser needed. |
| `tests/earth-logic.test.js` | Tests for `geo.js`: `node earth/tests/earth-logic.test.js` |
