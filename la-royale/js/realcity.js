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
import { mergeGeometries as mergeGeos } from '../vendor/jsm/utils/BufferGeometryUtils.js';
import { makeAtmosphere } from './atmosphere.js';
import { facadeMaterial, withSurface, fadeNearCamera, buildFurniture, makeDetailTexture, makeWaterNormals, mulberry, shadeHex, resample, noiseFill, SURFACE_GLSL } from './render.js';
import { buildFlora } from './flora.js';
import { REAL_LANDMARKS, landmarkMaterials, Kit } from './reallandmarks.js';
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
  ['Hollywood Sign', 34.1341, -118.3215, 160], ['Griffith Observatory', 34.11824, -118.30037, 90], ['Greek Theatre', 34.11951, -118.29642, 80],
  ['Mount Hollywood', 34.1286, -118.3005, 120], ['Bronson Caves', 34.1214, -118.3133, 60], ['Lake Hollywood Reservoir', 34.1219, -118.3326, 220],
  ['Hollywood Bowl', 34.11276, -118.33890, 120], ['Runyon Canyon', 34.1105, -118.3500, 200],
  ['Capitol Records Building', 34.1032, -118.3262, 40], ['Hollywood & Vine', 34.1016, -118.3267, 50], ['Pantages Theatre', 34.10223, -118.32578, 40],
  ['TCL Chinese Theatre', 34.10195, -118.34097, 50], ['Dolby Theatre', 34.1022, -118.3393, 50], ['El Capitan Theatre', 34.10116, -118.34010, 30],
  ['Hollywood Roosevelt Hotel', 34.1016, -118.3417, 40], ['Egyptian Theatre', 34.10081, -118.33659, 30], ['Musso & Frank Grill', 34.10169, -118.33538, 25],
  ['Hollywood Walk of Fame', 34.1016, -118.3310, 60], ['Hollywood Palladium', 34.09851, -118.32446, 50], ['Cinerama Dome', 34.09767, -118.32802, 45],
  ['Hollywood High School', 34.0982, -118.3386, 80], ["Pink's Hot Dogs", 34.0838, -118.3443, 25], ['Paramount Pictures', 34.0838, -118.3200, 250],
  ['Hollywood Forever Cemetery', 34.0899, -118.3195, 200], ['Barnsdall Art Park', 34.1001, -118.2944, 90], ['Vista Theatre', 34.0977, -118.2873, 25],
  ['Sunset Junction', 34.0906, -118.2770, 50], ['Silver Lake Reservoir', 34.0925, -118.2625, 300], ['Echo Park Lake', 34.0730, -118.2606, 160],
  ['Dodger Stadium', 34.0739, -118.2400, 230], ['Philippe the Original', 34.0597, -118.2368, 25], ['Chinatown Central Plaza', 34.0636, -118.2383, 50],
  ['Union Station', 34.0562, -118.2365, 120], ['Olvera Street', 34.0574, -118.2378, 60], ['Los Angeles City Hall', 34.05374, -118.24277, 70],
  ['Grand Park', 34.0555, -118.2460, 120], ['Cathedral of Our Lady of the Angels', 34.05795, -118.24545, 70], ['Dorothy Chandler Pavilion', 34.05662, -118.24892, 60],
  ['Walt Disney Concert Hall', 34.05563, -118.24959, 70], ['The Broad', 34.0544, -118.2506, 45], ['Angels Flight', 34.0513, -118.2500, 30],
  ['Grand Central Market', 34.0508, -118.2489, 40], ['Bradbury Building', 34.0505, -118.2478, 25], ['Los Angeles Central Library', 34.0505, -118.2551, 60],
  ['US Bank Tower', 34.0511, -118.2541, 40], ['Wilshire Grand Center', 34.0502, -118.2601, 50], ['Millennium Biltmore Hotel', 34.0495, -118.2535, 40],
  ['Pershing Square', 34.0484, -118.2532, 80], ['The Last Bookstore', 34.0477, -118.2497, 25], ['Hotel Cecil', 34.0441, -118.2504, 25],
  ['Eastern Columbia Building', 34.0447, -118.2560, 25], ['Orpheum Theatre', 34.0436, -118.2562, 25], ['Ace Hotel', 34.0425, -118.2569, 25],
  ['Crypto.com Arena', 34.0430, -118.2673, 110], ['L.A. Live', 34.0447, -118.2655, 90], ['Los Angeles Convention Center', 34.0403, -118.2696, 180],
  ['Japanese American National Museum', 34.0498, -118.2389, 40], ['Sixth Street Viaduct', 34.0423, -118.2310, 200], ['MacArthur Park', 34.0577, -118.2770, 180],
  ['Bullocks Wilshire', 34.0617, -118.2848, 50], ['The Wiltern', 34.06145, -118.30886, 40], ['LACMA', 34.0639, -118.3592, 130],
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
  const int16 = (j) => new Int16Array(Uint8Array.from(atob(j.int16le), (ch) => ch.charCodeAt(0)).buffer);
  const [hbuf, ovBlob, skyline, arteries, far] = await Promise.all([
    M.height.file.endsWith('.json')
      ? get(M.height.file).then((j) => Uint8Array.from(atob(j.int16le), (ch) => ch.charCodeAt(0)).buffer)
      : get(M.height.file, 'buf'), get(M.overview.file, 'blob'),
    M.skyline ? get(M.skyline) : {}, M.arteries ? get(M.arteries) : [],
    M.far ? get(M.far.file).then(int16) : null
  ]);
  const overview = await createImageBitmap(ovBlob);
  return makeRealWorld(M, new Int16Array(hbuf), { overview, skyline, arteries, base, far });
}

/* Builds the world object from loaded data (split out so tests can feed it
 * data directly). */
