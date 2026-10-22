/*
 * Trees and plants: the palms, street trees, chaparral and front-yard
 * planting of Los Angeles, at their real sizes.
 *
 *  - Mexican fan palms (Washingtonia robusta): the skinny 20-30 m street
 *    palms, a small round crown of fan fronds, some with a skirt of dead ones.
 *  - Canary Island date palms: thick trunks, 12-18 m, big feathery crowns
 *    (the Beverly Hills and Hollywood palm).
 *  - King palms: slim grey trunks, a green crownshaft, arching fronds.
 *  - Street trees: Indian laurel fig (ficus), California sycamore, coral tree
 *    (LA's official tree), southern magnolia, carrotwood; jacaranda in May
 *    lavender; coast live oak on the hills; Italian cypress by the houses.
 *  - Chaparral and dry golden grass on the hills.
 *  - Front yards: bird-of-paradise (the city flower), agave, bougainvillea.
 *
 * Every plant is a merged geometry of textured cards (leaf clusters on
 * transparency) on a real trunk, all sharing one texture atlas, instanced
 * per 1 km tile. Called from render.js with its batching helpers (ctx).
 */

import * as THREE from '../vendor/three.module.min.js';

/* The atlas: 4 × 4 cells. Grey cells are tinted by vertex colour; coloured
 * cells carry their own colour (vertex colour white). */
const CELL = {
  leaf: 0, jacaranda: 1, fan: 2, feather: 3, skirt: 4, grass: 5, needles: 6, bark: 7,
  coral: 8, bougain: 9, paradise: 10, magnolia: 11, sycBark: 12, palmBark: 13, canaryBark: 14, shrub: 15
};

/* The textures, materials and tree models are built once and shared by
 * every call (the true-scale map calls buildFlora once per streamed tile). */
let LIB = null;
function library(ctx) {
  const key = `${ctx.lo}|${ctx.hi}`;
  if (LIB && LIB.key === key) return LIB;
  const { lo, hi, fadeNearCamera, mulberry } = ctx;
  const atlas = makeAtlas(mulberry);
  const mat = fadeNearCamera(new THREE.MeshStandardMaterial({ map: atlas, vertexColors: true, alphaTest: 0.42, roughness: 0.82, side: THREE.DoubleSide }));
  const solid = fadeNearCamera(new THREE.MeshStandardMaterial({ map: atlas, vertexColors: true, roughness: 0.9 }));
  const far = lo ? 900 : hi ? 2000 : 1600;
  const detail = lo ? 0.55 : 1;
  const hash = (x, z) => { const s = Math.sin(x * 12.9898 + z * 78.233) * 43758.5453; return s - Math.floor(s); };

  /* --------------------------------------------- the geometry library */
  const G = {};
  const variants = (fn, n) => Array.from({ length: n }, (_, i) => fn(mulberry(1000 + i * 77 + fn.length)));
  G.fanTrunk = palmTrunk(0.19, 0.3, 0.42, CELL.palmBark, '#c9bba5', true);
  G.fanCrown = variants((r) => fanCrown(r, true), 2).concat(variants((r) => fanCrown(r, false), 1));
  G.canaryTrunk = palmTrunk(0.42, 0.46, 0.55, CELL.canaryBark, '#d8cbb4', false);
  G.canaryCrown = variants(canaryCrown, 2);
  G.kingTrunk = palmTrunk(0.15, 0.17, 0.22, CELL.palmBark, '#bdb7ad', false);
  G.kingCrown = variants(kingCrown, 1);
  const tree = (o) => variants((r) => broadleaf(r, { ...o, clusters: Math.round(o.clusters * detail) }), 2);
  G.ficus = tree({ h: 10, crownY: 6.4, crownR: 5.2, crownH: 4.4, trunkR: 0.4, branches: 5, clusters: 34, card: 2.6, leaf: '#3c6a30', cell: CELL.leaf, bark: '#9a968c' });
  G.sycamore = tree({ h: 15, crownY: 10, crownR: 6.2, crownH: 5.8, trunkR: 0.38, branches: 6, clusters: 26, card: 3, leaf: '#7da24f', cell: CELL.leaf, bark: '#ffffff', barkCell: CELL.sycBark, sparse: true, lean: 0.12 });
  G.coral = tree({ h: 10, crownY: 7, crownR: 6.2, crownH: 3.4, trunkR: 0.42, branches: 7, clusters: 26, card: 2.6, leaf: '#5d8c3c', cell: CELL.leaf, bark: '#a8a49a', flowers: CELL.coral, flowerShare: 0.4 });
  G.magnolia = tree({ h: 12, crownY: 7, crownR: 4.2, crownH: 7.4, trunkR: 0.34, branches: 4, clusters: 30, card: 2.4, leaf: '#ffffff', cell: CELL.magnolia, bark: '#8f8b84', cone: true });
  G.carrotwood = tree({ h: 8, crownY: 5.4, crownR: 3.6, crownH: 3.6, trunkR: 0.26, branches: 4, clusters: 22, card: 2.2, leaf: '#4f7d35', cell: CELL.leaf, bark: '#7f776c' });
  G.jacaranda = tree({ h: 10, crownY: 7.4, crownR: 5.6, crownH: 3.4, trunkR: 0.3, branches: 6, clusters: 30, card: 2.6, leaf: '#ffffff', cell: CELL.jacaranda, bark: '#8a8178', flowers: CELL.leaf, flowerShare: 0.15, flowerTint: '#5f8a3e', vase: true });
  G.oak = tree({ h: 9, crownY: 5.6, crownR: 8, crownH: 4.6, trunkR: 0.55, branches: 7, clusters: 38, card: 3, leaf: '#4b5c2e', cell: CELL.leaf, bark: '#6d6458', gnarled: true });
  G.cypress = variants((r) => cypress(r, detail), 2);
  G.shrub = variants((r) => shrub(r, 1.7, '#7a8462', CELL.shrub, 6), 2).concat(variants((r) => shrub(r, 1.6, '#4f6b38', CELL.leaf, 6), 1));
  G.grass = [grassTuft()];
  G.paradise = [birdOfParadise(mulberry(5))];
  G.agave = [agave()];
  G.bougain = variants((r) => shrub(r, 2.4, '#ffffff', CELL.bougain, 9, 1.5), 1);
  LIB = { key, mat, solid, far, detail, hash, G };
  return LIB;
}

