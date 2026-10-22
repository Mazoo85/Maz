/*
 * Street furniture: the clutter that makes a street read as LA.
 *
 * Cobra-head streetlights down the boulevards, wooden power poles with sagging
 * wires along the residential streets, traffic signals and zebra crosswalks
 * where boulevards cross, stop signs, yellow fire hydrants, bus benches,
 * blue mailboxes, black/blue/green bins out on the curb, and the big green
 * overhead signs on the freeways ("405 NORTH · Sherman Oaks").
 *
 * Everything goes into plain arrays on the world (W.props, W.wires,
 * W.crosswalks, W.gantries) that the renderer instances. Pure data — no DOM.
 */

import { ROADS, ROAD_WIDTH, HALF } from './geo.js';
import { walk, rot } from './world.js';

/* Shop signs, by what the neighbourhood would actually have. */
export const SIGN_TEXTS = [
  'TACOS', 'LIQUOR', 'DONUTS', 'NAILS', 'PHO', 'BOBA TEA', 'CHECK CASHING', '99¢ STORE', 'BURGERS', 'COFFEE',
  'DENTIST', 'BAIL BONDS', 'AUTO REPAIR', 'SMOG CHECK', 'MOTEL', 'PIZZA', 'JUICE BAR', 'YOGA', 'SURF SHOP', 'TATTOO',
  'VINTAGE', 'SOUVENIRS', 'CARNICERÍA', 'PANADERÍA', 'MERCADO', 'LAVANDERÍA', 'KOREAN BBQ', 'NORAEBANG', 'SUSHI', 'THAI FOOD',
  'BARBER', 'DISPENSARY', 'RECORDS', 'LAUNDROMAT', 'PAWN', 'HOT DOGS', 'FLOWERS', 'PHARMACY', 'PRODUCE', 'BAKERY',
  'DINER', 'COCKTAILS', 'NIGHTCLUB', 'PILATES', 'BOXING GYM', 'CHURROS', 'RAMEN', 'DIM SUM', 'BOOKS', 'HARDWARE',
  'MARISCOS', 'BOTÁNICA', 'COMEDY CLUB', 'GUITARS', 'CAR WASH', 'TAQUERÍA', 'DRIVE-THRU', 'PUPUSERÍA', 'POKE', 'CHICKEN'
];
const common = ['LIQUOR', 'DONUTS', 'NAILS', 'CHECK CASHING', '99¢ STORE', 'BURGERS', 'COFFEE', 'DENTIST', 'BAIL BONDS', 'AUTO REPAIR', 'SMOG CHECK', 'PIZZA', 'TACOS', 'LAUNDROMAT', 'PHARMACY', 'BARBER', 'CHICKEN', 'CAR WASH'];
const latino = ['CARNICERÍA', 'PANADERÍA', 'MERCADO', 'LAVANDERÍA', 'TACOS', 'TAQUERÍA', 'CHURROS', 'MARISCOS', 'BOTÁNICA', 'PUPUSERÍA', 'LIQUOR', 'CHECK CASHING', '99¢ STORE', 'BARBER', 'AUTO REPAIR'];
const beach = ['SURF SHOP', 'JUICE BAR', 'YOGA', 'PILATES', 'COFFEE', 'TACOS', 'SUSHI', 'POKE', 'VINTAGE', 'DISPENSARY', 'COCKTAILS'];
const hollywood = ['TATTOO', 'VINTAGE', 'SOUVENIRS', 'PIZZA', 'COCKTAILS', 'NIGHTCLUB', 'RECORDS', 'DISPENSARY', 'HOT DOGS', 'COMEDY CLUB', 'GUITARS', 'MOTEL', 'DINER'];
export const SIGN_POOLS = {
  default: common,
  'East LA': latino, 'Boyle Heights': latino, 'Highland Park': [...latino, 'RECORDS', 'COFFEE', 'VINTAGE'], Vernon: latino,
  Watts: [...latino, ...common], Compton: [...latino, ...common], 'South LA': [...latino, ...common],
  Koreatown: ['KOREAN BBQ', 'NORAEBANG', 'BOBA TEA', 'PHO', 'COFFEE', 'DENTIST', 'NAILS', 'BAKERY', 'RAMEN', 'PUPUSERÍA'],
  Chinatown: ['DIM SUM', 'BAKERY', 'PHARMACY', 'PRODUCE', 'RAMEN', 'BOBA TEA', 'FLOWERS'],
  Hollywood: hollywood, 'West Hollywood': [...hollywood, 'PILATES', 'FLOWERS', 'BOOKS'], 'Universal City': hollywood,
  Venice: beach, 'Santa Monica': [...beach, 'BOOKS'], Malibu: beach, 'Manhattan Beach': beach, 'Marina del Rey': beach,
  'Culver City': ['COFFEE', 'RAMEN', 'BOOKS', 'PILATES', 'SUSHI', 'BURGERS', 'COCKTAILS'], Pasadena: ['BOOKS', 'COFFEE', 'DINER', 'BAKERY', 'HARDWARE', 'RECORDS'],
  'Silver Lake': ['COFFEE', 'VINTAGE', 'RECORDS', 'TATTOO', 'COCKTAILS', 'TACOS'], 'Echo Park': ['COFFEE', 'VINTAGE', 'RECORDS', 'TATTOO', 'TACOS', 'BOTÁNICA']
};

