/*
 * The world: turns the geography in geo.js into a playable map.
 *
 * createWorld() builds, deterministically from a seed:
 *   - the terrain (mountains, beaches, river channel, lakes) as a height grid,
 *   - every building, tree, parked car and landmark,
 *   - the ground "paint" (blocks, pools, runways) the renderer draws,
 *   - the colliders and a spatial grid to query them.
 *
 * Pure logic: no DOM, no three.js, so it runs (and is tested) in plain Node.
 */

import {
  SIZE, HALF, COAST, OCEAN, WATER_POLYS, RIDGES, RIVERS, CANALS, LAKES, ROADS,
  ROAD_WIDTH, DISTRICTS, STYLES, PALETTE, project
} from './geo.js';
import { PADS, buildLandmarks } from './landmarks.js';
import { buildStreets, SIGN_TEXTS, SIGN_POOLS } from './street.js';
import { makeArchitecture, houseKind } from './architecture.js';

/* ---------------------------------------------------------------- maths */

export const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
export const lerp = (a, b, t) => a + (b - a) * t;
export function smoothstep(e0, e1, v) {
  const t = clamp((v - e0) / (e1 - e0), 0, 1);
  return t * t * (3 - 2 * t);
}

/* Rotate a local offset by a yaw, the same way three.js's rotation.y does:
 * local +x maps to (cos, -sin). */
export function rot(lx, lz, a) {
  const c = Math.cos(a), s = Math.sin(a);
  return [lx * c + lz * s, -lx * s + lz * c];
}

let SEG_T = 0;
export function segDist(px, pz, ax, az, bx, bz) {
  const dx = bx - ax, dz = bz - az;
  const len2 = dx * dx + dz * dz;
  let t = len2 > 0 ? ((px - ax) * dx + (pz - az) * dz) / len2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  SEG_T = t;
  const ex = ax + dx * t - px, ez = az + dz * t - pz;
  return Math.sqrt(ex * ex + ez * ez);
}

let POLY_I = 0, POLY_T = 0;
export function polyDist(pts, x, z) {
  let best = Infinity;
  for (let i = 0; i < pts.length - 1; i++) {
    const d = segDist(x, z, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
    if (d < best) { best = d; POLY_I = i; POLY_T = SEG_T; }
  }
  return best;
}

export function pointInPoly(x, z, pts) {
  let inside = false;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
    const xi = pts[i][0], zi = pts[i][1], xj = pts[j][0], zj = pts[j][1];
    if ((zi > z) !== (zj > z) && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) inside = !inside;
  }
  return inside;
}

export function rng(seed) {
  let a = seed >>> 0;
  const next = () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  next.range = (a0, b0) => a0 + (b0 - a0) * next();
  next.int = (a0, b0) => Math.floor(a0 + (b0 - a0 + 1) * next());
  next.pick = (arr) => arr[Math.floor(next() * arr.length)];
  next.chance = (p) => next() < p;
  return next;
}

function hash2(ix, iz, seed) {
  let h = (Math.imul(ix, 374761393) + Math.imul(iz, 668265263) + Math.imul(seed, 1442695041)) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}
