/*
 * MANDALA MAKER — Engine
 * ----------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test.
 *
 * A mandala here is a list of strokes. Each stroke is drawn once, by a finger
 * or by the generator, and then repeated around the centre: `slices` copies
 * rotated evenly round the circle, and — with `mirror` on — each copy also
 * flipped, the way a kaleidoscope does it.
 *
 * Points are stored in unit space: the centre is (0, 0) and the edge of the
 * circle is radius 1. That is what lets a drawing survive a resize, a phone
 * turned on its side, or a save and reload — the canvas only scales it.
 *
 * A generated mandala is reproducible: the same seed always yields the same
 * design.
 *
 * Exposed as: window.Mandala (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var TAU = Math.PI * 2;

  var MIN_SLICES = 2;
  var MAX_SLICES = 32;

  // Named colour sets for the swatches and for the generator. Each comes with
  // the background it was chosen to sit on; the first set is the default.
  var PALETTES = [
    { id: 'earth', label: 'Earth', bg: '#f4ecdf',
      colors: ['#7f4f24', '#936639', '#a68a64', '#414833', '#c1121f', '#333d29'] },
    { id: 'jewel', label: 'Jewel', bg: '#0d0818',
      colors: ['#ff2d95', '#00e5ff', '#39ff14', '#ffcf4d', '#b06bff', '#ff8a3d'] },
    { id: 'lotus', label: 'Lotus', bg: '#1a0b14',
      colors: ['#ff9ec7', '#ffd1e3', '#f7b267', '#f25c54', '#c77dff', '#fff1f7'] },
    { id: 'ocean', label: 'Ocean', bg: '#04121f',
      colors: ['#00b4d8', '#90e0ef', '#caf0f8', '#48cae4', '#0077b6', '#ffd166'] },
    { id: 'ink', label: 'Ink', bg: '#ffffff',
      colors: ['#111111', '#333333', '#555555', '#1d3557', '#e63946', '#000000'] },
    { id: 'gold', label: 'Gold', bg: '#0a0a0a',
      colors: ['#ffd700', '#f5c542', '#e6b800', '#fff3b0', '#c9a227', '#ffffff'] }
  ];

  function clampSlices(n) {
    n = Math.round(Number(n) || 0);
    if (n < MIN_SLICES) return MIN_SLICES;
    if (n > MAX_SLICES) return MAX_SLICES;
    return n;
  }

  /*
   * Every copy of one point: `slices` rotations, and with `mirror` the same
   * again reflected across the slice's first edge. The first copy is always
   * the point itself.
   */
  function symmetryPoints(x, y, slices, mirror) {
    slices = clampSlices(slices);
    var out = [];
    for (var k = 0; k < slices; k++) {
      var a = (TAU * k) / slices;
      var c = Math.cos(a);
      var s = Math.sin(a);
      out.push([x * c - y * s, x * s + y * c]);
      if (mirror) out.push([x * c + y * s, x * s - y * c]);
    }
    return out;
  }

  /*
   * The same as symmetryPoints, but for a whole stroke: one list of points
   * per copy, ready to be drawn as separate lines.
   */
  function symmetryPaths(points, slices, mirror) {
    var copies = clampSlices(slices) * (mirror ? 2 : 1);
    var paths = [];
    for (var c = 0; c < copies; c++) paths.push([]);
    for (var i = 0; i < points.length; i++) {
      var p = symmetryPoints(points[i][0], points[i][1], slices, mirror);
      for (var j = 0; j < copies; j++) paths[j].push(p[j]);
    }
    return paths;
  }

  /* Canvas pixels → unit space, and back. */
  function toUnit(px, py, cx, cy, radius) {
    return [(px - cx) / radius, (py - cy) / radius];
  }
  function fromUnit(ux, uy, cx, cy, radius) {
    return [cx + ux * radius, cy + uy * radius];
  }

  /*
   * The colour of a stroke at point `i`. A plain stroke is one colour; a
   * rainbow stroke walks the hue wheel as it goes.
   */
  function colorAt(stroke, i) {
    if (stroke.color !== 'rainbow') return stroke.color;
    var hue = ((stroke.hue || 0) + i * 6) % 360;
    return 'hsl(' + hue + ', 95%, 60%)';
  }

  function makeStroke(opts) {
    return {
      color: opts.color || '#ffffff',
      hue: opts.hue || 0,
      size: opts.size || 0.01,
      slices: clampSlices(opts.slices || 8),
      mirror: !!opts.mirror,
      glow: !!opts.glow,
      band: opts.band,
      points: opts.points ? opts.points.slice() : []
    };
  }

  /* ------------------------------------------------------------ motion
   * A moving mandala turns as rings, not as one plate. The circle is cut
   * into BANDS rings from the centre out; every stroke belongs to one ring
   * and moves with it. Each mode says, for a ring at a moment in time, how
   * far it is turned and how large it is drawn.
   */
  var BANDS = 6;
  var BASE_TURN = 0.25; // radians per second at speed 1

  var MOTIONS = [
    { id: 'still', label: 'Still' },
    { id: 'spin', label: 'Spin — the whole mandala turns' },
    { id: 'swirl', label: 'Swirl — rings turn opposite ways' },
    { id: 'breathe', label: 'Breathe — gently pulses' },
    { id: 'flow', label: 'Flow — swirl and breathe' }
  ];

  // Which ring a point at distance `r` from the centre is in.
  function bandOf(r) {
    var b = Math.floor(r * BANDS);
    if (!(b >= 0)) return 0;
    return b >= BANDS ? BANDS - 1 : b;
  }

  // A stroke's ring: the one it was started in, else where it sits on average.
  function strokeBand(stroke) {
    if (stroke.band >= 0 && stroke.band < BANDS) return Math.floor(stroke.band);
    var sum = 0;
    for (var i = 0; i < stroke.points.length; i++) {
      sum += Math.hypot(stroke.points[i][0], stroke.points[i][1]);
    }
    return bandOf(stroke.points.length ? sum / stroke.points.length : 0);
  }

  /*
   * How ring `band` is placed at `phase`: the animation's own clock, which
   * the page advances by (seconds × speed) each frame — so changing the speed
   * never makes the picture jump, and speed 0 pauses it where it is.
   * Returns { angle, scale }. Still always gives { 0, 1 }.
   */
  function motion(mode, band, phase) {
    var k = Number(phase) || 0;
    var angle = 0;
    var scale = 1;
    if (mode === 'spin') angle = BASE_TURN * k;
    if (mode === 'swirl' || mode === 'flow') {
      // Even rings turn one way, odd rings the other; outer rings a little faster.
      angle = BASE_TURN * k * (band % 2 ? -1 : 1) * (1 + band * 0.15);
    }
    if (mode === 'breathe' || mode === 'flow') {
      // A ripple: each ring swells a moment after the one inside it.
      scale = 1 + 0.06 * Math.sin(k * 1.6 - band * 0.6);
    }
    return { angle: angle, scale: scale };
  }

  // Unit-space point → the same point as ring `band` sees it, given the
  // ring's current { angle, scale } — so a line drawn on a moving ring lands
  // under the finger.
  function intoBand(x, y, m) {
    var c = Math.cos(-m.angle);
    var s = Math.sin(-m.angle);
    x /= m.scale;
    y /= m.scale;
    return [x * c - y * s, x * s + y * c];
  }

  /* ------------------------------------------------------------ the dice */

  // mulberry32: small, fast, and the same everywhere — which is what makes a
  // seed mean the same mandala on every device.
  function rng(seed) {
    var a = seed >>> 0;
    return function () {
      a = (a + 0x6d2b79f5) >>> 0;
      var t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function newSeed() {
    return Math.floor(Math.random() * 1e9);
  }

  function polar(r, theta) {
    return [r * Math.cos(theta), r * Math.sin(theta)];
  }

  /* ------------------------------------------------- generator shapes
   * Each shape is drawn inside one slice — the wedge from angle 0 to
   * `wedge` — and the symmetry does the rest. All return a list of points.
   */
  var SHAPES = {
    // A band round the circle: one slice's worth of arc.
    ring: function (r0, r1, wedge) {
      var pts = [];
      for (var i = 0; i <= 12; i++) pts.push(polar(r0, (wedge * i) / 12));
      return pts;
    },
    // A leaf pointing outwards, centred in the slice.
    petal: function (r0, r1, wedge) {
      var mid = wedge / 2;
      var w = wedge * 0.42;
      var pts = [];
      var i, t;
      for (i = 0; i <= 16; i++) {
        t = i / 16;
        pts.push(polar(r0 + (r1 - r0) * t, mid + w * Math.sin(Math.PI * t)));
      }
      for (i = 16; i >= 0; i--) {
        t = i / 16;
        pts.push(polar(r0 + (r1 - r0) * t, mid - w * Math.sin(Math.PI * t)));
      }
      return pts;
    },
    // A scalloped edge: one bump per slice.
    arch: function (r0, r1, wedge) {
      var pts = [];
      for (var i = 0; i <= 16; i++) {
        var t = i / 16;
        pts.push(polar(r0 + (r1 - r0) * Math.sin(Math.PI * t), wedge * t));
      }
      return pts;
    },
    // Sharp teeth: in, out, in.
    zigzag: function (r0, r1, wedge) {
      return [polar(r0, 0), polar(r1, wedge / 2), polar(r0, wedge)];
    },
    // A spoke from the inner edge of the band to the outer.
    ray: function (r0, r1, wedge) {
      return [polar(r0, wedge / 2), polar(r1, wedge / 2)];
    },
    // A single dot in the middle of the band.
    dot: function (r0, r1, wedge) {
      return [polar((r0 + r1) / 2, wedge / 2)];
    },
    // A teardrop loop — a small closed curve.
    loop: function (r0, r1, wedge) {
      var cr = (r0 + r1) / 2;
      var rad = Math.min((r1 - r0) / 2, cr * Math.sin(wedge / 2) * 0.8);
      var centre = polar(cr, wedge / 2);
      var pts = [];
      for (var i = 0; i <= 20; i++) {
        var a = (TAU * i) / 20;
        pts.push([centre[0] + rad * Math.cos(a), centre[1] + rad * Math.sin(a)]);
      }
      return pts;
    }
  };
  var SHAPE_NAMES = Object.keys(SHAPES);

  function paletteById(id) {
    for (var i = 0; i < PALETTES.length; i++) if (PALETTES[i].id === id) return PALETTES[i];
    return PALETTES[0];
  }

  /*
   * Build a whole mandala from a seed: rings of shapes from the centre out,
   * every ring sharing the same symmetry so the design reads as one piece.
   *
   *   opts.slices   number of copies round the circle (random when left out)
   *   opts.palette  palette id (random when left out)
   *   opts.mirror   reflect each copy (default true)
   */
  function generate(seed, opts) {
    opts = opts || {};
    var rand = rng(seed);
    var pick = function (list) { return list[Math.floor(rand() * list.length)]; };

    var slices = opts.slices ? clampSlices(opts.slices) : pick([6, 8, 10, 12, 16, 24]);
    var palette = opts.palette ? paletteById(opts.palette) : pick(PALETTES);
    var mirror = opts.mirror === undefined ? true : !!opts.mirror;
    var wedge = TAU / slices;

    var strokes = [];
    var bands = 5 + Math.floor(rand() * 4);
    var r = 0.04 + rand() * 0.04;
    var step = (0.94 - r) / bands;

    // A centre dot, always — every mandala has a heart.
    strokes.push(makeStroke({
      color: pick(palette.colors), size: 0.03 + rand() * 0.03,
      slices: slices, mirror: false, points: [[0, 0]]
    }));

    for (var b = 0; b < bands; b++) {
      var r0 = r;
      var r1 = Math.min(0.96, r + step * (0.8 + rand() * 0.5));
      var shape = pick(SHAPE_NAMES);
      var color = rand() < 0.08 ? 'rainbow' : pick(palette.colors);
      var size = shape === 'dot' ? 0.012 + rand() * 0.02 : 0.003 + rand() * 0.008;
      strokes.push(makeStroke({
        color: color, hue: Math.floor(rand() * 360), size: size,
        slices: slices, mirror: mirror, glow: rand() < 0.35,
        points: SHAPES[shape](r0, r1, wedge)
      }));
      // Sometimes a band carries a second, finer shape on top.
      if (rand() < 0.4) {
        var extra = pick(SHAPE_NAMES);
        strokes.push(makeStroke({
          color: pick(palette.colors), size: 0.002 + rand() * 0.004,
          slices: slices, mirror: mirror,
          points: SHAPES[extra](r0 + (r1 - r0) * 0.2, r1 - (r1 - r0) * 0.2, wedge)
        }));
      }
      r = r1;
    }

    return { seed: seed, slices: slices, mirror: mirror, palette: palette.id,
      background: palette.bg, strokes: strokes };
  }

  /* --------------------------------------------------------- save file */

  var FORMAT = 'maz-mandala/1';

  function serialize(drawing) {
    return JSON.stringify({
      format: FORMAT,
      background: drawing.background,
      strokes: drawing.strokes.map(function (s) {
        return {
          color: s.color, hue: s.hue, size: s.size, slices: s.slices,
          mirror: s.mirror, glow: s.glow, band: s.band,
          // Three decimals is well under a pixel on any screen, and keeps
          // the save small enough for localStorage.
          points: s.points.map(function (p) {
            return [Math.round(p[0] * 1000) / 1000, Math.round(p[1] * 1000) / 1000];
          })
        };
      })
    });
  }

  // Returns null for anything that is not a mandala save, rather than
  // throwing — a corrupt save should mean a blank page, not a broken one.
  function deserialize(text) {
    var data;
    try { data = JSON.parse(text); } catch (e) { return null; }
    if (!data || data.format !== FORMAT || !Array.isArray(data.strokes)) return null;
    var strokes = [];
    for (var i = 0; i < data.strokes.length; i++) {
      var s = data.strokes[i];
      if (!s || !Array.isArray(s.points)) continue;
      var pts = s.points.filter(function (p) {
        return Array.isArray(p) && isFinite(p[0]) && isFinite(p[1]);
      });
      if (!pts.length) continue;
      strokes.push(makeStroke({
        color: typeof s.color === 'string' ? s.color : '#ffffff',
        hue: Number(s.hue) || 0, size: Number(s.size) || 0.01,
        slices: s.slices, mirror: s.mirror, glow: s.glow,
        band: typeof s.band === 'number' ? s.band : undefined, points: pts
      }));
    }
    return {
      background: typeof data.background === 'string' ? data.background : PALETTES[0].bg,
      strokes: strokes
    };
  }

  var api = {
    MIN_SLICES: MIN_SLICES,
    MAX_SLICES: MAX_SLICES,
    PALETTES: PALETTES,
    SHAPES: SHAPE_NAMES,
    clampSlices: clampSlices,
    symmetryPoints: symmetryPoints,
    symmetryPaths: symmetryPaths,
    toUnit: toUnit,
    fromUnit: fromUnit,
    colorAt: colorAt,
    makeStroke: makeStroke,
    BANDS: BANDS,
    MOTIONS: MOTIONS,
    bandOf: bandOf,
    strokeBand: strokeBand,
    motion: motion,
    intoBand: intoBand,
    rng: rng,
    newSeed: newSeed,
    paletteById: paletteById,
    generate: generate,
    serialize: serialize,
    deserialize: deserialize
  };

  root.Mandala = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
