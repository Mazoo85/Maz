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
    { id: 'counter', label: 'Answer', color: '#ff8a3d' },
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
    meter: '',        // '' = let the genre choose
    scale: '',        // '' = let the genre choose
    humanise: 1,      // how far notes drift off the grid when they are written
    chordRate: 0,     // 0 = let the genre choose
    bpm: 0,           // 0 = auto
    song: null,
    seekDragging: false,
    seedEdited: false,    // true once the user types their own seed
    locked: {},           // parts protected from "re-roll every part"
    edited: {}            // parts the user has drawn on
  };

  let editor = null;
  let autoLane = null;

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
    const loose = el('looseInput');
    loose.addEventListener('input', function () {
      state.humanise = parseInt(this.value, 10) / 100;
      el('looseLabel').textContent = state.humanise === 0 ? 'dead straight'
        : state.humanise < 0.7 ? 'tight'
        : state.humanise > 1.4 ? 'very loose'
        : state.humanise > 1.05 ? 'loose' : 'normal';
    });
    loose.addEventListener('change', function () {
      status('Looseness set — it takes effect on the next song, or when you re-roll a part.');
    });

    /* Grouped, because there are more than forty of them and a flat list of
       forty scale names is a wall nobody reads. */
    const scaleSel = el('scaleSelect');
    T.SCALE_GROUPS.forEach(function (g) {
      const grp = document.createElement('optgroup');
      grp.label = g.name;
      g.ids.forEach(function (id) {
        if (!T.SCALES[id]) return;
        const o = document.createElement('option');
        o.value = id;
        o.textContent = T.SCALES[id].name;
        grp.appendChild(o);
      });
      scaleSel.appendChild(grp);
    });
    scaleSel.addEventListener('change', function () {
      state.scale = this.value;
      status(this.value
        ? 'New songs will use the ' + T.SCALES[this.value].name + ' scale.'
        : 'Scale back to whatever suits the style and mood.');
    });

    el('chordRate').addEventListener('change', function () {
      state.chordRate = parseInt(this.value, 10);
      status(state.chordRate
        ? 'Chords will change every ' + (state.chordRate === 1 ? 'bar.' : state.chordRate + ' bars.')
        : 'Chord changes back to whatever suits the style.');
    });

    el('meterSelect').addEventListener('change', function () {
      state.meter = this.value;
      status(this.value
        ? 'New songs will be in ' + this.value + '.'
        : 'Time signature back to whatever suits the style.');
    });

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
      bpm: opts.bpm !== undefined ? opts.bpm : state.bpm,
      meter: opts.meter !== undefined ? opts.meter : state.meter,
      scale: opts.scale !== undefined ? opts.scale : state.scale,
      barsPerChord: opts.barsPerChord !== undefined ? opts.barsPerChord : state.chordRate,
      humanise: opts.humanise !== undefined ? opts.humanise : state.humanise
    };

    let song;
    try {
      song = C.compose(cfg);
    } catch (err) {
      status('Could not compose that one: ' + err.message, true);
      throw err;
    }

    takeSong(song, { autoplay: opts.autoplay });
    status('"' + song.title + '" — ' + song.keyName + ', ' + song.bpm + ' BPM, seed ' + song.seed);
    return song;
  }

  /**
   * Put a song on the stand: wire it to the player, the editor, the mixer and
   * every panel. Composing a new song and loading a saved one differ only in
   * where the song came from, so they share this from here on.
   */
  function takeSong(song, opts) {
    opts = opts || {};
    state.song = song;
    window.__song = song;          // handle for the test suites
    if (!opts.keepFlags) {
      state.locked = {};
      state.edited = {};
    }
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
      if (editorSends) editorSends();
      syncEditUI();
    }
    refreshAutomationUI();
    updateHash();

    if (opts.autoplay !== false) {
      player.play(0);
      setPlayIcon(true);
    } else {
      setPlayIcon(false);
    }
  }

  /** Load a song that was saved rather than composed, mix settings and all. */
  function adoptSong(song, item) {
    applySession(item);                 // before takeSong, so the mixer draws restored
    takeSong(song, { keepFlags: true });
    if (editorSends) editorSends();
    const touched = Object.keys(state.edited).filter(function (k) { return state.edited[k]; });
    status('"' + song.title + '" loaded' +
      (touched.length ? ' — with your edits to the ' + touched.map(function (k) {
        return colorLabel(k).toLowerCase();
      }).join(', ') + '.' : ' exactly as you saved it.'));
  }

  function songMetaText() {
    const s = state.song;
    // 4/4 is the default everywhere; saying so on every song is noise.
    const meter = s.meter && s.meter !== '4/4' ? ' · ' + s.meter : '';
    return s.genre.name + ' · ' + s.mood.name + ' · ' + s.keyName + meter + ' · ' + s.bpm + ' BPM · ' +
      s.bars + ' bars · seed ' + s.seed;
  }

  function renderSong() {
    const s = state.song;
    el('songTitle').textContent = s.title;
    el('songMeta').textContent = songMetaText();
    el('timeTotal').textContent = fmtTime(s.duration);
    el('seedInput').value = s.seed;
    el('grooveSelect').value = s.groove || '';
    el('glueAmt').value = String(Math.round((s.glue || 0) * 100));
    el('modKind').value = s.modFx || 'flanger';
    el('colourFx').value = s.colourFx || 'ring';
    el('revKind').value = s.revKind || 'room';
    el('revSize').value = String(Math.round((s.revSize || 2.6) * 100));
    el('revSizeVal').textContent = (s.revSize || 2.6).toFixed(1) + 's';
    el('delKind').value = s.delKind || 'digital';
    el('pumpAmt').value = String(Math.round((s.sidechain || 0) * 100));
    el('pumpSpeed').value = String(Math.round((s.duckSpeed === undefined ? 0.5 : s.duckSpeed) * 100));
    el('chopRate').value = String(s.chopRate || 2);
    el('glueMulti').value = s.glueMulti ? '1' : '0';
    el('delDiv').value = String(s.delDiv === undefined ? 0.375 : s.delDiv);
    el('delFb').value = String(Math.round((s.delFb === undefined ? 0.34 : s.delFb) * 100));
    el('delFbVal').textContent = Math.round((s.delFb === undefined ? 0.34 : s.delFb) * 100) + '%';
    [['mEqLow', 0], ['mEqMid', 0], ['mEqHigh', 0]].forEach(function (spec) {
      const v = s[spec[0]] || spec[1];
      el(spec[0]).value = String(v);
      el(spec[0] + 'Val').textContent = (v > 0 ? '+' : '') + v + ' dB';
    });
    const w = Math.round((s.width === undefined ? 1 : s.width) * 100);
    el('widthAmt').value = String(w);
    el('widthAmtVal').textContent = w + '%';
    const mb = Math.round((s.monoBass || 0) * 100);
    el('monoBass').value = String(mb);
    el('monoBassVal').textContent = mb + '%';
    state.seedEdited = false;
    buildChordStrip();
    buildArrange();
    syncSongControls();
    resizeRoll();
  }

  const ROMAN_PICK = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII'];

  function buildChordStrip() {
    const strip = el('chordStrip');
    strip.innerHTML = '';
    const s = state.song;
    // Every chord in the song, so every one of them can be changed.
    s.chords.forEach(function (ch, i) {
      const d = document.createElement('button');
      d.type = 'button';
      d.className = 'chord-cell';
      d.dataset.index = String(i);
      d.title = 'Change this chord';
      d.innerHTML = '<div class="chord-name">' + escapeHtml(ch.name) + '</div>' +
                    '<div class="chord-roman">' + escapeHtml(ch.roman) + '</div>';
      d.addEventListener('click', function () { openChordPicker(i, d); });
      strip.appendChild(d);
    });
  }

  /** A little menu of the seven chords in this key, showing what each becomes. */
  function openChordPicker(index, anchor) {
    closeChordPicker();
    const song = state.song;
    const chord = song.chords[index];
    const menu = document.createElement('div');
    menu.className = 'chord-menu';
    menu.id = 'chordMenu';

    const head = document.createElement('div');
    head.className = 'chord-menu-head';
    head.textContent = 'Bar ' + (chord.bar + 1) + ' · ' + chord.name;
    menu.appendChild(head);

    const degrees = song.scaleSteps.length;
    for (let deg = 0; deg < degrees; deg++) {
      const built = T.sweetenChord(T.buildChord(song.scaleSteps, T.midi(song.rootPc, 4), deg, chord.shape));
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'chord-opt' + (deg === chord.degree ? ' on' : '');
      b.innerHTML = '<span class="opt-roman">' + (ROMAN_PICK[deg] || (deg + 1)) + '</span>' +
                    '<span class="opt-name">' + escapeHtml(T.chordName(built)) + '</span>';
      (function (d) {
        b.addEventListener('click', function () {
          if (editor) editor.pushSongHistory();
          if (C.setChordDegree(song, index, d)) {
            player.refresh();
            markRollDirty();
            buildChordStrip();
            status('Bar ' + (chord.bar + 1) + ' is now ' + song.chords[index].name +
              ' — the parts moved with it.');
          }
          closeChordPicker();
        });
      })(deg);
      menu.appendChild(b);
    }

    document.body.appendChild(menu);
    const r = anchor.getBoundingClientRect();
    const top = r.bottom + window.scrollY + 6;
    const left = Math.max(8, Math.min(window.innerWidth - menu.offsetWidth - 8, r.left + window.scrollX));
    menu.style.top = top + 'px';
    menu.style.left = left + 'px';

    setTimeout(function () {
      document.addEventListener('pointerdown', chordMenuOutside);
    }, 0);
  }

  function chordMenuOutside(e) {
    const menu = el('chordMenu');
    if (menu && !menu.contains(e.target)) closeChordPicker();
  }

  function closeChordPicker() {
    const menu = el('chordMenu');
    if (menu) menu.remove();
    document.removeEventListener('pointerdown', chordMenuOutside);
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

      const solo = document.createElement('button');
      solo.type = 'button';
      solo.className = 'solo-btn';
      solo.textContent = 'S';
      solo.title = 'Hear ' + meta.label.toLowerCase() + ' on its own';
      solo.setAttribute('aria-label', 'Solo ' + meta.label);
      solo.addEventListener('click', function () {
        const next = !player.mix[meta.id].solo;
        player.setTrack(meta.id, { solo: next });
        syncSolo();
        status(next ? meta.label + ' on its own.' : 'Back to the full mix.');
      });
      row.appendChild(solo);

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
      onStructure: function () {
        structureChanged('Arrangement restored.');
      },
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
        if (editorSends) editorSends();
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

    el('diceSound').addEventListener('click', function () {
      if (!state.song) return;
      const group = G.PRESET_GROUPS[editor.track] || G.PRESET_GROUPS.lead;
      const current = state.song.presetOverride[editor.track];
      const pool = group.filter(function (n) { return n !== current; });
      if (!pool.length) return;
      const pick = pool[Math.floor(Math.random() * pool.length)];
      state.song.presetOverride[editor.track] = pick;
      buildSoundPicker();
      status(colorLabel(editor.track) + ' now plays ' + (G.PRESET_LABEL[pick] || pick) + '.');
    });

    [['octDown', -1, 'down'], ['octUp', 1, 'up']].forEach(function (spec) {
      el(spec[0]).addEventListener('click', function () {
        if (!state.song) return;
        editor.pushHistory(editor.track);
        if (!C.shiftOctave(state.song, editor.track, spec[1])) {
          status(colorLabel(editor.track) + ' cannot go any further ' + spec[2] + '.');
          return;
        }
        state.edited[editor.track] = true;
        player.refresh();
        markRollDirty();
        editor.refit();
        syncEditUI();
        status(colorLabel(editor.track) + ' moved an octave ' + spec[2] + '.');
      });
    });

    /*
     * The per-part effects rack.
     *
     * Every control here is the same shape: a slider, a live readout, and one
     * field on the track's mix settings. Percentages for the sends and the
     * crush; decibels for the three EQ bands, because that is what the numbers
     * on every other mixer in the world mean.
     */
    const FX_CONTROLS = [
      { id: 'revSend', field: 'rev', label: 'Reverb', unit: '%', scale: 100, dflt: 1 },
      { id: 'delSend', field: 'del', label: 'Delay', unit: '%', scale: 100, dflt: 1 },
      { id: 'choSend', field: 'cho', label: 'Chorus', unit: '%', scale: 100, dflt: 0 },
      { id: 'modSend', field: 'mod', label: 'Swirl', unit: '%', scale: 100, dflt: 0 },
      { id: 'autopanAmt', field: 'autopan', label: 'Sweep', unit: '%', scale: 100, dflt: 0 },
      { id: 'chopAmt', field: 'chop', label: 'Chop', unit: '%', scale: 100, dflt: 0 },
      { id: 'colourAmt', field: 'colour', label: 'Colour', unit: '%', scale: 100, dflt: 0 },
      { id: 'crushAmt', field: 'crush', label: 'Crush', unit: '%', scale: 100, dflt: 0 },
      { id: 'compAmt', field: 'comp', label: 'Squeeze', unit: '%', scale: 100, dflt: 0 },
      { id: 'punchAmt', field: 'punch', label: 'Punch', unit: '', scale: 100, dflt: 0 },
      { id: 'eqLow', field: 'eqLow', label: 'Bass', unit: ' dB', scale: 1, dflt: 0 },
      { id: 'eqMid', field: 'eqMid', label: 'Mids', unit: ' dB', scale: 1, dflt: 0 },
      { id: 'eqHigh', field: 'eqHigh', label: 'Treble', unit: ' dB', scale: 1, dflt: 0 }
    ];

    function fxText(spec, raw) {
      const sign = (spec.unit === ' dB' || spec.unit === '') && raw > 0 ? '+' : '';
      return sign + raw + spec.unit;
    }

    function syncSends() {
      const m = player.mix[editor.track] || {};
      const who = el('fxRackWho');
      if (who) who.textContent = colorLabel(editor.track).toLowerCase();
      FX_CONTROLS.forEach(function (spec) {
        const v = m[spec.field] === undefined ? spec.dflt : m[spec.field];
        const raw = Math.round(v * spec.scale);
        el(spec.id).value = String(raw);
        const outEl = el(spec.id + 'Val');
        if (outEl) outEl.textContent = fxText(spec, raw);
      });
    }
    editorSends = syncSends;

    FX_CONTROLS.forEach(function (spec) {
      el(spec.id).addEventListener('input', function () {
        const raw = parseInt(this.value, 10);
        const opts = {};
        opts[spec.field] = raw / spec.scale;
        player.setTrack(editor.track, opts);
        const outEl = el(spec.id + 'Val');
        if (outEl) outEl.textContent = fxText(spec, raw);
      });
      el(spec.id).addEventListener('change', function () {
        status(spec.label + ' on the ' + colorLabel(editor.track).toLowerCase() + ': ' +
          fxText(spec, parseInt(this.value, 10)) + '.');
      });
    });

    el('fxReset').addEventListener('click', function () {
      const opts = {};
      FX_CONTROLS.forEach(function (spec) { opts[spec.field] = spec.dflt; });
      player.setTrack(editor.track, opts);
      syncSends();
      status(colorLabel(editor.track) + ' back to its plain sound.');
    });

    el('undoBtn').addEventListener('click', function () {
      if (!editor.undo()) status('Nothing left to undo.');
    });
    el('redoBtn').addEventListener('click', function () {
      if (!editor.redo()) status('Nothing to redo.');
    });

    [['simplifyBtn', -1, 'thinned out'], ['complicateBtn', 1, 'filled in']].forEach(function (spec) {
      el(spec[0]).addEventListener('click', function () {
        if (!state.song) return;
        editor.pushHistory(editor.track);
        const label = colorLabel(editor.track);
        const before = state.song.tracks[editor.track].length;
        if (!C.adjustDensity(state.song, editor.track, spec[1])) {
          status(label + ' is as ' + (spec[1] < 0 ? 'simple' : 'busy') + ' as it goes.');
          return;
        }
        state.edited[editor.track] = true;
        player.refresh();
        markRollDirty();
        editor.refit();
        syncEditUI();
        status(label + ' ' + spec[2] + ' — ' + before + ' notes → ' +
          state.song.tracks[editor.track].length + '. Ctrl+Z puts it back.');
      });
    });

    /* Harmony and octave doubling are arrangement, not effects: they add notes
       to the part and land in the undo history like any other edit, and they
       come out in the MIDI export because they really are notes. */
    [['harmoniseBtn', 'harmonise', 2, 'a third above'],
     ['harmoniseLowBtn', 'harmonise', -4, 'a sixth below'],
     ['doubleDownBtn', 'double', -1, 'an octave below']].forEach(function (spec) {
      el(spec[0]).addEventListener('click', function () {
        if (!state.song) return;
        const label = colorLabel(editor.track);
        if (editor.track === 'drums') {
          status('Drums have no tune to harmonise — pick a melodic part first.');
          return;
        }
        editor.pushHistory(editor.track);
        const before = state.song.tracks[editor.track].length;
        const ok = spec[1] === 'harmonise'
          ? C.harmonise(state.song, editor.track, spec[2])
          : C.doubleOctave(state.song, editor.track, spec[2]);
        if (!ok) {
          status('Nothing to add to ' + label.toLowerCase() + ' — it is empty, or the ' +
            'harmony would fall off the end of the keyboard.');
          return;
        }
        state.edited[editor.track] = true;
        player.refresh();
        markRollDirty();
        editor.refit();
        syncEditUI();
        status(label + ' doubled ' + spec[3] + ' — ' + before + ' notes → ' +
          state.song.tracks[editor.track].length + '. Ctrl+Z puts it back.');
      });
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
    window.addEventListener('resize', function () {
      editor.resize();
      if (autoLane) { autoLane.resize(); autoLane.draw(); }
    });
    editor.resize();
  }

  let editorSoundPicker = null;
  let editorSends = null;

  /* ------------------------------------------------------------------ *
   * Automation
   * ------------------------------------------------------------------ */

  function buildAutomation() {
    autoLane = new window.Automation({
      canvas: el('autoLane'),
      getSong: function () { return state.song; },
      getBeat: function () { return state.song ? player.currentBeat() : -1; },
      /* Every gesture is a whole-song change, so it goes on the same undo
         stack as arranging — one Ctrl+Z takes back one move, wherever it
         was made. */
      onBeforeChange: function () { if (editor) editor.pushSongHistory(); },
      onChange: function () {
        player.refreshAutomation();
        syncEditUI();
        el('autoHint').textContent = autoLane.describe();
      }
    });

    el('autoLanes').addEventListener('click', function (ev) {
      const b = ev.target.closest('button[data-lane]');
      if (!b) return;
      Array.prototype.forEach.call(this.children, function (c) {
        c.classList.toggle('on', c === b);
      });
      autoLane.setLane(b.dataset.lane);
      el('autoHint').textContent = autoLane.describe();
    });

    el('autoLane').parentNode.querySelector('.auto-shapes')
      .addEventListener('click', function (ev) {
        const b = ev.target.closest('button[data-shape]');
        if (!b || !state.song) return;
        if (editor) editor.pushSongHistory();
        const lane = C.applyShape(state.song, b.dataset.shape);
        if (!lane) return;
        showLane(lane);
        player.refreshAutomation();
        syncEditUI();
        status(b.textContent.trim() + ' — drag the points to taste.');
      });

    el('autoClear').addEventListener('click', function () {
      if (!state.song) return;
      if (editor) editor.pushSongHistory();
      C.clearLane(state.song, autoLane.lane);
      autoLane.draw();
      player.refreshAutomation();
      syncEditUI();
      status('Lane cleared — that setting holds still again.');
    });

    el('pingBtn').addEventListener('click', function () {
      if (!state.song) return;
      const on = !state.song.pingpong;
      player.setPingPong(on);
      this.classList.toggle('on', on);
      status(on ? 'Echoes now bounce left and right.' : 'Echoes back in the middle.');
    });
  }

  function showLane(lane) {
    if (!autoLane) return;
    autoLane.setLane(lane);
    Array.prototype.forEach.call(el('autoLanes').children, function (c) {
      c.classList.toggle('on', c.dataset.lane === lane);
    });
    el('autoHint').textContent = autoLane.describe();
  }

  function refreshAutomationUI() {
    if (!autoLane || !state.song) return;
    C.ensureAutomation(state.song);
    autoLane.resize();
    autoLane.draw();
    el('autoHint').textContent = autoLane.describe();
    el('pingBtn').classList.toggle('on', !!state.song.pingpong);
  }

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
   * Arranging
   * ------------------------------------------------------------------ */

  /** Everything that has to catch up after the song's shape changes. */
  function structureChanged(msg) {
    player.refresh();
    markRollDirty();
    if (editor) {
      editor.scrollTo(Math.min(editor.startBar, editor.maxStartBar()));
      editor.refit();
    }
    if (player.playing) player.seek(Math.min(player.currentBeat(), state.song.totalBeats - 0.01));
    buildChordStrip();
    buildArrange();
    refreshAutomationUI();
    player.refreshAutomation();
    syncEditUI();
    el('songMeta').textContent = songMetaText();
    el('timeTotal').textContent = fmtTime(state.song.duration);
    if (msg) status(msg);
  }

  function buildArrange() {
    const box = el('arrange');
    if (!box || !state.song) return;
    box.innerHTML = '';
    const secs = state.song.sections;

    secs.forEach(function (sec, i) {
      const card = document.createElement('div');
      card.className = 'sec-card' + (sec.type === 'chorus' ? ' chorus' : '');
      card.dataset.index = String(i);

      const name = document.createElement('div');
      name.className = 'sec-name';
      name.textContent = sec.name;
      card.appendChild(name);

      const bars = document.createElement('div');
      bars.className = 'sec-bars';
      bars.textContent = sec.bars + ' BARS';
      card.appendChild(bars);

      const btns = document.createElement('div');
      btns.className = 'sec-btns';

      function button(label, title, cls, disabled, run) {
        const b = document.createElement('button');
        b.type = 'button';
        b.textContent = label;
        b.title = title;
        b.setAttribute('aria-label', title);
        if (cls) b.className = cls;
        b.disabled = !!disabled;
        b.addEventListener('click', function () {
          if (editor) editor.pushSongHistory();
          run();
        });
        btns.appendChild(b);
      }

      button('◀', 'Move ' + sec.name + ' earlier', '', i === 0, function () {
        if (C.moveSection(state.song, i, -1)) structureChanged(sec.name + ' moved earlier.');
      });
      button('⧉', 'Duplicate ' + sec.name, '', false, function () {
        if (C.duplicateSection(state.song, i)) structureChanged(sec.name + ' duplicated.');
      });
      button('✕', 'Delete ' + sec.name, 'del', secs.length <= 1, function () {
        if (C.deleteSection(state.song, i)) structureChanged(sec.name + ' deleted.');
        else status('A song needs at least one section.');
      });
      button('▶', 'Move ' + sec.name + ' later', '', i === secs.length - 1, function () {
        if (C.moveSection(state.song, i, 1)) structureChanged(sec.name + ' moved later.');
      });

      card.appendChild(btns);
      box.appendChild(card);
    });
  }

  function syncSolo() {
    Array.prototype.forEach.call(el('mixer').children, function (row) {
      const m = player.mix[row.dataset.id];
      const btn = row.querySelector('.solo-btn');
      if (btn && m) btn.classList.toggle('on', !!m.solo);
    });
  }

  /* ------------------------------------------------------------------ *
   * Tempo, key and volume — changing the song you already have
   * ------------------------------------------------------------------ */

  function syncSongControls() {
    if (!state.song) return;
    el('liveTempo').value = String(state.song.bpm);
    el('liveTempoVal').textContent = state.song.bpm;
    el('keyVal').textContent = T.NOTE_NAMES[state.song.rootPc];
  }

  function bindSongControls() {
    /* The groove is a playback setting, so it changes what you hear without
       rewriting a note — moving it mid-listen is the whole point. */
    /* The swirl type changes the shape of the bus, not a gain on it, so this is
       one of the few controls that genuinely needs the graph rebuilding. */
    el('modKind').addEventListener('change', function () {
      if (!state.song) return;
      state.song.modFx = this.value;
      const at = player.currentBeat();
      const was = player.playing;
      player.stop();
      if (was) player.play(at);
      status('Swirl is now a ' + this.options[this.selectedIndex].text.toLowerCase() + '.');
    });

    /* Same for the colour type: ring, fold and wah are three different sets of
       nodes, so switching between them rebuilds. The Colour slider itself does
       not — it blends, and blending is live. */
    el('colourFx').addEventListener('change', function () {
      if (!state.song) return;
      state.song.colourFx = this.value;
      const at = player.currentBeat();
      const was = player.playing;
      player.stop();
      if (was) player.play(at);
      status('Colour is now ' + this.options[this.selectedIndex].text.toLowerCase().replace(/ \(.*\)/, '') + '.');
    });

    /* The reverb's shape and the delay's timing are built into the graph, so
       these rebuild it — quickly, and from the same beat you were on. */
    function rebuildAudio() {
      const at = player.currentBeat();
      const was = player.playing;
      player.stop();
      if (was) player.play(at);
    }

    [['revKind', 'revKind', null], ['delDiv', 'delDiv', parseFloat]].forEach(function (spec) {
      el(spec[0]).addEventListener('change', function () {
        if (!state.song) return;
        state.song[spec[1]] = spec[2] ? spec[2](this.value) : this.value;
        rebuildAudio();
        status(this.options[this.selectedIndex].text + ' — ' +
          (spec[0] === 'revKind' ? 'reverb.' : 'echo timing.'));
      });
    });

    /* All four echo lines are built every time, so the type is a gain change:
       the repeats already ringing out finish instead of being cut off. */
    el('delKind').addEventListener('change', function () {
      if (!state.song) return;
      player.setDelayKind(this.value);
      status('Echo is now ' +
        this.options[this.selectedIndex].text.toLowerCase().replace(/ \(.*\)/, '') + '.');
    });

    /* The pump is read fresh on every kick, so both of these move live — and
       the gain node is built whether or not the style pumps, so a song that
       started with none can be given some without rebuilding anything. */
    [['pumpAmt', 'depth'], ['pumpSpeed', 'speed']].forEach(function (spec) {
      el(spec[0]).addEventListener('input', function () {
        if (!state.song) return;
        const v = parseInt(this.value, 10) / 100;
        player.setSidechain(spec[1] === 'depth' ? v : undefined,
                            spec[1] === 'speed' ? v : undefined);
      });
      el(spec[0]).addEventListener('change', function () {
        status((spec[1] === 'depth' ? 'Pump: ' : 'Pump speed: ') + this.value + '%.');
      });
    });

    /* Both glue chains are built every time, so this is a gain change. */
    el('glueMulti').addEventListener('change', function () {
      if (!state.song) return;
      const on = this.value === '1';
      player.setGlueMulti(on);
      status(on
        ? 'Glue now squeezes bass, middle and treble separately.'
        : 'Glue is back to one compressor across everything.');
    });

    /* The chop grid is read on each step, so the rate changes on the next one. */
    el('chopRate').addEventListener('change', function () {
      if (!state.song) return;
      state.song.chopRate = parseInt(this.value, 10);
      status('Chop is now in ' + this.options[this.selectedIndex].text + ' notes.');
    });

    /* Master EQ is three AudioParams, so it moves live. */
    [['mEqLow', 'mEqLow'], ['mEqMid', 'mEqMid'], ['mEqHigh', 'mEqHigh']].forEach(function (spec) {
      el(spec[0]).addEventListener('input', function () {
        if (!state.song) return;
        const v = parseInt(this.value, 10);
        state.song[spec[1]] = v;
        el(spec[0] + 'Val').textContent = (v > 0 ? '+' : '') + v + ' dB';
        player.applyMix();
      });
    });

    /* Width and mono bass change the shape of the master chain rather than a
       value on it, so they rebuild — on release, not on every pixel of drag. */
    [['widthAmt', 'width', 100], ['monoBass', 'monoBass', 100]].forEach(function (spec) {
      el(spec[0]).addEventListener('input', function () {
        el(spec[0] + 'Val').textContent = this.value + '%';
      });
      el(spec[0]).addEventListener('change', function () {
        if (!state.song) return;
        state.song[spec[1]] = parseInt(this.value, 10) / spec[2];
        rebuildAudio();
      });
    });

    [['revSize', 'revSize', 100, 's'], ['delFb', 'delFb', 100, '%']].forEach(function (spec) {
      const input = el(spec[0]);
      input.addEventListener('input', function () {
        const v = parseInt(this.value, 10) / spec[2];
        el(spec[0] + 'Val').textContent = spec[3] === 's' ? v.toFixed(1) + 's'
                                                          : Math.round(v * 100) + '%';
      });
      input.addEventListener('change', function () {
        if (!state.song) return;
        state.song[spec[1]] = parseInt(this.value, 10) / spec[2];
        rebuildAudio();
      });
    });

    const glue = el('glueAmt');
    glue.addEventListener('input', function () {
      if (!state.song) return;
      state.song.glue = parseInt(this.value, 10) / 100;
      player.applyMix();
    });
    glue.addEventListener('change', function () {
      status(this.value === '0' ? 'Glue off — the parts stand on their own.'
        : 'Glue at ' + this.value + '% — the mix breathes as one thing.');
    });

    el('clickBtn').addEventListener('click', function () {
      const on = !player.metronome;
      player.setMetronome(on, true);
      this.classList.toggle('on', on);
      status(on ? 'Click on — a bar is counted in when you press play.' : 'Click off.');
      // Restart so a count-in actually counts you in.
      if (player.playing) { const at = player.currentBeat(); player.stop(); player.play(at); }
    });

    const groove = el('grooveSelect');
    const blank = document.createElement('option');
    blank.value = '';
    blank.textContent = 'As the style plays it';
    groove.appendChild(blank);
    Object.keys(C.GROOVES).forEach(function (id) {
      const o = document.createElement('option');
      o.value = id;
      o.textContent = C.GROOVES[id].name;
      groove.appendChild(o);
    });
    groove.addEventListener('change', function () {
      if (!state.song) return;
      state.song.groove = this.value;
      player.refresh();                  // re-times the score; no note is changed
      markRollDirty();
      status(this.value ? 'Feel: ' + C.GROOVES[this.value].name + '.'
                        : 'Back to the style\u2019s own feel.');
    });

    const tempo = el('liveTempo');
    tempo.addEventListener('input', function () {
      el('liveTempoVal').textContent = this.value;
    });
    // Retime on release: rebuilding the graph on every pixel of drag would stutter.
    ['change', 'pointerup'].forEach(function (ev) {
      tempo.addEventListener(ev, function () {
        if (!state.song) return;
        player.setTempo(parseInt(tempo.value, 10));
        setPlayIcon(player.playing);
        renderSong();
        status('Now ' + state.song.bpm + ' BPM — same song, new tempo.');
      });
    });

    function shift(by) {
      if (!state.song) return;
      const ok = C.transpose(state.song, by);
      if (!ok) { status('That would push a part off the end of the keyboard.', true); return; }
      player.refresh();
      markRollDirty();
      if (editor) editor.refit();
      buildChordStrip();
      syncSongControls();
      el('songMeta').textContent = songMetaText();
      status('Now in ' + state.song.keyName + '.');
    }
    el('keyDown').addEventListener('click', function () { shift(-1); });
    el('keyUp').addEventListener('click', function () { shift(1); });

    const vol = el('masterVol');
    vol.addEventListener('input', function () {
      player.setVolume(parseInt(this.value, 10) / 100);
    });
    player.setVolume(parseInt(vol.value, 10) / 100);
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
      const bpb = song.beatsPerBar || 4;
      const x0 = xOf(sec.startBar * bpb);
      const x1 = xOf((sec.startBar + sec.bars) * bpb);
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
  let lastSection = null;

  function frame() {
    if (state.song) {
      const beat = player.currentBeat();
      const spb = 60 / state.song.bpm;

      if (!state.seekDragging) {
        el('seek').value = String(Math.round((beat / state.song.totalBeats) * 1000));
        el('timeNow').textContent = fmtTime(beat * spb);
      }

      const playingSec = C.sectionOf(state.song, beat);
      if (playingSec !== lastSection) {
        lastSection = playingSec;
        Array.prototype.forEach.call(el('arrange').children, function (card, i) {
          card.classList.toggle('playing', state.song.sections[i] === playingSec);
        });
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
      if (autoLane && !el('mixPanel').hidden) autoLane.draw();
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
          el('exportBar').style.width = '75%';
          /* Measure before encoding, so the figure reported is the finished
             file's. Normalising is a single multiply, held short of clipping —
             see gainForTarget. */
          const target = el('loudTarget').value;
          let heard = X.loudness(buffer);
          let note = '';
          if (target) {
            const g = X.gainForTarget(heard, parseFloat(target));
            X.applyGain(buffer, g.gain);
            heard = { lufs: g.reached, peak: heard.peak * g.gain };
            note = g.limited
              ? ' (as loud as its peaks allow — ' + heard.lufs.toFixed(1) + ' LUFS)'
              : ' at ' + heard.lufs.toFixed(1) + ' LUFS';
          } else if (isFinite(heard.lufs)) {
            note = ' at ' + heard.lufs.toFixed(1) + ' LUFS, peaking ' +
              (20 * Math.log10(heard.peak || 1e-9)).toFixed(1) + ' dB';
          }
          el('exportBar').style.width = '90%';
          const blob = X.encodeWav(buffer);
          el('exportBar').style.width = '100%';
          return X.deliver(blob, X.safeName(state.song.title) + '-' + X.safeName(state.song.seed) + '.wav')
            .then(function (r) {
              status(r === 'declined' ? 'Download cancelled.'
                : r === 'busy' ? 'Another download is still open — try again in a moment.'
                : r === 'unavailable' ? 'This browser would not accept the file.'
                : 'Audio saved' + note + '.');
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

  const LIB_MAX = 30;

  function loadLibrary() {
    try {
      const raw = localStorage.getItem(STORE_KEY);
      const list = raw ? JSON.parse(raw) : [];
      return Array.isArray(list) ? list : [];
    } catch (e) { return []; }
  }

  /**
   * Write the library back.
   *
   * A whole song is tens of kilobytes rather than the handful a seed took, so
   * the browser's storage really can fill up. When it does, drop the oldest
   * entry and try again rather than silently losing the save — and say so if
   * even that is not enough.
   *
   * Returns 'ok', 'trimmed' (older songs were dropped to fit) or 'blocked'.
   */
  function saveLibrary(list) {
    let out = list.slice(0, LIB_MAX);
    let trimmed = false;
    for (let attempt = 0; attempt < LIB_MAX; attempt++) {
      try {
        localStorage.setItem(STORE_KEY, JSON.stringify(out));
        return trimmed ? 'trimmed' : 'ok';
      } catch (e) {
        if (out.length <= 1) return 'blocked';
        out = out.slice(0, out.length - 1);   // the newest save is the one to keep
        trimmed = true;
      }
    }
    return 'blocked';
  }

  /* An entry is either a whole song (v2) or one of the seed-only records saved
     before songs could be stored in full. Both still load. */
  function isFullSave(item) { return item && item.v === C.SAVE_VERSION && !!item.tracks; }

  function libraryLabel(item) {
    return {
      title: item.title,
      genre: item.genre,
      keyName: item.keyName || '',
      bpm: item.bpm,
      seed: item.seed,
      bars: item.bars || 0
    };
  }

  /** Everything about the session that lives outside the song itself. */
  function packSession() {
    const mix = {};
    E.TRACKS.forEach(function (t) {
      const m = player.mix[t] || {};
      mix[t] = {
        volume: m.volume, muted: m.muted, rev: m.rev, del: m.del, cho: m.cho,
        eqLow: m.eqLow, eqMid: m.eqMid, eqHigh: m.eqHigh, crush: m.crush
      };
    });
    return {
      mix: mix,
      length: state.length,
      edited: Object.assign({}, state.edited),
      locked: Object.assign({}, state.locked)
    };
  }

  function applySession(item) {
    state.length = item.length || 'medium';
    state.edited = Object.assign({}, item.edited || {});
    state.locked = Object.assign({}, item.locked || {});
    E.TRACKS.forEach(function (t) {
      const saved = (item.mix || {})[t];
      if (!saved) return;
      const opts = {};
      ['volume', 'muted', 'rev', 'del', 'cho', 'eqLow', 'eqMid', 'eqHigh', 'crush'].forEach(function (k) {
        if (saved[k] !== undefined) opts[k] = saved[k];
      });
      // Solo is a listening state, not part of the song — never restore it.
      opts.solo = false;
      player.setTrack(t, opts);
    });
  }

  function renderLibrary() {
    const list = loadLibrary();
    el('libraryPanel').hidden = list.length === 0;
    const box = el('library');
    box.innerHTML = '';
    list.forEach(function (item, i) {
      const row = document.createElement('div');
      row.className = 'lib-row';

      const meta = libraryLabel(item);
      const full = isFullSave(item);
      const info = document.createElement('div');
      info.className = 'lib-info';
      info.innerHTML = '<div class="lib-title">' + escapeHtml(meta.title) +
        (full ? '' : ' <span class="lib-tag">seed only</span>') + '</div>' +
        '<div class="lib-sub">' + escapeHtml((G.GENRES[meta.genre] || {}).name || meta.genre) +
        ' · ' + escapeHtml(meta.keyName) + ' · ' + meta.bpm + ' BPM' +
        (full && meta.bars ? ' · ' + meta.bars + ' bars' : '') +
        ' · ' + escapeHtml(meta.seed) + '</div>';
      info.title = full
        ? 'Saved in full — every note, the arrangement and the effects'
        : 'Saved before whole songs could be kept, so this reloads the original generated version';
      row.appendChild(info);

      const load = document.createElement('button');
      load.type = 'button';
      load.className = 'mini-btn';
      load.textContent = 'Load';
      load.addEventListener('click', function () {
        state.genre = item.genre; state.mood = item.mood;
        state.length = item.length || 'medium';
        state.key = item.rootPc === undefined ? item.key : item.rootPc;
        state.bpm = item.bpm;
        syncChips();
        el('seedInput').value = item.seed;

        if (full) {
          const song = C.unpackSong(item);
          if (song) { adoptSong(song, item); return; }
          status('That save could not be read — regenerating it from its seed instead.', true);
        }
        generate({ seed: item.seed, genre: item.genre, mood: item.mood,
                   length: item.length, key: state.key, bpm: item.bpm });
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
      const packed = C.packSong(state.song, packSession());
      const list = loadLibrary();

      /* Saving the same song again replaces it. It used to refuse as a
         duplicate, which made sense when a save was only a seed — but now the
         save holds your edits, and refusing to overwrite means refusing to
         keep the work you just did. */
      const at = list.findIndex(function (x) {
        return x.seed === packed.seed && x.genre === packed.genre && x.mood === packed.mood;
      });
      const replacing = at >= 0;
      if (replacing) list.splice(at, 1);
      list.unshift(packed);

      const result = saveLibrary(list);
      renderLibrary();
      if (result === 'blocked') {
        status('This browser would not store the song — its storage is full or switched off.', true);
      } else if (result === 'trimmed') {
        status('Saved — the oldest songs were dropped to make room.');
      } else {
        status(replacing
          ? 'Saved over the earlier copy — notes, arrangement and effects and all.'
          : 'Saved to this device — notes, arrangement and effects and all.');
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
      if (e.key === 'Escape') { el('helpModal').hidden = true; closeChordPicker(); }
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
    bindSongControls();
    bindExport();
    bindSongActions();
    bindHelp();
    bindRollSeek();
    buildEditor();
    buildAutomation();
    if (editorSoundPicker) editorSoundPicker();
    if (editorSends) editorSends();
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
      el('meterSelect').value = state.meter;
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
