/*
 * MANDALA MAKER — UI layer
 * ------------------------
 * Wires the DOM and a canvas to window.Mandala. Finished strokes are painted
 * once onto offscreen layers, one per ring; each frame turns and sizes each
 * ring's layer as the chosen motion says, then draws only the stroke under
 * the finger on top — so even a busy, moving mandala stays smooth.
 *
 * The only state that outlives a visit is the drawing itself, in
 * localStorage — nothing leaves the browser.
 */
(function () {
  'use strict';

  var M = window.Mandala;
  var SAVE_KEY = 'mandala.drawing.v1';
  var EXPORT_PX = 2048;

  var el = {
    wrap: document.getElementById('canvasWrap'),
    canvas: document.getElementById('canvas'),
    hint: document.getElementById('hint'),
    surprise: document.getElementById('surprise'),
    undo: document.getElementById('undo'),
    clear: document.getElementById('clear'),
    savePng: document.getElementById('savePng'),
    saveVideo: document.getElementById('saveVideo'),
    cue: document.getElementById('cue'),
    breathGuide: document.getElementById('breathGuide'),
    slices: document.getElementById('slices'),
    slicesOut: document.getElementById('slicesOut'),
    size: document.getElementById('size'),
    sizeOut: document.getElementById('sizeOut'),
    swatches: document.getElementById('swatches'),
    palette: document.getElementById('palette'),
    mirror: document.getElementById('mirror'),
    glow: document.getElementById('glow'),
    guides: document.getElementById('guides'),
    motion: document.getElementById('motion'),
    speed: document.getElementById('speed'),
    speedOut: document.getElementById('speedOut'),
    toast: document.getElementById('toast')
  };

  var ctx = el.canvas.getContext('2d');
  // One offscreen layer per ring, so each ring can move on its own.
  var layers = [];
  for (var li = 0; li < M.BANDS; li++) {
    var lc = document.createElement('canvas');
    layers.push({ canvas: lc, ctx: lc.getContext('2d') });
  }

  var state = {
    strokes: [],
    background: M.PALETTES[0].bg,
    palette: M.PALETTES[0],
    color: M.PALETTES[0].colors[0],
    live: null,       // the stroke being drawn right now
    pointerId: null,
    phase: 0,         // the animation's clock: seconds × speed
    recording: false, // true while a video is being made
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

  // Every finished stroke onto its ring's (transparent) layer.
  function paintLayers() {
    var g = geometry(layers[0].canvas);
    for (var b = 0; b < layers.length; b++) {
      layers[b].ctx.clearRect(0, 0, layers[b].canvas.width, layers[b].canvas.height);
    }
    for (var i = 0; i < state.strokes.length; i++) {
      drawStroke(layers[M.strokeBand(state.strokes[i])].ctx, state.strokes[i], g);
    }
  }

  function bandMotion(band) {
    return M.motion(el.motion.value, band, state.phase);
  }

  // Place ring `band` on `c`: turn and size it about the centre.
  function withBand(c, g, band, draw) {
    var m = bandMotion(band);
    c.save();
    c.translate(g.cx, g.cy);
    c.rotate(m.angle);
    c.scale(m.scale, m.scale);
    c.translate(-g.cx, -g.cy);
    draw();
    c.restore();
  }

  function frame(time) {
    var dt = state.lastTime ? Math.min(0.1, (time - state.lastTime) / 1000) : 0;
    state.lastTime = time;
    if (el.motion.value !== 'still') state.phase += dt * Number(el.speed.value);

    if (state.layerDirty) {
      paintLayers();
      state.layerDirty = false;
    }

    var g = geometry(el.canvas);
    var w = el.canvas.width;
    ctx.fillStyle = state.background;
    ctx.fillRect(0, 0, w, w);
    // Guide lines are a drawing aid: never in a video.
    if (el.guides.checked && !state.recording) paintGuides(ctx, g);
    for (var b = 0; b < layers.length; b++) {
      withBand(ctx, g, b, function () { ctx.drawImage(layers[b].canvas, 0, 0); });
    }
    if (state.live) {
      withBand(ctx, g, state.live.band, function () { drawStroke(ctx, state.live, g); });
    }

    showCue();
    requestAnimationFrame(frame);
  }

  // The breath guide: "Breathe in" while the mandala opens, "Breathe out"
  // while it draws in. Only when it is breathing, and only if asked for.
  var lastCue = '';
  function showCue() {
    var breathing = el.motion.value === 'breathe' || el.motion.value === 'flow';
    var cue = el.breathGuide.checked && breathing && Number(el.speed.value) > 0
      ? M.breathCue(state.phase) : '';
    if (cue === lastCue) return;
    lastCue = cue;
    el.cue.hidden = !cue;
    el.cue.textContent = cue === 'in' ? 'Breathe in' : 'Breathe out';
  }

  function resize() {
    // 2x is sharp on any phone, and keeps seven full-size canvases light.
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    var px = Math.max(1, Math.round(el.wrap.clientWidth * dpr));
    if (el.canvas.width === px) return;
    el.canvas.width = el.canvas.height = px;
    for (var b = 0; b < layers.length; b++) layers[b].canvas.width = layers[b].canvas.height = px;
    state.layerDirty = true;
  }

  /* -------------------------------------------------------------- input */

  // Where the pointer is on screen, in unit space.
  function pointerUnit(e) {
    var rect = el.canvas.getBoundingClientRect();
    var half = rect.width / 2;
    return M.toUnit(e.clientX - rect.left, e.clientY - rect.top, half, half, half * 0.96);
  }

  // The same point as ring `band` sees it right now, so a line drawn on a
  // moving ring lands under the finger and then travels with that ring.
  function pointerOnBand(e, band) {
    var u = pointerUnit(e);
    return M.intoBand(u[0], u[1], bandMotion(band));
  }

  function brushSize() {
    return Number(el.size.value) / 1000;
  }

  el.canvas.addEventListener('pointerdown', function (e) {
    if (state.live) return;
    e.preventDefault();
    state.pointerId = e.pointerId;
    try { el.canvas.setPointerCapture(e.pointerId); } catch (err) { /* older browsers */ }
    // The ring the line starts in is the ring it belongs to.
    var start = pointerUnit(e);
    var band = M.bandOf(Math.hypot(start[0], start[1]));
    state.live = M.makeStroke({
      color: state.color,
      hue: Math.floor(Math.random() * 360),
      size: brushSize(),
      slices: el.slices.value,
      mirror: el.mirror.checked,
      glow: el.glow.checked,
      band: band,
      points: [pointerOnBand(e, band)]
    });
    el.hint.classList.add('gone');
  });

  el.canvas.addEventListener('pointermove', function (e) {
    if (!state.live || e.pointerId !== state.pointerId) return;
    var p = pointerOnBand(e, state.live.band);
    var pts = state.live.points;
    var prev = pts[pts.length - 1];
    // Skip points closer than a few pixels — they add weight, not detail.
    if (Math.hypot(p[0] - prev[0], p[1] - prev[1]) > 0.004) pts.push(p);
  });

  function finish(e) {
    if (!state.live || e.pointerId !== state.pointerId) return;
    state.strokes.push(state.live);
    // Draw just the new stroke onto its ring's layer — no need to repaint them all.
    drawStroke(layers[state.live.band].ctx, state.live, geometry(layers[0].canvas));
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
    toast('Fresh page');
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
    toast('Draw on top, or press it again');
  });

  function stamp() {
    return new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-');
  }

  function download(blob, name) {
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
  }

  el.savePng.addEventListener('click', function () {
    var out = document.createElement('canvas');
    out.width = out.height = EXPORT_PX;
    paintArt(out.getContext('2d'));
    out.toBlob(function (blob) {
      if (!blob) { toast('Could not make the picture'); return; }
      download(blob, 'mandala-' + stamp() + '.png');
      toast('Picture saved');
    }, 'image/png');
  });

  /*
   * Save video: records the canvas for exactly one breath at the current
   * speed, so the clip loops. Starts at the top of a breath, with the guide
   * lines left out. MP4 where the browser can make it, else WebM.
   */
  el.saveVideo.addEventListener('click', function () {
    if (state.recording) return;
    var fmt = window.MediaRecorder && el.canvas.captureStream
      ? M.videoFormat(function (m) { return MediaRecorder.isTypeSupported(m); }) : null;
    if (!fmt) { toast('This browser cannot record video'); return; }

    var seconds = M.videoSeconds(el.motion.value, el.speed.value);
    var chunks = [];
    var recorder;
    try {
      recorder = new MediaRecorder(el.canvas.captureStream(30), { mimeType: fmt.mime });
    } catch (err) {
      toast('This browser cannot record video');
      return;
    }

    // From the top of a breath, once every ring has joined in.
    var T = M.BREATH_SECONDS;
    state.phase = Math.max(T, Math.ceil(state.phase / T) * T);
    state.recording = true;
    el.saveVideo.classList.add('recording');

    var left = Math.ceil(seconds);
    el.saveVideo.textContent = 'Recording… ' + left;
    var tick = setInterval(function () {
      left -= 1;
      if (left > 0) el.saveVideo.textContent = 'Recording… ' + left;
    }, 1000);

    recorder.ondataavailable = function (e) { if (e.data && e.data.size) chunks.push(e.data); };
    recorder.onstop = function () {
      clearInterval(tick);
      state.recording = false;
      el.saveVideo.classList.remove('recording');
      el.saveVideo.textContent = 'Save video';
      var blob = new Blob(chunks, { type: fmt.mime.split(';')[0] });
      if (!blob.size) { toast('Could not make the video'); return; }
      download(blob, 'mandala-' + stamp() + '.' + fmt.ext);
      toast('Video saved');
    };
    recorder.start(250);
    setTimeout(function () { recorder.stop(); }, seconds * 1000);
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
  function showSpeed() {
    el.speedOut.textContent = Number(el.speed.value) + '×';
  }
  el.speed.addEventListener('input', showSpeed);
  el.motion.addEventListener('change', function () {
    // Back to Still puts every ring home; picking a motion starts it fresh.
    state.phase = 0;
  });

  /* --------------------------------------------------------------- boot */

  M.MOTIONS.forEach(function (m) {
    var o = document.createElement('option');
    o.value = m.id;
    o.textContent = m.label;
    el.motion.appendChild(o);
  });
  // Moving by default — unless the device asks for less motion.
  var calm = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  el.motion.value = calm ? 'still' : 'flow';
  showSpeed();

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
