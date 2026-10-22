/*
 * The things that move: you, your parachute, the cars, the blimp.
 *
 * Car bodies are side profiles extruded across the car's width with
 * rounded (bevelled) edges, wheel arches cut into the outline, and a glass
 * greenhouse on top; the person is jointed capsules so knees and elbows
 * bend. Everything uses physically based materials, so paint and chrome
 * pick up the sky's reflections from the scene's environment map.
 */

import * as THREE from '../vendor/three.module.min.js';

/* ------------------------------------------------------------- materials */

const std = (color, roughness = 0.7, metalness = 0, extra = {}) => new THREE.MeshStandardMaterial({ color, roughness, metalness, ...extra });
const paint = (color, candy = false) => new THREE.MeshPhysicalMaterial({
  color, metalness: candy ? 0.75 : 0.45, roughness: candy ? 0.18 : 0.28, clearcoat: 1, clearcoatRoughness: 0.06
});
const glow = (color, intensity = 1.6) => std(color, 0.35, 0, { emissive: color, emissiveIntensity: intensity });
const M = {
  glass: std('#1d2731', 0.06, 0.6, { envMapIntensity: 1.4 }),
  chrome: std('#e9ecef', 0.12, 1),
  rubber: std('#161616', 0.85, 0),
  trim: std('#151719', 0.55, 0.2),
  rim: std('#a9afb5', 0.3, 0.9),
  well: std('#0b0b0c', 0.95, 0, { side: THREE.DoubleSide }),
  interior: std('#2a2623', 0.8, 0),
  head: glow('#fff8e1', 2.2),
  tail: glow('#ff2018', 1.4),
  amber: glow('#ffa31a', 1.2)
};

/* Bake several transformed geometries into one, so a car stays a handful
 * of meshes instead of dozens. */
function merge(geoms) {
  const pos = [], nor = [], uv = [];
  for (const g0 of geoms) {
    const g = g0.index ? g0.toNonIndexed() : g0;
    if (!g.attributes.normal) g.computeVertexNormals();
    pos.push(...g.attributes.position.array);
    nor.push(...g.attributes.normal.array);
    if (g.attributes.uv) uv.push(...g.attributes.uv.array);
    else for (let i = 0; i < g.attributes.position.count; i++) uv.push(0, 0);
  }
  const out = new THREE.BufferGeometry();
  out.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  out.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3));
  out.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  return out;
}
const at = (g, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0) => {
  g.applyMatrix4(new THREE.Matrix4().compose(new THREE.Vector3(x, y, z), new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz)), new THREE.Vector3(1, 1, 1)));
  return g;
};
const mesh = (geo, mat, parent) => {
  const m = new THREE.Mesh(geo, mat);
  m.castShadow = true;
  m.receiveShadow = true;
  if (parent) parent.add(m);
  return m;
};

/* A side profile (x along the car, y up) extruded across `width`, centred
 * on z, with rounded edges. */
function extrude(points, width, bevel = 0.06, holes = []) {
  const s = new THREE.Shape(points.map(([x, y]) => new THREE.Vector2(x, y)));
  for (const h of holes) s.holes.push(new THREE.Path(h.map(([x, y]) => new THREE.Vector2(x, y))));
  const depth = Math.max(0.01, width - bevel * 2);
  const g = new THREE.ExtrudeGeometry(s, { depth, bevelEnabled: bevel > 0, bevelThickness: bevel, bevelSize: bevel * 0.8, bevelSegments: 3, curveSegments: 10 });
  g.translate(0, 0, -depth / 2);
  g.computeVertexNormals();
  return g;
}

/* A rounded rectangle extruded: lamps, plates, panels. */
function slab(w, h, d, r = 0.04) {
  const s = new THREE.Shape();
  r = Math.min(r, w / 2, h / 2);
  s.moveTo(-w / 2 + r, -h / 2);
  s.lineTo(w / 2 - r, -h / 2); s.quadraticCurveTo(w / 2, -h / 2, w / 2, -h / 2 + r);
  s.lineTo(w / 2, h / 2 - r); s.quadraticCurveTo(w / 2, h / 2, w / 2 - r, h / 2);
  s.lineTo(-w / 2 + r, h / 2); s.quadraticCurveTo(-w / 2, h / 2, -w / 2, h / 2 - r);
  s.lineTo(-w / 2, -h / 2 + r); s.quadraticCurveTo(-w / 2, -h / 2, -w / 2 + r, -h / 2);
  const g = new THREE.ExtrudeGeometry(s, { depth: d, bevelEnabled: false, curveSegments: 4 });
  g.translate(0, 0, -d / 2);
  return g;
}

function canvasTex(w, h, draw) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 4;
  return t;
}

let plateTex = null;
function californiaPlate() {
  plateTex ||= canvasTex(256, 128, (ctx, w, h) => {
    ctx.fillStyle = '#f7f7f2'; ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = '#2a3b8f'; ctx.lineWidth = 6; ctx.strokeRect(4, 4, w - 8, h - 8);
    ctx.fillStyle = '#c8202f'; ctx.font = 'italic 700 30px Georgia, serif'; ctx.textAlign = 'center';
    ctx.fillText('California', w / 2, 36);
    ctx.fillStyle = '#1b2a6b'; ctx.font = '900 54px "Arial Narrow", Arial, sans-serif';
    ctx.fillText('7LAR0YL', w / 2, 98);
    ctx.font = '600 13px Arial'; ctx.fillText('dmv.ca.gov', w / 2, 120);
  });
  return plateTex;
}

/* ------------------------------------------------------------------ cars */

