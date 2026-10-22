/*
 * Los Angeles architecture, at real dimensions.
 *
 * The generic city used to be boxes. This builds what LA actually is, lot by
 * lot: Spanish Colonial Revival and Craftsman bungalows on 50 ft (15 m) lots
 * with a driveway down one side to a garage at the back; Valley ranch houses
 * with attached garages; 1950s dingbat apartments up on their carports; beach
 * houses on Venice's narrow lots; hillside moderns on stilts; Beverly Hills
 * villas behind their hedges; storefronts built to the sidewalk with awnings,
 * mini-malls behind striped parking lots, gas stations on the corners; Art
 * Deco midrises; Downtown towers on their podiums; sawtooth-roofed
 * warehouses; brick Romanesque campus halls.
 *
 * Every building is a few wall volumes (W.buildings: the renderer gives them
 * windows and a wall surface) plus detail parts (roofs, porches, awnings,
 * fences…) in the material classes the renderer textures.
 *
 * Distances are metres. A lot frame has +z pointing at the street.
 */

const SPANISH = ['#f4ede0', '#efe4cf', '#f2e6d0', '#e9dcc2', '#f7f2e8', '#eadbc6', '#f1e3d3'];
const TILE = ['#b5562f', '#a8492b', '#c0663b', '#9b4a2f'];
const SIDING = ['#6e7b5c', '#7a6a4f', '#8a5a3b', '#4f5d6b', '#a39274', '#6b4e3a', '#c2b28a', '#5b6e6a'];
const SHINGLE = ['#5a5650', '#4b4d50', '#6b5e50', '#3f4246'];
const RANCH = ['#e7dccb', '#d8cdb8', '#cfd8d6', '#e9e2c9', '#c9bba3', '#f0ece2', '#dfe3e6'];
const ACCENT = ['#f2b5c4', '#9fd8cb', '#f4d06f', '#7fb7d9', '#f28f6b', '#b9e3a8', '#e9e4da'];
const BEACH = ['#f6f1e4', '#9fd3d6', '#f4d06f', '#f28f6b', '#a8d5a2', '#ffffff', '#7fb7d9', '#e2e7ea', '#3c4a57'];
const SHOP = ['#e7dccb', '#d9c7a7', '#f0e2a8', '#c9d6c2', '#f2c6b4', '#e9e4da', '#b0a28a', '#9a5a43', '#d6e0e8', '#f4ead5'];
const AWNING = ['#b3262b', '#1f5f3f', '#1d3f8a', '#d9a21b', '#6b2a5c', '#2a2a2a', '#e3e0d6', '#0a7f8c'];
const DECO = ['#e7dcc4', '#d9c7a7', '#c9b48f', '#efe6d2', '#5f8f86', '#d8cbb3'];
const GLASS = ['#6f8797', '#4d6475', '#8aa1ad', '#3e4b56', '#5d6f7e', '#7d8f88'];
const STONE = ['#b8a88f', '#a39582', '#c4b59c', '#8b7d6b'];
const BRICK = ['#9a5a43', '#8b4a37', '#a76b4d', '#7d4434'];
const DINGBAT_NAMES = ['The Capri', 'The Flamingo', 'Casa Grande', 'The Tahitian', 'Sea Breeze', 'The Islander', 'Kona Kai', 'The Driftwood',
  'Palm Terrace', 'The Aloha', 'Villa Rosa', 'The Starlite', 'Lanai Gardens', 'The Pagoda', 'Royal Hawaiian', 'The Monaco', 'The Riviera', 'Shangri-La'];

/* Which houses a neighbourhood is made of: [kind, weight]. */
const HOUSES = {
  craftsman: ['Highland Park', 'Echo Park', 'Silver Lake', 'Los Feliz', 'South LA', 'Leimert Park', 'Mid-City', 'Pasadena', 'Glendale'],
  dingbat: { 'Mar Vista': 0.3, 'Mid-City': 0.2, 'Silver Lake': 0.12, 'Echo Park': 0.12, Inglewood: 0.12, Westchester: 0.08, 'Santa Monica': 0.25, 'Culver City': 0.2, Hollywood: 0.3, 'West Hollywood': 0.3 },
  ranch: { Westchester: 0.55, Hawthorne: 0.45, Gardena: 0.5, Compton: 0.35, 'Baldwin Hills': 0.4, Inglewood: 0.3 }
};

