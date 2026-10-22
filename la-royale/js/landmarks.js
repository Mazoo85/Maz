/*
 * Landmarks: the places that make the map Los Angeles.
 *
 * Each one sits at its real position (projected from latitude/longitude,
 * nudged only where the squeezed map would otherwise stack two on top of
 * each other) and is built from simple shapes. A `pad` flattens the ground
 * under it before anything is built — Dodger Stadium's plateau in Chavez
 * Ravine, the Getty's hilltop, the airfield at LAX.
 *
 * Built through the toolkit in world.js (api.box, api.cyl, api.arch, …),
 * so the renderer and the collision both come from the same description.
 */

import { project, COAST, DISTRICTS, ROADS, ROAD_WIDTH, STYLES } from './geo.js';
import { register as registerMore } from './landmarks2.js';

const P = (lat, lon) => project(lat, lon);
const rotv = (lx, lz, a) => { const c = Math.cos(a), s = Math.sin(a); return [lx * c + lz * s, -lx * s + lz * c]; };
const yawOf = (dx, dz) => Math.atan2(-dz, dx);

/* Nearest point on the shoreline, with the direction along it and the
 * direction out to sea. */
function shoreFrame(x, z) {
  let best = null, bd = Infinity;
  for (let i = 0; i < COAST.length - 1; i++) {
    const [ax, az] = COAST[i], [bx, bz] = COAST[i + 1];
    const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
    const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
    const px = ax + dx * t, pz = az + dz * t;
    const d = Math.hypot(px - x, pz - z);
    if (d < bd) { const L = Math.sqrt(L2); bd = d; best = { px, pz, tx: dx / L, tz: dz / L, nx: -dz / L, nz: dx / L }; }
  }
  return best;
}

/* Downtown's towers sit on its rotated street grid: (i, j) is a block. */
const DTLA = DISTRICTS.find((d) => d.name === 'Downtown LA');
const DT_PITCH = STYLES.downtown.bx + STYLES.downtown.st;
const dt = (i, j) => { const [ox, oz] = rotv(i * DT_PITCH, j * DT_PITCH, DTLA.rot); return [DTLA.x + ox, DTLA.z + oz]; };

const L = [];
const def = (name, at, opts, build) => L.push({ name, at: Array.isArray(at[0]) ? at[0] : at.length === 2 && Math.abs(at[0]) < 90 ? P(at[0], at[1]) : at, ...opts, build });

/* ------------------------------------------------------------- the coast */

function pier(api, x, z, len, width, deckY, inland = 60) {
  const { box, cyl, ramp, gh, reserveRect } = api;
  const f = shoreFrame(x, z);
  const a = yawOf(f.nx, f.nz);
  const sx = f.px - f.nx * inland, sz = f.pz - f.nz * inland;
  const cx = sx + f.nx * len / 2, cz = sz + f.nz * len / 2;
  box(cx, deckY - 0.5, cz, len, 1, width, '#8a6f55', { ry: a, col: true });
  for (let s = 10; s < len; s += 18) {
    for (const side of [-1, 1]) {
      const px = sx + f.nx * s + f.tx * side * (width / 2 - 1), pz = sz + f.nz * s + f.tz * side * (width / 2 - 1);
      const g = Math.min(gh(px, pz), 0) - 8;
      cyl(px, (g + deckY - 1) / 2, pz, 0.9, deckY - 1 - g, 0.9, '#5a4a3a');
    }
  }
  for (const side of [-1, 1]) {
    const rx = cx + f.tx * side * (width / 2 - 0.2), rz = cz + f.tz * side * (width / 2 - 0.2);
    box(rx, deckY + 0.55, rz, len, 1.1, 0.25, '#f1ede4', { ry: a, col: true });
  }
  const g0 = gh(sx - f.nx * 40, sz - f.nz * 40);
  ramp(sx - f.nx * 40, sz - f.nz * 40, g0, sx, sz, deckY, width, '#8a6f55');
  reserveRect(cx - f.nx * 20, cz - f.nz * 20, len / 2 + 30, width / 2 + 4, a);
  return { f, a, sx, sz, deckY };
}

def('Santa Monica Pier', [34.0083, -118.4987], { poiR: 230 }, (api) => {
  const { box, cyl, text, group, reserveRect, gh } = api;
  const p = pier(api, ...P(34.0083, -118.4987), 330, 16, 7.5, 70);
  const { f, a, sx, sz } = p;
  const at = (s, side) => [sx + f.nx * s + f.tx * side, sz + f.nz * s + f.tz * side];
  // Pacific Park: the wide deck at the end, on the south side
  const [px, pz] = at(265, 30);
  box(px, 7, pz, 120, 1, 46, '#8a6f55', { ry: a, col: true });
  reserveRect(px, pz, 64, 26, a);
  // the solar-powered Pacific Wheel
  const [wx, wz] = at(290, 34);
  for (const side of [-1, 1]) {
    for (const lean of [-1, 1]) {
      const [ox, oz] = rotv(lean * 6, side * 3, a);
      box(wx + ox, 7.5 + 11, wz + oz, 1, 23, 1, '#dcdcdc', { ry: a, rz: lean * 0.28 });
    }
  }
  group('ferris', { x: wx, y: 7.5 + 21, z: wz, ry: a, anim: 'spinZ', speed: 0.14 });
  api.torus(0, 0, 0, 36, 36, 3, '#e8e8e8', { g: 'ferris' });
  api.torus(0, 0, 0, 30, 30, 2, '#ff5aa5', { g: 'ferris', m: 'glow' });
  cyl(0, 0, 0, 3, 4, 3, '#cfcfcf', { g: 'ferris', rx: Math.PI / 2 });
  for (let k = 0; k < 16; k++) {
    const ang = (k / 16) * Math.PI * 2;
    box(Math.cos(ang) * 8.5, Math.sin(ang) * 8.5, 0, 17, 0.4, 0.4, '#d0d0d0', { g: 'ferris', rz: ang });
  }
  const gondola = ['#ff4f6d', '#ffd23f', '#4fd1ff', '#7cff6b', '#ff8c42', '#b388ff'];
  for (let k = 0; k < 20; k++) {
    const ang = (k / 20) * Math.PI * 2;
    box(Math.cos(ang) * 18.5, Math.sin(ang) * 18.5, 0, 2.2, 2.2, 2.6, gondola[k % gondola.length], { g: 'ferris', m: 'glow' });
  }
  // the West Coaster
  const [cxc, czc] = at(235, 36);
  let prev = null;
  for (let k = 0; k <= 32; k++) {
    const t = (k / 32) * Math.PI * 2;
    const [ox, oz] = rotv(Math.cos(t) * 42, Math.sin(t) * 14, a);
    const y = 7.5 + 10 + 5 * Math.sin(t * 2) + 3 * Math.cos(t * 3);
    const pt = [cxc + ox, y, czc + oz];
    if (prev) {
      const dx = pt[0] - prev[0], dz = pt[2] - prev[2], dy = pt[1] - prev[1];
      const len = Math.hypot(dx, dz);
      box((pt[0] + prev[0]) / 2, (pt[1] + prev[1]) / 2, (pt[2] + prev[2]) / 2, len + 0.4, 0.5, 2.2, '#d63b3b', { ry: yawOf(dx, dz), rz: Math.atan2(dy, len) });
      if (k % 2 === 0) cyl(pt[0], (7.5 + pt[1]) / 2, pt[2], 0.5, pt[1] - 7.5, 0.5, '#e6e6e6');
    }
    prev = pt;
  }
  // the Looff Hippodrome carousel building and the entrance sign
  const [hx, hz] = at(30, -2);
  box(hx, 7.5 + 5, hz, 26, 9, 18, '#f3e3c3', { ry: a, col: true });
  api.roof(hx, 7.5 + 9.5, hz, 28, 5, 20, '#8b3a2e', { ry: a });
  const [gx, gz] = at(-36, 0);
  const g0 = gh(gx, gz);
  for (const side of [-1, 1]) {
    const [qx, qz] = at(-36, side * 9);
    box(qx, g0 + 5, qz, 1.2, 10, 1.2, '#1f4e8c');
  }
  box(gx, g0 + 10, gz, 1, 3.4, 20, '#1f4e8c', { ry: a });
  text(gx - f.nx * 0.6, g0 + 10, gz - f.nz * 0.6, 19, 3, 'SANTA MONICA', '#ffd84a', { ry: a - Math.PI / 2, glow: true });
  const [rx, rz] = at(250, -6);
  box(rx, 7.5 + 2, rz, 0.3, 4, 0.3, '#444');
  text(rx, 7.5 + 4, rz, 6, 2.4, 'ROUTE 66 · END OF THE TRAIL', '#ffffff', { ry: a + Math.PI / 2, bg: '#1d5aa8', font: 14 });
});

def('Malibu Pier', [[-2850, -270]], { poiR: 160 }, (api) => {
  const p = pier(api, -2850, -270, 230, 9, 6.5, 50);
  const [x, z] = [p.sx + p.f.nx * 222, p.sz + p.f.nz * 222];
  api.box(x, 6.5 + 3, z, 16, 6, 11, '#efe6d2', { ry: p.a, col: true });
  api.roof(x, 6.5 + 6, z, 17, 3, 12, '#2f6e5a', { ry: p.a });
  // Malibu Colony: beach houses on stilts along the sand
  const { building, box, rnd, reserveRect, gh } = api;
  for (let k = 0; k < 18; k++) {
    const f = shoreFrame(-2990 + k * 32 + (k > 8 ? 60 : 0), -280);
    const x2 = f.px - f.nx * 42, z2 = f.pz - f.nz * 42;
    if (Math.abs(x2 + 2850) < 25) continue;
    const a = yawOf(f.tx, f.tz);
    const y = Math.max(gh(x2, z2), 1) + 2.4;
    for (const [lx, lz] of [[-6, -5], [6, -5], [-6, 5], [6, 5]]) {
      const [ox, oz] = rotv(lx, lz, a);
      box(x2 + ox, y - 1.6, z2 + oz, 0.6, 3.6, 0.6, '#6b5a48');
    }
    building(x2, z2, rnd.range(14, 20), rnd.range(10, 13), rnd.range(5, 8), a, rnd.pick(['#f4efe6', '#d9e4ea', '#efe1c6', '#c9d8d0', '#ffffff']), 1, null, null, { lo: y, hi: y });
    reserveRect(x2, z2, 11, 8, a);
  }
});