export const CAR_TYPES = {
  convertible: { name: 'Convertible', top: 44, accel: 14, color: ['#c3162d', '#1b998b', '#f2f2ee', '#2e6fdc'], len: 4.6, w: 1.95, roof: false },
  lowrider: { name: 'Lowrider', top: 32, accel: 10, color: ['#6a1fb0', '#0b9aa8', '#c9952a', '#d1233a'], len: 5.4, w: 2.0, roof: true, chrome: true, hop: true },
  cruiser: { name: 'Police Cruiser', top: 50, accel: 16, color: ['#0d0d0f'], len: 5.0, w: 1.98, roof: true, police: true },
  taxi: { name: 'Taxi', top: 40, accel: 13, color: ['#f2c00b'], len: 4.9, w: 1.92, roof: true, taxi: true },
  suv: { name: 'SUV', top: 40, accel: 12, color: ['#151618', '#eceeee', '#3c4a57'], len: 4.95, w: 2.02, roof: true, tall: true },
  exotic: { name: 'Supercar', top: 60, accel: 20, color: ['#ff7a00', '#9bd21d', '#d40a0a', '#f4f4f4'], len: 4.6, w: 2.05, roof: true, low: true },
  truck: { name: 'Pickup', top: 34, accel: 11, color: ['#7a8288', '#7c1f1f', '#26384b'], len: 5.6, w: 2.06, roof: true, bed: true, tall: true },
  tacotruck: { name: 'Taco Truck', top: 26, accel: 8, color: ['#f3efe6'], len: 6.4, w: 2.4, van: true }
};

/* Side profiles. `top` runs over the car from the front bumper to the rear
 * bumper; the underside and the wheel arches are generated. `glass` is the
 * greenhouse outline, `roof` the painted roof skin over it. */
const SHAPES = {
  sedan: {
    L: 4.9, r: 0.36, c: 0.3, fx: 1.45, rx: -1.45,
    top: [[2.45, 0.32], [2.49, 0.6], [2.44, 0.76], [2.2, 0.84], [1.0, 0.94], [-1.4, 0.97], [-2.3, 0.92], [-2.45, 0.78], [-2.47, 0.34]],
    glass: [[1.02, 0.93], [0.28, 1.42], [-0.72, 1.44], [-1.42, 0.97]],
    roof: [[0.32, 1.4], [-0.74, 1.42]]
  },
  lowrider: {
    L: 5.4, r: 0.34, c: 0.17, fx: 1.5, rx: -1.55,
    top: [[2.7, 0.2], [2.73, 0.52], [2.66, 0.72], [2.45, 0.8], [1.1, 0.86], [-1.45, 0.88], [-2.55, 0.86], [-2.72, 0.7], [-2.74, 0.24]],
    glass: [[1.0, 0.85], [0.3, 1.27], [-0.86, 1.28], [-1.52, 0.88]],
    roof: [[0.32, 1.26], [-0.88, 1.27]]
  },
  convertible: {
    L: 4.6, r: 0.36, c: 0.28, fx: 1.38, rx: -1.38,
    top: [[2.3, 0.3], [2.34, 0.58], [2.24, 0.78], [1.9, 0.86], [0.78, 0.92], [-1.6, 0.93], [-2.18, 0.9], [-2.32, 0.7], [-2.3, 0.32]]
  },
  suv: {
    L: 4.95, r: 0.43, c: 0.42, fx: 1.48, rx: -1.45,
    top: [[2.47, 0.44], [2.5, 0.82], [2.42, 1.08], [2.1, 1.16], [1.32, 1.22], [-2.3, 1.24], [-2.46, 1.06], [-2.47, 0.46]],
    glass: [[1.32, 1.21], [0.72, 1.8], [-2.25, 1.82], [-2.42, 1.24]],
    roof: [[0.74, 1.79], [-2.27, 1.81]]
  },
  exotic: {
    L: 4.6, r: 0.36, c: 0.14, fx: 1.38, rx: -1.32, rr: 0.39,
    top: [[2.3, 0.18], [2.34, 0.36], [2.2, 0.48], [1.4, 0.66], [0.62, 0.8], [-1.62, 0.86], [-2.18, 0.84], [-2.32, 0.6], [-2.3, 0.2]],
    glass: [[0.62, 0.79], [-0.22, 1.13], [-0.96, 1.12], [-1.72, 0.86]],
    roof: [[-0.2, 1.12], [-0.98, 1.11]]
  },
  truck: {
    L: 5.6, r: 0.43, c: 0.44, fx: 1.8, rx: -1.62,
    top: [[2.8, 0.46], [2.84, 0.88], [2.76, 1.12], [2.5, 1.2], [1.45, 1.24], [-0.55, 1.26], [-2.74, 1.22], [-2.82, 1.0], [-2.8, 0.46]],
    glass: [[1.42, 1.23], [0.96, 1.84], [-0.38, 1.86], [-0.52, 1.26]],
    roof: [[0.98, 1.83], [-0.4, 1.85]]
  },
  van: {
    L: 6.4, r: 0.46, c: 0.46, fx: 2.05, rx: -1.95,
    top: [[3.2, 0.5], [3.24, 1.08], [3.12, 1.3], [2.62, 1.42], [2.3, 2.82], [2.1, 2.92], [-3.1, 2.92], [-3.2, 2.8], [-3.2, 0.5]]
  }
};
const SHAPE_OF = { convertible: 'convertible', lowrider: 'lowrider', cruiser: 'sedan', taxi: 'sedan', suv: 'suv', exotic: 'exotic', truck: 'truck', tacotruck: 'van' };

/* The full body outline: the given top line, then back along the bottom
 * with a half-round arch cut over each wheel. */
function bodyOutline(S) {
  const pts = S.top.map((p) => [...p]);
  const arch = (x, r) => {
    const R = r + 0.1, c = S.c, e = Math.asin(Math.min(0.99, Math.max(-0.99, (r - c) / R)));
    const out = [];
    for (let k = 0; k <= 14; k++) {
      const a = -e + (k / 14) * (Math.PI + 2 * e);   // front edge, over the top, to the rear edge
      out.push([x + R * Math.cos(a), r + R * Math.sin(a)]);
    }
    return out;
  };
  // bottom, rear to front
  const rearR = S.rr || S.r;
  const bottom = [[S.top[S.top.length - 1][0] + 0.02, S.c], ...arch(S.rx, rearR).reverse(), ...arch(S.fx, S.r).reverse(), [S.top[0][0] - 0.02, S.c]];
  // arch() runs front→rear over the top; reversed it runs rear→front, which is
  // the direction the bottom edge is walked here.
  return [...pts, ...bottom.map((p) => p)].filter((p, i, a) => i === 0 || Math.hypot(p[0] - a[i - 1][0], p[1] - a[i - 1][1]) > 1e-4);
}

