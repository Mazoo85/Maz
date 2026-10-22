/*
 * The true-scale map: real Los Angeles at 1:1, from OpenStreetMap.
 *
 * tools/build-tiles.mjs packs the real buildings, streets, parks, trees and
 * shops into 1 km tiles (data/real/). This module loads the ground and the
 * skyline for the whole area up front, then streams the full-detail city in
 * around the player: every building raised from its real footprint to its
 * measured height, the real streets at their real widths, street trees,
 * parked cars, streetlights, pools, parking lots and the shop signs with
 * their real names, on the wall of the building they're in.
 *
 * It gives game.js the same world interface as the hand-built map
 * (createWorld in world.js), so the player, cars, camera, HUD and map all
 * work unchanged. Coordinates are metres, x east and z south, centred on
 * the middle of the area.
 */

import * as THREE from '../vendor/three.module.min.js';
import { makeAtmosphere } from './atmosphere.js';
import { facadeMaterial, withSurface, fadeNearCamera, buildFurniture, makeDetailTexture, makeWaterNormals, mulberry, shadeHex, resample, noiseFill, SURFACE_GLSL } from './render.js';
import { buildFlora } from './flora.js';
import { carGeometries, carMaterials } from './streetlife.js';
import { PROP } from './street.js';
import { projector, decodeTile, BTYPE, ROOF, MATERIAL, RCLASS, AREA, ringArea } from './osm.js';

/* ------------------------------------------------------------ the places */

/* Neighbourhoods, by the middle of each (lat, lon). */
export const NEIGHBOURHOODS = [
  ['Hollywood', 34.0985, -118.3300], ['Hollywood Hills', 34.1200, -118.3420], ['Beachwood Canyon', 34.1210, -118.3200],
  ['Los Feliz', 34.1070, -118.2880], ['Griffith Park', 34.1260, -118.2960], ['East Hollywood', 34.0910, -118.2990],
  ['Thai Town', 34.1010, -118.3090], ['Silver Lake', 34.0880, -118.2690], ['Echo Park', 34.0780, -118.2600],
  ['Elysian Park', 34.0830, -118.2420], ['Elysian Valley', 34.1000, -118.2470], ['Atwater Village', 34.1190, -118.2580],
  ['Chinatown', 34.0630, -118.2380], ['Downtown', 34.0490, -118.2520], ['Bunker Hill', 34.0545, -118.2520],
  ['Little Tokyo', 34.0495, -118.2400], ['Arts District', 34.0400, -118.2340], ['Fashion District', 34.0400, -118.2510],
  ['South Park', 34.0410, -118.2650], ['Westlake', 34.0580, -118.2760], ['Historic Filipinotown', 34.0700, -118.2640],
  ['Pico-Union', 34.0460, -118.2850], ['Koreatown', 34.0610, -118.3000], ['Wilshire Center', 34.0620, -118.2900],
  ['Hancock Park', 34.0690, -118.3330], ['Larchmont', 34.0770, -118.3240], ['Windsor Square', 34.0660, -118.3200],
  ['Mid-Wilshire', 34.0620, -118.3470], ['Fairfax', 34.0760, -118.3560], ['Mid-City', 34.0420, -118.3400],
  ['Virgil Village', 34.0810, -118.2880], ['Melrose Hill', 34.0810, -118.3070]
];

/* Landmarks, at their real spots (lat, lon, radius in metres). */
export const LANDMARKS = [
  ['Hollywood Sign', 34.1341, -118.3215, 160], ['Griffith Observatory', 34.1184, -118.3004, 90], ['Greek Theatre', 34.1196, -118.2965, 80],
  ['Mount Hollywood', 34.1286, -118.3005, 120], ['Bronson Caves', 34.1214, -118.3133, 60], ['Lake Hollywood Reservoir', 34.1219, -118.3326, 220],
  ['Hollywood Bowl', 34.1122, -118.3391, 120], ['Runyon Canyon', 34.1105, -118.3500, 200],
  ['Capitol Records Building', 34.1032, -118.3262, 40], ['Hollywood & Vine', 34.1016, -118.3267, 50], ['Pantages Theatre', 34.1022, -118.3254, 40],
  ['TCL Chinese Theatre', 34.1022, -118.3409, 50], ['Dolby Theatre', 34.1022, -118.3393, 50], ['El Capitan Theatre', 34.1013, -118.3397, 30],
  ['Hollywood Roosevelt Hotel', 34.1016, -118.3417, 40], ['Egyptian Theatre', 34.1015, -118.3374, 30], ['Musso & Frank Grill', 34.1016, -118.3355, 25],
  ['Hollywood Walk of Fame', 34.1016, -118.3310, 60], ['Hollywood Palladium', 34.0981, -118.3247, 50], ['Cinerama Dome', 34.0976, -118.3283, 45],
  ['Hollywood High School', 34.0982, -118.3386, 80], ["Pink's Hot Dogs", 34.0838, -118.3443, 25], ['Paramount Pictures', 34.0838, -118.3200, 250],
  ['Hollywood Forever Cemetery', 34.0899, -118.3195, 200], ['Barnsdall Art Park', 34.1001, -118.2944, 90], ['Vista Theatre', 34.0977, -118.2873, 25],
  ['Sunset Junction', 34.0906, -118.2770, 50], ['Silver Lake Reservoir', 34.0925, -118.2625, 300], ['Echo Park Lake', 34.0730, -118.2606, 160],
  ['Dodger Stadium', 34.0739, -118.2400, 230], ['Philippe the Original', 34.0597, -118.2368, 25], ['Chinatown Central Plaza', 34.0636, -118.2383, 50],
  ['Union Station', 34.0562, -118.2365, 120], ['Olvera Street', 34.0574, -118.2378, 60], ['Los Angeles City Hall', 34.0537, -118.2427, 70],
  ['Grand Park', 34.0555, -118.2460, 120], ['Cathedral of Our Lady of the Angels', 34.0577, -118.2456, 70], ['Dorothy Chandler Pavilion', 34.0573, -118.2488, 60],
  ['Walt Disney Concert Hall', 34.0553, -118.2498, 70], ['The Broad', 34.0544, -118.2506, 45], ['Angels Flight', 34.0513, -118.2500, 30],
  ['Grand Central Market', 34.0508, -118.2489, 40], ['Bradbury Building', 34.0505, -118.2478, 25], ['Los Angeles Central Library', 34.0505, -118.2551, 60],
  ['US Bank Tower', 34.0511, -118.2541, 40], ['Wilshire Grand Center', 34.0502, -118.2601, 50], ['Millennium Biltmore Hotel', 34.0495, -118.2535, 40],
  ['Pershing Square', 34.0484, -118.2532, 80], ['The Last Bookstore', 34.0477, -118.2497, 25], ['Hotel Cecil', 34.0441, -118.2504, 25],
  ['Eastern Columbia Building', 34.0447, -118.2560, 25], ['Orpheum Theatre', 34.0436, -118.2562, 25], ['Ace Hotel', 34.0425, -118.2569, 25],
  ['Crypto.com Arena', 34.0430, -118.2673, 110], ['L.A. Live', 34.0447, -118.2655, 90], ['Los Angeles Convention Center', 34.0403, -118.2696, 180],
  ['Japanese American National Museum', 34.0498, -118.2389, 40], ['Sixth Street Viaduct', 34.0423, -118.2310, 200], ['MacArthur Park', 34.0577, -118.2770, 180],
  ['Bullocks Wilshire', 34.0617, -118.2848, 50], ['The Wiltern', 34.0617, -118.3086, 40], ['LACMA', 34.0639, -118.3592, 130],
  ['La Brea Tar Pits', 34.0638, -118.3554, 120], ['Original Farmers Market', 34.0719, -118.3596, 100], ['Larchmont Village', 34.0763, -118.3237, 80]
];

/* Where the cars to drive are parked: next to these, on the nearest big street. */
const CAR_SPOTS = [
  ['TCL Chinese Theatre', 'convertible'], ['Capitol Records Building', 'taxi'], ['Hollywood Sign', 'suv'], ['Griffith Observatory', 'convertible'],
  ['Dodger Stadium', 'lowrider'], ['Dodger Stadium', 'cruiser'], ['Los Angeles City Hall', 'cruiser'], ['Union Station', 'taxi'],
  ['Crypto.com Arena', 'taxi'], ['Echo Park Lake', 'lowrider'], ['Sunset Junction', 'convertible'], ['The Wiltern', 'taxi'],
  ['MacArthur Park', 'lowrider'], ['Arts District', 'tacotruck'], ['Olvera Street', 'tacotruck'], ['Paramount Pictures', 'exotic'],
  ['LACMA', 'exotic'], ['Larchmont Village', 'convertible'], ['Pershing Square', 'cruiser'], ['Hollywood Bowl', 'suv'], ['Koreatown', 'taxi']
];

/* ------------------------------------------------------------ the world */

const G = 20;                                  // ground mesh grid (m); groundHeight matches it exactly

export async function loadRealWorld(base = 'data/real/', onStep = () => {}) {
  const get = async (f, as = 'json') => {
    const r = await fetch(base + f);
    if (!r.ok) throw new Error(`${base}${f}: ${r.status}`);
    return as === 'json' ? r.json() : as === 'buf' ? r.arrayBuffer() : r.blob();
  };
  const M = await get('manifest.json');
  onStep('Surveying the real ground, every 10 metres…');
  const [hbuf, ovBlob, skyline, arteries] = await Promise.all([
    M.height.file.endsWith('.json')
      ? get(M.height.file).then((j) => Uint8Array.from(atob(j.int16le), (ch) => ch.charCodeAt(0)).buffer)
      : get(M.height.file, 'buf'), get(M.overview.file, 'blob'),
    M.skyline ? get(M.skyline) : {}, M.arteries ? get(M.arteries) : []
  ]);
  const overview = await createImageBitmap(ovBlob);
  return makeRealWorld(M, new Int16Array(hbuf), { overview, skyline, arteries, base });
}

/* Builds the world object from loaded data (split out so tests can feed it
 * data directly). */