def('Venice Beach', [[-1695, 560]], { poiR: 200 }, (api) => {
  const { box, cyl, text, paint, reserveRect, reserveDisc, gh, poi } = api;
  const f = shoreFrame(-1695, 560);
  const a = yawOf(f.tx, f.tz);
  const at = (s, inland) => [f.px + f.tx * s - f.nx * inland, f.pz + f.tz * s - f.nz * inland];
  // the boardwalk (Ocean Front Walk)
  const walkPts = [];
  for (let s = -520; s <= 330; s += 30) walkPts.push(at(s, 92));
  paint({ t: 'line', pts: walkPts, w: 7, c: '#c7bfb0' });
  // Muscle Beach
  const [mx, mz] = at(-40, 58);
  const y = gh(mx, mz);
  paint({ t: 'rect', x: mx, z: mz, w: 34, d: 22, rot: a, c: '#b9a98a' }, 1);
  for (const [lx, lz, w, d] of [[0, -11, 34, 0.2], [0, 11, 34, 0.2], [-17, 0, 0.2, 22], [17, 0, 0.2, 22]]) {
    const [ox, oz] = rotv(lx, lz, a);
    box(mx + ox, y + 1.1, mz + oz, w || 0.2, 2.2, d || 0.2, '#9aa0a6', { ry: a, col: true });
  }
  for (let k = 0; k < 6; k++) {
    const [ox, oz] = rotv(-12 + k * 5, -2 + (k % 2) * 4, a);
    for (const e of [-1.1, 1.1]) { const [qx, qz] = rotv(0, e, a); cyl(mx + ox + qx, y + 0.9, mz + oz + qz, 0.9, 0.25, 0.9, '#222', { rx: Math.PI / 2, ry: a }); }
    box(mx + ox, y + 0.9, mz + oz, 0.12, 0.12, 2.6, '#bbb', { ry: a });
    box(mx + ox, y + 0.2, mz + oz, 1.6, 0.4, 0.8, '#5a5a5a', { ry: a });
  }
  text(mx - f.nx * -11, y + 3.4, mz - f.nz * -11, 14, 2.2, 'MUSCLE BEACH', '#ffd23f', { ry: a, bg: '#1b1b1b' });
  poi('Muscle Beach', mx, mz, 45);
  // the skate park
  const [kx, kz] = at(110, 55);
  paint({ t: 'ellipse', x: kx, z: kz, rx: 26, rz: 16, rot: a, c: '#a8a6a2' }, 1);
  paint({ t: 'ellipse', x: kx - 8, z: kz, rx: 8, rz: 6, rot: a, c: '#7f7d7a' }, 1);
  paint({ t: 'ellipse', x: kx + 9, z: kz + 2, rx: 6, rz: 5, rot: a, c: '#7f7d7a' }, 1);
  const ky = gh(kx, kz);
  for (let k = 0; k < 5; k++) {
    const [ox, oz] = rotv(-16 + k * 8, -9, a);
    box(kx + ox, ky + 0.5, kz + oz, 6, 1, 2.5, '#b4b2ad', { ry: a, rx: 0.35 });
  }
  poi('Venice Skate Park', kx, kz, 40);
  reserveDisc(kx, kz, 28); reserveRect(mx, mz, 18, 12, a);
  // the VENICE sign hung over Windward Ave
  const [vx, vz] = at(-10, 150);
  const vy = gh(vx, vz);
  for (const side of [-1, 1]) { const [ox, oz] = rotv(side * 11, 0, a); cyl(vx + ox, vy + 6, vz + oz, 0.8, 12, 0.8, '#5c5c5c', { col: true }); }
  box(vx, vy + 11.6, vz, 22, 0.25, 0.25, '#444', { ry: a });
  text(vx, vy + 10, vz, 18, 3.6, 'VENICE', '#fff7e0', { ry: a, glow: true, bg: '#00000000', font: 72 });
  // footbridges over the canals
  for (const z0 of [560, 600, 640]) for (const x0 of [-1560, -1500]) box(x0, api.W.canalLevel + 1.2, z0, 3, 0.5, 12, '#f2efe8', { col: true });
  for (const x0 of [-1590, -1530, -1470]) for (const z0 of [580, 620]) box(x0, api.W.canalLevel + 1.2, z0, 12, 0.5, 3, '#f2efe8', { col: true });
  poi('Venice Canals', -1530, 600, 110);
});

def('Manhattan Beach Pier', [33.8845, -118.4113], { poiR: 150 }, (api) => {
  const p = pier(api, ...P(33.8845, -118.4113), 230, 10, 6.5, 40);
  const [x, z] = [p.sx + p.f.nx * 222, p.sz + p.f.nz * 222];
  api.cyl(x, 6.5 + 3.5, z, 18, 7, 18, '#f3efe6', { col: true });
  api.cone(x, 6.5 + 8.6, z, 21, 3.2, 21, '#2b6f8f');
});

def('Marina del Rey', [[-1450, 760]], { poiR: 200 }, (api) => {
  const { box, cyl, rnd } = api;
  for (let row = 0; row < 4; row++) {
    const z = 700 + row * 45;
    const x0 = -1580 + row * 30, x1 = -1310;
    box((x0 + x1) / 2, 0.5, z, x1 - x0, 0.6, 3, '#cfc8bb', { col: true });
    for (let x = x0 + 6; x < x1 - 4; x += 8) {
      for (const side of [-1, 1]) {
        if (!rnd.chance(0.75)) continue;
        const bz = z + side * 8;
        box(x, 0.6, bz, 3.2, 1.6, 10, '#f7f7f5', { col: true });
        box(x, 1.6, bz - side, 2.2, 1, 3.5, '#e3e7ea');
        if (rnd.chance(0.55)) cyl(x, 9, bz, 0.25, 15, 0.25, '#d9d9d9');
      }
    }
  }
});

/* ------------------------------------------------------------- LAX & co */

def('LAX', [[-700, 1312]], { pad: { r: 380, blend: 80, open: true }, poiR: 420 }, (api) => {
  const { box, cyl, sphere, dome, text, paint, building, reserveRect, gh, arch, rnd, W } = api;
  const y = gh(-700, 1312);
  reserveRect(-705, 1312, 360, 210, 0);
  paint({ t: 'rect', x: -705, z: 1312, w: 730, d: 420, rot: 0, c: '#8f918d' });
  for (const z of [1095, 1530]) paint({ t: 'rect', x: -705, z, w: 730, d: 40, rot: 0, c: '#8c9a5c' });
  for (const [z, w] of [[1123, 30], [1158, 30], [1468, 30], [1503, 30]]) {
    paint({ t: 'rect', x: -705, z, w: 700, d: w, rot: 0, c: '#3d3f42' }, 1);
    paint({ t: 'line', pts: [[-1040, z], [-370, z]], w: 1.2, c: '#f4f4f4', dash: [14, 12] }, 1);
    for (const end of [-1045, -365]) for (let k = -3; k <= 3; k++) paint({ t: 'rect', x: end, z: z + k * 3.6, w: 14, d: 1.4, rot: 0, c: '#f4f4f4' }, 1);
  }
  for (const z of [1180, 1440]) paint({ t: 'line', pts: [[-1040, z], [-380, z]], w: 1, c: '#e8c547' }, 1);
  // terminals: a horseshoe round the central loop, open to Century Blvd
  building(-745, 1252, 440, 24, 14, 0, '#d9d6cf', 2);
  building(-745, 1358, 440, 24, 14, 0, '#d9d6cf', 2);
  building(-975, 1305, 24, 130, 14, 0, '#d9d6cf', 2);
  for (const x of [-905, -800, -695, -590]) {
    building(x, 1226, 16, 30, 9, 0, '#cfccc4', 2);
    building(x, 1384, 16, 30, 9, 0, '#cfccc4', 2);
  }
  building(-870, 1305, 110, 46, 12, 0, '#a7a49c', 4);
  building(-615, 1305, 90, 46, 12, 0, '#a7a49c', 4);
  paint({ t: 'rect', x: -745, z: 1305, w: 480, d: 80, rot: 0, c: '#55585b' });
  paint({ t: 'circle', x: -745, z: 1305, r: 26, c: '#7d9a52' }, 1);
  // the Theme Building: a flying saucer on crossed parabolic arches
  arch(-745, y, 1305, 24, 1.6, 1.6, Math.PI / 4, '#f4f4f2', 11);
  arch(-745, y, 1305, 24, 1.6, 1.6, -Math.PI / 4, '#f4f4f2', 11);
  cyl(-745, y + 9, 1305, 6, 18, 6, '#f4f4f2', { col: true });
  cyl(-745, y + 18, 1305, 30, 3.5, 30, '#f4f4f2');
  dome(-745, y + 19.7, 1305, 28, 5, 28, '#eef3f6');
  box(-745, y + 18, 1305, 31, 0.4, 31, '#7fd3ff', { m: 'glow', ry: 0.4 });
  // control tower
  cyl(-655, y + 30, 1290, 7, 60, 7, '#e6e2d8', { col: true });
  cyl(-655, y + 63, 1290, 14, 7, 14, '#5aa0c8', { m: 'glow' });
  cyl(-655, y + 67.5, 1290, 15, 1.4, 15, '#e6e2d8');
  // the gateway pylons glowing along Century Blvd, and the giant letters
  const hues = ['#ff5f7e', '#ffb347', '#fff275', '#7cff9b', '#5ce1ff', '#9b8cff', '#ff7ae5'];
  for (let k = 0; k < 7; k++) for (const side of [-1, 1]) {
    cyl(-470 + k * 15, y + 15, 1304 + side * 22, 3.2, 30, 3.2, hues[k], { m: 'glow', col: true });
  }
  text(-400, y + 5, 1270, 34, 10, 'LAX', '#ffffff', { ry: Math.PI / 2 + 0.6, font: 90 });
  // parked jets at the gates
  const tails = ['#1f4e9c', '#c8102e', '#0f6b4b', '#f2a900', '#6a2c91', '#d95f02'];
  for (const [i, x] of [-930, -850, -760, -660, -560].entries()) {
    plane(api, x, 1200, -Math.PI / 2, tails[i % tails.length]);
    plane(api, x + 40, 1412, Math.PI / 2, tails[(i + 3) % tails.length]);
  }
  plane(api, -980, 1123, 0, '#1f4e9c');
  void sphere; void rnd; void W;
});