export function buildFlora(W, scene, ctx) {
  const { batch, lo, hi, fadeNearCamera, mulberry } = ctx;
  const { mat, solid, far, detail, hash, G } = library(ctx);

  /* --------------------------------------------------- placing them */
  const buckets = {};
  const put = (key, variant, it) => { const k = `${key}|${variant}`; (buckets[k] ||= []).push(it); };
  const greenJitter = (x, z) => { const v = 0.85 + hash(z, x) * 0.3; return `#${new THREE.Color(v, v, v).getHexString()}`; };
  for (const t of W.trees) {
    const hsh = hash(t.x, t.z), s = t.s, ry = t.r;
    const tint = greenJitter(t.x, t.z);
    if (t.t === 0) {
      // which palm: mostly Mexican fan palms, a quarter Canary date palms, some king palms
      const lean = (hash(t.z, t.x) - 0.5) * 0.12;
      let kind = 'fan', trunkH = t.h;
      if (hsh < 0.22) { kind = 'canary'; trunkH = Math.max(9, t.h * 0.6); } else if (hsh < 0.3) { kind = 'king'; trunkH = Math.min(11, t.h * 0.45); }
      const topX = t.x + Math.sin(ry) * Math.sin(lean) * trunkH, topZ = t.z + Math.cos(ry) * Math.sin(lean) * trunkH;
      const k = kind === 'fan' ? 'fanTrunk' : kind === 'canary' ? 'canaryTrunk' : 'kingTrunk';
      put(k, 0, { x: t.x, z: t.z, py: t.y - 0.2, sx: s, sy: trunkH, sz: s, rx: lean, ry, c: tint });
      const crown = kind === 'fan' ? 'fanCrown' : kind === 'canary' ? 'canaryCrown' : 'kingCrown';
      put(crown, Math.floor(hash(t.x * 3, t.z) * G[crown].length), { x: t.x, z: t.z, px: topX, pz: topZ, py: t.y + trunkH - 0.25, sx: s, sy: s, sz: s, ry, c: tint });
    } else if (t.t === 1 || t.t === 2 || t.t === 5) {
      let sp = t.t === 2 ? 'jacaranda' : t.t === 5 ? 'oak' : ['ficus', 'ficus', 'sycamore', 'coral', 'magnolia', 'carrotwood'][Math.floor(hsh * 6)];
      if (t.t === 1 && t.h > 12 && sp === 'carrotwood') sp = 'sycamore';
      const ref = { ficus: 10, sycamore: 15, coral: 10, magnolia: 12, carrotwood: 8, jacaranda: 10, oak: 9 }[sp];
      const k = (t.h / ref) * s;
      put(sp, Math.floor(hash(t.x, t.z * 7) * 2), { x: t.x, z: t.z, py: t.y - 0.15, sx: k, sy: k, sz: k, ry, c: tint });
    } else if (t.t === 3) {
      const k = t.h / 1.7;
      put('shrub', Math.floor(hsh * 3), { x: t.x, z: t.z, py: t.y - 0.1, sx: k * s, sy: k, sz: k * s, ry, c: tint });
      // dry golden grass round the chaparral
      for (let g = 0; g < (lo ? 1 : 3); g++) {
        const a = hash(t.x + g, t.z) * 6.28, d = 1.5 + hash(t.z, t.x + g) * 3.5;
        const gx = t.x + Math.cos(a) * d, gz = t.z + Math.sin(a) * d;
        put('grass', 0, { x: gx, z: gz, py: W.groundHeight(gx, gz) - 0.05, sx: 1, sy: 0.7 + hash(gx, gz) * 0.6, sz: 1, ry: a, c: tint });
      }
    } else if (t.t === 4) {
      const k = t.h / 13;
      put('cypress', Math.floor(hsh * 2), { x: t.x, z: t.z, py: t.y - 0.1, sx: s, sy: k, sz: s, ry, c: tint });
    }
  }

  /* front-yard planting round the houses: bird-of-paradise and agave by the
   * walls, bougainvillea climbing a corner */
  if (!lo) {
    const clear = (x, z) => W.maskAt(x, z) === 0 && !W.isSea(x, z) && !W.queryPoint(x, W.groundHeight(x, z) + 0.6, z, 0.6);
    for (const b of W.buildings) {
      if (b.win !== 1 || b.h > 14) continue;
      const hb = hash(b.x, b.z);
      if (hb < 0.25) continue;
      const c = Math.cos(b.rot), sn = Math.sin(b.rot);
      const at = (lx, lz) => [b.x + lx * c + lz * sn, b.z - lx * sn + lz * c];
      const n = 1 + Math.floor(hb * 3);
      for (let i = 0; i < n; i++) {
        const side = hash(b.x + i, b.z - i) < 0.5 ? 1 : -1;
        const along = (hash(b.z + i * 3, b.x) - 0.5) * b.w * 0.8;
        const [x, z] = at(along, side * (b.d / 2 + 1.1));
        if (!clear(x, z)) continue;
        const y = W.groundHeight(x, z);
        const pick = hash(x, z);
        if (pick < 0.45) put('paradise', 0, { x, z, py: y, sx: 1, sy: 0.85 + pick * 0.5, sz: 1, ry: pick * 20, c: '#ffffff' });
        else if (pick < 0.75) put('agave', 0, { x, z, py: y, sx: 0.8 + pick * 0.6, sy: 0.8 + pick * 0.5, sz: 0.8 + pick * 0.6, ry: pick * 30, c: '#ffffff' });
        else put('bougain', 0, { x, z, py: y, sx: 1, sy: 1, sz: 1, ry: b.rot, c: '#ffffff' });
      }
    }
  }

  const farOf = { grass: far * 0.35, paradise: far * 0.3, agave: far * 0.3, bougain: far * 0.4, shrub: far * 0.7 };
  const castOf = { grass: false, paradise: false, agave: false, shrub: false };
  let instances = 0;
  for (const [key, items] of Object.entries(buckets)) {
    const [name, v] = key.split('|');
    const geom = Array.isArray(G[name]) ? G[name][Number(v)] : G[name];
    const m = name.endsWith('Trunk') || name === 'agave' ? solid : mat;
    batch(items, geom, m, { far: farOf[name] || far, receive: false, cast: castOf[name] !== false && !lo });
    instances += items.length;
  }
  return { instances };
}

