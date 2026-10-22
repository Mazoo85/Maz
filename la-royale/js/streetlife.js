/*
 * Street life: the parked cars along the kerbs (and the abandoned ones on
 * the freeways) from W.parked, and the people on the sidewalks.
 *
 * Cars are built from real side profiles, extruded and bevelled, at real
 * sizes: a midsize sedan is 4.85 × 1.85 × 1.45 m, a compact hatch 4.35 m, an
 * SUV 4.95 × 1.95 × 1.8 m, a full-size pickup 5.6 m, a minivan 5.3 m. Each kind
 * is three instanced meshes (paint, trim, glass); the instance colour is
 * the paint. ambient.js uses the same cars for the traffic.
 *
 * Pedestrians walk the boulevard sidewalks and the Venice boardwalk, most
 * thickly where LA actually walks: Hollywood Blvd, Downtown, Santa Monica,
 * Venice, Koreatown, the Strip. Their walk cycle runs in the vertex shader.
 */

import * as THREE from '../vendor/three.module.min.js';
import { mergeGeometries } from '../vendor/jsm/utils/BufferGeometryUtils.js';
import { ROAD_WIDTH } from './geo.js';

/* streetlife registers itself here so ambient.js can drive it each frame */
export const live = { streetLife: null };

/* ================================================================ cars */

const DIMS = {
  sedan: { L: 4.85, W: 1.85, H: 1.45, belt: 0.95, cab: [-1.35, 0.8], wheel: 0.34, nose: 0.82, tail: 0.92 },
  compact: { L: 4.35, W: 1.78, H: 1.45, belt: 0.95, cab: [-1.95, 0.7], wheel: 0.33, nose: 0.8, tail: 1.05, hatch: true },
  suv: { L: 4.95, W: 1.95, H: 1.8, belt: 1.08, cab: [-2.25, 0.95], wheel: 0.4, nose: 1.0, tail: 1.1, hatch: true, rails: true },
  pickup: { L: 5.6, W: 2.0, H: 1.9, belt: 1.12, cab: [-0.55, 0.95], wheel: 0.41, nose: 1.08, tail: 1.1, bed: true },
  van: { L: 5.3, W: 2.0, H: 2.1, belt: 1.05, cab: [-2.4, 1.7], wheel: 0.36, nose: 0.95, tail: 1.1, hatch: true, oneBox: true },
  bus: { L: 12.2, W: 2.6, H: 3.2, bus: true, wheel: 0.5 }
};
export const CAR_DIMS = DIMS;

let cached = null;
/* { kind: { paint, trim, glass, lights } } — all with the nose along +x and
 * the tyres on y = 0. */
export function carGeometries() {
  if (cached) return cached;
  cached = {};
  for (const kind of Object.keys(DIMS)) cached[kind] = DIMS[kind].bus ? busGeometry() : carGeometry(DIMS[kind]);
  return cached;
}

let mats = null;
export function carMaterials() {
  if (mats) return mats;
  mats = {
    paint: new THREE.MeshStandardMaterial({ roughness: 0.28, metalness: 0.45 }),
    trim: new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.55, metalness: 0.3 }),
    glass: new THREE.MeshStandardMaterial({ color: 0x1b242b, roughness: 0.05, metalness: 0.6 }),
    lights: new THREE.MeshStandardMaterial({ vertexColors: true, emissive: 0xffffff, emissiveIntensity: 0, roughness: 0.3 })
  };
  return mats;
}

const vcol = (g, hex) => {
  const c = new THREE.Color(hex), ng = g.index ? g.toNonIndexed() : g, n = ng.attributes.position.count, a = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) { a[i * 3] = c.r; a[i * 3 + 1] = c.g; a[i * 3 + 2] = c.b; }
  ng.setAttribute('color', new THREE.BufferAttribute(a, 3));
  if (!ng.attributes.uv) ng.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(n * 2), 2));
  return ng;
};
const strip = (g) => { const ng = g.index ? g.toNonIndexed() : g; for (const k of Object.keys(ng.attributes)) if (k !== 'position' && k !== 'normal') ng.deleteAttribute(k); return ng; };
const box = (w, h, d, x, y, z) => { const g = new THREE.BoxGeometry(w, h, d); g.translate(x, y, z); return g; };

