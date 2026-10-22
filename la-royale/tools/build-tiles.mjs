#!/usr/bin/env node
/*
 * Builds the true-scale map from the raw downloads (tools/fetch.mjs):
 *
 *   node la-royale/tools/build-tiles.mjs
 *
 * Writes to la-royale/data/real/:
 *   manifest.json     the area, projection, tile grid and counts
 *   tiles/i_j.json    one 1 km square each: buildings, roads, areas, trees, shops
 *   skyline.json      the taller buildings of every tile, drawn plain from afar
 *   arteries.json     the named two-way boulevards and avenues, for traffic
 *   height.json       ground elevation every 10 m (Int16 decimetres, base64)
 *   overview.png      a top-down map of the whole area for the minimap and map
 */

import { readFileSync, readdirSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { REAL, projector, toBuilding, toRoad, areaKind, treeKind, toPoi, encodeTile, ringArea, RCLASS, AREA, BTYPE, FAR } from '../js/osm.js';
import { decodePNG, encodePNG } from './png.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
// (LA_RAW / LA_OUT point it elsewhere, e.g. at the made-up test fixture)
const RAW = process.env.LA_RAW || join(ROOT, 'data', 'raw');
const OUT = process.env.LA_OUT || join(ROOT, 'data', 'real');
const DEM = process.env.LA_DEM || RAW;
mkdirSync(join(OUT, 'tiles'), { recursive: true });

const P = projector();
const [X0, Z0] = P.toXZ(REAL.north, REAL.west);
const [X1, Z1] = P.toXZ(REAL.south, REAL.east);
const T = REAL.tile;
const NX = Math.ceil((X1 - X0) / T), NZ = Math.ceil((Z1 - Z0) / T);
const tileOf = (x, z) => [Math.floor((x - X0) / T), Math.floor((z - Z0) / T)];
const inside = (i, j) => i >= 0 && j >= 0 && i < NX && j < NZ;

/* ----------------------------------------------------------- read OSM */
const ways = new Map(), rels = new Map(), nodes = new Map();
const files = existsSync(RAW) ? readdirSync(RAW).filter((f) => f.startsWith('osm_')) : [];
if (!files.length) { console.error('No OSM downloads in data/raw — run tools/fetch.mjs osm first.'); process.exit(1); }
for (const f of files) {
  const j = JSON.parse(readFileSync(join(RAW, f), 'utf8'));
  for (const e of j.elements || []) {
    if (e.type === 'way' && e.geometry) ways.set(e.id, e);
    else if (e.type === 'relation') rels.set(e.id, e);
    else if (e.type === 'node' && e.tags) nodes.set(e.id, e);
  }
}
console.log(`${ways.size} ways, ${rels.size} relations, ${nodes.size} tagged nodes from ${files.length} cells`);

const xz = (g) => g.map((p) => P.toXZ(p.lat, p.lon));
const centroid = (pts) => { let x = 0, z = 0; for (const p of pts) { x += p[0]; z += p[1]; } return [x / pts.length, z / pts.length]; };
const bboxOf = (pts) => { let a = Infinity, b = Infinity, c = -Infinity, d = -Infinity; for (const [x, z] of pts) { a = Math.min(a, x); b = Math.min(b, z); c = Math.max(c, x); d = Math.max(d, z); } return [a, b, c, d]; };
function pip(x, z, pts) {
  let c = false;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
    const [xi, zi] = pts[i], [xj, zj] = pts[j];
    if ((zi > z) !== (zj > z) && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) c = !c;
  }
  return c;
}
/* Join a multipolygon's outer member ways into closed rings. */
function rings(members) {
  const segs = members.filter((m) => m.role !== 'inner' && m.geometry).map((m) => xz(m.geometry));
  const out = [];
  while (segs.length) {
    let ring = segs.shift();
    let grown = true;
    while (grown && (ring[0][0] !== ring[ring.length - 1][0] || ring[0][1] !== ring[ring.length - 1][1])) {
      grown = false;
      for (let k = 0; k < segs.length; k++) {
        const s = segs[k], end = ring[ring.length - 1];
        const same = (a, b) => Math.abs(a[0] - b[0]) < 0.01 && Math.abs(a[1] - b[1]) < 0.01;
        if (same(s[0], end)) ring = ring.concat(s.slice(1));
        else if (same(s[s.length - 1], end)) ring = ring.concat(s.slice(0, -1).reverse());
        else continue;
        segs.splice(k, 1); grown = true; break;
      }
    }
    if (ring.length > 3) out.push(ring);
  }
  return out;
}