function vnoise(x, z, seed) {
  const ix = Math.floor(x), iz = Math.floor(z);
  const fx = x - ix, fz = z - iz;
  const u = fx * fx * (3 - 2 * fx), v = fz * fz * (3 - 2 * fz);
  const a = hash2(ix, iz, seed), b = hash2(ix + 1, iz, seed);
  const c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
export function fbm(x, z, oct, seed) {
  let s = 0, amp = 0.5, f = 1, n = 0;
  for (let i = 0; i < oct; i++) {
    s += amp * vnoise(x * f, z * f, seed + i * 17);
    n += amp; amp *= 0.5; f *= 2.03;
  }
  return s / n;
}

/* -------------------------------------------------------------- terrain */

const bboxOf = (pts, pad) => {
  let x0 = Infinity, z0 = Infinity, x1 = -Infinity, z1 = -Infinity;
  for (const p of pts) { x0 = Math.min(x0, p[0]); z0 = Math.min(z0, p[1]); x1 = Math.max(x1, p[0]); z1 = Math.max(z1, p[1]); }
  return [x0 - pad, z0 - pad, x1 + pad, z1 + pad];
};
const inBox = (b, x, z) => x >= b[0] && x <= b[2] && z >= b[1] && z <= b[3];

function makeTerrain(seed) {
  const ridges = RIDGES.map((r) => ({ ...r, box: bboxOf(r.pts, Math.max(...r.pts.map((p) => p[3]))) }));
  const waters = WATER_POLYS.map((w) => ({ ...w, box: bboxOf(w.pts, 0) }));
  const rivers = RIVERS.map((r) => ({ ...r, box: bboxOf(r.pts, r.w + 10) }));
  const canalBox = bboxOf(CANALS.flat(), 10);

  function mountains(x, z) {
    let m = 0;
    for (const r of ridges) {
      if (!inBox(r.box, x, z)) continue;
      const p = r.pts;
      for (let i = 0; i < p.length - 1; i++) {
        const d = segDist(x, z, p[i][0], p[i][1], p[i + 1][0], p[i + 1][1]);
        const t = SEG_T;
        const w = lerp(p[i][3], p[i + 1][3], t);
        if (d >= w) continue;
        const h = lerp(p[i][2], p[i + 1][2], t) * (1 - smoothstep(0, 1, d / w));
        if (h > m) m = h;
      }
    }
    return m;
  }

  const base = (x, z) =>
    4 + 16 * smoothstep(-600, -2700, z) + 30 * smoothstep(1200, 3000, x) * smoothstep(-500, -3000, z) +
    3 * (fbm(x / 600, z / 600, 3, seed) - 0.5);

  /* Signed distance to the shore: negative out at sea. */
  function shore(x, z) {
    const d = polyDist(COAST, x, z);
    return pointInPoly(x, z, OCEAN) ? -d : d;
  }

  function inHardWater(x, z) {
    for (const w of waters) if (inBox(w.box, x, z) && pointInPoly(x, z, w.pts)) return true;
    return false;
  }

  /* Height before landmark pads and lakes are cut in. */
  function raw(x, z) {
    let m = mountains(x, z);
    if (m > 0) {
      m *= 0.65 + 0.7 * fbm(x / 420, z / 420, 4, seed + 3);
      m += 18 * (fbm(x / 90, z / 90, 3, seed + 7) - 0.5) * Math.min(1, m / 40);
      if (m < 0) m = 0;
    }
    const land = base(x, z) + m;
    const s = shore(x, z);
    let h;
    // the beach: a quick drop at the waterline, then a long gentle rise
    if (s < 0) h = Math.max(-22, -1.0 + s * 0.05);
    else if (s < 12) h = -1.0 + s * 0.12;
    else if (s < 80) h = 0.44 + (s - 12) * 0.033;
    else if (s < 220) h = lerp(2.68, land, smoothstep(80, 220, s));
    else h = land;
    if (inHardWater(x, z)) h = Math.min(h, -4);
    for (const r of rivers) {
      if (!inBox(r.box, x, z)) continue;
      const d = polyDist(r.pts, x, z);
      if (d < r.w + 8) h -= r.depth * (1 - smoothstep(r.w, r.w + 8, d));
    }
    if (inBox(canalBox, x, z)) {
      let d = Infinity;
      for (const c of CANALS) d = Math.min(d, segDist(x, z, c[0][0], c[0][1], c[1][0], c[1][1]));
      if (d < 7) h -= 2.6 * (1 - smoothstep(4, 7, d));
    }
    return h;
  }

  const pads = PADS.map((p) => ({ ...p, h: p.y !== undefined ? p.y : raw(p.x, p.z) + (p.dy || 0) }));
  function padded(x, z) {
    let h = raw(x, z);
    for (const p of pads) {
      const dx = x - p.x, dz = z - p.z;
      const d = Math.sqrt(dx * dx + dz * dz);
      if (d < p.r + p.blend) h = lerp(h, p.h, 1 - smoothstep(p.r, p.r + p.blend, d));
    }
    return h;
  }
  const lakes = LAKES.map((l) => ({ ...l, level: padded(l.x, l.z) - 1.2 }));
  function height(x, z) {
    let h = padded(x, z);
    for (const l of lakes) {
      const dx = x - l.x, dz = z - l.z;
      const d = Math.sqrt(dx * dx + dz * dz);
      if (d < l.r + 12) {
        const bottom = l.level - 0.8 - 2.5 * (1 - Math.min(1, d / l.r));
        h = d < l.r ? Math.min(h, bottom) : lerp(Math.min(h, l.level + 0.4), h, smoothstep(l.r, l.r + 12, d));
      }
    }
    return h;
  }

  return { height, shore, inHardWater, lakes, pads, base };
}

/* ------------------------------------------------------------ the world */

export const GRID_N = 384;               // terrain cells per side
export const CELL = SIZE / GRID_N;       // 15.625 m
const MASK_RES = 4, MASK_N = SIZE / MASK_RES;
export const MASK = { ROAD: 1, RESERVED: 2, CHANNEL: 4, LAKE: 8 };
const CG = 32, CG_N = Math.ceil(SIZE / CG);   // collider grid

export function createWorld(opts = {}) {
  const seed = opts.seed ?? 1985;
  const t0 = Date.now();
  const rnd = rng(seed);
  const terrain = makeTerrain(seed);

  const W = {
    seed, size: SIZE, half: HALF, gridN: GRID_N, cell: CELL,
    heights: new Float32Array((GRID_N + 1) * (GRID_N + 1)),
    sea: new Uint8Array((GRID_N + 1) * (GRID_N + 1)),
    mask: new Uint8Array(MASK_N * MASK_N),
    lakes: terrain.lakes, roads: ROADS, rivers: RIVERS, canals: CANALS, coast: COAST,
    waterPolys: WATER_POLYS, districts: DISTRICTS,
    buildings: [], parts: [], groups: {}, trees: [], blocks: [], pools: [], paint: [], signs: [],
    props: [], wires: [], crosswalks: [], gantries: [], parked: [], walkers: [],
    colliders: [], pois: [], vehicles: [], billboards: [], bridges: [],
    stats: {}
  };

  /* Height grid, sampled exactly where the renderer puts its vertices. */
  const N1 = GRID_N + 1;
  for (let j = 0; j <= GRID_N; j++) {
    const z = -HALF + j * CELL;
    for (let i = 0; i <= GRID_N; i++) {
      const x = -HALF + i * CELL;
      const h = terrain.height(x, z);
      W.heights[j * N1 + i] = h;
      W.sea[j * N1 + i] = h < 0 && (terrain.shore(x, z) < 0 || terrain.inHardWater(x, z)) ? 1 : 0;
    }
  }

  /* Ground height that matches the rendered triangles exactly, so feet sit
   * on the surface you see (cell split along the b–d diagonal). */
  W.groundHeight = (x, z) => {
    let fx = (x + HALF) / CELL, fz = (z + HALF) / CELL;
    fx = clamp(fx, 0, GRID_N - 1e-6); fz = clamp(fz, 0, GRID_N - 1e-6);
    const i = Math.floor(fx), j = Math.floor(fz);
    const u = fx - i, v = fz - j;
    const H = W.heights;
    const ha = H[j * N1 + i], hd = H[j * N1 + i + 1], hb = H[(j + 1) * N1 + i], hc = H[(j + 1) * N1 + i + 1];
    if (u + v <= 1) return ha + (hd - ha) * u + (hb - ha) * v;
    return hc + (hb - hc) * (1 - u) + (hd - hc) * (1 - v);
  };
  const gh = W.groundHeight;
  W.normalAt = (x, z) => {
    const e = 4;
    const nx = gh(x - e, z) - gh(x + e, z), nz = gh(x, z - e) - gh(x, z + e), ny = 2 * e;
    const l = Math.hypot(nx, ny, nz);
    return [nx / l, ny / l, nz / l];
  };
  W.slopeAt = (x, z) => 1 - W.normalAt(x, z)[1];
  W.isSea = (x, z) => {
    const i = Math.round((x + HALF) / CELL), j = Math.round((z + HALF) / CELL);
    if (i < 0 || j < 0 || i > GRID_N || j > GRID_N) return true;
    return W.sea[j * N1 + i] === 1;
  };
  /* The surface of whatever water is here, or null if none. */
  W.waterLevelAt = (x, z) => {
    if (W.isSea(x, z)) return 0;
    for (const l of W.lakes) if (Math.hypot(x - l.x, z - l.z) < l.r) return l.level;
    for (const c of CANALS) {
      if (segDist(x, z, c[0][0], c[0][1], c[1][0], c[1][1]) < 4) return W.canalLevel;
    }
    return null;
  };
  W.canalLevel = gh(-1530, 600) + 1.3;
  W.shoreDist = terrain.shore;

  /* ---- masks: where roads, channels and landmarks already are */
  const M = W.mask;
  const mi = (x, z) => {
    const i = Math.floor((x + HALF) / MASK_RES), j = Math.floor((z + HALF) / MASK_RES);
    return i < 0 || j < 0 || i >= MASK_N || j >= MASK_N ? -1 : j * MASK_N + i;
  };
  W.maskAt = (x, z) => { const k = mi(x, z); return k < 0 ? 255 : M[k]; };
  function stampDisc(x, z, r, bit) {
    const i0 = Math.floor((x - r + HALF) / MASK_RES), i1 = Math.floor((x + r + HALF) / MASK_RES);
    const j0 = Math.floor((z - r + HALF) / MASK_RES), j1 = Math.floor((z + r + HALF) / MASK_RES);
    for (let j = Math.max(0, j0); j <= Math.min(MASK_N - 1, j1); j++) {
      for (let i = Math.max(0, i0); i <= Math.min(MASK_N - 1, i1); i++) {
        const cx = -HALF + (i + 0.5) * MASK_RES - x, cz = -HALF + (j + 0.5) * MASK_RES - z;
        if (cx * cx + cz * cz <= r * r) M[j * MASK_N + i] |= bit;
      }
    }
  }
  function stampLine(pts, r, bit) {
    for (let k = 0; k < pts.length - 1; k++) {
      const [ax, az] = pts[k], [bx, bz] = pts[k + 1];
      const len = Math.hypot(bx - ax, bz - az), n = Math.max(1, Math.ceil(len / 3));
      for (let s = 0; s <= n; s++) stampDisc(lerp(ax, bx, s / n), lerp(az, bz, s / n), r, bit);
    }
  }
  function stampRect(x, z, hw, hd, a, bit) {
    const r = Math.hypot(hw, hd);
    const i0 = Math.floor((x - r + HALF) / MASK_RES), i1 = Math.floor((x + r + HALF) / MASK_RES);
    const j0 = Math.floor((z - r + HALF) / MASK_RES), j1 = Math.floor((z + r + HALF) / MASK_RES);
    const c = Math.cos(a), s = Math.sin(a);
    for (let j = Math.max(0, j0); j <= Math.min(MASK_N - 1, j1); j++) {
      for (let i = Math.max(0, i0); i <= Math.min(MASK_N - 1, i1); i++) {
        const dx = -HALF + (i + 0.5) * MASK_RES - x, dz = -HALF + (j + 0.5) * MASK_RES - z;
        const lx = dx * c - dz * s, lz = dx * s + dz * c;
        if (Math.abs(lx) <= hw && Math.abs(lz) <= hd) M[j * MASK_N + i] |= bit;
      }
    }
  }
  for (const r of ROADS) stampLine(r.pts, ROAD_WIDTH[r.kind] / 2 + 2, MASK.ROAD);
  for (const r of RIVERS) stampLine(r.pts, r.w + 8, MASK.CHANNEL);
  for (const c of CANALS) stampLine(c, 9, MASK.CHANNEL);
  for (const l of W.lakes) stampDisc(l.x, l.z, l.r + 14, MASK.LAKE);
  for (const p of terrain.pads) if (!p.open) stampDisc(p.x, p.z, p.r + p.blend * 0.5, MASK.RESERVED);

  /* ---- districts (Voronoi), cached on a coarse grid */
  const DG = 20, DGN = SIZE / DG;
  const dgrid = new Uint8Array(DGN * DGN);
  for (let j = 0; j < DGN; j++) {
    for (let i = 0; i < DGN; i++) {
      const x = -HALF + (i + 0.5) * DG, z = -HALF + (j + 0.5) * DG;
      let best = 0, bd = Infinity;
      for (let k = 0; k < DISTRICTS.length; k++) {
        const d = (DISTRICTS[k].x - x) ** 2 + (DISTRICTS[k].z - z) ** 2;
        if (d < bd) { bd = d; best = k; }
      }
      dgrid[j * DGN + i] = best;
    }
  }
  W.districtIndexAt = (x, z) => {
    const i = clamp(Math.floor((x + HALF) / DG), 0, DGN - 1), j = clamp(Math.floor((z + HALF) / DG), 0, DGN - 1);
    return dgrid[j * DGN + i];
  };
  W.districtAt = (x, z) => DISTRICTS[W.districtIndexAt(x, z)];
  /* What the ground is like here, for painting: built-up styles turn wild
   * where the hills get steep or high. */
  W.styleAt = (x, z) => {
    const d = W.districtAt(x, z);
    if (d.style === 'hills' || d.style === 'wild' || d.style === 'park' || d.style === 'oilfield' || d.style === 'airport') return d.style;
    if (gh(x, z) - terrain.base(x, z) > 22 || W.slopeAt(x, z) > 0.06) return 'wild';
    return d.style;
  };

  /* ---- colliders */
  const C = W.colliders;
  function addBox(x, z, hw, hd, a, y0, y1, tag) {
    C.push({ k: 0, x, z, hx: hw, hz: hd, c: Math.cos(a), s: Math.sin(a), r: Math.hypot(hw, hd), y0, y1, tag });
  }
  function addCircle(x, z, r, y0, y1, tag) { C.push({ k: 1, x, z, r, y0, y1, tag }); }
  W.addBox = addBox; W.addCircle = addCircle;
  W.queryPoint = (x, y, z, r) => queryPoint(W, x, y, z, r);

  /* ---- the toolkit landmarks and generators build with */
  const P = W.parts;
  const part = (p) => {
    P.push(p);
    if (p.col && !p.g) {
      if (p.s === 'box' && !p.rx && !p.rz) addBox(p.x, p.z, p.sx / 2, p.sz / 2, p.ry || 0, p.y - p.sy / 2, p.y + p.sy / 2, p.tag);
      else if (p.s === 'cyl' || p.s === 'sphere' || p.s === 'cone') addCircle(p.x, p.z, ((p.sx + p.sz) / 4) * (p.s === 'cone' ? 0.6 : 1), p.y - p.sy / 2, p.y + p.sy / 2, p.tag);
      else if (p.s === 'dome') addCircle(p.x, p.z, ((p.sx + p.sz) / 4) * 0.8, p.y, p.y + p.sy * 0.5, p.tag);
    }
    return p;
  };
  const api = {
    W, rnd, gh, rot, project, lerp, clamp, MASK,
    box: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'box', x, y, z, sx, sy, sz, c, ...o }),
    cyl: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'cyl', x, y, z, sx, sy, sz, c, ...o }),
    cone: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'cone', x, y, z, sx, sy, sz, c, ...o }),
    sphere: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'sphere', x, y, z, sx, sy, sz, c, ...o }),
    dome: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'dome', x, y, z, sx, sy, sz, c, ...o }),
    roof: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'roof', x, y, z, sx, sy, sz, c, ...o }),
    torus: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'torus', x, y, z, sx, sy, sz, c, ...o }),
    prism: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'prism', x, y, z, sx, sy, sz, c, ...o }),
    gable: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'gable', x, y, z, sx, sy, sz, c, ...o }),
    archBlock: (x, y, z, sx, sy, sz, c, o = {}) => part({ s: 'arch', x, y, z, sx, sy, sz, c, ...o }),
    text: (x, y, z, w, h, text, c, o = {}) => part({ s: 'text', x, y, z, sx: w, sy: h, sz: 1, text, c, ...o }),
    paint: (op, layer = 0) => W.paint.push({ layer, ...op }),
    reserveDisc: (x, z, r) => stampDisc(x, z, r, MASK.RESERVED),
    reserveRect: (x, z, hw, hd, a) => stampRect(x, z, hw, hd, a, MASK.RESERVED),
    poi: (name, x, z, r = 120, kind = 'landmark') => W.pois.push({ name, x, z, r, kind }),
    group: (id, g) => { W.groups[id] = g; },
    /* A semicircular arch of `segs` boxes standing in the vertical plane
     * whose horizontal axis points along yaw `a`. */
    arch(cx, cy, cz, R, thick, depth, a, c, segs = 9, o = {}) {
      const len = 2 * R * Math.sin(Math.PI / (2 * segs)) * 1.08;
      for (let k = 0; k < segs; k++) {
        const am = ((k + 0.5) / segs) * Math.PI;
        const [ox, oz] = rot(R * Math.cos(am), 0, a);
        part({ s: 'box', x: cx + ox, y: cy + R * Math.sin(am), z: cz + oz, sx: len, sy: thick, sz: depth, ry: a, rz: am + Math.PI / 2, c, ...o });
      }
    },
    /* Steps from (x0,z0,y0) to (x1,z1,y1): walkable and drivable. */
    ramp(x0, z0, y0, x1, z1, y1, width, c, step = 0.32) {
      const n = Math.max(1, Math.ceil(Math.abs(y1 - y0) / step));
      const a = Math.atan2(-(z1 - z0), x1 - x0);
      const len = Math.hypot(x1 - x0, z1 - z0) / n;
      for (let i = 0; i < n; i++) {
        const t = (i + 0.5) / n;
        const top = lerp(y0, y1, (i + 1) / n);
        const bottom = Math.min(gh(lerp(x0, x1, t), lerp(z0, z1, t)), top) - 1;
        part({ s: 'box', x: lerp(x0, x1, t), y: (top + bottom) / 2, z: lerp(z0, z1, t), sx: len * 1.02, sy: top - bottom, sz: width, ry: a, c, col: true });
      }
    },
    canPlace, building, tree, car, pumpjack, billboard, stampLine: (pts, r, bit) => stampLine(pts, r, bit)
  };

  /* Footprint check: on dry land, clear of roads and landmarks, flat
   * enough. Returns the ground height to build from, or null. */
  function canPlace(x, z, hw, hd, a, maxSlope = 3, ignoreMask = 0) {
    let lo = Infinity, hi = -Infinity;
    // sample the footprint every ~6 m, so a big building can't straddle a road
    const nx = Math.max(2, Math.ceil((hw * 2) / 6)), nz = Math.max(2, Math.ceil((hd * 2) / 6));
    const pts = [];
    for (let i = 0; i <= nx; i++) for (let j = 0; j <= nz; j++) pts.push([-hw + (2 * hw * i) / nx, -hd + (2 * hd * j) / nz]);
    for (const [lx, lz] of pts) {
      const [ox, oz] = rot(lx, lz, a);
      const px = x + ox, pz = z + oz;
      if (Math.abs(px) > HALF - 20 || Math.abs(pz) > HALF - 20) return null;
      if (W.maskAt(px, pz) & ~ignoreMask) return null;
      const h = gh(px, pz);
      if (h < 1.0 || W.isSea(px, pz)) return null;
      lo = Math.min(lo, h); hi = Math.max(hi, h);
    }
    if (hi - lo > maxSlope) return null;
    return { lo, hi };
  }

  /* A building: a box with a window pattern and an optional pitched roof. */
  function building(x, z, w, d, h, a, color, win, roof = null, roofColor = null, ground = null) {
    const g = ground || { lo: gh(x, z), hi: gh(x, z) };
    const y = g.lo - 0.6;
    const top = g.hi + h;
    const b = { x, z, y, w, d, h: top - y, rot: a, color, win, roof, roofColor };
    W.buildings.push(b);
    addBox(x, z, w / 2, d / 2, a, y, top, 'building');
    if (roof) addBox(x, z, w / 4, d / 4, a, top, top + Math.min(w, d) * 0.28, 'roof');
    return b;
  }

  /* Trees: 0 palm, 1 shade tree, 2 jacaranda, 3 chaparral scrub, 4 cypress, 5 oak. */
  function tree(x, z, t, h) {
    const y = gh(x, z);
    W.trees.push({ x, y, z, t, h, s: 0.85 + rnd() * 0.3, r: rnd() * 6.28 });
    if (t === 0 || t === 4) addCircle(x, z, 0.45, y, y + h, 'tree');
    else if (t !== 3) addCircle(x, z, 0.6, y, y + h * 0.5, 'tree');
  }

  /* A parked or abandoned car, as instanced parts. */
  /* A parked or abandoned car. Drawn by streetlife.js from W.parked; here
   * it only needs to be solid. kind: sedan, van, suv, pickup, compact. */
  function car(x, z, a, color, kind = 'sedan') {
    const y = gh(x, z);
    const dims = { sedan: [4.85, 1.85, 1.45], van: [5.3, 2.0, 2.1], suv: [4.95, 1.95, 1.8], pickup: [5.6, 2.0, 1.9], compact: [4.35, 1.78, 1.45] }[kind] || [4.85, 1.85, 1.45];
    W.parked.push({ x, y, z, ry: a, color, kind, len: dims[0], wid: dims[1], hgt: dims[2] });
    addBox(x, z, dims[0] / 2, dims[1] / 2, a, y, y + dims[2], 'car');
  }

  /* An oil pumpjack — Baldwin Hills still has hundreds. The beam nods. */
  function pumpjack(x, z, a, id) {
    const y = gh(x, z);
    part({ s: 'box', x, y: y + 0.4, z, sx: 9, sy: 0.8, sz: 2.4, ry: a, c: '#4b4f54', col: true });
    for (const side of [-1, 1]) {
      const [ox, oz] = rot(0, side * 0.9, a);
      part({ s: 'box', x: x + ox, y: y + 3.2, z: z + oz, sx: 0.4, sy: 5.6, sz: 0.4, ry: a, rx: -side * 0.18, c: '#5d6166' });
    }
    const [hx, hz] = rot(-3.2, 0, a);
    part({ s: 'box', x: x + hx, y: y + 1.6, z: z + hz, sx: 1.6, sy: 2.4, sz: 1.6, ry: a, c: '#3c3f43' });
    const g = { x, y: y + 6, z, ry: a, anim: 'nod', speed: 0.9 + rnd() * 0.6, phase: rnd() * 6.28 };
    api.group(id, g);
    part({ s: 'box', g: id, x: 0, y: 0.3, z: 0, sx: 8.5, sy: 0.6, sz: 0.6, c: rnd.pick(['#2f6b3a', '#7a3b2e', '#3b4a6b', '#6b5d2f']) });
    part({ s: 'box', g: id, x: 4.4, y: -0.8, z: 0, sx: 1.2, sy: 2.6, sz: 0.7, c: '#2b2b2b' });
    part({ s: 'box', g: id, x: 4.6, y: -3.6, z: 0, sx: 0.12, sy: 4.5, sz: 0.12, c: '#222' });
  }

  function billboard(x, z, a, glowColor) {
    const y = gh(x, z);
    part({ s: 'cyl', x, y: y + 7, z, sx: 0.9, sy: 14, sz: 0.9, c: '#6a6d70', col: true });
    part({ s: 'box', x, y: y + 16, z, sx: 15, sy: 6, sz: 0.7, ry: a, c: '#2a2a2a' });
    const [fx, fz] = rot(0, 0.4, a);
    part({ s: 'box', x: x + fx, y: y + 16, z: z + fz, sx: 14, sy: 5.2, sz: 0.1, ry: a, c: glowColor, m: 'glow' });
    const [bx, bz] = rot(0, -0.4, a);
    part({ s: 'box', x: x + bx, y: y + 16, z: z + bz, sx: 14, sy: 5.2, sz: 0.1, ry: a, c: rnd.pick(['#f2c14e', '#e84a5f', '#48c9b0', '#5dade2', '#f5f5f5']), m: 'glow' });
  }

  /* ---- landmarks first, so the generic city fills in around them */
  buildLandmarks(api);
  W.stats.landmarkParts = P.length;

  /* ---- bridges where roads cross the river channel */
  for (const r of ROADS) {
    if (r.kind === 'local') continue;
    const w = ROAD_WIDTH[r.kind];
    for (const river of RIVERS) {
      for (let i = 0; i < r.pts.length - 1; i++) {
        for (let k = 0; k < river.pts.length - 1; k++) {
          const X = segIntersect(r.pts[i], r.pts[i + 1], river.pts[k], river.pts[k + 1]);
          if (!X) continue;
          const [x, z] = X;
          const a = Math.atan2(-(r.pts[i + 1][1] - r.pts[i][1]), r.pts[i + 1][0] - r.pts[i][0]);
          const span = (river.w + 12) * 2;
          const [ex, ez] = rot(span / 2, 0, a);
          const y = Math.max(gh(x + ex, z + ez), gh(x - ex, z - ez));
          part({ s: 'box', x, y: y - 0.4, z, sx: span, sy: 1.2, sz: w, ry: a, c: '#9c978c', col: true, tag: 'bridge' });
          W.bridges.push({ x, z, a, span, w, y });
        }
      }
    }
  }

  const signFor = (district) => {
    const pool = SIGN_POOLS[district] || SIGN_POOLS.default;
    return SIGN_TEXTS.indexOf(rnd.pick(pool));
  };

  /* The architecture: real LA building types at real dimensions. */
  const A = makeArchitecture({
    W, rnd, part, building, canPlace, tree, car, gh, signFor, MASK, paint: api.paint, carColors: PALETTE.car,
    donutCell: SIGN_TEXTS.indexOf('DONUTS'),
    helipad: (b) => helipad(b), rooftopKit: (b, water) => rooftopKit(b, water),
    rooftopUnit: (x, y, z, a) => part({ s: 'box', x, y: y + 0.7, z, sx: 2.2, sy: 1.4, sz: 1.6, ry: a, c: '#9a9ea2', m: 'metal' })
  });
  const blvdRoads = ROADS.filter((r) => r.kind === 'blvd');
  const CORNER_STORE = { 'East LA': 0.45, 'Boyle Heights': 0.45, 'Highland Park': 0.3, Watts: 0.35, Compton: 0.3, 'South LA': 0.35, Vernon: 0.3, 'Echo Park': 0.25, 'Silver Lake': 0.2, Inglewood: 0.25, 'Leimert Park': 0.25, Gardena: 0.2 };
  const onBoulevard = (x, z) => blvdRoads.some((r) => polyDist(r.pts, x, z) < ROAD_WIDTH.blvd / 2 + 12);

  /* ---- the city: every neighbourhood's street grid, block by block */
  const core = project(34.0505, -118.2550);
  let blockCount = 0;
  for (let di = 0; di < DISTRICTS.length; di++) {
    const d = DISTRICTS[di];
    const st = STYLES[d.style];
    if (!st) continue;
    const px = st.bx + st.st, pz = st.bz + st.st;
    const reach = 2700;
    const ni = Math.ceil(reach / px), nj = Math.ceil(reach / pz);
    for (let j = -nj; j <= nj; j++) {
      for (let i = -ni; i <= ni; i++) {
        const [ox, oz] = rot(i * px, j * pz, d.rot);
        const cx = d.x + ox, cz = d.z + oz;
        if (Math.abs(cx) > HALF - 50 || Math.abs(cz) > HALF - 50) continue;
        if (W.districtIndexAt(cx, cz) !== di) continue;
        let lo = Infinity, hi = -Infinity, wet = false;
        for (const [lx, lz] of [[0, 0], [st.bx / 2, st.bz / 2], [-st.bx / 2, st.bz / 2], [st.bx / 2, -st.bz / 2], [-st.bx / 2, -st.bz / 2]]) {
          const [qx, qz] = rot(lx, lz, d.rot);
          const h = gh(cx + qx, cz + qz);
          if (h < 0.8 || W.isSea(cx + qx, cz + qz)) wet = true;
          lo = Math.min(lo, h); hi = Math.max(hi, h);
        }
        if (wet || hi - lo > 11) continue;
        if (gh(cx, cz) - terrain.base(cx, cz) > 26) continue;
        const blk = { x: cx, z: cz, w: st.bx, d: st.bz, rot: d.rot, color: st.lot, style: d.style, district: d.name };
        W.blocks.push(blk);
        blockCount++;
        genBlock(blk, d, Math.hypot(cx - core[0], cz - core[1]));
      }
    }
  }
  W.stats.blocks = blockCount;

  function toWorld(blk, lx, lz) {
    const [ox, oz] = rot(lx, lz, blk.rot);
    return [blk.x + ox, blk.z + oz];
  }

  /* Lots in a row along both long sides of a block. Anything fronting a
   * boulevard becomes commercial, as in the real city: storefronts, the odd
   * mini-mall, a gas station on the corner. */
  function rowOfLots(blk, o) {
    const L = blk.w, Dd = blk.d, D = Dd / 2;
    for (const side of [-1, 1]) {
      const yaw = blk.rot + (side > 0 ? 0 : Math.PI);
      let lx = -L / 2 + 1, first = true;
      while (lx < L / 2 - 6) {
        const [fx, fz] = toWorld(blk, lx + 8, side * (Dd / 2 + 6));
        let kind = o.kind;
        let corner = false;
        if (kind !== 'shop' && kind !== 'mansion' && onBoulevard(fx, fz)) kind = 'shop';
        // the corner market (tienda, liquor store) on the end of a residential block
        else if (kind === 'house' && first && rnd.chance(CORNER_STORE[blk.district] ?? 0.08)) { kind = 'shop'; corner = true; }
        let lw;
        if (corner) lw = rnd.range(12, 16);
        else if (kind === 'shop') {
          if (first && rnd.chance(0.25) && L / 2 - lx >= 36) { kind = 'gas'; lw = 34; }
          else if (rnd.chance(0.14) && L / 2 - lx >= 48 && D >= 38) { kind = 'mall'; lw = 46; }
          else lw = rnd.range(10, 22);
        } else lw = rnd.range(o.lot[0], o.lot[1]);
        if (lx + lw > L / 2) break;
        const [cx, cz] = toWorld(blk, lx + lw / 2, side * D / 2);
        const F = A.frame(cx, cz, yaw);
        let built = false;
        if (kind === 'house') {
          const hk = o.houseKind ? o.houseKind() : houseKind(blk.district, blk.style, rnd);
          built = hk === 'dingbat' ? A.dingbat(blk, F, lw, D) : A.house(blk, F, lw, D, hk);
        } else if (kind === 'beach') built = A.beachHouse(F, lw, D);
        else if (kind === 'mansion') built = A.mansion(F, lw, D);
        else if (kind === 'gas') built = A.gasStation(blk, F, lw, D);
        else if (kind === 'mall') built = A.miniMall(blk, F, lw, D);
        else if (kind === 'apartment') {
          if (rnd.chance(0.2)) built = A.dingbat(blk, F, lw, D);
          else {
            const d = Math.min(D - 6, rnd.range(16, 24)), tall = rnd.chance(o.tallP || 0);
            const F2 = A.frame(...F.at(0, D / 2 - 2.5 - d / 2), yaw);
            built = !!A.midrise(blk, F2, lw - 2, d, tall ? rnd.range(25, 42) : rnd.range(10, 18), { aptP: 0.8, retailP: 0.2, palette: o.palette });
          }
        } else if (kind === 'shop') {
          if (o.tallP && rnd.chance(o.tallP) && lw >= 16) {
            const d = Math.min(D - 4, rnd.range(18, 26));
            const F2 = A.frame(...F.at(0, D / 2 - 1 - d / 2), yaw);
            built = !!A.midrise(blk, F2, lw - 0.6, d, rnd.range(...(o.tall || [18, 36])), { deco: rnd.chance(0.4), retailP: 0.9 });
          } else built = A.storefront(blk, F, lw, D, { twoP: corner ? 0.2 : o.twoP });
        }
        // a car parked at the kerb out front
        if (built && o.carP && kind !== 'gas' && kind !== 'mall' && rnd.chance(kind === 'shop' ? o.carP * 0.6 : o.carP)) {
          const [px, pz] = toWorld(blk, lx + lw / 2, side * (Dd / 2 + 4.6));
          if (W.maskAt(px, pz) === 0) car(px, pz, blk.rot + (side > 0 ? 0 : Math.PI), rnd.pick(PALETTE.car), rnd.pick(['sedan', 'sedan', 'compact', 'suv', 'pickup']));
        }
        lx += lw;
        first = false;
      }
    }
    if (o.curbPalms) {
      for (const side of [-1, 1]) {
        for (let lx = -L / 2 + 6; lx < L / 2 - 4; lx += o.curbPalms) {
          const [x, z] = toWorld(blk, lx, side * (Dd / 2 + 1.4));
          if (W.maskAt(x, z) === 0) tree(x, z, 0, rnd.range(16, 24));
        }
      }
    }
  }

  /* One to four big buildings sharing a block, each in its own frame. */
  function blockOfBuildings(blk, n, make) {
    const L = blk.w, Dd = blk.d;
    const cells = n === 1 ? [[0, 0, L, Dd]] : n === 2 ? [[-L / 4, 0, L / 2, Dd], [L / 4, 0, L / 2, Dd]]
      : [[-L / 4, -Dd / 4, L / 2, Dd / 2], [L / 4, -Dd / 4, L / 2, Dd / 2], [-L / 4, Dd / 4, L / 2, Dd / 2], [L / 4, Dd / 4, L / 2, Dd / 2]];
    for (const [lx, lz, cw, cd] of cells) {
      // face the nearer long side of the block: that's the street
      const yaw = blk.rot + (lz < 0 ? Math.PI : 0);
      const F = A.frame(...toWorld(blk, lx, lz), yaw);
      make(F, cw, cd);
    }
  }

  /* Old LA fire code put a helipad on every tall tower: hence the flat tops. */
  function helipad(b) {
    const top = b.y + b.h;
    const r = Math.min(b.w, b.d) * 0.36;
    api.cyl(b.x, top + 0.15, b.z, r * 2, 0.3, r * 2, '#4a4d52');
    api.box(b.x, top + 0.32, b.z, r * 0.2, 0.05, r * 0.9, '#f4f4f4', { ry: b.rot });
    const [ax, az] = rot(r * 0.35, 0, b.rot);
    api.box(b.x + ax, top + 0.32, b.z + az, r * 0.14, 0.05, r * 0.9, '#f4f4f4', { ry: b.rot });
    api.box(b.x - ax, top + 0.32, b.z - az, r * 0.14, 0.05, r * 0.9, '#f4f4f4', { ry: b.rot });
    api.box(b.x, top + 0.32, b.z, r * 0.7, 0.05, r * 0.14, '#f4f4f4', { ry: b.rot });
  }
  function rooftopKit(b, water = false) {
    const top = b.y + b.h;
    const [ox, oz] = rot(b.w * 0.2, -b.d * 0.15, b.rot);
    api.box(b.x + ox, top + 1.2, b.z + oz, Math.min(6, b.w * 0.3), 2.4, Math.min(5, b.d * 0.3), '#8b8f94', { ry: b.rot, col: true });
    if (water || rnd.chance(0.4)) {
      const [wx, wz] = rot(-b.w * 0.25, b.d * 0.2, b.rot);
      api.cyl(b.x + wx, top + 3, b.z + wz, 3, 3, 3, '#7a5b3e');
      for (const [lx, lz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) api.box(b.x + wx + lx, top + 1, b.z + wz + lz, 0.2, 2, 0.2, '#555');
    }
  }

  function genBlock(blk, d, coreDist) {
    const deco = ['Koreatown', 'Miracle Mile', 'Hollywood', 'Long Beach', 'Downtown LA', 'Mid-City'].includes(d.name) ? 0.4 : 0.15;
    switch (d.style) {
      case 'residential':
        return rowOfLots(blk, { kind: 'house', lot: [14.2, 16.2], carP: 0.45 });
      case 'suburb':
        return rowOfLots(blk, { kind: 'house', lot: [19, 23], carP: 0.3 });
      case 'beach':
        return rowOfLots(blk, { kind: 'beach', lot: [8.5, 11], carP: 0.25 });
      case 'coastal':
        return rowOfLots(blk, { kind: 'apartment', lot: [18, 30], tallP: 0.12, palette: [...PALETTE.stucco, ...PALETTE.white], carP: 0.35, curbPalms: 20 });
      case 'mansion':
        return rowOfLots(blk, { kind: 'mansion', lot: [36, 52], carP: 0.08, curbPalms: 17 });
      case 'mixed':
        return rowOfLots(blk, { kind: 'shop', tallP: 0.08, tall: [18, 40], twoP: 0.35, carP: 0.3 });
      case 'hollywood':
        return rowOfLots(blk, { kind: 'shop', tallP: 0.16, tall: [24, 60], twoP: 0.45, carP: 0.3 });
      case 'midrise':
        return blockOfBuildings(blk, rnd.chance(0.5) ? 4 : 2, (F, cw, cd) => {
          if (rnd.chance(0.1)) return;
          const tall = rnd.chance(0.08);
          const F2 = A.frame(...F.at(0, cd / 2 - 2 - (cd - 8) / 2), F.yaw);
          A.midrise(blk, F2, cw - rnd.range(4, 10), cd - 8, tall ? rnd.range(60, 110) : rnd.range(14, 42), { deco: !tall && rnd.chance(deco), palette: [...PALETTE.stucco, ...PALETTE.brick] });
        });
      case 'towers':
        return blockOfBuildings(blk, rnd.chance(0.6) ? 1 : 2, (F, cw, cd) => A.tower(blk, F, cw - rnd.range(14, 26), cd - rnd.range(14, 26), rnd.range(55, 165)));
      case 'downtown': {
        const near = coreDist < 560;
        return blockOfBuildings(blk, near ? (rnd.chance(0.5) ? 1 : 2) : (rnd.chance(0.5) ? 2 : 4), (F, cw, cd) => {
          const h = near ? (rnd.chance(0.25) ? rnd.range(150, 250) : rnd.range(60, 150)) : (rnd.chance(0.15) ? rnd.range(70, 130) : rnd.range(18, 60));
          const w = cw - rnd.range(4, 14), dd = cd - rnd.range(4, 14);
          if (h > 50) A.tower(blk, F, w, dd, h);
          else A.midrise(blk, F, w, dd, h, { deco: rnd.chance(0.45), retailP: 0.8, palette: [...PALETTE.brick, ...PALETTE.stucco] });
        });
      }
      case 'industrial':
        if ((d.name === 'El Segundo' || d.name === 'Carson') && rnd.chance(0.35)) return tankFarm(blk);
        return blockOfBuildings(blk, rnd.chance(0.5) ? 1 : 2, (F, cw, cd) => A.warehouse(blk, F, cw - rnd.range(14, 30), cd - rnd.range(16, 30), { brick: d.name === 'Arts District' ? rnd.chance(0.8) : rnd.chance(0.15) }));
      case 'campus':
        blockOfBuildings(blk, 4, (F, cw, cd) => { if (!rnd.chance(0.25)) A.campusHall(blk, F, cw - rnd.range(16, 26), cd - rnd.range(18, 26)); });
        for (let k = 0; k < 6; k++) { const [x, z] = toWorld(blk, rnd.range(-40, 40), rnd.range(-25, 25)); if (canPlace(x, z, 1, 1, 0)) tree(x, z, rnd.pick([1, 1, 2, 0, 5]), rnd.range(8, 14)); }
        return;
      case 'port':
        return rnd.chance(0.25) ? blockOfBuildings(blk, 1, (F, cw, cd) => A.warehouse(blk, F, cw - 30, cd - 30, { brick: false, fence: true })) : containerYard(blk);
    }
  }

  function tankFarm(blk) {
    for (const [lx, lz] of [[-45, -22], [0, -22], [45, -22], [-45, 22], [0, 22], [45, 22]]) {
      const r = rnd.range(9, 15);
      const [x, z] = toWorld(blk, lx, lz);
      const g = canPlace(x, z, r, r, 0, 4);
      if (!g) continue;
      const h = rnd.range(9, 16);
      api.cyl(x, g.lo + h / 2, z, r * 2, h, r * 2, rnd.pick(['#e9e6df', '#d8d4cb', '#c9c2b2']), { col: true });
    }
  }

  function containerYard(blk) {
    const colors = ['#c0392b', '#2e86c1', '#1e8449', '#d68910', '#7d3c98', '#5d6d7e', '#a04000', '#117a65', '#f4f6f7'];
    for (let lz = -blk.d / 2 + 6; lz < blk.d / 2 - 4; lz += 6.5) {
      for (let lx = -blk.w / 2 + 8; lx < blk.w / 2 - 8; lx += 13) {
        if (!rnd.chance(0.7)) continue;
        const [x, z] = toWorld(blk, lx, lz);
        const g = canPlace(x, z, 6.2, 1.3, blk.rot, 2);
        if (!g) continue;
        const n = rnd.int(1, 4);
        for (let k = 0; k < n; k++) api.box(x, g.lo + 1.3 + k * 2.6, z, 12.2, 2.55, 2.45, rnd.pick(colors), { ry: blk.rot });
        addBox(x, z, 6.1, 1.25, blk.rot, g.lo, g.lo + n * 2.6, 'container');
      }
    }
  }

  /* ---- hillside houses (Hollywood Hills, Bel Air, Laurel Canyon): no grid,
   * they cling to whatever slope will hold them */
  for (let z = -HALF + 30; z < HALF; z += 46) {
    for (let x = -HALF + 30; x < HALF; x += 46) {
      const px = x + rnd.range(-14, 14), pz = z + rnd.range(-14, 14);
      const d = W.districtAt(px, pz);
      if (d.style !== 'hills' && !(d.style === 'mansion' && gh(px, pz) - terrain.base(px, pz) > 26)) continue;
      if (!rnd.chance(0.42)) continue;
      const a = rnd.range(0, Math.PI * 2);
      // face downhill, towards the view
      const [nx, , nz] = W.normalAt(px, pz);
      const yaw = Math.hypot(nx, nz) > 0.02 ? Math.atan2(nx, nz) : a;
      A.hillside(px, pz, yaw);
    }
  }

  /* ---- oil field pumpjacks */
  let jacks = 0;
  for (let z = -HALF; z < HALF; z += 64) {
    for (let x = -HALF; x < HALF; x += 64) {
      const px = x + rnd.range(-20, 20), pz = z + rnd.range(-20, 20);
      if (W.districtAt(px, pz).style !== 'oilfield' || !rnd.chance(0.5)) continue;
      const a = rnd.range(0, Math.PI * 2);
      if (!canPlace(px, pz, 5, 2, a, 3)) continue;
      pumpjack(px, pz, a, `jack${jacks++}`);
      api.reserveDisc(px, pz, 8);
    }
  }
  W.stats.pumpjacks = jacks;

  /* ---- palms along the boulevards: the postcard LA street */
  for (const r of ROADS) {
    if (r.kind !== 'blvd') continue;
    const off = ROAD_WIDTH.blvd / 2 + 2.2;
    walk(r.pts, 21, (x, z, dx, dz) => {
      for (const side of [-1, 1]) {
        const px = x - dz * off * side, pz = z + dx * off * side;
        if (!rnd.chance(0.8)) continue;
        const m = W.maskAt(px, pz);
        if (m & (MASK.RESERVED | MASK.CHANNEL | MASK.LAKE)) continue;
        if (polyDistAll(px, pz) < 2) continue;
        const h = gh(px, pz);
        if (h < 1 || W.isSea(px, pz) || W.slopeAt(px, pz) > 0.2) continue;
        tree(px, pz, 0, rnd.range(17, 26));
      }
    });
  }
  function polyDistAll(x, z) {
    let best = Infinity;
    for (const r of ROADS) {
      const hw = ROAD_WIDTH[r.kind] / 2;
      const d = polyDist(r.pts, x, z) - hw;
      if (d < best) best = d;
    }
    return best;
  }

  /* ---- street furniture: lights, poles, signals, signs, hydrants */
  buildStreets(api, W);

  /* ---- the 405 at rush hour, forever: abandoned traffic on the freeways */
  let jam = 0;
  for (const r of ROADS) {
    if (r.kind !== 'freeway') continue;
    const p = r.name.startsWith('I-405') ? 0.5 : r.name.startsWith('US-101') || r.name.startsWith('I-10') ? 0.22 : 0.1;
    walk(r.pts, 10, (x, z, dx, dz) => {
      if (!rnd.chance(p)) return;
      const lane = rnd.pick([-12, -8.5, -5, 5, 8.5, 12]);
      const px = x - dz * lane, pz = z + dx * lane;
      if (Math.abs(px) > HALF - 30 || Math.abs(pz) > HALF - 30) return;
      if (W.isSea(px, pz) || W.maskAt(px, pz) & (MASK.CHANNEL | MASK.RESERVED)) return;
      const a = Math.atan2(-dz, dx) + (lane < 0 ? Math.PI : 0) + rnd.range(-0.25, 0.25);
      car(px, pz, a, rnd.pick(PALETTE.car), rnd.chance(0.12) ? 'van' : 'sedan');
      jam++;
    });
  }
  W.stats.abandonedCars = jam;

  /* ---- billboards along the freeways */
  for (const r of ROADS) {
    if (r.kind !== 'freeway') continue;
    walk(r.pts, 380, (x, z, dx, dz) => {
      if (!rnd.chance(0.45)) return;
      const side = rnd.chance(0.5) ? 1 : -1;
      const px = x - dz * 28 * side, pz = z + dx * 28 * side;
      if (W.maskAt(px, pz) || W.isSea(px, pz) || gh(px, pz) < 1) return;
      billboard(px, pz, Math.atan2(-dz, dx), rnd.pick(['#ff4f8b', '#4fd1ff', '#ffd23f', '#7cff6b', '#ff8c42', '#b388ff']));
    });
  }

  /* ---- chaparral on the hills, trees in the parks */
  for (let z = -HALF + 11; z < HALF; z += 22) {
    for (let x = -HALF + 11; x < HALF; x += 22) {
      const px = x + rnd.range(-9, 9), pz = z + rnd.range(-9, 9);
      const s = W.styleAt(px, pz);
      let t = -1, h = 0;
      if (s === 'wild' || s === 'hills') {
        const r = rnd();
        if (r < 0.34) { t = 3; h = rnd.range(1.4, 3); }
        else if (r < 0.39) { t = 5; h = rnd.range(7, 11); }
        else if (r < 0.41 && s === 'hills') { t = 4; h = rnd.range(10, 15); }
      } else if (s === 'park') {
        const r = rnd();
        if (r < 0.24) { t = 1; h = rnd.range(8, 14); } else if (r < 0.3) { t = 5; h = rnd.range(8, 12); } else if (r < 0.33) { t = 0; h = rnd.range(16, 24); } else if (r < 0.45) { t = 3; h = rnd.range(1.4, 2.6); }
      } else if (s === 'oilfield') {
        if (rnd() < 0.25) { t = 3; h = rnd.range(1.2, 2.4); }
      }
      if (t < 0) continue;
      if (W.maskAt(px, pz) || W.isSea(px, pz)) continue;
      const y = gh(px, pz);
      if (y < 1.5 || W.slopeAt(px, pz) > 0.45) continue;
      tree(px, pz, t, h);
    }
  }

  /* ---- lifeguard towers along the sand */
  {
    let acc = 0, n = 0;
    for (let i = 0; i < COAST.length - 1; i++) {
      const [ax, az] = COAST[i], [bx, bz] = COAST[i + 1];
      const len = Math.hypot(bx - ax, bz - az);
      const dx = (bx - ax) / len, dz = (bz - az) / len;
      for (let s = 0; s < len; s += 10) {
        acc += 10;
        if (acc < 240) continue;
        acc = 0;
        const x = ax + dx * s + dz * 38, z = az + dz * s - dx * 38;
        if (Math.abs(x) > HALF - 40 || Math.abs(z) > HALF - 40) continue;
        const y = gh(x, z);
        if (y < 0.4 || y > 5 || W.maskAt(x, z)) continue;
        lifeguard(x, z, Math.atan2(dz, -dx) + Math.PI, n++);
      }
    }
  }
  function lifeguard(x, z, a, n) {
    const y = gh(x, z);
    for (const [lx, lz] of [[-1.3, -1.3], [1.3, -1.3], [-1.3, 1.3], [1.3, 1.3]]) {
      const [ox, oz] = rot(lx, lz, a);
      api.box(x + ox, y + 1.2, z + oz, 0.25, 2.4, 0.25, '#e8e8e8');
    }
    api.box(x, y + 2.5, z, 3.4, 0.3, 3.4, '#e8e8e8', { ry: a, col: true });
    api.box(x, y + 3.9, z, 3, 2.6, 3, n % 3 === 0 ? '#5fb7e5' : n % 3 === 1 ? '#f2d16b' : '#9fd8cb', { ry: a, col: true });
    api.box(x, y + 5.35, z, 3.6, 0.3, 3.6, '#f4f4f4', { ry: a });
    const [rx, rz] = rot(0, 3.8, a);
    api.ramp(x + rx, z + rz, y, x + rx * 0.45, z + rz * 0.45, y + 2.6, 1.4, '#d8d0c0', 0.35);
  }

  /* ---- drivable cars, parked near the places people would drop */
  const spawns = [
    ['Santa Monica Pier', 'convertible'], ['Venice Beach', 'lowrider'], ['Venice Beach', 'convertible'],
    ['Malibu Pier', 'convertible'], ['LAX', 'cruiser'], ['LAX', 'taxi'], ['SoFi Stadium', 'suv'],
    ["Randy's Donuts", 'lowrider'], ['TCL Chinese Theatre', 'convertible'], ['Hollywood Sign', 'suv'],
    ['Griffith Observatory', 'convertible'], ['Dodger Stadium', 'lowrider'], ['Dodger Stadium', 'cruiser'],
    ['City Hall', 'cruiser'], ['Crypto.com Arena', 'taxi'], ['Union Station', 'taxi'], ['Rodeo Drive', 'exotic'],
    ['Beverly Hills Hotel', 'exotic'], ['Getty Center', 'suv'], ['UCLA', 'convertible'], ['Century City', 'exotic'],
    ['LA Memorial Coliseum', 'suv'], ['Watts Towers', 'lowrider'], ['Mariachi Plaza', 'tacotruck'],
    ['East LA', 'lowrider'], ['Port of Los Angeles', 'truck'], ['Queen Mary', 'convertible'], ['Universal Studios', 'taxi'],
    ['Burbank', 'suv'], ['Rose Bowl', 'convertible'], ['Urban Light (LACMA)', 'exotic'], ['Koreatown', 'taxi'],
    ['Echo Park Lake', 'lowrider'], ['Manhattan Beach Pier', 'convertible'], ['SpaceX Hawthorne', 'suv'],
    ['Culver City', 'taxi'], ['Marina del Rey', 'convertible'], ['Sunset Strip', 'exotic'], ['Arts District', 'tacotruck'],
    ['Pasadena', 'suv'], ['Sherman Oaks', 'convertible'], ['Baldwin Hills', 'truck']
  ];
  for (const [where, type] of spawns) {
    const target = W.pois.find((p) => p.name === where) || DISTRICTS.find((d) => d.name === where);
    if (!target) continue;
    const spot = nearestRoadSpot(target.x, target.z, 700);
    if (spot) W.vehicles.push({ ...spot, type });
  }
  function nearestRoadSpot(x, z, maxD) {
    let best = null, bd = maxD;
    for (const r of ROADS) {
      if (r.kind === 'local') continue;
      const d = polyDist(r.pts, x, z);
      if (d >= bd) continue;
      const [ax, az] = r.pts[POLY_I], [bx, bz] = r.pts[POLY_I + 1];
      const len = Math.hypot(bx - ax, bz - az), dx = (bx - ax) / len, dz = (bz - az) / len;
      const off = ROAD_WIDTH[r.kind] / 4;
      for (let k = 0; k < 12; k++) {
        const t = clamp(POLY_T + (k % 2 ? 1 : -1) * Math.ceil(k / 2) * (9 / len), 0, 1);
        const px = lerp(ax, bx, t) - dz * off, pz = lerp(az, bz, t) + dx * off;
        if (W.isSea(px, pz) || W.maskAt(px, pz) & (MASK.CHANNEL | MASK.LAKE)) continue;
        if (W.queryPoint(px, gh(px, pz) + 1, pz, 3.2)) continue;
        if (W.vehicles.some((v) => Math.hypot(v.x - px, v.z - pz) < 8)) continue;
        best = { x: px, z: pz, ry: Math.atan2(-dz, dx) }; bd = d; break;
      }
    }
    return best;
  }

  W.stats.genMs = Date.now() - t0;
  return W;
}