/* Extrude a side profile (points in x, y) across the width, bevelled. */
function extrude(pts, width, bevel = 0.06) {
  const s = new THREE.Shape();
  s.moveTo(pts[0][0], pts[0][1]);
  for (let i = 1; i < pts.length; i++) {
    const p = pts[i];
    if (p.arch) s.absarc(p.arch[0], p.arch[1], p.arch[2], 0, Math.PI, false);
    else s.lineTo(p[0], p[1]);
  }
  const g = new THREE.ExtrudeGeometry(s, { depth: width - bevel * 2, bevelEnabled: true, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 2, curveSegments: 10 });
  g.translate(0, 0, -(width - bevel * 2) / 2);
  return g;
}

function carGeometry(d) {
  const { L, W, H, belt, wheel } = d;
  const xf = L * 0.31, xr = -L * 0.31, ar = wheel + 0.08, ay = wheel - 0.02, low = 0.3;
  const h2 = L / 2;
  // the lower body, with wheel arches cut into its sill
  const body = [
    [h2 - 0.12, low], [xf + ar, low], { arch: [xf, ay, ar] }, [xr + ar, low], { arch: [xr, ay, ar] }, [-h2 + 0.08, low],
    [-h2, 0.45], [-h2, d.hatch ? belt : 0.82], [-h2 + 0.12, d.hatch ? belt + 0.02 : d.tail],
    [d.bed ? d.cab[0] - 0.1 : Math.max(-h2 + 0.14, d.cab[0] - 0.1), d.hatch ? belt + 0.02 : d.tail], [d.cab[0], belt], [d.cab[1], belt],
    [h2 - 0.35, d.nose + 0.04], [h2 - 0.05, d.nose - 0.08], [h2, 0.62], [h2, 0.42]
  ];
  const paint = [extrude(body, W, 0.07)];
  const glass = [];
  // the greenhouse: glass all round, a roof on top, pillars at the corners
  const [c0, c1] = d.cab;
  const roofH = H - 0.05;
  const rake = d.oneBox ? 0.9 : 0.62, back = d.hatch ? 0.18 : 0.55;
  const gl = [[c0 + 0.02, belt], [c0 + back, roofH - 0.06], [c1 - rake, roofH - 0.06], [c1, belt]];
  glass.push(strip(extrude(gl, W * 0.84, 0.03)));
  paint.push(extrude([[c0 + back - 0.04, roofH - 0.1], [c1 - rake + 0.06, roofH - 0.1], [c1 - rake + 0.02, roofH], [c0 + back, roofH]], W * 0.86, 0.04));
  for (const z of [-1, 1]) {
    const zz = z * (W * 0.84) / 2;
    const pillar = (xa, xb) => {
      const len = Math.hypot(xb - xa, roofH - 0.06 - belt);
      const g = new THREE.BoxGeometry(0.1, len, 0.05);
      g.rotateZ(Math.atan2(xa - xb, roofH - 0.06 - belt));
      g.translate((xa + xb) / 2, (belt + roofH - 0.06) / 2, zz);
      paint.push(g);
    };
    pillar(c1, c1 - rake);
    pillar(c0 + 0.02, c0 + back);
    if (c1 - c0 > 1.6) paint.push(box(0.1, roofH - belt - 0.06, 0.05, (c0 + c1) / 2 - 0.1, (belt + roofH) / 2, zz));
    if (c1 - c0 > 3) paint.push(box(0.1, roofH - belt - 0.06, 0.05, c0 + (c1 - c0) * 0.3, (belt + roofH) / 2, zz));
    // mirrors
    paint.push(box(0.14, 0.12, 0.2, c1 - 0.25, belt + 0.12, z * (W / 2 + 0.08)));
  }
  if (d.bed) {
    // the pickup bed: walls and a tailgate
    for (const z of [-1, 1]) paint.push(box(c0 + h2 - 0.1, 0.42, 0.06, (-h2 + c0) / 2, belt + 0.02, z * (W / 2 - 0.06)));
    paint.push(box(0.08, 0.42, W - 0.1, -h2 + 0.08, belt + 0.02, 0));
  }
  if (d.rails) for (const z of [-1, 1]) paint.push(box(c1 - c0 - rake - back, 0.05, 0.05, (c0 + back + c1 - rake) / 2, roofH + 0.06, z * W * 0.36));

  const trim = [];
  // tyres and rims
  for (const x of [xf, xr]) {
    for (const z of [-1, 1]) {
      const t = new THREE.CylinderGeometry(wheel, wheel, 0.24, 16); t.rotateX(Math.PI / 2); t.translate(x, wheel, z * (W / 2 - 0.15));
      trim.push(vcol(t, '#151515'));
      const r = new THREE.CylinderGeometry(wheel * 0.62, wheel * 0.62, 0.25, 12); r.rotateX(Math.PI / 2); r.translate(x, wheel, z * (W / 2 - 0.145));
      trim.push(vcol(r, '#a9adb1'));
    }
  }
  // bumpers, grille, plate
  trim.push(vcol(box(0.14, 0.2, W - 0.1, h2 + 0.02, 0.42, 0), '#2a2c2e'));
  trim.push(vcol(box(0.14, 0.2, W - 0.1, -h2 - 0.02, 0.42, 0), '#2a2c2e'));
  trim.push(vcol(box(0.04, 0.22, W * 0.45, h2 + 0.005, d.nose - 0.25, 0), '#1c1d1f'));
  trim.push(vcol(box(0.03, 0.15, 0.3, -h2 - 0.09, 0.62, 0), '#f2f2ee'));
  trim.push(vcol(box(0.03, 0.15, 0.3, h2 + 0.09, 0.4, 0), '#f2f2ee'));
  // door handles and a shut line
  for (const z of [-1, 1]) trim.push(vcol(box(0.18, 0.03, 0.02, (c0 + c1) / 2, belt - 0.12, z * (W / 2 + 0.005)), '#9aa0a6'));

  const lights = [];
  for (const z of [-1, 1]) {
    lights.push(vcol(box(0.05, 0.13, 0.38, h2 - 0.02, d.nose - 0.14, z * (W / 2 - 0.26)), '#fff4d6'));
    lights.push(vcol(box(0.05, 0.16, 0.36, -h2 - 0.005, (d.hatch ? belt - 0.18 : d.tail - 0.14), z * (W / 2 - 0.24)), '#d1111a'));
  }
  return {
    paint: mergeGeometries(paint.map((g) => strip(g))),
    trim: mergeGeometries(trim),
    glass: mergeGeometries(glass),
    lights: mergeGeometries(lights)
  };
}