/* Prop types the renderer knows how to draw. */
export const PROP = { STREETLIGHT: 0, POWER_POLE: 1, HYDRANT: 2, BENCH: 3, BIN: 4, MAILBOX: 5, SIGNAL: 6, STOP_SIGN: 7 };

export function buildStreets(api, W) {
  const { rnd, gh, MASK } = api;
  const props = W.props, wires = W.wires;
  const freeOf = MASK.RESERVED | MASK.CHANNEL | MASK.LAKE;
  const ok = (x, z, avoidRoad = false) => {
    if (Math.abs(x) > HALF - 20 || Math.abs(z) > HALF - 20) return false;
    const m = W.maskAt(x, z);
    if (m & freeOf) return false;
    if (avoidRoad && m & MASK.ROAD && onRoadway(x, z)) return false;
    if (W.isSea(x, z) || gh(x, z) < 0.6) return false;
    return W.slopeAt(x, z) < 0.25;
  };
  /* The mask marks a little verge round each road; this checks the
   * carriageway itself. */
  const onRoadway = (x, z) => ROADS.some((r) => distTo(r.pts, x, z) < ROAD_WIDTH[r.kind] / 2 + 0.6);
  const add = (t, x, z, ry, extra = {}) => {
    const y = gh(x, z);
    props.push({ t, x, y, z, ry, ...extra });
    if (t === 0 || t === 1 || t === 6 || t === 7) W.addCircle(x, z, t === 1 ? 0.2 : 0.16, y, y + (t === 1 ? 10 : 7), 'pole');
    else if (t === 3) W.addBox(x, z, 1, 0.35, ry, y, y + 0.9, 'bench');
    return y;
  };

  /* boulevards: streetlights, hydrants, bus benches, mailboxes */
  for (const r of ROADS) {
    if (r.kind !== 'blvd') continue;
    const off = ROAD_WIDTH.blvd / 2 + 1.0;
    let side = 1, n = 0;
    walk(r.pts, 46, (x, z, dx, dz) => {
      const nx = -dz * side, nz = dx * side;
      const px = x + nx * off, pz = z + nz * off;
      if (ok(px, pz, true)) add(PROP.STREETLIGHT, px, pz, Math.atan2(nz, -nx));
      side = -side;
      n++;
      // a hydrant, a bench or a mailbox on the near kerb now and then
      const k = n % 5;
      const qx = x - dz * (off + 0.6) * -side, qz = z + dx * (off + 0.6) * -side;
      if (!ok(qx, qz, true)) return;
      if (k === 0) add(PROP.HYDRANT, qx, qz, 0);
      else if (k === 2 && rnd.chance(0.5)) add(PROP.BENCH, qx, qz, Math.atan2(-dz * side, dx * side) + Math.PI / 2, { ad: rnd.int(0, 5) });
      else if (k === 4 && rnd.chance(0.25)) add(PROP.MAILBOX, qx, qz, Math.atan2(-dz, dx));
    });
  }

  /* where two boulevards cross: signals on opposite corners, crosswalks */
  const blvds = ROADS.filter((r) => r.kind === 'blvd');
  for (let a = 0; a < blvds.length; a++) {
    for (let b = a + 1; b < blvds.length; b++) {
      for (const X of crossings(blvds[a].pts, blvds[b].pts)) {
        const { x, z, d1, d2 } = X;
        if (!ok(x, z)) continue;
        const half = ROAD_WIDTH.blvd / 2;
        for (const s of [1, -1]) {
          const n1 = [-d1[1] * s, d1[0] * s], n2 = [-d2[1] * s, d2[0] * s];
          const cx = x + (n1[0] + n2[0]) * (half + 2), cz = z + (n1[1] + n2[1]) * (half + 2);
          if (!ok(cx, cz, true)) continue;
          // the mast arm reaches back over the road towards the middle
          add(PROP.SIGNAL, cx, cz, Math.atan2(n1[1], -n1[0]), { arm: half + 1 });
        }
        for (const [d, sgn] of [[d1, 1], [d1, -1], [d2, 1], [d2, -1]]) {
          const o = half + 3.5;
          const cx = x + d[0] * o * sgn, cz = z + d[1] * o * sgn;
          W.crosswalks.push({ x: cx, z: cz, y: gh(cx, cz), ry: Math.atan2(-d[1], d[0]), width: ROAD_WIDTH.blvd });
        }
      }
    }
  }

  /* residential streets: power poles and wires, bins, stop signs */
  for (const blk of W.blocks) {
    const res = blk.style === 'residential' || blk.style === 'suburb' || blk.style === 'beach' || blk.style === 'mixed';
    if (!res) continue;
    const toW = (lx, lz) => { const [ox, oz] = rot(lx, lz, blk.rot); return [blk.x + ox, blk.z + oz]; };
    // poles down one side, the wires slung between them
    const lz = blk.d / 2 + 1.3;
    let prev = null;
    for (let lx = -blk.w / 2 + 5; lx <= blk.w / 2 - 2; lx += 33) {
      const [x, z] = toW(lx, lz);
      if (!ok(x, z, true)) { prev = null; continue; }
      const y = add(PROP.POWER_POLE, x, z, blk.rot);
      const tops = [-0.95, 0, 0.95].map((c) => { const [ox, oz] = rot(0, c, blk.rot); return [x + ox, y + (c === 0 ? 10.2 : 9.6), z + oz]; });
      if (prev) for (let k = 0; k < 3; k++) sag(prev[k], tops[k], 0.55, wires);
      prev = tops;
    }
    // bins out for collection
    if (blk.style !== 'mixed') {
      for (let k = 0; k < 5; k++) {
        if (!rnd.chance(0.55)) continue;
        const side = rnd.chance(0.5) ? 1 : -1;
        const [x, z] = toW(rnd.range(-blk.w / 2 + 4, blk.w / 2 - 4), side * (blk.d / 2 + 0.6));
        if (ok(x, z, true)) add(PROP.BIN, x, z, blk.rot + rnd.range(-0.3, 0.3), { color: rnd.int(0, 2) });
      }
    }
    // a stop sign on one corner
    if (rnd.chance(0.55)) {
      const sx = rnd.chance(0.5) ? 1 : -1, sz = rnd.chance(0.5) ? 1 : -1;
      const [x, z] = toW(sx * (blk.w / 2 + 1.2), sz * (blk.d / 2 + 1.2));
      if (ok(x, z, true)) add(PROP.STOP_SIGN, x, z, blk.rot + (sx > 0 ? -Math.PI / 2 : Math.PI / 2));
    }
  }

  /* the big green signs over the freeways */
  const texts = W.gantryTexts = [];
  for (const r of ROADS) {
    if (r.kind !== 'freeway') continue;
    const route = routeOf(r.name);
    walk(r.pts, 760, (x, z, dx, dz) => {
      if (!ok(x, z) && !(W.maskAt(x, z) & MASK.ROAD)) return;
      if (W.isSea(x, z) || Math.abs(x) > HALF - 60 || Math.abs(z) > HALF - 60) return;
      const span = ROAD_WIDTH.freeway + 6;
      const nx = -dz, nz = dx;
      const panels = [];
      for (const dir of [1, -1]) {
        const word = compass(dx * dir, dz * dir);
        // the next real neighbourhood ahead, for the "next exit" line
        let next = null;
        for (const ahead of [700, 1100, 1600, 2200]) {
          const ax = Math.max(-HALF, Math.min(HALF, x + dx * dir * ahead)), az = Math.max(-HALF, Math.min(HALF, z + dz * dir * ahead));
          const d = W.districtAt(ax, az);
          if (!['wild', 'park', 'airport', 'oilfield'].includes(d.style) && d.name !== W.districtAt(x, z).name) { next = d.name; break; }
        }
        const control = (CONTROL[route.num] || {})[word] || 'Los Angeles';
        const key = `${route.num}|${word}|${control}|${next}`;
        let idx = texts.findIndex((t) => t.key === key);
        if (idx < 0) { idx = texts.length; texts.push({ key, ...route, dir: word, dest: control, next }); }
        panels.push({ cell: idx, off: (span / 4) * dir, flip: dir < 0 });
      }
      const y = gh(x, z);
      W.gantries.push({ x, y, z, ry: Math.atan2(-nz, nx), span, panels });
      for (const s of [-1, 1]) W.addCircle(x + nx * (span / 2) * s, z + nz * (span / 2) * s, 0.4, y, y + 8, 'gantry');
    });
  }
}