/* A wheel: tyre with a rounded sidewall, and a rim. Axis along z, hub face
 * towards +z (mirror it for the other side). */
const wheelCache = new Map();
function wheelGeoms(r, wide, style) {
  const key = `${r}|${wide}|${style}`;
  if (wheelCache.has(key)) return wheelCache.get(key);
  const w = wide / 2;
  const prof = [];
  const rim = r * (style === 'wire' ? 0.62 : 0.68);
  prof.push([rim, -w * 0.9]);
  for (let k = 0; k <= 8; k++) { const a = -Math.PI / 2 + (k / 8) * Math.PI; prof.push([r - 0.05 + Math.cos(a) * 0.05, Math.sin(a) * w]); }
  prof.push([rim, w * 0.9]);
  const tyre = new THREE.LatheGeometry(prof.map(([x, y]) => new THREE.Vector2(x, y)), 22);
  tyre.rotateX(Math.PI / 2);
  const parts = [];
  // rim barrel and face
  parts.push(at(new THREE.CylinderGeometry(rim, rim, wide * 0.8, 20, 1, true), 0, 0, 0, Math.PI / 2));
  parts.push(at(new THREE.CylinderGeometry(rim * 0.98, rim * 0.98, 0.02, 20), 0, 0, -w * 0.15, Math.PI / 2));
  parts.push(at(new THREE.TorusGeometry(rim * 0.98, 0.022, 6, 24), 0, 0, w * 0.35));
  parts.push(at(new THREE.CylinderGeometry(rim * 0.22, rim * 0.26, 0.12, 12), 0, 0, w * 0.32, Math.PI / 2));
  if (style === 'wire') {
    for (let k = 0; k < 24; k++) {
      const a = (k / 24) * Math.PI * 2;
      const sp = new THREE.CylinderGeometry(0.006, 0.006, rim * 0.8, 3);
      parts.push(at(sp, Math.cos(a) * rim * 0.5, Math.sin(a) * rim * 0.5, w * (0.12 + (k % 2) * 0.12), 0, 0, a - Math.PI / 2 + (k % 2 ? 0.25 : -0.25)));
    }
    parts.push(at(new THREE.ConeGeometry(rim * 0.2, 0.16, 3), 0, 0, w * 0.48, Math.PI / 2));   // knock-off spinner
  } else {
    const n = style === 'sport' ? 10 : style === 'steel' ? 6 : 5;
    for (let k = 0; k < n; k++) {
      const a = (k / n) * Math.PI * 2;
      parts.push(at(new THREE.BoxGeometry(rim * 0.78, style === 'sport' ? 0.035 : 0.07, 0.04), Math.cos(a) * rim * 0.5, Math.sin(a) * rim * 0.5, w * 0.28, 0, 0, a));
    }
  }
  const out = { tyre, rim: merge(parts) };
  wheelCache.set(key, out);
  return out;
}