/* ================================================== geometry builders */

/* Collects quads and tubes with UVs into one atlas cell each, vertex
 * colours and hand-set normals (foliage normals point out of the crown,
 * which lights a card canopy like a solid one). */
class Builder {
  constructor() { this.p = []; this.n = []; this.uv = []; this.c = []; this.i = []; }
  vert(x, y, z, nx, ny, nz, u, v, col) {
    this.p.push(x, y, z); this.n.push(nx, ny, nz); this.uv.push(u, v); this.c.push(col.r, col.g, col.b);
    return this.p.length / 3 - 1;
  }
  /* a quad from corner a along edge vectors e1 (u) and e2 (v) */
  quad(a, e1, e2, cell, col, normal) {
    const [u0, v0, u1, v1] = cellUV(cell);
    const nrm = normal || norm(cross(e1, e2));
    const k = [
      this.vert(a[0], a[1], a[2], ...nrm, u0, v0, col),
      this.vert(a[0] + e1[0], a[1] + e1[1], a[2] + e1[2], ...nrm, u1, v0, col),
      this.vert(a[0] + e1[0] + e2[0], a[1] + e1[1] + e2[1], a[2] + e1[2] + e2[2], ...nrm, u1, v1, col),
      this.vert(a[0] + e2[0], a[1] + e2[1], a[2] + e2[2], ...nrm, u0, v1, col)
    ];
    this.i.push(k[0], k[1], k[2], k[0], k[2], k[3]);
  }
  /* three crossed cards centred on c: a leafy clump that reads from any side */
  clump(c, size, cell, col, outward) {
    for (let a = 0; a < 3; a++) {
      const ang = (a / 3) * Math.PI + (outward ? Math.atan2(outward[2], outward[0]) : 0);
      const e1 = [Math.cos(ang) * size, 0, Math.sin(ang) * size];
      const tilt = a === 2 ? 0.9 : 0.25;
      const e2 = [-Math.sin(ang) * size * tilt * 0.3, size * (a === 2 ? 0.35 : 1), Math.cos(ang) * size * tilt * (a === 2 ? 1 : 0.3)];
      const corner = [c[0] - e1[0] / 2 - e2[0] / 2, c[1] - e1[1] / 2 - e2[1] / 2, c[2] - e1[2] / 2 - e2[2] / 2];
      this.quad(corner, e1, e2, cell, col, outward || [0, 1, 0]);
    }
  }
  /* a tapered tube from p0 to p1 */
  tube(p0, p1, r0, r1, seg, cell, col, vRepeat = 1) {
    const [u0, v0, u1, v1] = cellUV(cell);
    const d = norm([p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]]);
    const side = norm(Math.abs(d[1]) < 0.9 ? cross(d, [0, 1, 0]) : cross(d, [1, 0, 0]));
    const up = cross(side, d);
    const base = this.p.length / 3;
    for (let r = 0; r <= 1; r++) {
      const p = r ? p1 : p0, rad = r ? r1 : r0;
      for (let s = 0; s <= seg; s++) {
        const a = (s / seg) * Math.PI * 2, ca = Math.cos(a), sa = Math.sin(a);
        const nx = side[0] * ca + up[0] * sa, ny = side[1] * ca + up[1] * sa, nz = side[2] * ca + up[2] * sa;
        this.vert(p[0] + nx * rad, p[1] + ny * rad, p[2] + nz * rad, nx, ny, nz, u0 + (u1 - u0) * (s / seg), v0 + (v1 - v0) * r * vRepeat, col);
      }
    }
    for (let s = 0; s < seg; s++) {
      const a = base + s, b = base + s + 1, c2 = base + seg + 1 + s, d2 = base + seg + 2 + s;
      this.i.push(a, c2, b, b, c2, d2);
    }
  }
  /* a curved strip (a palm frond) along points pts, width w at each point */
  strip(pts, widths, up, cell, col) {
    const [u0, v0, u1, v1] = cellUV(cell);
    const base = this.p.length / 3;
    for (let k = 0; k < pts.length; k++) {
      const a = pts[Math.max(0, k - 1)], b = pts[Math.min(pts.length - 1, k + 1)];
      const d = norm([b[0] - a[0], b[1] - a[1], b[2] - a[2]]);
      const side = norm(cross(d, up));
      const nrm = norm(cross(side, d));
      const w = widths[k] / 2, t = k / (pts.length - 1);
      const p = pts[k];
      this.vert(p[0] - side[0] * w, p[1] - side[1] * w, p[2] - side[2] * w, ...nrm, u0, v0 + (v1 - v0) * t, col);
      this.vert(p[0] + side[0] * w, p[1] + side[1] * w, p[2] + side[2] * w, ...nrm, u1, v0 + (v1 - v0) * t, col);
    }
    for (let k = 0; k < pts.length - 1; k++) {
      const a = base + k * 2;
      this.i.push(a, a + 2, a + 1, a + 1, a + 2, a + 3);
    }
  }
  build() {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(this.p, 3));
    g.setAttribute('normal', new THREE.Float32BufferAttribute(this.n, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(this.uv, 2));
    g.setAttribute('color', new THREE.Float32BufferAttribute(this.c, 3));
    g.setIndex(this.i);
    return g;
  }
}

