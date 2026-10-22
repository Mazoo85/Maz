/*
 * Paints the ground: one big canvas, top-down, the whole of LA. It is the
 * texture on the terrain and, scaled down, the minimap and the full map.
 *
 * Works on any CanvasRenderingContext2D-like object.
 */

import { ROAD_WIDTH } from './geo.js';
import { fbm } from './world.js';

const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
const mix = (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];

const C = {
  deep: hex('#1f5f8b'), shallow: hex('#3f9cc0'), sand: hex('#e6d6ab'), wetSand: hex('#cdb98a'),
  dry: hex('#9a9064'), scrub: hex('#6f7a4a'), green: hex('#5f7f45'), dirt: hex('#a89a72'),
  park: hex('#6f9a4f'), oil: hex('#a69b76'), asphalt: hex('#606265'), airport: hex('#8f918d'),
  valley: hex('#646568')
};

/* The colour of bare ground at (x, z). */
export function groundColor(W, x, z) {
  const h = W.groundHeight(x, z);
  if (W.isSea(x, z)) return mix(C.shallow, C.deep, Math.min(1, -h / 14));
  const shore = W.shoreDist(x, z);
  if (shore < 86 && h < 4.2) return shore < 22 ? C.wetSand : C.sand;
  const s = W.styleAt(x, z);
  const n = fbm(x / 140, z / 140, 3, 9);
  switch (s) {
    case 'wild': case 'hills': {
      const slope = W.slopeAt(x, z);
      let c = mix(C.dry, C.scrub, Math.min(1, Math.max(0, n * 1.6 - 0.35)));
      if (h > 90) c = mix(c, C.green, Math.min(1, (h - 90) / 120));
      if (slope > 0.18) c = mix(c, C.dirt, Math.min(1, (slope - 0.18) * 2.5));
      return c;
    }
    case 'park': return mix(C.park, C.scrub, n * 0.5);
    case 'oilfield': return mix(C.oil, C.dry, n);
    case 'airport': return C.airport;
    default: return mix(C.asphalt, C.valley, n);
  }
}