function plane(api, x, z, a, tail) {
  const { box, cyl, sphere, gh } = api;
  const y = gh(x, z) + 4.2;
  const at = (lx, lz) => { const [ox, oz] = rotv(lx, lz, a); return [x + ox, z + oz]; };
  cyl(x, y, z, 5.4, 44, 5.4, '#f2f4f5', { rz: Math.PI / 2, ry: a });
  let [nx, nz] = at(22, 0); sphere(nx, y, nz, 5.4, 5.4, 5.4, '#f2f4f5');
  [nx, nz] = at(-24, 0); api.cone(nx, y + 0.8, nz, 5, 8, 5, '#f2f4f5', { rz: Math.PI / 2, ry: a });
  box(x, y - 1.2, z, 9, 0.6, 46, '#e7e9ea', { ry: a });
  for (const side of [-1, 1]) {
    const [ex, ez] = at(2, side * 9);
    cyl(ex, y - 2.8, ez, 3, 6, 3, '#9aa0a6', { rz: Math.PI / 2, ry: a });
  }
  const [tx, tz] = at(-21, 0);
  box(tx, y + 6, tz, 8, 9, 0.7, tail, { ry: a, rz: 0.35 });
  box(tx, y + 1.5, tz, 5, 0.4, 16, '#e7e9ea', { ry: a });
  for (const lx of [10, -3]) { const [gx, gz] = at(lx, 0); box(gx, y - 3.3, gz, 1.2, 2.2, 3, '#333', { ry: a }); }
  api.W.addBox(x, z, 24, 3, a, y - 4, y + 3, 'plane');
}

def('In-N-Out Burger', [[-560, 1050]], { poiR: 50 }, (api) => {
  const { box, building, tree, text, gh, reserveRect, cyl } = api;
  const y = gh(-560, 1050);
  building(-560, 1050, 18, 12, 5, 0, '#f6f6f2', 1);
  box(-560, y + 5.2, 1050, 18.4, 1, 12.4, '#c8102e');
  cyl(-540, y + 6, 1038, 0.6, 12, 0.6, '#c8102e');
  box(-540, y + 12, 1038, 7, 3, 0.6, '#ffcc00');
  text(-540, y + 12, 1037.6, 6.5, 2.6, 'IN-N-OUT', '#c8102e', { font: 64, ry: Math.PI });
  text(-540, y + 12, 1038.4, 6.5, 2.6, 'IN-N-OUT', '#c8102e', { font: 64 });
  tree(-578, 1036, 0, 15); tree(-575, 1037, 0, 14);
  reserveRect(-560, 1050, 22, 18, 0);
});

def("Randy's Donuts", [33.9617, -118.3703], { poiR: 50 }, (api) => {
  const { box, building, torus, text, gh, reserveRect } = api;
  const [x, z] = P(33.9617, -118.3703);
  const y = gh(x, z);
  building(x, z, 14, 10, 4, 0, '#f6f3ea', 1);
  for (const lx of [-3, 3]) box(x + lx, y + 6, z, 0.6, 4, 0.6, '#555');
  torus(x, y + 15, z, 20, 20, 9, '#a8743f', { ry: Math.PI / 2 });
  text(x + 7.2, y + 2.6, z, 9, 1.6, "RANDY'S DONUTS", '#c43b2e', { ry: Math.PI / 2, bg: '#fff3d6' });
  reserveRect(x, z, 12, 10, 0);
});

def('SoFi Stadium', [[200, 1170]], { pad: { r: 190, blend: 70 }, poiR: 200 }, (api) => {
  const { box, cyl, paint, gh } = api;
  const [x, z] = [200, 1170];
  const y = gh(x, z);
  paint({ t: 'ellipse', x, z, rx: 175, rz: 145, rot: 0, c: '#8a8c8e' });
  paint({ t: 'rect', x, z, w: 110, d: 50, rot: 0.2, c: '#3f8f3f' }, 1);
  paint({ t: 'rect', x, z, w: 0.8, d: 50, rot: 0.2, c: '#f4f4f4' }, 1);
  bowl(api, x, z, y, 82, 64, 30, 18, 0.2, ['#1f3a6b', '#2b5aa8', '#c9d6e8'], 4);
  // the floating translucent canopy, on a ring of columns
  cyl(x, y + 30, z, 288, 2, 228, '#e9f1f6', { m: 'glass', col: true });
  for (let k = 0; k < 18; k++) {
    const t = (k / 18) * Math.PI * 2;
    cyl(x + Math.cos(t) * 135, y + 15, z + Math.sin(t) * 104, 2.2, 30, 2.2, '#cfd6dc', { col: true });
  }
});

def('The Forum', [[200, 960]], { pad: { r: 80, blend: 40 }, poiR: 90 }, (api) => {
  const { cyl, dome, gh } = api;
  const y = gh(200, 960);
  cyl(200, y + 9, 960, 104, 18, 104, '#c4553f', { col: true });
  for (let k = 0; k < 40; k++) {
    const t = (k / 40) * Math.PI * 2;
    cyl(200 + Math.cos(t) * 55, y + 10, 960 + Math.sin(t) * 55, 2, 20, 2, '#f4efe6');
  }
  cyl(200, y + 20.4, 960, 114, 1.4, 114, '#f4efe6');
  dome(200, y + 21, 960, 96, 10, 96, '#d9d4c8');
});

def('Intuit Dome', [[275, 1370]], { pad: { r: 80, blend: 40 }, poiR: 80 }, (api) => {
  const { cyl, box, gh } = api;
  const y = gh(275, 1370);
  cyl(275, y + 12, 1370, 110, 24, 96, '#e9eef2', { col: true });
  box(275, y + 25, 1370, 100, 2, 80, '#1d4ed8', { ry: 0.3, rz: 0.06, m: 'glass' });
});

def('SpaceX Hawthorne', [[260, 1748]], { pad: { r: 140, blend: 60 }, poiR: 160 }, (api) => {
  const { building, cyl, box, gh, paint } = api;
  building(260, 1748, 200, 100, 14, 0, '#f4f6f7', 4);
  const [x, z] = [310, 1832];
  const y = gh(x, z);
  paint({ t: 'circle', x, z, r: 14, c: '#6f7378' }, 1);
  cyl(x, y + 23.5 + 3, z, 4.6, 47, 4.6, '#f7f7f7', { col: true });
  cyl(x, y + 47 + 3, z, 4.7, 5, 4.7, '#1d1d1d');
  for (let k = 0; k < 4; k++) {
    const t = (k / 4) * Math.PI * 2 + Math.PI / 4;
    box(x + Math.cos(t) * 4, y + 3.5, z + Math.sin(t) * 4, 0.8, 8, 0.8, '#222', { ry: -t, rz: 0.45 });
    box(x + Math.cos(t) * 2.6, y + 51, z + Math.sin(t) * 2.6, 2, 0.3, 1.6, '#333', { ry: -t });
  }
});

def('El Segundo Refinery', [[-830, 1910]], { pad: { r: 70, blend: 40 }, poiR: 220 }, (api) => {
  const { cyl, cone, box, gh } = api;
  for (let k = 0; k < 7; k++) {
    const x = -860 + (k % 4) * 18, z = 1890 + Math.floor(k / 4) * 22;
    const h = 30 + (k * 7) % 22;
    const y = gh(x, z);
    cyl(x, y + h / 2, z, 5, h, 5, '#b9b6ae', { col: true });
    for (let r = 6; r < h; r += 8) cyl(x, y + r, z, 7, 0.6, 7, '#8c8a85');
  }
  const y = gh(-790, 1940);
  cyl(-790, y + 30, 1940, 1.6, 60, 1.6, '#8c8a85', { col: true });
  cone(-790, y + 62, 1940, 3, 5, 3, '#ff8a2a', { m: 'glow' });
  box(-830, y + 4, 1910, 60, 1, 1, '#777', { ry: 0.5 });
});

def('Goodyear Blimp Base', [[900, 2520]], { pad: { r: 120, blend: 50 }, poiR: 130 }, (api) => {
  const { cyl, sphere, box, cone, paint, gh, text } = api;
  const y = gh(900, 2520);
  paint({ t: 'circle', x: 900, z: 2520, r: 110, c: '#88a35a' });
  cyl(900, y + 9, 2520, 1.4, 18, 1.4, '#d33', { col: true });
  sphere(900, y + 13, 2565, 16, 15, 64, '#d8dde3', { ry: 0 });
  box(900, y + 13, 2565, 0.6, 3.4, 40, '#1d3f8a');
  box(900, y + 4.6, 2560, 3, 2.4, 9, '#2a2a2a');
  for (const s of [-1, 1]) box(900 + s * 5, y + 13, 2595, 9, 0.5, 6, '#1d3f8a', { rz: s * 0.2 });
  box(900, y + 19, 2595, 0.5, 8, 6, '#1d3f8a');
  text(908.1, y + 13, 2565, 30, 5, 'GOODYEAR', '#ffd200', { ry: Math.PI / 2, font: 80 });
  void cone;
});

/* -------------------------------------------------------------- Hollywood */

def('Hollywood Sign', [[490, -1845]], { poiR: 230 }, (api) => {
  const { text, box, gh, reserveRect } = api;
  const letters = 'HOLLYWOOD';
  for (let i = 0; i < letters.length; i++) {
    const x = 490 - 84 + i * 21, z = -1845 + (i - 4) * 1.8;
    const y = gh(x, z);
    text(x, y + 12, z, 18, 22, letters[i], '#fbfbf8', { ry: 0.08, font: 200, solid: true });
    for (const lx of [-6, 0, 6]) box(x + lx, y + 9, z - 1.4, 0.4, 18, 0.4, '#7a7a7a', { rx: -0.25 });
  }
  reserveRect(490, -1845, 100, 20, 0);
  // the Mt. Lee antenna mast behind the sign
  const y = gh(500, -2010);
  box(500, y + 20, -2010, 2, 40, 2, '#c9c9c9', { col: true });
  box(500, y + 41, -2010, 0.4, 6, 0.4, '#ff3b3b', { m: 'glow' });
});

def('Griffith Observatory', [34.1184, -118.3004], { pad: { r: 75, blend: 55 }, poiR: 170 }, (api) => {
  const { building, cyl, dome, box, cone, paint, gh } = api;
  const [x, z] = P(34.1184, -118.3004);
  const y = gh(x, z);
  paint({ t: 'rect', x, z: z + 40, w: 70, d: 40, rot: 0, c: '#7f9c55' }, 1);
  paint({ t: 'rect', x, z: z + 62, w: 120, d: 14, rot: 0, c: '#9b9890' }, 1);
  building(x, z, 62, 22, 12, 0, '#f1ede4', 0);
  cyl(x, y + 13.5, z, 26, 4, 26, '#f1ede4');
  dome(x, y + 15.5, z, 26, 12, 26, '#7d8b85');
  for (const s of [-1, 1]) {
    cyl(x + s * 28, y + 12.5, z, 12, 3, 12, '#f1ede4');
    dome(x + s * 28, y + 14, z, 12, 6, 12, '#7d8b85');
  }
  box(x, y + 6, z + 38, 2.2, 12, 2.2, '#e9e4d6', { col: true });
  cone(x, y + 13.5, z + 38, 2.8, 3, 2.8, '#e9e4d6');
});

