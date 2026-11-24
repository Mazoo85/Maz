// Terra Desk core: the Cesium globe, terrain, imagery, the layer list, picking, search, places, measuring and
// settings. Each layer lives in js/layers/<id>.js and gets the shared context T.
// Built on Earth Explorer's keyless map sources (Esri photos, AWS terrain, Nominatim search) and its geo.js maths.
import ships from './layers/ships.js';
import aircraft from './layers/aircraft.js';
import launches from './layers/launches.js';
import aurora from './layers/aurora.js';
import volcanoes from './layers/volcanoes.js';
import lightning from './layers/lightning.js';
import battles from './layers/battles.js';
import oldmaps from './layers/oldmaps.js';
import worlds from './layers/worlds.js';
import ocean from './layers/ocean.js';
import roadtrip from './layers/roadtrip.js';
import trails from './layers/trails.js';
import builder from './layers/builder.js';

const C = window.Cesium, G = window.EarthGeo;
const $ = id => document.getElementById(id);
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

const LAYERS = [ships, aircraft, launches, aurora, volcanoes, lightning, battles, oldmaps, worlds, ocean, roadtrip, trails, builder];
const GROUPS = ['Live now', 'History and worlds', 'Tools'];

let settings = {}, toastT = 0;
function toast(msg, bad) { const t = $('toast'); t.textContent = msg; t.className = 'toast' + (bad ? ' bad' : ''); t.hidden = false; clearTimeout(toastT); toastT = setTimeout(() => { t.hidden = true; }, bad ? 8000 : 4500); }