const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
const C = (h) => new THREE.Color(h);
function cellUV(cell) {
  const col = cell % 4, row = Math.floor(cell / 4), e = 0.004;
  // canvas row 0 is the top of the texture (v = 1)
  return [col / 4 + e, 1 - (row + 1) / 4 + e, (col + 1) / 4 - e, 1 - row / 4 - e];
}

/* A palm trunk of unit height (the instance scales it to the tree's
 * height), with a flared foot. */
function palmTrunk(rTop, rMid, rFoot, cell, hex, curve) {
  const b = new Builder(), col = C(hex);
  const n = 8;
  let prev = [0, 0, 0];
  for (let k = 1; k <= n; k++) {
    const t = k / n;
    const p = [curve ? Math.sin(t * 2.2) * 0.012 : 0, t, 0];
    const r0 = k === 1 ? rFoot : rMid + (rTop - rMid) * ((k - 1) / n);
    const r1 = rMid + (rTop - rMid) * t;
    b.tube(prev, p, r0, r1, 9, cell, col, 1);
    prev = p;
  }
  return b.build();
}

/* Mexican fan palm crown: two dozen palmate fans on petioles, radiating up,
 * out and down; optionally the shaggy brown skirt of dead fronds below. */
function fanCrown(rnd, skirt) {
  const b = new Builder(), green = C('#6d8f45'), stalk = C('#8a7d5c');
  const fans = 34;
  for (let k = 0; k < fans; k++) {
    const yaw = (k / fans) * Math.PI * 2 + rnd() * 0.3;
    const pitch = -0.55 + (k % 5) * 0.38 + rnd() * 0.2;      // some up, some out, some drooping
    const dir = [Math.cos(yaw) * Math.cos(pitch), Math.sin(pitch), Math.sin(yaw) * Math.cos(pitch)];
    const len = 1.3 + rnd() * 0.5;
    const tip = [dir[0] * len, 0.3 + dir[1] * len, dir[2] * len];
    b.tube([0, 0.2, 0], tip, 0.04, 0.03, 3, CELL.bark, stalk);
    // the fan itself: a roughly round card facing outward, 1.6-1.9 m across
    const size = 2.0 + rnd() * 0.4;
    const side = norm(cross(dir, [0, 1, 0]));
    const up = norm(cross(side, dir));
    const e1 = side.map((v) => v * size), e2 = [dir[0] * size * 0.7 + up[0] * size * 0.7, dir[1] * size * 0.7 + up[1] * size * 0.7, dir[2] * size * 0.7 + up[2] * size * 0.7];
    const corner = [tip[0] - e1[0] / 2, tip[1] - e1[1] / 2 - size * 0.15, tip[2] - e1[2] / 2];
    b.quad(corner, e1, e2, CELL.fan, green, norm([dir[0], 0.5, dir[2]]));
  }
  b.tube([0, -0.6, 0], [0, 0.45, 0], 0.32, 0.26, 8, CELL.skirt, C('#ffffff'));
  if (skirt) {
    // the skirt: dead fronds hanging against the trunk, 2-4 m long, flaring a little
    const len = 2 + rnd() * 2;
    for (let k = 0; k < 18; k++) {
      const yaw = (k / 18) * Math.PI * 2 + rnd() * 0.2, l = len * (0.8 + rnd() * 0.35);
      const top = [Math.cos(yaw) * 0.45, 0.1, Math.sin(yaw) * 0.45];
      const e1 = [-Math.sin(yaw) * 1.0, 0, Math.cos(yaw) * 1.0];
      const e2 = [-Math.cos(yaw) * 0.12, -l, -Math.sin(yaw) * 0.12];
      b.quad([top[0] - e1[0] / 2 + e2[0], top[1] + e2[1], top[2] - e1[2] / 2 + e2[2]], e1, e2.map((v) => -v), CELL.skirt, C('#ffffff'), [Math.cos(yaw), 0.15, Math.sin(yaw)]);
    }
  }
  return b.build();
}