/* An LA Metro "Metro Local" bus: 12.2 m, orange with a silver roof band,
 * a black window band and a lit destination sign. */
function busGeometry() {
  const L = 12.2, W = 2.6, H = 3.2, wheel = 0.5;
  const prof = [[L / 2, 0.35], [L / 2 - 2.1 + 0.55, 0.35], { arch: [L / 2 - 2.1, wheel, 0.58] }, [-L / 2 + 3 + 0.58, 0.35], { arch: [-L / 2 + 3, wheel, 0.58] }, [-L / 2, 0.35], [-L / 2, H - 0.2], [-L / 2 + 0.15, H], [L / 2 - 0.25, H], [L / 2, H - 0.3]];
  const paint = [strip(extrude(prof, W, 0.08))];
  const glass = [];
  for (const z of [-1, 1]) glass.push(strip(box(L - 1.6, 1.15, 0.04, -0.3, 2.05, z * (W / 2 + 0.01))));
  glass.push(strip(box(0.04, 1.6, W - 0.3, L / 2 + 0.01, 1.85, 0)));
  const trim = [];
  for (const z of [-1, 1]) trim.push(vcol(box(L - 0.4, 0.42, 0.03, 0, H - 0.25, z * (W / 2 + 0.012)), '#c9cdd1'));
  trim.push(vcol(box(L - 0.4, 0.04, W + 0.02, 0, H + 0.01, 0), '#c9cdd1'));
  for (const x of [L / 2 - 2.1, -L / 2 + 3]) for (const z of [-1, 1]) {
    const t = new THREE.CylinderGeometry(wheel, wheel, 0.32, 16); t.rotateX(Math.PI / 2); t.translate(x, wheel, z * (W / 2 - 0.18));
    trim.push(vcol(t, '#151515'));
  }
  trim.push(vcol(box(0.15, 0.3, W, L / 2 + 0.03, 0.45, 0), '#2a2c2e'));
  const lights = [vcol(box(0.04, 0.3, 1.6, L / 2 + 0.03, H - 0.4, 0), '#ffb020')];
  for (const z of [-1, 1]) {
    lights.push(vcol(box(0.04, 0.18, 0.35, L / 2 + 0.02, 0.75, z * (W / 2 - 0.3)), '#fff4d6'));
    lights.push(vcol(box(0.04, 0.4, 0.25, -L / 2 - 0.01, 1.0, z * (W / 2 - 0.25)), '#d1111a'));
  }
  return { paint: mergeGeometries(paint), trim: mergeGeometries(trim), glass: mergeGeometries(glass), lights: mergeGeometries(lights) };
}