def('Hollywood Bowl', [34.1122, -118.3391], { pad: { r: 50, blend: 30 }, poiR: 120 }, (api) => {
  const { arch, box, gh } = api;
  const [x, z] = P(34.1122, -118.3391);
  const y = gh(x, z);
  box(x, y + 0.6, z - 2, 34, 1.2, 14, '#e8e2d5', { col: true });
  for (let k = 0; k < 6; k++) arch(x, y + 1, z + 4 - k * 2.6, 9 + k * 2.4, 1.2, 2.2, 0, '#f6f4ee', 11);
  for (let r = 0; r < 12; r++) {
    const rz = z - 16 - r * 5;
    const ry = gh(x, rz);
    box(x, ry + 0.3, rz, 60 + r * 4, 0.6, 3.5, '#9c9a94');
  }
});

def('Capitol Records', [[520, -1510]], { poiR: 60 }, (api) => {
  const { cyl, sphere, gh, reserveDisc, W } = api;
  const [x, z] = [520, -1510];
  const y = gh(x, z);
  for (let f = 0; f < 13; f++) {
    cyl(x, y + f * 3.3 + 1.6, z, 30 - f * 0.25, 3.2, 30 - f * 0.25, '#3c4650');
    cyl(x, y + f * 3.3 + 3.2, z, 34 - f * 0.25, 0.4, 34 - f * 0.25, '#f2efe8');
  }
  W.addCircle(x, z, 16, y, y + 43, 'building');
  cyl(x, y + 46, z, 2.6, 6, 2.6, '#f2efe8');
  cyl(x, y + 62, z, 0.5, 30, 0.5, '#d9d9d9');
  sphere(x, y + 77.5, z, 1.4, 1.4, 1.4, '#ff2b2b', { m: 'glow' });
  reserveDisc(x, z, 22);
});

def('Pantages Theatre', [[560, -1411]], { poiR: 45 }, (api) => {
  const { building, box, text, gh } = api;
  const y = gh(560, -1411);
  building(560, -1411, 40, 30, 17, 0, '#e8dcc0', 2);
  box(560, y + 24, -1395.5, 4, 14, 1, '#1a1a1a');
  text(560, y + 24, -1394.9, 3.4, 13, 'P\nA\nN\nT\nA\nG\nE\nS', '#ffe27a', { glow: true, font: 40 });
});

def('TCL Chinese Theatre', [[203, -1476]], { poiR: 80 }, (api) => {
  const { building, cyl, roof, box, paint, gh } = api;
  const [x, z] = [203, -1476];
  const y = gh(x, z);
  paint({ t: 'rect', x, z: z + 26, w: 46, d: 22, rot: 0, c: '#b7a07a' }, 1);
  building(x, z - 6, 46, 30, 15, 0, '#8f2f25', 0);
  for (const s of [-1, 1]) cyl(x + s * 9, y + 8, z + 12, 3, 16, 3, '#b8352a', { col: true });
  roof(x, y + 16, z + 9, 34, 9, 14, '#2e6b5e');
  box(x, y + 15.6, z + 9, 36, 0.8, 16, '#7a2219');
  roof(x, y + 15, z - 6, 48, 6, 32, '#245447');
  building(x + 64, z - 4, 60, 40, 22, 0, '#efe9df', 2);
});

def('Walk of Fame', [34.1016, -118.334], { poiR: 220 }, (api) => {
  const road = ROADS.find((r) => r.name === 'Hollywood Blvd');
  const pts = road.pts.filter(([x]) => x > 100 && x < 600);
  for (const side of [-1, 1]) {
    const off = [];
    for (let i = 0; i < pts.length; i++) {
      const a = pts[Math.max(0, i - 1)], b = pts[Math.min(pts.length - 1, i + 1)];
      const L2 = Math.hypot(b[0] - a[0], b[1] - a[1]);
      off.push([pts[i][0] - ((b[1] - a[1]) / L2) * 11 * side, pts[i][1] + ((b[0] - a[0]) / L2) * 11 * side]);
    }
    api.paint({ t: 'line', pts: off, w: 3, c: '#d79aa9', dash: [2, 2] }, 1);
  }
});

def('Chateau Marmont', [34.0981, -118.3685], { poiR: 50 }, (api) => {
  const { building, roof, gh } = api;
  const [x] = P(34.0981, -118.3685);
  const zz = -1340;                  // just north of Sunset, as on the real Strip
  const y = gh(x, zz);
  building(x, zz, 32, 20, 22, 0, '#efe9dc', 1);
  for (const s of [-1, 1]) {
    building(x + s * 14, zz + 6, 8, 8, 30, 0, '#efe9dc', 1);
    roof(x + s * 14, y + 30, zz + 6, 9, 6, 9, '#3b5d4a');
  }
  roof(x, y + 22, zz, 30, 5, 18, '#3b5d4a');
});

def('Sunset Strip', [34.0905, -118.385], { poiR: 260 }, (api) => {
  const { billboard, rnd, building, text, gh, reserveRect } = api;
  const road = ROADS.find((r) => r.name === 'Sunset Blvd');
  const seg = road.pts.filter(([x]) => x > -600 && x < -200);
  let side = 1;
  for (let i = 0; i < seg.length - 1; i++) {
    const [ax, az] = seg[i], [bx, bz] = seg[i + 1];
    const L2 = Math.hypot(bx - ax, bz - az);
    for (let s = 20; s < L2; s += 45) {
      const x = ax + (bx - ax) * s / L2, z = az + (bz - az) * s / L2;
      const nx = -(bz - az) / L2, nz = (bx - ax) / L2;
      billboard(x + nx * 18 * side, z + nz * 18 * side, yawOf(bx - ax, bz - az), rnd.pick(['#ff4f8b', '#4fd1ff', '#ffd23f', '#b388ff', '#ff8c42']));
      api.reserveDisc(x + nx * 18 * side, z + nz * 18 * side, 3);
      side = -side;
    }
  }
  // the Whisky a Go Go
  const [wx, wz] = P(34.0907, -118.3856);
  const y = gh(wx, wz - 24);
  building(wx, wz - 24, 20, 16, 7, 0, '#151515', 0);
  text(wx, y + 8.4, wz - 15.6, 16, 3, 'WHISKY', '#ff2d2d', { glow: true, font: 80 });
  reserveRect(wx, wz - 24, 12, 10, 0);
  // Tail o' the Pup: a hot dog stand shaped like a hot dog
  const [hx, hz] = [-385, -1200];
  const hy = gh(hx, hz);
  building(hx, hz, 8, 5, 3, 0, '#f5f0e6', 0);
  api.cyl(hx, hy + 4.6, hz, 3.6, 10, 3.6, '#e2b46b', { rz: Math.PI / 2 });
  api.cyl(hx, hy + 6, hz, 2.2, 12, 2.2, '#a5442f', { rz: Math.PI / 2 });
  api.poi("Tail o' the Pup", hx, hz, 30);
  reserveRect(hx, hz, 8, 6, 0);
});

def('Universal Studios', [[60, -2170]], { pad: { r: 170, blend: 80 }, poiR: 200 }, (api) => {
  const { cyl, sphere, torus, building, box, paint, gh, rnd } = api;
  const [x, z] = [60, -2170];
  const y = gh(x, z);
  paint({ t: 'circle', x: x - 60, z: z + 90, r: 22, c: '#7fb7d9' }, 1);
  cyl(x - 60, y + 2, z + 90, 10, 4, 10, '#dcdcdc', { col: true });
  sphere(x - 60, y + 14, z + 90, 18, 18, 18, '#2f66b3');
  torus(x - 60, y + 14, z + 90, 30, 30, 6, '#d9d9d9', { rx: 1.2, rz: 0.3 });
  for (let k = 0; k < 6; k++) {
    building(x + (k % 3) * 70 - 40, z - 40 - Math.floor(k / 3) * 60, 56, 40, 20, 0, '#dccfb4', 4);
  }
  for (let k = 0; k < 7; k++) {
    const bx = x - 120 + k * 22, bz = z + 40;
    building(bx, bz, 16, 14, rnd.range(9, 16), 0, rnd.pick(['#f2c14e', '#e84a5f', '#48c9b0', '#5dade2']), 2);
    box(bx, gh(bx, bz) + 18, bz + 7.2, 12, 4, 0.4, rnd.pick(['#ff4f8b', '#4fd1ff', '#ffd23f']), { m: 'glow' });
  }
});

def('Warner Bros. Studios', [[330, -2580]], { pad: { r: 110, blend: 50 }, poiR: 140 }, (api) => {
  const { cyl, cone, box, building, gh } = api;
  const [x, z] = [330, -2580];
  const y = gh(x, z);
  for (let k = 0; k < 6; k++) {
    const t = (k / 6) * Math.PI * 2;
    box(x + Math.cos(t) * 5.5, y + 11, z + Math.sin(t) * 5.5, 0.7, 22, 0.7, '#7e8a96');
  }
  cyl(x, y + 26, z, 15, 9, 15, '#2d4f8a', { col: true });
  cone(x, y + 33, z, 16, 5, 16, '#3a5f9a');
  for (let k = 0; k < 4; k++) building(x + 50 + (k % 2) * 60, z - 40 + Math.floor(k / 2) * 70, 50, 40, 18, 0, '#e3d8c3', 4);
});

/* ---------------------------------------------------- the Westside & hills */