export function makeRealWorld(M, heights, { overview = null, skyline = {}, arteries = [], base = '' } = {}) {
  const P = projector(M.lat0, M.lon0);
  const size = Math.ceil(Math.max(M.width, M.depth) / G) * G, half = size / 2;
  const OX = M.x0 + half, OZ = M.z0 + half;
  const toWorld = (lat, lon) => { const [x, z] = P.toXZ(lat, lon); return [x - OX, z - OZ]; };
  const toLatLon = (x, z) => P.toLatLon(x + OX, z + OZ);

  /* the ground: the 10 m elevation grid resampled to the 20 m mesh grid */
  const hs = M.height, fine = (i, j) => heights[Math.min(hs.nz - 1, Math.max(0, j)) * hs.nx + Math.min(hs.nx - 1, Math.max(0, i))] * hs.scale;
  const N = size / G, N1 = N + 1;
  const H = new Float32Array(N1 * N1);
  for (let j = 0; j <= N; j++) {
    for (let i = 0; i <= N; i++) {
      const fx = (i * G - half + OX - M.x0) / hs.step, fz = (j * G - half + OZ - M.z0) / hs.step;
      const i0 = Math.floor(fx), j0 = Math.floor(fz), u = fx - i0, v = fz - j0;
      H[j * N1 + i] = (fine(i0, j0) * (1 - u) + fine(i0 + 1, j0) * u) * (1 - v) + (fine(i0, j0 + 1) * (1 - u) + fine(i0 + 1, j0 + 1) * u) * v;
    }
  }
  function groundHeight(x, z) {
    let fx = (x + half) / G, fz = (z + half) / G;
    fx = Math.min(N - 1e-6, Math.max(0, fx)); fz = Math.min(N - 1e-6, Math.max(0, fz));
    const i = Math.floor(fx), j = Math.floor(fz), u = fx - i, v = fz - j;
    const a = H[j * N1 + i], d = H[j * N1 + i + 1], b = H[(j + 1) * N1 + i], c = H[(j + 1) * N1 + i + 1];
    return u + v < 1 ? a + (d - a) * u + (b - a) * v : c + (b - c) * (1 - u) + (d - c) * (1 - v);
  }

  const RW = {
    real: true, manifest: M, base, size, half, OX, OZ, gridN: N, cell: G, heights: H, overview, skyline,
    toWorld, toLatLon, groundHeight,
    gridHalf: half + 2000,                     // for the collider grid in world.js
    colliders: [], _dirty: true,
    water: [],                                 // lakes and pools of the loaded tiles
    isSea: () => false,
    maskAt: () => 0,
    waterLevelAt(x, z) {
      for (const w of RW.water) {
        if (x < w.a || x > w.c || z < w.b || z > w.d) continue;
        if (pip(x, z, w.pts)) return w.level;
      }
      return null;
    },
    districts: NEIGHBOURHOODS.map(([name, lat, lon]) => { const [x, z] = toWorld(lat, lon); return { name, x, z }; }),
    districtAt(x, z) {
      let best = RW.districts[0], bd = Infinity;
      for (const d of RW.districts) { const dd = (d.x - x) ** 2 + (d.z - z) ** 2; if (dd < bd) { bd = dd; best = d; } }
      return best;
    },
    pois: LANDMARKS.map(([name, lat, lon, r]) => { const [x, z] = toWorld(lat, lon); return { name, x, z, r, kind: 'landmark' }; }),
    // the hand-built map's lists, empty or summarised here
    buildings: { length: M.counts?.buildings || 0 }, trees: { length: (M.counts?.trees || 0) }, blocks: [], props: [], parked: [], parts: [],
    lakes: [], canals: [], crosswalks: [],
    roads: [], vehicles: [],
    stats: {}
  };
  RW.queryPoint = () => null;

  /* the big streets, for the traffic and the buses: named two-way
   * boulevards and avenues, with the names the buses know them by */
  for (const a of arteries) {
    const pts = [];
    for (let i = 0; i < a.p.length; i += 2) pts.push([M.x0 + a.p[i] / 10 - OX, M.z0 + a.p[i + 1] / 10 - OZ]);
    if (pts.length < 2) continue;
    const name = (a.n || '').replace(/ Boulevard$/, ' Blvd').replace(/ Avenue$/, ' Ave').replace(/ Street$/, ' St');
    RW.roads.push({ kind: 'blvd', name, pts, w: a.w });
  }

  /* the cars to drive */
  for (const [where, type] of CAR_SPOTS) {
    const t = RW.pois.find((p) => p.name === where) || RW.districts.find((d) => d.name === where);
    if (!t) continue;
    let best = null, bd = 600;
    for (const r of RW.roads) {
      for (let i = 0; i < r.pts.length - 1; i++) {
        const [ax, az] = r.pts[i], [bx, bz] = r.pts[i + 1];
        const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz || 1;
        const s = Math.max(0, Math.min(1, ((t.x - ax) * dx + (t.z - az) * dz) / L2));
        const d = Math.hypot(ax + dx * s - t.x, az + dz * s - t.z);
        if (d >= bd) continue;
        const L = Math.sqrt(L2), off = r.w / 2 - 1.4;
        const x = ax + dx * s - (dz / L) * off, z = az + dz * s + (dx / L) * off;
        if (RW.vehicles.some((v) => Math.hypot(v.x - x, v.z - z) < 9)) continue;
        bd = d; best = { x, z, ry: Math.atan2(-dz, dx) };
      }
    }
    if (best) RW.vehicles.push({ ...best, type });
  }

  /* where the game starts you, and the title camera */
  const [hx, hz] = toWorld(34.1016, -118.3390);
  RW.start = { x: hx, z: hz };
  const [dx, dz] = toWorld(34.0510, -118.2540);
  RW.title = { x: dx, z: dz, r: 900, h: 220, look: [dx, 120, dz] };
  return RW;
}

export function pip(x, z, pts) {
  let c = false;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
    const [xi, zi] = pts[i], [xj, zj] = pts[j];
    if ((zi > z) !== (zj > z) && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) c = !c;
  }
  return c;
}

/* The smallest rectangle round a footprint: { cx, cz, ux, uz, L, Wd } with
 * (ux, uz) along the long side and L, Wd the half-length and half-width. */
export function minRect(pts) {
  let best = null;
  for (let i = 0; i < pts.length; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[(i + 1) % pts.length];
    const l = Math.hypot(bx - ax, bz - az);
    if (l < 0.5) continue;
    const ux = (bx - ax) / l, uz = (bz - az) / l;
    let u0 = Infinity, u1 = -Infinity, v0 = Infinity, v1 = -Infinity;
    for (const [x, z] of pts) {
      const u = x * ux + z * uz, v = -x * uz + z * ux;
      u0 = Math.min(u0, u); u1 = Math.max(u1, u); v0 = Math.min(v0, v); v1 = Math.max(v1, v);
    }
    const area = (u1 - u0) * (v1 - v0);
    if (!best || area < best.area) {
      const cu = (u0 + u1) / 2, cv = (v0 + v1) / 2;
      best = { area, cx: cu * ux - cv * uz, cz: cu * uz + cv * ux, ux, uz, L: (u1 - u0) / 2, Wd: (v1 - v0) / 2 };
    }
  }
  if (best && best.Wd > best.L) {
    const { ux, uz, L, Wd } = best;
    Object.assign(best, { ux: -uz, uz: ux, L: Wd, Wd: L });
  }
  return best;
}

/* ----------------------------------------------------- how things look */

const hashOf = (x, z) => { const s = Math.sin(x * 12.9898 + z * 78.233) * 43758.5453; return s - Math.floor(s); };
const pick = (arr, h) => arr[Math.floor(h * arr.length) % arr.length];

/* LA stucco: creams, sands, whites, the odd terracotta, sage or pink */
const STUCCO = ['#e8dcc4', '#efe6d2', '#d9c7a7', '#f2efe6', '#cbb89a', '#e3cfb0', '#d6b79a', '#c9c2b4', '#e9d9c9', '#b9a68c', '#dcd3c0', '#f4f1ea', '#c98d6a', '#a9b5a4', '#e6c4b8', '#d8d0bd'];
const BRICK = ['#8a4b38', '#9a5a43', '#7b4636', '#a0654c'];
const CONCRETE = ['#bdb7ac', '#a9a49b', '#c8c3b8', '#b3aea4'];
const GLASS = ['#7f9fb4', '#6d8f8a', '#9aa8b0', '#5f7f9c', '#a08a6c', '#8fb0c0', '#b4bcc0'];   // blue, green, silver, bronze
const WOOD = ['#a08668', '#8a7a64', '#b39a7a'];
const STONE = ['#cdbf9f', '#d8ccb0', '#bfb293'];
const TILE_ROOF = ['#b5643c', '#a8593a', '#c0704a', '#9c5236'];
const SHINGLE = ['#5b5651', '#6e6862', '#4c4a48', '#7a6f62', '#857a6c'];
const FLAT_ROOF = ['#b9b6ae', '#c9c6be', '#a8a59d', '#d6d3cb', '#9a978f'];
const CAR_COLOURS = ['#c0392b', '#f5f5f5', '#1d1d1f', '#2c3e50', '#bdc3c7', '#7f8c8d', '#2980b9', '#e8e2d0', '#8e2b2b', '#3c4a57', '#ffffff', '#111111', '#a3a7ab', '#5d6d7e', '#d5d8dc'];
const SIGN_STYLES = [
  ['#b3221a', '#fff6e8'], ['#f4efe2', '#1d2a44'], ['#1a1a1a', '#ffd34a'], ['#0f4c81', '#ffffff'], ['#f7d046', '#1a1a1a'],
  ['#2e6b3a', '#f6f1e1'], ['#ffffff', '#c0392b'], ['#5a2d82', '#ffe9fb'], ['#e86a1c', '#ffffff'], ['#222831', '#7ef0ff']
];

const AWNINGS = ['#1f5f3a', '#7a1f1f', '#1d3557', '#c4572a', '#2b2b2b', '#d9a441', '#3f6e8c', '#5b2a4a'];

const SURF = { [MATERIAL.stucco]: 0, [MATERIAL.plaster]: 1, [MATERIAL.brick]: 2, [MATERIAL.wood]: 3, [MATERIAL.concrete]: 4, [MATERIAL.glass]: 4, [MATERIAL.stone]: 1, [MATERIAL.metal]: 4 };