const tiles = new Map();
const tile = (i, j) => {
  const k = `${i}_${j}`;
  if (!tiles.has(k)) tiles.set(k, { ox: X0 + i * T, oz: Z0 + j * T, buildings: [], roads: [], areas: [], trees: [], pois: [] });
  return tiles.get(k);
};
const counts = { buildings: 0, parts: 0, roads: 0, areas: 0, trees: 0, pois: 0, heightTagged: 0 };

/* buildings: where a building has mapped parts (towers, setbacks), draw the
 * parts and skip the outline */
const partWays = [...ways.values()].filter((w) => w.tags && w.tags['building:part'] && w.tags['building:part'] !== 'no');
const partCentres = partWays.map((w) => centroid(xz(w.geometry)));
const seen = new Map();          // the same footprint twice (two sources): keep the taller
const addBuilding = (tags, ring) => {
  const b = toBuilding(tags, ring);
  if (!b) return;
  const [cx, cz] = centroid(b.pts);
  const [i, j] = tileOf(cx, cz);
  if (!inside(i, j)) return;
  const key = `${Math.round(cx * 2)},${Math.round(cz * 2)},${Math.round(Math.abs(ringArea(b.pts)) / 5)},${b.mh > 1 ? 1 : 0}`;
  const prev = seen.get(key);
  if (prev) { if (b.h > prev.h) Object.assign(prev, { h: b.h }); return; }
  seen.set(key, b);
  tile(i, j).buildings.push(b);
  counts.buildings++;
  if (tags.height) counts.heightTagged++;
};
for (const w of ways.values()) {
  const t = w.tags || {};
  if (t.building && t.building !== 'no') {
    const ring = xz(w.geometry);
    const [a, b, c, d] = bboxOf(ring);
    const hasParts = partCentres.some(([x, z]) => x >= a && x <= c && z >= b && z <= d && pip(x, z, ring));
    if (!hasParts) addBuilding(t, ring);
  } else if (t['building:part'] && t['building:part'] !== 'no') {
    addBuilding({ ...t, building: t.building || t['building:part'] }, xz(w.geometry)); counts.parts++;
  }
}
for (const r of rels.values()) {
  const t = r.tags || {};
  if (t.building) for (const ring of rings(r.members || [])) addBuilding(t, ring);
}

/* roads: split each way into pieces by tile */
for (const w of ways.values()) {
  const t = w.tags || {};
  if (!t.highway && !t.railway) continue;
  const road = toRoad(t, xz(w.geometry));
  if (!road) continue;
  counts.roads++;
  let cur = [road.pts[0]], curTile = null;
  for (let k = 1; k < road.pts.length; k++) {
    const a = road.pts[k - 1], b = road.pts[k];
    const [i, j] = tileOf((a[0] + b[0]) / 2, (a[1] + b[1]) / 2);
    const key = `${i}_${j}`;
    if (curTile !== null && key !== curTile) {
      const [ci, cj] = curTile.split('_').map(Number);
      if (inside(ci, cj) && cur.length > 1) tile(ci, cj).roads.push({ ...road, pts: cur });
      cur = [a];
    }
    curTile = key;
    cur.push(b);
  }
  if (curTile) { const [ci, cj] = curTile.split('_').map(Number); if (inside(ci, cj) && cur.length > 1) tile(ci, cj).roads.push({ ...road, pts: cur }); }
}

/* areas: parks, water, lots — given to every tile they overlap */
const addArea = (kind, ring) => {
  if (ring.length < 4 || Math.abs(ringArea(ring)) < 30) return;
  const [a, b, c, d] = bboxOf(ring);
  const [i0, j0] = tileOf(a, b), [i1, j1] = tileOf(c, d);
  for (let i = Math.max(0, i0); i <= Math.min(NX - 1, i1); i++) for (let j = Math.max(0, j0); j <= Math.min(NZ - 1, j1); j++) tile(i, j).areas.push({ kind, pts: ring });
  counts.areas++;
};
for (const w of ways.values()) {
  const t = w.tags || {};
  if (t.building || t.highway && t.area !== 'yes') continue;
  const kind = areaKind(t);
  if (kind !== null) addArea(kind, xz(w.geometry));
}
for (const r of rels.values()) {
  const kind = areaKind(r.tags || {});
  if (kind !== null) for (const ring of rings(r.members || [])) addArea(kind, ring);
}