def('Getty Center', [[-1737, -1019]], { pad: { r: 120, blend: 70, dy: 4 }, poiR: 180 }, (api) => {
  const { building, cyl, paint, gh, box } = api;
  const [x, z] = [-1737, -1019];
  const y = gh(x, z);
  paint({ t: 'circle', x: x + 30, z: z + 45, r: 26, c: '#6f9a4f' }, 1);
  paint({ t: 'circle', x: x + 30, z: z + 45, r: 12, c: '#c47a9b' }, 1);
  for (const [lx, lz, w, d, h] of [[-40, -20, 60, 30, 14], [20, -40, 40, 36, 18], [-50, 30, 34, 40, 12], [50, 0, 30, 50, 15], [0, 20, 30, 20, 10]]) {
    building(x + lx, z + lz, w, d, h, 0.1, '#ece6da', 2);
  }
  cyl(x - 5, y + 9, z - 5, 34, 18, 34, '#f1ece2', { col: true });
  paint({ t: 'line', pts: [[x - 10, z - 60], [x + 80, z - 260], [x + 200, z - 420]], w: 3, c: '#b8b3a8' }, 1);
  box(x + 60, gh(x + 60, z - 215) + 2, z - 215, 3, 3, 12, '#f4f4f4', { ry: 0.4 });
});

def('UCLA', [34.0729, -118.4422], { pad: { r: 100, blend: 40 }, poiR: 190 }, (api) => {
  const { building, cyl, dome, cone, paint, gh, tree } = api;
  const [x, z] = P(34.0729, -118.4422);
  const y = gh(x, z);
  paint({ t: 'rect', x, z: z + 45, w: 60, d: 50, rot: 0, c: '#6f9a4f' }, 1);
  building(x, z, 70, 22, 18, 0, '#a5543d', 2, 'hip', '#b5562f');
  for (const s of [-1, 1]) {
    building(x + s * 14, z + 12, 9, 9, 34, 0, '#a5543d', 2);
    cone(x + s * 14, y + 37, z + 12, 9, 6, 9, '#b5562f');
  }
  building(x, z + 90, 60, 26, 16, 0, '#a5543d', 2, 'hip', '#b5562f');
  cyl(x, y + 19, z + 90, 14, 6, 14, '#a5543d');
  dome(x, y + 22, z + 90, 13, 8, 13, '#b5562f');
  for (let k = 0; k < 6; k++) tree(x - 25 + k * 10, z + 45 + (k % 2 ? 15 : -15), 1, 11);
});

def('Beverly Hills Hotel', [[-854, -1144]], { pad: { r: 60, blend: 30 }, poiR: 70 }, (api) => {
  const { building, text, tree, gh } = api;
  const [x, z] = [-854, -1144];
  const y = gh(x, z);
  building(x, z, 80, 20, 14, 0, '#f2b8c6', 1, 'hip', '#4f7a5a');
  building(x, z + 4, 16, 16, 24, 0, '#f2b8c6', 1, 'hip', '#4f7a5a');
  text(x, y + 19, z + 12.2, 15, 2.4, 'The Beverly Hills Hotel', '#2f7d4a', { glow: true, font: 46 });
  for (let k = 0; k < 4; k++) building(x - 30 + k * 20, z - 28, 12, 9, 5, 0, '#f2b8c6', 1, 'hip', '#4f7a5a');
  for (let k = 0; k < 8; k++) tree(x - 42 + k * 12, z + 22, 0, 22);
});

def('Rodeo Drive', [[-703, -850]], { poiR: 110 }, (api) => {
  const { building, box, tree, rnd, gh, reserveRect } = api;
  const a = Math.PI / 2;
  for (let k = -1; k <= 1; k++) {
    for (const side of [-1, 1]) {
      const [ox, oz] = rotv(k * 13, side * 15, a);
      const x = -703 + ox, z = -850 + oz;
      const h = rnd.range(7, 13);
      building(x, z, 12, 13, h, a, rnd.pick(['#f7f3ea', '#e9e4da', '#dcd2c2', '#222426']), 2);
      const [fx, fz] = rotv(0, -side * 7.6, a);
      box(x + fx, gh(x, z) + 3.6, z + fz, 12, 0.3, 2.4, rnd.pick(['#1b1b1b', '#e3c78d', '#7a1f2b', '#f4f4f4']), { ry: a, rx: side * 0.25 });
      const [tx, tz] = rotv(0, -side * 9.5, a); tree(x + tx, z + tz, 0, 20);
    }
  }
  reserveRect(-703, -850, 22, 24, a);
  building(-690, -786, 60, 30, 44, 0, '#e7dcc4', 2, 'hip', '#5a5f66');
  api.poi('Beverly Wilshire', -690, -786, 40);
});

def('Beverly Hills Sign', [[-635, -968]], { poiR: 50 }, (api) => {
  const { box, text, paint, gh, reserveRect } = api;
  const y = gh(-635, -968);
  paint({ t: 'rect', x: -635, z: -980, w: 36, d: 12, rot: 0.38, c: '#5b8fa8' }, 1);
  box(-635, y + 1, -968, 34, 2, 1.6, '#f4f1ea', { ry: 0.38, col: true });
  text(-635 + Math.sin(0.38) * 0.9, y + 1.2, -968 + Math.cos(0.38) * 0.9, 30, 1.6, 'BEVERLY HILLS', '#1f4d3a', { ry: 0.38, font: 60 });
  reserveRect(-635, -972, 20, 10, 0.38);
});

def('Greystone Mansion', [34.0920, -118.4013], { pad: { r: 45, blend: 30 }, poiR: 60 }, (api) => {
  const [x, z] = P(34.0920, -118.4013);
  api.building(x, z, 50, 28, 16, 0.2, '#8a8378', 1, 'hip', '#4a4f55');
});

def('Century City', [[-880, -640]], { poiR: 160 }, (api) => {
  const { prism, gh, W, reserveDisc, building, text } = api;
  for (const [x, z, a] of [[-905, -628, 0.4], [-840, -660, 0.4 + Math.PI]]) {
    const y = gh(x, z);
    prism(x, y + 87, z, 46, 174, 46, '#aeb6bb', { ry: a, m: 'win' });
    W.addCircle(x, z, 18, y, y + 174, 'building');
    reserveDisc(x, z, 30);
  }
  // Fox Plaza — Nakatomi Plaza in Die Hard
  building(-760, -640, 44, 34, 150, 0.6, '#8b6f5a', 3);
  reserveDisc(-760, -640, 32);
  api.poi('Fox Plaza ("Nakatomi Plaza")', -760, -640, 45);
  void text;
});

def('Sony Pictures Studios', [[-720, 120]], { poiR: 120 }, (api) => {
  const { arch, building, gh } = api;
  const y = gh(-720, 120);
  const colors = ['#e53935', '#fb8c00', '#fdd835', '#43a047', '#1e88e5', '#8e24aa'];
  colors.forEach((c, k) => arch(-720, y, 120, 24 - k * 1.2, 1.2, 2, 0, c, 15));
  for (let k = 0; k < 4; k++) building(-760 + k * 34, 165, 28, 34, 16, 0, '#e6dcc8', 4);
});

/* ------------------------------------------------------------ Mid-Wilshire */

def('Urban Light (LACMA)', [[-63, -782]], { poiR: 100 }, (api) => {
  const { cyl, sphere, building, box, gh, reserveRect, rnd } = api;
  const y = gh(-63, -782);
  for (let i = 0; i < 17; i++) for (let j = 0; j < 12; j++) {
    if (i * 12 + j >= 202) continue;
    const x = -78 + i * 1.85, z = -793 + j * 1.85;
    const h = 6 + ((i * 7 + j * 3) % 4) * 0.8;
    cyl(x, y + h / 2, z, 0.28, h, 0.28, '#8a8e91');
    sphere(x, y + h + 0.3, z, 0.6, 0.9, 0.6, '#fff1c9', { m: 'glow' });
  }
  api.W.addBox(-63, -782, 16, 11, 0, y, y + 7, 'lamps');
  reserveRect(-63, -782, 18, 13, 0);
  building(-135, -815, 70, 40, 14, 0, '#c8553d', 4);
  building(-10, -862, 46, 40, 22, 0, '#d8d2c6', 2);
  // the Geffen Galleries, bridging Wilshire
  const wil = (x) => -748 + x * 0.0712;          // Wilshire's centre line here
  box(-60, y + 13, wil(-60), 120, 7, 34, '#a9a49b', { col: true, ry: -0.071 });
  for (const x of [-110, -10]) for (const lz of [-18, 18]) cyl(x, y + 5, wil(x) + lz, 3, 10, 3, '#a9a49b', { col: true });
  void rnd;
});

def('La Brea Tar Pits', [34.0656, -118.3554], { poiR: 60 }, (api) => {
  const [x, z] = P(34.0656, -118.3554);
  mammoth(api, x - 4, z + 2, 0.4, 1, true);
  mammoth(api, x + 26, z - 14, -0.6, 1, false);
  mammoth(api, x + 32, z - 9, -0.4, 0.6, false);
});

function mammoth(api, x, z, a, s, sinking) {
  const { box, cyl, sphere, gh } = api;
  const y = sinking ? api.W.lakes.find((l) => l.tar).level - 0.8 : gh(x, z);
  const at = (lx, lz) => { const [ox, oz] = rotv(lx * s, lz * s, a); return [x + ox, z + oz]; };
  const fur = '#5b4636';
  box(x, y + 3.2 * s, z, 5.5 * s, 3.2 * s, 2.8 * s, fur, { ry: a });
  for (const [lx, lz] of [[1.8, 0.9], [1.8, -0.9], [-1.8, 0.9], [-1.8, -0.9]]) {
    const [px, pz] = at(lx, lz);
    cyl(px, y + 1 * s, pz, 1.1 * s, 2.4 * s, 1.1 * s, fur);
  }
  let [hx, hz] = at(3.3, 0);
  sphere(hx, y + 4.3 * s, hz, 2.4 * s, 2.6 * s, 2.2 * s, fur);
  [hx, hz] = at(4.4, 0);
  cyl(hx, y + 2.6 * s, hz, 0.7 * s, 3.2 * s, 0.7 * s, fur, { ry: a, rz: -0.2 });
  for (const side of [-1, 1]) {
    const [tx, tz] = at(4.4, side * 0.8);
    box(tx, y + 3 * s, tz, 2.6 * s, 0.35 * s, 0.35 * s, '#efe6d0', { ry: a, rz: 0.6 });
  }
}

def('The Grove', [[-29, -915]], { poiR: 80 }, (api) => {
  const { building, roof, box, gh, rnd } = api;
  const y = gh(-29, -915);
  building(-29, -915, 6, 6, 26, 0, '#f4efe6', 1);
  roof(-29, y + 26, -915, 7, 5, 7, '#2e6b5e');
  for (let k = 0; k < 8; k++) {
    building(-80 + k * 15, -950, 13, 18, rnd.range(8, 14), 0, rnd.pick(['#efe2c8', '#e9d1b0', '#f4ead5', '#dfc9a6']), 2);
    building(-80 + k * 15, -880, 13, 18, rnd.range(8, 14), 0, rnd.pick(['#efe2c8', '#e9d1b0', '#f4ead5', '#dfc9a6']), 2);
  }
  void box;
});

