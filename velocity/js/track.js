/*
 * VELOCITY — the track.
 *
 * Harbor Loop is a closed loop of control points. build() turns it into a
 * dense centreline (a point every few metres) that the physics, the AI and
 * the 3D scene all read. No DOM and no Three.js: this file runs in plain Node.
 *
 * Coordinates are metres: x runs east, z runs south, y is up (the Three.js
 * conventions). A heading θ means the direction (cos θ, sin θ) in x–z.
 * "lateral" is the signed distance from the centreline, positive to the right
 * of the direction of travel.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.VelocityTrack = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const RAMP_LENGTH = 30;   // metres of wedge before the lip
  const RAMP_HEIGHT = 2.5;  // metres at the lip
  const SEARCH_WINDOW = 25; // points either side of a hint to search

  // Control points before scaling. w is the road width in metres. The flags
  // describe the stretch from this point to the next.
  const HARBOR_LOOP = {
    name: 'Harbor Loop',
    scale: 1.35,
    spacing: 4,
    controls: [
      { x: 0, z: 0, w: 16, water: true },        // start/finish straight, harbor on the right
      { x: 250, z: 0, w: 16, water: true },
      { x: 420, z: -20, w: 16, water: true },
      { x: 520, z: -90, w: 14 },                  // the harbor sweep
      { x: 560, z: -220, w: 14 },
      { x: 540, z: -340, w: 14 },
      { x: 460, z: -420, w: 12 },                 // chicane
      { x: 400, z: -400, w: 12 },
      { x: 340, z: -460, w: 12 },
      { x: 260, z: -470, w: 14 },
      { x: 120, z: -480, w: 14, tunnel: true },   // the tunnel
      { x: -40, z: -470, w: 14, tunnel: true },
      { x: -160, z: -400, w: 14 },
      { x: -200, z: -300, w: 14, ramp: true },    // the jump
      { x: -210, z: -180, w: 14 },
      { x: -240, z: -100, w: 14 },
      { x: -180, z: -40, w: 14 },
      { x: -80, z: -10, w: 16, water: true },
    ],
  };

  function catmull(p0, p1, p2, p3, t) {
    const t2 = t * t, t3 = t2 * t;
    const f = (a, b, c, d) => 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
    return { x: f(p0.x, p1.x, p2.x, p3.x), z: f(p0.z, p1.z, p2.z, p3.z) };
  }

  function wrapAngle(a) {
    while (a > Math.PI) a -= 2 * Math.PI;
    while (a < -Math.PI) a += 2 * Math.PI;
    return a;
  }

  function build(def) {
    def = def || HARBOR_LOOP;
    const sc = def.scale || 1;
    const ctl = def.controls.map((c) => Object.assign({}, c, { x: c.x * sc, z: c.z * sc }));
    const n = ctl.length;
    const pts = [];
    for (let i = 0; i < n; i++) {
      const p0 = ctl[(i - 1 + n) % n], p1 = ctl[i], p2 = ctl[(i + 1) % n], p3 = ctl[(i + 2) % n];
      const chord = Math.hypot(p2.x - p1.x, p2.z - p1.z);
      const steps = Math.max(2, Math.round(chord / def.spacing));
      for (let k = 0; k < steps; k++) {
        const t = k / steps;
        const q = catmull(p0, p1, p2, p3, t);
        pts.push({
          x: q.x, z: q.z, w: p1.w + (p2.w - p1.w) * t,
          tunnel: !!p1.tunnel, water: !!p1.water, control: i,
          ramp: false, rampLip: k === 0 && !!p1.ramp, rampHeight: 0,
          s: 0, dx: 1, dz: 0, curvature: 0,
        });
      }
    }
    const N = pts.length;
    // Distance along the lap.
    let s = 0;
    for (let i = 0; i < N; i++) {
      pts[i].s = s;
      const nx = pts[(i + 1) % N];
      s += Math.hypot(nx.x - pts[i].x, nx.z - pts[i].z);
    }
    const length = s;
    // Direction and curvature from the neighbours.
    for (let i = 0; i < N; i++) {
      const a = pts[(i - 1 + N) % N], b = pts[(i + 1) % N];
      const dx = b.x - a.x, dz = b.z - a.z, d = Math.hypot(dx, dz) || 1;
      pts[i].dx = dx / d; pts[i].dz = dz / d;
    }
    for (let i = 0; i < N; i++) {
      const a = pts[(i - 1 + N) % N], b = pts[(i + 1) % N];
      const da = wrapAngle(Math.atan2(b.dz, b.dx) - Math.atan2(a.dz, a.dx));
      let ds = b.s - a.s; if (ds <= 0) ds += length;
      pts[i].curvature = da / ds;
    }
    // Ramps: the wedge rises over RAMP_LENGTH metres before each lip.
    const ramps = [];
    for (let i = 0; i < N; i++) {
      if (!pts[i].rampLip) continue;
      ramps.push(pts[i].s);
      for (let k = 0; k < N; k++) {
        let back = pts[i].s - pts[k].s; if (back < 0) back += length;
        if (back <= RAMP_LENGTH) { pts[k].ramp = true; pts[k].rampHeight = RAMP_HEIGHT * (1 - back / RAMP_LENGTH); }
      }
    }

    function project(a, b, x, z) {
      const abx = b.x - a.x, abz = b.z - a.z;
      const len2 = abx * abx + abz * abz || 1;
      const t = Math.max(0, Math.min(1, ((x - a.x) * abx + (z - a.z) * abz) / len2));
      const px = a.x + abx * t, pz = a.z + abz * t;
      const len = Math.sqrt(len2);
      return { t, px, pz, len, dx: abx / len, dz: abz / len, d2: (x - px) * (x - px) + (z - pz) * (z - pz) };
    }

    function nearestIndex(x, z, hint) {
      let best = -1, bestD = Infinity;
      if (typeof hint === 'number' && hint >= 0) {
        for (let k = -SEARCH_WINDOW; k <= SEARCH_WINDOW; k++) {
          const i = ((hint + k) % N + N) % N;
          const d = (pts[i].x - x) * (pts[i].x - x) + (pts[i].z - z) * (pts[i].z - z);
          if (d < bestD) { bestD = d; best = i; }
        }
        if (bestD > 40 * 40) best = -1; // nowhere near the hinted stretch: scan everything
      }
      if (best < 0) {
        bestD = Infinity;
        for (let i = 0; i < N; i++) {
          const d = (pts[i].x - x) * (pts[i].x - x) + (pts[i].z - z) * (pts[i].z - z);
          if (d < bestD) { bestD = d; best = i; }
        }
      }
      return best;
    }

    function nearest(x, z, hint) {
      const i = nearestIndex(x, z, hint);
      const prev = (i - 1 + N) % N, next = (i + 1) % N;
      const r1 = project(pts[prev], pts[i], x, z);
      const r2 = project(pts[i], pts[next], x, z);
      let seg, idx;
      if (r1.d2 <= r2.d2) { seg = r1; idx = prev; } else { seg = r2; idx = i; }
      const a = pts[idx], b = pts[(idx + 1) % N];
      const sAlong = (a.s + seg.t * seg.len) % length;
      const lateral = seg.dx * (z - seg.pz) - seg.dz * (x - seg.px);
      return {
        i, s: sAlong, lateral, dx: seg.dx, dz: seg.dz,
        w: a.w + (b.w - a.w) * seg.t, px: seg.px, pz: seg.pz,
        tunnel: pts[i].tunnel, curvature: pts[i].curvature, rampHeight: pts[i].rampHeight,
      };
    }

    function at(sIn) {
      let sv = sIn % length; if (sv < 0) sv += length;
      let lo = 0, hi = N - 1;
      while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (pts[mid].s <= sv) lo = mid; else hi = mid - 1; }
      const a = pts[lo], b = pts[(lo + 1) % N];
      let segLen = b.s - a.s; if (segLen <= 0) segLen += length;
      const t = segLen > 0 ? (sv - a.s) / segLen : 0;
      return {
        x: a.x + (b.x - a.x) * t, z: a.z + (b.z - a.z) * t, w: a.w + (b.w - a.w) * t,
        dx: a.dx, dz: a.dz, curvature: a.curvature, i: lo, tunnel: a.tunnel,
      };
    }

    function isOffRoad(x, z, hint) {
      const nr = nearest(x, z, hint);
      return Math.abs(nr.lateral) > nr.w / 2;
    }

    function gridSlots() {
      const slots = [];
      for (let k = 0; k < 8; k++) {
        const row = Math.floor(k / 2), col = k % 2;
        const sv = length - 14 - row * 8;
        const p = at(sv);
        const lat = col ? 3.2 : -3.2;
        slots.push({ x: p.x - p.dz * lat, z: p.z + p.dx * lat, heading: Math.atan2(p.dz, p.dx), s: sv, hint: p.i });
      }
      return slots;
    }

    return { name: def.name, points: pts, length, ramps, nearest, at, isOffRoad, gridSlots, RAMP_LENGTH, RAMP_HEIGHT };
  }

  return { HARBOR_LOOP, build, catmull, wrapAngle };
});