/* trees and shops */
for (const n of nodes.values()) {
  const [x, z] = P.toXZ(n.lat, n.lon);
  const [i, j] = tileOf(x, z);
  if (!inside(i, j)) continue;
  if (n.tags.natural === 'tree') { tile(i, j).trees.push({ x, z, kind: treeKind(n.tags) }); counts.trees++; continue; }
  const poi = toPoi(n.tags);
  if (poi) { tile(i, j).pois.push({ x, z, ...poi }); counts.pois++; }
}

/* ------------------------------------------------------------ write tiles */
const list = [];
for (const [k, t] of tiles) {
  writeFileSync(join(OUT, 'tiles', `${k}.json`), JSON.stringify(encodeTile(t)));
  list.push(k);
}

/* skyline: everything 9 m and up, as [height, colour, flat points] per tile */
const q = (v, o) => Math.round((v - o) * 10);
const skyline = {};
for (const [k, t] of tiles) {
  const list = t.buildings.filter((b) => b.h >= 9 && b.mh < 1).map((b) => [Math.round(b.h * 10) / 10, b.col ?? -1, b.pts.flatMap(([x, z]) => [q(x, X0), q(z, Z0)])]);
  if (list.length) skyline[k] = list;
}
writeFileSync(join(OUT, 'skyline.json'), JSON.stringify(skyline));

/* arteries: named two-way primary, secondary and tertiary streets, the
 * OSM pieces of each street joined end to end so traffic runs its length */
const byName = new Map();
for (const w of ways.values()) {
  const t = w.tags || {};
  if (!t.highway || !t.name) continue;
  const r = toRoad(t, xz(w.geometry));
  if (!r || r.oneway || r.cls < RCLASS.primary || r.cls > RCLASS.tertiary || r.w < 15) continue;
  (byName.get(t.name) || byName.set(t.name, []).get(t.name)).push(r);
}
const arteries = [];
const near = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]) < 1;
for (const [name, pieces] of byName) {
  while (pieces.length) {
    const first = pieces.shift();
    let pts = first.pts.slice(), w = first.w, grown = true;
    while (grown) {
      grown = false;
      for (let k = 0; k < pieces.length; k++) {
        const p = pieces[k].pts;
        if (near(p[0], pts[pts.length - 1])) pts = pts.concat(p.slice(1));
        else if (near(p[p.length - 1], pts[pts.length - 1])) pts = pts.concat(p.slice(0, -1).reverse());
        else if (near(p[p.length - 1], pts[0])) pts = p.slice(0, -1).concat(pts);
        else if (near(p[0], pts[0])) pts = p.slice(1).reverse().concat(pts);
        else continue;
        w = Math.min(w, pieces[k].w);
        pieces.splice(k, 1); grown = true; break;
      }
    }
    let len = 0;
    for (let i = 1; i < pts.length; i++) len += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    if (len > 150) arteries.push({ n: name, c: first.cls, w: Math.round(w * 10) / 10, p: pts.flatMap(([x, z]) => [q(x, X0), q(z, Z0)]) });
  }
}
writeFileSync(join(OUT, 'arteries.json'), JSON.stringify(arteries));

