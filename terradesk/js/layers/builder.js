// Real Map Builder: turns a box on the real Earth into game-ready files.
//  heightmap.png   16-bit greyscale heightmap (Godot, Stratoscape, Unreal, Blender)
//  heightmap.r16   16-bit little-endian RAW (Unity terrain, Unreal landscape)
//  features.geojson roads, buildings (with heights where mapped) and water from OpenStreetMap
//  map.json        bounds, size, lowest and highest point, metres per pixel; README.txt says how to import
// Heights: AWS Terrain Tiles (USGS, ETOPO1, GEBCO…). Features: © OpenStreetMap contributors, ODbL, so a game that
// ships them must credit OpenStreetMap and share changes to that data under the ODbL.
let rect = null, rectEnt = null, size = 1025, withFeatures = true, preview = null, busy = false;
const SIZES = [257, 513, 1025, 2049, 4097];

// ---- PNG (16-bit greyscale) without libraries: zlib from CompressionStream, CRC32 by table
const CRC = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc32 = b => { let c = ~0; for (const v of b) c = CRC[(c ^ v) & 255] ^ (c >>> 8); return (~c) >>> 0; };
async function zlib(bytes) { const s = new Blob([bytes]).stream().pipeThrough(new CompressionStream('deflate')); return new Uint8Array(await new Response(s).arrayBuffer()); }
function chunk(type, data) { const out = new Uint8Array(12 + data.length), dv = new DataView(out.buffer); dv.setUint32(0, data.length); out.set([...type].map(c => c.charCodeAt(0)), 4); out.set(data, 8); dv.setUint32(8 + data.length, crc32(out.subarray(4, 8 + data.length))); return out; }
async function png16(w, h, u16) {
  const raw = new Uint8Array(h * (1 + w * 2));
  for (let y = 0; y < h; y++) { raw[y * (1 + w * 2)] = 0; for (let x = 0; x < w; x++) { const v = u16[y * w + x], o = y * (1 + w * 2) + 1 + x * 2; raw[o] = v >> 8; raw[o + 1] = v & 255; } }
  const ihdr = new Uint8Array(13), dv = new DataView(ihdr.buffer); dv.setUint32(0, w); dv.setUint32(4, h); ihdr[8] = 16; ihdr[9] = 0;
  const parts = [new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', await zlib(raw)), chunk('IEND', new Uint8Array(0))];
  const out = new Uint8Array(parts.reduce((a, p) => a + p.length, 0)); let o = 0; for (const p of parts) { out.set(p, o); o += p.length; } return out;
}
const b64 = u8 => { let s = ''; for (let i = 0; i < u8.length; i += 32768) s += String.fromCharCode.apply(null, u8.subarray(i, i + 32768)); return btoa(s); };

// ---- heights for the box from Terrarium tiles, bilinear, sized N x N
const mercY = lat => { const r = lat * Math.PI / 180; return (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2; };
async function heights(T, [w, s, e, n], N, progress) {
  const midLat = (s + n) / 2, widthM = T.G.distance([w, midLat], [e, midLat]);
  let z = Math.ceil(Math.log2(156543.03 * Math.cos(midLat * Math.PI / 180) / (widthM / N)));
  z = Math.max(1, Math.min(14, z));
  const tilesFor = z => { const k = 1 << z; return [Math.floor((w + 180) / 360 * k), Math.floor((e + 180) / 360 * k), Math.floor(mercY(n) * k), Math.floor(mercY(s) * k)]; };
  let [x0, x1, y0, y1] = tilesFor(z);
  while ((x1 - x0 + 1) * (y1 - y0 + 1) > 144 && z > 1) { z--; [x0, x1, y0, y1] = tilesFor(z); }
  const tiles = new Map(); let done = 0, total = (x1 - x0 + 1) * (y1 - y0 + 1);
  await Promise.all([...Array(total).keys()].map(async i => { const x = x0 + i % (x1 - x0 + 1), y = y0 + Math.floor(i / (x1 - x0 + 1)); tiles.set(`${x}/${y}`, await T.tile(z, x, y)); progress(++done / total); }));
  const k = 1 << z, out = new Float32Array(N * N);
  const at = (px, py) => { const tx = Math.floor(px / 256), ty = Math.floor(py / 256), d = tiles.get(`${tx}/${ty}`); if (!d) return 0; return T.decode(d, ((py - ty * 256) * 256 + (px - tx * 256)) * 4); };
  for (let j = 0; j < N; j++) {
    const lat = n - (n - s) * j / (N - 1), py = mercY(lat) * k * 256 - 0.5;
    for (let i = 0; i < N; i++) {
      const lon = w + (e - w) * i / (N - 1), px = (lon + 180) / 360 * k * 256 - 0.5;
      const ix = Math.max(x0 * 256, Math.min((x1 + 1) * 256 - 2, Math.floor(px))), iy = Math.max(y0 * 256, Math.min((y1 + 1) * 256 - 2, Math.floor(py))), fx = px - ix, fy = py - iy;
      out[j * N + i] = at(ix, iy) * (1 - fx) * (1 - fy) + at(ix + 1, iy) * fx * (1 - fy) + at(ix, iy + 1) * (1 - fx) * fy + at(ix + 1, iy + 1) * fx * fy;
    }
  }
  return { h: out, z, widthM, heightM: T.G.distance([w, s], [w, n]) };
}

// ---- OpenStreetMap features as GeoJSON
async function features(T, [w, s, e, n]) {
  const km2 = T.G.distance([w, s], [e, s]) * T.G.distance([w, s], [w, n]) / 1e6;
  const big = km2 > 80;
  const q = `[out:json][timeout:120];(
    way["highway"${big ? '~"motorway|trunk|primary|secondary|tertiary"' : ''}](${s},${w},${n},${e});
    ${big ? '' : `way["building"](${s},${w},${n},${e});`}
    way["natural"="water"](${s},${w},${n},${e}); way["waterway"~"river|stream|canal"](${s},${w},${n},${e}););out geom;`;
  const j = await T.json('https://overpass-api.de/api/interpreter', { method: 'POST', body: 'data=' + encodeURIComponent(q), headers: { 'content-type': 'application/x-www-form-urlencoded' } });
  const feats = [];
  for (const el of j.elements || []) {
    if (!el.geometry) continue;
    const c = el.geometry.map(g => [+g.lon.toFixed(7), +g.lat.toFixed(7)]);
    const closed = c.length > 3 && c[0][0] === c[c.length - 1][0] && c[0][1] === c[c.length - 1][1];
    const t = el.tags || {};
    const kind = t.building ? 'building' : t.highway ? 'road' : 'water';
    const props = { kind, osm_id: el.id, name: t.name || '', type: t.highway || t.building || t.waterway || t.natural || '' };
    if (kind === 'building') props.height_m = t.height ? parseFloat(t.height) : t['building:levels'] ? parseFloat(t['building:levels']) * 3 : null;
    if (kind === 'road') props.lanes = t.lanes ? +t.lanes : null;
    feats.push({ type: 'Feature', properties: props, geometry: closed && kind !== 'road' ? { type: 'Polygon', coordinates: [c] } : { type: 'LineString', coordinates: c } });
  }
  return { fc: { type: 'FeatureCollection', features: feats }, big, counts: feats.reduce((a, f) => (a[f.properties.kind] = (a[f.properties.kind] || 0) + 1, a), {}) };
}

function drawRect(T) {
  const { C, viewer } = T;
  if (rectEnt) viewer.entities.remove(rectEnt);
  rectEnt = rect ? viewer.entities.add({ rectangle: { coordinates: C.Rectangle.fromDegrees(...rect), material: C.Color.fromCssColorString('#ffbf47').withAlpha(0.15), outline: true, outlineColor: C.Color.fromCssColorString('#ffbf47'), height: 0 } }) : null;
}
function previewCanvas(el, h, N, lo, hi) {
  const c = el.querySelector('#mbPrev'); if (!c) return; const S = Math.min(N, 300); c.width = c.height = S; c.hidden = false;
  const g = c.getContext('2d'), img = g.createImageData(S, S);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) { const v = h[Math.floor(y * N / S) * N + Math.floor(x * N / S)], k = Math.round((v - lo) / (hi - lo || 1) * 255), i = (y * S + x) * 4; img.data[i] = img.data[i + 1] = img.data[i + 2] = k; img.data[i + 3] = 255; }
  g.putImageData(img, 0, 0);
}

async function build(T, L, el, save = true) {
  if (!rect) { T.toast('Pick an area first.'); return null; }
  busy = true; const say = t => { const o = el && el.querySelector('#mbOut'); if (o) o.textContent = t; };
  try {
    say('Reading heights…');
    const r = await heights(T, rect, size, f => say(`Reading heights… ${Math.round(f * 100)}%`));
    let lo = Infinity, hi = -Infinity; for (const v of r.h) { if (v < lo) lo = v; if (v > hi) hi = v; }
    const u16 = new Uint16Array(r.h.length); for (let i = 0; i < r.h.length; i++) u16[i] = Math.round((r.h[i] - lo) / (hi - lo || 1) * 65535);
    if (el) previewCanvas(el, r.h, size, lo, hi);
    let feat = null;
    if (withFeatures) { say('Getting roads, buildings and water from OpenStreetMap…'); try { feat = await features(T, rect); } catch (e) { feat = { error: e.message }; } }
    const meta = { name: 'Terra Desk map export', bounds: { west: rect[0], south: rect[1], east: rect[2], north: rect[3] }, size_px: size, width_m: Math.round(r.widthM), length_m: Math.round(r.heightM),
      metres_per_pixel: +(r.widthM / (size - 1)).toFixed(3), lowest_m: +lo.toFixed(2), highest_m: +hi.toFixed(2), height_range_m: +(hi - lo).toFixed(2), source_zoom: r.z,
      features: feat && !feat.error ? feat.counts : null, credits: ['Heights: AWS Terrain Tiles (USGS 3DEP/NED, ETOPO1, GEBCO, EU-DEM, and others)', feat ? 'Roads, buildings, water: © OpenStreetMap contributors, ODbL 1.0' : ''].filter(Boolean) };
    preview = meta;
    if (!save) { say(`${size} × ${size} px · ${meta.width_m.toLocaleString()} × ${meta.length_m.toLocaleString()} m · ${meta.lowest_m} to ${meta.highest_m} m`); return { meta, u16 }; }
    const raw = new Uint8Array(u16.length * 2); for (let i = 0; i < u16.length; i++) { raw[i * 2] = u16[i] & 255; raw[i * 2 + 1] = u16[i] >> 8; }
    const readme = `Terra Desk map export\n\nArea: ${rect.map(v => v.toFixed(5)).join(', ')} (west, south, east, north)\nSize: ${size} x ${size} pixels, ${meta.metres_per_pixel} m per pixel, ${meta.width_m} x ${meta.length_m} m\nHeights: ${meta.lowest_m} m (black) to ${meta.highest_m} m (white), range ${meta.height_range_m} m\n\n` +
      `Unity: Terrain > Import Raw > heightmap.r16, Depth 16 bit, Byte order Windows, Resolution ${size}; set Terrain Width/Length to ${meta.width_m} / ${meta.length_m} and Height to ${meta.height_range_m}.\n` +
      `Unreal: Landscape > Import from File > heightmap.png; Z scale = ${(meta.height_range_m / 512 * 100).toFixed(2)} for a 1 m range (Unreal maps 16-bit to 512 m at scale 100).\n` +
      `Godot (Terrain3D / HTerrain): import heightmap.png as a 16-bit heightmap with a height range of ${meta.height_range_m} m.\n` +
      `Stratoscape: Terrain Editor > Import > heightmap.png (16-bit) or heightmap.r16.\nBlender: Displace modifier with heightmap.png on a grid, strength ${meta.height_range_m} (in metres at scale 1).\n\n` +
      `features.geojson: roads, buildings (height_m where mapped) and water, in longitude/latitude (EPSG:4326).\n\nCredits you must keep in a game that uses this:\n${meta.credits.join('\n')}\n` +
      (feat ? 'The OpenStreetMap data is under the Open Database License (ODbL): credit "© OpenStreetMap contributors" and share any changes you make to that data under the ODbL.\n' : '');
    const files = [{ name: 'heightmap.png', data: b64(await png16(size, size, u16)), encoding: 'base64' }, { name: 'heightmap.r16', data: b64(raw), encoding: 'base64' }, { name: 'map.json', data: JSON.stringify(meta, null, 1) }, { name: 'README.txt', data: readme }];
    if (feat && !feat.error) files.push({ name: 'features.geojson', data: JSON.stringify(feat.fc) });
    say('Saving…');
    const res = await T.td.saveFolder({ name: `Map ${rect[1].toFixed(3)} ${rect[0].toFixed(3)} ${size}px`, files });
    if (res.dir) { say(`Saved to ${res.dir}`); T.toast('Map saved: ' + res.dir); }
    else say('Not saved.');
    return { meta, res, featureError: feat && feat.error };
  } finally { busy = false; }
}

export default {
  id: 'builder', name: 'Real Map Builder', group: 'Tools', tool: true, worldOnly: true, sub: 'Real terrain for your games', credit: 'Map builder: AWS Terrain Tiles, OpenStreetMap',
  info: 'Pick any area of the real Earth and save it as a 16-bit heightmap (PNG and RAW) with roads, buildings and water, ready for Unity, Unreal, Godot, Blender or Stratoscape.',
  async enable(T) { this.on = true; },
  async disable(T) { rect = null; drawRect(T); T.capture(null); },
  panel(T) {
    return `<p class="note">Pick an area, choose a size, then save. You get a heightmap, roads, buildings and water, and a README with import steps for Unity, Unreal, Godot, Blender and Stratoscape.</p>
      <div class="row"><button id="mbView">Use the current view</button><button id="mbDraw">Draw a box</button></div>
      ${rect ? `<div class="small muted">${rect.map(v => v.toFixed(4)).join(', ')}</div>` : ''}
      <label class="f">Heightmap size <select id="mbSize">${SIZES.map(s => `<option value="${s}" ${s === size ? 'selected' : ''}>${s} × ${s}</option>`).join('')}</select></label>
      <label class="row"><input type="checkbox" id="mbFeat" ${withFeatures ? 'checked' : ''}> Roads, buildings and water (OpenStreetMap)</label>
      <div class="row"><button id="mbPreview" ${rect ? '' : 'disabled'}>Preview</button><button id="mbSave" class="primary" ${rect ? '' : 'disabled'}>Save map</button></div>
      <canvas id="mbPrev" hidden style="width:100%;image-rendering:pixelated;border-radius:6px"></canvas><div id="mbOut" class="small muted"></div>
      <p class="note">OpenStreetMap data is ODbL: a game that ships it must credit "© OpenStreetMap contributors". Areas over 80 km² skip buildings and minor roads.</p>`;
  },
  wire(T, el) {
    const L = this;
    el.querySelector('#mbView').onclick = () => { const b = T.view().box; const mx = (b[0] + b[2]) / 2, my = (b[1] + b[3]) / 2, dx = Math.min((b[2] - b[0]) / 2, 0.6), dy = Math.min((b[3] - b[1]) / 2, 0.45); rect = [mx - dx * 0.8, my - dy * 0.8, mx + dx * 0.8, my + dy * 0.8]; drawRect(T); T.panel(L); };
    el.querySelector('#mbDraw').onclick = () => { const p = []; T.toast('Tap two opposite corners of the area.'); T.capture((lon, lat) => { p.push([lon, lat]); if (p.length < 2) return; T.capture(null); rect = [Math.min(p[0][0], p[1][0]), Math.min(p[0][1], p[1][1]), Math.max(p[0][0], p[1][0]), Math.max(p[0][1], p[1][1])]; drawRect(T); T.panel(L); }); };
    el.querySelector('#mbSize').onchange = e => { size = +e.target.value; };
    el.querySelector('#mbFeat').onchange = e => { withFeatures = e.target.checked; };
    el.querySelector('#mbPreview').onclick = () => !busy && build(T, L, el, false).catch(e => T.toast('Map: ' + e.message, true));
    el.querySelector('#mbSave').onclick = () => !busy && build(T, L, el, true).catch(e => T.toast('Map: ' + e.message, true));
  },
  async test(T) {
    rect = [-105.70, 40.24, -105.60, 40.32]; size = 513; withFeatures = true;   // Longs Peak area
    const r = await build(T, this, null, false);
    const png = await png16(4, 4, new Uint16Array(16).map((_, i) => i * 4000));
    return { ...r.meta, pngBytes: png.length, pngSignature: Array.from(png.slice(1, 4)).map(c => String.fromCharCode(c)).join('') };
  },
};
