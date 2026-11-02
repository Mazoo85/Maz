/*
 * RIFFSMITH — App
 * ---------------
 * Wires the page to the engine (riff.js) and the player (synth.js): the
 * transport bar, the piano roll (with note editing and a velocity strip), the
 * eight step tabs and their controls, history (Back / Next), your own
 * presets, saving MIDI, and remembering everything.
 *
 * A riff is normally exactly what the settings generate. Once you edit a note
 * by hand, your edited notes are kept (state.edited) and shown instead, until
 * something regenerates the riff — a roll, a preset, a changed setting. Back
 * and Next remember edits too, so a roll never loses them for good.
 */
(function () {
  'use strict';

  var R = window.Riff;
  var player = new window.RiffSynth.Player();
  var STORE_KEY = 'riffsmith.v1';
  var PRESET_KEY = 'riffsmith.presets.v1';
  var HISTORY_MAX = 100;

  var $ = function (id) { return document.getElementById(id); };

  var state = {
    settings: R.applyPreset('sad-arp'),
    edited: null,       // hand-edited notes, or null
    locked: {},
    tab: 0,
    upTo: false,
    ghost: false,
    tool: 'draw',
    sound: 'pluck',
    loop: true,
    preset: 'sad-arp'   // a built-in id, 'my:<name>', or '' for custom
  };
  var myPresets = [];   // [{ name, settings, sound }]
  var history = [];
  var hIndex = -1;
  var riff = null;
  var ghostNotes = null;

  // ------------------------------------------------------------- storage

  function load() {
    try {
      var raw = localStorage.getItem(STORE_KEY);
      if (raw) {
        var saved = JSON.parse(raw);
        state.settings = R.normalize(saved.settings);
        state.locked = saved.locked && typeof saved.locked === 'object' ? saved.locked : {};
        state.tab = Math.min(7, Math.max(0, saved.tab | 0));
        state.upTo = !!saved.upTo;
        state.ghost = !!saved.ghost;
        if (validSound(saved.sound) && saved.sound !== 'sample') state.sound = saved.sound;
        state.loop = saved.loop !== false;
        state.preset = typeof saved.preset === 'string' ? saved.preset : '';
        if (Array.isArray(saved.edited)) {
          state.edited = R.tidy(saved.edited, state.settings.progression.bars * R.BAR);
        }
      }
    } catch (e) { /* private mode or bad data: start fresh */ }
    try {
      var rawP = localStorage.getItem(PRESET_KEY);
      if (rawP) myPresets = R.readPresetFile(rawP);
    } catch (e2) { myPresets = []; }
  }

  function save() {
    try {
      localStorage.setItem(STORE_KEY, JSON.stringify({
        settings: state.settings, edited: state.edited, locked: state.locked, tab: state.tab,
        upTo: state.upTo, ghost: state.ghost, sound: state.sound, loop: state.loop, preset: state.preset
      }));
    } catch (e) { /* storage unavailable: nothing to do */ }
  }

  function saveMyPresets() {
    try {
      if (myPresets.length) localStorage.setItem(PRESET_KEY, R.writePresetFile(myPresets));
      else localStorage.removeItem(PRESET_KEY);
    } catch (e) { flash('Couldn\'t save presets in this browser — use Export to keep them.'); }
  }

  function validSound(id) {
    return window.RiffSynth.SOUNDS.some(function (s) { return s[0] === id; });
  }

  // ------------------------------------------------------------- history

  function snapshot() { return JSON.stringify({ s: state.settings, n: state.edited }); }

  function remember() {
    var snap = snapshot();
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
    var snap = JSON.parse(history[i]);
    state.settings = R.normalize(snap.s);
    state.edited = snap.n;
    refresh({ record: false, keepEdits: true });
  }

  function updateHistoryButtons() {
    $('back').disabled = hIndex <= 0;
    $('forward').disabled = hIndex >= history.length - 1;
  }

  // ------------------------------------------------------------- refresh

  // Rebuild the riff from the settings and redraw everything that shows it.
  //   record     add the result to Back / Next history (default true)
  //   keepEdits  keep hand-edited notes (default false: settings changed, so
  //              the riff is regenerated)
  function refresh(opts) {
    opts = opts || {};
    if (!opts.keepEdits) state.edited = null;
    rebuild();
    if (opts.record !== false) remember();
    updateHistoryButtons();
    renderTransport();
    renderTabs();
    renderPanel();
    drawRoll();
    renderSummary();
    save();
  }

  // Regenerate the riff (and the "before" ghost) without touching the page.
  function rebuild() {
    riff = R.generate(state.settings, state.upTo ? state.tab : null);
    if (state.edited && !state.upTo) riff.notes = state.edited;
    player.riff = riff;
    player.setTempo(state.settings.bpm);
    ghostNotes = null;
    if (state.ghost && state.tab > 0 && !(state.edited && !state.upTo)) {
      ghostNotes = R.generate(state.settings, state.tab - 1).notes;
    }
  }

  function change(mutator, keepEdits) {
    mutator(state.settings);
    state.settings = R.normalize(state.settings);
    if (!keepEdits) state.preset = '';
    refresh({ keepEdits: !!keepEdits });
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

  function fillPresetSelect() {
    var sel = $('preset');
    sel.innerHTML = '';
    var custom = document.createElement('option');
    custom.value = '';
    custom.textContent = 'Custom';
    sel.appendChild(custom);
    var g1 = document.createElement('optgroup');
    g1.label = 'Built in';
    R.PRESETS.forEach(function (p) {
      var o = document.createElement('option');
      o.value = p.id; o.textContent = p.label;
      g1.appendChild(o);
    });
    sel.appendChild(g1);
    if (myPresets.length) {
      var g2 = document.createElement('optgroup');
      g2.label = 'My presets';
      myPresets.forEach(function (p) {
        var o = document.createElement('option');
        o.value = 'my:' + p.name; o.textContent = p.name;
        g2.appendChild(o);
      });
      sel.appendChild(g2);
    }
    sel.value = state.preset || '';
    if (sel.value !== (state.preset || '')) { state.preset = ''; sel.value = ''; }
    $('deletePreset').disabled = state.preset.indexOf('my:') !== 0;
    $('exportPresets').disabled = !myPresets.length;
  }

  function setupTransport() {
    fillPresetSelect();
    fillSelect($('key'), R.NOTE_NAMES.map(function (n, i) { return [i, n]; }), state.settings.key);
    fillSelect($('snap'), R.SNAPS, state.settings.snap);
    fillSelect($('sound'), window.RiffSynth.SOUNDS, state.sound);

    $('preset').addEventListener('change', function (e) {
      var v = e.target.value;
      if (!v) return;
      if (v.indexOf('my:') === 0) {
        var mine = myPresets.filter(function (p) { return 'my:' + p.name === v; })[0];
        if (!mine) return;
        state.settings = R.normalize(mine.settings);
        if (mine.sound && validSound(mine.sound) && mine.sound !== 'sample') setSound(mine.sound);
      } else {
        state.settings = R.applyPreset(v);
        var sounds = { bassline: 'bass', 'dreamy-bells': 'bell', 'chill-keys': 'keys' };
        setSound(sounds[v] || 'pluck');
      }
      state.preset = v;
      refresh();
    });
    $('key').addEventListener('change', function (e) {
      change(function (s) { s.key = Number(e.target.value); });
    });
    $('bpm').addEventListener('change', function (e) {
      change(function (s) { s.bpm = Number(e.target.value) || 120; }, true);
    });
    $('snap').addEventListener('change', function (e) {
      // Snap only regenerates the riff when the arpeggio follows it.
      var follows = state.settings.arp.rate === 'snap';
      change(function (s) { s.snap = Number(e.target.value); }, !follows);
    });
    $('sound').addEventListener('change', function (e) {
      setSound(e.target.value);
      if (e.target.value === 'sample' && !player.sample) $('sampleFile').click();
    });
    $('loadSample').addEventListener('click', function () { $('sampleFile').click(); });
    $('sampleFile').addEventListener('change', function (e) {
      var file = e.target.files && e.target.files[0];
      e.target.value = '';
      if (!file) return;
      file.arrayBuffer().then(function (buf) { return player.loadSample(buf, file.name); })
        .then(function () {
          setSound('sample');
          flash('Loaded ' + file.name + ' — it plays at its own pitch on C4.');
          player.audition(60);
        })
        .catch(function (err) { flash(err.message); });
    });
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

    $('savePreset').addEventListener('click', saveAsPreset);
    $('deletePreset').addEventListener('click', deletePreset);
    $('exportPresets').addEventListener('click', exportPresets);
    $('importPresets').addEventListener('click', function () { $('presetFile').click(); });
    $('presetFile').addEventListener('change', importPresets);
  }

  function setSound(id) {
    state.sound = id;
    player.sound = id;
    $('sound').value = id;
    $('loadSample').hidden = id !== 'sample';
    if (id === 'sample') {
      $('loadSample').textContent = player.sample ? '“' + shorten(player.sampleName, 18) + '” — change…' : 'Load sample…';
    }
    save();
  }

  function shorten(s, n) { return s.length > n ? s.slice(0, n - 1) + '…' : s; }

  function renderTransport() {
    $('key').value = String(state.settings.key);
    $('bpm').value = state.settings.bpm;
    $('snap').value = String(state.settings.snap);
    $('preset').value = state.preset || '';
    $('deletePreset').disabled = state.preset.indexOf('my:') !== 0;
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
        flash('This browser cannot play sound.');
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

  function download(bytes, name, type) {
    var blob = new Blob([bytes], { type: type });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
  }

  function saveMidi() {
    // Always the whole riff (with your edits), never a part-way preview.
    var full = R.generate(state.settings);
    if (state.edited) full.notes = state.edited;
    var bytes = R.toMidi(full, { bpm: state.settings.bpm, name: 'Riffsmith' });
    var first = full.chords[0] ? full.chords[0].name.replace('#', 's').replace('°', 'dim') : 'riff';
    var name = 'riffsmith-' + first + '-' + state.settings.bpm + 'bpm.mid';
    download(bytes, name, 'audio/midi');
    flash('Saved ' + name);
  }

  // ---------------------------------------------------------- my presets

  function saveAsPreset() {
    var current = state.preset.indexOf('my:') === 0 ? state.preset.slice(3) : '';
    var name = window.prompt('Name this preset:', current || 'My riff');
    name = R.cleanPresetName(name);
    if (!name) return;
    var existing = myPresets.filter(function (p) { return p.name === name; })[0];
    if (existing && name !== current && !window.confirm('Replace your preset “' + name + '”?')) return;
    var entry = { name: name, settings: R.normalize(state.settings), sound: state.sound === 'sample' ? null : state.sound };
    if (existing) myPresets[myPresets.indexOf(existing)] = entry;
    else myPresets.push(entry);
    state.preset = 'my:' + name;
    saveMyPresets();
    fillPresetSelect();
    closeMenu();
    save();
    flash('Saved preset “' + name + '”.');
  }

  function deletePreset() {
    if (state.preset.indexOf('my:') !== 0) return;
    var name = state.preset.slice(3);
    if (!window.confirm('Delete your preset “' + name + '”?')) return;
    myPresets = myPresets.filter(function (p) { return p.name !== name; });
    state.preset = '';
    saveMyPresets();
    fillPresetSelect();
    closeMenu();
    save();
    flash('Deleted “' + name + '”.');
  }

  function exportPresets() {
    if (!myPresets.length) return;
    download(R.writePresetFile(myPresets), 'riffsmith-presets.json', 'application/json');
    closeMenu();
    flash('Exported ' + myPresets.length + (myPresets.length === 1 ? ' preset.' : ' presets.'));
  }

  function importPresets(e) {
    var file = e.target.files && e.target.files[0];
    e.target.value = '';
    if (!file) return;
    file.text().then(function (text) {
      var list = R.readPresetFile(text);
      list.forEach(function (p) {
        var at = -1;
        myPresets.forEach(function (q, i) { if (q.name === p.name) at = i; });
        if (at >= 0) myPresets[at] = p; else myPresets.push(p);
      });
      saveMyPresets();
      fillPresetSelect();
      closeMenu();
      flash('Imported ' + list.length + (list.length === 1 ? ' preset.' : ' presets.'));
    }).catch(function (err) { flash(err.message); });
  }

  function closeMenu() { $('presetMenu').open = false; }

  document.addEventListener('click', function (e) {
    var menu = $('presetMenu');
    if (menu.open && !menu.contains(e.target)) menu.open = false;
  });

  // -------------------------------------------------------------- status

  var flashTimer = null;
  function flash(msg) {
    var el = $('summary');
    el.textContent = msg;
    el.classList.add('flash');
    clearTimeout(flashTimer);
    flashTimer = setTimeout(function () { el.classList.remove('flash'); renderSummary(); }, 2600);
  }

  function renderSummary() {
    if ($('summary').classList.contains('flash')) return;
    var s = state.settings;
    var mood = R.MOODS[s.progression.mood].label;
    var text = R.NOTE_NAMES[s.key] + ' · ' + mood + ' · ' + s.bpm + ' BPM · ' + riff.bars +
      (riff.bars === 1 ? ' bar · ' : ' bars · ') + riff.notes.length + ' notes';
    if (state.upTo) text += ' · showing steps 1–' + (state.tab + 1) + ' only';
    else if (state.edited) text += ' · edited by hand (Roll starts fresh; Back brings edits back)';
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
          if (state.upTo || state.ghost) refresh({ record: false, keepEdits: true });
          else { renderTabs(); renderPanel(); save(); }
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
    $('ghost').checked = state.ghost;
    $('panel').classList.toggle('is-off', !cfg.on);

    var box = $('controls');
    box.innerHTML = '';
    R.controlsFor(step.id, state.settings).forEach(function (c) {
      var id = 'c-' + step.id + '-' + c.key;
      var wrap = document.createElement('div');
      wrap.className = 'control ' + c.type;
      var value = cfg[c.key];
      // Speed only matters when the rhythm is even.
      var idle = step.id === 'arp' && (c.key === 'rate' || c.key === 'triplet') && cfg.rhythm !== 'even';
      if (idle) wrap.title = 'Only used when Rhythm is Even';
      if (c.type === 'toggle') {
        wrap.innerHTML = '<label class="check"><input type="checkbox" id="' + id + '"> ' + c.label + '</label>';
        var cb = wrap.querySelector('input');
        cb.checked = !!value;
        cb.disabled = !cfg.on || idle;
        cb.addEventListener('change', function () {
          change(function (s) { s[step.id][c.key] = cb.checked; });
        });
      } else if (c.type === 'select') {
        wrap.innerHTML = '<label for="' + id + '">' + c.label + '</label><select id="' + id + '"></select>';
        var sel = wrap.querySelector('select');
        fillSelect(sel, c.options, value);
        sel.disabled = !cfg.on || idle;
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
          state.edited = null;
          rebuild();
          drawRoll();
          renderSummary();
        });
        range.addEventListener('change', function () {
          change(function (s) { s[step.id][c.key] = Number(range.value); });
        });
      }
      if (idle) wrap.classList.add('idle');
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
      refresh({ record: false, keepEdits: true });
    });
    $('ghost').addEventListener('change', function (e) {
      state.ghost = e.target.checked;
      refresh({ record: false, keepEdits: true });
      if (state.ghost && state.edited) flash('“Before” outlines show on a generated riff, not one edited by hand.');
      else if (state.ghost && state.tab === 0) flash('Pick a step after Progression to see what it changed.');
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
    $('toolDraw').addEventListener('click', function () { setTool('draw'); });
    $('toolErase').addEventListener('click', function () { setTool('erase'); });
  }

  function setTool(t) {
    state.tool = t;
    $('toolDraw').classList.toggle('on', t === 'draw');
    $('toolErase').classList.toggle('on', t === 'erase');
    $('toolDraw').setAttribute('aria-pressed', t === 'draw' ? 'true' : 'false');
    $('toolErase').setAttribute('aria-pressed', t === 'erase' ? 'true' : 'false');
  }

  // ----------------------------------------------------------- piano roll

  var canvas = $('roll');
  var ctx2d = canvas.getContext('2d');
  var view = null;     // geometry of the last drawing, for pointer hits
  var frozen = null;   // the pitch range, held still while a drag is under way
  var sticky = null;   // the range in use while editing, so the view doesn't jump

  function css(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function pitchRange() {
    if (frozen) return frozen;
    if (state.edited && sticky && !state.upTo) {
      // While you edit, keep the view where it is as long as every note fits.
      var fits = riff.notes.every(function (n) { return n.pitch >= sticky.lo && n.pitch <= sticky.hi; });
      if (fits) return sticky;
    }
    sticky = naturalRange();
    return sticky;
  }

  function naturalRange() {
    var lo = 127, hi = 0;
    riff.notes.forEach(function (n) { lo = Math.min(lo, n.pitch); hi = Math.max(hi, n.pitch); });
    if (ghostNotes) ghostNotes.forEach(function (n) { lo = Math.min(lo, n.pitch); hi = Math.max(hi, n.pitch); });
    if (lo > hi) { lo = 57; hi = 69; }
    lo -= 2; hi += 2;
    while (hi - lo < 24) { lo--; hi++; }
    return { lo: Math.max(0, lo), hi: Math.min(127, hi) };
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
    var laneH = h >= 260 ? 54 : 40;
    var gridBottom = h - laneH - 6;
    var range = pitchRange();
    var lo = range.lo, hi = range.hi;
    var rows = hi - lo + 1;
    var rowH = (gridBottom - topH) / rows;
    var gridW = w - keyW;
    var px = gridW / riff.length;
    var laneTop = gridBottom + 6;
    view = { keyW: keyW, topH: topH, lo: lo, hi: hi, rowH: rowH, px: px, gridBottom: gridBottom,
             laneTop: laneTop, laneH: laneH, w: w };

    var colBg = css('--roll-bg'), colBlack = css('--roll-black'), colLine = css('--roll-line'),
        colBeat = css('--roll-beat'), colBar = css('--roll-bar'), colNote = css('--note'),
        colNoteHi = css('--note-hot'), colText = css('--muted'), colKeyW = css('--key-white'),
        colKeyB = css('--key-black'), colChord = css('--chord-bg'), colGhost = css('--ghost');

    c.fillStyle = colBg;
    c.fillRect(keyW, topH, gridW, gridBottom - topH);

    // Rows: black-key rows a shade darker, like a piano roll.
    for (var p = lo; p <= hi; p++) {
      var y = topH + (hi - p) * rowH;
      var black = [1, 3, 6, 8, 10].indexOf(p % 12) !== -1;
      if (black) { c.fillStyle = colBlack; c.fillRect(keyW, y, gridW, rowH); }
      c.fillStyle = black ? colKeyB : colKeyW;
      c.fillRect(0, y, keyW - 1, rowH - 0.5);
      if (p % 12 === 0 && rowH >= 7) {
        c.fillStyle = colText;
        c.font = Math.min(11, rowH) + 'px system-ui, sans-serif';
        c.textBaseline = 'middle';
        c.fillText(R.noteName(p), 3, y + rowH / 2);
      }
      if (p % 12 === 0) {
        c.fillStyle = colLine;
        c.fillRect(keyW, y + rowH - 0.5, gridW, 0.5);
      }
    }

    // Vertical grid: the Snap grid faint, beats stronger, bars strongest.
    var snap = state.settings.snap;
    var showSnap = px * snap >= 6;
    for (var t = 0; t <= riff.length; t += snap) {
      var x = keyW + t * px;
      if (t % R.BAR === 0) { c.fillStyle = colBar; c.fillRect(x - 0.5, topH, 1.2, gridBottom - topH); }
      else if (t % riff.ppq === 0) { c.fillStyle = colBeat; c.fillRect(x - 0.5, topH, 1, gridBottom - topH); }
      else if (showSnap) { c.fillStyle = colLine; c.fillRect(x - 0.25, topH, 0.5, gridBottom - topH); }
    }
    for (var b = 0; b <= riff.length; b += riff.ppq) { // beats that fall between triplet snaps
      if (b % snap !== 0) { c.fillStyle = b % R.BAR === 0 ? colBar : colBeat; c.fillRect(keyW + b * px - 0.5, topH, 1, gridBottom - topH); }
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

    // "Before this step" ghosts: outlines only.
    if (ghostNotes) {
      c.strokeStyle = colGhost;
      c.lineWidth = 1;
      ghostNotes.forEach(function (n) {
        if (n.pitch < lo || n.pitch > hi) return;
        roundRect(c, keyW + n.t * px + 0.5, topH + (hi - n.pitch) * rowH + 1,
          Math.max(2, n.len * px - 2), Math.max(2, rowH - 2), 2);
        c.stroke();
      });
    }

    // Notes: brightness follows velocity; notes under the playhead light up.
    var pos = player.position();
    riff.notes.forEach(function (n) {
      if (n.pitch < lo || n.pitch > hi) return;
      var nx = keyW + n.t * px;
      var ny = topH + (hi - n.pitch) * rowH;
      var nw = Math.max(2, n.len * px - 1);
      var hot = pos !== null && pos >= n.t && pos < n.t + n.len;
      c.globalAlpha = 0.45 + 0.55 * n.vel;
      c.fillStyle = hot || n === drag.note ? colNoteHi : colNote;
      roundRect(c, nx, ny + 0.5, nw, Math.max(2, rowH - 1), Math.min(3, rowH / 3));
      c.fill();
    });
    c.globalAlpha = 1;

    // Velocity strip: one stalk per note, as tall as it is loud.
    c.fillStyle = colBg;
    c.fillRect(keyW, laneTop, gridW, laneH);
    c.fillStyle = colText;
    c.font = '10px system-ui, sans-serif';
    c.textBaseline = 'middle';
    c.save();
    c.translate(keyW / 2 + 3, laneTop + laneH / 2);
    c.rotate(-Math.PI / 2);
    c.textAlign = 'center';
    c.fillText('VEL', 0, 0);
    c.restore();
    riff.notes.forEach(function (n) {
      var vx = keyW + n.t * px;
      var vh = (laneH - 6) * n.vel;
      c.fillStyle = n === drag.note ? colNoteHi : colNote;
      c.globalAlpha = 0.85;
      c.fillRect(vx, laneTop + laneH - vh, 1.5, vh);
      c.beginPath();
      c.arc(vx + 0.75, laneTop + laneH - vh, 2.4, 0, Math.PI * 2);
      c.fill();
    });
    c.globalAlpha = 1;

    if (pos !== null) {
      c.fillStyle = colNoteHi;
      c.fillRect(keyW + pos * px - 1, topH, 2, gridBottom - topH);
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

  // ------------------------------------------------------------- editing

  var drag = { mode: null, note: null, grabTicks: 0, changed: false, lastPitch: null };
  var lastLen = null;

  function pointerAt(e) {
    var r = canvas.getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top };
  }

  function ticksAt(x) { return (x - view.keyW) / view.px; }
  function pitchAt(y) { return view.hi - Math.floor((y - view.topH) / view.rowH); }

  function noteAt(pt) {
    if (pt.y < view.topH || pt.y >= view.gridBottom || pt.x < view.keyW) return null;
    var pitch = pitchAt(pt.y);
    var tk = ticksAt(pt.x);
    for (var i = riff.notes.length - 1; i >= 0; i--) {
      var n = riff.notes[i];
      if (n.pitch === pitch && tk >= n.t && tk < n.t + Math.max(n.len, 4 / view.px)) return n;
    }
    return null;
  }

  function nearEnd(n, pt) {
    var right = view.keyW + (n.t + n.len) * view.px;
    var width = n.len * view.px;
    return pt.x >= right - Math.max(5, Math.min(10, width * 0.3));
  }

  // From the first hand edit on, the riff's notes are your own copy.
  function beginEdit() {
    if (!state.edited) {
      state.edited = riff.notes.map(function (n) { return Object.assign({}, n); });
      riff.notes = state.edited;
      player.riff = riff;
      ghostNotes = null;
    }
  }

  function commitEdit() {
    state.edited = R.tidy(state.edited, riff.length);
    riff.notes = state.edited;
    player.riff = riff;
    remember();
    drawRoll();
    renderSummary();
    save();
  }

  function snapTicks(t) {
    var s = state.settings.snap;
    return Math.floor(t / s) * s;
  }

  function setVelocityAt(pt) {
    var tk = ticksAt(pt.x);
    var reach = Math.max(3 / view.px, state.settings.snap / 2);
    var v = Math.min(1, Math.max(0.05, 1 - (pt.y - view.laneTop - 3) / (view.laneH - 6)));
    var hit = false;
    riff.notes.forEach(function (n) {
      if (Math.abs(n.t - tk) <= reach) { n.vel = Math.round(v * 1000) / 1000; hit = true; }
    });
    return hit;
  }

  canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); });

  canvas.addEventListener('pointerdown', function (e) {
    if (!view) return;
    var pt = pointerAt(e);
    if (pt.x < view.keyW && pt.y >= view.topH && pt.y < view.gridBottom) {
      player.sound = state.sound;
      player.audition(pitchAt(pt.y));
      return;
    }
    if (pt.y < view.topH || pt.x < view.keyW) return;
    if (state.upTo) {
      flash('Untick “Only up to this step” to edit notes.');
      return;
    }
    e.preventDefault();
    frozen = pitchRange();
    canvas.setPointerCapture(e.pointerId);

    if (pt.y >= view.laneTop) {
      beginEdit();
      drag.mode = 'velocity';
      drag.changed = setVelocityAt(pt);
      drawRoll();
      return;
    }

    var hit = noteAt(pt);
    if (e.button === 2 || state.tool === 'erase') {
      drag.mode = 'erase';
      if (hit) eraseNote(hit);
      return;
    }
    // beginEdit may swap in a copy of the notes: find the hit note again there.
    var hitIndex = hit ? riff.notes.indexOf(hit) : -1;
    beginEdit();
    if (hit) {
      hit = riff.notes[hitIndex];
      drag.note = hit;
      drag.mode = nearEnd(hit, pt) ? 'resize' : 'move';
      drag.grabTicks = ticksAt(pt.x) - hit.t;
      lastLen = hit.len;
    } else {
      var t = snapTicks(ticksAt(pt.x));
      var len = Math.min(lastLen || state.settings.snap, riff.length - t);
      if (t < 0 || t >= riff.length || len <= 0) { frozen = null; return; }
      var fresh = { t: t, len: len, pitch: pitchAt(pt.y), vel: 0.8, pan: 0, modx: 1, mody: 0, fine: 0 };
      state.edited.push(fresh);
      drag.note = fresh;
      drag.mode = 'move';
      drag.grabTicks = ticksAt(pt.x) - t;
      drag.changed = true;
    }
    drag.lastPitch = drag.note.pitch;
    player.sound = state.sound;
    player.audition(drag.note.pitch);
    drawRoll();
  });

  function eraseNote(n) {
    var i = riff.notes.indexOf(n);
    if (i < 0) return;
    beginEdit();
    riff.notes.splice(i, 1);
    drag.changed = true;
    drawRoll();
  }

  canvas.addEventListener('pointermove', function (e) {
    if (!view) return;
    var pt = pointerAt(e);
    if (!drag.mode) {
      // Hover: show what a click would do.
      var cur = 'default';
      if (pt.x < view.keyW && pt.y >= view.topH && pt.y < view.gridBottom) cur = 'pointer';
      else if (pt.y >= view.laneTop) cur = 'ns-resize';
      else if (pt.y >= view.topH) {
        var hn = noteAt(pt);
        cur = state.tool === 'erase' ? (hn ? 'not-allowed' : 'default')
          : hn ? (nearEnd(hn, pt) ? 'ew-resize' : 'move') : 'crosshair';
      }
      canvas.style.cursor = cur;
      return;
    }
    if (drag.mode === 'velocity') {
      if (setVelocityAt(pt)) drag.changed = true;
    } else if (drag.mode === 'erase') {
      var over = noteAt(pt);
      if (over) eraseNote(over);
    } else if (drag.mode === 'move') {
      var n = drag.note;
      var t = snapTicks(ticksAt(pt.x) - drag.grabTicks + state.settings.snap / 2);
      t = Math.max(0, Math.min(riff.length - n.len, t));
      var p = Math.max(view.lo, Math.min(view.hi, pitchAt(pt.y)));
      if (t !== n.t || p !== n.pitch) drag.changed = true;
      n.t = t;
      n.pitch = p;
      if (p !== drag.lastPitch) { drag.lastPitch = p; player.audition(p); }
    } else if (drag.mode === 'resize') {
      var rn = drag.note;
      var s = state.settings.snap;
      var end = Math.round(ticksAt(pt.x) / s) * s;
      var len = Math.max(s, Math.min(riff.length - rn.t, end - rn.t));
      if (len !== rn.len) drag.changed = true;
      rn.len = len;
      lastLen = len;
    }
    drawRoll();
  });

  function endDrag() {
    if (!drag.mode) return;
    var changed = drag.changed;
    drag.mode = null;
    drag.note = null;
    drag.changed = false;
    frozen = null;
    if (changed) commitEdit(); else drawRoll();
  }
  canvas.addEventListener('pointerup', endDrag);
  canvas.addEventListener('pointercancel', endDrag);

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
    else if (e.key === 'p' || e.key === 'P') setTool('draw');
    else if (e.key === 'e' || e.key === 'E') setTool('erase');
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
  setTool('draw');
  if (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) {
    $('rollHint').textContent = 'Tap to add a note · drag to move · drag its end to resize · ' +
      'switch to Erase to remove notes · drag in the bottom strip to set loudness';
  }
  setSound(state.sound);
  refresh({ keepEdits: true });
})();