/* How a footprint becomes a building: its windows, wall, colour and roof. */
export function styleOf(b, area) {
  const h = hashOf(b.pts[0][0], b.pts[0][1]);
  let { type, roof, mat } = b;
  // most LA footprints are just tagged "building": small and low means a house
  const houseLike = type === BTYPE.house || (type === BTYPE.other && area < 350 && b.h < 9);
  if (houseLike && roof === ROOF.flat && area < 400 && h < 0.85) roof = area < 140 ? ROOF.gabled : ROOF.hipped;
  let win;
  if (type === BTYPE.garage || type === BTYPE.roof) win = 0;
  else if (type === BTYPE.industrial || type === BTYPE.warehouse || type === BTYPE.parking) win = 4;
  else if (mat === MATERIAL.glass) win = 3;
  else if (houseLike || type === BTYPE.apartments || type === BTYPE.school || type === BTYPE.church) win = 1;
  else if (type === BTYPE.other) win = b.h > 30 ? (h < 0.4 ? 3 : 2) : area > 600 || b.h > 9 ? 2 : 1;
  else win = 2;
  const wall = b.col !== null && b.col !== undefined ? `#${b.col.toString(16).padStart(6, '0')}`
    : mat === MATERIAL.brick ? pick(BRICK, h) : mat === MATERIAL.concrete || mat === MATERIAL.metal ? pick(CONCRETE, h)
      : mat === MATERIAL.glass ? pick(GLASS, h) : mat === MATERIAL.wood ? pick(WOOD, h) : mat === MATERIAL.stone ? pick(STONE, h) : pick(STUCCO, h);
  const spanish = hashOf(b.pts[0][1], b.pts[0][0]) < 0.55;
  const roofCol = b.roofCol !== null && b.roofCol !== undefined ? `#${b.roofCol.toString(16).padStart(6, '0')}`
    : roof === ROOF.flat ? pick(FLAT_ROOF, h) : spanish ? pick(TILE_ROOF, h) : pick(SHINGLE, h);
  // trim: a lighter or darker version of the wall
  const trim = shadeHex(wall, hashOf(b.pts[0][0] + 1, b.pts[0][1]) < 0.6 ? 1.18 : 0.78);
  return { win, wall, trim, roof, roofCol, roofMat: roof === ROOF.flat ? 4 : spanish ? 5 : 6, surf: SURF[mat] ?? 0, house: houseLike };
}

/* ----------------------------------------------------- geometry builders */

/* Collects triangles with normals, colours and any per-vertex extras. */
class Mesh {
  constructor(extras = {}) { this.p = []; this.n = []; this.c = []; this.uv = []; this.x = Object.fromEntries(Object.keys(extras).map((k) => [k, []])); }
  get count() { return this.p.length / 3; }
  /* a triangle, wound to face along normal (nx, ny, nz) */
  tri(a, b, c, nx, ny, nz, col, ex, uvs) {
    const e1 = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], e2 = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
    const cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
    const flip = cx * nx + cy * ny + cz * nz < 0;
    const vs = flip ? [a, c, b] : [a, b, c];
    const us = uvs ? (flip ? [uvs[0], uvs[2], uvs[1]] : uvs) : null;
    for (let k = 0; k < 3; k++) {
      this.p.push(vs[k][0], vs[k][1], vs[k][2]); this.n.push(nx, ny, nz); this.c.push(col.r, col.g, col.b);
      if (us) this.uv.push(us[k][0], us[k][1]);
      for (const key in this.x) { const v = ex[key]; if (Array.isArray(v)) this.x[key].push(...v); else this.x[key].push(v); }
    }
  }
  quad(a, b, c, d, nx, ny, nz, col, ex, uvs) {
    this.tri(a, b, c, nx, ny, nz, col, ex, uvs && [uvs[0], uvs[1], uvs[2]]);
    this.tri(a, c, d, nx, ny, nz, col, ex, uvs && [uvs[0], uvs[2], uvs[3]]);
  }
  geometry() {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(this.p, 3));
    g.setAttribute('normal', new THREE.Float32BufferAttribute(this.n, 3));
    g.setAttribute('color', new THREE.Float32BufferAttribute(this.c, 3));
    if (this.uv.length) g.setAttribute('uv', new THREE.Float32BufferAttribute(this.uv, 2));
    for (const [k, arr] of Object.entries(this.x)) g.setAttribute(k, new THREE.Float32BufferAttribute(arr, 1));
    g.computeBoundingSphere();
    return g;
  }
}

/* One building into the wall and roof meshes. Returns its collider. */
function addBuilding(walls, roofs, b, base, st, opts = {}) {
  const pts = b.pts, n = pts.length;
  const y0 = base + b.mh, top = base + b.h;
  const area = Math.abs(ringArea(pts));
  const seed = hashOf(pts[0][0] * 0.37, pts[0][1] * 0.71);
  const wallCol = new THREE.Color(st.wall), roofCol = new THREE.Color(st.roofCol);
  const parapet = st.roof === ROOF.flat && !st.house && b.h >= 6 && !opts.far ? 0.8 : 0;
  // storeys that fit the building's real height exactly
  const typical = st.win === 1 ? 3.1 : st.win === 3 ? 3.9 : st.win === 4 ? 6.5 : 3.8;
  const floor = b.h / Math.max(1, Math.round(b.h / typical));
  const ex = { aWin: st.win, aMat: st.surf, aSeed: seed, aBase: base, aTop: top, aFloor: floor };
  // walls (from below the ground on slopes, so nothing floats)
  const yb = Math.min(y0, b.mh > 0 ? y0 : base - 1.5);
  for (let i = 0; i < n; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[(i + 1) % n];
    const l = Math.hypot(bx - ax, bz - az);
    if (l < 0.01) continue;
    const nx = (bz - az) / l, nz = -(bx - ax) / l;
    walls.quad([ax, yb, az], [bx, yb, bz], [bx, top + parapet, bz], [ax, top + parapet, az], nx, 0, nz, wallCol, ex);
    if (parapet) walls.quad([ax, top, az], [bx, top, bz], [bx, top + parapet, bz], [ax, top + parapet, az], -nx, 0, -nz, wallCol, { ...ex, aWin: 0 });
  }
  if (!opts.far && !st.house && b.mh < 1) {
    const trim = new THREE.Color(st.trim), plain = { ...ex, aWin: 0 };
    // a cornice round the top of the older and mid-rise blocks
    if (st.win !== 3 && b.h >= 8 && st.roof === ROOF.flat) ledge(walls, pts, top + parapet - 0.55, top + parapet + 0.1, 0.32, trim, plain);
    // a belt course over the shopfronts
    if (st.win === 2 && b.h >= 7) ledge(walls, pts, base + 3.9, base + 4.25, 0.18, trim, plain);
  }
  // the roof
  let ridge = top;
  if (st.roof !== ROOF.flat && !opts.far) ridge = pitchedRoof(walls, roofs, b, top, st, area, roofCol, wallCol, ex);
  if (ridge === top) {
    const contour = pts.map(([x, z]) => new THREE.Vector2(x, z));
    const tris = THREE.ShapeUtils.triangulateShape(contour, []);
    const rex = { ...ex, aWin: 0 };
    for (const [a, c, d] of tris) walls.tri([pts[a][0], top, pts[a][1]], [pts[c][0], top, pts[c][1]], [pts[d][0], top, pts[d][1]], 0, 1, 0, roofCol, rex);
  }
  let r = 0, cx = 0, cz = 0;
  for (const [x, z] of pts) { cx += x / n; cz += z / n; }
  for (const [x, z] of pts) r = Math.max(r, Math.hypot(x - cx, z - cz));
  return { k: 2, x: cx, z: cz, r, y0, y1: top, pts };
}

/* A footprint grown outward by d metres, corners mitred (limited on sharp ones). */
export function offsetRing(pts, d) {
  const n = pts.length, out = [];
  for (let i = 0; i < n; i++) {
    const [px, pz] = pts[(i + n - 1) % n], [x, z] = pts[i], [qx, qz] = pts[(i + 1) % n];
    const l1 = Math.hypot(x - px, z - pz) || 1, l2 = Math.hypot(qx - x, qz - z) || 1;
    const n1x = (z - pz) / l1, n1z = -(x - px) / l1, n2x = (qz - z) / l2, n2z = -(qx - x) / l2;
    let mx = n1x + n2x, mz = n1z + n2z;
    const ml = Math.hypot(mx, mz) || 1;
    mx /= ml; mz /= ml;
    const k = Math.min(2, 1 / Math.max(0.2, mx * n1x + mz * n1z));
    out.push([x + mx * d * k, z + mz * d * k]);
  }
  return out;
}

/* A ledge (cornice, belt course) standing out d metres from the walls, from y0 to y1. */
function ledge(m, pts, y0, y1, d, col, ex) {
  const o = offsetRing(pts, d), n = pts.length;
  for (let i = 0; i < n; i++) {
    const j = (i + 1) % n;
    const [ax, az] = o[i], [bx, bz] = o[j];
    const l = Math.hypot(bx - ax, bz - az);
    if (l < 0.01) continue;
    const nx = (bz - az) / l, nz = -(bx - ax) / l;
    m.quad([ax, y0, az], [bx, y0, bz], [bx, y1, bz], [ax, y1, az], nx, 0, nz, col, ex);
    m.quad([pts[i][0], y1, pts[i][1]], [pts[j][0], y1, pts[j][1]], [bx, y1, bz], [ax, y1, az], 0, 1, 0, col, ex);
    m.quad([pts[i][0], y0, pts[i][1]], [pts[j][0], y0, pts[j][1]], [bx, y0, bz], [ax, y0, az], 0, -1, 0, col, ex);
  }
}

/* Hipped, gabled or pyramid roofs. Returns the ridge height (or the eave
 * height if it fell back to a flat roof). */