export function paintGround(W, ctx, size) {
  const S = size / W.size;
  const P = (x) => (x + W.half) * S;

  /* 1. bare ground, sampled coarse and smoothed up */
  const R = Math.min(512, size);
  const tmp = makeCanvas(R, R);
  const tctx = tmp.getContext('2d');
  const img = tctx.createImageData(R, R);
  for (let j = 0; j < R; j++) {
    for (let i = 0; i < R; i++) {
      const x = -W.half + ((i + 0.5) / R) * W.size, z = -W.half + ((j + 0.5) / R) * W.size;
      const c = groundColor(W, x, z);
      const k = (j * R + i) * 4;
      img.data[k] = c[0]; img.data[k + 1] = c[1]; img.data[k + 2] = c[2]; img.data[k + 3] = 255;
    }
  }
  tctx.putImageData(img, 0, 0);
  ctx.imageSmoothingEnabled = true;
  ctx.drawImage(tmp, 0, 0, size, size);

  const rect = (x, z, w, d, rot, color) => {
    ctx.save();
    ctx.translate(P(x), P(z));
    ctx.rotate(-rot);
    ctx.fillStyle = color;
    ctx.fillRect((-w / 2) * S, (-d / 2) * S, w * S, d * S);
    ctx.restore();
  };
  const line = (pts, w, color, dash) => {
    ctx.beginPath();
    pts.forEach(([x, z], i) => (i ? ctx.lineTo(P(x), P(z)) : ctx.moveTo(P(x), P(z))));
    ctx.strokeStyle = color;
    ctx.lineWidth = Math.max(0.6, w * S);
    ctx.lineCap = dash ? 'butt' : 'round';
    ctx.lineJoin = 'round';
    ctx.setLineDash(dash ? dash.map((d) => d * S) : []);
    ctx.stroke();
    ctx.setLineDash([]);
  };
  const op = (o) => {
    switch (o.t) {
      case 'rect': return rect(o.x, o.z, o.w, o.d, o.rot || 0, o.c);
      case 'circle': ctx.beginPath(); ctx.arc(P(o.x), P(o.z), o.r * S, 0, Math.PI * 2); ctx.fillStyle = o.c; return ctx.fill();
      case 'ring': ctx.beginPath(); ctx.arc(P(o.x), P(o.z), o.r * S, 0, Math.PI * 2); ctx.strokeStyle = o.c; ctx.lineWidth = Math.max(0.5, o.w * S); return ctx.stroke();
      case 'ellipse': ctx.beginPath(); ctx.ellipse(P(o.x), P(o.z), o.rx * S, o.rz * S, -(o.rot || 0), 0, Math.PI * 2); ctx.fillStyle = o.c; return ctx.fill();
      case 'line': return line(o.pts, o.w, o.c, o.dash);
      case 'poly': ctx.beginPath(); o.pts.forEach(([x, z], i) => (i ? ctx.lineTo(P(x), P(z)) : ctx.moveTo(P(x), P(z)))); ctx.closePath(); ctx.fillStyle = o.c; return ctx.fill();
    }
  };

  /* 2. city blocks: sidewalk, then the lot */
  for (const b of W.blocks) {
    rect(b.x, b.z, b.w + 3, b.d + 3, b.rot, '#bdb8ad');
    rect(b.x, b.z, b.w - 2, b.d - 2, b.rot, b.color);
  }
  for (const o of W.paint) if (o.layer === 0) op(o);

  /* 3. water features: the river's concrete channel, canals, lakes */
  for (const r of W.rivers) {
    line(r.pts, r.w * 2 + 10, '#b9b5ab');
    line(r.pts, r.w * 2 - 2, '#a7a399');
    line(r.pts, 3, '#5e7f73');
  }
  for (const c of W.canals) line(c, 8, '#3f8fa8');
  for (const l of W.lakes) {
    ctx.beginPath(); ctx.arc(P(l.x), P(l.z), (l.r + 3) * S, 0, Math.PI * 2); ctx.fillStyle = '#7d9a52'; ctx.fill();
    ctx.beginPath(); ctx.arc(P(l.x), P(l.z), l.r * S, 0, Math.PI * 2); ctx.fillStyle = l.tar ? '#141210' : '#3d8fa8'; ctx.fill();
  }

  /* 4. roads, freeways on top */
  const order = ['local', 'blvd', 'freeway'];
  for (const kind of order) {
    for (const r of W.roads.filter((q) => q.kind === kind)) {
      const w = ROAD_WIDTH[kind];
      if (kind === 'blvd') line(r.pts, w + 5, '#bdb8ad');
      if (kind === 'freeway') line(r.pts, w + 4, '#8d8a83');
      line(r.pts, w, kind === 'freeway' ? '#47494c' : kind === 'blvd' ? '#4f5154' : '#6a6b6c');
    }
  }
  for (const r of W.roads) {
    if (r.kind === 'freeway') {
      for (const off of [-4.5, 4.5]) line(offset(r.pts, off), 0.25, '#e8c547');
      for (const off of [-9.5, 9.5]) line(offset(r.pts, off), 0.35, '#f2f2f2', [3, 9]);
    } else if (r.kind === 'blvd') {
      line(offset(r.pts, -0.35), 0.22, '#e8c547');
      line(offset(r.pts, 0.35), 0.22, '#e8c547');
    }
  }
  for (const b of W.bridges) rect(b.x, b.z, b.span, b.w, b.a, '#9c978c');

  /* 5. landmark paint that goes over the roads, pools, building footprints */
  for (const o of W.paint) if (o.layer === 1) op(o);
  for (const p of W.pools) { rect(p.x, p.z, p.w + 1.4, p.d + 1.4, p.rot, '#eeeae0'); rect(p.x, p.z, p.w, p.d, p.rot, '#48c0e6'); }
  /* 6. baked ambient occlusion: soft dark halos where walls meet the
   * ground and under the trees, layered to fake a blur */
  for (const [grow, a] of [[7, 0.06], [4.5, 0.07], [2.5, 0.09], [1, 0.12]]) {
    ctx.fillStyle = `rgba(20,16,10,${a})`;
    for (const b of W.buildings) {
      if (b.h < 2.5) continue;
      ctx.save(); ctx.translate(P(b.x), P(b.z)); ctx.rotate(-b.rot);
      const g = grow * Math.min(1.6, 0.6 + b.h / 30);
      ctx.fillRect((-b.w / 2 - g) * S, (-b.d / 2 - g) * S, (b.w + 2 * g) * S, (b.d + 2 * g) * S);
      ctx.restore();
    }
  }
  ctx.fillStyle = 'rgba(20,24,10,0.18)';
  for (const t of W.trees) {
    if (t.t === 0 || t.t === 4) continue;
    const r = (t.t === 3 ? t.h * 0.8 : t.h * 0.45) * S;
    ctx.beginPath(); ctx.arc(P(t.x), P(t.z), Math.max(0.6, r), 0, Math.PI * 2); ctx.fill();
  }
  for (const b of W.buildings) rect(b.x, b.z, b.w, b.d, b.rot, shade(b.roof ? b.roofColor : b.color, 0.82));
}