// ---------------------------------------------------------------- terrain: AWS Terrain Tiles (Terrarium PNG)
// Heights decode as (R*256 + G + B/256) - 32768 metres. Tiles go to zoom 15; deeper levels sample their ancestor.
// Ocean depths are in the same tiles (ETOPO1 / GEBCO); they are shown only when the Ocean Deep layer is on.
const tileCache = new Map();
function terrariumTile(z, x, y) {
  const k = `${z}/${x}/${y}`;
  if (tileCache.has(k)) return tileCache.get(k);
  const p = fetch(`https://s3.amazonaws.com/elevation-tiles-prod/terrarium/${k}.png`).then(r => { if (!r.ok) throw new Error('tile'); return r.blob(); })
    .then(b => createImageBitmap(b)).then(bmp => { const c = new OffscreenCanvas(256, 256), g = c.getContext('2d'); g.drawImage(bmp, 0, 0); return g.getImageData(0, 0, 256, 256).data; });
  tileCache.set(k, p);
  if (tileCache.size > 600) tileCache.delete(tileCache.keys().next().value);
  p.catch(() => tileCache.delete(k));
  return p;
}
const terrarium = (d, i) => (d[i] * 256 + d[i + 1] + d[i + 2] / 256) - 32768;
// The globe uses Cesium's geographic tiles (which reach the poles); each sample is read from the Web Mercator
// Terrarium tile that holds it. Above 85.05° (where Web Mercator stops) the nearest row is used.
const mercY = lat => { const r = Math.max(-85.0511, Math.min(85.0511, lat)) * Math.PI / 180; return (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2; };
function makeTerrain(withOcean) {
  const N = 65, geo = new C.GeographicTilingScheme();
  return new C.CustomHeightmapTerrainProvider({
    width: N, height: N, tilingScheme: geo, credit: 'Terrain: AWS Terrain Tiles (USGS, ETOPO1, GEBCO and others)',
    callback: async (x, y, level) => {
      const r = geo.tileXYToRectangle(x, y, level), z = Math.max(0, Math.min(14, level + 1)), k = 1 << z;
      const w = C.Math.toDegrees(r.west), e = C.Math.toDegrees(r.east), s = C.Math.toDegrees(r.south), n = C.Math.toDegrees(r.north);
      const px = lon => Math.min(k * 256 - 1, Math.max(0, ((lon + 180) / 360) * k * 256)), py = lat => Math.min(k * 256 - 1, Math.max(0, mercY(lat) * k * 256));
      // fetch every Terrarium tile this geographic tile touches (usually 1 to 4)
      const tx0 = Math.floor(px(w) / 256), tx1 = Math.floor(px(e) / 256), ty0 = Math.floor(py(n) / 256), ty1 = Math.floor(py(s) / 256);
      const tiles = new Map();
      try { await Promise.all([...Array((tx1 - tx0 + 1) * (ty1 - ty0 + 1)).keys()].map(async i => { const tx = tx0 + i % (tx1 - tx0 + 1), ty = ty0 + Math.floor(i / (tx1 - tx0 + 1)); tiles.set(tx + '/' + ty, await terrariumTile(z, tx, ty)); })); }
      catch { return new Float32Array(N * N); }
      const out = new Float32Array(N * N);
      for (let j = 0; j < N; j++) {
        const lat = n - (n - s) * j / (N - 1), Y = Math.floor(py(lat)), ty = Math.floor(Y / 256);
        for (let i = 0; i < N; i++) {
          const lon = w + (e - w) * i / (N - 1), X = Math.floor(px(lon)), tx = Math.floor(X / 256), d = tiles.get(tx + '/' + ty);
          const h = d ? terrarium(d, ((Y - ty * 256) * 256 + (X - tx * 256)) * 4) : 0;
          out[j * N + i] = withOcean ? h : Math.max(0, h);
        }
      }
      return out;
    },
  });
}
// Height at a point straight from the tiles (zoom 12), used for the cursor read-out and profiles.
async function heightAt(lon, lat, z = 12) {
  const n = 1 << z, xf = (lon + 180) / 360 * n, lr = lat * Math.PI / 180;
  const yf = (1 - Math.log(Math.tan(lr) + 1 / Math.cos(lr)) / Math.PI) / 2 * n;
  const x = Math.floor(xf), y = Math.floor(yf);
  try { const d = await terrariumTile(z, x, y); return terrarium(d, (Math.floor((yf - y) * 256) * 256 + Math.floor((xf - x) * 256)) * 4); } catch { return null; }
}

// ---------------------------------------------------------------- viewer
C.Ion.defaultAccessToken = '';                       // no Cesium ion: every source is free and keyless
const viewer = new C.Viewer('globe', {
  baseLayer: false, baseLayerPicker: false, geocoder: false, homeButton: false, sceneModePicker: false, navigationHelpButton: false,
  animation: false, timeline: false, fullscreenButton: false, infoBox: false, selectionIndicator: true, creditContainer: document.createElement('div'),
  terrainProvider: makeTerrain(false),
});
viewer.scene.globe.depthTestAgainstTerrain = false;
viewer.scene.fog.enabled = true;
viewer.scene.skyAtmosphere.show = true;
viewer.scene.globe.showGroundAtmosphere = true;
let world = 'earth', dayLayer, nightLayer, photoLayer, labelLayer;

async function setImagery() {
  const L = viewer.imageryLayers;
  L.removeAll();
  if (world !== 'earth') return;
  // The Earth map comes with the app (2K, Solar System Scope); if it cannot load, Cesium's own small
  // Natural Earth map (part of the globe library) is the base.
  let day;
  try { day = await C.SingleTileImageryProvider.fromUrl('assets/base/day.jpg', { credit: 'Earth: Solar System Scope (CC BY 4.0)' }); }
  catch { day = await C.TileMapServiceImageryProvider.fromUrl(C.buildModuleUrl('Assets/Textures/NaturalEarthII'), { credit: 'Earth: Natural Earth II' }); }
  if (world !== 'earth') return;
  L.removeAll();
  dayLayer = L.addImageryProvider(day);
  if (settings.imagery === 'esri') photoLayer = L.addImageryProvider(new C.UrlTemplateImageryProvider({ url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', maximumLevel: 19, credit: 'Imagery: Esri, Maxar, Earthstar Geographics' }));
  nightLayer = null;
  if (settings.lighting) {   // the night map is only loaded when day and night is on, to save memory
    try {
      nightLayer = L.addImageryProvider(await C.SingleTileImageryProvider.fromUrl('assets/base/night.jpg', { credit: 'Night lights: Solar System Scope (CC BY 4.0)' }));
      nightLayer.dayAlpha = 0; nightLayer.nightAlpha = 1;
    } catch { toast('The city-lights map could not be downloaded yet. It needs the internet the first time.', true); }
  }
  if (settings.labels) labelLayer = L.addImageryProvider(new C.UrlTemplateImageryProvider({ url: 'https://server.arcgisonline.com/ArcGIS/rest/services/Reference/World_Boundaries_and_Places/MapServer/tile/{z}/{y}/{x}', maximumLevel: 19, credit: 'Labels: Esri' }));
  viewer.scene.globe.enableLighting = !!settings.lighting;
  // With day and night on, the dark side is shown from space; zoomed in below about 4,000 km it stays lit,
  // so places on the night side can still be seen.
  viewer.scene.globe.lightingFadeOutDistance = 4.0e6; viewer.scene.globe.lightingFadeInDistance = 8.0e6;
  viewer.scene.globe.nightFadeOutDistance = 4.0e6; viewer.scene.globe.nightFadeInDistance = 8.0e6;
  for (const l of LAYERS) if (l.on && l.onImagery) l.onImagery(T);
  showCredits();
}
function setTerrain() {
  if (world !== 'earth') return;
  viewer.terrainProvider = settings.terrain ? makeTerrain(!!T.oceanFloor) : new C.EllipsoidTerrainProvider();
  viewer.scene.verticalExaggeration = settings.exaggeration || 1;
}
function showCredits() {
  const c = [...new Set(viewer.imageryLayers._layers.map(l => l.imageryProvider && l.imageryProvider.credit && l.imageryProvider.credit.html).filter(Boolean).concat(settings.terrain && world === 'earth' ? ['Terrain: AWS Terrain Tiles'] : []).concat(LAYERS.filter(l => l.on && l.credit).map(l => l.credit)))];
  $('credits').innerHTML = c.map(x => x.replace(/<[^>]+>/g, '')).map(esc).join(' · ');
  $('credits').title = $('credits').textContent;
}

// ---------------------------------------------------------------- shared context for layers
const T = {
  C, G, viewer, td: window.td, esc, toast, heightAt, tile: terrariumTile, decode: terrarium, settings: () => settings, oceanFloor: false,
  // JSON through the main-process bridge (allow-listed services, polite gaps, caching)
  async json(url, init) { const r = await window.td.fetch(url, init); if (r.error) throw new Error(r.error); if (!r.ok) throw new Error(`${new URL(url).host} answered HTTP ${r.status}`); return JSON.parse(r.body); },
  async text(url, init) { const r = await window.td.fetch(url, init); if (r.error) throw new Error(r.error); if (!r.ok) throw new Error(`${new URL(url).host} answered HTTP ${r.status}`); return r.body; },
  status(layer, text, kind) { layer.statusText = text; layer.statusKind = kind || ''; renderLayers(); },
  panel(layer) { openPanel(layer); },
  refreshPanel(layer) { if (panelLayer === layer) openPanel(layer); },
  detail(title, html, onReady) { $('detailTitle').textContent = title; $('detailBody').innerHTML = html; $('detail').hidden = false; document.body.classList.remove('menu'); if (onReady) onReady($('detailBody')); },
  closeDetail() { $('detail').hidden = true; },
  view() { // current view as [west, south, east, north] degrees and centre
    const r = viewer.camera.computeViewRectangle(viewer.scene.globe.ellipsoid);
    const c = viewer.camera.positionCartographic;
    const box = r ? [r.west, r.south, r.east, r.north].map(C.Math.toDegrees) : [-180, -90, 180, 90];
    return { box, lon: C.Math.toDegrees(c.longitude), lat: C.Math.toDegrees(c.latitude), height: c.height };
  },
  flyTo(lon, lat, height = 1500000, opts = {}) { viewer.camera.flyTo({ destination: C.Cartesian3.fromDegrees(lon, lat, height), duration: opts.duration ?? 2, orientation: opts.orientation }); },
  async setWorld(id, texture, radii) { world = id; $('world').textContent = id === 'earth' ? 'Earth' : id[0].toUpperCase() + id.slice(1).replace(/_.*/, ''); await rebuildGlobe(texture, radii); },
  world: () => world,
  setOceanFloor(on) { T.oceanFloor = on; setTerrain(); },
  save(patch) { Object.assign(settings, patch); window.td.setSettings(patch); },
  credits: showCredits,
};
window.terra = { viewer, layers: LAYERS, T };

// Other worlds: a new globe on that body's ellipsoid with its texture; terrain off.
async function rebuildGlobe(texture, radii) {
  const ell = world === 'earth' ? C.Ellipsoid.WGS84 : new C.Ellipsoid(radii[0], radii[0], radii[1] || radii[0]);
  viewer.scene.globe = new C.Globe(ell);
  viewer.scene.globe.showGroundAtmosphere = world === 'earth';
  viewer.scene.skyAtmosphere.show = world === 'earth';
  viewer.scene.fog.enabled = world === 'earth';
  if (world === 'earth') { setTerrain(); await setImagery(); T.flyTo(10, 25, 18000000, { duration: 0 }); }
  else {
    viewer.terrainProvider = new C.EllipsoidTerrainProvider({ ellipsoid: ell });
    viewer.imageryLayers.removeAll();
    viewer.imageryLayers.addImageryProvider(await C.SingleTileImageryProvider.fromUrl(texture, { credit: 'Texture: Solar System Scope (CC BY 4.0)', tilingScheme: new C.GeographicTilingScheme({ ellipsoid: ell }) }));
    viewer.scene.globe.enableLighting = false;
    viewer.camera.flyTo({ destination: C.Cartesian3.fromRadians(0, 0, ell.maximumRadius * 3.2, ell), duration: 0 });
  }
  showCredits();
}

// ---------------------------------------------------------------- layer list
function renderLayers() {
  $('layers').innerHTML = GROUPS.map(g => `<div class="grp">${g}</div>` + LAYERS.filter(l => l.group === g).map(l => `
    <div class="lyr${l.on ? ' on' : ''}" data-id="${l.id}" title="${esc(l.info || '')}">
      <input type="checkbox" ${l.on ? 'checked' : ''} ${l.tool ? 'hidden' : ''}><span class="nm">${esc(l.name)}</span><span class="st ${l.statusKind || ''}">${esc(l.statusText || '')}</span>
      <span class="sub">${esc(l.sub || '')}</span></div>`).join('')).join('');
}
$('layers').addEventListener('click', async e => {
  const row = e.target.closest('.lyr'); if (!row) return;
  const l = LAYERS.find(x => x.id === row.dataset.id);
  if (l.tool || e.target.type !== 'checkbox') { if (!l.on) await toggle(l, true); openPanel(l); return; }
  await toggle(l, e.target.checked);
});
async function toggle(l, on) {
  if (on === !!l.on) return;
  if (l.worldOnly && world !== 'earth' && on && l.id !== 'worlds') { toast(`${l.name} works on Earth. Switch back with Other Worlds → Earth.`); renderLayers(); return; }
  l.on = on;
  try { if (on) await l.enable(T); else { await l.disable(T); l.statusText = ''; } }
  catch (err) { l.on = false; T.status(l, 'error', 'err'); toast(`${l.name}: ${err.message}`, true); }
  T.save({ layers: LAYERS.filter(x => x.on && !x.tool).map(x => x.id) });
  if (!on && panelLayer === l) closePanel();
  renderLayers(); showCredits();
}
let panelLayer = null;
function openPanel(l) {
  panelLayer = l;
  $('panelTitle').textContent = l.name;
  $('panelBody').innerHTML = l.panel ? l.panel(T) : `<p class="note">${esc(l.info || '')}</p>`;
  $('panel').hidden = false;
  document.body.classList.remove('menu'); document.body.classList.add('sheet');
  if (l.wire) l.wire(T, $('panelBody'));
}
function closePanel() { $('panel').hidden = true; document.body.classList.remove('sheet'); if (panelLayer && panelLayer.tool && panelLayer.on) { panelLayer.closed && panelLayer.closed(T); } panelLayer = null; }
$('panelClose').onclick = closePanel;
$('detailClose').onclick = () => { $('detail').hidden = true; };

// ---------------------------------------------------------------- picking and the cursor read-out
const handler = new C.ScreenSpaceEventHandler(viewer.scene.canvas);
let clickMode = null;   // a tool can capture clicks on the globe: fn(lon, lat)
T.capture = fn => { clickMode = fn; viewer.scene.canvas.style.cursor = fn ? 'crosshair' : ''; };
function lonLatAt(pos) {
  const ray = viewer.camera.getPickRay(pos); const p = ray && viewer.scene.globe.pick(ray, viewer.scene) || viewer.camera.pickEllipsoid(pos, viewer.scene.globe.ellipsoid);
  if (!p) return null; const c = C.Cartographic.fromCartesian(p, viewer.scene.globe.ellipsoid);
  return { lon: C.Math.toDegrees(c.longitude), lat: C.Math.toDegrees(c.latitude), height: c.height };
}
async function readout(pos) {
  const ll = lonLatAt(pos); if (!ll) { $('cursor').textContent = '—'; return; }
  let txt = G.formatCoord(ll.lat, ll.lon);
  if (world === 'earth') { const h = await heightAt(ll.lon, ll.lat, 10); if (h != null) txt += ` · ${h < 0 ? 'depth ' + G.formatElevation(-h, settings.units) : 'elevation ' + G.formatElevation(h, settings.units)}`; }
  $('cursor').textContent = txt;
}
handler.setInputAction(ev => {
  if (clickMode) { const ll = lonLatAt(ev.position); if (ll) clickMode(ll.lon, ll.lat); return; }
  const picked = viewer.scene.pick(ev.position);
  const tag = picked && (picked.id && picked.id.td || picked.primitive && picked.primitive.td || picked.td);
  if (tag) { const l = LAYERS.find(x => x.id === tag.layer); if (l && l.onPick) l.onPick(T, tag.data, picked); return; }
  if (measuring) { const ll = lonLatAt(ev.position); if (ll) { measurePts.push([ll.lon, ll.lat]); drawMeasure(); } }
  readout(ev.position);    // phones have no pointer to hover: a tap shows the place, height or depth
}, C.ScreenSpaceEventType.LEFT_CLICK);
let moveT = 0;
handler.setInputAction(ev => {
  clearTimeout(moveT);
  moveT = setTimeout(() => { if (matchMedia('(hover: hover)').matches) readout(ev.endPosition); }, 120);
}, C.ScreenSpaceEventType.MOUSE_MOVE);

// ---------------------------------------------------------------- phone menu
$('menuBtn').onclick = () => document.body.classList.add('menu');
$('menuClose').onclick = $('scrim').onclick = () => document.body.classList.remove('menu');

// ---------------------------------------------------------------- search (Nominatim: only on Enter, cached, 1/second)
$('search').addEventListener('submit', async e => {
  e.preventDefault();
  const q = $('q').value.trim(); if (!q) return;
  const coord = G.parseCoordinates(q);
  if (coord) { T.flyTo(coord.lng, coord.lat, 20000); document.body.classList.remove('menu'); return; }
  try {
    const list = await T.json(`https://nominatim.openstreetmap.org/search?format=jsonv2&limit=6&q=${encodeURIComponent(q)}`);
    $('results').innerHTML = list.map((r, i) => `<li data-i="${i}">${esc(r.display_name)}</li>`).join('') || '<li class="muted">Nothing found.</li>';
    $('results').hidden = false;
    $('results').onclick = ev => { const li = ev.target.closest('li[data-i]'); if (!li) return; const r = list[+li.dataset.i]; $('results').hidden = true; document.body.classList.remove('menu'); const bb = r.boundingbox && r.boundingbox.map(Number);
      if (bb) viewer.camera.flyTo({ destination: C.Rectangle.fromDegrees(bb[2], bb[0], bb[3], bb[1]), duration: 2 }); else T.flyTo(+r.lon, +r.lat, 20000); };
  } catch (err) { toast('Search: ' + err.message, true); }
});

// ---------------------------------------------------------------- places and measuring (Earth Explorer's tools)
const placeEnts = [];
function drawPlaces() {
  for (const e of placeEnts.splice(0)) viewer.entities.remove(e);
  for (const p of settings.places || []) placeEnts.push(viewer.entities.add({ position: C.Cartesian3.fromDegrees(p.lng, p.lat), point: { pixelSize: 9, color: C.Color.fromCssColorString('#ffbf47'), outlineColor: C.Color.BLACK, outlineWidth: 1, heightReference: C.HeightReference.CLAMP_TO_GROUND, disableDepthTestDistance: Number.POSITIVE_INFINITY }, label: { text: p.name, font: '13px Segoe UI', pixelOffset: new C.Cartesian2(0, -16), fillColor: C.Color.WHITE, showBackground: true, backgroundColor: C.Color.fromCssColorString('rgba(5,8,13,.75)'), disableDepthTestDistance: Number.POSITIVE_INFINITY, distanceDisplayCondition: new C.DistanceDisplayCondition(0, 8e6) } }));
}
const placesTool = { id: 'places', name: 'Places', tool: true, on: true,
  panel: () => `<p class="note">Saved places. Press Add, then tap the globe.</p><div class="row"><input id="plName" placeholder="Name" class="grow"><button id="plAdd" class="primary">Add</button></div>
    <div class="list">${(settings.places || []).map((p, i) => `<div class="it" data-i="${i}"><span>${esc(p.name)}</span><button class="small" data-del="${i}">Remove</button><span class="s">${esc(G.formatCoord(p.lat, p.lng))}${p.note ? ' · ' + esc(p.note) : ''}</span></div>`).join('') || '<div class="it muted">No places yet.</div>'}</div>`,
  wire: (T, el) => {
    el.querySelector('#plAdd').onclick = () => { toast('Tap the globe where the place is.'); T.capture((lon, lat) => { T.capture(null); const name = el.querySelector('#plName').value.trim() || G.formatCoord(lat, lon); settings.places = (settings.places || []).concat([G.normalizePlace({ name, lat, lng: lon })]); T.save({ places: settings.places }); drawPlaces(); openPanel(placesTool); }); };
    el.querySelector('.list').onclick = ev => { const d = ev.target.closest('[data-del]'); if (d) { settings.places.splice(+d.dataset.del, 1); T.save({ places: settings.places }); drawPlaces(); openPanel(placesTool); return; } const it = ev.target.closest('.it[data-i]'); if (it) { const p = settings.places[+it.dataset.i]; T.flyTo(p.lng, p.lat, 15000); } };
  } };
let measuring = false, measurePts = [], measureEnt = null;
function drawMeasure() {
  if (measureEnt) viewer.entities.remove(measureEnt);
  measureEnt = measurePts.length > 1 ? viewer.entities.add({ polyline: { positions: C.Cartesian3.fromDegreesArray(G.densify(measurePts, 20000).flat()), width: 3, material: C.Color.fromCssColorString('#8fd0ff'), clampToGround: true } }) : null;
  T.detail('Measure', `<div class="big">${esc(G.formatDistance(G.pathLength(measurePts), settings.units))}</div><p class="note">${measurePts.length} point${measurePts.length === 1 ? '' : 's'} along the curve of the Earth. Tap the globe to add points.</p><div class="row"><button id="mClear">Clear</button><button id="mDone">Done</button></div>`, el => {
    el.querySelector('#mClear').onclick = () => { measurePts = []; drawMeasure(); };
    el.querySelector('#mDone').onclick = () => { measuring = false; $('tMeasure').classList.remove('on'); measurePts = []; drawMeasure(); T.closeDetail(); };
  });
}
$('tPlaces').onclick = () => openPanel(placesTool);
$('tMeasure').onclick = () => { document.body.classList.remove('menu'); measuring = !measuring; $('tMeasure').classList.toggle('on', measuring); measurePts = []; if (measuring) drawMeasure(); else { drawMeasure(); T.closeDetail(); } };
$('tHome').onclick = () => { document.body.classList.remove('menu'); if (world !== 'earth') { const w = LAYERS.find(l => l.id === 'worlds'); w.goEarth(T); } else T.flyTo(10, 25, 18000000); };

// ---------------------------------------------------------------- settings
const settingsTool = { id: 'settings', name: 'Settings', tool: true, on: true,
  panel: () => `
    <label class="f">Satellite photos <select id="sImg"><option value="esri">Esri World Imagery (online)</option><option value="offline">Offline Earth map only</option></select></label>
    <label class="row"><input type="checkbox" id="sLab"> Place names and borders</label>
    <label class="row"><input type="checkbox" id="sLight"> Day and night (sun lighting, city lights on the dark side)</label>
    <label class="row"><input type="checkbox" id="sTer"> 3D terrain</label>
    <label class="f">Terrain height <input type="range" id="sEx" min="1" max="4" step="0.1"></label>
    <label class="f">Units <select id="sUnits"><option value="metric">Metric</option><option value="imperial">Miles and feet</option></select></label>
    <hr style="border:0;border-top:1px solid var(--line);width:100%">
    <b>Keys (free; you create them)</b>
    <label class="f">aisstream.io API key (live ships worldwide) <input id="kAis" type="password" placeholder="${settings.hasAis ? 'saved' : 'paste your key'}"></label>
    <p class="note">Get one free at aisstream.io (sign in, then API Keys). Meridian uses the same kind of key.</p>
    <label class="f">OpenSky client ID (optional, more aircraft detail) <input id="kOsId" value="${esc(settings.openskyId || '')}"></label>
    <label class="f">OpenSky client secret <input id="kOsSec" type="password" placeholder="${settings.hasOpensky ? 'saved' : ''}"></label>
    <label class="f">adsb.lol API key (optional, raises the daily limit) <input id="kAdsb" type="password" placeholder="${settings.adsbKey ? 'saved' : ''}"></label>
    <div class="row"><button id="sSave" class="primary">Save keys</button></div>`,
  wire: (T, el) => {
    const s = settings;
    el.querySelector('#sImg').value = s.imagery; el.querySelector('#sLab').checked = !!s.labels; el.querySelector('#sLight').checked = !!s.lighting; el.querySelector('#sTer').checked = !!s.terrain;
    el.querySelector('#sEx').value = s.exaggeration; el.querySelector('#sUnits').value = s.units;
    const re = async () => { T.save({ imagery: el.querySelector('#sImg').value, labels: el.querySelector('#sLab').checked, lighting: el.querySelector('#sLight').checked, terrain: el.querySelector('#sTer').checked, exaggeration: +el.querySelector('#sEx').value, units: el.querySelector('#sUnits').value }); setTerrain(); await setImagery(); };
    for (const id of ['sImg', 'sLab', 'sLight', 'sTer', 'sUnits']) el.querySelector('#' + id).onchange = re;
    el.querySelector('#sEx').oninput = () => { viewer.scene.verticalExaggeration = +el.querySelector('#sEx').value; }; el.querySelector('#sEx').onchange = re;
    el.querySelector('#sSave').onclick = async () => {
      const p = {}; const ais = el.querySelector('#kAis').value.trim(), id = el.querySelector('#kOsId').value.trim(), sec = el.querySelector('#kOsSec').value.trim(), ad = el.querySelector('#kAdsb').value.trim();
      if (ais) p.aisKey = ais; p.openskyId = id; if (sec) p.openskySecret = sec; if (ad) p.adsbKey = ad;
      const r = await window.td.setSettings(p); settings.hasAis = r.hasAis; settings.hasOpensky = r.hasOpensky; if (ad) settings.adsbKey = 'saved'; toast('Keys saved on this phone.'); openPanel(settingsTool);
    };
  } };
$('tSettings').onclick = () => openPanel(settingsTool);

// ---------------------------------------------------------------- test hook (npm run selftest)
window.terra.test = async (id, off) => {
  const l = LAYERS.find(x => x.id === id);
  if (off) { await toggle(l, false); return true; }
  await toggle(l, true);
  if (l.test) return await l.test(T);
  return { on: l.on, status: l.statusText || '' };
};

// ---------------------------------------------------------------- links: terradesk://...
// terradesk://aircraft?type=F-16C%20Fighting%20Falcon[&mil=0]   live aircraft of one type (Global Force Registry)
// terradesk://fly?lat=..&lon=..[&height=metres]                  fly to a place
// terradesk://battles?from=1939&to=1945[&q=Stalingrad]           Battle Atlas for a span or a search
// terradesk://layer?id=aurora                                    switch a layer on
async function openLink(href) {
  let u; try { u = new URL(href); } catch { return { error: 'bad link' }; }
  const what = (u.hostname || u.pathname.replace(/^\/+/, '')).toLowerCase(), q = k => u.searchParams.get(k), L = id => LAYERS.find(x => x.id === id);
  if (what === 'aircraft') { const l = L('aircraft'); await toggle(l, true); const r = await l.showType(T, q('type'), q('mil') !== '0'); openPanel(l); toast(q('type') ? `Showing live ${q('type')} aircraft` : 'Showing live aircraft'); return r; }
  if (what === 'fly') { const lat = +q('lat'), lon = +q('lon'); if (!isFinite(lat) || !isFinite(lon)) return { error: 'lat and lon needed' }; viewer.camera.flyTo({ destination: C.Cartesian3.fromDegrees(lon, lat, +q('height') || 20000) }); return { lat, lon }; }
  if (what === 'battles') { const l = L('battles'); await toggle(l, true); const r = l.setRange(T, q('from'), q('to'), q('q')); openPanel(l); return r; }
  if (what === 'layer') { const l = L(q('id')); if (!l) return { error: 'no layer ' + q('id') }; await toggle(l, true); openPanel(l); return { on: l.on }; }
  return { error: 'unknown link ' + what };
}
window.td.on('link', href => openLink(href).catch(e => toast(e.message, true)));
window.terra.openLink = openLink;

// ---------------------------------------------------------------- start
(async () => {
  const d = await window.td.init();
  settings = d.settings;
  await setImagery(); setTerrain();
  viewer.camera.setView({ destination: settings.camera ? C.Cartesian3.fromDegrees(settings.camera.lon, settings.camera.lat, settings.camera.height) : C.Cartesian3.fromDegrees(10, 25, 18000000) });
  viewer.camera.moveEnd.addEventListener(() => { const v = T.view(); if (world === 'earth') T.save({ camera: { lon: v.lon, lat: v.lat, height: v.height } }); for (const l of LAYERS) if (l.on && l.onView) l.onView(T, v); });
  drawPlaces();
  renderLayers();
  for (const id of settings.layers || []) { const l = LAYERS.find(x => x.id === id); if (l && !l.tool) await toggle(l, true); }
})();