/* ========================================================= street life */

const BUSY = { hollywood: 1, downtown: 1, mixed: 0.7, coastal: 0.75, beach: 0.8, midrise: 0.6, towers: 0.5, campus: 0.5, residential: 0.12, suburb: 0.06, industrial: 0.04, port: 0.02 };
const HOTSPOTS = ['Walk of Fame', 'TCL Chinese Theatre', 'Third Street Promenade', 'Venice Beach', 'Santa Monica Pier', 'Grand Central Market', 'Olvera Street', 'Rodeo Drive', 'Sunset Strip', 'Koreatown', 'Pershing Square', 'The Grove', 'Abbot Kinney', 'Little Tokyo', 'Chinatown Dragon Gate', 'Muscle Beach', 'Westwood Village'];
const PEOPLE = 1000;

export function buildStreetLife(W, scene, ctx) {
  const { batch, lo, mulberry } = ctx;
  const geos = carGeometries(), m = carMaterials();

  /* parked and abandoned cars */
  const byKind = {};
  for (const p of W.parked) (byKind[DIMS[p.kind] ? p.kind : 'sedan'] ||= []).push(p);
  for (const [kind, list] of Object.entries(byKind)) {
    const g = geos[kind];
    const items = (c) => list.map((p) => ({ x: p.x, z: p.z, py: p.y, ry: p.ry, c: c || p.color }));
    batch(items(), g.paint, m.paint, { far: 900 });
    batch(items('#ffffff'), g.trim, m.trim, { far: 500, cast: false });
    batch(items('#ffffff'), g.glass, m.glass, { far: 700, cast: false });
    batch(items('#ffffff'), g.lights, m.trim, { far: 300, cast: false });
  }

  /* pedestrians */
  const rnd = mulberry(77);
  const hot = W.pois.filter((p) => HOTSPOTS.includes(p.name));
  const freeways = W.roads.filter((r) => r.kind === 'freeway');
  const nearFreeway = (x, z) => freeways.some((r) => {
    for (let i = 0; i < r.pts.length - 1; i++) {
      const [ax, az] = r.pts[i], [bx, bz] = r.pts[i + 1];
      if (Math.max(ax, bx) + 40 < x || Math.min(ax, bx) - 40 > x || Math.max(az, bz) + 40 < z || Math.min(az, bz) - 40 > z) continue;
      const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
      const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
      if (Math.hypot(ax + dx * t - x, az + dz * t - z) < ROAD_WIDTH.freeway / 2 + 6) return true;
    }
    return false;
  });
  const paths = [];
  const addPath = (pts, lift, boost = 1) => {
    const pp = resample(pts, 4);
    const P = pp.map(([x, z]) => {
      const bad = Math.abs(x) > W.half - 10 || Math.abs(z) > W.half - 10 || W.isSea(x, z) || W.groundHeight(x, z) < 0.3 || nearFreeway(x, z);
      return { x, z, y: W.groundHeight(x, z) + lift, bad };
    });
    paths.push({ P, boost });
  };
  for (const r of W.roads) {
    if (r.kind !== 'blvd') continue;
    for (const side of [-1, 1]) addPath(offset(r.pts, side * (ROAD_WIDTH.blvd / 2 + 1.65)), 0.34 + 0.16);
  }
  const boardwalk = W.paint.find((o) => o.t === 'line' && o.c === '#c7bfb0');
  if (boardwalk) addPath(boardwalk.pts, 0.02, 3);

  // where to put people: weighted by how busy the neighbourhood is
  const slots = [];
  for (let pi = 0; pi < paths.length; pi++) {
    const { P, boost } = paths[pi];
    for (let k = 0; k < P.length; k += 3) {
      const p = P[k];
      if (p.bad) continue;
      let w = (BUSY[W.styleAt(p.x, p.z)] ?? 0.03) * boost;
      if (hot.some((h) => Math.hypot(h.x - p.x, h.z - p.z) < Math.max(200, h.r))) w *= 3;
      slots.push({ pi, k, w });
    }
  }
  const total = slots.reduce((s, x) => s + x.w, 0);
  const want = lo ? PEOPLE * 0.5 : PEOPLE;
  const peds = [];
  for (const s of slots) {
    if (rnd() < (s.w / total) * want) {
      peds.push({ path: paths[s.pi], s: s.k * 4 + rnd() * 4, dir: rnd() < 0.5 ? 1 : -1, speed: 1.1 + rnd() * 0.55, lat: (rnd() - 0.5) * 2.2, pause: 0, x: 0, y: 0, z: 0, yaw: 0, moving: true, scale: 0.9 + rnd() * 0.15 });
    }
  }

  const person = personGeometry();
  const n = Math.max(1, peds.length);
  const shirts = ['#f4f4f0', '#1d1d1f', '#c0392b', '#2c5f8a', '#f1c40f', '#7f8c8d', '#8e44ad', '#16a085', '#e67e22', '#d6c3a5', '#ff6fa8', '#3b6b3b'];
  const pants = ['#2d4a7a', '#1b1b1d', '#6b5d4f', '#3a3a3a', '#c9b48f', '#4c5a66', '#7a1f2b', '#e9e4d6'];
  const skins = ['#f1c7a5', '#e0ac85', '#c68d6a', '#a86b4a', '#8d5638', '#5f3a25'];
  const attr = (size, fn) => { const a = new Float32Array(n * size); peds.forEach((p, i) => { const v = fn(p, i); for (let c = 0; c < size; c++) a[i * size + c] = v[c]; }); return new THREE.InstancedBufferAttribute(a, size); };
  const rgb = (h) => { const c = new THREE.Color(h); return [c.r, c.g, c.b]; };
  person.setAttribute('aShirt', attr(3, () => rgb(shirts[Math.floor(rnd() * shirts.length)])));
  person.setAttribute('aPants', attr(3, () => rgb(pants[Math.floor(rnd() * pants.length)])));
  person.setAttribute('aSkin', attr(3, () => rgb(skins[Math.floor(rnd() * skins.length)])));
  const phase = attr(3, (p) => [rnd() * 6.28, p.speed * 4.2, 1]);
  person.setAttribute('aPhase', phase);
  const uniforms = { uTime: { value: 0 } };
  const pm = new THREE.MeshStandardMaterial({ roughness: 0.8 });
  pm.onBeforeCompile = (sh) => {
    sh.uniforms.uTime = uniforms.uTime;
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', `#include <common>
        attribute float aRegion; attribute float aPart; attribute vec3 aShirt; attribute vec3 aPants; attribute vec3 aSkin; attribute vec3 aPhase;
        uniform float uTime; varying vec3 vPC;`)
      .replace('#include <begin_vertex>', `#include <begin_vertex>
        float sw = sin(uTime * aPhase.y + aPhase.x) * aPhase.z;
        float ang = 0.0; float piv = 0.0;
        if (aPart > 0.5 && aPart < 2.5) { ang = (aPart < 1.5 ? sw : -sw) * 0.5; piv = 0.92; }
        else if (aPart > 2.5) { ang = (aPart < 3.5 ? -sw : sw) * 0.42; piv = 1.42; }
        if (aPart > 0.5) {
          float yy = transformed.y - piv, cs = cos(ang), sn = sin(ang);
          transformed = vec3(transformed.x * cs - yy * sn, transformed.x * sn + yy * cs + piv, transformed.z);
        }
        transformed.y += abs(sw) * 0.03;
        vPC = aRegion < 0.5 ? aShirt : aRegion < 1.5 ? aPants : aRegion < 2.5 ? aSkin : aRegion < 3.5 ? vec3(0.06, 0.05, 0.05) : vec3(0.92);`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nvarying vec3 vPC;')
      .replace('#include <color_fragment>', '#include <color_fragment>\ndiffuseColor.rgb *= vPC;');
  };
  const crowd = new THREE.InstancedMesh(person, pm, n);
  crowd.frustumCulled = false;
  crowd.castShadow = !lo;
  scene.add(crowd);
  if (!peds.length) crowd.count = 0;

  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0), v = new THREE.Vector3(), sc = new THREE.Vector3(), zero = new THREE.Vector3(0, 0, 0);
  let clock = 0;
  function update(dt, player) {
    clock += dt;
    uniforms.uTime.value = clock;
    const px = player ? player.x : 0, pz = player ? player.z : 0;
    for (let i = 0; i < peds.length; i++) {
      const p = peds[i];
      const P = p.path.P;
      const far = Math.abs(p.x - px) > 700 || Math.abs(p.z - pz) > 700;
      if (!far) {
        // stop for the player, browse a shop window now and then, else walk on
        const near = Math.hypot(p.x - px, p.z - pz) < 1.6;
        if (p.pause > 0) p.pause -= dt;
        else if (Math.random() < dt * 0.01) p.pause = 2 + Math.random() * 5;
        p.moving = !near && p.pause <= 0;
        if (p.moving) {
          p.s += p.speed * p.dir * dt;
          const k = Math.floor(p.s / 4) + (p.dir > 0 ? 1 : 0);
          if (k < 0 || k >= P.length || P[Math.max(0, Math.min(P.length - 1, k))].bad) { p.dir = -p.dir; p.s += p.speed * p.dir * dt * 2; }
        }
      }
      const f = Math.max(0, Math.min(P.length - 1.001, p.s / 4)), k = Math.floor(f), t = f - k;
      const a = P[k], b = P[Math.min(P.length - 1, k + 1)];
      const dx = (b.x - a.x) * p.dir, dz = (b.z - a.z) * p.dir, l = Math.hypot(dx, dz) || 1;
      const nx = -dz / l, nz = dx / l;
      p.x = a.x + (b.x - a.x) * t + nx * p.lat * p.dir; p.z = a.z + (b.z - a.z) * t + nz * p.lat * p.dir;
      p.y = a.y + (b.y - a.y) * t;
      p.yaw = Math.atan2(-dz, dx);
      phase.array[i * 3 + 2] = p.moving ? 1 : 0;
      q.setFromAxisAngle(up, p.yaw);
      m4.compose(v.set(p.x, p.y, p.z), q, far ? zero : sc.setScalar(p.scale));
      crowd.setMatrixAt(i, m4);
    }
    crowd.instanceMatrix.needsUpdate = true;
    phase.needsUpdate = true;
  }

  const api = { update, people: peds.length, peds };
  live.streetLife = api;
  return api;
}

/* A person about 1.75 m tall facing +x, feet at y = 0. Per-vertex aRegion
 * (0 shirt, 1 trousers, 2 skin, 3 hair/shoes, 4 white) and aPart (0 body,
 * 1-2 legs, 3-4 arms) drive the colours and the walk in the shader. */
function personGeometry() {
  const parts = [];
  const add = (g, region, part) => {
    const ng = g.index ? g.toNonIndexed() : g;
    const n = ng.attributes.position.count;
    ng.setAttribute('aRegion', new THREE.BufferAttribute(new Float32Array(n).fill(region), 1));
    ng.setAttribute('aPart', new THREE.BufferAttribute(new Float32Array(n).fill(part), 1));
    if (ng.attributes.uv) ng.deleteAttribute('uv');
    parts.push(ng);
  };
  const cyl = (r0, r1, h, x, y, z) => { const g = new THREE.CylinderGeometry(r0, r1, h, 7); g.translate(x, y, z); return g; };
  for (const [z, part] of [[0.1, 1], [-0.1, 2]]) {
    add(cyl(0.085, 0.07, 0.86, 0, 0.5, z), 1, part);
    add(box(0.26, 0.09, 0.11, 0.05, 0.05, z), 3, part);
  }
  add(cyl(0.17, 0.15, 0.2, 0, 0.98, 0), 1, 0);                       // hips
  const torso = new THREE.CapsuleGeometry(0.17, 0.32, 4, 8); torso.scale(0.75, 1, 1.15); torso.translate(0, 1.24, 0);
  add(torso, 0, 0);
  for (const [z, part] of [[0.25, 3], [-0.25, 4]]) {
    add(cyl(0.055, 0.05, 0.3, 0, 1.3, z), 0, part);
    add(cyl(0.045, 0.04, 0.26, 0.01, 1.02, z), 2, part);
    add(box(0.07, 0.09, 0.05, 0.01, 0.85, z), 2, part);
  }
  add(cyl(0.05, 0.05, 0.08, 0, 1.47, 0), 2, 0);
  const head = new THREE.SphereGeometry(0.11, 10, 8); head.scale(1, 1.15, 0.95); head.translate(0, 1.62, 0);
  add(head, 2, 0);
  const hair = new THREE.SphereGeometry(0.118, 10, 6, 0, Math.PI * 2, 0, Math.PI * 0.55); hair.translate(-0.01, 1.64, 0);
  add(hair, 3, 0);
  return mergeGeometries(parts);
}

function resample(pts, step) {
  const out = [pts[0]];
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[i + 1];
    const n = Math.max(1, Math.round(Math.hypot(bx - ax, bz - az) / step));
    for (let k = 1; k <= n; k++) out.push([ax + ((bx - ax) * k) / n, az + ((bz - az) * k) / n]);
  }
  return out;
}

function offset(pts, d) {
  return pts.map((p, i) => {
    const a = pts[Math.max(0, i - 1)], b = pts[Math.min(pts.length - 1, i + 1)];
    const L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1;
    return [p[0] - ((b[1] - a[1]) / L) * d, p[1] + ((b[0] - a[0]) / L) * d];
  });
}