def('Petersen Museum', [[-91, -716]], { poiR: 50 }, (api) => {
  const { building, box, gh } = api;
  const y = gh(-91, -716);
  building(-91, -716, 56, 36, 18, 0, '#c8102e', 0);
  for (let k = 0; k < 5; k++) box(-91, y + 3 + k * 3.4, -716, 62, 0.8, 2.2, '#d9dde0', { m: 'metal', rx: 0.2 * (k % 2 ? 1 : -1), ry: 0.1 * k });
});

def('Echo Park Lake', [34.0726, -118.2606], { poiR: 80 }, (api) => {
  const { cyl, box, rnd } = api;
  const lake = api.W.lakes.find((l) => l.name === 'Echo Park Lake');
  cyl(lake.x, lake.level + 9, lake.z, 1.2, 18, 1.2, '#eaf6ff', { m: 'glow' });
  for (let k = 0; k < 8; k++) {
    const t = rnd() * 6.28, r = 12 + rnd() * 26;
    box(lake.x + Math.cos(t) * r, lake.level + 0.5, lake.z + Math.sin(t) * r, 2.4, 1, 1.6, '#ffffff', { ry: t });
  }
});

def('MacArthur Park', [34.0575, -118.2775], { poiR: 60 }, () => {});
def('Silver Lake Reservoir', [34.0928, -118.2701], { poiR: 90 }, () => {});
def('Lake Hollywood', [34.1255, -118.3314], { poiR: 80 }, () => {});
def('Mulholland Overlook', [[60, -1985]], { poiR: 60 }, () => {});
def('Runyon Canyon', [34.110, -118.350], { poiR: 120 }, () => {});
def('Third Street Promenade', [[-2010, 40]], { poiR: 80 }, () => {});
def('Will Rogers State Beach', [[-2560, -200]], { poiR: 150 }, () => {});
def('Surfrider Beach', [[-2700, -255]], { poiR: 120 }, () => {});
def('Baldwin Hills Overlook', [[-180, 160]], { poiR: 120 }, () => {});
def('Four Level Interchange', [[1570, -748]], { poiR: 70 }, () => {});
def('Little Tokyo', dt(3, 2), { poiR: 80 }, () => {});

/* ---------------------------------------------------------------- Downtown */

/* Downtown's skyline. Real towers, each given the nearest block (on
 * Downtown's rotated grid) that keeps it clear of the 110 and the
 * boulevards: the squeezed map can't fit them all at their exact spots. */
const roadClear = (x, z, r) => ROADS.every((rd) => {
  const hw = ROAD_WIDTH[rd.kind] / 2;
  for (let i = 0; i < rd.pts.length - 1; i++) {
    const [ax, az] = rd.pts[i], [bx, bz] = rd.pts[i + 1];
    const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
    const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
    if (Math.hypot(ax + dx * t - x, az + dz * t - z) < hw + r + 3) return false;
  }
  return true;
});

def('Downtown Towers', dt(0, 0), { poiR: 0 }, (api) => {
  const { building, cyl, prism, box, roof, text, gh, reserveDisc, poi, W, paint } = api;
  const a = DTLA.rot;
  const blocks = [];
  for (let r = 0; r <= 4; r++) for (let i = -r; i <= r; i++) for (let j = -r; j <= r; j++) if (Math.max(Math.abs(i), Math.abs(j)) === r) blocks.push([i, j]);
  const used = new Set();
  const place = (radius) => {
    for (const [i, j] of blocks) {
      const k = `${i},${j}`;
      if (used.has(k)) continue;
      const [x, z] = dt(i, j);
      if (!roadClear(x, z, radius)) continue;
      used.add(k);
      reserveDisc(x, z, radius + 3);
      return [x, z];
    }
    return null;
  };
  const tower = (name, w, d, h, color, extra) => {
    const at = place(Math.hypot(w, d) / 2);
    if (!at) return;
    const [x, z] = at;
    const b = building(x, z, w, d, h, a, color, 3);
    if (extra) extra(x, z, b.y + b.h);
    if (name) poi(name, x, z, 45);
  };
  // US Bank Tower: the round one with the glass crown
  let at = place(30);
  if (at) {
    const [x, z] = at, y = gh(x, z);
    cyl(x, y + 115, z, 52, 230, 52, '#a7a39a', { m: 'win' });
    cyl(x, y + 245, z, 44, 30, 44, '#a7a39a', { m: 'win' });
    cyl(x, y + 268, z, 36, 16, 36, '#a7a39a', { m: 'win' });
    cyl(x, y + 282, z, 30, 12, 30, '#bfe3ff', { m: 'glow' });
    W.addCircle(x, z, 26, y, y + 288, 'building');
    poi('US Bank Tower', x, z, 45);
  }
  // Wilshire Grand Center: the tallest in the west, with its sail and spire
  tower('Wilshire Grand Center', 48, 34, 270, '#5c7f99', (x, z, top) => {
    prism(x, top + 12, z, 30, 24, 30, '#7fa3bf', { ry: a, m: 'win' });
    cyl(x, top + 42, z, 1.6, 50, 1.6, '#dfe6ea');
    cyl(x, top + 68, z, 0.8, 1.6, 0.8, '#ff3b3b', { m: 'glow' });
  });
  tower('Aon Center', 46, 34, 262, '#2d3136');
  tower('Gas Company Tower', 42, 34, 229, '#4f6f8f', (x, z, top) => box(x, top + 7, z, 26, 14, 20, '#5ab0ff', { ry: a, m: 'glow' }));
  tower('Two California Plaza', 40, 40, 228, '#8fa4b0', (x, z, top) => roof(x, top, z, 30, 16, 30, '#8fa4b0', { ry: a }));
  at = place(46);
  if (at) {
    for (const lz of [-18, 18]) {
      const [ox, oz] = rotv(0, lz, a);
      building(at[0] + ox, at[1] + oz, 30, 26, 213, a, '#1f2226', 3);
    }
    poi('City National Plaza', at[0], at[1], 50);
  }
  tower('Bank of America Plaza', 40, 40, 200, '#7a4b3a');
  tower(null, 36, 36, 180, '#8a9aa6');
  // Walt Disney Concert Hall: billowing stainless-steel sails
  at = place(46);
  if (at) {
    const [x, z] = at, y = gh(x, z);
    box(x, y + 5, z, 76, 10, 58, '#c6c2b8', { ry: a, col: true });
    const sails = [[-22, -12, 34, 30, 0.25, 0.2], [-6, -18, 30, 34, -0.2, 0.3], [12, -10, 36, 26, 0.3, -0.25], [24, 6, 26, 30, -0.35, 0.2],
      [-18, 14, 30, 24, 0.2, -0.3], [4, 16, 34, 28, -0.25, -0.2], [-2, 0, 22, 38, 0.15, 0.05], [28, -14, 18, 22, 0.4, 0.35]];
    for (const [lx, lz, w, h, rz, rx] of sails) {
      const [ox, oz] = rotv(lx, lz, a);
      box(x + ox, y + 10 + h / 2 - 4, z + oz, w, h, 3, '#dfe3e6', { ry: a + rx * 2, rz, rx: rx * 0.5, m: 'metal' });
    }
    W.addBox(x, z, 36, 28, a, y, y + 32, 'building');
    poi('Walt Disney Concert Hall', x, z, 55);
  }
  // The Broad
  at = place(38);
  if (at) { building(at[0], at[1], 58, 48, 22, a, '#f4f2ee', 0); poi('The Broad', at[0], at[1], 40); }
  // Grand Park and its fountain
  at = place(50);
  if (at) {
    const [x, z] = at;
    paint({ t: 'rect', x, z, w: 92, d: 92, rot: a, c: '#78a052' }, 1);
    paint({ t: 'circle', x, z, r: 12, c: '#78c4e6' }, 1);
    cyl(x, gh(x, z) + 1.5, z, 3, 3, 3, '#ff7ae5', { m: 'glow' });
    poi('Grand Park', x, z, 60);
  }
  // Grand Central Market
  at = place(42);
  if (at) {
    const [x, z] = at;
    building(x, z, 70, 46, 12, a, '#9a5a43', 2);
    text(x + Math.sin(a) * 23.4, gh(x, z) + 9, z + Math.cos(a) * 23.4, 26, 2.8, 'GRAND CENTRAL MARKET', '#ffd84a', { ry: a, glow: true, font: 40 });
    poi('Grand Central Market', x, z, 45);
  }
});

def('City Hall', [34.0537, -118.2427], { poiR: 80 }, (api) => {
  const { building, roof, box, gh, reserveRect } = api;
  const [x, z] = P(34.0537, -118.2427);
  const y = gh(x, z);
  building(x, z, 90, 40, 40, 0, '#ece6d8', 2);
  building(x, z, 30, 30, 98, 0, '#ece6d8', 2);
  box(x, y + 103, z, 22, 10, 22, '#ece6d8', { col: true });
  roof(x, y + 108, z, 18, 18, 18, '#e3dccb');
  box(x, y + 127, z, 0.8, 3, 0.8, '#ff3b3b', { m: 'glow' });
  reserveRect(x, z, 50, 25, 0);
});

def('Union Station', [[1790, -735]], { poiR: 80 }, (api) => {
  const { building, roof, box, arch, paint, gh, reserveRect, text } = api;
  const [x, z] = [1790, -735];
  const y = gh(x, z);
  building(x, z, 140, 30, 14, 0, '#efe2c4', 1);
  roof(x, y + 14, z, 142, 7, 32, '#b5562f');
  building(x + 40, z, 10, 10, 38, 0, '#efe2c4', 1);
  roof(x + 40, y + 38, z, 11, 6, 11, '#b5562f');
  text(x + 40, y + 33, z + 5.2, 6, 6, '🕓', '#2b2b2b', { bg: '#fffaf0', font: 120 });
  for (let k = 0; k < 6; k++) arch(x - 50 + k * 8, y + 4, z + 16, 3.2, 0.8, 1.2, 0, '#efe2c4', 7);
  for (let k = 0; k < 5; k++) paint({ t: 'rect', x: x + 10, z: z - 40 - k * 12, w: 220, d: 3, rot: 0, c: '#5d5246' }, 1);
  reserveRect(x, z - 30, 115, 50, 0);
  void box;
});