/* Walk a polyline, calling fn(x, z, dirX, dirZ) every `step` metres. */
export function walk(pts, step, fn) {
  let carry = 0;
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[i + 1];
    const len = Math.hypot(bx - ax, bz - az);
    if (len === 0) continue;
    const dx = (bx - ax) / len, dz = (bz - az) / len;
    let s = carry;
    for (; s < len; s += step) fn(ax + dx * s, az + dz * s, dx, dz);
    carry = s - len;
  }
}

function segIntersect(a, b, c, d) {
  const r0 = b[0] - a[0], r1 = b[1] - a[1], s0 = d[0] - c[0], s1 = d[1] - c[1];
  const den = r0 * s1 - r1 * s0;
  if (Math.abs(den) < 1e-9) return null;
  const t = ((c[0] - a[0]) * s1 - (c[1] - a[1]) * s0) / den;
  const u = ((c[0] - a[0]) * r1 - (c[1] - a[1]) * r0) / den;
  if (t < 0 || t > 1 || u < 0 || u > 1) return null;
  return [a[0] + r0 * t, a[1] + r1 * t];
}

/* ------------------------------------------------------------ collision */

/*
 * The collider grid is built lazily the first time anything asks, because
 * the generators themselves ask (to keep cars off each other) while the
 * colliders are still being added. Adding a collider marks it stale.
 */