/* A feather frond as an arching strip: base at origin, out along yaw. */
function frond(b, yaw, rise, len, width, droop, col) {
  const pts = [], widths = [];
  for (let k = 0; k <= 5; k++) {
    const t = k / 5;
    const out = t * len;
    pts.push([Math.cos(yaw) * out, rise * t - droop * t * t, Math.sin(yaw) * out]);
    widths.push(width * Math.sin(Math.PI * Math.min(1, t * 1.1 + 0.08)));
  }
  b.strip(pts, widths, [0, 1, 0], CELL.feather, col);
}

/* Canary Island date palm: forty-odd long feathery fronds and the
 * pineapple-shaped knob of old leaf bases at the top of the trunk. */
function canaryCrown(rnd) {
  const b = new Builder(), green = C('#5e8236');
  for (let k = 0; k < 44; k++) {
    const yaw = k * 2.39996 + rnd() * 0.2;                     // golden-angle spiral
    const tier = k / 44;
    frond(b, yaw, 2.6 - tier * 2.4, 4.6 + rnd() * 1.2, 1.4, 1.6 + tier * 2.2, green);
  }
  b.tube([0, -1.2, 0], [0, 0.3, 0], 0.55, 0.62, 10, CELL.canaryBark, C('#cdbd9c'));
  b.tube([0, 0.3, 0], [0, 0.9, 0], 0.62, 0.2, 10, CELL.canaryBark, C('#a99a78'));
  return b.build();
}

/* King palm: a smooth green crownshaft and a dozen arching fronds. */
function kingCrown(rnd) {
  const b = new Builder(), green = C('#6c9a45');
  b.tube([0, 0, 0], [0, 1.7, 0], 0.2, 0.17, 9, CELL.leaf, C('#5f8a3f'));
  for (let k = 0; k < 11; k++) {
    const yaw = (k / 11) * Math.PI * 2 + rnd() * 0.3;
    const pts = [], widths = [];
    for (let s = 0; s <= 5; s++) {
      const t = s / 5, out = t * (3 + rnd() * 0.3);
      pts.push([Math.cos(yaw) * out, 1.7 + 1.3 * t - 2 * t * t, Math.sin(yaw) * out]);
      widths.push(1.1 * Math.sin(Math.PI * Math.min(1, t * 1.1 + 0.08)));
    }
    b.strip(pts, widths, [0, 1, 0], CELL.feather, green);
  }
  return b.build();
}

/* A broadleaf tree: trunk, branches out to the crown, and leaf clumps
 * scattered over an ellipsoid (or vase, or cone) crown. */
function broadleaf(rnd, o) {
  const b = new Builder();
  const barkCol = C(o.bark), leafCol = C(o.leaf);
  const fork = o.crownY - o.crownH * 0.45;
  const lean = o.lean || 0;
  const top = [lean * fork, fork, 0];
  b.tube([0, 0, 0], top, o.trunkR, o.trunkR * 0.75, 9, o.barkCell ?? CELL.bark, barkCol, 2);
  const ends = [];
  for (let k = 0; k < o.branches; k++) {
    const yaw = (k / o.branches) * Math.PI * 2 + rnd() * 0.5;
    const out = o.crownR * (o.vase ? 0.75 : 0.55) * (0.7 + rnd() * 0.3);
    const up = o.crownH * (o.gnarled ? 0.15 : o.vase ? 0.55 : 0.35) * (0.6 + rnd() * 0.5);
    let mid = top;
    if (o.gnarled) {
      // live oaks twist: a kinked branch, low and wide
      mid = [top[0] + Math.cos(yaw) * out * 0.5, fork + up * 0.3 + rnd() * 0.8, Math.sin(yaw) * out * 0.5];
      b.tube(top, mid, o.trunkR * 0.55, o.trunkR * 0.4, 6, o.barkCell ?? CELL.bark, barkCol);
    }
    const end = [top[0] + Math.cos(yaw) * out, fork + up, Math.sin(yaw) * out];
    b.tube(mid, end, o.trunkR * (o.gnarled ? 0.36 : 0.32), o.trunkR * 0.1, 5, o.barkCell ?? CELL.bark, barkCol);
    ends.push(end);
  }
  const centre = [lean * fork, o.crownY, 0];
  // a dense, darker core so you never see daylight (or branch spokes) through the middle
  for (let k = 0; k < Math.round(o.clusters * 0.45); k++) {
    const a = rnd() * Math.PI * 2, u = rnd() * 2 - 1, rr = Math.sqrt(1 - u * u), d = 0.3 + rnd() * 0.35;
    const p = [centre[0] + rr * Math.cos(a) * o.crownR * d, centre[1] + u * o.crownH * 0.4 * d, centre[2] + rr * Math.sin(a) * o.crownR * d];
    b.clump(p, o.card * 1.1, o.cell, leafCol.clone().multiplyScalar(0.62), norm([rr * Math.cos(a), 0.5, rr * Math.sin(a)]));
  }
  for (let k = 0; k < Math.round(o.clusters * 1.5); k++) {
    // a point on (or just inside) the crown's surface
    const u = rnd() * 2 - 1, a = rnd() * Math.PI * 2, rr = Math.sqrt(1 - u * u);
    let dx = rr * Math.cos(a), dy = u, dz = rr * Math.sin(a);
    if (o.vase || o.gnarled) dy = Math.abs(dy) * 0.9 - 0.1;          // flat-bottomed, spreading
    let rx = o.crownR, rz = o.crownR;
    if (o.cone) { const w = 1 - (dy + 1) / 2 * 0.75; rx *= w; rz *= w; }
    const depth = o.sparse ? 0.8 + rnd() * 0.25 : 0.72 + rnd() * 0.28;
    const p = [centre[0] + dx * rx * depth, centre[1] + dy * o.crownH * 0.5 * depth, centre[2] + dz * rz * depth];
    if (o.sparse && rnd() < 0.3) { const e = ends[k % ends.length]; p[0] = (p[0] + e[0]) / 2; p[1] = (p[1] + e[1]) / 2; p[2] = (p[2] + e[2]) / 2; }
    const flower = o.flowers !== undefined && rnd() < o.flowerShare;
    const cell = flower ? o.flowers : o.cell;
    const col = flower ? (o.flowerTint ? C(o.flowerTint) : C('#ffffff')) : leafCol.clone().multiplyScalar(0.85 + rnd() * 0.3);
    b.clump(p, o.card * (0.62 + rnd() * 0.35), cell, col, norm([dx, dy * 0.6 + 0.4, dz]));
  }
  return b.build();
}