def('Olvera Street', [[1690, -768]], { poiR: 50 }, (api) => {
  const { cyl, cone, paint, gh, reserveDisc, building, rnd } = api;
  const y = gh(1690, -768);
  paint({ t: 'circle', x: 1690, z: -768, r: 18, c: '#b5795a' }, 1);
  cyl(1690, y + 0.4, -768, 9, 0.8, 9, '#e8dcc4', { col: true });
  for (let k = 0; k < 8; k++) { const t = k / 8 * 6.28; cyl(1690 + Math.cos(t) * 4, y + 3, -768 + Math.sin(t) * 4, 0.4, 5, 0.4, '#f4efe6'); }
  cone(1690, y + 6.5, -768, 11, 3, 11, '#b5562f');
  for (let k = 0; k < 4; k++) building(1714 + k * 13, -776, 11, 10, 5, 0, rnd.pick(['#f2c14e', '#e84a5f', '#48c9b0', '#f4ead5']), 1);
  reserveDisc(1690, -778, 30);
});

def('Chinatown Dragon Gate', [[1610, -822]], { poiR: 60 }, (api) => {
  const { box, roof, gh, reserveRect } = api;
  const y = gh(1610, -822);
  for (const s of [-1, 1]) box(1610 + s * 9, y + 5, -822, 1.6, 10, 1.6, '#c0392b', { col: true });
  box(1610, y + 10.5, -822, 22, 1.4, 2.4, '#c0392b');
  roof(1610, y + 11.2, -822, 24, 3, 4, '#2e7d4f');
  reserveRect(1610, -822, 13, 4, 0);
});

def('Crypto.com Arena', [[1343, -300]], { pad: { r: 80, blend: 30 }, poiR: 110 }, (api) => {
  const { cyl, dome, box, building, gh, rnd } = api;
  const [x, z] = [1343, -300];
  const y = gh(x, z);
  cyl(x, y + 14, z, 132, 28, 104, '#b9c2c9', { col: true, m: 'metal' });
  dome(x, y + 28, z, 128, 12, 100, '#d3d9de');
  for (let k = 0; k < 2; k++) {
    building(x + 92, z + 10 + k * 50, 36, 34, 20, 0, '#2a2d31', 2);
    box(x + 73.6, y + 14, z + 10 + k * 50, 0.4, 10, 28, rnd.pick(['#ff4f8b', '#4fd1ff', '#7c4dff']), { m: 'glow' });
  }
  api.poi('LA Live', x + 92, z + 35, 50);
});

def('Sixth Street Viaduct', [[1877, -440]], { poiR: 150 }, (api) => {
  const { box, arch, ramp, gh, reserveRect } = api;
  const [x, z] = [1877, -440];
  const a = 0.1;
  const end = (s) => { const [ox, oz] = rotv(s, 0, a); return [x + ox, z + oz]; };
  const [ex0, ez0] = end(-110), [ex1, ez1] = end(110);
  const deck = Math.max(gh(ex0, ez0), gh(ex1, ez1)) + 5;
  box(x, deck - 0.5, z, 220, 1, 18, '#cfccc4', { ry: a, col: true });
  for (let k = 0; k < 8; k++) {
    const [ax, az] = end(-84 + k * 24);
    for (const side of [-1, 1]) {
      const [sx, sz] = rotv(0, side * 9.5, a);
      arch(ax + sx, deck, az + sz, 11, 0.8, 0.8, a, '#f4f4f2', 9);
    }
    const [px, pz] = end(-96 + k * 24);
    box(px, (deck + gh(px, pz) - 8) / 2, pz, 3, deck - gh(px, pz) + 8, 12, '#bdb9b0', { ry: a });
  }
  const [r0x, r0z] = end(-142), [r1x, r1z] = end(142);
  ramp(r0x, r0z, gh(r0x, r0z), ex0, ez0, deck, 18, '#cfccc4');
  ramp(r1x, r1z, gh(r1x, r1z), ex1, ez1, deck, 18, '#cfccc4');
  reserveRect(x, z, 146, 12, a);
});

def('LA River Channel', [[1895, -150]], { poiR: 90 }, () => {});

def('Dodger Stadium', [34.0739, -118.2400], { pad: { r: 250, blend: 110 }, poiR: 260 }, (api) => {
  const { box, cyl, paint, gh, text, tree } = api;
  const [x, z] = P(34.0739, -118.2400);
  const y = gh(x, z);
  const face = -Math.PI / 4;                      // outfield opens to the north-east
  paint({ t: 'circle', x, z, r: 235, c: '#8d8f91' });
  for (let r = 150; r < 235; r += 14) paint({ t: 'ring', x, z, r, w: 0.8, c: '#c9c9c9' }, 1);
  const [fx, fz] = rotv(40, 0, face);
  paint({ t: 'circle', x: x + fx, z: z + fz, r: 82, c: '#3f8f3f' }, 1);
  paint({ t: 'rect', x: x + fx * 0.45, z: z + fz * 0.45, w: 34, d: 34, rot: face + Math.PI / 4, c: '#b07a4a' }, 1);
  paint({ t: 'rect', x: x + fx * 0.45, z: z + fz * 0.45, w: 22, d: 22, rot: face + Math.PI / 4, c: '#3f8f3f' }, 1);
  // three decks of seats in Dodger blue, yellow, orange and teal, opening to the outfield
  const tiers = [[60, 0, '#e6b54a'], [76, 9, '#e07b39'], [92, 18, '#3a8fb7'], [108, 27, '#5bb5a2']];
  const n = 26;
  for (const [R, h0, c] of tiers) {
    for (let k = 0; k < n; k++) {
      const t = face + Math.PI * 0.42 + ((k + 0.5) / n) * Math.PI * 1.16;
      const px = x + Math.cos(t) * (R + 7), pz = z - Math.sin(t) * (R + 7);
      const len = (2 * Math.PI * (R + 7) * 0.58) / n + 1;
      box(px, y + h0 + 4.5, pz, len, 1.2, 15, c, { ry: t + Math.PI / 2, rx: -Math.atan2(8, 14) });
      // the outside: a concourse band under each deck, columns, a wall at the bottom
      const wx = x + Math.cos(t) * (R + 15), wz = z - Math.sin(t) * (R + 15);
      box(wx, y + h0 + 8.4, wz, len, 1.6, 2, '#e8e2d4', { ry: t + Math.PI / 2 });
      if (k % 2 === 0) box(wx, y + (h0 + 8) / 2, wz, 1.4, h0 + 8, 1.4, '#e8e2d4', { ry: t + Math.PI / 2 });
      if (R === 108) box(wx, y + 3, wz, len, 6, 1.2, '#d9d4c8', { ry: t + Math.PI / 2, col: true });
    }
  }
  for (const s of [-1, 1]) {
    const t = face + s * 0.95;
    const px = x + Math.cos(t) * 95, pz = z - Math.sin(t) * 95;
    cyl(px, y + 10, pz, 1.6, 20, 1.6, '#ddd');
    box(px, y + 24, pz, 18, 9, 2, '#222', { ry: t + Math.PI / 2 });
  }
  for (let k = 0; k < 6; k++) {
    const t = face + Math.PI * 0.5 + k * 0.42;
    const px = x + Math.cos(t) * 112, pz = z - Math.sin(t) * 112;
    cyl(px, y + 26, pz, 1, 52, 1, '#d9d9d9');
    box(px, y + 52, pz, 8, 4, 1, '#fffbe6', { ry: t + Math.PI / 2, m: 'glow' });
  }
  for (let k = 0; k < 24; k++) {
    const t = (k / 24) * Math.PI * 2;
    tree(x + Math.cos(t) * 225, z + Math.sin(t) * 225, 0, 22);
  }
  const [sx, sz] = rotv(-150, 0, face);
  box(x + sx, y + 3, z + sz, 30, 6, 1, '#f4f4f4', { ry: face + Math.PI / 2 });
  text(x + sx + Math.cos(face) * -0.6, y + 3, z + sz - Math.sin(face) * -0.6, 28, 4, 'Dodger Stadium', '#1e5aa8', { ry: face - Math.PI / 2, font: 64 });
});

/* ------------------------------------------------------------------ South */

/* A stadium bowl: seating ring of tilted panels, wall round the outside. */
function bowl(api, x, z, y, a, b, depth, h, rot, colors, gaps = 2) {
  const { box } = api;
  const n = 40;
  for (let k = 0; k < n; k++) {
    const t = ((k + 0.5) / n) * Math.PI * 2;
    if (gaps && k % Math.floor(n / gaps) === 0) continue;
    const ex = Math.cos(t) * (a + depth / 2), ez = Math.sin(t) * (b + depth / 2);
    const [ox, oz] = rotv(ex, ez, rot);
    let nx = Math.cos(t) * b, nz = Math.sin(t) * a;
    const nl = Math.hypot(nx, nz); nx /= nl; nz /= nl;
    const [wnx, wnz] = rotv(nx, nz, rot);
    const ry = Math.atan2(wnx, wnz);
    const len = (2 * Math.PI * Math.hypot(a + depth / 2, b + depth / 2) / Math.SQRT2) / n + 1.5;
    box(x + ox, y + h / 2, z + oz, len, 1.4, depth * 1.12, colors[k % colors.length], { ry, rx: -Math.atan2(h, depth) });
    const wx = x + ox + wnx * (depth / 2 + 1), wz = z + oz + wnz * (depth / 2 + 1);
    box(wx, y + h / 2 + 1, wz, len, h + 2, 1.6, '#d9d4c8', { ry, col: true });
  }
}

def('LA Memorial Coliseum', [[1029, 110]], { pad: { r: 120, blend: 40 }, poiR: 160 }, (api) => {
  const { arch, box, cyl, cone, paint, gh } = api;
  const [x, z] = [1029, 110];
  const y = gh(x, z);
  paint({ t: 'ellipse', x, z, rx: 84, rz: 58, rot: 0, c: '#3f8f3f' }, 1);
  paint({ t: 'rect', x, z, w: 92, d: 44, rot: 0, c: '#46994a' }, 1);
  for (let k = -5; k <= 5; k++) paint({ t: 'rect', x: x + k * 8.4, z, w: 0.5, d: 44, rot: 0, c: '#f4f4f4' }, 1);
  bowl(api, x, z, y, 55, 38, 30, 20, 0, ['#9c8e7a', '#a89a85'], 2);
  // the peristyle and the Olympic torch, closing the east end
  const px = x + 60;
  for (let k = 0; k < 7; k++) {
    arch(px, y + 10, z - 27 + k * 9, 4.2, 1, 3, Math.PI / 2, '#d8cdb8', 7);
    box(px, y + 5, z - 31.5 + k * 9, 2.4, 10, 2.4, '#d8cdb8', { col: true });
  }
  box(px, y + 15, z, 3, 2, 66, '#d8cdb8');
  cyl(px, y + 21, z, 4, 10, 4, '#c9b48f');
  cone(px, y + 28, z, 4, 6, 4, '#ff8a2a', { m: 'glow' });
});