function ensureGrid(W) {
  if (W._grid && W._gridCount === W.colliders.length) return;
  const cells = new Map();
  for (let n = 0; n < W.colliders.length; n++) {
    const c = W.colliders[n];
    const r = c.r || Math.hypot(c.hx, c.hz);
    const i0 = Math.floor((c.x - r + HALF) / CG), i1 = Math.floor((c.x + r + HALF) / CG);
    const j0 = Math.floor((c.z - r + HALF) / CG), j1 = Math.floor((c.z + r + HALF) / CG);
    for (let j = j0; j <= j1; j++) {
      for (let i = i0; i <= i1; i++) {
        const k = j * CG_N + i;
        let list = cells.get(k);
        if (!list) cells.set(k, (list = []));
        list.push(n);
      }
    }
  }
  W._grid = cells;
  W._gridCount = W.colliders.length;
  W._stamp = new Uint32Array(W.colliders.length);
  W._stampN = 0;
}

/* Colliders whose footprint might touch the circle (x, z, r). */
export function query(W, x, z, r, out = []) {
  ensureGrid(W);
  out.length = 0;
  const stamp = ++W._stampN;
  const i0 = Math.floor((x - r + HALF) / CG), i1 = Math.floor((x + r + HALF) / CG);
  const j0 = Math.floor((z - r + HALF) / CG), j1 = Math.floor((z + r + HALF) / CG);
  for (let j = j0; j <= j1; j++) {
    for (let i = i0; i <= i1; i++) {
      const list = W._grid.get(j * CG_N + i);
      if (!list) continue;
      for (const n of list) {
        if (W._stamp[n] === stamp) continue;
        W._stamp[n] = stamp;
        out.push(W.colliders[n]);
      }
    }
  }
  return out;
}