/* Italian cypress: a tall, very narrow column of dark green. */
function cypress(rnd, detail) {
  const b = new Builder(), col = C('#2f4a2a');
  b.tube([0, 0, 0], [0, 2, 0], 0.18, 0.14, 6, CELL.bark, C('#6b5d4f'));
  const n = Math.round(26 * detail);
  for (let k = 0; k < n; k++) {
    const t = k / n, y = 0.8 + t * 12;
    const w = 0.55 + 0.7 * Math.sin(Math.PI * Math.min(1, t * 1.15 + 0.05));
    const a = rnd() * 6.28;
    b.clump([Math.cos(a) * w * 0.3, y, Math.sin(a) * w * 0.3], w * 2.1, CELL.needles, col.clone().multiplyScalar(0.85 + rnd() * 0.3), norm([Math.cos(a), 0.3, Math.sin(a)]));
  }
  return b.build();
}

/* A low clump of shrub (chaparral, or bougainvillea), height h. */
function shrub(rnd, h, hex, cell, n, spread = 1) {
  const b = new Builder(), col = C(hex);
  for (let k = 0; k < n; k++) {
    const a = rnd() * 6.28, d = rnd() * 0.9 * spread;
    const y = h * (0.35 + rnd() * 0.45);
    b.clump([Math.cos(a) * d, y, Math.sin(a) * d], h * (0.8 + rnd() * 0.5), cell, col.clone().multiplyScalar(0.85 + rnd() * 0.3), norm([Math.cos(a), 0.6, Math.sin(a)]));
  }
  return b.build();
}

function grassTuft() {
  const b = new Builder();
  b.clump([0, 0.42, 0], 0.9, CELL.grass, C('#ffffff'), [0, 1, 0]);
  return b.build();
}

/* Bird-of-paradise: a fan of upright paddle leaves, orange-and-blue flowers. */
function birdOfParadise(rnd) {
  const b = new Builder();
  for (let k = 0; k < 7; k++) {
    const a = (k / 7) * Math.PI * 2 + rnd() * 0.4, lean = 0.25 + rnd() * 0.25;
    const e1 = [-Math.sin(a) * 0.55, 0, Math.cos(a) * 0.55];
    const e2 = [Math.cos(a) * lean * 1.3, 1.3, Math.sin(a) * lean * 1.3];
    b.quad([-e1[0] / 2, 0, -e1[2] / 2], e1, e2, CELL.paradise, C('#ffffff'), norm([Math.cos(a), 0.5, Math.sin(a)]));
  }
  return b.build();
}

/* Agave: a rosette of thick blue-grey spikes. */
function agave() {
  const b = new Builder(), col = C('#8fa9a8');
  for (let k = 0; k < 16; k++) {
    const a = (k / 16) * Math.PI * 2 + (k % 2) * 0.2;
    const pitch = 0.35 + (k % 3) * 0.3;
    const tip = [Math.cos(a) * Math.cos(pitch) * 0.9, 0.15 + Math.sin(pitch) * 0.9, Math.sin(a) * Math.cos(pitch) * 0.9];
    b.tube([0, 0.12, 0], tip, 0.09, 0.005, 4, CELL.bark, col);
  }
  return b.build();
}

/* ======================================================= the texture */