def('California Science Center', [[1010, -60]], { poiR: 60 }, (api) => {
  const { cyl, cone, box, building, gh, reserveRect } = api;
  const [x, z] = [1010, -60];
  const y = gh(x, z);
  building(x + 70, z - 75, 64, 40, 16, 0, '#a76b4d', 2, 'hip', '#b5562f');
  cyl(x + 70, y + 22, z - 75, 14, 10, 14, '#a76b4d');
  api.poi('Natural History Museum', x + 70, z - 75, 40);
  for (const lx of [-10, 10]) box(x + lx, y + 3, z, 1.2, 6, 1.2, '#666');
  cyl(x, y + 9, z, 8, 34, 8, '#f6f6f2', { rz: Math.PI / 2, col: true });
  cone(x + 19, y + 9, z, 8, 6, 8, '#2a2a2a', { rz: -Math.PI / 2 });
  box(x - 2, y + 7, z, 22, 0.8, 24, '#2a2a2a', { ry: 0, rz: 0 });
  box(x - 14, y + 15, z, 7, 10, 0.8, '#f6f6f2', { rz: 0.4 });
  for (const s of [-1, 1]) cyl(x - 18, y + 8 + s * 1.6, z + s * 2, 2.4, 3, 2.4, '#d9d9d9', { rz: Math.PI / 2 });
  reserveRect(x, z, 30, 16, 0);
  reserveRect(x + 70, z - 75, 36, 24, 0);
  api.poi('Space Shuttle Endeavour', x, z, 40);
});

def('Watts Towers', [33.9389, -118.2411], { pad: { r: 30, blend: 20 }, poiR: 70 }, (api) => {
  const { cone, torus, box, gh, reserveDisc } = api;
  const [x, z] = P(33.9389, -118.2411);
  const y = gh(x, z);
  const tiles = ['#3d8bd9', '#d9a83d', '#c94f4f', '#4fae6b', '#e9e4da'];
  for (const [lx, lz, H] of [[0, 0, 30], [7, 5, 29], [-6, 7, 24], [4, -8, 16], [-8, -5, 13]]) {
    cone(x + lx, y + H / 2, z + lz, 3.4, H, 3.4, '#8f8a7e', { col: true });
    for (let r = 0; r < 7; r++) {
      const f = r / 7;
      torus(x + lx, y + H * f * 0.9 + 1, z + lz, 9 * (1 - f) + 2, 9 * (1 - f) + 2, 4, tiles[(r + Math.abs(lx)) % tiles.length], { rx: Math.PI / 2 });
    }
  }
  for (const [lx, lz, w, d] of [[0, -12, 26, 0.6], [0, 12, 26, 0.6], [-13, 0, 0.6, 24], [13, 0, 0.6, 24]]) box(x + lx, y + 1, z + lz, w, 2, d, '#c9b48f');
  reserveDisc(x, z, 18);
});

def('Mariachi Plaza', [[2075, -470]], { poiR: 60 }, (api) => {
  const { cyl, cone, paint, gh, reserveDisc } = api;
  const [x, z] = [2075, -470];
  const y = gh(x, z);
  paint({ t: 'circle', x, z, r: 16, c: '#b5795a' }, 1);
  cyl(x, y + 0.5, z, 10, 1, 10, '#e8dcc4', { col: true });
  for (let k = 0; k < 8; k++) { const t = k / 8 * 6.28; cyl(x + Math.cos(t) * 4.4, y + 3.4, z + Math.sin(t) * 4.4, 0.4, 5, 0.4, '#d8c47a'); }
  cone(x, y + 7.2, z, 12, 3.4, 12, '#c9a54a');
  reserveDisc(x, z, 16);
});

/* ---------------------------------------------------------------- Pasadena */

def('Rose Bowl', [34.1613, -118.1676], { pad: { r: 170, blend: 70 }, poiR: 180 }, (api) => {
  const { paint, text, box, gh } = api;
  const [x, z] = P(34.1613, -118.1676);
  const y = gh(x, z);
  paint({ t: 'circle', x, z, r: 160, c: '#7f9a5a' });
  paint({ t: 'ellipse', x, z, rx: 70, rz: 48, rot: 0.1, c: '#3f8f3f' }, 1);
  paint({ t: 'rect', x, z, w: 90, d: 44, rot: 0.1, c: '#46994a' }, 1);
  bowl(api, x, z, y, 72, 50, 36, 18, 0.1, ['#c4b9a4', '#d1c7b3'], 2);
  box(x, y + 4, z + 104, 30, 8, 1, '#a51c30');
  text(x, y + 4, z + 104.6, 28, 6, 'ROSE BOWL', '#fff6d8', { font: 80 });
});

def('Pasadena City Hall', [[2600, -2040]], { pad: { r: 60, blend: 40 }, poiR: 80 }, (api) => {
  const { building, dome, cyl, gh } = api;
  const [x, z] = [2600, -2040];
  const y = gh(x, z);
  building(x, z, 70, 50, 20, 0, '#eadfc8', 1, 'hip', '#b5562f');
  building(x, z, 16, 16, 40, 0, '#eadfc8', 1);
  cyl(x, y + 43, z, 13, 6, 13, '#eadfc8');
  dome(x, y + 46, z, 13, 10, 13, '#c9a54a');
});

/* -------------------------------------------------------------- the harbour */

def('Port of Los Angeles', [[1650, 2650]], { poiR: 280 }, (api) => {
  const { box, cyl, rnd, gh } = api;
  const quay = [[1300, 2770], [1600, 2720], [2000, 2670]];
  // gantry cranes along the quay, booms out over the water
  for (let k = 0; k < 9; k++) {
    const t = k / 8;
    const seg = t < 0.43 ? 0 : 1;
    const tt = seg === 0 ? t / 0.43 : (t - 0.43) / 0.57;
    const [ax, az] = quay[seg], [bx, bz] = quay[seg + 1];
    const qx = ax + (bx - ax) * tt, qz = az + (bz - az) * tt;
    const len = Math.hypot(bx - ax, bz - az);
    const tx = (bx - ax) / len, tz = (bz - az) / len, nx = -tz, nz = tx;
    const a = yawOf(nx, nz);
    const cx = qx - nx * 14, cz = qz - nz * 14;
    const y = Math.max(1, gh(cx, cz));
    for (const [lx, lz] of [[-12, -9], [12, -9], [-12, 9], [12, 9]]) {
      const [ox, oz] = rotv(lx, lz, a);
      box(cx + ox, y + 21, cz + oz, 1.8, 42, 1.8, k % 2 ? '#c0392b' : '#e8e8e8', { col: true, ry: a });
    }
    box(cx, y + 43, cz, 30, 3, 22, '#c0392b', { ry: a });
    const [bx2, bz2] = rotv(30, 0, a);
    box(cx + bx2, y + 46, cz + bz2, 90, 2.4, 4, '#e8e8e8', { ry: a });
    box(cx, y + 47, cz, 10, 5, 8, '#f4f4f4', { ry: a });
  }
  // a container ship alongside
  const sx = 1800, sz = 2735, sa = yawOf(400, -50);
  box(sx, 1, sz, 260, 12, 36, '#22324a', { ry: sa, col: true });
  box(sx, -4.5, sz, 262, 3, 37, '#9b2c2c', { ry: sa });
  const colors = ['#c0392b', '#2e86c1', '#1e8449', '#d68910', '#7d3c98', '#5d6d7e', '#a04000', '#117a65'];
  for (let i = -8; i <= 7; i++) for (let j = -2; j <= 2; j++) {
    const n = rnd.int(1, 4);
    const [ox, oz] = rotv(i * 13, j * 6.4, sa);
    for (let h = 0; h < n; h++) box(sx + ox, 7 + 1.3 + h * 2.6, sz + oz, 12.2, 2.55, 6, rnd.pick(colors), { ry: sa });
  }
  const [bx3, bz3] = rotv(-118, 0, sa);
  box(sx + bx3, 7 + 12, sz + bz3, 14, 24, 32, '#f4f4f4', { ry: sa, col: true });
  void cyl;
});

def('Queen Mary', [[2860, 2600]], { poiR: 180 }, (api) => {
  const { box, cyl, ramp, gh } = api;
  const [x, z] = [2860, 2600];
  box(x, 1, z, 236, 14, 34, '#1b1b1b', { col: true });
  box(x, -4, z, 238, 4, 35, '#a32020');
  box(x, 6.4, z, 238, 1, 35, '#f4f4f4');
  box(x - 6, 11.5, z, 160, 9, 28, '#f4f1ea', { col: true });
  box(x - 6, 18, z, 110, 4, 24, '#f4f1ea', { col: true });
  box(x + 38, 21.5, z, 16, 3, 20, '#f4f1ea');
  for (const [k, lx] of [-40, -8, 24].entries()) {
    cyl(x + lx, 27, z, 9, 14, 7, '#c0392b', { rz: 0.08 });
    cyl(x + lx - 0.5, 34.5, z, 9.2, 2, 7.2, '#1b1b1b', { rz: 0.08 });
    void k;
  }
  for (const lx of [-95, 90]) cyl(x + lx, 25, z, 0.8, 30, 0.8, '#3a2a1a');
  ramp(x - 30, 2525, gh(x - 30, 2525), x - 30, 2580, 8, 4, '#8a6f55');
});

/* ------------------------------------------------------------------ build */

/* The second wave of places (theatres, studios, murals, parks) lives in
 * landmarks2.js; it registers through the same def(). */
registerMore(def, { P, rotv, yawOf, shoreFrame, roadClear, DTLA });

export const PADS = L.filter((l) => l.pad).map((l) => ({ name: l.name, x: l.at[0], z: l.at[1], r: l.pad.r, blend: l.pad.blend, dy: l.pad.dy, y: l.pad.y, open: l.pad.open }));
export const LANDMARKS = L.map((l) => ({ name: l.name, x: l.at[0], z: l.at[1] }));

export function buildLandmarks(api) {
  for (const l of L) {
    l.build(api, l.at[0], l.at[1]);
    if (l.poiR) api.poi(l.name, l.at[0], l.at[1], l.poiR);
  }
}