/* A wire slung between two points, sagging in the middle. */
function sag(a, b, drop, out) {
  const n = 4;
  for (let i = 0; i < n; i++) {
    const t0 = i / n, t1 = (i + 1) / n;
    const y0 = a[1] + (b[1] - a[1]) * t0 - drop * 4 * t0 * (1 - t0);
    const y1 = a[1] + (b[1] - a[1]) * t1 - drop * 4 * t1 * (1 - t1);
    out.push(a[0] + (b[0] - a[0]) * t0, y0, a[2] + (b[2] - a[2]) * t0, a[0] + (b[0] - a[0]) * t1, y1, a[2] + (b[2] - a[2]) * t1);
  }
}

function crossings(p, q) {
  const out = [];
  for (let i = 0; i < p.length - 1; i++) {
    for (let k = 0; k < q.length - 1; k++) {
      const a = p[i], b = p[i + 1], c = q[k], d = q[k + 1];
      const r0 = b[0] - a[0], r1 = b[1] - a[1], s0 = d[0] - c[0], s1 = d[1] - c[1];
      const den = r0 * s1 - r1 * s0;
      if (Math.abs(den) < 1e-9) continue;
      const t = ((c[0] - a[0]) * s1 - (c[1] - a[1]) * s0) / den;
      const u = ((c[0] - a[0]) * r1 - (c[1] - a[1]) * r0) / den;
      if (t < 0 || t > 1 || u < 0 || u > 1) continue;
      const l1 = Math.hypot(r0, r1), l2 = Math.hypot(s0, s1);
      out.push({ x: a[0] + r0 * t, z: a[1] + r1 * t, d1: [r0 / l1, r1 / l1], d2: [s0 / l2, s1 / l2] });
    }
  }
  return out;
}

