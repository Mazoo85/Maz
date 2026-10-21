/*
 * MANDALA MAKER — UI layer
 * ------------------------
 * Wires the DOM and a canvas to window.Mandala. Finished strokes are painted
 * once onto an offscreen layer; each frame copies that layer and draws only
 * the stroke under the finger on top, so a busy mandala stays smooth.
 *
 * The only state that outlives a visit is the drawing itself, in
 * localStorage — nothing leaves the browser.
 */
(function () {
  'use strict';

  var M = window.Mandala;
  var SAVE_KEY = 'mandala.drawing.v1';
  var EXPORT_PX = 2048;
  var SPIN_RATE = 0.12; // radians per second — one turn in under a minute

  var el = {
    wrap: document.getElementById('canvasWrap'),
    canvas: document.getElementById('canvas'),
    hint: document.getElementById('hint'),
    surprise: document.getElementById('surprise'),
    undo: document.getElementById('undo'),
    clear: document.getElementById('clear'),
    savePng: document.getElementById('savePng'),
    slices: document.getElementById('slices'),
    slicesOut: document.getElementById('slicesOut'),
    size: document.getElementById('size'),
    sizeOut: document.getElementById('sizeOut'),
    swatches: document.getElementById('swatches'),
    palette: document.getElementById('palette'),
    mirror: document.getElementById('mirror'),
    glow: document.getElementById('glow'),
    guides: document.getElementById('guides'),
    spin: document.getElementById('spin'),
    toast: document.getElementById('toast')
  };

  var ctx = el.canvas.getContext('2d');
  var layer = document.createElement('canvas');
  var layerCtx = layer.getContext('2d');

  var state = {
    strokes: [],
    background: M.PALETTES[0].bg,
    palette: M.PALETTES[0],
    color: M.PALETTES[0].colors[0],
    live: null,       // the stroke being drawn right now
    pointerId: null,
    angle: 0,         // current spin angle
    lastTime: 0,
    layerDirty: true
  };

  /* ------------------------------------------------------------ storage */

  function store(fn) {
    try { return fn(window.localStorage); } catch (e) { return null; }
  }

  function save() {
    var text = M.serialize({ background: state.background, strokes: state.strokes });
    store(function (ls) { ls.setItem(SAVE_KEY, text); });
  }

  function load() {
    var text = store(function (ls) { return ls.getItem(SAVE_KEY); });
    var data = text ? M.deserialize(text) : null;
    if (!data) return false;
    state.strokes = data.strokes;
    state.background = data.background;
    return data.strokes.length > 0;
  }

  /* ------------------------------------------------------------ drawing */

  function geometry(canvas) {
    var w = canvas.width;
    return { cx: w / 2, cy: w / 2, r: w / 2 * 0.96 };
  }

  function drawStroke(c, stroke, g) {
    var paths = M.symmetryPaths(stroke.points, stroke.slices, stroke.mirror);
    var width = Math.max(1, stroke.size * g.r * 2);
    c.lineCap = 'round';
    c.lineJoin = 'round';
    c.lineWidth = width;
    c.shadowBlur = stroke.glow ? width * 2.5 : 0;

    for (var p = 0; p < paths.length; p++) {
      var pts = paths[p];
      var a = M.fromUnit(pts[0][0], pts[0][1], g.cx, g.cy, g.r);

      if (pts.length === 1) {
        c.fillStyle = c.shadowColor = M.colorAt(stroke, 0);
        c.beginPath();
        c.arc(a[0], a[1], width / 2, 0, Math.PI * 2);
        c.fill();
        continue;
      }

      if (stroke.color === 'rainbow') {
        // Each segment its own hue.
        for (var i = 1; i < pts.length; i++) {
          var b = M.fromUnit(pts[i][0], pts[i][1], g.cx, g.cy, g.r);
          c.strokeStyle = c.shadowColor = M.colorAt(stroke, i);
          c.beginPath();
          c.moveTo(a[0], a[1]);
          c.lineTo(b[0], b[1]);
          c.stroke();
          a = b;
        }
        continue;
      }

      // Smooth curve through the midpoints, so a shaky finger draws a clean line.
      c.strokeStyle = c.shadowColor = stroke.color;
      c.beginPath();
      c.moveTo(a[0], a[1]);
      for (var j = 1; j < pts.length - 1; j++) {
        var q = M.fromUnit(pts[j][0], pts[j][1], g.cx, g.cy, g.r);
        var n = M.fromUnit(pts[j + 1][0], pts[j + 1][1], g.cx, g.cy, g.r);
        c.quadraticCurveTo(q[0], q[1], (q[0] + n[0]) / 2, (q[1] + n[1]) / 2);
      }
      var last = M.fromUnit(pts[pts.length - 1][0], pts[pts.length - 1][1], g.cx, g.cy, g.r);
      c.lineTo(last[0], last[1]);
      c.stroke();
    }
    c.shadowBlur = 0;
  }

  // Background and every finished stroke, at whatever size `c` is.
  function paintArt(c) {
    var g = geometry(c.canvas);
    c.fillStyle = state.background;
    c.fillRect(0, 0, c.canvas.width, c.canvas.height);
    for (var i = 0; i < state.strokes.length; i++) drawStroke(c, state.strokes[i], g);
  }

  function isLight(hex) {
    var m = /^#?([0-9a-f]{6})$/i.exec(hex || '');
    if (!m) return false;
    var v = parseInt(m[1], 16);
    return (0.299 * (v >> 16) + 0.587 * ((v >> 8) & 255) + 0.114 * (v & 255)) > 150;
  }

  function paintGuides(c, g) {
    var slices = M.clampSlices(el.slices.value);
    c.save();
    c.strokeStyle = isLight(state.background) ? 'rgba(0,0,0,0.12)' : 'rgba(255,255,255,0.10)';
    c.lineWidth = Math.max(1, g.r / 400);
    c.beginPath();
    for (var k = 0; k < slices; k++) {
      var a = (Math.PI * 2 * k) / slices;
      c.moveTo(g.cx, g.cy);
      c.lineTo(g.cx + Math.cos(a) * g.r, g.cy + Math.sin(a) * g.r);
    }
    for (var ring = 1; ring <= 4; ring++) {
      c.moveTo(g.cx + (g.r * ring) / 4, g.cy);
      c.arc(g.cx, g.cy, (g.r * ring) / 4, 0, Math.PI * 2);
    }
    c.stroke();
    c.restore();
  }

  function frame(time) {
    var dt = state.lastTime ? Math.min(0.1, (time - state.lastTime) / 1000) : 0;
    state.lastTime = time;
    if (el.spin.checked) state.angle = (state.angle + dt * SPIN_RATE) % (Math.PI * 2);

    if (state.layerDirty) {
      paintArt(layerCtx);
      state.layerDirty = false;
    }

    var g = geometry(el.canvas);
    var w = el.canvas.width;
    ctx.save();
    ctx.fillStyle = state.background;
    ctx.fillRect(0, 0, w, w);
    ctx.translate(g.cx, g.cy);
    ctx.rotate(state.angle);
    ctx.translate(-g.cx, -g.cy);
    ctx.drawImage(layer, 0, 0);
    if (el.guides.checked) paintGuides(ctx, g);
    if (state.live) drawStroke(ctx, state.live, g);
    ctx.restore();

    requestAnimationFrame(frame);
  }

  function resize() {
    var dpr = Math.min(window.devicePixelRatio || 1, 3);
    var px = Math.max(1, Math.round(el.wrap.clientWidth * dpr));
    if (el.canvas.width === px) return;
    el.canvas.width = el.canvas.height = px;
    layer.width = layer.height = px;
    state.layerDirty = true;
  }

  /* -------------------------------------------------------------- input */

  // Where the pointer is, in unit space, undoing the spin so the line lands
  // under the finger even while the mandala turns.
  function pointerUnit(e) {
    var rect = el.canvas.getBoundingClientRect();
    var half = rect.width / 2;
    var radius = half * 0.96;
    var u = M.toUnit(e.clientX - rect.left, e.clientY - rect.top, half, half, radius);
    var c = Math.cos(-state.angle);
    var s = Math.sin(-state.angle);
    return [u[0] * c - u[1] * s, u[0] * s + u[1] * c];
  }

  function brushSize() {
    return Number(el.size.value) / 1000;
  }

  el.canvas.addEventListener('pointerdown', function (e) {
    if (state.live) return;
    e.preventDefault();
    state.pointerId = e.pointerId;
    try { el.canvas.setPointerCapture(e.pointerId); } catch (err) { /* older browsers */ }
    state.live = M.makeStroke({
      color: state.color,
      hue: Math.floor(Math.random() * 360),
      size: brushSize(),
      slices: el.slices.value,
      mirror: el.mirror.checked,
      glow: el.glow.checked,
      points: [pointerUnit(e)]
    });
    el.hint.classList.add('gone');
  });

  el.canvas.addEventListener('pointermove', function (e) {
    if (!state.live || e.pointerId !== state.pointerId) return;
    var p = pointerUnit(e);
    var pts = state.live.points;
    var prev = pts[pts.length - 1];
    // Skip points closer than a few pixels — they add weight, not detail.
    if (Math.hypot(p[0] - prev[0], p[1] - prev[1]) > 0.004) pts.push(p);
  });

  function finish(e) {
    if (!state.live || e.pointerId !== state.pointerId) return;
    state.strokes.push(state.live);
    // Draw just the new stroke onto the layer — no need to repaint them all.
    drawStroke(layerCtx, state.live, geometry(layer));
    state.live = null;
    state.pointerId = null;
    save();
    updateButtons();
  }
  el.canvas.addEventListener('pointerup', finish);
  el.canvas.addEventListener('pointercancel', finish);

  /* ------------------------------------------------------------ actions */

  var toastTimer = 0;
  function toast(msg) {
    el.toast.textContent = msg;
    el.toast.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { el.toast.classList.remove('show'); }, 1800);
  }

  function updateButtons() {
    var empty = state.strokes.length === 0;
    el.undo.disabled = empty;
    el.clear.disabled = empty;
    el.hint.classList.toggle('gone', !empty);
  }

  function undo() {
    if (!state.strokes.length) return;
    state.strokes.pop();
    state.layerDirty = true;
    save();
    updateButtons();
  }

  el.undo.addEventListener('click', undo);

  el.clear.addEventListener('click', function () {
    if (!state.strokes.length) return;
    if (!window.confirm('Clear the whole mandala and start again?')) return;
    state.strokes = [];
    state.layerDirty = true;
    save();
    updateButtons();
    toast('Fresh page ✨');
  });

  el.surprise.addEventListener('click', function () {
    var made = M.generate(M.newSeed(), {
      slices: el.slices.value,
      palette: el.palette.value,
      mirror: el.mirror.checked
    });
    state.strokes = made.strokes;
    state.background = made.background;
    state.layerDirty = true;
    save();
    updateButtons();
    toast('Ta-da! Draw on top, or try again');
  });

  el.savePng.addEventListener('click', function () {
    var out = document.createElement('canvas');
    out.width = out.height = EXPORT_PX;
    paintArt(out.getContext('2d'));
    var name = 'mandala-' + new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-') + '.png';
    out.toBlob(function (blob) {
      if (!blob) { toast('Could not make the picture'); return; }
      var url = URL.createObjectURL(blob);
      var a = document.createElement('a');
      a.href = url;
      a.download = name;
      document.body.appendChild(a);
      a.click();
      a.remove();
      setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
      toast('Picture saved 🖼️');
    }, 'image/png');
  });

  document.addEventListener('keydown', function (e) {
    if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key.toLowerCase() === 'z') {
      e.preventDefault();
      undo();
    }
  });

  /* ----------------------------------------------------------- controls */

  function selectSwatch(btn) {
    var all = el.swatches.querySelectorAll('.swatch');
    for (var i = 0; i < all.length; i++) all[i].setAttribute('aria-checked', 'false');
    btn.setAttribute('aria-checked', 'true');
  }

  function addSwatch(color, cls, label) {
    var b = document.createElement('button');
    b.type = 'button';
    b.className = 'swatch' + (cls ? ' ' + cls : '');
    b.setAttribute('role', 'radio');
    b.setAttribute('aria-label', label);
    b.title = label;
    if (!cls) b.style.background = color;
    b.addEventListener('click', function () {
      state.color = color;
      selectSwatch(b);
    });
    el.swatches.appendChild(b);
    return b;
  }

  function buildSwatches() {
    el.swatches.textContent = '';
    var first = null;
    state.palette.colors.forEach(function (c, i) {
      var b = addSwatch(c, '', 'Colour ' + (i + 1) + ' (' + c + ')');
      if (!first) first = b;
    });
    addSwatch('rainbow', 'rainbow', 'Rainbow');

    // Any colour at all, through the browser's own picker.
    var custom = document.createElement('label');
    custom.className = 'swatch custom';
    custom.title = 'Pick any colour';
    custom.setAttribute('aria-label', 'Pick any colour');
    custom.textContent = '🎨';
    var input = document.createElement('input');
    input.type = 'color';
    input.value = '#ffffff';
    input.addEventListener('input', function () {
      state.color = input.value;
      custom.style.background = input.value;
      var all = el.swatches.querySelectorAll('.swatch');
      for (var i = 0; i < all.length; i++) all[i].setAttribute('aria-checked', 'false');
    });
    custom.appendChild(input);
    el.swatches.appendChild(custom);

    state.color = state.palette.colors[0];
    selectSwatch(first);
  }

  M.PALETTES.forEach(function (p) {
    var o = document.createElement('option');
    o.value = p.id;
    o.textContent = p.label;
    el.palette.appendChild(o);
  });

  el.palette.addEventListener('change', function () {
    state.palette = M.paletteById(el.palette.value);
    buildSwatches();
    // Each colour set comes with the background it was chosen to sit on.
    state.background = state.palette.bg;
    state.layerDirty = true;
    save();
  });

  el.slices.addEventListener('input', function () {
    el.slicesOut.textContent = el.slices.value;
  });
  el.size.addEventListener('input', function () {
    el.sizeOut.textContent = el.size.value;
  });
  el.spin.addEventListener('change', function () {
    if (!el.spin.checked) state.angle = 0;
  });

  /* --------------------------------------------------------------- boot */

  buildSwatches();
  load();
  // Match the colour set to a saved background, if one fits.
  M.PALETTES.forEach(function (p) {
    if (p.bg === state.background) {
      state.palette = p;
      el.palette.value = p.id;
      buildSwatches();
    }
  });
  updateButtons();
  resize();
  window.addEventListener('resize', resize);
  if (window.ResizeObserver) new ResizeObserver(resize).observe(el.wrap);
  requestAnimationFrame(frame);
})();
