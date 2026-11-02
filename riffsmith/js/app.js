/*
 * RIFFSMITH — App
 * ---------------
 * Wires the page to the engine (riff.js) and the player (synth.js): the
 * transport bar, the piano roll, the eight step tabs and their controls,
 * history (Back / Next), saving MIDI, and remembering settings.
 */
(function () {
  'use strict';

  var R = window.Riff;
  var player = new window.RiffSynth.Player();
  var STORE_KEY = 'riffsmith.v1';
  var HISTORY_MAX = 100;

  var $ = function (id) { return document.getElementById(id); };

  var state = {
    settings: R.applyPreset('sad-arp'),
    locked: {},
    tab: 0,
    upTo: false,
    sound: 'pluck',
    loop: true,
    preset: 'sad-arp'
  };
  var history = [];
  var hIndex = -1;
  var riff = null;

  // ------------------------------------------------------------- storage

  function load() {
    try {
      var raw = localStorage.getItem(STORE_KEY);
      if (!raw) return;
      var saved = JSON.parse(raw);
      state.settings = R.normalize(saved.settings);
      state.locked = saved.locked && typeof saved.locked === 'object' ? saved.locked : {};
      state.tab = Math.min(7, Math.max(0, saved.tab | 0));
      state.upTo = !!saved.upTo;
      if (window.RiffSynth.SOUNDS.some(function (s) { return s[0] === saved.sound; })) state.sound = saved.sound;
      state.loop = saved.loop !== false;
      state.preset = typeof saved.preset === 'string' ? saved.preset : '';
    } catch (e) { /* private mode or bad data: start fresh */ }
  }

  function save() {
    try {
      localStorage.setItem(STORE_KEY, JSON.stringify({
        settings: state.settings, locked: state.locked, tab: state.tab, upTo: state.upTo,
        sound: state.sound, loop: state.loop, preset: state.preset
      }));
    } catch (e) { /* storage unavailable: nothing to do */ }
  }

  // ------------------------------------------------------------- history

  function remember() {
    var snap = JSON.stringify(state.settings);
    if (hIndex >= 0 && history[hIndex] === snap) return;
    history = history.slice(0, hIndex + 1);
    history.push(snap);
    if (history.length > HISTORY_MAX) history.shift();
    hIndex = history.length - 1;
    updateHistoryButtons();
  }

  function goHistory(delta) {
    var i = hIndex + delta;
    if (i < 0 || i >= history.length) return;
    hIndex = i;
    state.settings = R.normalize(JSON.parse(history[i]));
    refresh(false);
  }

  function updateHistoryButtons() {
    $('back').disabled = hIndex <= 0;
    $('forward').disabled = hIndex >= history.length - 1;
  }

  // ------------------------------------------------------------- refresh

  // Rebuild the riff from the settings and redraw everything that shows it.
  // `record` adds the new settings to Back / Next history.
  function refresh(record) {
    riff = R.generate(state.settings, state.upTo ? state.tab : null);
    player.riff = riff;
    player.setTempo(state.settings.bpm);
    if (record !== false) remember();
    updateHistoryButtons();
    renderTransport();
    renderTabs();
    renderPanel();
    drawRoll();
    renderSummary();
    save();
  }

  function change(mutator) {
    mutator(state.settings);
    state.settings = R.normalize(state.settings);
    state.preset = '';
    refresh();
  }

  // ----------------------------------------------------------- transport

  function fillSelect(sel, options, value) {
    sel.innerHTML = '';
    options.forEach(function (o) {
      var opt = document.createElement('option');
      opt.value = String(o[0]);
      opt.textContent = o[1];
      sel.appendChild(opt);
    });
    sel.value = String(value);
  }

  function setupTransport() {
    fillSelect($('preset'), [['', 'Custom']].concat(R.PRESETS.map(function (p) { return [p.id, p.label]; })),
      state.preset);
    fillSelect($('key'), R.NOTE_NAMES.map(function (n, i) { return [i, n]; }), state.settings.key);
    fillSelect($('sound'), window.RiffSynth.SOUNDS, state.sound);

    $('preset').addEventListener('change', function (e) {
      if (!e.target.value) return;
      state.settings = R.applyPreset(e.target.value);
      state.preset = e.target.value;
      if (e.target.value === 'bassline') setSound('bass');
      else if (e.target.value === 'dreamy-bells') setSound('bell');
      else if (e.target.value === 'chill-keys') setSound('keys');
      refresh();
    });
    $('key').addEventListener('change', function (e) {
      change(function (s) { s.key = Number(e.target.value); });
    });
    $('bpm').addEventListener('change', function (e) {
      change(function (s) { s.bpm = Number(e.target.value) || 120; });
    });
    $('sound').addEventListener('change', function (e) { setSound(e.target.value); });
    $('loop').addEventListener('change', function (e) {
      state.loop = e.target.checked;
      player.loop = state.loop;
      save();
    });
    $('play').addEventListener('click', togglePlay);
    $('dice').addEventListener('click', roll);
    $('surprise').addEventListener('click', surprise);
    $('back').addEventListener('click', function () { goHistory(-1); });
    $('forward').addEventListener('click', function () { goHistory(1); });
    $('saveMidi').addEventListener('click', saveMidi);
  }

  function setSound(id) {
    state.sound = id;
    player.sound = id;
    $('sound').value = id;
    save();
  }

  function renderTransport() {
    $('key').value = String(state.settings.key);
    $('bpm').value = state.settings.bpm;
    $('preset').value = state.preset || '';
    $('loop').checked = state.loop;
  }

  function togglePlay() {
    if (player.playing) {
      player.stop();
      drawRoll();
    } else {
      player.riff = riff;
      player.loop = state.loop;
      player.sound = state.sound;
      player.setTempo(state.settings.bpm);
      if (!player.start()) {
        $('summary').textContent = 'This browser cannot play sound.';
        return;
      }
      requestAnimationFrame(animate);
    }
    renderPlay();
  }

  function renderPlay() {
    var b = $('play');
    b.setAttribute('aria-pressed', player.playing ? 'true' : 'false');
    b.querySelector('.ico').textContent = player.playing ? '■' : '▶';
    b.querySelector('.txt').textContent = player.playing ? 'Stop' : 'Play';
  }

  player.onStop = function () { renderPlay(); drawRoll(); };

  function roll() {
    state.settings = R.reroll(state.settings, state.locked);
    state.preset = '';
    refresh();
  }

  function surprise() {
    var s = state.settings;
    R.STEPS.forEach(function (step) {
      if (!state.locked[step.id]) s = R.randomizeStep(s, step.id);
    });
    if (!state.locked.progression) s.key = Math.floor(Math.random() * 12);
    state.settings = R.normalize(s);
    state.preset = '';
    refresh();
  }

  function saveMidi() {
    var full = R.generate(state.settings); // always the whole riff, not a part-way preview
    var bytes = R.toMidi(full, { bpm: state.settings.bpm, name: 'Riffsmith' });
    var first = full.chords[0] ? full.chords[0].name.replace('#', 's').replace('°', 'dim') : 'riff';
    var name = 'riffsmith-' + first + '-' + state.settings.bpm + 'bpm.mid';
    var blob = new Blob([bytes], { type: 'audio/midi' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
    flash('Saved ' + name);
  }

  var flashTimer = null;
  function flash(msg) {
    var el = $('summary');
    el.textContent = msg;
    el.classList.add('flash');
    clearTimeout(flashTimer);
    flashTimer = setTimeout(function () { el.classList.remove('flash'); renderSummary(); }, 2200);
  }

  function renderSummary() {
    if ($('summary').classList.contains('flash')) return;
    var s = state.settings;
    var mood = R.MOODS[s.progression.mood].label;
    var text = R.NOTE_NAMES[s.key] + ' · ' + mood + ' · ' + s.bpm + ' BPM · ' + riff.bars +
      (riff.bars === 1 ? ' bar · ' : ' bars · ') + riff.notes.length + ' notes';
    if (state.upTo) text += ' · showing steps 1–' + (state.tab + 1) + ' only';
    $('summary').textContent = text;
  }

  // ----------------------------------------------------------------- tabs

  function renderTabs() {
    var tabs = $('tabs');
    if (!tabs.children.length) {
      R.STEPS.forEach(function (step, i) {
        var b = document.createElement('button');
        b.className = 'tab';
        b.setAttribute('role', 'tab');
        b.innerHTML = '<span class="num">' + (i + 1) + '</span><span class="name">' + step.label +
          '</span><span class="dot" aria-hidden="true"></span><span class="lock" aria-hidden="true">🔒</span>';
        b.addEventListener('click', function () {
          state.tab = i;
          if (state.upTo) refresh(false); else { renderTabs(); renderPanel(); save(); }
        });
        tabs.appendChild(b);
      });
    }
    R.STEPS.forEach(function (step, i) {
      var b = tabs.children[i];
      var on = state.settings[step.id].on;
      b.classList.toggle('active', i === state.tab);
      b.classList.toggle('off', !on);
      b.classList.toggle('dimmed', state.upTo && i > state.tab);
      b.classList.toggle('locked', !!state.locked[step.id]);
      b.setAttribute('aria-selected', i === state.tab ? 'true' : 'false');
      b.title = step.label + (on ? '' : ' (off)') + (state.locked[step.id] ? ' — kept' : '');
    });
  }

  function formatValue(c, v) {
    if (c.pct) return Math.round(v * 100) + '%';
    if (c.times) return (Math.round(v * 100) / 100) + '×';
    if (c.note) return R.noteName(v);
    return String(v);
  }

  function renderPanel() {
    var step = R.STEPS[state.tab];
    var cfg = state.settings[step.id];
    $('stepTitle').textContent = (state.tab + 1) + '. ' + step.label;
    $('stepBlurb').textContent = step.blurb;
    $('stepOn').checked = cfg.on;
    $('stepLock').checked = !!state.locked[step.id];
    $('upTo').checked = state.upTo;
    $('panel').classList.toggle('is-off', !cfg.on);

    var box = $('controls');
    box.innerHTML = '';
    R.controlsFor(step.id, state.settings).forEach(function (c) {
      var id = 'c-' + step.id + '-' + c.key;
      var wrap = document.createElement('div');
      wrap.className = 'control ' + c.type;
      var value = cfg[c.key];
      if (c.type === 'toggle') {
        wrap.innerHTML = '<label class="check"><input type="checkbox" id="' + id + '"> ' + c.label + '</label>';
        var cb = wrap.querySelector('input');
        cb.checked = !!value;
        cb.disabled = !cfg.on;
        cb.addEventListener('change', function () {
          change(function (s) { s[step.id][c.key] = cb.checked; });
        });
      } else if (c.type === 'select') {
        wrap.innerHTML = '<label for="' + id + '">' + c.label + '</label><select id="' + id + '"></select>';
        var sel = wrap.querySelector('select');
        fillSelect(sel, c.options, value);
        sel.disabled = !cfg.on;
        sel.addEventListener('change', function () {
          var opt = c.options.filter(function (o) { return String(o[0]) === sel.value; })[0];
          change(function (s) {
            s[step.id][c.key] = opt ? opt[0] : sel.value;
            if (step.id === 'progression' && c.key === 'mood') s.progression.pattern = 'auto';
          });
        });
      } else {
        wrap.innerHTML = '<label for="' + id + '">' + c.label + ' <output></output></label>' +
          '<input type="range" id="' + id + '" min="' + c.min + '" max="' + c.max + '" step="' + c.step + '">';
        var range = wrap.querySelector('input');
        var out = wrap.querySelector('output');
        range.value = value;
        range.disabled = !cfg.on;
        out.textContent = formatValue(c, value);
        // While dragging, rebuild the riff live but don't fill history with
        // every in-between value; the release records one step.
        range.addEventListener('input', function () {
          out.textContent = formatValue(c, Number(range.value));
          state.settings[step.id][c.key] = Number(range.value);
          state.settings = R.normalize(state.settings);
          riff = R.generate(state.settings, state.upTo ? state.tab : null);
          player.riff = riff;
          drawRoll();
          renderSummary();
        });
        range.addEventListener('change', function () {
          change(function (s) { s[step.id][c.key] = Number(range.value); });
        });
      }
      box.appendChild(wrap);
    });
  }

  function setupPanel() {
    $('stepOn').addEventListener('change', function (e) {
      var id = R.STEPS[state.tab].id;
      change(function (s) { s[id].on = e.target.checked; });
    });
    $('stepLock').addEventListener('change', function (e) {
      state.locked[R.STEPS[state.tab].id] = e.target.checked;
      renderTabs();
      save();
    });
    $('upTo').addEventListener('change', function (e) {
      state.upTo = e.target.checked;
      refresh(false);
    });
    $('stepDice').addEventListener('click', function () {
      var id = R.STEPS[state.tab].id;
      var locked = {};
      R.STEPS.forEach(function (s) { locked[s.id] = s.id !== id; });
      state.settings = R.reroll(state.settings, locked);
      state.preset = '';
      refresh();
    });
    $('stepSurprise').addEventListener('click', function () {
      state.settings = R.randomizeStep(state.settings, R.STEPS[state.tab].id);
      state.preset = '';
      refresh();
    });
  }

  // ----------------------------------------------------------- piano roll

  var canvas = $('roll');
  var ctx2d = canvas.getContext('2d');
  var view = null; // geometry of the last drawing, for clicks

  function css(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function drawRoll() {
    if (!riff) return;
    var dpr = window.devicePixelRatio || 1;
    var w = canvas.clientWidth;
    var h = canvas.clientHeight;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(h * dpr);
    }
    var c = ctx2d;
    c.setTransform(dpr, 0, 0, dpr, 0, 0);
    c.clearRect(0, 0, w, h);

    var keyW = w < 500 ? 30 : 42;
    var topH = 24;
    var lo = 127, hi = 0;
    riff.notes.forEach(function (n) { lo = Math.min(lo, n.pitch); hi = Math.max(hi, n.pitch); });
    if (lo > hi) { lo = 57; hi = 69; }
    lo -= 2; hi += 2;
    while (hi - lo < 24) { lo--; hi++; }
    var rows = hi - lo + 1;
    var rowH = (h - topH) / rows;
    var gridW = w - keyW;
    var px = gridW / riff.length;
    view = { keyW: keyW, topH: topH, lo: lo, hi: hi, rowH: rowH, h: h };

    var colBg = css('--roll-bg'), colBlack = css('--roll-black'), colLine = css('--roll-line'),
        colBeat = css('--roll-beat'), colBar = css('--roll-bar'), colNote = css('--note'),
        colNoteHi = css('--note-hot'), colText = css('--muted'), colKeyW = css('--key-white'),
        colKeyB = css('--key-black'), colChord = css('--chord-bg');

    c.fillStyle = colBg;
    c.fillRect(keyW, topH, gridW, h - topH);

    // Rows: black-key rows a shade darker, like a piano roll.
    for (var p = lo; p <= hi; p++) {
      var y = topH + (hi - p) * rowH;
      var black = [1, 3, 6, 8, 10].indexOf(p % 12) !== -1;
      if (black) { c.fillStyle = colBlack; c.fillRect(keyW, y, gridW, rowH); }
      c.fillStyle = black ? colKeyB : colKeyW;
      c.fillRect(0, y, keyW - 1, rowH - 0.5);
      if (p % 12 === 0 && rowH >= 7) {
        c.fillStyle = black ? colKeyW : colText;
        c.font = Math.min(11, rowH) + 'px system-ui, sans-serif';
        c.textBaseline = 'middle';
        c.fillText(R.noteName(p), 3, y + rowH / 2);
      }
      if (p % 12 === 0) {
        c.fillStyle = colLine;
        c.fillRect(keyW, y + rowH - 0.5, gridW, 0.5);
      }
    }

    // Vertical grid: 16ths faint, beats stronger, bars strongest.
    var sixteenth = riff.ppq / 4;
    var showSixteenths = px * sixteenth >= 6;
    for (var t = 0; t <= riff.length; t += sixteenth) {
      var x = keyW + t * px;
      if (t % R.BAR === 0) { c.fillStyle = colBar; c.fillRect(x - 0.5, topH, 1.2, h - topH); }
      else if (t % riff.ppq === 0) { c.fillStyle = colBeat; c.fillRect(x - 0.5, topH, 1, h - topH); }
      else if (showSixteenths) { c.fillStyle = colLine; c.fillRect(x - 0.25, topH, 0.5, h - topH); }
    }

    // Chord names across the top.
    c.font = '600 12px system-ui, sans-serif';
    c.textBaseline = 'middle';
    riff.chords.forEach(function (ch, i) {
      var x0 = keyW + ch.t * px;
      var cw = ch.len * px;
      c.fillStyle = colChord;
      c.globalAlpha = i % 2 ? 0.55 : 1;
      c.fillRect(x0, 2, cw - 2, topH - 5);
      c.globalAlpha = 1;
      c.fillStyle = css('--ink');
      if (cw > 22) c.fillText(cw > 70 ? ch.name + '  ' + ch.roman : ch.name, x0 + 5, topH / 2);
    });

    // Notes: brightness follows velocity; notes under the playhead light up.
    var pos = player.position();
    riff.notes.forEach(function (n) {
      var nx = keyW + n.t * px;
      var ny = topH + (hi - n.pitch) * rowH;
      var nw = Math.max(2, n.len * px - 1);
      var hot = pos !== null && pos >= n.t && pos < n.t + n.len;
      c.globalAlpha = 0.45 + 0.55 * n.vel;
      c.fillStyle = hot ? colNoteHi : colNote;
      roundRect(c, nx, ny + 0.5, nw, Math.max(2, rowH - 1), Math.min(3, rowH / 3));
      c.fill();
    });
    c.globalAlpha = 1;

    if (pos !== null) {
      c.fillStyle = colNoteHi;
      c.fillRect(keyW + pos * px - 1, topH, 2, h - topH);
    }
  }

  function roundRect(c, x, y, w, h, r) {
    r = Math.max(0, Math.min(r, w / 2, h / 2)); // a corner can't be wider than half the note
    c.beginPath();
    c.moveTo(x + r, y);
    c.arcTo(x + w, y, x + w, y + h, r);
    c.arcTo(x + w, y + h, x, y + h, r);
    c.arcTo(x, y + h, x, y, r);
    c.arcTo(x, y, x + w, y, r);
    c.closePath();
  }

  function animate() {
    drawRoll();
    if (player.playing) requestAnimationFrame(animate);
  }

  // Tap the keyboard strip to hear a note.
  canvas.addEventListener('pointerdown', function (e) {
    if (!view) return;
    var r = canvas.getBoundingClientRect();
    var x = e.clientX - r.left, y = e.clientY - r.top;
    if (x > view.keyW || y < view.topH) return;
    var pitch = view.hi - Math.floor((y - view.topH) / view.rowH);
    player.sound = state.sound;
    player.audition(pitch);
  });

  window.addEventListener('resize', drawRoll);
  if (window.matchMedia) {
    var mq = window.matchMedia('(prefers-color-scheme: dark)');
    if (mq.addEventListener) mq.addEventListener('change', drawRoll);
  }

  // ------------------------------------------------------------ keyboard

  document.addEventListener('keydown', function (e) {
    var tag = (e.target && e.target.tagName) || '';
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA' || e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.code === 'Space') { e.preventDefault(); togglePlay(); }
    else if (e.key === 'd' || e.key === 'D') roll();
    else if (e.key === 'z' || e.key === 'Z') goHistory(-1);
    else if (e.key === 'x' || e.key === 'X') goHistory(1);
  });

  // ------------------------------------------------------------- install

  var deferredInstall = null;
  window.addEventListener('beforeinstallprompt', function (e) {
    e.preventDefault();
    deferredInstall = e;
    $('install').hidden = false;
  });
  $('install').addEventListener('click', function () {
    if (!deferredInstall) return;
    deferredInstall.prompt();
    deferredInstall.userChoice.then(function () { $('install').hidden = true; deferredInstall = null; });
  });
  window.addEventListener('appinstalled', function () { $('install').hidden = true; });

  if ('serviceWorker' in navigator && location.protocol !== 'file:') {
    window.addEventListener('load', function () {
      navigator.serviceWorker.register('sw.js').catch(function () { /* works fine without it */ });
    });
  }

  // ---------------------------------------------------------------- boot

  load();
  player.sound = state.sound;
  player.loop = state.loop;
  setupTransport();
  setupPanel();
  refresh();
})();