/* The city at night from above: every street a line of sodium-orange
 * light, brightest Downtown and on the boulevards. The terrain uses it as
 * its glow map once the sun goes down. */
export function paintNight(W, ctx, size) {
  const S = size / W.size;
  const P = (x) => (x + W.half) * S;
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, size, size);
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  // the streets between blocks
  for (const b of W.blocks) {
    const bright = b.style === 'downtown' || b.style === 'hollywood' || b.style === 'towers' ? 0.9 : b.style === 'midrise' || b.style === 'mixed' || b.style === 'coastal' ? 0.6 : 0.4;
    ctx.save();
    ctx.translate(P(b.x), P(b.z));
    ctx.rotate(-b.rot);
    ctx.strokeStyle = `rgba(255,${150 + bright * 60},${70 + bright * 60},${bright})`;
    ctx.lineWidth = Math.max(1, 5 * S);
    ctx.strokeRect((-b.w / 2 - 4) * S, (-b.d / 2 - 4) * S, (b.w + 8) * S, (b.d + 8) * S);
    ctx.restore();
  }
  for (const r of W.roads) {
    const w = ROAD_WIDTH[r.kind];
    ctx.beginPath();
    r.pts.forEach(([x, z], i) => (i ? ctx.lineTo(P(x), P(z)) : ctx.moveTo(P(x), P(z))));
    ctx.strokeStyle = r.kind === 'freeway' ? 'rgba(255,190,120,0.75)' : r.kind === 'blvd' ? 'rgba(255,165,80,0.85)' : 'rgba(255,160,70,0.5)';
    ctx.lineWidth = Math.max(1.2, w * 1.1 * S);
    ctx.stroke();
  }
  // lit pools glow turquoise in the hills
  for (const p of W.pools) { ctx.fillStyle = 'rgba(80,220,255,0.8)'; ctx.fillRect(P(p.x) - 1, P(p.z) - 1, 2, 2); }
}

function offset(pts, d) {
  return pts.map((p, i) => {
    const a = pts[Math.max(0, i - 1)], b = pts[Math.min(pts.length - 1, i + 1)];
    const L = Math.hypot(b[0] - a[0], b[1] - a[1]) || 1;
    return [p[0] - ((b[1] - a[1]) / L) * d, p[1] + ((b[0] - a[0]) / L) * d];
  });
}

function shade(h, k) {
  const c = hex(h);
  return `rgb(${(c[0] * k) | 0},${(c[1] * k) | 0},${(c[2] * k) | 0})`;
}

function makeCanvas(w, h) {
  if (typeof OffscreenCanvas !== 'undefined') return new OffscreenCanvas(w, h);
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return c;
}