/* How far circle (x, z, r) sinks into collider c, and which way is out.
 * Returns false if they don't touch; otherwise fills PEN. */
const PEN = { d: 0, nx: 0, nz: 0 };
export function penetrate(c, x, z, r) {
  if (c.k === 1) {
    const dx = x - c.x, dz = z - c.z;
    const d = Math.sqrt(dx * dx + dz * dz);
    if (d >= c.r + r) return false;
    PEN.d = c.r + r - d;
    PEN.nx = d > 1e-6 ? dx / d : 1; PEN.nz = d > 1e-6 ? dz / d : 0;
    return true;
  }
  const dx = x - c.x, dz = z - c.z;
  const lx = dx * c.c - dz * c.s, lz = dx * c.s + dz * c.c;
  const qx = clamp(lx, -c.hx, c.hx), qz = clamp(lz, -c.hz, c.hz);
  const ex = lx - qx, ez = lz - qz;
  const d2 = ex * ex + ez * ez;
  let nlx, nlz;
  if (d2 > 1e-12) {
    const d = Math.sqrt(d2);
    if (d >= r) return false;
    PEN.d = r - d; nlx = ex / d; nlz = ez / d;
  } else {
    const px = c.hx - Math.abs(lx), pz = c.hz - Math.abs(lz);
    if (px < pz) { PEN.d = r + px; nlx = Math.sign(lx) || 1; nlz = 0; } else { PEN.d = r + pz; nlx = 0; nlz = Math.sign(lz) || 1; }
  }
  // back to world: inverse of the local transform
  PEN.nx = nlx * c.c + nlz * c.s;
  PEN.nz = -nlx * c.s + nlz * c.c;
  return true;
}