export function makeArchitecture(ctx) {
  const { W, rnd, part, building, canPlace, tree, car, gh, signFor, MASK, paint } = ctx;
  // each text part is its own mesh: keep a budget per kind
  const texts = { name: 0, price: 0 }, BUDGET = { name: 60, price: 30 };

  /* A local frame: origin, yaw; lot frames have +z towards the street. */
  function frame(x0, z0, yaw) {
    const c = Math.cos(yaw), s = Math.sin(yaw);
    const at = (lx, lz) => [x0 + lx * c + lz * s, z0 - lx * s + lz * c];
    return {
      yaw, at,
      ground: (lx, lz) => gh(...at(lx, lz)),
      /* a box with its bottom at y */
      box(lx, y, lz, sx, sy, sz, col, o = {}) {
        const [x, z] = at(lx, lz);
        if (o.clear) {
          // edge pieces (fences, hedges, docks) only where there's no road: check both ends
          const { clear, ...rest } = o;
          void clear;
          const ends = sx > sz ? [at(lx - sx / 2, lz), at(lx + sx / 2, lz)] : [at(lx, lz - sz / 2), at(lx, lz + sz / 2)];
          if ([[x, z], ...ends].some(([px, pz]) => W.maskAt(px, pz) & (MASK.ROAD | MASK.RESERVED))) return null;
          o = rest;
        }
        return part({ s: 'box', x, y: y + sy / 2, z, sx, sy, sz, c: col, ...o, ry: yaw + (o.ry || 0) });
      },
      /* shapes whose base sits at y: gable, roof (hip), arch, dome */
      base(shape, lx, y, lz, sx, sy, sz, col, o = {}) {
        const [x, z] = at(lx, lz);
        return part({ s: shape, x, y, z, sx, sy, sz, c: col, ...o, ry: yaw + (o.ry || 0) });
      },
      cyl(lx, y, lz, d, h, col, o = {}) {
        const [x, z] = at(lx, lz);
        return part({ s: 'cyl', x, y: y + h / 2, z, sx: d, sy: h, sz: d, c: col, ...o });
      },
      paint(lx, lz, w, d, col, layer = 0) {
        const [x, z] = at(lx, lz);
        paint({ t: 'rect', x, z, w, d, rot: yaw, c: col }, layer);
      },
      can(lx, lz, w, d, slope = 3) { const [x, z] = at(lx, lz); return canPlace(x, z, w / 2, d / 2, yaw, slope); },
      bld(lx, lz, w, d, h, col, win, g, extra = {}) {
        const [x, z] = at(lx, lz);
        const b = building(x, z, w, d, h, yaw, col, win, extra.roof || null, extra.roofColor || null, g);
        if (extra.mat) b.mat = extra.mat;
        if (extra.noLip) b.noLip = true;
        return b;
      },
      text(lx, y, lz, w, h, str, col, o = {}, kind = 'name') {
        if (texts[kind] >= BUDGET[kind]) return null;
        texts[kind]++;
        const [x, z] = at(lx, lz);
        return part({ s: 'text', x, y, z, sx: w, sy: h, sz: 1, text: str, c: col, ...o, ry: yaw + (o.ry || 0) });
      },
      sign(lx, y, lz, w, district, ry = 0) {
        const [x, z] = at(lx, lz);
        W.signs.push({ x, y, z, ry: yaw + ry, w, h: 1.4, cell: signFor(district), look: rnd.int(0, 7) });
      },
      tree(lx, lz, t, h) {
        const [x, z] = at(lx, lz);
        if (!(W.maskAt(x, z) & (MASK.ROAD | MASK.RESERVED)) && !W.isSea(x, z)) tree(x, z, t, h);
      },
      car(lx, lz, ry, kind) {
        const [x, z] = at(lx, lz);
        if (!(W.maskAt(x, z) & (MASK.RESERVED | MASK.CHANNEL))) car(x, z, yaw + ry, rnd.pick(ctx.carColors), kind);
      }
    };
  }

  /* ------------------------------------------------------- lot dressing */

  /* Something along the front lot line: low stucco wall, picket, wrought
   * iron, hedge, chain-link, or nothing. Leaves a gap for the driveway. */
  function frontFence(F, W2, D2, gapX0, gapX1, style) {
    const kinds = { none: [0, 0, 0, null, null], wall: [0.9, 0.3, 0, 'stucco', '#efe6d6'], picket: [1.0, 0.06, 0, 'wood', '#f4f1ea'],
      iron: [1.3, 0.05, 0, 'metal', '#1e1e1e'], hedge: [1.3, 0.9, 0, 'foliage', '#3f6b34'], chain: [1.2, 0.04, 0, 'metal', '#9aa0a3'] };
    const k = style || rnd.pick(['none', 'none', 'wall', 'picket', 'iron', 'hedge', 'chain', 'wall', 'hedge']);
    const [h, t, , m, c] = kinds[k];
    if (!m) return;
    const y = F.ground(0, D2 - 0.4);
    const segs = [[-W2 + 0.2, gapX0], [gapX1, W2 - 0.2]];
    for (const [a, b] of segs) {
      if (b - a < 0.5) continue;
      F.box((a + b) / 2, y, D2 - 0.4, b - a, h, t, c, { m, clear: true });
      if (k === 'picket' || k === 'iron') for (let px = a; px <= b; px += 2.4) F.box(px, y, D2 - 0.4, 0.12, h + 0.15, 0.12, c, { m, clear: true });
    }
  }

  /* A detached garage at the back of the lot, with its door to the drive. */
  function garage(F, gx, gz, kind = 'wood') {
    const g = F.can(gx, gz, 6.2, 6.2, 2);
    if (!g) return;
    const col = rnd.pick(SPANISH);
    F.bld(gx, gz, 6, 6, 2.8, col, 0, g, { noLip: true, mat: 'stucco' });
    F.base('gable', gx, g.hi + 2.8, gz, 6.3, 1.2, 6.3, rnd.pick([...TILE, ...SHINGLE]), { m: rnd.chance(0.5) ? 'tile' : 'asphalt' });
    F.box(gx, g.hi, gz + 3.03, 4.8, 2.2, 0.08, kind === 'wood' ? '#8a6f55' : '#e8e6e0', { m: kind === 'wood' ? 'wood' : 'metal' });
  }

  /* --------------------------------------------------------- the houses */

  /* A single-family lot: house, driveway down one side, garage at the back,
   * walk to the front door, maybe a pool, a fence, trees. */
  function house(blk, F, Wl, D, kind) {
    const W2 = Wl / 2, D2 = D / 2;
    const driveRight = rnd.chance(0.5);
    const ds = driveRight ? 1 : -1;
    const setback = kind === 'ranch' ? 7.5 : 6;
    const hw = kind === 'ranch' ? Math.min(Wl - 2.6, rnd.range(16, 19)) : Math.min(Wl - 4.8, rnd.range(9.5, 11.5));
    const hd = rnd.range(13, 16.5);
    const hx = kind === 'ranch' ? 0 : -ds * (W2 - 1.3 - hw / 2);
    const hz = D2 - setback - hd / 2;
    const g = F.can(hx, hz, hw, hd, 3);
    if (!g || !F.can(hx, hz + 2, hw, hd + 4, 4)) return false;      // room for the porch too
    if (kind === 'craftsman') craftsman(F, hx, hz, hw, hd, g);
    else if (kind === 'ranch') ranch(F, hx, hz, hw, hd, g, ds);
    else spanish(F, hx, hz, hw, hd, g);
    const lot = blk.color;
    void lot;
    // the driveway (ranch: wide, straight to the attached garage)
    if (kind === 'ranch') {
      const gx = ds * (hw / 2 - 3.3);
      F.paint(gx, D2 - setback / 2, 6, setback, '#b9b4a8');
      if (rnd.chance(0.5)) F.car(gx + 1.4, D2 - setback / 2, Math.PI / 2, rnd.pick(['sedan', 'suv', 'pickup']));
    } else {
      const dx = ds * (W2 - 1.7);
      const dlen = D - 8;
      F.paint(dx, D2 - dlen / 2, 3, dlen, '#b9b4a8');
      if (rnd.chance(0.7)) garage(F, ds * (W2 - 3.4), -D2 + 4, rnd.chance(0.5) ? 'wood' : 'rollup');
      if (rnd.chance(0.35)) F.car(dx, D2 - 9, Math.PI / 2, rnd.pick(['sedan', 'compact', 'suv', 'pickup']));
      F.paint(hx, D2 - setback / 2, 1.2, setback, '#c9c4b8');
    }
    // backyard pool
    if (rnd.chance(kind === 'ranch' ? 0.4 : 0.2)) {
      const [px, pz] = F.at(hx, hz - hd / 2 - 5);
      W.pools.push({ x: px, z: pz, w: Math.min(hw - 2, 9), d: 4.5, rot: F.yaw });
    }
    // trees: one in front, one behind, a parkway tree at the kerb
    if (rnd.chance(0.6)) F.tree(-hx * 0.3 + rnd.range(-2, 2), D2 - setback * 0.5, rnd.pick([1, 1, 2, 0, 4, 5]), rnd.range(7, 12));
    if (rnd.chance(0.5)) F.tree(hx + rnd.range(-3, 3), -D2 + 6, rnd.pick([1, 5, 0, 2]), rnd.range(8, 14));
    if (rnd.chance(0.45)) F.tree(rnd.range(-W2 + 2, W2 - 2), D2 + 1.6, rnd.pick([0, 1, 2]), rnd.range(8, 22));
    const gap0 = kind === 'ranch' ? ds * (hw / 2 - 6.6) : ds * (W2 - 3.4), gap1 = kind === 'ranch' ? ds * (hw / 2) : ds * W2;
    frontFence(F, W2, D2, Math.min(gap0, gap1), Math.max(gap0, gap1));
    return true;
  }

  /* Spanish Colonial Revival: cream stucco, red tile, no eaves, an arched
   * picture window, an arched entry porch, a chimney, maybe a little tower. */
  function spanish(F, x, z, w, d, g) {
    const wall = rnd.pick(SPANISH), tile = rnd.pick(TILE);
    const h = rnd.range(3.2, 3.6);
    F.bld(x, z, w, d, h, wall, 1, g, { noLip: true, mat: 'stucco' });
    const top = g.hi + h;
    if (rnd.chance(0.55)) F.base('gable', x, top, z, w + 0.15, rnd.range(1.6, 2.2), d + 0.15, tile, { m: 'tile' });
    else F.base('roof', x, top, z, w + 0.2, rnd.range(1.8, 2.4), d + 0.2, tile, { m: 'tile' });
    const fz = z + d / 2;
    // the entry: a projecting porch with a round arch
    const ex = x + (rnd.chance(0.5) ? 1 : -1) * (w / 2 - 2);
    const eg = g.hi;
    F.box(ex, eg - 0.3, fz + 1.2, 3.2, h + 0.3, 2.4, wall, { m: 'stucco' });
    F.base('gable', ex, eg + h, fz + 1.2, 3.4, 1.1, 2.6, tile, { m: 'tile', ry: Math.PI / 2 });
    F.base('arch', ex, eg, fz + 2.42, 1.9, 2.6, 0.06, '#2e241d', { m: 'matte' });
    // the arched picture window, with its wrought-iron grille
    const wx = x - (ex - x) * 0.9;
    F.base('arch', wx, eg + 0.8, fz + 0.04, 2.2, 2.2, 0.06, '#26323b', { m: 'glass' });
    for (let k = -2; k <= 2; k++) F.box(wx + k * 0.42, eg + 0.8, fz + 0.12, 0.04, 1.8, 0.04, '#1b1b1b', { m: 'metal' });
    // chimney, and on some a little tower with a tile cap
    const cx = x + (rnd.chance(0.5) ? 1 : -1) * (w / 2 - 0.5);
    F.box(cx, eg, z - d * 0.15, 1.0, h + 2.6, 1.0, wall, { m: 'stucco' });
    F.base('roof', cx, eg + h + 2.6, z - d * 0.15, 1.3, 0.5, 1.3, tile, { m: 'tile' });
    if (rnd.chance(0.25)) {
      F.box(ex, eg + h, fz - 0.5, 2, 1.8, 2, wall, { m: 'stucco' });
      F.base('roof', ex, eg + h + 1.8, fz - 0.5, 2.4, 1.2, 2.4, tile, { m: 'tile' });
    }
    // a few steps up to the porch
    F.box(ex, eg - 0.2, fz + 2.9, 2, 0.4, 1, '#c9c2b4', { m: 'concrete' });
  }

  /* Craftsman bungalow: wood siding, a low gable with deep eaves facing the
   * street, a full-width porch on tapered columns over stone piers. */
  function craftsman(F, x, z, w, d, g) {
    const wall = rnd.pick(SIDING), trim = rnd.pick(['#efe9da', '#e6dcc4', '#2f3a33', '#f4f1ea']), roof = rnd.pick(SHINGLE);
    const h = rnd.range(3.1, 3.5);
    F.bld(x, z, w, d, h, wall, 1, g, { noLip: true, mat: 'wood' });
    const top = g.hi + h;
    F.base('gable', x, top, z, d + 1.6, rnd.range(1.8, 2.4), w + 1.6, roof, { m: 'asphalt', ry: Math.PI / 2 });
    F.box(x, top - 0.25, z + d / 2 + 0.75, w + 1.6, 0.25, 0.1, trim, { m: 'wood' });  // fascia
    // the porch: deck, piers, tapered columns, its own low gable
    const fz = z + d / 2, pd = 2.6;
    F.box(x, g.hi - 0.3, fz + pd / 2, w, 0.75, pd, '#9c958a', { m: 'concrete', col: true });
    for (const s of [-1, 1]) {
      const px = x + s * (w / 2 - 0.4);
      F.box(px, g.hi + 0.45, fz + pd - 0.4, 0.7, 0.9, 0.7, rnd.pick(['#8a8077', '#7d4434', '#9b8f80']), { m: rnd.chance(0.5) ? 'brick' : 'concrete' });
      F.box(px, g.hi + 1.35, fz + pd - 0.4, 0.34, h - 1.35, 0.34, trim, { m: 'wood' });
    }
    F.base('gable', x, g.hi + h - 0.2, fz + pd / 2, w + 0.8, 1.1, pd + 0.8, roof, { m: 'asphalt' });
    F.box(x, g.hi + 0.45, fz + 0.06, 1.1, 2.2, 0.05, '#5b3f2c', { m: 'wood' });           // front door
    F.box(x - w * 0.28, g.hi + 1.1, fz + 0.06, 2.2, 1.4, 0.05, '#26323b', { m: 'glass' });
    F.box(x + w * 0.28, g.hi + 1.1, fz + 0.06, 2.2, 1.4, 0.05, '#26323b', { m: 'glass' });
    F.box(x + w * 0.3, g.hi, z - d * 0.2, 0.9, h + 2.2, 0.9, '#8b4a37', { m: 'brick' });  // chimney
  }

  /* Mid-century ranch: long and low under a shallow hip roof, two-car
   * garage at one end facing the street, a big picture window. */
  function ranch(F, x, z, w, d, g, ds) {
    const wall = rnd.pick(RANCH), roof = rnd.pick(SHINGLE);
    const h = rnd.range(2.8, 3.1);
    F.bld(x, z, w, d, h, wall, 1, g, { noLip: true, mat: rnd.chance(0.3) ? 'wood' : 'stucco' });
    const top = g.hi + h;
    F.base('roof', x, top, z, w + 1.4, rnd.range(1.3, 1.8), d + 1.4, roof, { m: 'asphalt' });
    const fz = z + d / 2;
    const gx = x + ds * (w / 2 - 3.3);
    F.box(gx, g.hi, fz + 0.05, 5, 2.2, 0.08, rnd.pick(['#efece4', '#d9d3c5', '#8a6f55']), { m: rnd.chance(0.6) ? 'metal' : 'wood' });
    F.box(x - ds * w * 0.18, g.hi + 0.8, fz + 0.05, 4.2, 1.6, 0.06, '#26323b', { m: 'glass' });
    F.box(x - ds * w * 0.42, g.hi, fz + 0.05, 1, 2.1, 0.06, rnd.pick(['#b3262b', '#1d3f8a', '#f4d06f', '#2f3a33']), { m: 'wood' });
    if (rnd.chance(0.3)) {                            // breeze-block screen
      F.box(x - ds * (w / 2 + 0.2), g.hi, fz - 2, 0.25, 1.8, 4, '#e6e1d6', { m: 'concrete' });
    }
    if (rnd.chance(0.3)) {                            // solar on the street-facing slope
      F.box(x - ds * w * 0.15, top + 0.55, z + d * 0.25, w * 0.4, 0.06, d * 0.3, '#1d2a44', { m: 'glass', rx: 0.3 });
    }
  }

  /* Dingbat: two storeys of stucco box up on posts over open carports,
   * a contrasting sheet on the front, a starburst, a name in script. */
  function dingbat(blk, F, Wl, D) {
    const w = Math.min(Wl - 1.5, 15), d = Math.min(D - 8, rnd.range(20, 24));
    const z = D / 2 - 2.5 - d / 2;
    const g = F.can(0, z, w, d, 2);
    if (!g) return false;
    const wall = rnd.pick(['#f4f1ea', '#efe6d2', '#e9e4da', '#f2e6d0']), accent = rnd.pick(ACCENT);
    const ground = 2.7, up = 5.8;
    // the enclosed back half on the ground floor, then the floating box
    F.bld(0, z - d / 4, w, d / 2, ground, wall, 1, g, { noLip: true, mat: 'stucco' });
    F.box(0, g.hi + ground, z, w, up, d, wall, { m: 'win', win: 1, col: true });
    F.box(0, g.hi + ground + up, z, w + 0.3, 0.35, d + 0.3, wall, { m: 'stucco' });
    for (let px = -w / 2 + 0.3; px <= w / 2 - 0.2; px += w / 4) F.box(px, g.hi, z + d / 2 - 0.3, 0.25, ground, 0.25, '#d9d4c8', { m: 'metal', col: true });
    for (let k = 0; k < 3; k++) if (rnd.chance(0.6)) F.car(-w / 2 + 2.5 + k * 4.8, z + d / 4, -Math.PI / 2, rnd.pick(['sedan', 'compact', 'suv']));
    // the front: a sheet of contrasting stucco, the starburst, the name
    const fz = z + d / 2 + 0.06;
    F.box(-w * 0.18, g.hi + ground, fz, w * 0.55, up, 0.08, accent, { m: 'stucco' });
    const sx = w * 0.25, sy = g.hi + ground + up * 0.55;
    for (let k = 0; k < 4; k++) F.box(sx, sy, fz + 0.06, 2.4, 0.08, 0.06, '#c9a54a', { m: 'metal', rz: (k * Math.PI) / 4 });
    F.text(-w * 0.18, g.hi + ground + up * 0.5, fz + 0.12, Math.min(w * 0.5, 7), 1.4, rnd.pick(DINGBAT_NAMES), '#1b1b1b', { font: 60 });
    // the open stair and walkway along one side
    F.box(w / 2 + 0.6, g.hi + ground + 2.6, z, 1.2, 0.2, d * 0.8, '#d9d4c8', { m: 'concrete' });
    F.box(w / 2 + 1.15, g.hi + ground + 2.8, z, 0.05, 1.0, d * 0.8, '#1e1e1e', { m: 'metal' });
    F.paint(0, z + d / 4 + 1.5, w, d / 2 + 3, '#9c978c');
    return true;
  }

  /* Venice / Manhattan Beach: narrow lots, two or three storeys, boxy
   * modern or cottage, a roof deck with glass railing, little setback. */
  function beachHouse(F, Wl, D) {
    const w = Wl - 1.2, d = Math.min(D - 6, rnd.range(14, 20));
    const z = D / 2 - 2.5 - d / 2;
    const g = F.can(0, z, w, d, 2);
    if (!g) return false;
    const col = rnd.pick(BEACH);
    const cottage = rnd.chance(0.3);
    const h = cottage ? rnd.range(4.5, 6) : rnd.range(7, 10);
    F.bld(0, z, w, d, h, col, 1, g, { noLip: true, mat: rnd.chance(0.35) ? 'wood' : 'stucco' });
    const top = g.hi + h;
    if (cottage) F.base('gable', 0, top, z, d + 0.4, 2.2, w + 0.4, rnd.pick(SHINGLE), { m: 'asphalt', ry: Math.PI / 2 });
    else {
      for (const [lx, lz, sx, sz] of [[0, d / 2, w, 0.06], [0, -d / 2, w, 0.06], [w / 2, 0, 0.06, d], [-w / 2, 0, 0.06, d]]) {
        F.box(lx, top, z + lz, sx, 1.05, sz, '#bcd6de', { m: 'glass' });
      }
      F.box(0, top, z - d * 0.3, w * 0.6, 2.4, 3, col, { m: 'stucco' });   // stair hut
    }
    // a front balcony and a big window
    F.box(0, g.hi + 3.2, z + d / 2 + 0.8, w * 0.8, 0.2, 1.6, '#d9d4c8', { m: 'concrete' });
    F.box(0, g.hi + 3.4, z + d / 2 + 1.55, w * 0.8, 1.0, 0.05, '#bcd6de', { m: 'glass' });
    F.box(0, g.hi + 3.6, z + d / 2 + 0.05, w * 0.6, 2.2, 0.06, '#2a3642', { m: 'glass' });
    if (rnd.chance(0.4)) F.tree(w / 2 - 0.5, D / 2 - 1.2, rnd.pick([0, 0, 1]), rnd.range(6, 14));
    return true;
  }

  /* A Beverly Hills / Bel Air estate: a two-storey villa (or Georgian) with
   * wings, behind a tall hedge with a gate, a motor court, pool, palms. */
  function mansion(F, Wl, D) {
    const W2 = Wl / 2, D2 = D / 2;
    const w = Math.min(Wl - 12, rnd.range(24, 32)), d = rnd.range(15, 19);
    const z = D2 - 16 - d / 2;
    const g = F.can(0, z, w + 16, d, 5);
    if (!g) return false;
    const georgian = rnd.chance(0.3);
    const wall = georgian ? rnd.pick(['#8b4a37', '#f4f1ea', '#e9e4da']) : rnd.pick(SPANISH);
    const roofC = georgian ? '#4a4f55' : rnd.pick(TILE), roofM = georgian ? 'asphalt' : 'tile';
    const mat = georgian && wall === '#8b4a37' ? 'brick' : 'stucco';
    F.bld(0, z, w, d, 7.2, wall, 1, g, { noLip: true, mat });
    F.base('roof', 0, g.hi + 7.2, z, w + 0.8, 3, d + 0.8, roofC, { m: roofM });
    for (const s of [-1, 1]) {
      if (!rnd.chance(0.75)) continue;
      const wx = s * (w / 2 + 5);
      F.bld(wx, z - 1, 10, d - 3, 4.2, wall, 1, g, { noLip: true, mat });
      F.base('roof', wx, g.hi + 4.2, z - 1, 10.6, 2, d - 2.4, roofC, { m: roofM });
    }
    const fz = z + d / 2;
    if (georgian) {
      for (let k = -2; k <= 2; k++) F.cyl(k * 1.6, g.hi, fz + 1.4, 0.55, 6.6, '#f4f1ea', { m: 'plaster' });
      F.box(0, g.hi + 6.6, fz + 1.4, 8, 0.6, 3, '#f4f1ea', { m: 'plaster' });
      F.base('gable', 0, g.hi + 7.2, fz + 1.4, 8.4, 1.6, 3.2, '#f4f1ea', { m: 'plaster' });
    } else {
      for (let k = -1; k <= 1; k++) F.base('arch', k * 3.2, g.hi, fz + 0.05, 2.4, 3.4, 0.1, '#2e241d', { m: 'matte' });
      F.box(0, g.hi + 3.8, fz + 1.1, 10, 0.2, 2.2, wall, { m: 'stucco' });
      for (let k = -2; k <= 2; k++) F.box(k * 2.2, g.hi + 4.0, fz + 2.15, 0.06, 1.0, 0.06, '#1b1b1b', { m: 'metal' });
    }
    // motor court with a fountain, drive to the gate, hedge along the street
    F.paint(0, D2 - 9, 18, 14, '#c9bfae');
    F.cyl(0, g.hi, D2 - 9, 3.2, 0.7, '#d9d4c8', { m: 'concrete' });
    F.cyl(0, g.hi + 0.7, D2 - 9, 0.4, 1.4, '#d9d4c8', { m: 'concrete' });
    const hy = F.ground(0, D2 - 0.8);
    for (const [a, b] of [[-W2 + 0.5, -3], [3, W2 - 0.5]]) F.box((a + b) / 2, hy, D2 - 0.8, b - a, 3.6, 1.4, '#3a5f30', { m: 'foliage', col: true, clear: true });
    for (const s of [-1, 1]) F.box(s * 3, hy, D2 - 0.8, 0.6, 3.2, 0.6, '#d9d4c8', { m: 'stucco', clear: true });
    F.box(0, hy, D2 - 0.8, 5.4, 2.4, 0.08, '#1b1b1b', { m: 'metal', clear: true });
    const [px, pz] = F.at(0, z - d / 2 - 8);
    W.pools.push({ x: px, z: pz, w: 14, d: 6, rot: F.yaw });
    for (const s of [-1, 1]) F.tree(s * (w / 2 + 2), z - d / 2 - 8, 0, rnd.range(18, 26));
    if (rnd.chance(0.6)) F.car(-4, D2 - 9, 0.3, rnd.pick(['sedan', 'suv']));
    return true;
  }

  /* Hollywood Hills / Laurel Canyon / Bel Air: a glass-and-stucco box
   * cantilevered off the slope on steel posts, with a pool deck. */
  function hillside(x, z, a) {
    const F = frame(x, z, a);
    const w = rnd.range(14, 20), d = rnd.range(10, 14);
    const g = F.can(0, 0, w, d, 9);
    if (!g) return false;
    const modern = rnd.chance(0.6);
    const floor = g.hi - 0.2;
    const h = modern ? rnd.range(6, 7) : rnd.range(5.5, 6.5);
    const wall = modern ? rnd.pick(['#f4f1ea', '#e9e4da', '#3c3f43', '#c9c2b4']) : rnd.pick(SPANISH);
    F.box(0, floor, 0, w, h, d, wall, { m: 'win', win: 1, col: true });
    if (modern) {
      F.box(0, floor + h, 0, w + 2, 0.35, d + 2, '#e9e4da', { m: 'concrete' });
      F.box(0, floor + 0.3, d / 2 + 0.05, w * 0.8, h - 0.8, 0.06, '#2a3642', { m: 'glass' });
    } else F.base('roof', 0, floor + h, 0, w + 0.3, 2.4, d + 0.3, rnd.pick(TILE), { m: 'tile' });
    // the posts down to the slope where the ground falls away
    for (const [lx, lz] of [[-w / 2 + 0.5, -d / 2 + 0.5], [w / 2 - 0.5, -d / 2 + 0.5], [-w / 2 + 0.5, d / 2 - 0.5], [w / 2 - 0.5, d / 2 - 0.5], [0, d / 2 - 0.5], [0, -d / 2 + 0.5]]) {
      const gy = F.ground(lx, lz);
      if (floor - gy > 0.6) F.box(lx, gy - 0.5, lz, 0.3, floor - gy + 0.5, 0.3, '#3d4146', { m: 'metal' });
    }
    if (rnd.chance(0.65) && F.can(0, d / 2 + 4, w, 8, 30)) {          // pool deck off the front
      const dz = d / 2 + 4;
      F.box(0, floor - 0.4, dz, w, 0.4, 8, '#d9d4c8', { m: 'concrete', col: true });
      F.box(0, floor - 0.02, dz, w * 0.6, 0.05, 4, '#48c0e6', { m: 'glass' });
      F.box(0, floor, dz + 4, w, 1.05, 0.05, '#bcd6de', { m: 'glass' });
      const gy = F.ground(0, dz + 3.6);
      if (floor - gy > 0.6) for (const s of [-1, 1]) F.box(s * (w / 2 - 0.5), gy - 0.5, dz + 3.6, 0.3, floor - gy + 0.1, 0.3, '#3d4146', { m: 'metal' });
    }
    for (let k = 0; k < 2; k++) F.tree(rnd.range(-w, w), rnd.range(-d * 1.5, -d), rnd.pick([4, 1, 5, 0]), rnd.range(8, 16));
    return true;
  }

  /* ---------------------------------------------------------- commercial */

  /* A storefront built to the sidewalk: parapet and cornice, an awning,
   * shop windows, a sign. One or two storeys. */
  function storefront(blk, F, Wl, D, opts = {}) {
    const w = Wl - 0.3, d = Math.min(D - 2, rnd.range(15, 22));
    const z = D / 2 - 0.6 - d / 2;
    const g = F.can(0, z, w, d, 3);
    if (!g) return false;
    const two = rnd.chance(opts.twoP ?? 0.35);
    const h = two ? rnd.range(7.4, 8.6) : rnd.range(4.6, 5.6);
    const col = rnd.pick(SHOP);
    const mat = BRICK.includes(col) || col === '#9a5a43' ? 'brick' : 'stucco';
    F.bld(0, z, w, d, h, col, 2, g, { noLip: true, mat });
    const top = g.hi + h, fz = z + d / 2;
    F.box(0, top, fz - 0.18, w, rnd.range(0.9, 1.6), 0.36, col, { m: mat });                 // false-front parapet
    F.box(0, top - 0.2, fz + 0.05, w + 0.2, 0.35, 0.5, '#e9e4da', { m: 'concrete' });         // cornice
    F.box(0, top, z - d / 2 + 0.15, w, 0.6, 0.3, col, { m: mat });
    F.box(w / 2 - 0.15, top, z, 0.3, 0.6, d, col, { m: mat });
    F.box(-w / 2 + 0.15, top, z, 0.3, 0.6, d, col, { m: mat });
    if (rnd.chance(0.7)) {
      const aw = Math.min(w * 0.85, 12);
      F.box(0, g.hi + 2.75, fz + 0.75, aw, 0.1, 1.6, rnd.pick(AWNING), { m: 'fabric', rx: 0.38 });
    }
    F.sign(0, g.hi + 3.9, fz + 0.1, Math.min(w * 0.75, 9), blk.district);
    if (rnd.chance(0.5)) { const [ax, az] = F.at(w * 0.2, z - d * 0.1); ctx.rooftopUnit(ax, top, az, F.yaw); }
    return true;
  }

  /* The LA mini-mall: an L of shops behind a striped parking lot, with a
   * donut shop on the corner by the street. */
  function miniMall(blk, F, Wl, D) {
    const W2 = Wl / 2, D2 = D / 2;
    const g = F.can(0, 0, Wl - 1, D - 1, 2);
    if (!g) return false;
    const col = rnd.pick(SHOP);
    F.paint(0, 0, Wl, D, '#55585b');
    const rowZ = -D2 + 7;
    F.bld(-6, rowZ, Wl - 13, 12, 4.6, col, 2, g, { noLip: true, mat: 'stucco' });
    F.bld(W2 - 6.5, -D2 + 13 + (D - 30) / 2, 12, D - 26, 4.6, col, 2, g, { noLip: true, mat: 'stucco' });
    // the covered walk along the shops, on posts
    F.box(-6, g.hi + 3.3, rowZ + 7, Wl - 13, 0.35, 2.2, '#e9e4da', { m: 'concrete' });
    for (let x = -W2 + 1; x < W2 - 13; x += 4) F.box(x, g.hi, rowZ + 7.9, 0.25, 3.3, 0.25, '#e9e4da', { m: 'metal' });
    const units = Math.floor((Wl - 13) / 7);
    for (let k = 0; k < units; k++) F.sign(-W2 + 4.5 + k * 7, g.hi + 3.95, rowZ + 6.1, 5.5, blk.district);
    // stalls, cars
    for (let x = -W2 + 2; x < W2 - 13; x += 2.7) {
      F.paint(x, rowZ + 11.5, 0.15, 5.4, '#e8e6e0', 1);
      if (rnd.chance(0.4)) F.car(x + 1.35, rowZ + 11.6, Math.PI / 2, rnd.pick(['sedan', 'compact', 'suv', 'pickup']));
    }
    // the donut shop
    const dg = F.can(-W2 + 5, D2 - 5, 8, 8, 2);
    if (dg) {
      F.bld(-W2 + 5, D2 - 5, 8, 8, 4.2, '#f6f3ea', 2, dg, { noLip: true, mat: 'stucco' });
      W.signs.push({ ...(() => { const [x, z] = F.at(-W2 + 5, D2 - 0.9); return { x, z }; })(), y: dg.hi + 3.6, ry: F.yaw, w: 6, h: 1.4, cell: ctx.donutCell, look: 0 });
    }
    // a tall pole sign at the kerb
    const [sx, sz] = F.at(W2 - 2, D2 - 1);
    const sy = gh(sx, sz);
    part({ s: 'cyl', x: sx, y: sy + 4, z: sz, sx: 0.4, sy: 8, sz: 0.4, c: '#8c8f93', m: 'metal' });
    W.signs.push({ x: sx, y: sy + 8.6, z: sz, ry: F.yaw, w: 4, h: 1.4, cell: signFor(blk.district), look: 1 });
    return true;
  }

  /* A gas station on a boulevard corner: canopy on columns, pump islands,
   * a mini-mart, the price sign by the kerb. No brands, just GAS. */
  function gasStation(blk, F, Wl, D) {
    const W2 = Wl / 2, D2 = D / 2;
    const g = F.can(0, 0, Wl - 1, D - 1, 2);
    if (!g) return false;
    F.paint(0, 0, Wl, D, '#a9a69d');
    const cz = D2 - 12;
    F.box(0, g.hi + 5.2, cz, 20, 0.9, 11, '#f4f4f2', { m: 'metal', col: true });
    F.box(0, g.hi + 5.5, cz + 5.52, 20.1, 0.35, 0.05, '#c8102e', { m: 'glow' });
    for (const [x, z] of [[-6, -2.6], [6, -2.6], [-6, 2.6], [6, 2.6]]) F.cyl(x, g.hi, cz + z, 0.5, 5.2, '#d9dde0', { m: 'metal', col: true });
    for (const x of [-6, 0, 6]) {
      F.box(x, g.hi, cz, 1.4, 0.25, 6.5, '#c9c4b8', { m: 'concrete' });
      for (const z of [-1.6, 1.6]) F.box(x, g.hi + 0.25, cz + z, 0.8, 1.7, 0.5, '#e6e8ea', { m: 'metal', col: true });
    }
    const mg = F.can(0, -D2 + 8, 14, 10, 2);
    if (mg) {
      F.bld(0, -D2 + 8, 14, 10, 4.3, '#efece4', 2, mg, { noLip: true, mat: 'stucco' });
      F.box(0, mg.hi + 4.3, -D2 + 13.05, 14, 0.8, 0.1, '#1d3f8a', { m: 'matte' });
    }
    const [sx, sz] = F.at(W2 - 2, D2 - 1.5);
    const sy = gh(sx, sz);
    part({ s: 'cyl', x: sx, y: sy + 3, z: sz, sx: 0.35, sy: 6, sz: 0.35, c: '#8c8f93', m: 'metal' });
    const price = `GAS\n$${(5.19 + rnd.int(0, 90) / 100).toFixed(2)}`;
    F.text(W2 - 2, sy + 7, D2 - 1.5, 2.6, 2.6, price, '#ffffff', { bg: '#1d3f8a', font: 40, glow: true }, 'price');
    for (let k = 0; k < 2; k++) if (rnd.chance(0.5)) F.car(-6 + k * 12 + 1.4, cz, Math.PI / 2, rnd.pick(['sedan', 'suv', 'compact']));
    return true;
  }

  /* ------------------------------------------------------- larger blocks */

  /* An apartment or office midrise; some 1920s-30s Art Deco with setbacks,
   * piers and a stepped crown; apartments get balconies. */
  function midrise(blk, F, w, d, h, opts = {}) {
    const g = F.can(0, 0, w, d, 4);
    if (!g) return null;
    const deco = opts.deco ?? rnd.chance(0.3);
    const apt = !deco && rnd.chance(opts.aptP ?? 0.5);
    const col = deco ? rnd.pick(DECO) : rnd.pick(opts.palette || SHOP);
    const mat = BRICK.includes(col) ? 'brick' : deco ? 'plaster' : 'stucco';
    const b = F.bld(0, 0, w, d, h, col, 2, g, { mat, noLip: deco });
    if (deco) {
      // setbacks: a narrower tier, then a stepped crown with a finial
      const t1 = h * 0.72;
      F.box(0, g.hi + h, 0, w * 0.7, Math.min(10, h * 0.2), d * 0.7, col, { m: 'win', win: 2, col: true });
      const crownY = g.hi + h + Math.min(10, h * 0.2);
      for (let k = 0; k < 3; k++) F.box(0, crownY + k * 1.2, 0, w * (0.5 - k * 0.12), 1.2, d * (0.5 - k * 0.12), col, { m: 'plaster' });
      F.cyl(0, crownY + 3.6, 0, 0.3, 5, '#c9a54a', { m: 'metal' });
      const n = Math.min(8, Math.floor(w / 4));
      for (let k = 0; k < n; k++) {
        const px = -w / 2 + (k + 0.5) * (w / n);
        F.box(px, g.hi + 4, d / 2 + 0.2, 0.6, h - 4 + 1.2, 0.4, col, { m: 'plaster' });
      }
      void t1;
    } else if (apt) {
      const floors = Math.min(10, Math.floor((h - 4) / 3.2));
      for (let f = 1; f <= floors; f++) {
        const y = g.hi + 4 + (f - 1) * 3.2;
        for (const bx of [-w * 0.3, w * 0.3]) {
          F.box(bx, y, d / 2 + 0.75, 3.2, 0.18, 1.5, '#d9d4c8', { m: 'concrete' });
          F.box(bx, y + 0.18, d / 2 + 1.48, 3.2, 1.0, 0.05, '#2b2b2b', { m: 'metal' });
        }
      }
    }
    // ground-floor retail: an awning and a sign on the street face
    if (rnd.chance(opts.retailP ?? 0.6)) {
      F.box(0, g.hi + 2.9, d / 2 + 0.75, Math.min(w * 0.6, 14), 0.1, 1.6, rnd.pick(AWNING), { m: 'fabric', rx: 0.38 });
      F.sign(0, g.hi + 4.0, d / 2 + 0.1, Math.min(w * 0.5, 10), blk.district);
    }
    if (!deco) ctx.rooftopKit(b);
    return b;
  }

  /* Downtown / Century City: a podium on the block edge, the tower above in
   * setback tiers, a crown or helipad; some older stone towers. */
  function tower(blk, F, w, d, h) {
    const g = F.can(0, 0, w, d, 5);
    if (!g) return null;
    const stone = h < 130 && rnd.chance(0.3);
    const col = stone ? rnd.pick(STONE) : rnd.pick(GLASS);
    const win = stone ? 2 : 3;
    const pod = rnd.range(12, 22);
    F.bld(0, 0, w, d, pod, stone ? col : rnd.pick(STONE), 2, g, { mat: 'concrete' });
    const tw = w * rnd.range(0.62, 0.8), td = d * rnd.range(0.62, 0.8);
    let b = F.bld(0, 0, tw, td, h, col, win, g, { mat: stone ? 'plaster' : 'concrete', noLip: true });
    if (h > 120 && rnd.chance(0.6)) b = F.bld(0, 0, tw * 0.78, td * 0.78, h + rnd.range(15, 35), col, win, g, { mat: 'concrete', noLip: true });
    const top = b.y + b.h;
    if (stone) {
      for (let k = 0; k < 3; k++) F.box(0, top + k * 2, 0, tw * (0.7 - k * 0.18), 2, td * (0.7 - k * 0.18), col, { m: 'plaster' });
    } else if (b.h > 90) ctx.helipad(b);
    else F.box(0, top, 0, tw * 0.5, 4, td * 0.5, '#8b8f94', { m: 'metal' });
    F.paint(0, 0, w + 10, d + 10, '#c9c4b8');
    return b;
  }

  /* A warehouse: brick in the Arts District, tilt-up concrete elsewhere;
   * sawtooth or flat roof, loading docks with roll-up doors, trailers. */
  function warehouse(blk, F, w, d, opts = {}) {
    const g = F.can(0, 0, w, d, 3);
    if (!g) return null;
    const brick = opts.brick ?? rnd.chance(0.4);
    const col = brick ? rnd.pick(BRICK) : rnd.pick(['#c9c2b2', '#b0a28a', '#d8d2c6', '#a7a49c', '#8d8f8c']);
    const h = rnd.range(7, 9.5);
    const b = F.bld(0, 0, w, d, h, col, 4, g, { mat: brick ? 'brick' : 'concrete', noLip: true });
    const top = g.hi + h;
    if (rnd.chance(0.5)) {
      for (let x = -w / 2 + 3; x < w / 2 - 2; x += 6) F.base('gable', x, top, 0, d - 1, 2.6, 6, '#8d9296', { m: 'metal', ry: Math.PI / 2 });
    } else {
      for (const [lx, lz, sx, sz] of [[0, d / 2, w, 0.4], [0, -d / 2, w, 0.4], [w / 2, 0, 0.4, d], [-w / 2, 0, 0.4, d]]) F.box(lx, top, lz, sx, 1, sz, col, { m: brick ? 'brick' : 'concrete' });
      for (let k = 0; k < 3; k++) F.cyl(rnd.range(-w / 3, w / 3), top, rnd.range(-d / 3, d / 3), 0.9, 1.2, '#9aa0a3', { m: 'metal' });
    }
    // loading docks on the street side
    const fz = d / 2;
    F.box(0, g.hi - 0.2, fz + 1.5, w * 0.7, 1.4, 3, '#a9a49b', { m: 'concrete', col: true, clear: true });
    const doors = Math.max(2, Math.floor(w * 0.7 / 6));
    for (let k = 0; k < doors; k++) {
      const x = -w * 0.35 + (k + 0.5) * (w * 0.7 / doors);
      F.box(x, g.hi + 1.2, fz + 0.06, 3.6, 3.8, 0.08, '#c9ccce', { m: 'metal' });
      if (rnd.chance(0.25) && F.can(x, fz + 9.6, 3, 16, 2)) {
        F.box(x, g.hi + 1.2, fz + 9.6, 2.6, 2.9, 15.5, '#f2f2ee', { m: 'metal', col: true });
        F.box(x, g.hi + 0.2, fz + 9.6, 2.4, 1.0, 14, '#2a2a2a', { m: 'metal' });
      }
    }
    if (brick && rnd.chance(0.35)) {                          // a mural on the end wall
      F.box(w / 2 + 0.06, g.hi + 1, 0, 0.05, h - 2, d * 0.6, rnd.pick(['#e84a5f', '#2e86c1', '#f2c14e', '#48c9b0', '#8e44ad']), { m: 'matte' });
    }
    if (rnd.chance(0.15)) ctx.rooftopKit(b, true);
    if (opts.fence ?? rnd.chance(0.4)) {
      for (const [lx, lz, sx, sz] of [[0, -d / 2 - 4, w + 8, 0.05], [w / 2 + 4, 0, 0.05, d + 8], [-w / 2 - 4, 0, 0.05, d + 8]]) {
        if (F.box(lx, F.ground(lx, lz), lz, sx, 2.4, sz, '#9aa0a3', { m: 'metal', clear: true })) F.box(lx, F.ground(lx, lz) + 2.4, lz, sx, 0.25, Math.max(sz, 0.25), '#b9bec2', { m: 'metal' });
      }
    }
    return b;
  }

  /* Campus: brick Romanesque halls with a ground-floor arcade and a tile
   * roof, the odd tower (USC's Bovard, UCLA's Royce). */
  function campusHall(blk, F, w, d) {
    const g = F.can(0, 0, w, d, 4);
    if (!g) return null;
    const col = rnd.pick(['#a5543d', '#b56a4c', '#9c4f3a', '#c9b28c']);
    const h = rnd.range(11, 18);
    const b = F.bld(0, 0, w, d, h, col, 2, g, { mat: col === '#c9b28c' ? 'stucco' : 'brick', noLip: true, roof: 'hip', roofColor: rnd.pick(TILE) });
    const n = Math.floor(w / 4.5);
    for (let k = 0; k < n; k++) F.base('arch', -w / 2 + (k + 0.5) * (w / n), g.hi, d / 2 + 0.04, 3, 3.8, 0.08, '#3a2a22', { m: 'matte' });
    if (rnd.chance(0.3)) {
      F.box(w / 2 - 4, g.hi, 0, 6, h + 12, 6, col, { m: 'win', win: 2, col: true });
      F.base('roof', w / 2 - 4, g.hi + h + 12, 0, 6.6, 4, 6.6, rnd.pick(TILE), { m: 'tile' });
    }
    return b;
  }

  return { frame, house, dingbat, beachHouse, mansion, hillside, storefront, miniMall, gasStation, midrise, tower, warehouse, campusHall, HOUSES };
}

/* Which kind of house goes on a lot in this neighbourhood. */
export function houseKind(district, style, rnd) {
  if (style === 'suburb') return rnd.chance(0.85) ? 'ranch' : 'spanish';
  const ding = HOUSES.dingbat[district] || 0.04;
  if (rnd.chance(ding)) return 'dingbat';
  const ranch = HOUSES.ranch[district] || 0;
  if (rnd.chance(ranch)) return 'ranch';
  if (HOUSES.craftsman.includes(district)) return rnd.chance(0.55) ? 'craftsman' : 'spanish';
  return rnd.chance(0.8) ? 'spanish' : 'craftsman';
}