function distTo(pts, x, z) {
  let best = Infinity;
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, az] = pts[i], [bx, bz] = pts[i + 1];
    const dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
    const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / L2));
    best = Math.min(best, Math.hypot(ax + dx * t - x, az + dz * t - z));
  }
  return best;
}

/* Which shield goes on the sign: Interstate, US route or California route. */
export function routeOf(name) {
  if (name.startsWith('I-')) return { kind: 'interstate', num: name.slice(2).split(' ')[0] };
  if (name.startsWith('US-')) return { kind: 'us', num: name.slice(3).split(' ')[0] };
  if (name.startsWith('SR-')) return { kind: 'state', num: name.slice(3).split(' ')[0] };
  if (name.startsWith('Arroyo')) return { kind: 'state', num: '110' };
  return { kind: 'state', num: '1' };
}

/* The control cities on LA's real freeway signs. */
const CONTROL = {
  405: { NORTH: 'Sacramento', SOUTH: 'Long Beach', EAST: 'Long Beach', WEST: 'Sacramento' },
  10: { WEST: 'Santa Monica', EAST: 'San Bernardino', NORTH: 'Santa Monica', SOUTH: 'San Bernardino' },
  101: { NORTH: 'Ventura', WEST: 'Ventura', SOUTH: 'Los Angeles', EAST: 'Los Angeles' },
  110: { NORTH: 'Pasadena', EAST: 'Pasadena', SOUTH: 'San Pedro', WEST: 'San Pedro' },
  5: { NORTH: 'Sacramento', WEST: 'Sacramento', SOUTH: 'Santa Ana', EAST: 'Santa Ana' },
  105: { WEST: 'El Segundo', EAST: 'Norwalk', NORTH: 'El Segundo', SOUTH: 'Norwalk' },
  710: { SOUTH: 'Long Beach', NORTH: 'Pasadena', EAST: 'Pasadena', WEST: 'Long Beach' },
  134: { EAST: 'Pasadena', WEST: 'Ventura', NORTH: 'Ventura', SOUTH: 'Pasadena' }
};

function compass(dx, dz) {
  return Math.abs(dx) > Math.abs(dz) ? (dx > 0 ? 'EAST' : 'WEST') : (dz < 0 ? 'NORTH' : 'SOUTH');
}
