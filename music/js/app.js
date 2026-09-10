/*
 * app.js — the interface: settings, transport, mixer, the scrolling note
 * timeline, exports and the little on-device library of saved songs.
 */
(function () {
  'use strict';

  const T = window.Theory;
  const G = window.Genres;
  const C = window.Composer;
  const E = window.Engine;
  const X = window.Exporter;

  const TRACK_META = [
    { id: 'drums',  label: 'Drums',  color: '#ff2d95' },
    { id: 'bass',   label: 'Bass',   color: '#a45cff' },
    { id: 'chords', label: 'Chords', color: '#00e5ff' },
    { id: 'arp',    label: 'Arp',    color: '#6bff8f' },
    { id: 'lead',   label: 'Lead',   color: '#ffc857' },
    { id: 'pad',    label: 'Pad',    color: '#7a8cff' }
  ];
  const DRUM_LANES = ['kick', 'snare', 'clap', 'hh', 'oh', 'ride', 'tom', 'conga',
                      'perc', 'shaker', 'tamb', 'cowbell', 'crash', 'riser', 'impact'];
  const STORE_KEY = 'songforge.library.v1';

  const el = function (id) { return document.getElementById(id); };

  const state = {
    genre: 'lofi',
    mood: 'chill',
    key: -1,          // -1 = let the composer choose
    length: 'medium',
    bpm: 0,           // 0 = auto
    song: null,
    seekDragging: false,
    seedEdited: false,    // true once the user types their own seed
    locked: {},           // parts protected from "re-roll every part"
    edited: {}            // parts the user has drawn on
  };

  let editor = null;

  const player = new E.Player();

  /* ------------------------------------------------------------------ *
   * Status line
   * ------------------------------------------------------------------ */

  let statusTimer = null;
  function status(msg, sticky) {
    el('statusLine').textContent = msg;
    if (statusTimer) clearTimeout(statusTimer);
    if (!sticky) statusTimer = setTimeout(function () { el('statusLine').textContent = 'Ready.'; }, 4000);
  }

  /* ------------------------------------------------------------------ *
   * Setup controls
   * ------------------------------------------------------------------ */

  function buildChips() {
    const gc = el('genreChips');
    G.list().forEach(function (g) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'chip' + (g.id === state.genre ? ' on' : '');
      b.textContent = g.name;
      b.title = g.blurb;
      b.setAttribute('role', 'radio');
      b.setAttribute('aria-checked', g.id === state.genre ? 'true' : 'false');
      b.addEventListener('click', function () {
        state.genre = g.id;
        syncChips();
        status(g.name + ' — ' + g.blurb);
      });
      b.dataset.id = g.id;
      gc.appendChild(b);
    });

    const mc = el('moodChips');
    mc.classList.add('mood');
    G.moodList().forEach(function (m) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'chip' + (m.id === state.mood ? ' on' : '');
      b.textContent = m.name;
      b.title = m.blurb;
      b.setAttribute('role', 'radio');
      b.addEventListener('click', function () {
        state.mood = m.id;
        syncChips();
        status(m.name + ' — ' + m.blurb);
      });
      b.dataset.id = m.id;
      mc.appendChild(b);
    });
  }

  function syncChips() {
    Array.prototype.forEach.call(el('genreChips').children, function (b) {
      const on = b.dataset.id === state.genre;
      b.classList.toggle('on', on);
      b.setAttribute('aria-checked', on ? 'true' : 'false');
    });
    Array.prototype.forEach.call(el('moodChips').children, function (b) {
      const on = b.dataset.id === state.mood;
      b.classList.toggle('on', on);
      b.setAttribute('aria-checked', on ? 'true' : 'false');
    });
  }

  function buildKeySelect() {
    const sel = el('keySelect');
    const auto = document.createElement('option');
    auto.value = '-1';
    auto.textContent = 'Auto';
    sel.appendChild(auto);
    T.NOTE_NAMES.forEach(function (n, i) {
      const o = document.createElement('option');
      o.value = String(i);
      o.textContent = n;
      sel.appendChild(o);
    });
    sel.addEventListener('change', function () { state.key = parseInt(sel.value, 10); });
  }

  function bindOptions() {
    el('lengthSelect').addEventListener('change', function () { state.length = this.value; });

    el('seedInput').addEventListener('input', function () { state.seedEdited = true; });

    const tempo = el('tempoInput');
    tempo.addEventListener('input', function () {
      state.bpm = parseInt(tempo.value, 10);
      el('tempoLabel').textContent = state.bpm + ' BPM';
    });
    el('tempoAuto').addEventListener('click', function () {
      state.bpm = 0;
      el('tempoLabel').textContent = 'auto';
      tempo.value = 115;
    });
  }

  /* ------------------------------------------------------------------ *
   * Generate
   * ------------------------------------------------------------------ */

  function generate(opts) {
    opts = opts || {};
    const seedField = state.seedEdited ? el('seedInput').value.trim() : '';
    const cfg = {
      seed: opts.seed || seedField || T.randomSeed(),
      genre: opts.genre || state.genre,
      mood: opts.mood || state.mood,
      length: opts.length || state.length,
      key: opts.key !== undefined ? opts.key : state.key,
      bpm: opts.bpm !== undefined ? opts.bpm : state.bpm
    };

    let song;
    try {
      song = C.compose(cfg);
    } catch (err) {
      status('Could not compose that one: ' + err.message, true);
      throw err;
    }

    state.song = song;
    window.__song = song;          // handle for the test suites
    state.locked = {};
    state.edited = {};
    player.load(song);

    el('songPanel').hidden = false;
    el('editPanel').hidden = false;
    el('mixPanel').hidden = false;
    el('exportPanel').hidden = false;

    renderSong();
    buildMixer();
    if (editor) {
      editor.startBar = 0;
      editor.clearHistory();
      editor.setTrack(editor.track);
      editor.resize();
      if (editorSoundPicker) editorSoundPicker();
      syncEditUI();
    }
    updateHash();

    if (opts.autoplay !== false) {
      player.play(0);
      setPlayIcon(true);
    } else {
      setPlayIcon(false);
    }
    status('"' + song.title + '" — ' + song.keyName + ', ' + song.bpm + ' BPM, seed ' + song.seed);
    return song;
  }

  function renderSong() {
    const s = state.song;
    el('songTitle').textContent = s.title;
    el('songMeta').textContent =
      s.genre.name + ' · ' + s.mood.name + ' · ' + s.keyName + ' · ' + s.bpm + ' BPM · ' +
      s.bars + ' bars · seed ' + s.seed;
    el('timeTotal').textContent = fmtTime(s.duration);
    el('seedInput').value = s.seed;
    state.seedEdited = false;
    buildChordStrip();
    resizeRoll();
  }

  function buildChordStrip() {
    const strip = el('chordStrip');
    strip.innerHTML = '';
    const s = state.song;
    // Show one cycle of the harmony rather than every repeat.
    const shown = s.chords.slice(0, Math.min(16, s.chords.length));
    shown.forEach(function (ch, i) {
      const d = document.createElement('div');
      d.className = 'chord-cell';
      d.dataset.index = String(i);
      d.innerHTML = '<div class="chord-name">' + ch.name + '</div>' +
                    '<div class="chord-roman">' + ch.roman + '</div>';
      strip.appendChild(d);
    });
  }

  /* ------------------------------------------------------------------ *
   * Mixer
   * ------------------------------------------------------------------ */

  function buildMixer() {
    const box = el('mixer');
    box.innerHTML = '';
    TRACK_META.forEach(function (meta) {
      const events = state.song.tracks[meta.id] || [];
      const row = document.createElement('div');
      row.className = 'track' + (events.length ? '' : ' silent');
      row.dataset.id = meta.id;

      const name = document.createElement('div');
      name.className = 'track-name';
      name.innerHTML = '<span class="dot" style="background:' + meta.color +
        ';box-shadow:0 0 8px ' + meta.color + '"></span>' + meta.label;
      row.appendChild(name);

      const lock = document.createElement('button');
      lock.type = 'button';
      lock.className = 'lock-btn' + (state.locked[meta.id] ? ' on' : '');
      lock.textContent = state.locked[meta.id] ? '🔒' : '🔓';
      lock.title = 'Protect ' + meta.label.toLowerCase() + ' from a re-roll of everything';
      lock.setAttribute('aria-label', 'Lock ' + meta.label);
      lock.addEventListener('click', function () {
        const next = !state.locked[meta.id];
        state.locked[meta.id] = next;
        lock.textContent = next ? '🔒' : '🔓';
        lock.classList.toggle('on', next);
        status(meta.label + (next ? ' locked — a re-roll of everything will leave it alone.'
                                  : ' unlocked.'));
      });
      row.appendChild(lock);

      const mute = document.createElement('button');
      mute.type = 'button';
      mute.className = 'mute-btn';
      mute.textContent = '🔊';
      mute.title = 'Mute ' + meta.label;
      mute.addEventListener('click', function () {
        const m = player.mix[meta.id];
        const next = !m.muted;
        player.setTrack(meta.id, { muted: next });
        mute.textContent = next ? '🔇' : '🔊';
        mute.classList.toggle('off', next);
        markRollDirty();
      });
      row.appendChild(mute);

      const vol = document.createElement('input');
      vol.type = 'range';
      vol.className = 'vol';
      vol.min = '0'; vol.max = '100';
      vol.value = String(Math.round(player.mix[meta.id].volume * 100));
      vol.title = meta.label + ' volume';
      vol.addEventListener('input', function () {
        player.setTrack(meta.id, { volume: parseInt(vol.value, 10) / 100 });
      });
      row.appendChild(vol);

      const reroll = document.createElement('button');
      reroll.type = 'button';
      reroll.className = 'mini-btn reroll';
      reroll.textContent = '🎲';
      reroll.title = 'Re-roll the ' + meta.label.toLowerCase();
      reroll.setAttribute('aria-label', 'Re-roll ' + meta.label);
      reroll.addEventListener('click', function () {
        if (editor) editor.pushHistory(meta.id);
        C.rerollPart(state.song, meta.id);
        state.edited[meta.id] = false;
        player.refresh();
        markRollDirty();
        if (editor && editor.track === meta.id) editor.refit();
        row.classList.toggle('silent', (state.song.tracks[meta.id] || []).length === 0);
        syncEditUI();
        status('New ' + meta.label.toLowerCase() + ' written.');
      });
      row.appendChild(reroll);

      box.appendChild(row);
    });
  }

  /* ------------------------------------------------------------------ *
   * Note editor
   * ------------------------------------------------------------------ */

  function colorFor(id) {
    for (let i = 0; i < TRACK_META.length; i++) if (TRACK_META[i].id === id) return TRACK_META[i].color;
    return '#00e5ff';
  }

  function buildEditor() {
    editor = new window.Editor({
      canvas: el('editor'),
      getSong: function () { return state.song; },
      player: player,
      colorFor: colorFor,
      onFollowOff: function () {
        el('followBtn').classList.remove('on');
        status('Follow off while you edit — turn it back on to scroll with the music.');
      },
      onChange: function () {
        state.edited[editor.track] = true;
        player.refresh();
        markRollDirty();
        syncEditUI();
      }
    });

    const chips = el('editTracks');
    TRACK_META.forEach(function (meta) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'chip' + (meta.id === editor.track ? ' on' : '');
      b.dataset.id = meta.id;
      b.setAttribute('role', 'radio');
      b.innerHTML = '<span class="dot" style="background:' + meta.color +
        ';box-shadow:0 0 8px ' + meta.color + '"></span>' + meta.label;
      b.addEventListener('click', function () {
        editor.setTrack(meta.id);
        if (editorSoundPicker) editorSoundPicker();
        syncEditUI();
        status('Editing ' + meta.label.toLowerCase() +
          (meta.id === 'drums' ? ' — tap the grid to add or remove hits.'
                               : ' — draw notes, drag the right edge to lengthen.'));
      });
      chips.appendChild(b);
    });

    function setTool(t) {
      editor.tool = t;
      el('toolDraw').classList.toggle('on', t === 'draw');
      el('toolErase').classList.toggle('on', t === 'erase');
      el('editor').classList.toggle('erasing', t === 'erase');
    }
    el('toolDraw').addEventListener('click', function () { setTool('draw'); });
    el('toolErase').addEventListener('click', function () { setTool('erase'); });

    el('snapSelect').addEventListener('change', function () { editor.snap = parseFloat(this.value); });
    el('lenSelect').addEventListener('change', function () { editor.noteLen = parseFloat(this.value); });

    el('inKeyBtn').addEventListener('click', function () {
      editor.inKey = !editor.inKey;
      this.classList.toggle('on', editor.inKey);
      status(editor.inKey ? 'Notes you draw will fit the key.' : 'Free drawing — any note, in key or not.');
    });
    el('followBtn').addEventListener('click', function () {
      editor.follow = !editor.follow;
      this.classList.toggle('on', editor.follow);
    });

    el('barPrev').addEventListener('click', function () {
      editor.scrollTo(editor.startBar - editor.bars);
      syncEditUI();
    });
    el('barNext').addEventListener('click', function () {
      editor.scrollTo(editor.startBar + editor.bars);
      syncEditUI();
    });
    el('barScroll').addEventListener('input', function () {
      editor.follow = false;
      el('followBtn').classList.remove('on');
      editor.scrollTo(parseInt(this.value, 10));
    });
    el('zoomSelect').addEventListener('change', function () {
      editor.bars = parseInt(this.value, 10);
      editor.scrollTo(editor.startBar);
      syncEditUI();
    });

    function buildSoundPicker() {
      const sel = el('soundSelect');
      sel.innerHTML = '';
      const track = editor.track;
      if (track === 'drums' || !state.song) {
        sel.disabled = true;
        const o = document.createElement('option');
        o.textContent = 'Drum kit';
        sel.appendChild(o);
        return;
      }
      sel.disabled = false;
      const names = G.PRESET_GROUPS[track] || [];
      const fallback = E.defaultPresetName(state.song, track);
      const auto = document.createElement('option');
      auto.value = '';
      auto.textContent = 'Default (' + (G.PRESET_LABEL[fallback] || fallback) + ')';
      sel.appendChild(auto);
      names.forEach(function (n) {
        const o = document.createElement('option');
        o.value = n;
        o.textContent = G.PRESET_LABEL[n] || n;
        sel.appendChild(o);
      });
      sel.value = (state.song.presetOverride && state.song.presetOverride[track]) || '';
    }
    el('soundSelect').addEventListener('change', function () {
      if (!state.song) return;
      state.song.presetOverride = state.song.presetOverride || {};
      if (this.value) state.song.presetOverride[editor.track] = this.value;
      else delete state.song.presetOverride[editor.track];
      status(colorLabel(editor.track) + ' now plays ' +
        (this.value ? (G.PRESET_LABEL[this.value] || this.value) : 'its default sound') + '.');
    });
    editorSoundPicker = buildSoundPicker;

    el('undoBtn').addEventListener('click', function () {
      if (!editor.undo()) status('Nothing left to undo.');
    });
    el('redoBtn').addEventListener('click', function () {
      if (!editor.redo()) status('Nothing to redo.');
    });

    el('clearTrackBtn').addEventListener('click', function () {
      if (!state.song) return;
      const label = colorLabel(editor.track);
      editor.clearTrack();
      status(label + ' cleared — draw your own, or re-roll it in the mixer.');
    });

    el('developBtn').addEventListener('click', function () {
      if (!state.song) return;
      const label = colorLabel(editor.track);
      if (editor.track === 'drums') {
        status('Develop works on the melodic parts — try it on the lead, bass or arp.', true);
        return;
      }
      editor.pushHistory();
      const ok = C.developPart(state.song, editor.track);
      if (!ok) {
        status('Draw a few more notes first — it needs an idea to develop.', true);
        return;
      }
      state.edited[editor.track] = true;
      player.refresh();
      markRollDirty();
      syncEditUI();
      status('Your idea now runs through the whole song\u2019s ' + label.toLowerCase() + '.');
    });

    window.__editor = editor;      // handle for the test suites
    window.addEventListener('resize', function () { editor.resize(); });
    editor.resize();
  }

  let editorSoundPicker = null;

  function colorLabel(id) {
    for (let i = 0; i < TRACK_META.length; i++) if (TRACK_META[i].id === id) return TRACK_META[i].label;
    return id;
  }

  function syncEditUI() {
    if (!editor || !state.song) return;
    Array.prototype.forEach.call(el('editTracks').children, function (b) {
      const on = b.dataset.id === editor.track;
      b.classList.toggle('on', on);
      b.setAttribute('aria-checked', on ? 'true' : 'false');
    });
    const scroll = el('barScroll');
    scroll.max = String(editor.maxStartBar());
    scroll.value = String(editor.startBar);
    el('developBtn').disabled = editor.track === 'drums';
    el('undoBtn').disabled = !editor.canUndo();
    el('redoBtn').disabled = !editor.canRedo();
    // The mixer shows which parts have been touched by hand.
    Array.prototype.forEach.call(el('mixer').children, function (row) {
      row.classList.toggle('edited', !!state.edited[row.dataset.id]);
    });
  }

  /* ------------------------------------------------------------------ *
   * Transport
   * ------------------------------------------------------------------ */

  function setPlayIcon(playing) {
    el('playIcon').textContent = playing ? '❚❚' : '▶';
    el('playBtn').classList.toggle('playing', playing);
    el('playBtn').setAttribute('aria-label', playing ? 'Pause' : 'Play');
  }

  function togglePlay() {
    if (!state.song) { generate(); return; }
    if (player.playing) {
      player.pause();
      setPlayIcon(false);
    } else {
      player.play();
      setPlayIcon(true);
    }
  }

  function fmtTime(sec) {
    if (!isFinite(sec) || sec < 0) sec = 0;
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return m + ':' + (s < 10 ? '0' : '') + s;
  }

  function bindTransport() {
    el('playBtn').addEventListener('click', togglePlay);

    el('loopBtn').addEventListener('click', function () {
      player.loop = !player.loop;
      this.classList.toggle('on', player.loop);
      status(player.loop ? 'Looping.' : 'Playing once through.');
    });

    const seek = el('seek');
    seek.addEventListener('input', function () {
      state.seekDragging = true;
      if (state.song) {
        const beat = (parseInt(seek.value, 10) / 1000) * state.song.totalBeats;
        el('timeNow').textContent = fmtTime(beat * (60 / state.song.bpm));
      }
    });
    seek.addEventListener('change', function () {
      if (state.song) {
        const beat = (parseInt(seek.value, 10) / 1000) * state.song.totalBeats;
        player.seek(beat);
      }
      state.seekDragging = false;
    });

    player.onEnd = function () { setPlayIcon(false); };

    document.addEventListener('keydown', function (e) {
      if (e.target && /input|select|textarea/i.test(e.target.tagName)) return;
      if (e.code === 'Space') { e.preventDefault(); togglePlay(); }
      if (e.key === 'g' || e.key === 'G') generate();
      if ((e.ctrlKey || e.metaKey) && (e.key === 'z' || e.key === 'Z')) {
        e.preventDefault();
        if (!editor) return;
        const did = e.shiftKey ? editor.redo() : editor.undo();
        if (!did) status(e.shiftKey ? 'Nothing to redo.' : 'Nothing left to undo.');
      }
    });
  }

  /* ------------------------------------------------------------------ *
   * Note timeline
   * ------------------------------------------------------------------ */

  const canvas = el('roll');
  const cx = canvas.getContext('2d');
  /* The notes never move, so they are drawn once into an offscreen canvas and
     blitted each frame. Only the playhead is redrawn at 60fps — which is what
     keeps this smooth on a phone. */
  const cache = document.createElement('canvas');
  const cacheCx = cache.getContext('2d');
  let rollW = 0, rollH = 0, rollDirty = true;

  function markRollDirty() { rollDirty = true; }

  function resizeRoll() {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    rollW = Math.max(1, Math.floor(rect.width));
    rollH = Math.max(1, Math.floor(rect.height));
    canvas.width = Math.floor(rollW * dpr);
    canvas.height = Math.floor(rollH * dpr);
    cx.setTransform(dpr, 0, 0, dpr, 0, 0);
    cache.width = canvas.width;
    cache.height = canvas.height;
    cacheCx.setTransform(dpr, 0, 0, dpr, 0, 0);
    rollDirty = true;
  }

  function pitchRange(song) {
    let lo = 127, hi = 0, any = false;
    TRACK_META.forEach(function (m) {
      if (m.id === 'drums') return;
      const evs = song.tracks[m.id] || [];
      for (let i = 0; i < evs.length; i++) {
        any = true;
        if (evs[i].p < lo) lo = evs[i].p;
        if (evs[i].p > hi) hi = evs[i].p;
      }
    });
    if (!any) { lo = 48; hi = 84; }
    if (hi - lo < 12) { hi = lo + 12; }
    return { lo: lo - 1, hi: hi + 1 };
  }

  function drawRollCache() {
    const song = state.song;
    const cx = cacheCx;
    cx.clearRect(0, 0, rollW, rollH);

    const drumH = Math.min(56, rollH * 0.3);
    const melH = rollH - drumH - 16;
    const topY = 14;
    const range = pitchRange(song);
    const span = range.hi - range.lo;
    const beats = song.totalBeats;
    const xOf = function (b) { return (b / beats) * rollW; };

    // Section bands + labels
    cx.font = '9px system-ui, sans-serif';
    for (let i = 0; i < song.sections.length; i++) {
      const sec = song.sections[i];
      const x0 = xOf(sec.startBar * 4);
      const x1 = xOf((sec.startBar + sec.bars) * 4);
      cx.fillStyle = i % 2 ? 'rgba(255,255,255,0.028)' : 'rgba(255,255,255,0.055)';
      cx.fillRect(x0, 0, x1 - x0, rollH);
      cx.fillStyle = 'rgba(200,190,225,0.55)';
      if (x1 - x0 > 30) cx.fillText(sec.name.toUpperCase(), x0 + 4, 10);
      cx.strokeStyle = 'rgba(255,255,255,0.09)';
      cx.beginPath(); cx.moveTo(x0, 0); cx.lineTo(x0, rollH); cx.stroke();
    }

    // Melodic notes
    TRACK_META.forEach(function (m) {
      if (m.id === 'drums') return;
      const evs = song.tracks[m.id] || [];
      const muted = player.mix[m.id].muted;
      cx.fillStyle = m.color;
      cx.globalAlpha = muted ? 0.16 : (m.id === 'pad' ? 0.4 : 0.85);
      for (let i = 0; i < evs.length; i++) {
        const e = evs[i];
        const x = xOf(e.t);
        const w = Math.max(1.5, xOf(e.t + e.d) - x);
        const y = topY + melH - ((e.p - range.lo) / span) * melH;
        cx.fillRect(x, y - 1.6, w, 3.2);
      }
    });
    cx.globalAlpha = 1;

    // Drum lanes
    const evs = song.tracks.drums || [];
    const laneY = topY + melH + 12;
    const laneH = drumH / DRUM_LANES.length;
    cx.globalAlpha = player.mix.drums.muted ? 0.18 : 0.9;
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      const lane = DRUM_LANES.indexOf(e.inst);
      if (lane < 0) continue;
      cx.fillStyle = lane <= 1 ? '#ff2d95' : lane <= 3 ? '#ff7ac0' : '#c76bd8';
      const x = xOf(e.t);
      cx.fillRect(x, laneY + lane * laneH, Math.max(1.2, rollW / (beats * 4)), Math.max(1.4, laneH - 1));
    }
    cx.globalAlpha = 1;
    rollDirty = false;
  }

  function drawRoll() {
    const song = state.song;
    if (!song || !rollW) return;
    if (rollDirty) drawRollCache();

    cx.clearRect(0, 0, rollW, rollH);
    cx.drawImage(cache, 0, 0, rollW, rollH);

    const px = (player.currentBeat() / song.totalBeats) * rollW;
    const lvl = player.level();
    if (lvl > 0.01) {
      const grad = cx.createLinearGradient(px - 40, 0, px + 40, 0);
      grad.addColorStop(0, 'rgba(0,229,255,0)');
      grad.addColorStop(0.5, 'rgba(0,229,255,' + (0.10 + lvl * 0.22).toFixed(3) + ')');
      grad.addColorStop(1, 'rgba(0,229,255,0)');
      cx.fillStyle = grad;
      cx.fillRect(px - 40, 0, 80, rollH);
    }

    cx.strokeStyle = '#ffffff';
    cx.lineWidth = 1.4;
    cx.beginPath();
    cx.moveTo(px, 0); cx.lineTo(px, rollH);
    cx.stroke();
  }

  function bindRollSeek() {
    function seekFromEvent(ev) {
      if (!state.song) return;
      const rect = canvas.getBoundingClientRect();
      const clientX = ev.touches && ev.touches.length ? ev.touches[0].clientX : ev.clientX;
      const frac = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
      player.seek(frac * state.song.totalBeats);
      if (!player.playing) drawRoll();
    }
    canvas.addEventListener('pointerdown', function (ev) { ev.preventDefault(); seekFromEvent(ev); });
  }

  /* ------------------------------------------------------------------ *
   * Frame loop — playhead, clock, current chord
   * ------------------------------------------------------------------ */

  let lastChordIndex = -1;

  function frame() {
    if (state.song) {
      const beat = player.currentBeat();
      const spb = 60 / state.song.bpm;

      if (!state.seekDragging) {
        el('seek').value = String(Math.round((beat / state.song.totalBeats) * 1000));
        el('timeNow').textContent = fmtTime(beat * spb);
      }

      const chord = C.chordAt(state.song, beat);
      const idx = state.song.chords.indexOf(chord);
      const shownIdx = idx % Math.min(16, state.song.chords.length);
      if (shownIdx !== lastChordIndex) {
        lastChordIndex = shownIdx;
        const cells = el('chordStrip').children;
        for (let i = 0; i < cells.length; i++) {
          cells[i].classList.toggle('now', i === shownIdx);
        }
      }
      drawRoll();

      if (editor) {
        if (editor.followPlayhead()) el('barScroll').value = String(editor.startBar);
        editor.draw();
      }
    }
    requestAnimationFrame(frame);
  }

  /* ------------------------------------------------------------------ *
   * Export
   * ------------------------------------------------------------------ */

  function bindExport() {
    el('wavBtn').addEventListener('click', function () {
      if (!state.song) return;
      const btn = this;
      btn.disabled = true;
      const label = btn.textContent;
      btn.textContent = 'Rendering…';
      el('exportProgress').hidden = false;
      el('exportBar').style.width = '5%';
      status('Rendering audio — this takes a few seconds.', true);

      setTimeout(function () {
        E.renderOffline(state.song, player.mix, function (p) {
          el('exportBar').style.width = Math.round(5 + p * 55) + '%';
        }).then(function (buffer) {
          el('exportBar').style.width = '85%';
          const blob = X.encodeWav(buffer);
          el('exportBar').style.width = '100%';
          return X.deliver(blob, X.safeName(state.song.title) + '-' + X.safeName(state.song.seed) + '.wav')
            .then(function (r) {
              status(r === 'declined' ? 'Download cancelled.'
                : r === 'busy' ? 'Another download is still open — try again in a moment.'
                : r === 'unavailable' ? 'This browser would not accept the file.'
                : 'Audio saved.');
            });
        }).catch(function (err) {
          status('Render failed: ' + err.message, true);
        }).then(function () {
          btn.disabled = false;
          btn.textContent = label;
          setTimeout(function () {
            el('exportProgress').hidden = true;
            el('exportBar').style.width = '0';
          }, 800);
        });
      }, 40);
    });

    el('stemsBtn').addEventListener('click', function () {
      if (!state.song) return;
      const btn = this;
      const label = btn.textContent;
      btn.disabled = true;
      el('exportProgress').hidden = false;
      el('exportBar').style.width = '3%';
      status('Rendering each part on its own — this takes longer than one mix.', true);

      setTimeout(function () {
        E.renderStems(state.song, player.mix, function (frac, name) {
          btn.textContent = 'Rendering ' + name + '…';
          el('exportBar').style.width = Math.round(3 + frac * 80) + '%';
        }).then(function (stems) {
          const base = X.safeName(state.song.title);
          const files = stems.map(function (st) {
            return { name: base + '-' + st.name + '.wav', blob: X.encodeWav(st.buffer) };
          });
          el('exportBar').style.width = '92%';
          return X.deliverMany(files, base + '-stems.zip');
        }).then(function (r) {
          status(r === 'declined' ? 'Download cancelled.'
            : r === 'busy' ? 'Another download is still open — try again in a moment.'
            : r === 'unavailable' ? 'This browser would not accept the file.'
            : 'Stems saved — one audio file per part.');
        }).catch(function (err) {
          status('Could not render the stems: ' + err.message, true);
        }).then(function () {
          btn.disabled = false;
          btn.textContent = label;
          setTimeout(function () {
            el('exportProgress').hidden = true;
            el('exportBar').style.width = '0';
          }, 800);
        });
      }, 40);
    });

    el('midiBtn').addEventListener('click', function () {
      if (!state.song) return;
      const blob = X.buildMidi(state.song);
      X.deliver(blob, X.safeName(state.song.title) + '-' + X.safeName(state.song.seed) + '.mid')
        .then(function (r) {
          status(r === 'declined' ? 'Download cancelled.'
            : r === 'busy' ? 'Another download is still open — try again in a moment.'
            : r === 'unavailable' ? 'This browser would not accept the file.'
            : 'MIDI saved — open it in any music app.');
        });
    });
  }

  /* ------------------------------------------------------------------ *
   * Sharing + saved library
   * ------------------------------------------------------------------ */

  function songConfig(s) {
    return {
      seed: s.seed, genre: s.genreId, mood: s.moodId,
      length: state.length, key: s.rootPc, bpm: s.bpm,
      title: s.title, keyName: s.keyName
    };
  }

  function updateHash() {
    const s = state.song;
    if (!s) return;
    const parts = ['seed=' + encodeURIComponent(s.seed), 'genre=' + s.genreId, 'mood=' + s.moodId,
      'len=' + state.length, 'key=' + s.rootPc, 'bpm=' + s.bpm];
    history.replaceState(null, '', '#' + parts.join('&'));
  }

  function readHash() {
    const h = (location.hash || '').replace(/^#/, '');
    if (!h) return null;
    const out = {};
    h.split('&').forEach(function (kv) {
      const i = kv.indexOf('=');
      if (i > 0) out[kv.slice(0, i)] = decodeURIComponent(kv.slice(i + 1));
    });
    if (!out.seed) return null;
    return {
      seed: out.seed,
      genre: G.GENRES[out.genre] ? out.genre : undefined,
      mood: G.MOODS[out.mood] ? out.mood : undefined,
      length: C.LENGTHS[out.len] ? out.len : 'medium',
      key: out.key !== undefined ? parseInt(out.key, 10) : -1,
      bpm: out.bpm ? parseInt(out.bpm, 10) : 0
    };
  }

  function loadLibrary() {
    try {
      const raw = localStorage.getItem(STORE_KEY);
      return raw ? JSON.parse(raw) : [];
    } catch (e) { return []; }
  }

  function saveLibrary(list) {
    try { localStorage.setItem(STORE_KEY, JSON.stringify(list.slice(0, 30))); } catch (e) { /* full or blocked */ }
  }

  function renderLibrary() {
    const list = loadLibrary();
    el('libraryPanel').hidden = list.length === 0;
    const box = el('library');
    box.innerHTML = '';
    list.forEach(function (item, i) {
      const row = document.createElement('div');
      row.className = 'lib-row';

      const info = document.createElement('div');
      info.className = 'lib-info';
      info.innerHTML = '<div class="lib-title">' + escapeHtml(item.title) + '</div>' +
        '<div class="lib-sub">' + escapeHtml((G.GENRES[item.genre] || {}).name || item.genre) +
        ' · ' + escapeHtml(item.keyName || '') + ' · ' + item.bpm + ' BPM · ' + escapeHtml(item.seed) + '</div>';
      row.appendChild(info);

      const load = document.createElement('button');
      load.type = 'button';
      load.className = 'mini-btn';
      load.textContent = 'Load';
      load.addEventListener('click', function () {
        state.genre = item.genre; state.mood = item.mood;
        state.length = item.length || 'medium';
        state.key = item.key; state.bpm = item.bpm;
        syncChips();
        el('seedInput').value = item.seed;
        generate({ seed: item.seed, genre: item.genre, mood: item.mood, length: item.length, key: item.key, bpm: item.bpm });
      });
      row.appendChild(load);

      const del = document.createElement('button');
      del.type = 'button';
      del.className = 'mini-btn';
      del.textContent = '✕';
      del.setAttribute('aria-label', 'Remove ' + item.title);
      del.addEventListener('click', function () {
        const l = loadLibrary();
        l.splice(i, 1);
        saveLibrary(l);
        renderLibrary();
      });
      row.appendChild(del);

      box.appendChild(row);
    });
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function bindSongActions() {
    el('saveBtn').addEventListener('click', function () {
      if (!state.song) return;
      const list = loadLibrary();
      const cfg = songConfig(state.song);
      if (!list.some(function (x) { return x.seed === cfg.seed && x.genre === cfg.genre; })) {
        list.unshift(cfg);
        saveLibrary(list);
        renderLibrary();
        status('Saved to this device.');
      } else {
        status('Already saved.');
      }
    });

    el('shareBtn').addEventListener('click', function () {
      updateHash();
      const url = location.href;
      const done = function () { status('Link copied — it recreates this exact song.'); };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(url).then(done, function () { status(url, true); });
      } else {
        status(url, true);
      }
    });
  }

  function bindHelp() {
    el('helpBtn').addEventListener('click', function () { el('helpModal').hidden = false; });
    el('helpClose').addEventListener('click', function () { el('helpModal').hidden = true; });
    el('helpModal').addEventListener('click', function (e) {
      if (e.target === this) this.hidden = true;
    });
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape') el('helpModal').hidden = true;
    });
  }

  /* ------------------------------------------------------------------ *
   * Boot
   * ------------------------------------------------------------------ */

  function init() {
    buildChips();
    buildKeySelect();
    bindOptions();
    bindTransport();
    bindExport();
    bindSongActions();
    bindHelp();
    bindRollSeek();
    buildEditor();
    if (editorSoundPicker) editorSoundPicker();
    renderLibrary();

    el('generateBtn').addEventListener('click', function () {
      generate();                       // a typed seed wins, otherwise a fresh one
    });
    el('rerollAllBtn').addEventListener('click', function () {
      if (!state.song) { generate(); return; }
      const kept = [];
      E.TRACKS.forEach(function (t) {
        if (state.locked[t]) { kept.push(colorLabel(t).toLowerCase()); return; }
        C.rerollPart(state.song, t);
        state.edited[t] = false;
      });
      player.refresh();
      markRollDirty();
      if (editor) editor.refit();
      syncEditUI();
      status(kept.length
        ? 'Rewritten around your locked ' + kept.join(' and ') + '.'
        : 'Every part rewritten — same chords, new performance.');
    });

    window.addEventListener('resize', function () { resizeRoll(); });

    // Hosted, the file arrives zipped; as a plain page it downloads directly.
    X.hostedSave().then(function (hosted) {
      if (!hosted) return;
      el('wavBtn').textContent = '⬇ Save audio (.wav in a .zip)';
      el('midiBtn').textContent = '⬇ Save MIDI (.mid in a .zip)';
      const note = el('exportNote');
      if (note) {
        note.textContent = 'Files arrive inside a .zip — open it to get the track out. ' +
          'The WAV is the finished song; the MIDI holds every note on its own track, so you can ' +
          'open it in GarageBand, Ableton, FL Studio, Logic or MuseScore and change anything.';
      }
    });

    const shared = readHash();
    if (shared) {
      state.genre = shared.genre || state.genre;
      state.mood = shared.mood || state.mood;
      state.length = shared.length;
      state.key = shared.key;
      state.bpm = shared.bpm;
      syncChips();
      el('lengthSelect').value = state.length;
      el('keySelect').value = String(state.key);
      el('seedInput').value = shared.seed;
      generate({ seed: shared.seed, autoplay: false });
      status('Shared song loaded — press play.', true);
    }

    resizeRoll();
    requestAnimationFrame(frame);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