export function makeCar(type, colorIndex = 0) {
  const T = CAR_TYPES[type];
  const S = SHAPES[SHAPE_OF[type]];
  const color = T.color[colorIndex % T.color.length];
  const W = T.w;
  const root = new THREE.Group();
  const chassis = new THREE.Group();
  root.add(chassis);
  const body = paint(color, type === 'lowrider');
  const bump = T.chrome || type === 'convertible' || type === 'tacotruck' ? M.chrome : M.trim;
  const front = S.top[0][0], rear = S.top[S.top.length - 1][0];

  // the body shell
  mesh(extrude(bodyOutline(S), W, type === 'tacotruck' ? 0.08 : 0.07), body, chassis);
  // greenhouse and roof skin
  if (S.glass) {
    mesh(extrude(S.glass, W - 0.24, 0.05), M.glass, chassis);
    if (S.roof) {
      const [[a, ay], [b, by]] = S.roof;
      mesh(extrude([[a + 0.02, ay - 0.02], [a + 0.06, ay + 0.06], [b - 0.04, by + 0.06], [b - 0.02, by - 0.02]], W - 0.2, 0.05), type === 'cruiser' ? paint('#f4f4f4') : body, chassis);
    }
    // A and B pillars in body colour
    const pillars = [];
    const [g0, g1, g2, g3] = S.glass;
    for (const side of [-1, 1]) {
      const z = side * (W / 2 - 0.13);
      for (const [p, q] of [[g0, g1], [g2, g3]]) {
        const len = Math.hypot(q[0] - p[0], q[1] - p[1]);
        pillars.push(at(new THREE.BoxGeometry(len, 0.08, 0.05), (p[0] + q[0]) / 2, (p[1] + q[1]) / 2, z, 0, 0, Math.atan2(q[1] - p[1], q[0] - p[0])));
      }
      const midX = (g1[0] + g2[0]) / 2 + 0.05;
      pillars.push(at(new THREE.BoxGeometry(0.09, g1[1] - g0[1] - 0.04, 0.05), midX, (g0[1] + g1[1]) / 2 + 0.02, z));
    }
    mesh(merge(pillars), type === 'cruiser' ? paint('#f4f4f4') : body, chassis);
  }

  // dark wheel wells inside the arches
  const wells = [];
  for (const [x, r] of [[S.fx, S.r], [S.rx, S.rr || S.r]]) wells.push(at(new THREE.CylinderGeometry(r + 0.1, r + 0.1, W - 0.12, 16, 1, true, Math.PI / 2, Math.PI), x, r, 0, Math.PI / 2));
  mesh(merge(wells), M.well, chassis);
  mesh(at(new THREE.BoxGeometry(Math.abs(rear - front) * 0.9, 0.12, W - 0.2), (front + rear) / 2, S.c + 0.04, 0), M.well, chassis);

  // lamps, bumpers, grille, plates
  const yLamp = type === 'tacotruck' ? 1.0 : (S.top[1][1] + S.top[2][1]) / 2;
  const heads = [], tails = [];
  for (const side of [-1, 1]) {
    heads.push(at(slab(0.34, type === 'exotic' ? 0.08 : 0.16, 0.08, 0.05), front + 0.005, yLamp, side * W * 0.33, 0, Math.PI / 2));
    tails.push(at(slab(0.36, 0.15, 0.08, 0.04), rear - 0.005, (S.top.at(-2)[1] + S.top.at(-3)[1]) / 2 - 0.02, side * W * 0.34, 0, Math.PI / 2));
  }
  mesh(merge(heads), M.head, chassis);
  mesh(merge(tails), M.tail, chassis);
  const bumpers = [];
  bumpers.push(at(slab(W - 0.08, 0.16, 0.12, 0.06), front + 0.03, S.c + 0.14, 0, 0, Math.PI / 2));
  bumpers.push(at(slab(W - 0.08, 0.16, 0.12, 0.06), rear - 0.03, S.c + 0.14, 0, 0, Math.PI / 2));
  mesh(merge(bumpers), bump, chassis);
  if (type !== 'exotic') {
    const grille = [at(slab(W * 0.42, Math.max(0.1, yLamp - S.c - 0.2), 0.04, 0.03), front + 0.01, (yLamp + S.c + 0.22) / 2, 0, 0, Math.PI / 2)];
    mesh(merge(grille), type === 'lowrider' || type === 'tacotruck' ? M.chrome : M.trim, chassis);
  } else {
    // side intakes and a rear wing
    const ex = [];
    for (const side of [-1, 1]) ex.push(at(slab(0.7, 0.22, 0.04, 0.06), -0.5, 0.48, side * (W / 2 + 0.005)));
    ex.push(at(new THREE.BoxGeometry(0.34, 0.04, W * 0.92), rear + 0.18, 1.02, 0));
    for (const side of [-1, 1]) ex.push(at(new THREE.BoxGeometry(0.08, 0.16, 0.05), rear + 0.2, 0.92, side * W * 0.3));
    mesh(merge(ex), M.trim, chassis);
  }
  const plateMat = std('#ffffff', 0.5, 0, { map: californiaPlate() });
  for (const [x, ry] of [[front + 0.1, Math.PI / 2], [rear - 0.1, -Math.PI / 2]]) {
    const p = mesh(new THREE.PlaneGeometry(0.52, 0.26), plateMat, chassis);
    p.position.set(x, S.c + 0.32, 0);
    p.rotation.y = ry;
  }

  // mirrors, door lines and side trim
  const bits = [], lines = [];
  const beltY = type === 'tacotruck' ? 1.4 : (S.glass ? S.glass[0][1] : S.top[4][1]);
  const doorX = S.glass ? S.glass[0][0] - 0.05 : type === 'tacotruck' ? 2.25 : 0.7;
  for (const side of [-1, 1]) {
    const z = side * (W / 2 + 0.11);
    bits.push(at(slab(0.16, 0.12, 0.14, 0.04), doorX, beltY + 0.1, z));
    bits.push(at(new THREE.BoxGeometry(0.04, 0.03, 0.14), doorX, beltY + 0.05, side * (W / 2 + 0.04)));
    if (type !== 'tacotruck') {
      const zl = side * (W / 2 + 0.004);
      lines.push(at(new THREE.BoxGeometry(0.012, beltY - S.c - 0.12, 0.012), doorX - 0.05, (beltY + S.c) / 2 + 0.03, zl));
      if (type !== 'convertible' && type !== 'exotic') lines.push(at(new THREE.BoxGeometry(0.012, beltY - S.c - 0.12, 0.012), (S.glass[2][0] + S.glass[1][0]) / 2, (beltY + S.c) / 2 + 0.03, zl));
      lines.push(at(new THREE.BoxGeometry(0.15, 0.02, 0.016), doorX - 0.3, beltY - 0.15, zl));
    }
  }
  mesh(merge(bits), M.trim, chassis);
  if (lines.length) mesh(merge(lines), M.trim, chassis);
  if (T.chrome) {
    const strip = [];
    // chrome spear down each flank, broken by the wheel arches
    const R = S.r + 0.12;
    const runs = [[rear + 0.1, S.rx - R], [S.rx + R, S.fx - R], [S.fx + R, front - 0.1]];
    for (const side of [-1, 1]) for (const [a, b] of runs) strip.push(at(new THREE.BoxGeometry(b - a, 0.035, 0.02), (a + b) / 2, 0.62, side * (W / 2 + 0.012)));
    strip.push(at(new THREE.BoxGeometry(0.03, 0.03, W * 0.8), S.glass[1][0], S.glass[1][1] + 0.02, 0, 0, 0, 0));
    mesh(merge(strip), M.chrome, chassis);
  }

  // type-specific kit
  if (type === 'convertible') {
    const cab = [];
    cab.push(at(new THREE.BoxGeometry(2.0, 0.06, W - 0.32), -0.45, 0.9, 0));                          // the tub
    for (const z of [-0.42, 0.42]) {
      cab.push(at(slab(0.5, 0.12, 0.5, 0.06), -0.2, 0.98, z, Math.PI / 2));                           // seat cushions
      cab.push(at(slab(0.5, 0.62, 0.12, 0.08), -0.48, 1.22, z, 0, Math.PI / 2, 0.25));                // seat backs
      cab.push(at(slab(0.5, 0.5, 0.12, 0.08), -1.25, 1.0, z, 0, Math.PI / 2, 0.25));
    }
    cab.push(at(new THREE.TorusGeometry(0.18, 0.025, 6, 16), 0.32, 1.12, 0.42, 0, Math.PI / 2 - 0.4));    // steering wheel
    cab.push(at(new THREE.BoxGeometry(0.4, 0.2, W - 0.36), 0.62, 0.98, 0));                           // dash
    mesh(merge(cab), std('#d9c9a8', 0.75), chassis);
    const ws = mesh(new THREE.PlaneGeometry(W - 0.3, 0.42), M.glass, chassis);
    ws.position.set(0.8, 1.13, 0); ws.rotation.set(0, Math.PI / 2, 0); ws.rotateX(-0.95);
    const frame = [];
    frame.push(at(new THREE.BoxGeometry(0.04, 0.04, W - 0.26), 0.66, 1.33, 0));
    for (const side of [-1, 1]) frame.push(at(new THREE.BoxGeometry(0.04, 0.5, 0.04), 0.8, 1.13, side * (W / 2 - 0.14), 0, 0, 0.6));
    mesh(merge(frame), M.chrome, chassis);
  }
  if (type === 'taxi') {
    const sign = mesh(slab(0.9, 0.3, 0.26, 0.08), std('#ffffff', 0.4, 0, { map: canvasTex(256, 96, (ctx, w, h) => {
      ctx.fillStyle = '#fff6c9'; ctx.fillRect(0, 0, w, h);
      ctx.fillStyle = '#1b1b1b'; ctx.font = '900 64px "Arial Black", Arial'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText('TAXI', w / 2, h / 2 + 4);
    }), emissive: '#fff2a0', emissiveIntensity: 0.25 }), chassis);
    sign.position.set(-0.2, S.roof[0][1] + 0.24, 0); sign.rotation.y = Math.PI / 2;
    const check = [];
    for (const side of [-1, 1]) for (let k = 0; k < 14; k++) check.push(at(new THREE.BoxGeometry(0.12, 0.06, 0.01), -1.3 + k * 0.24, 0.66 + (k % 2) * 0.06, side * (W / 2 + 0.006)));
    mesh(merge(check), M.trim, chassis);
  }
  if (type === 'cruiser') {
    // two-tone doors, push bar, light bar, door shield
    for (const side of [-1, 1]) {
      const doors = mesh(extrude([[1.0, 0.42], [1.0, 0.92], [-1.38, 0.95], [-1.38, 0.42]], 0.02, 0), paint('#f4f4f4'), chassis);
      doors.position.z = side * (W / 2 + 0.002);
      const shield = mesh(new THREE.CircleGeometry(0.16, 6), std('#c9a53a', 0.4, 0.7), chassis);
      shield.position.set(-0.2, 0.7, side * (W / 2 + 0.03)); shield.rotation.y = side > 0 ? 0 : Math.PI;
    }
    const bar = [];
    bar.push(at(new THREE.BoxGeometry(0.06, 0.42, W * 0.5), front + 0.2, 0.62, 0));
    for (const y of [0.5, 0.74]) bar.push(at(new THREE.BoxGeometry(0.06, 0.05, W * 0.62), front + 0.21, y, 0));
    mesh(merge(bar), M.trim, chassis);
    const lb = new THREE.Group();
    lb.position.set(-0.22, S.roof[0][1] + 0.12, 0);
    mesh(slab(0.32, 0.1, W * 0.82, 0.04), M.trim, lb).rotation.y = Math.PI / 2;
    const red = mesh(slab(0.28, 0.13, W * 0.36, 0.05), glow('#ff1a1a', 2.4), lb);
    red.position.set(0, 0.06, -W * 0.21); red.rotation.y = Math.PI / 2;
    const blue = mesh(slab(0.28, 0.13, W * 0.36, 0.05), glow('#1f5bff', 2.4), lb);
    blue.position.set(0, 0.06, W * 0.21); blue.rotation.y = Math.PI / 2;
    chassis.add(lb);
    root.userData.siren = { red, blue };
  }
  if (type === 'suv') {
    const rack = [];
    for (const side of [-1, 1]) rack.push(at(new THREE.BoxGeometry(2.6, 0.05, 0.05), -0.75, 1.92, side * (W / 2 - 0.25)));
    for (const x of [-1.6, 0.0]) rack.push(at(new THREE.BoxGeometry(0.05, 0.04, W - 0.5), x, 1.93, 0));
    rack.push(at(new THREE.CylinderGeometry(0.4, 0.4, 0.24, 16), rear - 0.1, 0.95, 0, 0, 0, Math.PI / 2));   // spare tyre
    mesh(merge(rack), M.trim, chassis);
  }
  if (type === 'truck') {
    const bed = [];
    bed.push(at(new THREE.BoxGeometry(2.0, 0.02, W - 0.3), -1.68, 1.265, 0));                       // bed liner
    for (let k = 0; k < 6; k++) bed.push(at(new THREE.BoxGeometry(1.9, 0.015, 0.05), -1.68, 1.28, -0.7 + k * 0.28));
    bed.push(at(new THREE.BoxGeometry(0.03, 0.3, W - 0.1), -2.8, 1.0, 0));                           // tailgate seam
    mesh(merge(bed), M.interior, chassis);
  }
  if (type === 'tacotruck') {
    const ws = mesh(new THREE.PlaneGeometry(W - 0.3, 1.36), M.glass, chassis);
    ws.position.set(2.49, 2.1, 0); ws.rotation.set(0, Math.PI / 2, 0); ws.rotateX(-0.22);
    // the serving window, awning and menu on the +z side
    const win = mesh(new THREE.BoxGeometry(2.4, 1.0, 0.06), std('#3a2a20', 0.6), chassis);
    win.position.set(-0.6, 1.9, W / 2 + 0.01);
    const counter = mesh(new THREE.BoxGeometry(2.5, 0.06, 0.32), M.chrome, chassis);
    counter.position.set(-0.6, 1.38, W / 2 + 0.16);
    const awning = mesh(new THREE.BoxGeometry(2.7, 0.05, 1.0), std('#d7263d', 0.6), chassis);
    awning.position.set(-0.6, 2.55, W / 2 + 0.45); awning.rotation.x = 0.28;
    const menu = mesh(new THREE.PlaneGeometry(1.1, 0.9), std('#ffffff', 0.6, 0, { map: canvasTex(256, 210, (ctx, w, h) => {
      ctx.fillStyle = '#1d1d1d'; ctx.fillRect(0, 0, w, h);
      ctx.fillStyle = '#ffd23f'; ctx.font = '900 34px "Arial Black", Arial'; ctx.fillText('MENU', 18, 40);
      ctx.fillStyle = '#ffffff'; ctx.font = '700 22px Arial';
      ['Al pastor  $2', 'Carne asada  $2.5', 'Pollo  $2', 'Burrito  $9', 'Horchata  $3'].forEach((l, i) => ctx.fillText(l, 18, 80 + i * 26));
    }) }), chassis);
    menu.position.set(1.15, 1.85, W / 2 + 0.02);
    const sign = mesh(slab(2.8, 0.55, 0.12, 0.08), std('#ffffff', 0.5, 0, { map: canvasTex(512, 100, (ctx, w, h) => {
      ctx.fillStyle = '#c1121f'; ctx.fillRect(0, 0, w, h);
      ctx.fillStyle = '#ffd23f'; ctx.font = '900 72px "Arial Black", Arial'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText('TACOS', w / 2, h / 2 + 4);
    }), emissive: '#ff3030', emissiveIntensity: 0.15 }), chassis);
    sign.position.set(-0.6, 3.25, 0); sign.rotation.y = 0;
    const st = mesh(new THREE.BoxGeometry(0.1, 0.3, 0.1), M.trim, chassis); st.position.set(-0.6, 3.0, 0);
    const vent = mesh(new THREE.BoxGeometry(0.5, 0.2, 0.5), M.chrome, chassis); vent.position.set(-2.2, 3.02, -0.4);
  }

  // wheels
  const wheels = [];
  const style = T.chrome ? 'wire' : type === 'exotic' ? 'sport' : type === 'tacotruck' || type === 'truck' ? 'steel' : 'alloy';
  const rimMat = T.chrome ? M.chrome : type === 'exotic' ? std('#2a2a2c', 0.35, 0.8) : M.rim;
  for (const [x, front2] of [[S.fx, true], [S.rx, false]]) {
    const r = front2 ? S.r : (S.rr || S.r);
    const g = wheelGeoms(r, type === 'exotic' && !front2 ? 0.34 : type === 'tacotruck' ? 0.3 : 0.26, style);
    for (const side of [-1, 1]) {
      const steer = new THREE.Group();
      steer.position.set(x, r, side * (W / 2 - 0.16));
      const spin = new THREE.Group();
      const face = new THREE.Group();
      face.scale.z = side;                       // hub faces outwards on both sides
      mesh(g.tyre, M.rubber, face);
      mesh(g.rim, rimMat, face);
      spin.add(face);
      steer.add(spin);
      root.add(steer);
      wheels.push({ steer, spin, front: front2 });
    }
  }
  return { root, chassis, wheels, type, T, radius: S.r };
}

