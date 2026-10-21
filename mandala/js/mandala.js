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

  // One breath at speed 1: in for half of it, out for the other half — the
  // slow pace of a calm breath, not a flicker.
  var BREATH_SECONDS = 6;
  // How far the mandala draws in at the bottom of a breath. It is at full
  // size at the top, so it never grows past the edge of the circle.
  var BREATH_DEPTH = 0.14;
  // Each ring a moment behind the one inside it, so the breath starts at the
  // heart and travels out — measured as a fraction of one breath.
  var BREATH_LAG = 0.04;

  var MOTIONS = [
    { id: 'still', label: 'Still' },
    { id: 'spin', label: 'Spin — the whole mandala turns' },
    { id: 'swirl', label: 'Swirl — rings turn opposite ways' },
    { id: 'breathe', label: 'Breathe — slowly in and out' },
    { id: 'flow', label: 'Flow — swirl and breathe' },
    { id: 'vortex', label: 'Vortex — spirals into itself' }
  ];

  /* ------------------------------------------------------------- vortex
   * The mandala turning into itself, forever. The page draws it as a
   * stack of copies of itself, each VORTEX_RATIO the size of the one
   * outside it and turned a little further, and each copy shows only its
   * own outer ring — the part between VORTEX_RATIO and full size — so the
   * copies fit together into one picture. As time passes every copy
   * shrinks towards the centre and turns; by the end of one cycle each has
   * taken exactly the place of the copy inside it, so the motion never
   * jumps and a recording of one cycle loops.
   *
   * A copy is named by its level `j`: level 0 is full size at phase 0, and
   * each level inward is one step smaller. Levels are fixed to the picture,
   * not the screen, so a line drawn on one stays with it as it spirals in.
   */
  var VORTEX_SECONDS = 6;   // one cycle at speed 1
  var VORTEX_RATIO = 0.55;  // each copy's size against the one outside it
  var VORTEX_TWIST = 0.6;   // radians each copy is turned past the one outside it

  // Where level `j` is at `phase`: { angle, scale }.
  function vortexLevel(j, phase) {
    var k = j + (Number(phase) || 0) / VORTEX_SECONDS;
    return { angle: VORTEX_TWIST * k, scale: Math.pow(VORTEX_RATIO, k) };
  }

  /*
   * Every copy needed to draw the vortex at `phase`, outermost first: its
   * level, its { angle, scale }, and the ring of the screen it fills, from
   * `inner` to `outer` (unit radii). The first copy is always large enough
   * to reach the edge; copies stop once they are too small to see.
   */
  function vortexCopies(phase) {
    var cycle = Math.floor((Number(phase) || 0) / VORTEX_SECONDS);
    var out = [];
    for (var j = -cycle - 1; ; j++) {
      var m = vortexLevel(j, phase);
      if (m.scale < 0.004) break;
      if (m.scale * VORTEX_RATIO >= 1.05) continue;
      out.push({ level: j, angle: m.angle, scale: m.scale, inner: m.scale * VORTEX_RATIO, outer: m.scale });
    }
    return out;
  }

  // The copy that fills distance `r` from the centre, or null at the very centre.
  function vortexPick(r, copies) {
    for (var i = 0; i < copies.length; i++) {
      if (r > copies[i].inner && r <= copies[i].outer) return copies[i];
    }
    return null;
  }

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
      // Outer rings wait their turn, then follow — so all start at rest.
      var lagged = Math.max(0, k - band * BREATH_LAG * BREATH_SECONDS);
      scale = 1 - BREATH_DEPTH * (1 - breath(lagged));
    }
    return { angle: angle, scale: scale };
  }

  // How full the breath is at `phase`: 1 at the top (fully in), 0 at the
  // bottom (fully out). Starts full, so a mandala at rest is full size.
  function breath(phase) {
    return (1 + Math.cos((2 * Math.PI * phase) / BREATH_SECONDS)) / 2;
  }

  // Which half of the breath `phase` is in, for the breathing guide: the
  // mandala is drawing in ('out') or opening up ('in').
  function breathCue(phase) {
    var t = (((Number(phase) || 0) % BREATH_SECONDS) + BREATH_SECONDS) % BREATH_SECONDS;
    return t < BREATH_SECONDS / 2 ? 'out' : 'in';
  }

  /* ------------------------------------------------------------- video
   * The format to record in: the first this browser can make. MP4 first —
   * it plays everywhere, iPhones included — then WebM.
   */
  var VIDEO_TYPES = [
    { mime: 'video/mp4;codecs=avc1', ext: 'mp4' },
    { mime: 'video/mp4', ext: 'mp4' },
    { mime: 'video/webm;codecs=vp9', ext: 'webm' },
    { mime: 'video/webm', ext: 'webm' }
  ];

  function videoFormat(isSupported) {
    for (var i = 0; i < VIDEO_TYPES.length; i++) {
      try { if (isSupported(VIDEO_TYPES[i].mime)) return VIDEO_TYPES[i]; } catch (e) { /* next */ }
    }
    return null;
  }

  // How long to record: exactly one breath at the chosen speed, so the clip
  // loops — kept between 3 and 12 seconds. A still or paused mandala gets a
  // short clip of its own.
  function videoSeconds(mode, speed) {
    speed = Number(speed) || 0;
    if (mode === 'still' || speed <= 0) return 4;
    var cycle = mode === 'vortex' ? VORTEX_SECONDS : BREATH_SECONDS;
    return Math.min(12, Math.max(3, cycle / speed));
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
    },
    // A teardrop: round at the inner edge, coming to a point outwards.
    drop: function (r0, r1, wedge) {
      var mid = wedge / 2;
      var w = wedge * 0.36;
      var pts = [];
      for (var i = 0; i <= 24; i++) {
        var a = (TAU * i) / 24;
        var t = (1 - Math.cos(a)) / 2;              // 0 at the tip, 1 at the round end
        var bulge = Math.sin(a) * Math.pow(t, 0.6); // fat at the round end, sharp at the tip
        pts.push(polar(r1 - (r1 - r0) * t, mid + w * bulge));
      }
      return pts;
    },
    // Crossing lines that make a row of diamonds — a lattice.
    diamond: function (r0, r1, wedge) {
      var m = (r0 + r1) / 2;
      return [polar(m, 0), polar(r1, wedge / 2), polar(m, wedge), polar(r0, wedge / 2), polar(m, 0)];
    }
  };
  var SHAPE_NAMES = Object.keys(SHAPES);

  // Bands that carry the design, and the finer bands that sit between them.
  var FEATURES = ['petal', 'lotus', 'leaf', 'drop', 'arch', 'loop', 'diamond'];
  var DETAILS = ['ring', 'beads', 'zigzag', 'scallop', 'diamond'];

  function paletteById(id) {
    for (var i = 0; i < PALETTES.length; i++) if (PALETTES[i].id === id) return PALETTES[i];
    return PALETTES[0];
  }

  /*
   * Build a whole mandala from a seed. It is laid out the way a hand-drawn
   * one is: a rosette at the heart, then rings from the centre out that
   * alternate between bold feature bands — petals, lotus petals, leaves,
   * teardrops, lattices, each layered with a smaller echo inside and often
   * dots at the tips — and fine detail bands of beads, scallops, zigzags
   * and rings between them, finished with a border at the edge. Colours
   * step through the palette rather than landing at random, so neighbouring
   * rings belong together.
   *
   *   opts.slices   number of copies round the circle (random when left out);
   *                 some fine bands use twice as many, for detail
   *   opts.palette  palette id (random when left out)
   *   opts.mirror   reflect each copy (default true)
   */
  function generate(seed, opts) {
    opts = opts || {};
    var rand = rng(seed);
    var pick = function (list) { return list[Math.floor(rand() * list.length)]; };
    var between = function (lo, hi) { return lo + rand() * (hi - lo); };

    var slices = opts.slices ? clampSlices(opts.slices) : pick([6, 8, 10, 12, 16, 24]);
    var palette = opts.palette ? paletteById(opts.palette) : pick(PALETTES);
    var mirror = opts.mirror === undefined ? true : !!opts.mirror;
    // Twice the repeats, for fine detail — only when that still fits.
    var fine = slices * 2 <= MAX_SLICES ? slices * 2 : slices;

    // Walk the palette in a shuffled order, so colours progress ring by ring.
    var order = palette.colors.slice();
    for (var o = order.length - 1; o > 0; o--) {
      var j = Math.floor(rand() * (o + 1));
      var tmp = order[o]; order[o] = order[j]; order[j] = tmp;
    }
    var ci = 0;
    var nextColor = function () { return order[ci++ % order.length]; };
    var accent = order[order.length - 1];

    var strokes = [];
    function add(points, color, size, extra) {
      extra = extra || {};
      strokes.push(makeStroke({
        color: color, hue: Math.floor(rand() * 360), size: size,
        slices: extra.slices || slices, mirror: mirror, glow: !!extra.glow, points: points
      }));
    }
    function shape(name, r0, r1, n) {
      return SHAPES[name](r0, r1, TAU / (n || slices));
    }

    // The heart: a dot, a rosette of petals round it, and a ring to hold them.
    var heart = between(0.09, 0.13);
    strokes.push(makeStroke({
      color: accent, size: between(0.025, 0.04), slices: slices, mirror: false, points: [[0, 0]]
    }));
    add(shape('petal', 0.025, heart), nextColor(), between(0.004, 0.007));
    add(shape('ring', heart + 0.01, heart + 0.01), nextColor(), 0.004);

    var r = heart + 0.03;
    var feature = true;
    var EDGE = 0.9;
    while (r < EDGE - 0.03) {
      var r0 = r;
      var r1;
      if (feature) {
        r1 = Math.min(EDGE, r0 + between(0.09, 0.16));
        var name = pick(FEATURES);
        var color = rand() < 0.06 ? 'rainbow' : nextColor();
        var size = between(0.005, 0.011);
        var glow = rand() < 0.25;
        var span = r1 - r0;

        if (name === 'lotus') {
          // Two rows of petals, the second sitting between the first.
          add(shape('petal', r0, r1), color, size, { glow: glow });
          add(SHAPES.petal(r0, r0 + span * 0.75, TAU / slices).map(function (p) {
            return rotate(p, Math.PI / slices);
          }), nextColor(), size * 0.8);
        } else if (name === 'leaf') {
          // A petal with a vein down the middle.
          add(shape('petal', r0, r1), color, size, { glow: glow });
          add(shape('ray', r0 + span * 0.1, r1 - span * 0.1), color, size * 0.5);
        } else {
          add(shape(name, r0, r1), color, size, { glow: glow });
        }

        // An echo: the same shape again, smaller, inside the first.
        if (name !== 'diamond' && rand() < 0.75) {
          var echo = name === 'lotus' || name === 'leaf' ? 'petal' : name;
          add(shape(echo, r0 + span * 0.18, r1 - span * 0.22), nextColor(), size * 0.6);
        }
        // Dots at the tips, just beyond the band.
        if (rand() < 0.6 && r1 + 0.025 < EDGE) {
          add(shape('dot', r1 + 0.01, r1 + 0.03), accent, between(0.012, 0.02));
          r1 += 0.03;
        }
      } else {
        r1 = Math.min(EDGE, r0 + between(0.025, 0.045));
        var kind = pick(DETAILS);
        var dc = nextColor();
        var n = rand() < 0.5 ? fine : slices;
        if (kind === 'beads') {
          // A string of beads: several dots spread across each slice.
          var count = 3;
          var mid = (r0 + r1) / 2;
          for (var k = 0; k < count; k++) {
            var a = ((k + 0.5) * TAU) / (n * count);
            strokes.push(makeStroke({
              color: dc, size: between(0.009, 0.015), slices: n, mirror: false,
              points: [polar(mid, a)]
            }));
          }
        } else if (kind === 'scallop') {
          add(shape('arch', r0, r1, n), dc, between(0.003, 0.005), { slices: n });
        } else if (kind === 'ring') {
          // Two fine rings, close together.
          add(shape('ring', r0, r0, n), dc, 0.003, { slices: n });
          add(shape('ring', r1, r1, n), dc, 0.003, { slices: n });
        } else {
          add(shape(kind, r0, r1, n), dc, between(0.003, 0.006), { slices: n });
        }
      }
      r = r1 + between(0.005, 0.02);
      feature = !feature;
    }

    // The border: a ring at the edge, with a scalloped trim just inside it.
    add(shape('ring', 0.95, 0.95), nextColor(), 0.007);
    add(shape('arch', 0.92, 0.95, fine), nextColor(), 0.003, { slices: fine });

    return { seed: seed, slices: slices, mirror: mirror, palette: palette.id,
      background: palette.bg, strokes: strokes };
  }

  function rotate(p, a) {
    var c = Math.cos(a);
    var s = Math.sin(a);
    return [p[0] * c - p[1] * s, p[0] * s + p[1] * c];
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
    BREATH_SECONDS: BREATH_SECONDS,
    BREATH_DEPTH: BREATH_DEPTH,
    breath: breath,
    breathCue: breathCue,
    videoFormat: videoFormat,
    VORTEX_SECONDS: VORTEX_SECONDS,
    VORTEX_RATIO: VORTEX_RATIO,
    vortexLevel: vortexLevel,
    vortexCopies: vortexCopies,
    vortexPick: vortexPick,
    videoSeconds: videoSeconds,
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