/* -------------------------------------------------------------- elevation */
const STEP = 10;
const HX = Math.ceil((X1 - X0) / STEP) + 1, HZ = Math.ceil((Z1 - Z0) / STEP) + 1;
const height = new Int16Array(HX * HZ);
/* elevation from Terrarium tiles (height = R·256 + G + B/256 − 32768 m), bilinear */
function terrarium(prefix) {
  const files = existsSync(DEM) ? readdirSync(DEM).filter((f) => f.startsWith(prefix)) : [];
  if (!files.length) return null;
  const dem = new Map();
  let z = 0;
  for (const f of files) { const [, zz, x, y] = f.replace('.png', '').split('_').map(Number); z = zz; dem.set(`${x}_${y}`, decodePNG(readFileSync(join(DEM, f)))); }
  const n = 2 ** z;
  return (lat, lon) => {
    const r = (lat * Math.PI) / 180;
    const px = ((lon + 180) / 360) * n * 256, py = ((1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2) * n * 256;
    const at = (gx, gy) => {
      const t = dem.get(`${Math.floor(gx / 256)}_${Math.floor(gy / 256)}`);
      if (!t) return 0;
      const ix = Math.floor(gx) % 256, iy = Math.floor(gy) % 256, o = (iy * 256 + ix) * 4, d = t.data;
      return d[o] * 256 + d[o + 1] + d[o + 2] / 256 - 32768;
    };
    const fx = px - 0.5, fy = py - 0.5, x0 = Math.floor(fx), y0 = Math.floor(fy), u = fx - x0, v = fy - y0;
    return (at(x0, y0) * (1 - u) + at(x0 + 1, y0) * u) * (1 - v) + (at(x0, y0 + 1) * (1 - u) + at(x0 + 1, y0 + 1) * u) * v;
  };
}
const sample = terrarium('dem_');
if (sample) {
  for (let j = 0; j < HZ; j++) for (let i = 0; i < HX; i++) {
    const [lat, lon] = P.toLatLon(X0 + i * STEP, Z0 + j * STEP);
    height[j * HX + i] = Math.round(sample(lat, lon) * 10);
  }
} else console.warn('No elevation downloads (tools/fetch.mjs dem): the ground will be flat.');
// as base64 in JSON, which every web host serves (some won't serve a .bin)
writeFileSync(join(OUT, 'height.json'), JSON.stringify({ int16le: Buffer.from(height.buffer).toString('base64') }));

/* the country round it, for the horizon: every FAR.step metres out to FAR.reach */
const farSample = terrarium('far_');
let farInfo = null;
if (farSample) {
  const cx = (X0 + X1) / 2, cz = (Z0 + Z1) / 2, FN = Math.round((2 * FAR.reach) / FAR.step) + 1;
  const far = new Int16Array(FN * FN);
  for (let j = 0; j < FN; j++) for (let i = 0; i < FN; i++) {
    const [lat, lon] = P.toLatLon(cx - FAR.reach + i * FAR.step, cz - FAR.reach + j * FAR.step);
    far[j * FN + i] = Math.max(-32000, Math.min(32000, Math.round(farSample(lat, lon))));
  }
  writeFileSync(join(OUT, 'far.json'), JSON.stringify({ int16le: Buffer.from(far.buffer).toString('base64') }));
  farInfo = { file: 'far.json', n: FN, step: FAR.step, cx, cz, scale: 1 };
} else console.warn('No horizon elevation (tools/fetch.mjs far): no mountains on the horizon.');

/* --------------------------------------------------------------- overview */
const OV = 4096, S = OV / Math.max(X1 - X0, Z1 - Z0);
const img = Buffer.alloc(OV * OV * 4);
const fill = (r, g, b) => { for (let k = 0; k < OV * OV; k++) { img[k * 4] = r; img[k * 4 + 1] = g; img[k * 4 + 2] = b; img[k * 4 + 3] = 255; } };
fill(190, 188, 181);                // (lots, drives and pavement: light concrete, not sand)
const px = (x) => (x - X0) * S, pz = (z) => (z - Z0) * S;
function poly(pts, [r, g, b]) {
  const ys = pts.map((p) => pz(p[1]));
  const y0 = Math.max(0, Math.floor(Math.min(...ys))), y1 = Math.min(OV - 1, Math.ceil(Math.max(...ys)));
  for (let y = y0; y <= y1; y++) {
    const xs = [];
    for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
      const ya = pz(pts[j][1]), yb = pz(pts[i][1]);
      if ((ya > y + 0.5) !== (yb > y + 0.5)) xs.push(px(pts[j][0]) + ((y + 0.5 - ya) / (yb - ya)) * (px(pts[i][0]) - px(pts[j][0])));
    }
    xs.sort((a, b) => a - b);
    for (let k = 0; k + 1 < xs.length; k += 2) {
      for (let x = Math.max(0, Math.round(xs[k])); x < Math.min(OV, Math.round(xs[k + 1])); x++) { const o = (y * OV + x) * 4; img[o] = r; img[o + 1] = g; img[o + 2] = b; }
    }
  }
}
function line(pts, w, [r, g, b]) {
  const rad = Math.max(0.6, (w * S) / 2);
  for (let k = 0; k < pts.length - 1; k++) {
    const ax = px(pts[k][0]), az = pz(pts[k][1]), bx = px(pts[k + 1][0]), bz = pz(pts[k + 1][1]);
    const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, bz - az)));
    for (let s = 0; s <= n; s++) {
      const cx = ax + ((bx - ax) * s) / n, cz = az + ((bz - az) * s) / n;
      for (let y = Math.floor(cz - rad); y <= cz + rad; y++) for (let x = Math.floor(cx - rad); x <= cx + rad; x++) {
        if (x < 0 || y < 0 || x >= OV || y >= OV || (x - cx) ** 2 + (y - cz) ** 2 > rad * rad) continue;
        const o = (y * OV + x) * 4; img[o] = r; img[o + 1] = g; img[o + 2] = b;
      }
    }
  }
}
const AREA_COL = { [AREA.water]: [86, 156, 196], [AREA.park]: [146, 180, 110], [AREA.grass]: [160, 186, 120], [AREA.beach]: [232, 218, 176], [AREA.parking]: [176, 174, 170], [AREA.pitch]: [120, 170, 96], [AREA.wood]: [110, 140, 90], [AREA.scrub]: [168, 170, 120], [AREA.cemetery]: [150, 176, 120], [AREA.sand]: [220, 205, 160], [AREA.plaza]: [200, 196, 188] };
const all = [...tiles.values()];
for (const t of all) for (const a of t.areas) poly(a.pts, AREA_COL[a.kind] || [200, 196, 188]);
// lawns and gardens round the houses (LA's yards: green where watered, straw where not)
function disc(x, z, r, [cr, cg, cb]) {
  const cx = px(x), cz = pz(z), rr = r * S;
  for (let y = Math.floor(cz - rr); y <= cz + rr; y++) for (let xx = Math.floor(cx - rr); xx <= cx + rr; xx++) {
    if (xx < 0 || y < 0 || xx >= OV || y >= OV || (xx - cx) ** 2 + (y - cz) ** 2 > rr * rr) continue;
    const o = (y * OV + xx) * 4; img[o] = cr; img[o + 1] = cg; img[o + 2] = cb;
  }
}
for (const t of all) for (const b of t.buildings) {
  const area = Math.abs(ringArea(b.pts));
  if (b.h > 10 || area > 450 || (b.type !== BTYPE.house && b.type !== BTYPE.other && b.type !== BTYPE.garage)) continue;
  const [cx, cz] = centroid(b.pts), k = (Math.sin(cx * 0.013 + cz * 0.029) + 1) / 2;
  disc(cx, cz, Math.sqrt(area) * 0.6 + 11, k < 0.75 ? [104 + k * 22, 140 + k * 12, 74] : [168, 162, 116]);
}
for (const t of all) for (const r of [...t.roads].sort((a, b) => b.cls - a.cls)) line(r.pts, r.cls === RCLASS.footway ? 2 : r.w, r.cls <= RCLASS.trunk ? [214, 208, 196] : r.cls === RCLASS.rail ? [120, 110, 100] : r.cls <= RCLASS.tertiary ? [250, 250, 248] : [236, 234, 228]);
for (const t of all) for (const b of t.buildings) poly(b.pts, b.h > 40 ? [120, 116, 124] : [168, 160, 152]);
writeFileSync(join(OUT, 'overview.png'), encodePNG(OV, OV, img));