/* ---------------------------------------------------------------- player */

function capsule(r, len, mat, parent, y = 0) {
  const g = new THREE.CapsuleGeometry(r, Math.max(0.01, len - 2 * r), 5, 10);
  g.translate(0, -len / 2, 0);
  const m = mesh(g, mat, parent);
  m.position.y = y;
  return m;
}

export function makePlayer(look = {}) {
  const skin = std(look.skin || '#b97e5a', 0.6);
  const shirt = std(look.shirt || '#f2f2ee', 0.85);
  const denim = std(look.pants || '#2f4f84', 0.9);
  const sneaker = std('#f4f4f4', 0.6), sole = std('#d6d0c4', 0.8);
  const cap = std(look.cap || '#c41e2a', 0.7), hair = std('#2a1d16', 0.9), shades = std('#0b0b0b', 0.15, 0.6);

  const root = new THREE.Group();
  const body = new THREE.Group();          // whole-body pose (lean, freefall)
  body.position.y = 0.94;
  root.add(body);

  // pelvis (jeans) and torso (tee) as lathed shapes, flattened front to back
  const lathe = (pts, mat, parent, y, sz = 0.62) => {
    const g = new THREE.LatheGeometry(pts.map(([x, yy]) => new THREE.Vector2(x, yy)), 16);
    g.scale(1, 1, sz);
    const m = mesh(g, mat, parent);
    m.position.y = y;
    return m;
  };
  lathe([[0.001, -0.1], [0.12, -0.09], [0.165, -0.02], [0.16, 0.08], [0.15, 0.12], [0.001, 0.12]], denim, body, 0);
  mesh(at(new THREE.TorusGeometry(0.152, 0.016, 6, 20), 0, 0.105, 0, Math.PI / 2), std('#3a2a1e', 0.6), body).scale.z = 0.62;
  const spine = new THREE.Group();
  spine.position.y = 0.1;
  body.add(spine);
  lathe([[0.001, 0], [0.15, 0], [0.158, 0.1], [0.152, 0.2], [0.18, 0.32], [0.205, 0.42], [0.2, 0.48], [0.13, 0.53], [0.06, 0.55], [0.001, 0.55]], shirt, spine, 0, 0.6);
  capsule(0.045, 0.1, skin, spine, 0.6);

  // head
  const head = new THREE.Group();
  head.position.y = 0.66;
  spine.add(head);
  const skull = mesh(new THREE.SphereGeometry(0.1, 18, 14), skin, head);
  skull.scale.set(1, 1.2, 1.06); skull.position.y = 0.06;
  const jaw = mesh(new THREE.SphereGeometry(0.075, 14, 10), skin, head);
  jaw.scale.set(1.05, 0.8, 1.1); jaw.position.set(0, -0.01, 0.025);
  const nose = mesh(new THREE.ConeGeometry(0.018, 0.05, 6), skin, head);
  nose.position.set(0, 0.04, 0.11); nose.rotation.x = Math.PI / 2 - 0.3;
  for (const s of [-1, 1]) {
    const ear = mesh(new THREE.SphereGeometry(0.022, 8, 6), skin, head);
    ear.scale.set(0.5, 1, 0.8); ear.position.set(s * 0.1, 0.05, 0);
    const lens = mesh(slab(0.07, 0.04, 0.012, 0.015), shades, head);
    lens.position.set(s * 0.042, 0.075, 0.1);
    lens.rotation.y = -s * 0.25;
    const arm = mesh(new THREE.BoxGeometry(0.006, 0.008, 0.1), shades, head);
    arm.position.set(s * 0.095, 0.08, 0.05);
  }
  mesh(new THREE.BoxGeometry(0.03, 0.008, 0.01), shades, head).position.set(0, 0.08, 0.107);
  mesh(new THREE.BoxGeometry(0.045, 0.008, 0.01), std('#6e3b2c', 0.7), head).position.set(0, -0.015, 0.1);
  const hairCap = mesh(new THREE.SphereGeometry(0.105, 16, 10, 0, Math.PI * 2, 0, Math.PI * 0.62), hair, head);
  hairCap.scale.set(1.02, 1.18, 1.08); hairCap.position.set(0, 0.07, -0.01); hairCap.rotation.x = -0.5;
  const capDome = mesh(new THREE.SphereGeometry(0.11, 16, 8, 0, Math.PI * 2, 0, Math.PI / 2), cap, head);
  capDome.scale.set(1, 0.8, 1.08); capDome.position.set(0, 0.105, -0.005);
  const brim = mesh(new THREE.CylinderGeometry(0.09, 0.09, 0.012, 16, 1, false, 0, Math.PI), cap, head);
  brim.position.set(0, 0.11, -0.08); brim.rotation.set(0.12, Math.PI / 2, 0); brim.scale.set(1, 1, 0.9);
  mesh(new THREE.SphereGeometry(0.012, 6, 4), cap, head).position.set(0, 0.19, 0);

  // arms: shoulder → upper arm (sleeve) → elbow → forearm → hand
  const arm = (side) => {
    const shoulder = new THREE.Group();
    shoulder.position.set(side * 0.215, 0.47, 0);
    spine.add(shoulder);
    capsule(0.05, 0.3, skin, shoulder);
    capsule(0.062, 0.16, shirt, shoulder, 0.01);
    const elbow = new THREE.Group();
    elbow.position.y = -0.28;
    shoulder.add(elbow);
    capsule(0.042, 0.26, skin, elbow);
    const hand = mesh(new THREE.SphereGeometry(0.045, 10, 8), skin, elbow);
    hand.scale.set(0.8, 1.25, 1); hand.position.y = -0.29;
    return { shoulder, elbow };
  };
  const armL = arm(-1), armR = arm(1);

  // legs: hip → thigh → knee → shin → sneaker
  const leg = (side) => {
    const hip = new THREE.Group();
    hip.position.set(side * 0.09, -0.02, 0);
    body.add(hip);
    capsule(0.075, 0.46, denim, hip);
    const knee = new THREE.Group();
    knee.position.y = -0.44;
    hip.add(knee);
    capsule(0.062, 0.44, denim, knee);
    const foot = new THREE.Group();
    foot.position.y = -0.43;
    knee.add(foot);
    const shoe = mesh(new THREE.CapsuleGeometry(0.05, 0.16, 4, 8), sneaker, foot);
    shoe.rotation.x = Math.PI / 2; shoe.scale.set(1.05, 1, 0.8); shoe.position.set(0, -0.035, 0.05);
    const s = mesh(slab(0.11, 0.26, 0.03, 0.05), sole, foot);
    s.rotation.x = Math.PI / 2; s.position.set(0, -0.075, 0.05);
    mesh(new THREE.BoxGeometry(0.08, 0.012, 0.06), std('#c41e2a', 0.6), foot).position.set(0, -0.01, 0.08);
    return { hip, knee, foot };
  };
  const legL = leg(-1), legR = leg(1);

  /* the ram-air parachute: a row of arched cells in alternating colours */
  const chute = new THREE.Group();
  const cells = 9, span = 8.4;
  const cols = [std('#ff3d6b', 0.7, 0, { side: THREE.DoubleSide }), std('#ffd23f', 0.7, 0, { side: THREE.DoubleSide })];
  const linePts = [];
  for (let k = 0; k < cells; k++) {
    const u = (k + 0.5) / cells - 0.5;
    const a = u * 1.25;
    const x = Math.sin(a) * span * 0.62, y = 6.2 + Math.cos(a) * 1.4 - 1.4 * 0.4;
    const cellW = span / cells * 1.02;
    const prof = [[1.25, 0], [0.8, 0.28], [-0.2, 0.34], [-1.25, 0.12], [-1.25, 0], [1.25, 0]];
    const g = extrude(prof.slice(0, 5), cellW, 0.02);
    const c = mesh(g, cols[k % 2], chute);
    c.rotation.set(0, Math.PI / 2, -a);
    c.position.set(x, y, 0);
    for (const z of [0.9, -0.9]) linePts.push(0, 1.45, 0, x, y, z);
  }
  const lines = new THREE.LineSegments(new THREE.BufferGeometry().setAttribute('position', new THREE.Float32BufferAttribute(linePts, 3)), new THREE.LineBasicMaterial({ color: '#2b2b2b' }));
  chute.add(lines);
  chute.visible = false;
  root.add(chute);

  let phase = 0;
  const set = (o, x = 0, y = 0, z = 0) => o.rotation.set(x, y, z);

  /* Pose for what you're doing: a walk or run cycle (knees lift, elbows
   * pump), the skydiver's spread, hanging under the chute, a crawl stroke. */
  function animate(dt, state, speed) {
    chute.visible = state === 'chute';
    set(body); set(spine); set(head);
    body.position.set(0, 0.94, 0);
    if (state === 'freefall') {
      phase += dt * 3;
      body.rotation.x = Math.PI / 2.15;
      body.position.y = 1.2;
      head.rotation.x = -0.7;
      set(armL.shoulder, -0.2, 0, -1.45 + Math.sin(phase) * 0.05); set(armL.elbow, 0, 0, -0.5);
      set(armR.shoulder, -0.2, 0, 1.45 - Math.sin(phase) * 0.05); set(armR.elbow, 0, 0, 0.5);
      set(legL.hip, 0.15, 0, -0.32); set(legL.knee, 0.9); set(legR.hip, 0.15, 0, 0.32); set(legR.knee, 0.9);
      return;
    }
    if (state === 'chute') {
      phase += dt * 1.5;
      set(armL.shoulder, 0, 0, -2.75); set(armL.elbow, 0, 0, -0.35);
      set(armR.shoulder, 0, 0, 2.75); set(armR.elbow, 0, 0, 0.35);
      set(legL.hip, -0.15 + Math.sin(phase) * 0.08, 0, -0.06); set(legL.knee, 0.25);
      set(legR.hip, -0.05 - Math.sin(phase) * 0.08, 0, 0.06); set(legR.knee, 0.3);
      return;
    }
    if (state === 'swim') {
      phase += dt * 5;
      body.rotation.x = Math.PI / 2.3;
      body.position.y = 0.55;
      head.rotation.x = -0.9;
      set(armL.shoulder, -Math.PI + Math.sin(phase) * 2.2, 0, -0.25); set(armL.elbow, -0.4 * Math.max(0, Math.cos(phase)));
      set(armR.shoulder, -Math.PI - Math.sin(phase) * 2.2, 0, 0.25); set(armR.elbow, -0.4 * Math.max(0, -Math.cos(phase)));
      set(legL.hip, Math.sin(phase * 2) * 0.35); set(legL.knee, 0.2);
      set(legR.hip, -Math.sin(phase * 2) * 0.35); set(legR.knee, 0.2);
      return;
    }
    if (state === 'air') {
      set(armL.shoulder, -0.6, 0, -0.9); set(armL.elbow, -0.6);
      set(armR.shoulder, -0.3, 0, 0.9); set(armR.elbow, -0.5);
      set(legL.hip, -0.7); set(legL.knee, 1.1); set(legR.hip, 0.25); set(legR.knee, 0.5);
      return;
    }
    // walking / running
    const run = Math.min(1, Math.max(0, (speed - 5.5) / 3));
    const amp = Math.min(1, speed / 5) * (0.45 + run * 0.4);
    phase += dt * (speed > 0.2 ? 3.2 + speed * 0.95 : 0);
    if (speed < 0.2) phase = phase % (Math.PI * 2) * 0.9;
    const s = Math.sin(phase), c = Math.cos(phase);
    const kneeK = 0.4 + run * 1.2;
    set(legL.hip, -s * amp); set(legL.knee, Math.max(0, c) * kneeK * Math.min(1, speed / 3) + 0.05);
    set(legR.hip, s * amp); set(legR.knee, Math.max(0, -c) * kneeK * Math.min(1, speed / 3) + 0.05);
    set(legL.foot, s * amp * 0.3); set(legR.foot, -s * amp * 0.3);
    set(armL.shoulder, s * amp * 0.9, 0, -0.07); set(armL.elbow, -0.25 - run * 1.1);
    set(armR.shoulder, -s * amp * 0.9, 0, 0.07); set(armR.elbow, -0.25 - run * 1.1);
    spine.rotation.x = 0.04 + run * 0.18;
    spine.rotation.y = s * amp * 0.15;
    body.position.y = 0.94 + Math.abs(c) * 0.035 * amp * (1 + run) - (speed > 0.2 ? 0.02 : 0);
    if (speed < 0.2) {
      // idle: a slow breath
      phase += dt;
      spine.rotation.x = 0.02 + Math.sin(performance.now() / 900) * 0.01;
    }
  }
  return { root, animate };
}