/*
 * Resolve a cylinder body {x, y, z, vx, vy, vz, r, h, step} against the
 * world: push it out of walls, stand it on roofs, terrain and steps.
 * Sets body.onGround and body.groundY.
 */
const NEAR = [];
export function collideBody(W, b) {
  let support = W.groundHeight(b.x, b.z);
  for (let iter = 0; iter < 3; iter++) {
    query(W, b.x, b.z, b.r + 1, NEAR);
    let moved = false;
    for (const c of NEAR) {
      if (!penetrate(c, b.x, b.z, b.r)) continue;
      if (b.y >= c.y1 - b.step) {
        // standing on (or stepping up onto) its top — if mostly over it
        if (PEN.d > b.r * 0.35 && c.y1 > support) support = c.y1;
        continue;
      }
      if (b.y + b.h <= c.y0) continue;           // passing underneath
      b.x += PEN.nx * PEN.d; b.z += PEN.nz * PEN.d;
      const vn = b.vx * PEN.nx + b.vz * PEN.nz;
      if (vn < 0) { b.vx -= vn * PEN.nx; b.vz -= vn * PEN.nz; b.hitWall = Math.max(b.hitWall || 0, -vn); }
      moved = true;
    }
    if (!moved) break;
  }
  const g = W.groundHeight(b.x, b.z);
  if (g > support) support = g;
  b.groundY = support;
  if (b.y <= support + 0.05 && b.vy <= 0.01) {
    if (b.y < support - b.step * 1.5 && b.vy <= 0) b.y = support; // never sink through
    b.y = support; b.vy = 0; b.onGround = true;
  } else {
    b.onGround = false;
  }
  if (b.y < support - 0.5) b.y = support;
}