export function makeRealWorld(M, heights, { overview = null, skyline = {}, arteries = [], base = '', far = null } = {}) {
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
    real: true, manifest: M, base, size, half, OX, OZ,
    // the country round the map, for the horizon
    far: far && M.far ? { data: far, n: M.far.n, step: M.far.step, x0: M.far.cx - OX - ((M.far.n - 1) / 2) * M.far.step, z0: M.far.cz - OZ - ((M.far.n - 1) / 2) * M.far.step } : null, gridN: N, cell: G, heights: H, overview, skyline,
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
  // the dingbat: a two- or three-storey stucco box of flats over an open carport (LA, 1950s–60s)
  if (win === 1 && !houseLike && b.h >= 6.5 && b.h <= 12 && area > 150 && area < 900 && hashOf(b.pts[0][0] * 1.7, b.pts[0][1] * 0.3) < 0.4) win = 5;
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
  const n = b.pts.length;
  // OSM maps many tall buildings as parts sharing an outline (a podium and a
  // tower): their walls would sit in the same plane and flicker. Each is drawn
  // a few centimetres out, more the taller it is, so the taller always shows.
  const pts = offsetRing(b.pts, Math.min(0.3, 0.015 + b.h * 0.002));
  const y0 = base + b.mh, top = base + b.h;
  const area = Math.abs(ringArea(b.pts));
  const seed = hashOf(b.pts[0][0] * 0.37, b.pts[0][1] * 0.71);
  const wallCol = new THREE.Color(st.wall), roofCol = new THREE.Color(st.roofCol);
  const parapet = st.roof === ROOF.flat && !st.house && b.h >= 6 && !opts.far ? 0.8 : 0;
  // storeys that fit the building's real height exactly
  const typical = st.win === 1 || st.win === 5 ? 3.1 : st.win === 3 ? 3.9 : st.win === 4 ? 6.5 : 3.8;
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
    // balconies (or the open walkways of a courtyard block) along one or two walls of the flats
    if ((st.win === 1 || st.win === 5) && b.h >= 8 && b.h <= 45 && hashOf(b.pts[0][1] * 0.9, b.pts[0][0] * 1.3) < 0.45) {
      const rail = new THREE.Color(st.trim).multiplyScalar(0.55);
      for (let i = 0; i < n; i++) {
        const [ax, az] = pts[i], [bx, bz] = pts[(i + 1) % n], l = Math.hypot(bx - ax, bz - az);
        if (l < 8 || hashOf(ax * 0.7 + i, az * 0.7) > 0.5) continue;
        const nx = (bz - az) / l, nz = -(bx - ax) / l, tx = (bx - ax) / l, tz = (bz - az) / l;
        const s0 = l * 0.08, s1 = l * 0.92, dep = 1.2;
        const P = (s, o, y) => [ax + tx * s + nx * o, y, az + tz * s + nz * o];
        for (let y = base + (st.win === 5 ? 2.8 : floor); y < top - 1.5; y += floor) {
          walls.quad(P(s0, 0, y), P(s1, 0, y), P(s1, dep, y), P(s0, dep, y), 0, 1, 0, trim, plain);
          walls.quad(P(s0, dep, y - 0.18), P(s1, dep, y - 0.18), P(s1, dep, y), P(s0, dep, y), nx, 0, nz, trim, plain);
          walls.quad(P(s0, 0, y - 0.18), P(s1, 0, y - 0.18), P(s1, dep, y - 0.18), P(s0, dep, y - 0.18), 0, -1, 0, trim, plain);
          walls.quad(P(s0, dep, y), P(s1, dep, y), P(s1, dep, y + 1.0), P(s0, dep, y + 1.0), nx, 0, nz, rail, plain);
          walls.quad(P(s0, dep, y), P(s1, dep, y), P(s1, dep, y + 1.0), P(s0, dep, y + 1.0), -nx, 0, -nz, rail, plain);
        }
      }
    }
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
  for (const [x, z] of b.pts) { cx += x / n; cz += z / n; }
  for (const [x, z] of b.pts) r = Math.max(r, Math.hypot(x - cx, z - cz));
  return { k: 2, x: cx, z: cz, r, y0, y1: top, pts: b.pts };
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

/* One square of the Walk of Fame: charcoal terrazzo, a coral-pink star with a brass rim,
 * the brass name strip and the round emblem below it. */
function walkOfFameTex() {
  return canvasTex('star', 256, 256, (ctx, w) => {
    noiseFill(ctx, w, w, '#3a3735', 22, 31);
    ctx.fillStyle = 'rgba(200,190,180,0.25)';
    for (let i = 0; i < 400; i++) ctx.fillRect(Math.random() * w, Math.random() * w, 2, 2);   // the terrazzo chips
    ctx.strokeStyle = '#8a8580'; ctx.lineWidth = 4; ctx.strokeRect(2, 2, w - 4, w - 4);
    const star = (r0, r1) => { ctx.beginPath(); for (let k = 0; k < 10; k++) { const a = -Math.PI / 2 + (k * Math.PI) / 5, r = k % 2 ? r1 : r0; ctx.lineTo(w / 2 + Math.cos(a) * r, w / 2 + Math.sin(a) * r); } ctx.closePath(); };
    star(118, 47); ctx.fillStyle = '#c7a24a'; ctx.fill();            // brass rim
    star(108, 42); ctx.fillStyle = '#d98c8c'; ctx.fill();            // coral pink
    ctx.fillStyle = '#c7a24a'; ctx.fillRect(w / 2 - 52, w / 2 - 22, 104, 12); ctx.fillRect(w / 2 - 40, w / 2 - 4, 80, 10);
    ctx.beginPath(); ctx.arc(w / 2, w / 2 + 34, 14, 0, 6.283); ctx.fill();
  });
}

/* Eight murals, painted procedurally in LA's styles: a Pacific sunset through the palms, the
 * San Gabriels at dusk, waves, Chicano-style sun and zigzags, bougainvillea, Memphis shapes,
 * a postcard greeting, a hummingbird among flowers. */
function muralAtlas() {
  return canvasTex('murals', 2048, 1024, (ctx) => {
    const rnd = mulberry(1977);
    const cell = (k, draw) => { ctx.save(); ctx.translate((k % 4) * 512, Math.floor(k / 4) * 512); ctx.beginPath(); ctx.rect(0, 0, 512, 512); ctx.clip(); draw(); ctx.restore(); };
    const grad = (stops) => { const g = ctx.createLinearGradient(0, 0, 0, 512); stops.forEach(([o, c]) => g.addColorStop(o, c)); ctx.fillStyle = g; ctx.fillRect(0, 0, 512, 512); };
    const palm = (x, y, h, col) => {
      ctx.strokeStyle = col; ctx.fillStyle = col; ctx.lineWidth = 9;
      ctx.beginPath(); ctx.moveTo(x, y); ctx.quadraticCurveTo(x + 18, y - h / 2, x + 10, y - h); ctx.stroke();
      for (let f = 0; f < 9; f++) { const a = -Math.PI / 2 + (f - 4) * 0.42; ctx.beginPath(); ctx.ellipse(x + 10 + Math.cos(a) * 34, y - h + Math.sin(a) * 26 + 10, 40, 8, a, 0, 6.283); ctx.fill(); }
    };
    cell(0, () => {
      grad([[0, '#2b1055'], [0.45, '#d7396b'], [0.75, '#ff9d3c'], [1, '#ffd36b']]);
      ctx.fillStyle = '#ffe49a'; ctx.beginPath(); ctx.arc(256, 330, 90, 0, 6.283); ctx.fill();
      ctx.fillStyle = '#1a3d6b'; ctx.fillRect(0, 380, 512, 132);
      for (let i = 0; i < 6; i++) { ctx.fillStyle = `rgba(255,220,150,${0.5 - i * 0.07})`; ctx.fillRect(170 + i * 8, 392 + i * 16, 172 - i * 16, 5); }
      palm(70, 512, 330, '#140a1e'); palm(420, 512, 380, '#140a1e'); palm(470, 512, 250, '#140a1e');
    });
    cell(1, () => {
      grad([[0, '#f6b26b'], [0.5, '#e38b9e'], [1, '#6c5b9e']]);
      for (const [base, col, amp] of [[300, '#4b3f72', 120], [360, '#352b55', 90], [420, '#22203a', 60]]) {
        ctx.fillStyle = col; ctx.beginPath(); ctx.moveTo(0, 512);
        for (let x = 0; x <= 512; x += 16) ctx.lineTo(x, base - Math.abs(Math.sin(x * 0.013 + base) * amp) - rnd() * 18);
        ctx.lineTo(512, 512); ctx.fill();
      }
      ctx.fillStyle = '#fff6e0'; ctx.font = 'bold 64px Georgia, serif'; ctx.textAlign = 'center'; ctx.fillText('San Gabriel', 256, 110);
    });
    cell(2, () => {
      grad([[0, '#8fd3e8'], [1, '#1b6ca8']]);
      for (let row = 0; row < 7; row++) for (let x = -40; x < 560; x += 110) {
        const y = 120 + row * 60;
        ctx.strokeStyle = row % 2 ? '#ffffff' : '#0d4f86'; ctx.lineWidth = 12;
        ctx.beginPath(); ctx.arc(x + (row % 2) * 55, y, 40, Math.PI, Math.PI * 1.9); ctx.stroke();
      }
    });
    cell(3, () => {
      ctx.fillStyle = '#1f4e79'; ctx.fillRect(0, 0, 512, 512);
      for (let i = 0; i < 24; i++) { ctx.fillStyle = i % 2 ? '#f2c14e' : '#e4572e'; ctx.beginPath(); ctx.moveTo(256, 256); ctx.arc(256, 256, 260, (i / 24) * 6.283, ((i + 1) / 24) * 6.283); ctx.fill(); }
      ctx.fillStyle = '#f9e076'; ctx.beginPath(); ctx.arc(256, 256, 95, 0, 6.283); ctx.fill();
      ctx.fillStyle = '#3b7d4f';
      for (let y = 0; y < 512; y += 64) for (let x = 0; x < 512; x += 64) if ((x + y) % 128 === 0) { ctx.beginPath(); ctx.moveTo(x, y + 64); ctx.lineTo(x + 32, y + 32); ctx.lineTo(x + 64, y + 64); ctx.fill(); }
      ctx.fillStyle = '#7b2d26'; ctx.beginPath(); ctx.arc(256, 256, 40, 0, 6.283); ctx.fill();
    });
    cell(4, () => {
      ctx.fillStyle = '#f4eee2'; ctx.fillRect(0, 0, 512, 512);
      for (let i = 0; i < 70; i++) {
        const x = rnd() * 512, y = rnd() * 512, r = 18 + rnd() * 30;
        ctx.fillStyle = '#2f6b33'; ctx.beginPath(); ctx.ellipse(x, y, r * 1.2, r * 0.6, rnd() * 3, 0, 6.283); ctx.fill();
        ctx.fillStyle = rnd() < 0.7 ? '#d6337a' : '#f07ab2';
        for (let p = 0; p < 3; p++) { ctx.beginPath(); ctx.arc(x + (rnd() - 0.5) * r, y + (rnd() - 0.5) * r, r * 0.45, 0, 6.283); ctx.fill(); }
      }
    });
    cell(5, () => {
      ctx.fillStyle = '#fbe9d0'; ctx.fillRect(0, 0, 512, 512);
      const cols = ['#ff5e7e', '#00b2a9', '#ffc845', '#2d2a6e', '#7ed957'];
      for (let i = 0; i < 26; i++) {
        ctx.fillStyle = cols[i % cols.length]; const x = rnd() * 512, y = rnd() * 512, s = 30 + rnd() * 80;
        const t = i % 4;
        if (t === 0) { ctx.beginPath(); ctx.arc(x, y, s / 2, 0, 6.283); ctx.fill(); }
        else if (t === 1) { ctx.save(); ctx.translate(x, y); ctx.rotate(rnd() * 3); ctx.fillRect(-s / 2, -s / 6, s, s / 3); ctx.restore(); }
        else if (t === 2) { ctx.beginPath(); ctx.moveTo(x, y - s / 2); ctx.lineTo(x + s / 2, y + s / 2); ctx.lineTo(x - s / 2, y + s / 2); ctx.fill(); }
        else { ctx.strokeStyle = cols[(i + 2) % cols.length]; ctx.lineWidth = 8; ctx.beginPath(); for (let k = 0; k < 6; k++) ctx.lineTo(x + k * 18, y + (k % 2 ? 18 : -18)); ctx.stroke(); }
      }
    });
    cell(6, () => {
      grad([[0, '#79c7e3'], [1, '#e9f6fb']]);
      ctx.fillStyle = '#ffcc33'; ctx.beginPath(); ctx.arc(420, 90, 50, 0, 6.283); ctx.fill();
      ctx.font = 'italic bold 54px Georgia, serif'; ctx.fillStyle = '#c0392b'; ctx.textAlign = 'center'; ctx.fillText('Greetings from', 256, 170);
      ctx.font = 'bold 118px Impact, Arial Black, sans-serif'; ctx.lineWidth = 10; ctx.strokeStyle = '#1d3557'; ctx.strokeText('L.A.', 256, 320); ctx.fillStyle = '#f4a261'; ctx.fillText('L.A.', 256, 320);
      palm(60, 512, 220, '#2a6f4a'); palm(460, 512, 260, '#2a6f4a');
    });
    cell(7, () => {
      ctx.fillStyle = '#163a2b'; ctx.fillRect(0, 0, 512, 512);
      for (let i = 0; i < 40; i++) { ctx.fillStyle = ['#ff7f50', '#ffd166', '#ef476f', '#06d6a0'][i % 4]; ctx.beginPath(); ctx.arc(rnd() * 512, rnd() * 512, 10 + rnd() * 18, 0, 6.283); ctx.fill(); }
      ctx.save(); ctx.translate(256, 250);
      ctx.fillStyle = '#1e9e8a'; ctx.beginPath(); ctx.ellipse(0, 0, 70, 34, -0.4, 0, 6.283); ctx.fill();
      ctx.fillStyle = '#c2185b'; ctx.beginPath(); ctx.ellipse(-40, 10, 26, 22, 0, 0, 6.283); ctx.fill();
      ctx.fillStyle = '#2bc0a9'; ctx.beginPath(); ctx.ellipse(20, -60, 110, 26, -1.1, 0, 6.283); ctx.fill(); ctx.beginPath(); ctx.ellipse(40, 40, 100, 22, 0.9, 0, 6.283); ctx.fill();
      ctx.strokeStyle = '#111'; ctx.lineWidth = 5; ctx.beginPath(); ctx.moveTo(-60, 20); ctx.lineTo(-150, 60); ctx.stroke();
      ctx.restore();
    });
  });
}

/* Graffiti: tags and throw-ups in bubble letters, on transparent. */
function graffitiAtlas() {
  return canvasTex('graffiti', 2048, 1024, (ctx) => {
    ctx.clearRect(0, 0, 2048, 1024);
    const rnd = mulberry(213);
    const words = ['LA', 'SK8', 'RIOT', 'EKO', 'KRSH', 'NOVA', 'ZEPH', 'MSK'];
    const fills = ['#ff3d7f', '#3ddcff', '#c6ff3d', '#ffb43d', '#ffffff', '#b06bff', '#ff5a36', '#4dff9c'];
    words.forEach((w, k) => {
      ctx.save(); ctx.translate((k % 4) * 512 + 256, Math.floor(k / 4) * 512 + 280); ctx.rotate((rnd() - 0.5) * 0.3);
      ctx.font = `bold ${k % 2 ? 150 : 190}px "Arial Black", Impact, sans-serif`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.lineJoin = 'round'; ctx.lineWidth = 26; ctx.strokeStyle = '#111'; ctx.strokeText(w, 0, 0);
      ctx.lineWidth = 12; ctx.strokeStyle = fills[(k + 3) % 8]; ctx.strokeText(w, 0, 0);
      ctx.fillStyle = fills[k]; ctx.fillText(w, 0, 0);
      ctx.fillStyle = fills[k]; for (let d = 0; d < 6; d++) ctx.fillRect(-180 + rnd() * 360, 60, 5, 20 + rnd() * 70);    // drips
      ctx.restore();
    });
  });
}

/* The street-name blades of one tile: white names on LA blue (Hollywood) or green. */
function streetSignAtlas(names) {
  const c = document.createElement('canvas');
  c.width = 512; c.height = 32 * 64;
  const ctx = c.getContext('2d');
  names.forEach(({ name, hwd }, i) => {
    const y = i * 64;
    ctx.fillStyle = hwd ? '#1d4f9c' : '#1f6b3f'; ctx.fillRect(0, y, 512, 64);
    ctx.strokeStyle = '#f2f2f2'; ctx.lineWidth = 3; ctx.strokeRect(4, y + 4, 504, 56);
    ctx.fillStyle = '#ffffff'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    let size = 44; ctx.font = `bold ${size}px Helvetica, Arial, sans-serif`;
    while (ctx.measureText(name).width > 470 && size > 16) { size -= 2; ctx.font = `bold ${size}px Helvetica, Arial, sans-serif`; }
    ctx.fillText(name, 256, y + 34);
    if (hwd) { ctx.fillStyle = '#ffd34a'; ctx.font = 'bold 14px Helvetica, Arial'; ctx.fillText('★ HOLLYWOOD', 70, y + 14); }
  });
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8;
  return t;
}

/* Eight billboards, all made up: a summer movie, a juice bar, an injury lawyer (it's LA),
 * a surf school, a tequila, an energy drink, a streaming show and a radio station. */
function adAtlas() {
  return canvasTex('ads', 2048, 1024, (ctx) => {
    const cell = (k, draw) => { ctx.save(); ctx.translate((k % 4) * 512, Math.floor(k / 4) * 512); ctx.beginPath(); ctx.rect(0, 0, 512, 512); ctx.clip(); draw(); ctx.restore(); };
    const big = (t, y, size, col, font = 'Impact, "Arial Black", sans-serif') => { ctx.font = `bold ${size}px ${font}`; ctx.fillStyle = col; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(t, 256, y); };
    cell(0, () => { const g = ctx.createLinearGradient(0, 0, 0, 512); g.addColorStop(0, '#0b0f2b'); g.addColorStop(1, '#c2410c'); ctx.fillStyle = g; ctx.fillRect(0, 0, 512, 512);
      ctx.fillStyle = '#ffb703'; ctx.beginPath(); ctx.arc(256, 330, 120, 0, 6.283); ctx.fill(); big('SOLAR FLARE', 120, 76, '#ffffff'); big('IN THEATERS FRIDAY', 450, 34, '#ffe8c2', 'Helvetica, Arial'); });
    cell(1, () => { ctx.fillStyle = '#9be15d'; ctx.fillRect(0, 0, 512, 512); ctx.fillStyle = '#ff6b6b'; ctx.beginPath(); ctx.arc(380, 260, 140, 0, 6.283); ctx.fill(); big('GREEN', 170, 96, '#0b3d20'); big('& GOLD', 290, 72, '#0b3d20'); big('JUICE BAR', 420, 40, '#0b3d20', 'Helvetica, Arial'); });
    cell(2, () => { ctx.fillStyle = '#ffd400'; ctx.fillRect(0, 0, 512, 512); big('HURT?', 130, 120, '#111111'); big('CALL', 270, 70, '#111111'); big('1-800-555-0199', 370, 52, '#c1121f', 'Helvetica, Arial'); });
    cell(3, () => { const g = ctx.createLinearGradient(0, 0, 0, 512); g.addColorStop(0, '#48cae4'); g.addColorStop(1, '#023e8a'); ctx.fillStyle = g; ctx.fillRect(0, 0, 512, 512);
      ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 16; ctx.beginPath(); ctx.arc(150, 420, 140, Math.PI * 1.1, Math.PI * 1.9); ctx.stroke(); big('VENICE', 150, 92, '#ffffff'); big('SURF SCHOOL', 250, 52, '#ffffff'); });
    cell(4, () => { ctx.fillStyle = '#1b1b1b'; ctx.fillRect(0, 0, 512, 512); ctx.fillStyle = '#c9a227'; ctx.fillRect(210, 160, 92, 250); ctx.fillRect(235, 110, 42, 60); big('CASA', 80, 70, '#c9a227'); big('LUNA', 460, 60, '#c9a227'); });
    cell(5, () => { ctx.fillStyle = '#7209b7'; ctx.fillRect(0, 0, 512, 512); ctx.fillStyle = '#4cc9f0'; ctx.beginPath(); ctx.moveTo(280, 40); ctx.lineTo(170, 280); ctx.lineTo(260, 280); ctx.lineTo(220, 480); ctx.lineTo(350, 220); ctx.lineTo(260, 220); ctx.fill(); big('VOLT', 120, 90, '#ffffff'); });
    cell(6, () => { ctx.fillStyle = '#e63946'; ctx.fillRect(0, 0, 512, 512); big('MIDNIGHT', 180, 86, '#ffffff'); big('ON MULHOLLAND', 280, 50, '#ffffff'); big('NOW STREAMING', 420, 36, '#ffd6d9', 'Helvetica, Arial'); });
    cell(7, () => { ctx.fillStyle = '#ffffff'; ctx.fillRect(0, 0, 512, 512); ctx.fillStyle = '#ff006e'; ctx.fillRect(0, 380, 512, 132); big('KMZR', 170, 140, '#3a0ca3'); big('104.3 FM', 445, 60, '#ffffff'); });
  });
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

/* The country round the map out to 64 km, from coarse elevation: the
 * San Gabriels (Mount Baldy, 3,068 m), the Santa Monica Mountains, the
 * Verdugos, Palos Verdes, the basin and the Pacific. Coloured by height —
 * the city spreads over everything low and flat, chaparral on the slopes,
 * pines up high, bare rock on the peaks — and hazed by distance like the
 * real view across the basin. At night the flat land glows with streetlights. */
function buildHorizon(RW, scene, uniforms, lo) {
  if (!RW.far) return null;
  const F = RW.far, n = F.n, keep = lo ? 2 : 1, m = Math.floor((n - 1) / keep) + 1;
  const hAt = (i, j) => F.data[Math.min(n - 1, j) * n + Math.min(n - 1, i)];
  const pos = new Float32Array(m * m * 3), col = new Float32Array(m * m * 3), urb = new Float32Array(m * m);
  const C = (hex) => new THREE.Color(hex);
  const city = C('#b3aea4'), sand = C('#d8c8a0'), chap = C('#6e7050'), dry = C('#8f7f5c'), pine = C('#4a573d'), rock = C('#8e877c');
  const c = new THREE.Color();
  for (let j = 0; j < m; j++) for (let i = 0; i < m; i++) {
    const gi = i * keep, gj = j * keep, k = j * m + i;
    let h = hAt(gi, gj);
    const x = F.x0 + gi * F.step, z = F.z0 + gj * F.step;
    const slope = Math.hypot(hAt(gi + 1, gj) - hAt(Math.max(0, gi - 1), gj), hAt(gi, gj + 1) - hAt(gi, Math.max(0, gj - 1))) / (2 * F.step);
    const noise = hashOf(x * 0.001, z * 0.001);
    let u = 0;
    if (h <= 0.5) { h = -25; c.copy(sand); }
    else if (h < 12 && slope < 0.02) c.copy(sand);
    else if (h < 450 && slope < 0.09) { c.copy(city).multiplyScalar(0.9 + noise * 0.2); u = 1; }
    else if (h < 1700) c.copy(noise < 0.5 ? chap : dry).lerp(rock, Math.min(1, slope * 0.8));
    else if (h < 2500) c.copy(pine).lerp(rock, Math.min(1, slope));
    else c.copy(rock);
    // under the detailed map: tuck it out of sight
    if (Math.abs(x) < RW.half - F.step && Math.abs(z) < RW.half - F.step) h -= 60;
    pos[k * 3] = x; pos[k * 3 + 1] = h; pos[k * 3 + 2] = z;
    col[k * 3] = c.r; col[k * 3 + 1] = c.g; col[k * 3 + 2] = c.b;
    urb[k] = u;
  }
  const idx = new Uint32Array((m - 1) * (m - 1) * 6);
  let q = 0;
  for (let j = 0; j < m - 1; j++) for (let i = 0; i < m - 1; i++) {
    const a = j * m + i, d = a + 1, b = a + m, cc = b + 1;
    idx[q++] = a; idx[q++] = b; idx[q++] = d; idx[q++] = b; idx[q++] = cc; idx[q++] = d;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.setAttribute('aUrban', new THREE.BufferAttribute(urb, 1));
  g.setIndex(new THREE.BufferAttribute(idx, 1));
  g.computeVertexNormals();
  const haze = { value: new THREE.Color(0xcfd3d0) }, distance = new THREE.Color().setRGB(0.74, 0.79, 0.84, THREE.SRGBColorSpace).convertLinearToSRGB();
  const hazed = (m0, urban) => {
    m0.fog = false;
    m0.onBeforeCompile = (sh) => {
      sh.uniforms.uHaze = haze; sh.uniforms.uNight = uniforms.uNight;
      sh.vertexShader = sh.vertexShader
        .replace('#include <common>', `#include <common>\nvarying vec3 vHzW; ${urban ? 'attribute float aUrban; varying float vUrb;' : ''}`)
        .replace('#include <project_vertex>', `#include <project_vertex>\nvHzW = (modelMatrix * vec4(transformed, 1.0)).xyz; ${urban ? 'vUrb = aUrban;' : ''}`);
      sh.fragmentShader = sh.fragmentShader
        .replace('#include <common>', `#include <common>\nuniform vec3 uHaze; uniform float uNight; varying vec3 vHzW; ${urban ? 'varying float vUrb;' : ''}
          float hzh(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }`)
        .replace('#include <dithering_fragment>', `#include <dithering_fragment>
          float hzd = distance(vHzW, cameraPosition);
          ${urban ? `// the city at night: a carpet of sodium streetlights on the grid
          vec2 cell = floor(vHzW.xz / 90.0);
          float lamp = step(0.55, hzh(cell)) * vUrb * uNight;
          gl_FragColor.rgb *= 1.0 - vUrb * (0.18 * hzh(floor(vHzW.xz / 140.0)) - 0.06);   // blocks, lots and roofs
          gl_FragColor.rgb += lamp * vec3(1.0, 0.62, 0.3) * (0.6 + 0.8 * hzh(cell + 7.0));` : ''}
          // aerial perspective: the further, the more it fades into the haze (but the peaks still show)
          gl_FragColor.rgb = mix(gl_FragColor.rgb, uHaze, clamp(1.0 - exp(-hzd / 16000.0), 0.0, 0.88) * (1.0 - 0.5 * uNight));`);
    };
    m0.customProgramCacheKey = () => `horizon:${urban}`;
    return m0;
  };
  const land = new THREE.Mesh(g, hazed(new THREE.MeshLambertMaterial({ vertexColors: true }), true));
  land.frustumCulled = false;
  scene.add(land);
  const sea = new THREE.Mesh(new THREE.PlaneGeometry(400000, 400000), hazed(new THREE.MeshStandardMaterial({ color: 0x24627e, roughness: 0.25, metalness: 0.1 }), false));
  sea.rotation.x = -Math.PI / 2; sea.position.y = 0;
  scene.add(sea);
  // far off, the haze is bluer than the smog close in (the light scattered by the whole depth of air)
  // (mixed in after the tone mapping, so in display colour)
  const update = (fog) => haze.value.copy(fog).convertLinearToSRGB().lerp(distance, 0.5);
  return { land, sea, haze, update };
}

/* ------------------------------------------------------- street paint */

/* The road markings atlas: a solid cell for stripes, then STOP, a left-turn
 * arrow, a straight arrow and BUS ONLY, white on transparent (each 512×128,
 * read along the road: the top of the cell is "ahead"). */
function paintAtlas() {
  return canvasTex('paint', 512, 1024, (ctx, w) => {
    ctx.clearRect(0, 0, w, 1024);
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, w, 128);
    const cell = (k, draw) => { ctx.save(); ctx.translate(0, k * 128); draw(); ctx.restore(); };
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.font = 'bold 118px Helvetica, Arial, sans-serif';
    cell(1, () => { ctx.save(); ctx.translate(256, 64); ctx.scale(1, 1); ctx.fillText('STOP', 0, 4); ctx.restore(); });
    cell(2, () => {           // left-turn arrow: up the stem, then hooked left
      ctx.fillRect(300, 50, 34, 78); ctx.fillRect(220, 40, 114, 30);
      ctx.beginPath(); ctx.moveTo(170, 55); ctx.lineTo(232, 14); ctx.lineTo(232, 96); ctx.fill();
    });
    cell(3, () => { ctx.fillRect(240, 40, 34, 88); ctx.beginPath(); ctx.moveTo(257, 0); ctx.lineTo(206, 50); ctx.lineTo(308, 50); ctx.fill(); });
    cell(4, () => { ctx.font = 'bold 62px Helvetica, Arial, sans-serif'; ctx.fillText('BUS', 256, 34); ctx.fillText('ONLY', 256, 98); });
  });
}
const CELL_V = (k) => [1 - (k + 1) / 8, 1 - k / 8];      // atlas cell k → [v0, v1]

/* Junctions, crosswalks, stop lines, arrows, bus lanes and the coloured
 * kerbs of LA (red: no stopping, yellow: loading, white: drop-off, green:
 * short stay, blue: disabled) for one tile's roads. */
function streetPaint(roads, gh, onOtherRoad, yOff, busLane) {
  const paint = new Mesh(), curbs = new Mesh();
  const white = new THREE.Color(0xf2f1ea), red = new THREE.Color(0xb3261e), busRed = new THREE.Color(0x9c2a22);
  const CURB = { grey: new THREE.Color(0xc9c5bb), red: new THREE.Color(0xc0392b), yellow: new THREE.Color(0xe0b63a), white: new THREE.Color(0xf0eee6), green: new THREE.Color(0x3f8f4e), blue: new THREE.Color(0x2e5fa8) };
  const drivable = (r) => r.cls <= RCLASS.service;
  const key = (p) => `${Math.round(p[0] * 5)},${Math.round(p[1] * 5)}`;
  // every vertex shared by roads, with the legs that leave it
  const verts = new Map();
  roads.forEach((r) => {
    if (!drivable(r)) return;
    r.pts.forEach((p, i) => {
      const k = key(p);
      let v = verts.get(k);
      if (!v) verts.set(k, (v = { p, legs: [] }));
      if (i > 0) v.legs.push({ r, i, dir: -1 });
      if (i < r.pts.length - 1) v.legs.push({ r, i, dir: 1 });
    });
  });
  // a point s metres along a road from vertex i, going dir
  const along = (r, i, dir, s) => {
    let j = i, x = r.pts[i][0], z = r.pts[i][1];
    for (;;) {
      const n = j + dir;
      if (n < 0 || n >= r.pts.length) { const [ax, az] = r.pts[j - dir] || r.pts[j], l = Math.hypot(x - ax, z - az) || 1; return [x, z, (x - ax) / l, (z - az) / l]; }
      const [bx, bz] = r.pts[n], l = Math.hypot(bx - x, bz - z);
      if (l >= s) { const tx = (bx - x) / (l || 1), tz = (bz - z) / (l || 1); return [x + tx * s, z + tz * s, tx, tz]; }
      s -= l; x = bx; z = bz; j = n;
    }
  };
  const lay = (cx, cz, tx, tz, len, wid, col, cell, yo) => {
    // a decal centred on (cx, cz), len along (tx, tz), wid across; the atlas cell's top faces +t
    const nx = -tz, nz = tx, hl = len / 2, hw = wid / 2;
    const P = (a, b) => { const x = cx + tx * a + nx * b, z = cz + tz * a + nz * b; return [x, gh(x, z) + yo, z]; };
    const [v0, v1] = CELL_V(cell);
    paint.quad(P(-hl, hw), P(-hl, -hw), P(hl, -hw), P(hl, hw), 0, 1, 0, col, null, [[0, v0], [1, v0], [1, v1], [0, v1]]);
  };
  /* a lane arrow pointing along (tx, tz): a stem and a head, the turn arrow hooked left */
  const arrow = (cx, cz, tx, tz, left, yo) => {
    const lx = tz, lz = -tx;                 // to the driver's left
    const P = (a, b) => { const x = cx + tx * a + lx * b, z = cz + tz * a + lz * b; return [x, gh(x, z) + yo, z]; };
    const v = CELL_V(0), uv = [[0, v[0]], [1, v[0]], [1, v[1]], [0, v[1]]];
    paint.quad(P(-3, -0.12), P(-3, 0.12), P(left ? 0.4 : 0.6, 0.12), P(left ? 0.4 : 0.6, -0.12), 0, 1, 0, white, null, uv);
    if (left) {
      paint.quad(P(0.1, -0.12), P(0.1, 0.9), P(0.4, 0.9), P(0.4, -0.12), 0, 1, 0, white, null, uv);
      paint.tri(P(-0.35, 0.9), P(0.85, 0.9), P(0.25, 1.75), 0, 1, 0, white, null, [uv[0], uv[1], uv[2]]);
    } else paint.tri(P(0.6, -0.5), P(0.6, 0.5), P(2.0, 0), 0, 1, 0, white, null, [uv[0], uv[1], uv[2]]);
  };
  const junctionRoads = new Set();
  for (const v of verts.values()) {
    if (v.legs.length < 3) continue;
    const minCls = Math.min(...v.legs.map((l) => l.r.cls));
    const reach = Math.max(...v.legs.map((l) => l.r.w)) / 2;
    for (const leg of v.legs) {
      const r = leg.r, w = r.w, yo = (yOff[r.cls] ?? 0.1) + 0.012;
      junctionRoads.add(r);
      if (r.cls === RCLASS.service || w < 6) continue;
      const set = reach + 1.2;
      // a continental crosswalk: white bars along the road, across its width
      if (minCls <= RCLASS.tertiary && r.cls !== RCLASS.motorway && r.cls !== RCLASS.trunk) {
        const [x, z, tx, tz] = along(r, leg.i, leg.dir, set + 1.6);
        const nx = -tz, nz = tx;
        for (let b = -w / 2 + 0.6; b < w / 2 - 0.3; b += 1.2) lay(x + nx * b, z + nz * b, tx, tz, 3, 0.6, white, 0, yo);
      }
      // traffic comes in along −t; its right-hand half gets the stop line
      const [sx, sz, tx, tz] = along(r, leg.i, leg.dir, set + 4.0);
      const rx = tz, rz = -tx;
      if (r.cls > minCls || r.cls === RCLASS.residential) {
        lay(sx + rx * w / 4, sz + rz * w / 4, tx, tz, 0.5, w / 2 - 0.4, white, 0, yo);
        if (r.cls >= RCLASS.tertiary) { const [qx, qz] = along(r, leg.i, leg.dir, set + 7.5); lay(qx + rx * w / 4, qz + rz * w / 4, -tx, -tz, 4.2, Math.min(3, w / 2 - 0.6), white, 1, yo); }
      } else if (!r.oneway && w >= 18) {
        // the inside lane: a left-turn arrow; outside: straight on
        const [ax, az] = along(r, leg.i, leg.dir, set + 14);
        arrow(ax + rx * 1.9, az + rz * 1.9, -tx, -tz, true, yo);
        arrow(ax + rx * (w / 2 - 2.2), az + rz * (w / 2 - 2.2), -tx, -tz, false, yo);
      }
    }
  }
  // the red bus lanes (Wilshire's, the real ones), with BUS ONLY every 80 m
  for (const r of roads) {
    if (!busLane(r)) continue;
    const yo = (yOff[r.cls] ?? 0.1) + 0.008;
    const pts = resample(r.pts, 6);
    for (let i = 0; i < pts.length - 1; i++) {
      const [ax, az] = pts[i], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1, tx = (bx - ax) / l, tz = (bz - az) / l;
      for (const s of r.oneway ? [1] : [-1, 1]) {      // (one-way halves of the boulevard: the kerb side only)
        const off = s * (r.w / 2 - 2.3), mx = (ax + bx) / 2 - tz * off, mz = (az + bz) / 2 + tx * off;
        if (onOtherRoad(mx, mz, r.id)) continue;
        lay(mx, mz, tx, tz, l + 0.05, 3.4, busRed, 0, yo);
        if (i % 13 === 6) lay(mx, mz, tx * -s, tz * -s, 5, 2.8, white, 4, yo + 0.004);
      }
    }
  }
  // the kerbs: a low concrete edge, painted where you can't park
  for (const r of roads) {
    if (r.cls < RCLASS.primary || r.cls > RCLASS.residential) continue;
    const yo = (yOff[r.cls] ?? 0.1);
    const pts = resample(r.pts, 3);
    const ends = [pts[0], pts[pts.length - 1]].filter((p) => (verts.get(key(p))?.legs.length || 0) >= 3);
    for (let i = 0; i < pts.length - 1; i++) {
      const [ax, az] = pts[i], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1, tx = (bx - ax) / l, tz = (bz - az) / l;
      for (const s of [-1, 1]) {
        const ox = -tz * s * r.w / 2, oz = tx * s * r.w / 2;
        const A = [ax + ox, az + oz], B = [bx + ox, bz + oz];
        if (onOtherRoad(A[0], A[1], r.id) || onOtherRoad(B[0], B[1], r.id)) continue;
        const dEnd = Math.min(...ends.map((p) => Math.hypot(p[0] - ax, p[1] - az)), 99);
        const run = Math.floor(i / 7), hh = hashOf(r.pts[0][0] + run * 7.3 + s, r.pts[0][1] - run * 3.1);
        const col = dEnd < 14 ? CURB.red : r.cls <= RCLASS.secondary ? (hh < 0.07 ? CURB.yellow : hh < 0.1 ? CURB.green : hh < 0.12 ? CURB.white : hh < 0.13 ? CURB.blue : hh < 0.2 ? CURB.red : CURB.grey) : (hh < 0.04 ? CURB.red : CURB.grey);
        const ya = gh(...A) + yo, yb = gh(...B) + yo, nx = -tz * s, nz = tx * s, k = 0.16, ins = 0.18;
        const A2 = [A[0] + nx * ins, A[1] + nz * ins], B2 = [B[0] + nx * ins, B[1] + nz * ins];
        curbs.quad([A[0], ya - 0.02, A[1]], [B[0], yb - 0.02, B[1]], [B[0], yb + k, B[1]], [A[0], ya + k, A[1]], -nx, 0, -nz, col, null);
        curbs.quad([A[0], ya + k, A[1]], [B[0], yb + k, B[1]], [B2[0], yb + k, B2[1]], [A2[0], ya + k, A2[1]], 0, 1, 0, col, null);
      }
    }
  }
  const junctions = [...verts.values()].filter((v) => v.legs.length >= 3).map((v) => ({
    x: v.p[0], z: v.p[1], legs: v.legs, minCls: Math.min(...v.legs.map((l) => l.r.cls)),
    reach: Math.max(...v.legs.map((l) => l.r.w)) / 2, dirs: v.legs.map((l) => along(l.r, l.i, l.dir, 6))
  }));
  return { paint, curbs, junctions, along };
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
  const adMat = new THREE.MeshStandardMaterial({ map: adAtlas(), emissiveMap: adAtlas(), emissive: 0xffffff, emissiveIntensity: 0.05, roughness: 0.55 });
  glowMats.push({ mat: adMat, day: 0.05, night: 0.7 });
  const POST = new THREE.CylinderGeometry(0.05, 0.06, 3.9, 8).translate(0, 1.95, 0);
  const METER = mergeGeos([new THREE.CylinderGeometry(0.04, 0.04, 1.15, 6).translate(0, 0.58, 0), new THREE.BoxGeometry(0.22, 0.34, 0.16).translate(0, 1.3, 0)]);
  const muralMat = new THREE.MeshStandardMaterial({ map: muralAtlas(), roughness: 0.85 });
  const tagMat = new THREE.MeshStandardMaterial({ map: graffitiAtlas(), roughness: 0.8, transparent: true, alphaTest: 0.08, depthWrite: false });
  const STAR = new THREE.PlaneGeometry(1.0, 1.0).rotateX(-Math.PI / 2);
  const starMat = new THREE.MeshStandardMaterial({ map: walkOfFameTex(), roughness: 0.35, metalness: 0.1 });
  const LM = landmarkMaterials(withSurface);
  glowMats.push({ mat: LM.lamp, day: 0.2, night: 2.5 });
  const tileKeyAt = (x, z) => `${Math.floor((x + RW.OX - M.x0) / T)}_${Math.floor((z + RW.OZ - M.z0) / T)}`;
  const centroid2 = (pts) => { let x = 0, z = 0; for (const p of pts) { x += p[0]; z += p[1]; } return [x / pts.length, z / pts.length]; };
  const wireMat = new THREE.LineBasicMaterial({ color: 0x1d1d1d, transparent: true, opacity: 0.8 });
  const paintMat = new THREE.MeshStandardMaterial({ map: paintAtlas(), vertexColors: true, alphaTest: 0.45, roughness: 0.55 });
  const curbMat = withSurface(new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.85 }), '4');
  // the real bus lanes: Wilshire Blvd's red lanes, from Western Ave east to MacArthur Park
  const [wx0] = RW.toWorld(34.0617, -118.3092), [wx1] = RW.toWorld(34.0577, -118.2745);
  const busLanes = (r) => /^Wilshire Boulevard/.test(r.name || '') && r.pts.some(([x]) => x > wx0 && x < wx1);
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
    /* the landmarks in this tile: find their buildings in the data first */
    const lmHere = REAL_LANDMARKS.map((l) => ({ l, xz: RW.toWorld(...l.at) })).filter(({ xz: [x, z] }) => tileKeyAt(x, z) === t.key);
    const drop = new Set(), styleFor = new Map(), hostOf = new Map();
    for (const lm of lmHere) {
      const [x, z] = lm.xz, near = lm.l.near ?? (lm.l.find ? 200 : 0);
      let host = null;
      d.buildings.forEach((b0, i) => {
        const [cx, cz] = W2(centroid2(b0.pts)), dist = Math.hypot(cx - x, cz - z);
        if (lm.l.clear && dist < lm.l.clear) drop.add(i);
        if (!lm.l.find || dist > near || !lm.l.find(b0)) return;
        if (!host || b0.h > d.buildings[host].h) host = i;
        if (lm.l.replace) drop.add(i);
      });
      if (host !== null) { hostOf.set(lm, host); if (lm.l.style) styleFor.set(host, lm.l.style); }
    }
    const hostRecs = new Map();
    let n = 0;
    for (let bi = 0; bi < d.buildings.length; bi++) {
      const b0 = d.buildings[bi];
      const pts = b0.pts.map(W2);
      const b = { ...b0, pts };
      if (drop.has(bi)) {
        // (replaced by its model: just remember where it was)
        let base = Infinity; for (const [x, z] of pts) base = Math.min(base, gh(x, z));
        const [cx, cz] = centroid2(pts);
        hostRecs.set(bi, { pts, h: b.h, base, cx, cz, R: minRect(pts), area: Math.abs(ringArea(pts)), name: b.name });
        continue;
      }
      const area = Math.abs(ringArea(pts));
      let base = Infinity;
      for (const [x, z] of pts) base = Math.min(base, gh(x, z));
      const st = styleOf(b, area);
      const over = styleFor.get(bi);
      if (over) { if (over.wall) { st.wall = over.wall; st.trim = shadeHex(over.wall, 1.15); } if (over.win !== undefined) st.win = over.win; }
      const col = addBuilding(walls, roofs, b, base, st);
      if ([...hostOf.values()].includes(bi)) { const [cx, cz] = centroid2(pts); hostRecs.set(bi, { pts, h: b.h, base, cx, cz, R: minRect(pts), area, name: b.name }); }
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

    /* billboards up on the commercial roofs, facing the street (the ads are made up) */
    const boards = new Kit(), adFaces = new Mesh(), adCol = new THREE.Color(1, 1, 1);
    let nb = 0;
    for (const r of blds) {
      if (nb >= 8 || r.st.roof !== ROOF.flat || r.st.house || (r.st.win !== 2 && r.st.win !== 4) || r.b.h < 5 || r.b.h > 30 || r.area < 300) continue;
      if (hashOf(r.a * 0.13, r.e * 0.17) > 0.09) continue;
      const R = minRect(r.pts);
      if (!R || R.L < 9) continue;
      const ry = -Math.atan2(R.uz, R.ux), y = r.base + r.b.h + 0.8;
      const nx = -R.uz, nz = R.ux, cx = R.cx + nx * (R.Wd - 2), cz = R.cz + nz * (R.Wd - 2);
      for (const s2 of [-4.5, 4.5]) boards.box(0.35, 4.2, 0.35, LM.dark, cx + R.ux * s2, y, cz + R.uz * s2, ry);
      boards.box(15, 0.15, 1.2, LM.dark, cx - nx * 0.5, y + 4.0, cz - nz * 0.5, ry);
      boards.box(14.9, 4.5, 0.3, LM.dark, cx - nx * 0.25, y + 4.2, cz - nz * 0.25, ry);
      const cell = Math.floor(hashOf(r.e, r.a) * 8), cu = (cell % 4) / 4, cv = 1 - Math.floor(cell / 4) / 2;
      const P = (s2, yy) => [cx + R.ux * s2 + nx * 0.05, yy, cz + R.uz * s2 + nz * 0.05];
      adFaces.quad(P(-7.3, y + 4.25), P(7.3, y + 4.25), P(7.3, y + 8.6), P(-7.3, y + 8.6), nx, 0, nz, adCol, null, [[cu + 0.25, cv - 0.5], [cu, cv - 0.5], [cu, cv], [cu + 0.25, cv]]);
      nb++;
    }
    if (nb) {
      group.add(boards.object());
      const am = new THREE.Mesh(adFaces.geometry(), adMat);
      group.add(am); culls.push({ mesh: am, cx: tcx, cz: tcz, half: T / 2, far: 1400 });
    }


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

    /* the landmark models */
    for (const lm of lmHere) {
      if (!lm.l.build) continue;
      const hi = hostOf.get(lm), host = hi !== undefined ? hostRecs.get(hi) : null;
      if (lm.l.find && !host) continue;                // its building isn't in the data: leave it be
      const kit = lm.l.build({ x: lm.xz[0], z: lm.xz[1], host, gh, rect: minRect, M: LM });
      group.add(kit.object());
      if (kit.extra) group.add(kit.extra);
      colliders.push(...kit.colliders);
    }
    yield;

    let junctions = [], alongRoad = null;
    /* roads, sidewalks, parking lots */
    const groups = new Map();                    // material → Mesh builder
    const meshFor = (mat) => groups.get(mat) || groups.set(mat, new Mesh()).get(mat);
    const roadGrid = new Map();                  // 6 m cells → road ids, to keep things off other streets
    const roads = d.roads.map((r, id) => ({ ...r, id, pts: r.pts.map(W2) }));
    // which roads cross each 6 m cell (not footpaths: LA's sidewalks are often mapped as their own ways)
    for (const r of roads) {
      if (r.cls === RCLASS.footway) continue;
      const hw = r.w / 2 + 1;
      for (const [x, z] of resample(r.pts, 3)) {
        const rr = Math.ceil(hw / 6);
        for (let dz = -rr; dz <= rr; dz++) for (let dx = -rr; dx <= rr; dx++) {
          const cx = Math.floor(x / 6) + dx, cz = Math.floor(z / 6) + dz;
          if (Math.hypot((cx + 0.5) * 6 - x, (cz + 0.5) * 6 - z) > hw + 4.3) continue;
          const k = cz * 100000 + cx; (roadGrid.get(k) || roadGrid.set(k, new Set()).get(k)).add(r.id);
        }
      }
    }
    /* is (x, z) on some other road's carriageway (within pad metres of its edge)? */
    const onOtherRoad = (x, z, id, pad = 0.4) => {
      const s = roadGrid.get(Math.floor(z / 6) * 100000 + Math.floor(x / 6));
      if (!s) return false;
      for (const o of s) {
        if (o === id) continue;
        const r = roads[o], lim = r.w / 2 + pad;
        for (let i = 0; i < r.pts.length - 1; i++) {
          const [ax, az] = r.pts[i], [bx, bz] = r.pts[i + 1];
          if (Math.min(ax, bx) - lim > x || Math.max(ax, bx) + lim < x || Math.min(az, bz) - lim > z || Math.max(az, bz) + lim < z) continue;
          const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz || 1;
          const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
          if (Math.hypot(ax + dx * t - x, az + dz * t - z) < lim) return true;
        }
      }
      return false;
    };
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
          // (footpaths mapped across a street are its crosswalk, painted already: don't pave over the road)
          if (!(footway && onOtherRoad((ax + bx) / 2, (az + bz) / 2, r.id, -0.5)))
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
    {
      const sp = streetPaint(roads, gh, onOtherRoad, Y_OFF, (r) => busLanes(r));
      const { paint, curbs } = sp;
      junctions = sp.junctions; alongRoad = sp.along;
      if (paint.count) { const pm = new THREE.Mesh(paint.geometry(), paintMat); pm.receiveShadow = !lo; group.add(pm); culls.push({ mesh: pm, cx: tcx, cz: tcz, half: T / 2, far: 700 }); }
      if (curbs.count) { const cm = new THREE.Mesh(curbs.geometry(), curbMat); cm.receiveShadow = !lo; group.add(cm); culls.push({ mesh: cm, cx: tcx, cz: tcz, half: T / 2, far: 500 }); }
    }
    yield;
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

    /* murals on the blank walls (LA has thousands), graffiti low on the industrial ones */
    const art = new Mesh(), tags = new Mesh(), artCol = new THREE.Color(1, 1, 1);
    let murals = 0, tagged = 0;
    for (const r of blds) {
      const win = r.st.win;
      if (r.st.house || r.b.h < 5 || r.b.mh > 0) continue;
      for (let i = 0; i < r.pts.length; i++) {
        const [ax, az] = r.pts[i], [bx, bz] = r.pts[(i + 1) % r.pts.length], l = Math.hypot(bx - ax, bz - az);
        if (l < 6) continue;
        const nx = (bz - az) / l, nz = -(bx - ax) / l, tx = (bx - ax) / l, tz = (bz - az) / l, hh = hashOf(ax * 1.1 + i, az * 0.9 - i);
        const facesStreet = onOtherRoad((ax + bx) / 2 + nx * 9, (az + bz) / 2 + nz * 9, -1);
        const P = (s, y) => [ax + tx * s + nx * 0.36, y, az + tz * s + nz * 0.36];
        if ((win === 0 || win === 4) && l >= 10 && hh < 0.18 && murals < 30 && facesStreet) {
          const cell = Math.floor(hashOf(bx, bz) * 8), cu = (cell % 4) / 4, cv = 1 - Math.floor(cell / 4) / 2;
          const w = Math.min(l * 0.85, 30), s0 = (l - w) / 2, y0 = r.base + 0.6, y1 = r.base + Math.min(r.b.h - 0.8, 0.6 + w * 0.5);
          art.quad(P(s0 + w, y0), P(s0, y0), P(s0, y1), P(s0 + w, y1), nx, 0, nz, artCol, null, [[cu, cv - 0.5], [cu + 0.25, cv - 0.5], [cu + 0.25, cv], [cu, cv]]);
          murals++;
        } else if ((win === 4 || win === 0 || win === 5) && hh > 0.62 && tagged < 60) {
          const cell = Math.floor(hashOf(az, ax) * 8), cu = (cell % 4) / 4, cv = 1 - Math.floor(cell / 4) / 2;
          const w = Math.min(l * 0.6, 5 + hh * 3), s0 = (l - w) * hashOf(ax, bz), y0 = r.base + 0.3, y1 = y0 + w * 0.42;
          tags.quad(P(s0 + w, y0), P(s0, y0), P(s0, y1), P(s0 + w, y1), nx, 0, nz, artCol, null, [[cu, cv - 0.5], [cu + 0.25, cv - 0.5], [cu + 0.25, cv], [cu, cv]]);
          tagged++;
        }
      }
    }
    if (art.count) { const am = new THREE.Mesh(art.geometry(), muralMat); am.receiveShadow = !lo; group.add(am); culls.push({ mesh: am, cx: tcx, cz: tcz, half: T / 2, far: 900 }); }
    if (tags.count) { const tm = new THREE.Mesh(tags.geometry(), tagMat); tm.receiveShadow = !lo; group.add(tm); culls.push({ mesh: tm, cx: tcx, cz: tcz, half: T / 2, far: 350 }); }
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
    /* wooden power poles down the residential streets, wires sagging between
     * them: three on the crossarm, a phone and cable line lower down */
    const poles = [], wires = [];
    for (const r of roads) {
      if (r.cls !== RCLASS.residential && r.cls !== RCLASS.tertiary) continue;
      const side = hashOf(r.pts[0][0] * 0.3, r.pts[0][1] * 0.3) < 0.5 ? 1 : -1;
      const pts = resample(r.pts, 38);
      let prev = null;
      for (let i = 0; i < pts.length; i++) {
        const [ax, az] = pts[Math.max(0, i - 1)], [bx, bz] = pts[Math.min(pts.length - 1, i + 1)], l = Math.hypot(bx - ax, bz - az) || 1;
        const tx = (bx - ax) / l, tz = (bz - az) / l, off = (r.w / 2 + 1.0) * side;
        const x = pts[i][0] - tz * off, z = pts[i][1] + tx * off;
        if (onOtherRoad(x, z, r.id) || inBuilding(x, z, 0.8)) { prev = null; continue; }
        const y = gh(x, z), a = Math.atan2(-tz, tx), sa = Math.sin(a), ca = Math.cos(a);
        poles.push({ x, z, py: y, ry: a });
        const here = [[sa * 0.95, 9.9, ca * 0.95], [-sa * 0.95, 9.9, -ca * 0.95], [0, 10.45, 0], [0, 7.2, 0], [0, 6.6, 0]].map(([dx, dy, dz]) => [x + dx, y + dy, z + dz]);
        // (no span that would cut through the hillside between the poles)
        const clear = prev && [0.25, 0.5, 0.75].every((t) => {
          const qx = prev[3][0] + (x - prev[3][0]) * t, qz = prev[3][2] + (z - prev[3][2]) * t;
          return gh(qx, qz) + 3.5 < prev[3][1] + (here[3][1] - prev[3][1]) * t - 1.5 && !inBuilding(qx, qz, 0);
        });
        if (prev && clear && Math.hypot(prev[0][0] - x, prev[0][2] - z) < 60) {
          for (let k = 0; k < here.length; k++) {
            const A = prev[k], B = here[k], L = Math.hypot(B[0] - A[0], B[2] - A[2]), sag = 0.35 + L * 0.012;
            for (let s2 = 0; s2 < 8; s2++) {
              const t0 = s2 / 8, t1 = (s2 + 1) / 8;
              const P = (t) => [A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t - 4 * sag * t * (1 - t), A[2] + (B[2] - A[2]) * t];
              wires.push(...P(t0), ...P(t1));
            }
          }
        }
        prev = here;
      }
    }
    for (const part of furniture[PROP.POWER_POLE]) batch(poles, part.geo, part.mat, { far: part.far, cast: !lo });

    /* street-name signs on a post at a corner of each junction (blue in Hollywood, green elsewhere),
     * LA Metro bus stops by the junctions of the boulevards, bins out on the kerb, parking meters */
    const nameOf = (r) => (r.name || '').replace(/\bBoulevard\b/, 'Blvd').replace(/\bAvenue\b/, 'Ave').replace(/\bStreet\b/, 'St').replace(/\bDrive\b/, 'Dr').replace(/\bPlace\b/, 'Pl').replace(/\bRoad\b/, 'Rd').replace(/^North /, 'N ').replace(/^South /, 'S ').replace(/^East /, 'E ').replace(/^West /, 'W ');
    const signNames = [], signIdx = new Map(), namePosts = [], blades = new Mesh();
    const stops = [], shelters = new Kit();
    for (const jn of junctions.slice(0, 120)) {
      if (jn.minCls > RCLASS.residential) continue;
      const named = jn.legs.map((l, k) => ({ l, k, name: nameOf(l.r) })).filter((q) => q.name && q.l.r.cls <= RCLASS.residential);
      const a = named[0], b = named.find((q) => q.name !== a?.name);
      if (!a || !b || namePosts.length >= 40) continue;
      const [, , ax, az] = jn.dirs[a.k], [, , bx, bz] = jn.dirs[b.k];
      let mx = ax + bx, mz = az + bz; const ml = Math.hypot(mx, mz) || 1; mx /= ml; mz /= ml;
      const off = (jn.reach + 1.6) / Math.max(0.5, Math.sqrt((1 + ax * bx + az * bz) / 2)) * 0.75 + 1;
      const px = jn.x + mx * off, pz = jn.z + mz * off;
      if (onOtherRoad(px, pz, -1, 0.2) || inBuilding(px, pz, 0.4)) continue;
      const [lat, lon] = RW.toLatLon(px, pz), hwd = lat > 34.09 && lon < -118.30;
      const y = gh(px, pz);
      namePosts.push({ x: px, z: pz, py: y, ry: 0 });
      [[a, ax, az, 3.25], [b, bx, bz, 3.55]].forEach(([q, tx, tz, hgt]) => {
        const key = `${q.name}|${hwd ? 1 : 0}`;
        if (!signIdx.has(key)) { signIdx.set(key, signNames.length); signNames.push({ name: q.name, hwd }); }
        const cell = signIdx.get(key);
        if (cell >= 32) return;
        const [v0, v1] = [1 - (cell + 1) / 32, 1 - cell / 32];
        const w = 1.9, hh = 0.32, nx = -tz, nz = tx;
        const P = (s, yy) => [px + tx * s + nx * 0.03, y + yy, pz + tz * s + nz * 0.03];
        const Q = (s, yy) => [px + tx * s - nx * 0.03, y + yy, pz + tz * s - nz * 0.03];
        blades.quad(P(-w / 2, hgt), P(w / 2, hgt), P(w / 2, hgt + hh), P(-w / 2, hgt + hh), nx, 0, nz, white, null, [[1, v0], [0, v0], [0, v1], [1, v1]]);
        blades.quad(Q(-w / 2, hgt), Q(w / 2, hgt), Q(w / 2, hgt + hh), Q(-w / 2, hgt + hh), -nx, 0, -nz, white, null, [[0, v0], [1, v0], [1, v1], [0, v1]]);
      });
      // a bus stop on the boulevard: bench, sign, and a shelter at the busier ones
      const big = jn.legs.find((l) => l.r.cls <= RCLASS.secondary && !l.r.oneway);
      if (big && hashOf(jn.x, jn.z) < 0.45 && stops.length < 10) {
        const [sx, sz, tx, tz] = alongRoad(big.r, big.i, big.dir, jn.reach + 14);
        const rx = tz, rz = -tx, so = big.r.w / 2 + 2.1;          // the kerb on the right of traffic coming in
        const bx2 = sx + rx * so, bz2 = sz + rz * so;
        if (!onOtherRoad(bx2, bz2, big.r.id, 0.2) && !inBuilding(bx2, bz2, 1.5)) {
          const ry = Math.atan2(rx, rz), by = gh(bx2, bz2);
          stops.push({ x: bx2, z: bz2, py: by, ry: ry + Math.PI });
          if (hashOf(jn.z, jn.x) < 0.6) {
            const ox = bx2 + rx * 0.9, oz = bz2 + rz * 0.9;
            shelters.box(4.2, 0.12, 1.9, LM.dark, bx2 + rx * 0.2, by + 2.6, bz2 + rz * 0.2, ry);
            shelters.box(4.2, 2.4, 0.06, LM.glass, ox, by + 0.15, oz, ry);
            for (const e of [-1, 1]) shelters.box(0.1, 2.6, 0.1, LM.dark, ox - tx * e * 2, by, oz - tz * e * 2, ry);
          }
        }
      }
    }
    if (namePosts.length) {
      batch(namePosts, POST, LM.dark, { far: 350, cast: false });
      const tex = streetSignAtlas(signNames.slice(0, 32));
      const mat = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.5, side: THREE.FrontSide });
      disposables.push(tex, mat);
      const bm = new THREE.Mesh(blades.geometry(), mat);
      group.add(bm); culls.push({ mesh: bm, cx: tcx, cz: tcz, half: T / 2, far: 300 });
    }
    for (const part of furniture[PROP.BENCH]) batch(stops, part.geo, part.mat, { far: part.far, cast: false });
    if (shelters.parts.size) { const so = shelters.object(); group.add(so); }
    // bins out for collection on the residential kerbs, meters along the shopping streets
    const bins = [], meters = [];
    for (const r of roads) {
      if (r.cls < RCLASS.primary || r.cls > RCLASS.residential) continue;
      const res = r.cls === RCLASS.residential;
      const pts = resample(r.pts, res ? 23 : 7.5);
      for (let i = 1; i < pts.length - 1; i++) {
        const [ax, az] = pts[i - 1], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1, tx = (bx - ax) / l, tz = (bz - az) / l;
        for (const sd of [-1, 1]) {
          const hh = hashOf(pts[i][0] * 0.31 + sd, pts[i][1] * 0.27);
          if (res ? hh > 0.25 : hh > 0.55 || r.cls > RCLASS.secondary) continue;
          const off = res ? r.w / 2 - 0.5 : r.w / 2 + 0.55, x0 = pts[i][0] - tz * off * sd, z0 = pts[i][1] + tx * off * sd;
          if (onOtherRoad(x0, z0, r.id, 0.3) || inBuilding(x0, z0, 0.6)) continue;
          if (res) for (let k = 0; k < 3; k++) { const x = x0 + tx * (k - 1) * 0.85, z = z0 + tz * (k - 1) * 0.85; bins.push({ x, z, py: gh(x, z) + 0.1, ry: Math.atan2(-tz, tx) + Math.PI / 2 * sd, color: k }); }
          else meters.push({ x: x0, z: z0, py: gh(x0, z0) + 0.1, ry: Math.atan2(-tz, tx) });
        }
      }
    }
    for (const part of furniture[PROP.BIN]) batch(bins.map((b) => ({ ...b, c: part.tint ? part.tint(b) : undefined })), part.geo, part.mat, { far: 260, cast: false });
    batch(meters, METER, LM.dark, { far: 220, cast: false });
    if (wires.length) {
      const wg = new THREE.BufferGeometry();
      wg.setAttribute('position', new THREE.Float32BufferAttribute(wires, 3));
      const wm = new THREE.LineSegments(wg, wireMat);
      group.add(wm);
      culls.push({ mesh: wm, cx: tcx, cz: tcz, half: T / 2, far: 450 });
    }
    /* the Hollywood Walk of Fame: a coral-pink terrazzo star every six feet down both sidewalks of
     * Hollywood Blvd from Gower to La Brea and of Vine from Yucca to Sunset */
    const stars = [];
    for (const r of roads) {
      const onHwd = /^Hollywood Boulevard/.test(r.name || ''), onVine = /^Vine Street/.test(r.name || '');
      if (!onHwd && !onVine) continue;
      const pts = resample(r.pts, 1.83);
      for (let i = 1; i < pts.length - 1; i++) {
        const [px, pz] = pts[i], [lat, lon] = RW.toLatLon(px, pz);
        if (onHwd ? lon < -118.3446 || lon > -118.3218 : lat < 34.0980 || lat > 34.1035) continue;
        const [ax, az] = pts[i - 1], [bx, bz] = pts[i + 1], l = Math.hypot(bx - ax, bz - az) || 1, tx = (bx - ax) / l, tz = (bz - az) / l;
        for (const sd of [-1, 1]) {
          const off = r.w / 2 + 1.9, x = px - tz * off * sd, z = pz + tx * off * sd;
          if (onOtherRoad(x, z, r.id, 0.3) || inBuilding(x, z, 0.3)) continue;
          stars.push({ x, z, py: gh(x, z) + 0.09, ry: Math.atan2(-tz, tx) + (sd < 0 ? Math.PI : 0) });
        }
      }
    }
    if (stars.length) batch(stars, STAR, starMat, { far: 250, cast: false });

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

    /* shop signs: each named shop, café and bar on the wall of its building; the cinemas and
     * theatres get a marquee and a blade sign */
    const signs = [], theatres = [];
    const isStage = (p) => /cinema|theat|movie|performing_arts|music_venue|concert/.test(p.what || '');
    const rank = (p) => (isStage(p) ? (/theat|cinema|dome|bowl/i.test(p.name) ? 0 : 1) : 2);
    for (const p of [...d.pois].sort((a, b) => rank(a) - rank(b))) {
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
      if (!best) continue;
      if (isStage(p) && theatres.length < 8 && best.r.b.h > 7 && !theatres.some((q) => q.r === best.r)) theatres.push({ ...best, name: p.name.replace(/^(The |Hollywood )/, '').slice(0, 22) });
      else if (signs.length < 40) signs.push({ ...best, name: p.name.slice(0, 28) });
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
        const ox = s.qx + s.nx * 0.34, oz = s.qz + s.nz * 0.34;
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
          const P = (along, o, yy) => [s.qx + s.dx * along + s.nx * (0.35 + o), yy, s.qz + s.dz * along + s.nz * (0.35 + o)];
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

    if (theatres.length) {
      // one atlas: the marquee's name (rows 0-7) and the blade's (rows 8-15), lit at night
      const c = document.createElement('canvas');
      c.width = 1024; c.height = 2048;
      const ctx = c.getContext('2d');
      const m = new Mesh(), bulbs = new Mesh();
      theatres.forEach((s, i) => {
        const [bg, fg] = [['#14110d', '#ffd36b'], ['#7a1010', '#fff4d0'], ['#0e2a4a', '#ffe9a8'], ['#1b1b1b', '#ff5ec4']][Math.floor(hashOf(s.qx, s.qz) * 4)];
        for (const row of [i, i + 8]) {
          ctx.fillStyle = bg; ctx.fillRect(0, row * 128, 1024, 128);
          ctx.strokeStyle = fg; ctx.lineWidth = 6; ctx.strokeRect(8, row * 128 + 8, 1008, 112);
          ctx.fillStyle = fg; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
          let size = 92; ctx.font = `bold ${size}px "Arial Narrow", Helvetica, Arial, sans-serif`;
          const txt = s.name.toUpperCase();
          while (ctx.measureText(txt).width > 960 && size > 30) { size -= 4; ctx.font = `bold ${size}px "Arial Narrow", Helvetica, Arial, sans-serif`; }
          ctx.shadowColor = fg; ctx.shadowBlur = 14; ctx.fillText(txt, 512, row * 128 + 68); ctx.shadowBlur = 0;
        }
        const cell = (row) => [1 - (row + 1) / 16, 1 - row / 16];
        const base = s.r.base, w = Math.min(s.len * 0.85, 16), out = 3.4, y0 = base + 4.3, y1 = y0 + 2.4;
        const P = (along, o, y) => [s.qx + s.dx * along + s.nx * (0.34 + o), y, s.qz + s.dz * along + s.nz * (0.34 + o)];
        const [v0, v1] = cell(i);
        // the marquee: its name on the front and both ends, bulbs round the edges, lit underneath
        m.quad(P(w / 2, out, y0), P(-w / 2, out, y0), P(-w / 2, out, y1), P(w / 2, out, y1), s.nx, 0, s.nz, white, null, [[0, v0], [1, v0], [1, v1], [0, v1]]);
        for (const e of [-1, 1]) m.quad(P(e * w / 2, 0, y0), P(e * w / 2, out, y0), P(e * w / 2, out, y1), P(e * w / 2, 0, y1), s.dx * e, 0, s.dz * e, white, null, e > 0 ? [[0.2, v0], [0.8, v0], [0.8, v1], [0.2, v1]] : [[0.8, v0], [0.2, v0], [0.2, v1], [0.8, v1]]);
        const plain = (a, b, cc, d, nx, ny, nz) => m.quad(a, b, cc, d, nx, ny, nz, white, null, [[0.01, v0 + 0.002], [0.02, v0 + 0.002], [0.02, v0 + 0.004], [0.01, v0 + 0.004]]);
        plain(P(-w / 2, 0, y1), P(w / 2, 0, y1), P(w / 2, out, y1), P(-w / 2, out, y1), 0, 1, 0);   // (the roof of the marquee: a corner of its own cell, plain)
        bulbs.quad(P(-w / 2, 0, y0), P(w / 2, 0, y0), P(w / 2, out, y0), P(-w / 2, out, y0), 0, -1, 0, white, null);
        for (const y of [y0 + 0.08, y1 - 0.08]) bulbs.quad(P(w / 2, out + 0.02, y - 0.07), P(-w / 2, out + 0.02, y - 0.07), P(-w / 2, out + 0.02, y + 0.07), P(w / 2, out + 0.02, y + 0.07), s.nx, 0, s.nz, white, null);
        // the blade: a tall sign standing out from the wall, the name running up it
        const bh = Math.min(13, s.r.b.h - 8), bx = w / 2 - 0.6;
        if (bh > 4) {
          const [u0, u1] = cell(i + 8), by0 = y1 + 0.8, by1 = by0 + bh, dep = 1.8;
          for (const e of [-1, 1]) {
            const Q = (o, y) => P(bx + e * 0.22, o, y);
            m.quad(Q(0, by0), Q(dep, by0), Q(dep, by1), Q(0, by1), s.dx * e, 0, s.dz * e, white, null,
              e > 0 ? [[0, u1], [0, u0], [1, u0], [1, u1]] : [[0, u0], [0, u1], [1, u1], [1, u0]]);
          }
          const R = (y) => [P(bx - 0.22, dep, y), P(bx + 0.22, dep, y)];
          bulbs.quad(R(by0)[0], R(by0)[1], R(by1)[1], R(by1)[0], s.nx, 0, s.nz, white, null);
        }
      });
      const tex = new THREE.CanvasTexture(c);
      tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 8;
      const mat = new THREE.MeshStandardMaterial({ map: tex, emissiveMap: tex, emissive: 0xffffff, emissiveIntensity: 0.15, roughness: 0.5, side: THREE.DoubleSide });
      glowMats.push({ mat, day: 0.15, night: 1.6 });
      disposables.push(tex, mat);
      group.add(new THREE.Mesh(m.geometry(), mat));
      group.add(new THREE.Mesh(bulbs.geometry(), LM.lamp));
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
  const atmo = makeAtmosphere(scene, renderer, { quality, uniforms, fogNear: 800, fogFar: 8000, groundAt: RW.groundHeight, skyRadius: RW.far ? 150000 : 9000 });
  /* --------------------------------- the horizon: mountains, basin, sea */
  const horizon = buildHorizon(RW, scene, uniforms, lo);
  atmo.onTime.push((night) => {
    for (const m of roadMats) m.emissiveIntensity = night * 0.5;
    for (const g of glowMats) g.mat.emissiveIntensity = g.day + (g.night - g.day) * night;
    poolMat.emissiveIntensity = 0.2 + night * 1.2;
  });
  atmo.onUpdate.push((dt, time, camera, focus) => {
    waterNormals.offset.set((time * 0.01) % 1, (time * 0.007) % 1);
    if (horizon && scene.fog) horizon.update(scene.fog.color);
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
    stats: () => ({ tiles: [...tiles.values()].map((t) => `${t.key}:${t.state}`), colliders: RW.colliders.length, focus: lastFocus, farShown: [...far.entries()].filter(([, m]) => m.visible).map(([k]) => k) }),
    far
  };
}