function pitchedRoof(walls, roofs, b, top, st, area, roofCol, wallCol, ex) {
  const R = minRect(b.pts);
  if (!R) return top;
  const rect = area / (4 * R.L * R.Wd);
  const rex = { aMat: st.roofMat };
  if (rect > 0.62) {
    const o = 0.45, L = R.L + o, Wd = R.Wd + o;
    const rise = Math.min(Wd * (st.roofMat === 5 ? 0.42 : 0.5), 4.5);
    const yR = top + rise;
    const P = (u, v, y) => [R.cx + R.ux * u - R.uz * v, y, R.cz + R.uz * u + R.ux * v];
    const e = top - 0.05;
    const c00 = P(-L, -Wd, e), c10 = P(L, -Wd, e), c11 = P(L, Wd, e), c01 = P(-L, Wd, e);
    const sideN = (v) => { const ny = Wd, nh = rise; const l = Math.hypot(ny, nh); return [(-R.uz * Math.sign(v) * nh) / l, ny / l, (R.ux * Math.sign(v) * nh) / l]; };
    if (st.roof === ROOF.gabled) {
      const r0 = P(-L, 0, yR), r1 = P(L, 0, yR);
      const [ax, ay, az] = sideN(-1), [bx, by, bz] = sideN(1);
      roofs.quad(c00, c10, r1, r0, ax, ay, az, roofCol, rex);
      roofs.quad(c01, c11, r1, r0, bx, by, bz, roofCol, rex);
      // gable ends are wall
      const g = { ...ex, aWin: 0 };
      const ge = (s) => { const a = P(s * R.L, -R.Wd, top), c = P(s * R.L, R.Wd, top), p = P(s * R.L, 0, top + rise * (R.Wd / Wd)); walls.tri(a, c, p, R.ux * s, 0, R.uz * s, wallCol, g); };
      ge(-1); ge(1);
      // a sliver of underside so the overhang isn't see-through from below
      roofs.quad(c00, c10, c11, c01, 0, -1, 0, roofCol, rex);
    } else {
      const k = Math.max(0, L - Wd);
      const r0 = P(-k, 0, yR), r1 = P(k, 0, yR);
      const [ax, ay, az] = sideN(-1), [bx, by, bz] = sideN(1);
      roofs.quad(c00, c10, r1, r0, ax, ay, az, roofCol, rex);
      roofs.quad(c01, c11, r1, r0, bx, by, bz, roofCol, rex);
      const endN = (s) => { const run = L - k || 0.01, l = Math.hypot(run, rise); return [(R.ux * s * rise) / l, run / l, (R.uz * s * rise) / l]; };
      const [ex0, ey0, ez0] = endN(-1), [ex1, ey1, ez1] = endN(1);
      roofs.tri(c00, c01, r0, ex0, ey0, ez0, roofCol, rex);
      roofs.tri(c10, c11, r1, ex1, ey1, ez1, roofCol, rex);
      roofs.quad(c00, c10, c11, c01, 0, -1, 0, roofCol, rex);
    }
    return yR;
  }
  // an odd shape: a low pyramid if it's star-shaped round its middle
  const n = b.pts.length;
  let cx = 0, cz = 0;
  for (const [x, z] of b.pts) { cx += x / n; cz += z / n; }
  for (let i = 0; i < n; i++) {
    const [ax, az] = b.pts[i], [bx, bz] = b.pts[(i + 1) % n];
    if ((ax - cx) * (bz - cz) - (az - cz) * (bx - cx) > 0) return top;
  }
  const rise = Math.min(Math.sqrt(area) * 0.22, 3.5), apex = [cx, top + rise, cz];
  for (let i = 0; i < n; i++) {
    const [ax, az] = b.pts[i], [bx, bz] = b.pts[(i + 1) % n];
    const a = [ax, top, az], c = [bx, top, bz];
    const e1 = [c[0] - a[0], 0, c[2] - a[2]], e2 = [apex[0] - a[0], rise, apex[2] - a[2]];
    let nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0];
    if (ny < 0) { nx = -nx; ny = -ny; nz = -nz; }
    const l = Math.hypot(nx, ny, nz) || 1;
    roofs.tri(a, c, apex, nx / l, ny / l, nz / l, roofCol, rex);
  }
  return top + rise;
}

/* ------------------------------------------------------ road textures */

const TEX = new Map();
function canvasTex(key, w, h, draw, opts = {}) {
  if (TEX.has(key)) return TEX.get(key);
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = opts.linear ? THREE.NoColorSpace : THREE.SRGBColorSpace;
  t.wrapS = opts.wrapS ?? THREE.ClampToEdgeWrapping; t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = 8;
  TEX.set(key, t);
  return t;
}
/* One stretch of road 24 m long, across its whole width: asphalt, lane
 * lines (yellow down the middle of a two-way street) and wear. */
function roadTex(style, lanes) {
  return canvasTex(`road:${style}:${lanes}`, 256, 512, (ctx, w, h) => {
    const free = style === 'free';
    noiseFill(ctx, w, h, free ? '#8e8b85' : style === 'service' ? '#57544f' : '#4d4b48', free ? 20 : 26, lanes * 7 + style.length);
    // tyre-polished wheel paths and oil down the middle of each lane
    for (let l = 0; l < lanes; l++) {
      const x0 = (l / lanes) * w, lw = w / lanes;
      ctx.fillStyle = 'rgba(0,0,0,0.10)'; ctx.fillRect(x0 + lw * 0.42, 0, lw * 0.16, h);
      ctx.fillStyle = 'rgba(255,255,255,0.035)'; ctx.fillRect(x0 + lw * 0.18, 0, lw * 0.12, h); ctx.fillRect(x0 + lw * 0.7, 0, lw * 0.12, h);
    }
    // patches and cracks
    const rnd = mulberry(lanes * 31 + style.length);
    for (let k = 0; k < 9; k++) { ctx.fillStyle = `rgba(${rnd() < 0.5 ? '20,20,20' : '120,118,112'},${0.08 + rnd() * 0.1})`; ctx.fillRect(rnd() * w, rnd() * h, 20 + rnd() * 60, 10 + rnd() * 50); }
    ctx.strokeStyle = 'rgba(15,15,15,0.35)'; ctx.lineWidth = 1;
    for (let k = 0; k < 14; k++) { ctx.beginPath(); let x = rnd() * w, y = rnd() * h; ctx.moveTo(x, y); for (let s = 0; s < 5; s++) { x += (rnd() - 0.5) * 30; y += rnd() * 25; ctx.lineTo(x, y); } ctx.stroke(); }
    if (style === 'plain' || style === 'service') return;
    const lw = w / lanes, px = w / 24;             // ~px per metre along
    const dash = (x, col, len = 3, gap = 9) => { ctx.fillStyle = col; for (let y = 0; y < h; y += (len + gap) * (h / 24)) ctx.fillRect(x - 2, y, 4, len * (h / 24)); };
    const solid = (x, col, wd = 4) => { ctx.fillStyle = col; ctx.fillRect(x - wd / 2, 0, wd, h); };
    if (style === 'two') {
      const mid = lanes / 2;
      for (let l = 1; l < lanes; l++) if (l !== mid) dash(l * lw, 'rgba(240,240,232,0.85)');
      solid(mid * lw - 4, '#e3b72e', 3); solid(mid * lw + 4, '#e3b72e', 3);
      solid(6, 'rgba(240,240,232,0.6)', 3); solid(w - 6, 'rgba(240,240,232,0.6)', 3);
    } else {
      for (let l = 1; l < lanes; l++) dash(l * lw, 'rgba(240,240,232,0.85)', 3, 9);
      solid(5, free ? '#e3b72e' : 'rgba(240,240,232,0.7)', 4); solid(w - 5, 'rgba(240,240,232,0.85)', 4);
    }
    void px;
  });
}
function railTex() {
  return canvasTex('rail', 128, 256, (ctx, w, h) => {
    noiseFill(ctx, w, h, '#6d6559', 60, 8);
    for (let y = 0; y < h; y += 16) { ctx.fillStyle = '#4a3c30'; ctx.fillRect(18, y, w - 36, 7); }
    ctx.fillStyle = '#9a9a98'; ctx.fillRect(38, 0, 5, h); ctx.fillRect(w - 43, 0, 5, h);
  });
}
function pathTex() {
  return canvasTex('path', 64, 256, (ctx, w, h) => {
    noiseFill(ctx, w, h, '#bdb7aa', 18, 4);
    ctx.fillStyle = 'rgba(70,64,58,0.4)';
    for (let y = 0; y < h; y += 32) ctx.fillRect(0, y, w, 2);
  });
}
function sidewalkTex() {
  return canvasTex('sidewalk', 128, 128, (ctx, w, h) => {
    noiseFill(ctx, w, h, '#b9b4a8', 16, 5);
    ctx.fillStyle = 'rgba(60,55,50,0.35)';
    ctx.fillRect(0, 0, w, 2); ctx.fillRect(0, 64, w, 2); ctx.fillRect(64, 0, 2, h);
    ctx.fillStyle = '#d6d2c8'; ctx.fillRect(0, 0, 8, h);           // the kerb
    ctx.fillStyle = 'rgba(50,45,40,0.5)'; ctx.fillRect(8, 0, 2, h);
  });
}
function parkingTex() {
  return canvasTex('parking', 256, 256, (ctx, w, h) => {
    noiseFill(ctx, w, h, '#5a5852', 24, 11);
    ctx.fillStyle = 'rgba(238,238,230,0.85)';
    // two rows of 2.7 m stalls, 5.5 m deep, back to back (texture = 16.2 × 16 m)
    for (let x = 0; x <= w; x += w / 6) { ctx.fillRect(x - 1.5, h * 0.03, 3, h * 0.34); ctx.fillRect(x - 1.5, h * 0.63, 3, h * 0.34); }
    ctx.fillRect(0, h * 0.37, w, 3); ctx.fillRect(0, h * 0.63, w, 3);
    ctx.fillStyle = 'rgba(0,0,0,0.18)';
    for (let k = 0; k < 6; k++) { ctx.beginPath(); ctx.ellipse((k + 0.5) * (w / 6), h * 0.2, 12, 18, 0, 0, 6.28); ctx.fill(); }
  }, { wrapS: THREE.RepeatWrapping });
}

function helipadTex() {
  return canvasTex('helipad', 256, 256, (ctx, w) => {
    noiseFill(ctx, w, w, '#56585a', 18, 21);
    ctx.strokeStyle = '#e8e6df'; ctx.lineWidth = 7; ctx.strokeRect(8, 8, w - 16, w - 16);
    ctx.strokeStyle = '#f1c232'; ctx.lineWidth = 10; ctx.beginPath(); ctx.arc(w / 2, w / 2, w * 0.36, 0, 6.283); ctx.stroke();
    ctx.fillStyle = '#efede6';
    ctx.fillRect(w * 0.36, w * 0.3, w * 0.07, w * 0.4); ctx.fillRect(w * 0.57, w * 0.3, w * 0.07, w * 0.4); ctx.fillRect(w * 0.36, w * 0.465, w * 0.28, w * 0.07);
  });
}

/* ------------------------------------------------------------ the scene */