/* Is a point inside any collider (with some radius)? */
export function queryPoint(W, x, y, z, r = 0) {
  query(W, x, z, r + 1, NEAR);
  for (const c of NEAR) {
    if (y < c.y0 || y > c.y1) continue;
    if (penetrate(c, x, z, Math.max(r, 0.01))) return c;
  }
  return null;
}

/* March from a to b; return the fraction of the way that is clear (for the
 * camera, so it never ends up inside a wall or under the hills). */
export function rayClear(W, ax, ay, az, bx, by, bz, pad = 0.4) {
  const len = Math.hypot(bx - ax, by - ay, bz - az);
  const n = Math.max(2, Math.ceil(len / 0.6));
  for (let i = 1; i <= n; i++) {
    const t = i / n;
    const x = lerp(ax, bx, t), y = lerp(ay, by, t), z = lerp(az, bz, t);
    if (y < W.groundHeight(x, z) + pad || queryPoint(W, x, y, z, pad)) return Math.max(0, (i - 1) / n);
  }
  return 1;
}

/* Where am I? The nearest landmark if you're at one, plus the neighbourhood. */
export function locationAt(W, x, z) {
  let poi = null, best = Infinity;
  for (const p of W.pois) {
    const d = Math.hypot(p.x - x, p.z - z);
    if (d < p.r && d < best) { best = d; poi = p; }
  }
  const district = W.isSea(x, z) ? (z > 2200 && x > 500 ? 'San Pedro Bay' : 'Pacific Ocean') : W.districtAt(x, z).name;
  return { district, poi: poi ? poi.name : null };
}