function makeAtlas(mulberry) {
  const S = 512, c = document.createElement('canvas');
  c.width = c.height = S * 4;
  const ctx = c.getContext('2d');
  const at = (cell) => [(cell % 4) * S, Math.floor(cell / 4) * S];
  const rnd = mulberry(321);
  const grey = (l, a = 1) => `rgba(${(l * 255) | 0},${(l * 255) | 0},${(l * 255) | 0},${a})`;

  /* a cluster of leaves inside a soft blob */
  /* leaves bunched into several overlapping lobes, thinning out towards
   * the edge, so a card reads as a ragged sprig and never as a disc */
  const leaves = (cell, n, size, colour, inner = 0.42) => {
    const [ox, oy] = at(cell);
    const lobes = Array.from({ length: 6 }, (_, i) => {
      const a = (i / 6) * 6.28 + rnd() * 0.8, d = i === 0 ? 0 : S * inner * (0.35 + rnd() * 0.35);
      return [S / 2 + Math.cos(a) * d, S / 2 + Math.sin(a) * d, S * inner * (0.32 + rnd() * 0.2)];
    });
    for (let k = 0; k < n; k++) {
      const [lx, ly, lr] = lobes[k % lobes.length];
      const a = rnd() * 6.28, f = Math.pow(rnd(), 0.75), r = f * lr;
      if (f > 0.7 && rnd() < (f - 0.7) * 2.2) continue;            // sparse, broken edges
      const x = ox + lx + Math.cos(a) * r, y = oy + ly + Math.sin(a) * r * 0.9;
      const l = size * (0.6 + rnd() * 0.6);
      ctx.fillStyle = colour(rnd(), r / (S * inner));
      ctx.beginPath(); ctx.ellipse(x, y, l, l * 0.42, rnd() * Math.PI, 0, Math.PI * 2); ctx.fill();
    }
  };
  leaves(CELL.leaf, 2600, 15, (q, d) => grey(0.45 + q * 0.45 + (1 - d) * 0.05));
  leaves(CELL.shrub, 3200, 9, (q) => grey(0.5 + q * 0.4));
  leaves(CELL.magnolia, 1600, 24, (q) => `rgb(${40 + q * 40 | 0},${70 + q * 50 | 0},${30 + q * 25 | 0})`);
  { const [ox, oy] = at(CELL.magnolia); for (let k = 0; k < 14; k++) { ctx.fillStyle = '#f4f1e6'; ctx.beginPath(); ctx.arc(ox + S / 2 + (rnd() - 0.5) * S * 0.7, oy + S / 2 + (rnd() - 0.5) * S * 0.7, 10 + rnd() * 8, 0, 6.28); ctx.fill(); } }
  // jacaranda: lavender blossom with a little green showing through
  leaves(CELL.jacaranda, 600, 13, (q) => `rgb(${70 + q * 30 | 0},${100 + q * 30 | 0},${55 + q * 20 | 0})`);
  leaves(CELL.jacaranda, 2800, 9, (q) => `rgb(${150 + q * 50 | 0},${120 + q * 45 | 0},${205 + q * 40 | 0})`);
  // coral tree: green leaves, spikes of red-orange flowers
  leaves(CELL.coral, 1400, 14, (q) => `rgb(${70 + q * 30 | 0},${110 + q * 40 | 0},${50 + q * 20 | 0})`);
  leaves(CELL.coral, 700, 8, (q) => `rgb(${220 + q * 30 | 0},${60 + q * 50 | 0},${30 + q * 20 | 0})`, 0.38);
  // bougainvillea: magenta bracts over green
  leaves(CELL.bougain, 900, 12, (q) => `rgb(${60 + q * 30 | 0},${100 + q * 40 | 0},${45 | 0})`);
  leaves(CELL.bougain, 2600, 9, (q) => `rgb(${200 + q * 50 | 0},${30 + q * 40 | 0},${120 + q * 50 | 0})`);
  // needles / scale foliage
  { const [ox, oy] = at(CELL.needles); for (let k = 0; k < 5000; k++) { const a = rnd() * 6.28, r = Math.sqrt(rnd()) * S * 0.44; ctx.strokeStyle = grey(0.45 + rnd() * 0.5); ctx.lineWidth = 2; const x = ox + S / 2 + Math.cos(a) * r, y = oy + S / 2 + Math.sin(a) * r; ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + (rnd() - 0.5) * 10, y + rnd() * 12); ctx.stroke(); } }

  // a palmate fan: segments radiating from the petiole, slightly torn tips
  {
    const [ox, oy] = at(CELL.fan);
    const cx = ox + S / 2, cy = oy + S * 0.92;
    for (let k = 0; k < 46; k++) {
      const a = Math.PI * 1.06 + (k / 45) * Math.PI * 0.88;
      const len = S * (0.62 + rnd() * 0.22);
      const l = 0.55 + rnd() * 0.4;
      ctx.strokeStyle = grey(l); ctx.lineWidth = 15; ctx.lineCap = 'round';
      ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(cx + Math.cos(a) * len, cy + Math.sin(a) * len); ctx.stroke();
      ctx.strokeStyle = grey(l * 0.85); ctx.lineWidth = 3;
      ctx.beginPath(); ctx.moveTo(cx + Math.cos(a) * len, cy + Math.sin(a) * len); ctx.lineTo(cx + Math.cos(a + 0.03) * (len + 30), cy + Math.sin(a + 0.03) * (len + 30)); ctx.stroke();
    }
    ctx.fillStyle = grey(0.5); ctx.beginPath(); ctx.arc(cx, cy, 16, 0, 6.28); ctx.fill();
  }
  // a feather frond: rib with leaflets, drawn along the cell's height
  {
    const [ox, oy] = at(CELL.feather);
    const cx = ox + S / 2;
    ctx.lineCap = 'round';
    for (let y = 6; y < S - 6; y += 5) {
      const t = y / S, len = S * 0.46 * Math.sin(Math.PI * Math.min(1, t * 1.05 + 0.05));
      ctx.strokeStyle = grey(0.55 + rnd() * 0.4); ctx.lineWidth = 4;
      for (const s of [-1, 1]) { ctx.beginPath(); ctx.moveTo(cx, oy + y); ctx.lineTo(cx + s * len, oy + y + 18 + len * 0.25); ctx.stroke(); }
    }
    ctx.strokeStyle = grey(0.75); ctx.lineWidth = 7; ctx.beginPath(); ctx.moveTo(cx, oy); ctx.lineTo(cx, oy + S); ctx.stroke();
  }
  // the dead-frond skirt: shaggy straw-brown
  {
    const [ox, oy] = at(CELL.skirt);
    for (let k = 0; k < 900; k++) {
      const x = ox + rnd() * S, y = oy + rnd() * S * 0.2;
      ctx.strokeStyle = `rgb(${120 + rnd() * 70 | 0},${95 + rnd() * 55 | 0},${60 + rnd() * 35 | 0})`; ctx.lineWidth = 3 + rnd() * 4;
      ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + (rnd() - 0.5) * 30, y + S * (0.6 + rnd() * 0.4)); ctx.stroke();
    }
  }
  // dry golden grass
  {
    const [ox, oy] = at(CELL.grass);
    for (let k = 0; k < 500; k++) {
      const x = ox + S * 0.15 + rnd() * S * 0.7, len = S * (0.4 + rnd() * 0.5);
      ctx.strokeStyle = `rgb(${170 + rnd() * 60 | 0},${145 + rnd() * 50 | 0},${80 + rnd() * 40 | 0})`; ctx.lineWidth = 2 + rnd() * 2;
      ctx.beginPath(); ctx.moveTo(x, oy + S); ctx.quadraticCurveTo(x + (rnd() - 0.5) * 60, oy + S - len * 0.6, x + (rnd() - 0.5) * 120, oy + S - len); ctx.stroke();
    }
  }
  // bird-of-paradise paddle leaf with an orange-and-blue flower
  {
    const [ox, oy] = at(CELL.paradise);
    ctx.fillStyle = '#3f6e3a';
    ctx.beginPath(); ctx.ellipse(ox + S / 2, oy + S * 0.4, S * 0.2, S * 0.36, 0, 0, 6.28); ctx.fill();
    ctx.strokeStyle = '#5c8a4c'; ctx.lineWidth = 6; ctx.beginPath(); ctx.moveTo(ox + S / 2, oy + S * 0.05); ctx.lineTo(ox + S / 2, oy + S); ctx.stroke();
    ctx.fillStyle = '#ff7a1a'; ctx.beginPath(); ctx.moveTo(ox + S * 0.62, oy + S * 0.12); ctx.lineTo(ox + S * 0.9, oy + S * 0.02); ctx.lineTo(ox + S * 0.7, oy + S * 0.2); ctx.fill();
    ctx.fillStyle = '#2b4cc2'; ctx.beginPath(); ctx.moveTo(ox + S * 0.66, oy + S * 0.16); ctx.lineTo(ox + S * 0.86, oy + S * 0.1); ctx.lineTo(ox + S * 0.7, oy + S * 0.21); ctx.fill();
  }
  // barks (opaque): generic grey-brown, mottled sycamore, ringed palm, diamond-patterned Canary palm
  const bark = (cell, base, fn) => {
    const [ox, oy] = at(cell);
    ctx.fillStyle = base; ctx.fillRect(ox, oy, S, S);
    fn(ox, oy);
  };
  bark(CELL.bark, '#8c8378', (ox, oy) => { for (let k = 0; k < 700; k++) { ctx.fillStyle = grey(0.35 + rnd() * 0.4, 0.5); ctx.fillRect(ox + rnd() * S, oy + rnd() * S, 3 + rnd() * 6, 20 + rnd() * 50); } });
  bark(CELL.sycBark, '#e6e1d4', (ox, oy) => { for (let k = 0; k < 160; k++) { ctx.fillStyle = ['#b8b09a', '#9d9a86', '#d4cfbf', '#c2b48e'][k % 4]; ctx.beginPath(); ctx.ellipse(ox + rnd() * S, oy + rnd() * S, 15 + rnd() * 40, 10 + rnd() * 30, rnd() * 3, 0, 6.28); ctx.fill(); } });
  bark(CELL.palmBark, '#a99c86', (ox, oy) => { for (let y = 0; y < S; y += 9) { ctx.fillStyle = grey(0.35 + rnd() * 0.2, 0.55); ctx.fillRect(ox, oy + y, S, 3); } for (let k = 0; k < 300; k++) { ctx.fillStyle = grey(0.5 + rnd() * 0.3, 0.3); ctx.fillRect(ox + rnd() * S, oy + rnd() * S, 2, 12); } });
  bark(CELL.canaryBark, '#9c8a68', (ox, oy) => { ctx.strokeStyle = 'rgba(60,45,30,0.6)'; ctx.lineWidth = 6; for (let k = -S; k < S * 2; k += 46) { ctx.beginPath(); ctx.moveTo(ox + k, oy); ctx.lineTo(ox + k + S, oy + S); ctx.stroke(); ctx.beginPath(); ctx.moveTo(ox + k + S, oy); ctx.lineTo(ox + k, oy + S); ctx.stroke(); } });
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 4;
  return t;
}