/* ----------------------------------------------------------------- blimp */

export function makeBlimp() {
  const root = new THREE.Group();
  const prof = [];
  for (let k = 0; k <= 32; k++) {
    const t = k / 32;                                     // nose (t=0) to tail (t=1)
    const r = 9.6 * Math.pow(Math.sin(Math.PI * Math.pow(t, 0.85)), 0.75) * (t > 0.7 ? 1 - (t - 0.7) * 0.6 : 1);
    prof.push(new THREE.Vector2(Math.max(0.01, r), 36 - t * 72));
  }
  const hullGeo = new THREE.LatheGeometry(prof, 40);
  hullGeo.rotateZ(-Math.PI / 2);                          // lathe axis to +x, nose forward
  const hull = mesh(hullGeo, std('#e3e7ec', 0.45, 0.25), root);
  hull.receiveShadow = false;
  const band = mesh(new THREE.CylinderGeometry(9.66, 9.66, 4.2, 40, 1, true), std('#1d3f8a', 0.5, 0.2, { side: THREE.DoubleSide }), root);
  band.rotation.z = Math.PI / 2;
  const word = (side) => {
    const tex = canvasTex(1024, 160, (ctx) => {
      ctx.fillStyle = '#ffd200';
      ctx.font = '900 120px "Arial Black", Impact, sans-serif';
      ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText('LA ROYALE', 512, 84);
    });
    const m = new THREE.Mesh(new THREE.PlaneGeometry(34, 5.3), new THREE.MeshBasicMaterial({ map: tex, transparent: true, toneMapped: false }));
    m.position.set(10, 0, side * 9.75);
    if (side < 0) m.rotation.y = Math.PI;
    return m;
  };
  root.add(word(1), word(-1));
  // tail fins: four swept trapezoids
  const finShape = [[0, 0], [9, 0], [10.5, 6.5], [4.5, 6.5]];
  const finMat = std('#1d3f8a', 0.5, 0.2);
  for (let k = 0; k < 4; k++) {
    const f = mesh(extrude(finShape, 0.5, 0.15), finMat, root);
    f.position.set(-35, 0, 0);
    f.rotation.x = (k * Math.PI) / 2;
    f.translateY(3.2);
  }
  // gondola with a strip of windows
  const gondola = new THREE.Group();
  gondola.position.set(5, -10.2, 0);
  root.add(gondola);
  mesh(extrude([[-5.5, -1.2], [5, -1.2], [6, -0.2], [5.5, 1.3], [-5.5, 1.3]], 3.4, 0.35), std('#2a2d31', 0.5, 0.3), gondola);
  const winGeo = [];
  for (let k = 0; k < 7; k++) for (const z of [-1.72, 1.72]) winGeo.push(at(slab(0.9, 0.7, 0.02, 0.15), -4 + k * 1.3, 0.45, z));
  mesh(merge(winGeo), std('#9fd3ff', 0.1, 0.5, { emissive: '#3a6fa0', emissiveIntensity: 0.6 }), gondola);
  const prop = new THREE.Group();
  prop.position.set(-6.2, 0, 0);
  for (let k = 0; k < 3; k++) mesh(at(new THREE.BoxGeometry(0.15, 1.8, 0.4), 0, 0.9, 0), std('#3a3a3a', 0.5, 0.5), prop).rotation.x = (k / 3) * Math.PI * 2;
  gondola.add(prop);
  mesh(at(new THREE.ConeGeometry(0.35, 0.8, 10), -6.1, 0, 0, 0, 0, Math.PI / 2), std('#d33', 0.5, 0.3), gondola);
  root.userData.prop = prop;
  return root;
}