/* --------------------------------------------------------------- manifest */
const manifest = {
  v: 1, name: process.env.LA_NAME || REAL.name, lat0: REAL.lat0, lon0: REAL.lon0, bounds: { south: REAL.south, north: REAL.north, west: REAL.west, east: REAL.east },
  x0: X0, z0: Z0, width: X1 - X0, depth: Z1 - Z0, tile: T, tilesX: NX, tilesZ: NZ, tiles: list.sort(),
  skyline: 'skyline.json', arteries: 'arteries.json', far: farInfo,
  height: { file: 'height.json', nx: HX, nz: HZ, step: STEP, scale: 0.1 }, overview: { file: 'overview.png', size: OV },
  counts, built: new Date().toISOString(), attribution: `Map data © OpenStreetMap contributors (ODbL)${files.some((f) => f.includes('overture')) ? ', via Overture Maps (overturemaps.org; places CDLA-Permissive-2.0)' : ''}. Elevation: Mapzen Terrarium tiles (AWS Open Data).`
};
writeFileSync(join(OUT, 'manifest.json'), JSON.stringify(manifest, null, 1));
console.log(`${list.length} tiles · ${counts.buildings} buildings (${counts.heightTagged} with measured heights) · ${counts.roads} roads · ${counts.areas} areas · ${counts.trees} trees · ${counts.pois} named shops`);