export function buildRealScene(RW, renderer, opts = {}) {
  const quality = opts.quality || 'high';
  const hi = quality === 'high', lo = quality === 'low';
  const scene = new THREE.Scene();
  const uniforms = { uNight: { value: 0 }, uTime: { value: 0 } };
  const glowMats = [];
  const T = RW.manifest.tile;
  const M = RW.manifest;

  /* ----------------------------------------------------------- ground */
  const texSize = lo ? 2048 : 4096;
  const groundCanvas = document.createElement('canvas');
  groundCanvas.width = groundCanvas.height = texSize;
  const gctx = groundCanvas.getContext('2d');
  gctx.fillStyle = '#d6d0c4'; gctx.fillRect(0, 0, texSize, texSize);
  if (RW.overview) {
    // the overview covers the square from (x0, z0); the world square is a hair larger
    const span = Math.max(M.width, M.depth), k = texSize / RW.size;
    gctx.drawImage(RW.overview, 0, 0, span * k, span * k);
  }
  const groundTex = new THREE.CanvasTexture(groundCanvas);
  groundTex.colorSpace = THREE.SRGBColorSpace;
  groundTex.anisotropy = renderer.capabilities.getMaxAnisotropy();
  const detail = makeDetailTexture();
  const N = RW.gridN, N1 = N + 1;
  const pos = new Float32Array(N1 * N1 * 3), uv = new Float32Array(N1 * N1 * 2);
  for (let j = 0; j <= N; j++) {
    for (let i = 0; i <= N; i++) {
      const k = j * N1 + i;
      pos[k * 3] = -RW.half + i * G; pos[k * 3 + 1] = RW.heights[k]; pos[k * 3 + 2] = -RW.half + j * G;
      uv[k * 2] = i / N; uv[k * 2 + 1] = 1 - j / N;
    }
  }
  const idx = new Uint32Array(N * N * 6);
  let q = 0;
  for (let j = 0; j < N; j++) {
    for (let i = 0; i < N; i++) {
      const a = j * N1 + i, d = a + 1, b = a + N1, c = b + 1;
      idx[q++] = a; idx[q++] = b; idx[q++] = d;
      idx[q++] = b; idx[q++] = c; idx[q++] = d;
    }
  }
  const tg = new THREE.BufferGeometry();
  tg.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  tg.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  tg.setIndex(new THREE.BufferAttribute(idx, 1));
  tg.computeVertexNormals();
  const terrainMat = new THREE.MeshStandardMaterial({ map: groundTex, roughness: 0.95, metalness: 0 });
  const rep = RW.size / 8;                       // detail repeats: 8 m, ~50 m, ~20 m
  terrainMat.onBeforeCompile = (sh) => {
    sh.uniforms.uDetail = { value: detail };
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vTN; varying vec3 vTW;')
      .replace('#include <beginnormal_vertex>', '#include <beginnormal_vertex>\nvTN = objectNormal; vTW = position;');
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nuniform sampler2D uDetail;\nvarying vec3 vTN; varying vec3 vTW;\n' + SURFACE_GLSL)
      .replace('#include <map_fragment>', `#include <map_fragment>
        float dn = texture2D(uDetail, vMapUv * ${rep.toFixed(1)}).r * 0.55 + texture2D(uDetail, vMapUv * ${(rep / 6).toFixed(1)}).g * 0.45;
        float slope = 1.0 - clamp(vTN.y, 0.0, 1.0);
        vec3 rock = vec3(0.47, 0.42, 0.36) * (0.75 + 0.5 * texture2D(uDetail, vMapUv * ${(rep / 2.5).toFixed(1)}).b);
        diffuseColor.rgb = mix(diffuseColor.rgb, rock, smoothstep(0.32, 0.55, slope));
        diffuseColor.rgb *= 0.8 + 0.4 * dn;
        vec3 gc = diffuseColor.rgb;
        float gsat = max(gc.r, max(gc.g, gc.b)) - min(gc.r, min(gc.g, gc.b));
        int tk = 12;
        if (gc.g > gc.r * 1.02 && gc.g > gc.b * 1.15) tk = 10;
        else if (gc.r > gc.b * 1.3 && gc.r > 0.35 && gsat > 0.1) tk = 11;
        else if (gsat > 0.06) tk = 13;
        float tR = 0.95;
        float tH = surfaceDetail(tk, vTW.xz, diffuseColor.rgb, tR);`)
      .replace('#include <roughnessmap_fragment>', '#include <roughnessmap_fragment>\nroughnessFactor = tR;')
      .replace('#include <normal_fragment_maps>', '#include <normal_fragment_maps>\nnormal = bumpNormal(-vViewPosition, normal, tH);');
  };
  const terrain = new THREE.Mesh(tg, terrainMat);
  terrain.receiveShadow = !lo;
  scene.add(terrain);
  // the rest of the basin, out to the horizon
  let minH = Infinity;
  for (const h of RW.heights) minH = Math.min(minH, h);
  const apron = new THREE.Mesh(new THREE.PlaneGeometry(80000, 80000), new THREE.MeshStandardMaterial({ color: 0x8c877a, roughness: 1 }));
  apron.rotation.x = -Math.PI / 2; apron.position.y = minH - 2;
  scene.add(apron);

  /* -------------------------------------------------------- materials */
  const facade = facadeMaterial(uniforms, true);
  const roofMat = withSurface(new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.8 }), 'int(vMat + 0.5)', 'vMat = aMat;');
  const roadMat = (map) => {
    const m = withSurface(new THREE.MeshStandardMaterial({ map, roughness: 0.92, emissive: 0x3a2410, emissiveIntensity: 0 }), '6');
    roadMats.push(m);
    return m;
  };
  const roadMats = [];
  const roadMatCache = new Map();
  const roadMaterial = (style, lanes) => {
    const k = `${style}:${lanes}`;
    if (!roadMatCache.has(k)) roadMatCache.set(k, roadMat(style === 'rail' ? railTex() : style === 'path' ? pathTex() : roadTex(style, lanes)));
    return roadMatCache.get(k);
  };
  const sidewalkMat = withSurface(new THREE.MeshStandardMaterial({ map: sidewalkTex(), roughness: 0.9 }), '4');
  const parkingMat = roadMat(parkingTex());
  const waterNormals = makeWaterNormals();
  waterNormals.wrapS = waterNormals.wrapT = THREE.RepeatWrapping;
  waterNormals.repeat.set(0.05, 0.05);
  const lakeMat = new THREE.MeshStandardMaterial({ color: 0x1f6a7d, roughness: 0.06, metalness: 0.1, normalMap: waterNormals, normalScale: new THREE.Vector2(0.3, 0.3) });
  const poolMat = new THREE.MeshStandardMaterial({ color: 0x3fc4d8, roughness: 0.05, metalness: 0.05, emissive: 0x0b4a58, emissiveIntensity: 0.2 });
  const furniture = buildFurniture(glowMats);
  const carGeo = carGeometries(), carMat = carMaterials();
  const tmpCol = new THREE.Color();
  const BOX = new THREE.BoxGeometry(1, 1, 1);
  const awningMat = withSurface(new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.9, side: THREE.DoubleSide }), '8');
  const padMat = new THREE.MeshStandardMaterial({ map: helipadTex(), roughness: 0.85, polygonOffset: true, polygonOffsetFactor: -2 });
  const acMat = withSurface(new THREE.MeshStandardMaterial({ roughness: 0.6, metalness: 0.3 }), '7');

  /* ---------------------------------------------- the skyline, far off */
  // the taller buildings of every tile, plain, so the whole skyline is always there
  const far = new Map();
  {
    const sky = RW.skyline || {};
    for (const [key, list] of Object.entries(sky)) {
      const [ti, tj] = key.split('_').map(Number);
      const walls = new Mesh({ aWin: 1, aMat: 1, aSeed: 1, aBase: 1, aTop: 1, aFloor: 1 }), none = new Mesh({ aMat: 1 });
      for (const [h, col, flat] of list) {
        const pts = [];
        for (let i = 0; i < flat.length; i += 2) pts.push([M.x0 + flat[i] / 10 - RW.OX, M.z0 + flat[i + 1] / 10 - RW.OZ]);
        if (ringArea(pts) < 0) pts.reverse();
        let base = Infinity;
        for (const [x, z] of pts) base = Math.min(base, RW.groundHeight(x, z));
        const b = { pts, h, mh: 0, type: BTYPE.other, roof: ROOF.flat, mat: h > 60 ? MATERIAL.glass : MATERIAL.stucco, col: col < 0 ? null : col, roofCol: null };
        addBuilding(walls, none, b, base, styleOf(b, Math.abs(ringArea(pts))), { far: true });
      }
      if (!walls.count) continue;
      const mesh = new THREE.Mesh(walls.geometry(), facade);
      mesh.castShadow = false; mesh.receiveShadow = !lo;
      scene.add(mesh);
      far.set(`${ti}_${tj}`, mesh);
    }
  }

  /* ------------------------------------------------- streaming tiles */
  const tiles = new Map();                       // key → { state, group, colliders, water, culls }
  const have = new Set(M.tiles);
  const LOAD = lo ? 650 : hi ? 1100 : 850, DROP = LOAD + 500;
  let fetching = 0;
  const queue = [];                              // tiles fetched and waiting to be built
  let building = null;                           // { key, it } — the tile being built, a bit each frame
  const tileCentre = (key) => { const [i, j] = key.split('_').map(Number); return [M.x0 + (i + 0.5) * T - RW.OX, M.z0 + (j + 0.5) * T - RW.OZ]; };
  const tileDist = (key, x, z) => { const [cx, cz] = tileCentre(key); return Math.hypot(Math.max(0, Math.abs(x - cx) - T / 2), Math.max(0, Math.abs(z - cz) - T / 2)); };

  function want(fx, fz) {
    const out = [];
    const i0 = Math.floor((fx + RW.OX - LOAD - M.x0) / T), i1 = Math.floor((fx + RW.OX + LOAD - M.x0) / T);
    const j0 = Math.floor((fz + RW.OZ - LOAD - M.z0) / T), j1 = Math.floor((fz + RW.OZ + LOAD - M.z0) / T);
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const k = `${i}_${j}`;
      if (have.has(k) && tileDist(k, fx, fz) < LOAD) out.push(k);
    }
    return out.sort((a, b) => tileDist(a, fx, fz) - tileDist(b, fx, fz));
  }

  function request(key) {
    const t = { key, state: 'fetching', group: null, colliders: [], water: [], culls: [], disposables: [] };
    tiles.set(key, t);
    fetching++;
    fetch(`${RW.base}tiles/${key}.json`).then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then((json) => { if (tiles.get(key) !== t) return; t.state = 'queued'; t.data = decodeTile(json); queue.push(t); })
      .catch((e) => { console.warn(`tile ${key}:`, e); t.state = 'failed'; })
      .finally(() => { fetching--; });
  }

  function unload(t) {
    tiles.delete(t.key);
    if (t.group) {
      scene.remove(t.group);
      t.group.traverse((o) => { if (o.geometry && !o.userData.shared) o.geometry.dispose(); });
      for (const d of t.disposables) d.dispose();
    }
    if (t.colliders.length) { const s = new Set(t.colliders); RW.colliders = RW.colliders.filter((c) => !s.has(c)); RW._dirty = true; }
    if (t.water.length) { const s = new Set(t.water); RW.water = RW.water.filter((w) => !s.has(w)); }
    const f = far.get(t.key); if (f) f.visible = true;
    if (building && building.t === t) building = null;
  }

  function finish(t, out) {
    t.group = out.group;
    t.colliders = out.colliders; t.water = out.water; t.culls = out.culls; t.disposables = out.disposables;
    scene.add(t.group);
    RW.colliders.push(...t.colliders); RW._dirty = true;
    RW.water.push(...t.water);
    const f = far.get(t.key); if (f) f.visible = false;
    t.state = 'ready';
    t.data = null;
  }

  /* -------------------------------------------- building one tile */
  function* buildTile(t) {
    const d = t.data;
    const group = new THREE.Group();
    const colliders = [], water = [], culls = [], disposables = [];
    const W2 = (p) => [p[0] - RW.OX, p[1] - RW.OZ];
    const gh = RW.groundHeight;
    const [tcx, tcz] = tileCentre(t.key);
    const SUB = T / 2;
    const subKey = (x, z) => `${Math.floor((x - tcx + T / 2) / SUB)},${Math.floor((z - tcz + T / 2) / SUB)}`;
    const subCentre = (k) => { const [a, b] = k.split(',').map(Number); return [tcx - T / 2 + (a + 0.5) * SUB, tcz - T / 2 + (b + 0.5) * SUB]; };

    /* instanced things, split into quarter tiles so the far ones can hide */
    const m4 = new THREE.Matrix4(), qt = new THREE.Quaternion(), eu = new THREE.Euler(0, 0, 0, 'YXZ'), v3 = new THREE.Vector3(), s3 = new THREE.Vector3();
    function batch(items, geometry, material, { cast = true, receive = true, far: fd = 0 } = {}) {
      const by = new Map();
      for (const it of items) { const k = subKey(it.x, it.z); (by.get(k) || by.set(k, []).get(k)).push(it); }
      for (const [k, arr] of by) {
        const mesh = new THREE.InstancedMesh(geometry, material, arr.length);
        mesh.userData.shared = true;
        arr.forEach((it, n) => {
          eu.set(it.rx || 0, it.ry || 0, it.rz || 0, 'YXZ');
          qt.setFromEuler(eu);
          m4.compose(v3.set(it.px ?? it.x, it.py, it.pz ?? it.z), qt, s3.set(it.sx ?? 1, it.sy ?? 1, it.sz ?? 1));
          mesh.setMatrixAt(n, m4);
          mesh.setColorAt(n, tmpCol.set(it.c || '#ffffff'));
        });
        mesh.instanceMatrix.needsUpdate = true;
        if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
        mesh.castShadow = cast && !lo; mesh.receiveShadow = receive && !lo;
        mesh.computeBoundingSphere();
        group.add(mesh);
        if (fd) { const [cx, cz] = subCentre(k); culls.push({ mesh, cx, cz, half: SUB / 2, far: fd }); }
      }
    }

    /* buildings */
    const walls = new Mesh({ aWin: 1, aMat: 1, aSeed: 1, aBase: 1, aTop: 1, aFloor: 1 }), roofs = new Mesh({ aMat: 1 });
    const blds = [];
    const bgrid = new Map();                     // 20 m cells → buildings, for "is this spot inside a building?"
    let n = 0;
    for (const b0 of d.buildings) {
      const pts = b0.pts.map(W2);
      const b = { ...b0, pts };
      const area = Math.abs(ringArea(pts));
      let base = Infinity;
      for (const [x, z] of pts) base = Math.min(base, gh(x, z));
      const st = styleOf(b, area);
      const col = addBuilding(walls, roofs, b, base, st);
      colliders.push(col);
      let a = Infinity, bb = Infinity, c = -Infinity, e = -Infinity;
      for (const [x, z] of pts) { a = Math.min(a, x); bb = Math.min(bb, z); c = Math.max(c, x); e = Math.max(e, z); }
      const rec = { b, pts, base, st, area, a, bb, c, e, name: b0.name };
      blds.push(rec);
      for (let gz = Math.floor(bb / 20); gz <= Math.floor(e / 20); gz++) for (let gx = Math.floor(a / 20); gx <= Math.floor(c / 20); gx++) {
        const k = gz * 100000 + gx; (bgrid.get(k) || bgrid.set(k, []).get(k)).push(rec);
      }
      if (++n % 120 === 0) yield;
    }
    const inBuilding = (x, z, pad = 0) => {
      const l = bgrid.get(Math.floor(z / 20) * 100000 + Math.floor(x / 20));
      if (!l) return false;
      for (const r of l) {
        if (x < r.a - pad || x > r.c + pad || z < r.bb - pad || z > r.e + pad) continue;
        if (pip(x, z, r.pts)) return true;
        if (pad) for (const [px, pz] of [[pad, 0], [-pad, 0], [0, pad], [0, -pad]]) if (pip(x + px, z + pz, r.pts)) return true;
      }
      return false;
    };
    if (walls.count) { const m = new THREE.Mesh(walls.geometry(), facade); m.castShadow = !lo; m.receiveShadow = !lo; group.add(m); }
    if (roofs.count) { const m = new THREE.Mesh(roofs.geometry(), roofMat); m.castShadow = !lo; m.receiveShadow = !lo; group.add(m); }
    yield;

    /* rooftop air-conditioning units and stair housings on the flat roofs */
    const ac = [];
    for (const r of blds) {
      if (r.st.roof !== ROOF.flat || r.st.house || r.area < 250 || r.b.h < 5) continue;
      const h = hashOf(r.a, r.e), cnt = Math.min(6, 1 + Math.floor(r.area / 700) + Math.floor(h * 2));
      for (let k = 0; k < cnt; k++) {
        const x = r.a + (r.c - r.a) * (0.2 + 0.6 * hashOf(r.a + k, r.bb)), z = r.bb + (r.e - r.bb) * (0.2 + 0.6 * hashOf(r.bb + k * 3, r.c));
        if (!pip(x, z, r.pts) || !pip(x + 2, z + 2, r.pts) || !pip(x - 2, z - 2, r.pts)) continue;
        const big = hashOf(x, z) < 0.25;
        ac.push({ x, z, py: r.base + r.b.h + (big ? 1.2 : 0.55), sx: big ? 3 : 1.6, sy: big ? 2.4 : 1.1, sz: big ? 3.4 : 1.4, ry: hashOf(z, x) < 0.5 ? 0 : 0.6, c: big ? '#b8b4ac' : '#d4d2cc' });
      }
    }
    if (ac.length) batch(ac, BOX, acMat, { far: 600, cast: true });

    /* helipads: for decades LA's fire code made every tower over 75 ft have
     * one, which is why Downtown's skyline is flat-topped */
    const pads = new Mesh(), padCol = new THREE.Color(1, 1, 1);
    for (const r of blds) {
      if (r.st.roof !== ROOF.flat || r.b.h < 70 || r.area < 500) continue;
      const R = minRect(r.pts);
      if (!R || !pip(R.cx, R.cz, r.pts)) continue;
      const half = Math.min(R.Wd * 0.8, R.L * 0.8, 11);
      if (half < 6) continue;
      const y = r.base + r.b.h + 0.12;
      const P = (u, v) => [R.cx + R.ux * u - R.uz * v, y, R.cz + R.uz * u + R.ux * v];
      pads.quad(P(-half, -half), P(half, -half), P(half, half), P(-half, half), 0, 1, 0, padCol, null, [[0, 0], [1, 0], [1, 1], [0, 1]]);
    }
    if (pads.count) { const pm = new THREE.Mesh(pads.geometry(), padMat); pm.receiveShadow = !lo; group.add(pm); }

    /* roads, sidewalks, parking lots */
    const groups = new Map();                    // material → Mesh builder
    const meshFor = (mat) => groups.get(mat) || groups.set(mat, new Mesh()).get(mat);
    const roadGrid = new Map();                  // 6 m cells → road ids, to keep things off other streets
    const roads = d.roads.map((r, id) => ({ ...r, id, pts: r.pts.map(W2) }));
    for (const r of roads) {
      const hw = r.w / 2 + 0.5;
      for (const [x, z] of resample(r.pts, 3)) {
        const rr = Math.ceil(hw / 6);
        for (let dz = -rr; dz <= rr; dz++) for (let dx = -rr; dx <= rr; dx++) {
          const cx = Math.floor(x / 6) + dx, cz = Math.floor(z / 6) + dz;
          if (Math.hypot((cx + 0.5) * 6 - x, (cz + 0.5) * 6 - z) > hw + 4.3) continue;
          const k = cz * 100000 + cx; (roadGrid.get(k) || roadGrid.set(k, new Set()).get(k)).add(r.id);
        }
      }
    }
    const onOtherRoad = (x, z, id) => { const s = roadGrid.get(Math.floor(z / 6) * 100000 + Math.floor(x / 6)); if (!s) return false; for (const o of s) if (o !== id) return true; return false; };
    const Y_OFF = [0.17, 0.16, 0.15, 0.14, 0.13, 0.12, 0.11, 0.1, 0.1];
    const white = new THREE.Color(1, 1, 1);
    let rn = 0;
    for (const r of roads) {
      const pts = resample(r.pts, 6);
      if (pts.length < 2) continue;
      const footway = r.cls === RCLASS.footway, rail = r.cls === RCLASS.rail;
      const style = rail ? 'rail' : footway ? 'path' : r.cls <= RCLASS.trunk ? 'free' : r.cls >= RCLASS.service ? 'service' : r.cls === RCLASS.residential ? 'plain' : r.oneway ? 'one' : 'two';
      const lanes = style === 'two' ? Math.max(2, 2 * Math.round(r.w / 7.2)) : Math.max(1, Math.round(r.w / 3.7));
      const mat = roadMaterial(style, Math.min(lanes, 8));
      const yo = Y_OFF[r.cls] ?? 0.1;
      const y0 = gh(...pts[0]), y1 = gh(...pts[pts.length - 1]);
      let along = 0;
      const ribbon = (m, off0, off1, yoff, vScale, u0 = 0, u1 = 1) => {
        let s = 0;
        for (let i = 0; i < pts.length - 1; i++) {
          const [ax, az] = pts[i], [bx, bz] = pts[i + 1];
          const l = Math.hypot(bx - ax, bz - az) || 1;
          const nx = -(bz - az) / l, nz = (bx - ax) / l;
          const ya = r.bridge ? y0 + ((y1 - y0) * s) / Math.max(1, along) : gh(ax, az), yb = r.bridge ? y0 + ((y1 - y0) * (s + l)) / Math.max(1, along) : gh(bx, bz);
          const A0 = [ax + nx * off0, Math.max(ya, gh(ax + nx * off0, az + nz * off0)) + yoff, az + nz * off0];
          const A1 = [ax + nx * off1, Math.max(ya, gh(ax + nx * off1, az + nz * off1)) + yoff, az + nz * off1];
          const B0 = [bx + nx * off0, Math.max(yb, gh(bx + nx * off0, bz + nz * off0)) + yoff, bz + nz * off0];
          const B1 = [bx + nx * off1, Math.max(yb, gh(bx + nx * off1, bz + nz * off1)) + yoff, bz + nz * off1];
          const v0 = s / vScale, v1 = (s + l) / vScale;
          m.quad(A0, B0, B1, A1, 0, 1, 0, white, null, [[u0, v0], [u0, v1], [u1, v1], [u1, v0]]);
          s += l;
        }
      };
      for (let i = 0; i < pts.length - 1; i++) along += Math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]);
      ribbon(meshFor(mat), -r.w / 2, r.w / 2, yo, footway ? 8 : rail ? 12 : 24);
      if (r.cls >= RCLASS.primary && r.cls <= RCLASS.residential) {
        const sw = r.cls === RCLASS.residential ? 2.4 : 3.6;
        ribbon(meshFor(sidewalkMat), r.w / 2, r.w / 2 + sw, 0.07, 4, 0, 1);
        ribbon(meshFor(sidewalkMat), -r.w / 2, -r.w / 2 - sw, 0.07, 4, 0, 1);
      }
      if (++rn % 60 === 0) yield;
    }
    for (const a of d.areas) {
      if (a.kind !== AREA.parking && a.kind !== AREA.plaza) continue;
      const pts = a.pts.map(W2);
      const R = minRect(pts);
      if (!R) continue;
      const contour = pts.map(([x, z]) => new THREE.Vector2(x, z));
      const tris = THREE.ShapeUtils.triangulateShape(contour, []);
      const m = meshFor(a.kind === AREA.parking ? parkingMat : sidewalkMat);
      const uvOf = (x, z) => [((x - R.cx) * R.ux + (z - R.cz) * R.uz) / 16.2, (-(x - R.cx) * R.uz + (z - R.cz) * R.ux) / 16];
      for (const [i, j, k] of tris) {
        const P3 = (p) => [p[0], gh(p[0], p[1]) + 0.06, p[1]];
        m.tri(P3(pts[i]), P3(pts[j]), P3(pts[k]), 0, 1, 0, white, null, [uvOf(...pts[i]), uvOf(...pts[j]), uvOf(...pts[k])]);
      }
    }
    for (const [mat, m] of groups) {
      if (!m.count) continue;
      const mesh = new THREE.Mesh(m.geometry(), mat);
      mesh.receiveShadow = !lo;
      group.add(mesh);
    }
    yield;

    /* lakes, reservoirs and backyard pools */
    for (const a of d.areas) {
      if (a.kind !== AREA.water) continue;
      const pts = a.pts.map(W2);
      const area = Math.abs(ringArea(pts));
      let lvl = 0;
      for (const [x, z] of pts) lvl += gh(x, z) / pts.length;
      const pool = area < 400;
      lvl += pool ? 0.08 : 0.35;
      const contour = pts.map(([x, z]) => new THREE.Vector2(x, z));
      const tris = THREE.ShapeUtils.triangulateShape(contour, []);
      const m = new Mesh();
      for (const [i, j, k] of tris) m.tri([pts[i][0], lvl, pts[i][1]], [pts[j][0], lvl, pts[j][1]], [pts[k][0], lvl, pts[k][1]], 0, 1, 0, white, null, [[pts[i][0], pts[i][1]], [pts[j][0], pts[j][1]], [pts[k][0], pts[k][1]]]);
      if (!m.count) continue;
      group.add(new THREE.Mesh(m.geometry(), pool ? poolMat : lakeMat));
      let a0 = Infinity, b0 = Infinity, c0 = -Infinity, d0 = -Infinity;
      for (const [x, z] of pts) { a0 = Math.min(a0, x); b0 = Math.min(b0, z); c0 = Math.max(c0, x); d0 = Math.max(d0, z); }
      if (!pool) water.push({ pts, level: lvl, a: a0, b: b0, c: c0, d: d0 });
    }
    yield;

    /* trees: the mapped ones, then street trees down the parkways, park
     * trees and big backyard trees */
    const trees = [];
    const treeGrid = new Set();
    const tkey = (x, z) => Math.floor(z / 5) * 100000 + Math.floor(x / 5);
    const nearTree = (x, z) => { for (let a = -1; a <= 1; a++) for (let b = -1; b <= 1; b++) if (treeGrid.has(tkey(x + a * 5, z + b * 5))) return true; return false; };
    const plant = (x, z, tt, h, s = 1) => { trees.push({ x, z, y: gh(x, z), t: tt, h, s, r: hashOf(z, x) * 6.28 }); treeGrid.add(tkey(x, z)); };
    const CAP = lo ? 2500 : hi ? 9000 : 6000;
    for (const tr of d.trees) {
      const [x, z] = W2([tr.x, tr.z]);
      const hh = hashOf(x, z);
      const palm = tr.kind === 0 || tr.kind === 6 || tr.kind === 7;
      plant(x, z, palm ? 0 : tr.kind === 2 ? 2 : tr.kind === 4 ? 4 : tr.kind === 5 ? 5 : 1, palm ? 14 + hh * 12 : 7 + hh * 7, 0.85 + hh * 0.3);
    }
    yield;
    for (const r of roads) {
      if (trees.length > CAP) break;
      if (r.cls < RCLASS.primary || r.cls > RCLASS.residential) continue;
      // each street has its species: a palm-lined street, jacarandas, or a mix
      const sh = hashOf(r.pts[0][0] * 0.1, r.pts[0][1] * 0.1);
      const kind = sh < 0.28 ? 0 : sh < 0.42 ? 2 : sh < 0.47 ? 4 : 1;
      const step = kind === 0 ? 14 : 11, off = r.w / 2 + (r.cls === RCLASS.residential ? 1.4 : 1.9);
      const pts = resample(r.pts, step);
      for (let i = 1; i < pts.length - 1; i++) {
        const [ax, az] = pts[i - 1], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1;
        const nx = -(bz - az) / l, nz = (bx - ax) / l;
        for (const s of [-1, 1]) {
          const x = pts[i][0] + nx * off * s, z = pts[i][1] + nz * off * s;
          const hh = hashOf(x, z);
          if (hh > (r.cls === RCLASS.residential ? 0.72 : 0.55)) continue;
          if (onOtherRoad(x, z, r.id) || inBuilding(x, z, 1.5) || nearTree(x, z)) continue;
          plant(x, z, kind, kind === 0 ? 16 + hh * 10 : kind === 4 ? 9 + hh * 4 : 7 + hh * 6, 0.85 + hh * 0.3);
        }
      }
    }
    yield;
    for (const a of d.areas) {
      if (trees.length > CAP) break;
      const dens = a.kind === AREA.wood ? 160 : a.kind === AREA.park ? 420 : a.kind === AREA.cemetery ? 380 : a.kind === AREA.scrub ? 90 : 0;
      if (!dens) continue;
      const pts = a.pts.map(W2);
      let a0 = Infinity, b0 = Infinity, c0 = -Infinity, d0 = -Infinity;
      for (const [x, z] of pts) { a0 = Math.min(a0, x); b0 = Math.min(b0, z); c0 = Math.max(c0, x); d0 = Math.max(d0, z); }
      // only the part in this tile
      a0 = Math.max(a0, tcx - T / 2); c0 = Math.min(c0, tcx + T / 2); b0 = Math.max(b0, tcz - T / 2); d0 = Math.min(d0, tcz + T / 2);
      const sp = Math.sqrt(dens);
      for (let z = b0; z < d0; z += sp) for (let x = a0; x < c0; x += sp) {
        const jx = x + (hashOf(x, z) - 0.5) * sp, jz = z + (hashOf(z, x) - 0.5) * sp;
        if (!pip(jx, jz, pts) || hashOf(jx * 3, jz) < 0.35) continue;
        if (onOtherRoad(jx, jz, -1) || inBuilding(jx, jz, 1) || nearTree(jx, jz)) continue;
        const hh = hashOf(jx, jz * 3);
        if (a.kind === AREA.scrub) plant(jx, jz, hh < 0.15 ? 5 : 3, hh < 0.15 ? 6 + hh * 20 : 1.2 + hh * 1.6);
        else if (a.kind === AREA.cemetery) plant(jx, jz, hh < 0.35 ? 4 : hh < 0.5 ? 0 : 1, hh < 0.35 ? 10 + hh * 6 : hh < 0.5 ? 18 + hh * 8 : 8 + hh * 6);
        else plant(jx, jz, hh < 0.15 ? 0 : a.kind === AREA.wood && hh < 0.6 ? 5 : 1, hh < 0.15 ? 16 + hh * 40 : 8 + hh * 8);
        if (trees.length > CAP) break;
      }
    }
    for (const r of blds) {
      if (trees.length > CAP) break;
      if (!r.st.house) continue;
      const hh = hashOf(r.a * 0.3, r.e * 0.7);
      if (hh < 0.45) continue;
      const cx = (r.a + r.c) / 2, cz = (r.bb + r.e) / 2, ang = hh * 40, dist = Math.max(r.c - r.a, r.e - r.bb) / 2 + 4 + hh * 4;
      const x = cx + Math.cos(ang) * dist, z = cz + Math.sin(ang) * dist;
      if (onOtherRoad(x, z, -1) || inBuilding(x, z, 2) || nearTree(x, z)) continue;
      plant(x, z, hh > 0.9 ? 0 : hh > 0.8 ? 4 : 1, hh > 0.9 ? 15 + hh * 8 : 8 + hh * 6);
    }
    yield;
    if (trees.length) buildFlora({ trees, buildings: [], groundHeight: gh, maskAt: () => 0, isSea: () => false, queryPoint: () => null }, group, { THREE, batch, lo, hi, fadeNearCamera, mulberry, shadeHex, glowMats });
    yield;

    /* streetlights, hydrants, and the cars parked along the kerbs */
    const lights = [], hydrants = [], parked = [];
    for (const r of roads) {
      if (r.cls < RCLASS.primary || r.cls > RCLASS.residential) continue;
      const res = r.cls === RCLASS.residential;
      const pts = resample(r.pts, res ? 48 : 36);
      for (let i = 1; i < pts.length - 1; i++) {
        const [ax, az] = pts[i - 1], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1;
        const nx = -(bz - az) / l, nz = (bx - ax) / l;
        const s = res || i % 2 ? 1 : -1;
        const off = r.w / 2 + 0.6;
        const x = pts[i][0] + nx * off * s, z = pts[i][1] + nz * off * s;
        if (onOtherRoad(x, z, r.id) || inBuilding(x, z, 0.6)) continue;
        lights.push({ x, z, py: gh(x, z), ry: Math.atan2(nz * s, -nx * s) });
        if (hashOf(x, z) < 0.3) { const hx = x + (bx - ax) / l * 3, hz = z + (bz - az) / l * 3; if (!inBuilding(hx, hz, 0.5)) hydrants.push({ x: hx, z: hz, py: gh(hx, hz), ry: 0 }); }
      }
      if (r.cls < RCLASS.secondary || r.oneway) continue;
      // parked cars, nose to tail along the kerb, with gaps for driveways
      const cp = resample(r.pts, 6.4);
      for (let i = 2; i < cp.length - 2; i++) {
        const [ax, az] = cp[i - 1], [bx, bz] = cp[i + 1], l = Math.hypot(bx - ax, bz - az) || 1;
        const dx = (bx - ax) / l, dz = (bz - az) / l, nx = -dz, nz = dx;
        for (const s of [-1, 1]) {
          const hh = hashOf(cp[i][0] * 1.3 + s, cp[i][1] * 0.7);
          if (hh > (res ? 0.42 : 0.3)) continue;
          const off = r.w / 2 - 1.2;
          const x = cp[i][0] + nx * off * s, z = cp[i][1] + nz * off * s;
          if (onOtherRoad(x, z, r.id) || onOtherRoad(x + dx * 4, z + dz * 4, r.id) || onOtherRoad(x - dx * 4, z - dz * 4, r.id)) continue;
          const kind = ['sedan', 'sedan', 'sedan', 'compact', 'compact', 'suv', 'suv', 'pickup', 'van'][Math.floor(hashOf(x, z) * 9)];
          parked.push({ x, z, py: gh(x, z) + 0.12, ry: Math.atan2(-dz * s, dx * s), kind, c: CAR_COLOURS[Math.floor(hashOf(z, x) * CAR_COLOURS.length)] });
        }
      }
    }
    for (const part of furniture[PROP.STREETLIGHT]) batch(lights, part.geo, part.mat, { far: part.far, cast: part.cast !== false });
    for (const part of furniture[PROP.HYDRANT]) batch(hydrants, part.geo, part.mat, { far: part.far, cast: false });
    const byKind = {};
    for (const p of parked) (byKind[p.kind] ||= []).push(p);
    for (const [kind, list] of Object.entries(byKind)) {
      const g = carGeo[kind];
      const plain = list.map((p) => ({ ...p, c: '#ffffff' }));
      batch(list, g.paint, carMat.paint, { far: 800 });
      batch(plain, g.trim, carMat.trim, { far: 450, cast: false });
      batch(plain, g.glass, carMat.glass, { far: 650, cast: false });
      batch(plain, g.lights, carMat.trim, { far: 300, cast: false });
    }
    yield;

    /* shop signs: each named shop, café and bar on the wall of its building */
    const signs = [];
    for (const p of d.pois) {
      const [x, z] = W2([p.x, p.z]);
      let best = null, bd = 28;
      const l = [];
      for (let gz = Math.floor((z - 28) / 20); gz <= Math.floor((z + 28) / 20); gz++) for (let gx = Math.floor((x - 28) / 20); gx <= Math.floor((x + 28) / 20); gx++) for (const r of bgrid.get(gz * 100000 + gx) || []) l.push(r);
      for (const r of l) {
        if (r.b.h < 3.5) continue;
        const pts = r.pts;
        for (let i = 0; i < pts.length; i++) {
          const [ax, az] = pts[i], [bx, bz] = pts[(i + 1) % pts.length];
          const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
          if (L2 < 16) continue;
          const s = Math.max(0.15, Math.min(0.85, ((x - ax) * dx + (z - az) * dz) / L2));
          const qx = ax + dx * s, qz = az + dz * s, dd = Math.hypot(qx - x, qz - z);
          // prefer walls that face a street
          const L = Math.sqrt(L2), nx = dz / L, nz = -dx / L;
          const street = onOtherRoad(qx + nx * 8, qz + nz * 8, -1) ? 0 : 12;
          if (dd + street < bd) { bd = dd + street; best = { r, qx, qz, nx, nz, len: L, dx: dx / L, dz: dz / L }; }
        }
      }
      if (!best || signs.length >= 40) continue;
      signs.push({ ...best, name: p.name.slice(0, 28) });
    }
    const awnings = new Mesh();
    if (signs.length) {
      const c = document.createElement('canvas');
      c.width = 1024; c.height = 2048;
      const ctx = c.getContext('2d');
      const CW = 512, CH = 96;
      const m = new Mesh();
      signs.forEach((s, i) => {
        const col = i % 2, row = Math.floor(i / 2), x0 = col * CW, y0 = row * CH;
        const [bg, fg] = SIGN_STYLES[Math.floor(hashOf(s.qx, s.qz) * SIGN_STYLES.length)];
        ctx.fillStyle = bg; ctx.fillRect(x0 + 2, y0 + 2, CW - 4, CH - 4);
        ctx.strokeStyle = shadeHex(bg, 0.6); ctx.lineWidth = 6; ctx.strokeRect(x0 + 5, y0 + 5, CW - 10, CH - 10);
        ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        let size = 60;
        const font = (sz) => `${i % 3 === 0 ? 'italic ' : ''}bold ${sz}px ${i % 4 === 1 ? 'Georgia, serif' : 'Helvetica, Arial, sans-serif'}`;
        ctx.font = font(size);
        while (ctx.measureText(s.name).width > CW - 40 && size > 18) { size -= 3; ctx.font = font(size); }
        ctx.fillText(s.name, x0 + CW / 2, y0 + CH / 2 + 3);
        // the sign on the wall
        const w = Math.min(s.len * 0.8, Math.max(2.6, Math.min(9, ctx.measureText(s.name).width / size * 1.1 + 1))), hgt = w * (CH / CW);
        const y = s.r.base + Math.min(4.2, s.r.b.h - hgt / 2 - 0.4);
        const ox = s.qx + s.nx * 0.12, oz = s.qz + s.nz * 0.12;
        const A = [ox - s.dx * w / 2, y - hgt / 2, oz - s.dz * w / 2], B = [ox + s.dx * w / 2, y - hgt / 2, oz + s.dz * w / 2];
        const Cc = [B[0], y + hgt / 2, B[2]], D = [A[0], y + hgt / 2, A[2]];
        const u0 = x0 / 1024, u1 = (x0 + CW) / 1024, v0 = 1 - (y0 + CH) / 2048, v1 = 1 - y0 / 2048;
        // the wall runs a→b with the outside on the left (nx, nz): text reads left to right from outside
        m.quad(B, A, D, Cc, s.nx, 0, s.nz, white, null, [[u0, v0], [u1, v0], [u1, v1], [u0, v1]]);
        // about half the shops have a canvas awning over the door
        const yt = y - hgt / 2 - 0.15;
        if (hashOf(s.qz, s.qx) < 0.55 && yt - s.r.base > 2.7) {
          const aw = Math.min(s.len * 0.9, w + 0.6) / 2, out = 1.3, drop = 0.55;
          const col = new THREE.Color(AWNINGS[Math.floor(hashOf(s.qx * 3, s.qz) * AWNINGS.length)]);
          const P = (along, o, yy) => [s.qx + s.dx * along + s.nx * (0.15 + o), yy, s.qz + s.dz * along + s.nz * (0.15 + o)];
          const sl = Math.hypot(out, drop), ny = out / sl, nh = drop / sl;
          awnings.quad(P(-aw, 0, yt), P(aw, 0, yt), P(aw, out, yt - drop), P(-aw, out, yt - drop), s.nx * nh, ny, s.nz * nh, col, null);
          awnings.quad(P(-aw, out, yt - drop), P(aw, out, yt - drop), P(aw, out, yt - drop - 0.3), P(-aw, out, yt - drop - 0.3), s.nx, 0, s.nz, col, null);
          for (const e of [-1, 1]) awnings.tri(P(e * aw, 0, yt), P(e * aw, out, yt - drop), P(e * aw, 0, yt - drop), s.dx * e, 0, s.dz * e, col, null);
        }
      });
      if (awnings.count) {
        const am = new THREE.Mesh(awnings.geometry(), awningMat);
        am.castShadow = !lo;
        group.add(am);
        culls.push({ mesh: am, cx: tcx, cz: tcz, half: T / 2, far: 450 });
      }
      const tex = new THREE.CanvasTexture(c);
      tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 8;
      const mat = new THREE.MeshStandardMaterial({ map: tex, emissiveMap: tex, emissive: 0xffffff, emissiveIntensity: 0.05, roughness: 0.6 });
      glowMats.push({ mat, day: 0.05, night: 0.9 });
      disposables.push(tex, mat);
      const mesh = new THREE.Mesh(m.geometry(), mat);
      group.add(mesh);
      culls.push({ mesh, cx: tcx, cz: tcz, half: T / 2, far: 500 });
    }

    return { group, colliders, water, culls, disposables };
  }
  function step(budgetMs) {
    const t0 = performance.now();
    while (performance.now() - t0 < budgetMs) {
      if (!building) {
        const t = queue.shift();
        if (!t) return;
        if (tiles.get(t.key) !== t) continue;
        t.state = 'building';
        building = { t, it: buildTile(t) };
      }
      const r = building.it.next();
      if (r.done) { const t = building.t; building = null; if (tiles.get(t.key) === t) finish(t, r.value); }
    }
  }

  let lastFocus = null;
  function stream(fx, fz, budget) {
    lastFocus = [fx, fz];
    const wanted = want(fx, fz);
    for (const k of wanted) if (!tiles.has(k) && fetching < 3) request(k);
    for (const t of [...tiles.values()]) if (tileDist(t.key, fx, fz) > DROP) unload(t);
    queue.sort((a, b) => tileDist(a.key, fx, fz) - tileDist(b.key, fx, fz));
    step(budget);
  }

  /* --------------------------------------- sky, sun, light, bloom */
  // a bigger map: the haze starts further out than on the classic one
  const atmo = makeAtmosphere(scene, renderer, { quality, uniforms, fogNear: 800, fogFar: 8000, groundAt: RW.groundHeight });
  atmo.onTime.push((night) => {
    for (const m of roadMats) m.emissiveIntensity = night * 0.5;
    for (const g of glowMats) g.mat.emissiveIntensity = g.day + (g.night - g.day) * night;
    poolMat.emissiveIntensity = 0.2 + night * 1.2;
  });
  atmo.onUpdate.push((dt, time, camera, focus) => {
    waterNormals.offset.set((time * 0.01) % 1, (time * 0.007) % 1);
    const f = focus || camera.position;
    stream(f.x, f.z, lo ? 5 : 8);
    const cx = camera.position.x, cz = camera.position.z;
    for (const t of tiles.values()) {
      for (const c of t.culls) {
        const dx = Math.max(0, Math.abs(cx - c.cx) - c.half), dz = Math.max(0, Math.abs(cz - c.cz) - c.half);
        c.mesh.visible = dx * dx + dz * dz < c.far * c.far;
      }
    }
  });

  return {
    scene, uniforms, groundCanvas, sun: atmo.sun, streetLife: null,
    update: atmo.update, setTime: atmo.setTime, render: atmo.render, setSize: atmo.setSize,
    /* load everything round (x, z) now, for the title screen and tests */
    async preload(x, z, onProgress = () => {}) {
      for (;;) {
        stream(x, z, 40);
        const wanted = want(x, z);
        const done = wanted.filter((k) => tiles.get(k)?.state === 'ready' || tiles.get(k)?.state === 'failed').length;
        onProgress(done, wanted.length);
        if (done === wanted.length) return;
        await new Promise((r) => setTimeout(r, 0));
      }
    },
    stats: () => ({ tiles: [...tiles.values()].map((t) => `${t.key}:${t.state}`), colliders: RW.colliders.length, focus: lastFocus })
  };
}
