/*
 * editor.js — the hands-on half of the program.
 *
 * A piano roll for the melodic parts and a step grid for the drums, drawn on
 * one canvas. You edit `song.tracks[...]` directly — the same arrays the
 * composer writes and the player reads — so a note you draw plays, renders and
 * exports exactly like a note the composer wrote. There is no separate
 * "user data" model to keep in sync, which is the whole reason the two halves
 * fit together.
 *
 * The grid helps rather than judges: rows in the song's key are shaded, and
 * with "In key" on, a note you draw lands on the nearest note that fits.
 */
(function (global) {
  'use strict';

  const T = global.Theory;
  const BEATS_PER_BAR = 4;
  const STEP_BEATS = 0.25;
  const LABEL_W = 52;
  const MIN_ROWS = 15;
  const MAX_ROWS = 34;

  const DRUM_ROWS = ['kick', 'snare', 'clap', 'hh', 'oh', 'ride', 'tom', 'conga',
                     'perc', 'shaker', 'tamb', 'cowbell', 'crash', 'riser', 'impact'];
  const DRUM_LABEL = {
    kick: 'Kick', snare: 'Snare', clap: 'Clap', hh: 'Hat', oh: 'Open hat',
    tom: 'Tom', perc: 'Perc', shaker: 'Shaker', crash: 'Crash',
    riser: 'Riser', impact: 'Impact', ride: 'Ride', tamb: 'Tambourine',
    cowbell: 'Cowbell', conga: 'Conga'
  };

  function Editor(opts) {
    this.canvas = opts.canvas;
    this.cx = this.canvas.getContext('2d');
    this.getSong = opts.getSong;
    this.player = opts.player;
    this.onChange = opts.onChange || function () {};
    this.onStructure = opts.onStructure || function () {};
    this.colorFor = opts.colorFor || function () { return '#00e5ff'; };

    this.track = 'lead';
    this.tool = 'draw';        // draw | erase
    this.snap = 0.25;          // beats
    this.noteLen = 1;          // beats
    this.inKey = true;
    this.startBar = 0;
    this.bars = 4;
    this.follow = true;

    this.low = 48;
    this.rows = 25;
    this.w = 0;
    this.h = 0;
    this._drag = null;
    this._painted = null;
    this._undo = [];
    this._redo = [];
    this._bind();
  }

  /* ------------------------------------------------------------------ *
   * Undo
   *
   * Snapshots of one track's notes, taken before each gesture. Editing without
   * undo means one careless drag can lose an idea, which is the fastest way to
   * stop trusting a tool.
   * ------------------------------------------------------------------ */

  const HISTORY_MAX = 60;

  function snapshot(song, track) {
    return { track: track, events: JSON.parse(JSON.stringify(song.tracks[track] || [])) };
  }

  /* Rearranging moves every note in the song, so it needs a snapshot of the
     whole thing rather than one track. */
  function snapshotSong(song) {
    return {
      full: true,
      tracks: JSON.parse(JSON.stringify(song.tracks)),
      sections: JSON.parse(JSON.stringify(song.sections)),
      chords: JSON.parse(JSON.stringify(song.chords)),
      bars: song.bars,
      totalBeats: song.totalBeats,
      duration: song.duration
    };
  }

  function restoreSong(song, s) {
    song.tracks = s.tracks;
    song.sections = s.sections;
    song.chords = s.chords;
    song.bars = s.bars;
    song.totalBeats = s.totalBeats;
    song.duration = s.duration;
    // Sections hold their own view of the harmony; re-link it to the restored one.
    song.sections.forEach(function (sec) {
      sec.chords = song.chords.filter(function (c) {
        return c.bar >= sec.startBar && c.bar < sec.startBar + sec.bars;
      });
    });
  }

  Editor.prototype.pushHistory = function (track) {
    const song = this.getSong();
    if (!song) return;
    this._undo.push(snapshot(song, track || this.track));
    if (this._undo.length > HISTORY_MAX) this._undo.shift();
    this._redo.length = 0;
  };

  /** Snapshot the whole song — for arranging, which no single track describes. */
  Editor.prototype.pushSongHistory = function () {
    const song = this.getSong();
    if (!song) return;
    this._undo.push(snapshotSong(song));
    if (this._undo.length > HISTORY_MAX) this._undo.shift();
    this._redo.length = 0;
  };

  Editor.prototype.canUndo = function () { return this._undo.length > 0; };
  Editor.prototype.canRedo = function () { return this._redo.length > 0; };

  Editor.prototype._restore = function (from, to) {
    const song = this.getSong();
    if (!song || !from.length) return false;
    const snap = from.pop();

    if (snap.full) {
      to.push(snapshotSong(song));
      restoreSong(song, snap);
      this.scrollTo(this.startBar);
      this.refit();
      this.onStructure();
      return true;
    }

    to.push(snapshot(song, snap.track));
    song.tracks[snap.track] = snap.events;
    if (snap.track !== this.track) this.setTrack(snap.track);
    else this.refit();
    this.onChange();
    return true;
  };

  Editor.prototype.undo = function () { return this._restore(this._undo, this._redo); };
  Editor.prototype.redo = function () { return this._restore(this._redo, this._undo); };

  Editor.prototype.clearHistory = function () {
    this._undo.length = 0;
    this._redo.length = 0;
  };

  /* ------------------------------------------------------------------ *
   * Geometry
   * ------------------------------------------------------------------ */

  Editor.prototype.isDrums = function () { return this.track === 'drums'; };

  Editor.prototype.resize = function () {
    const dpr = global.devicePixelRatio || 1;
    const rect = this.canvas.getBoundingClientRect();
    this.w = Math.max(1, Math.floor(rect.width));
    this.h = Math.max(1, Math.floor(rect.height));
    this.canvas.width = Math.floor(this.w * dpr);
    this.canvas.height = Math.floor(this.h * dpr);
    this.cx.setTransform(dpr, 0, 0, dpr, 0, 0);
  };

  Editor.prototype.drumRows = function () {
    const song = this.getSong();
    const used = {};
    const evs = song.tracks.drums || [];
    for (let i = 0; i < evs.length; i++) used[evs[i].inst] = true;
    // Always offer the core pieces so there is something to draw on.
    ['kick', 'snare', 'hh'].forEach(function (k) { used[k] = true; });
    return DRUM_ROWS.filter(function (k) { return used[k]; });
  };

  /** Recompute the visible pitch window for the current track. */
  Editor.prototype.refit = function () {
    const song = this.getSong();
    if (!song || this.isDrums()) return;
    const evs = song.tracks[this.track] || [];
    let lo = Infinity, hi = -Infinity;
    for (let i = 0; i < evs.length; i++) {
      if (evs[i].p < lo) lo = evs[i].p;
      if (evs[i].p > hi) hi = evs[i].p;
    }
    if (!isFinite(lo)) {
      const oct = (song.genre[this.track] && song.genre[this.track].octave) || 4;
      lo = T.midi(song.rootPc, oct);
      hi = lo + 12;
    }
    let rows = Math.max(MIN_ROWS, Math.min(MAX_ROWS, (hi - lo) + 5));
    const mid = Math.round((lo + hi) / 2);
    let low = mid - Math.floor(rows / 2);
    if (low > lo - 1) low = lo - 1;
    if (low + rows < hi + 2) rows = Math.min(MAX_ROWS, hi + 2 - low);
    this.low = Math.max(12, low);
    this.rows = rows;
  };

  Editor.prototype.startBeat = function () { return this.startBar * BEATS_PER_BAR; };
  Editor.prototype.spanBeats = function () { return this.bars * BEATS_PER_BAR; };

  Editor.prototype.xOfBeat = function (b) {
    return LABEL_W + ((b - this.startBeat()) / this.spanBeats()) * (this.w - LABEL_W);
  };
  Editor.prototype.beatOfX = function (x) {
    return this.startBeat() + ((x - LABEL_W) / (this.w - LABEL_W)) * this.spanBeats();
  };
  Editor.prototype.rowH = function () {
    const n = this.isDrums() ? this.drumRows().length : this.rows;
    return (this.h - 16) / Math.max(1, n);
  };
  Editor.prototype.yOfRow = function (r) { return 16 + r * this.rowH(); };
  Editor.prototype.rowOfY = function (y) { return Math.floor((y - 16) / this.rowH()); };

  /** Melodic: row 0 is the top (highest pitch). */
  Editor.prototype.pitchOfRow = function (r) { return this.low + (this.rows - 1 - r); };
  Editor.prototype.rowOfPitch = function (p) { return (this.rows - 1) - (p - this.low); };

  Editor.prototype.maxStartBar = function () {
    const song = this.getSong();
    return Math.max(0, song.bars - this.bars);
  };

  /* ------------------------------------------------------------------ *
   * Musical helpers
   * ------------------------------------------------------------------ */

  Editor.prototype.inScale = function (pitch) {
    const song = this.getSong();
    const rel = ((pitch - T.midi(song.rootPc, 4)) % 12 + 12) % 12;
    return song.scaleSteps.indexOf(rel) >= 0;
  };

  Editor.prototype.snapPitch = function (pitch) {
    if (!this.inKey) return pitch;
    const song = this.getSong();
    return T.snapToScale(pitch, song.scaleSteps, T.midi(song.rootPc, 4));
  };

  Editor.prototype.quantize = function (beat) {
    return Math.max(0, Math.round(beat / this.snap) * this.snap);
  };

  /* ------------------------------------------------------------------ *
   * Drawing
   * ------------------------------------------------------------------ */

  Editor.prototype.draw = function () {
    const song = this.getSong();
    if (!song || !this.w) return;
    const cx = this.cx;
    const W = this.w, H = this.h;
    cx.clearRect(0, 0, W, H);

    const gridW = W - LABEL_W;
    const beats = this.spanBeats();
    const start = this.startBeat();
    const rowH = this.rowH();
    const color = this.colorFor(this.track);

    cx.fillStyle = 'rgba(0,0,0,0.32)';
    cx.fillRect(LABEL_W, 16, gridW, H - 16);

    if (this.isDrums()) this._drawDrumRows(rowH);
    else this._drawPitchRows(rowH, color);

    // Beat and bar lines
    for (let b = 0; b <= beats; b += this.snap) {
      const x = this.xOfBeat(start + b);
      const onBar = Math.abs((b % BEATS_PER_BAR)) < 1e-6;
      const onBeat = Math.abs((b % 1)) < 1e-6;
      if (!onBeat && this.snap >= 0.5) continue;
      cx.strokeStyle = onBar ? 'rgba(255,255,255,0.30)'
        : onBeat ? 'rgba(255,255,255,0.14)' : 'rgba(255,255,255,0.055)';
      cx.lineWidth = 1;
      cx.beginPath();
      cx.moveTo(Math.round(x) + 0.5, 16);
      cx.lineTo(Math.round(x) + 0.5, H);
      cx.stroke();
    }

    // Bar numbers and the section this window sits in
    cx.font = '10px ui-monospace, monospace';
    cx.textAlign = 'left';
    for (let b = 0; b < this.bars; b++) {
      const x = this.xOfBeat(start + b * BEATS_PER_BAR);
      cx.fillStyle = 'rgba(200,190,225,0.75)';
      cx.fillText(String(this.startBar + b + 1), x + 4, 11);
    }
    const sec = global.Composer.sectionOf(song, start);
    if (sec) {
      cx.textAlign = 'right';
      cx.fillStyle = 'rgba(255,200,87,0.85)';
      cx.fillText(sec.name.toUpperCase(), W - 6, 11);
      cx.textAlign = 'left';
    }

    if (this.isDrums()) this._drawDrumHits(rowH);
    else this._drawNotes(rowH, color);

    // Playhead
    const beat = this.player.currentBeat();
    if (beat >= start && beat <= start + beats) {
      const px = this.xOfBeat(beat);
      cx.strokeStyle = '#ffffff';
      cx.lineWidth = 1.4;
      cx.beginPath();
      cx.moveTo(px, 16); cx.lineTo(px, H);
      cx.stroke();
    }
  };

  Editor.prototype._drawPitchRows = function (rowH, color) {
    const cx = this.cx;
    const W = this.w;
    cx.font = '9px ui-monospace, monospace';
    for (let r = 0; r < this.rows; r++) {
      const pitch = this.pitchOfRow(r);
      const y = this.yOfRow(r);
      const inKey = this.inScale(pitch);
      const isRoot = ((pitch % 12) + 12) % 12 === this.getSong().rootPc;
      if (isRoot) cx.fillStyle = 'rgba(255,255,255,0.085)';
      else if (inKey) cx.fillStyle = 'rgba(255,255,255,0.038)';
      else cx.fillStyle = 'rgba(0,0,0,0.22)';
      cx.fillRect(LABEL_W, y, W - LABEL_W, rowH - 0.5);

      if (rowH >= 9 && (isRoot || rowH >= 14)) {
        cx.fillStyle = isRoot ? 'rgba(255,255,255,0.8)' : 'rgba(162,151,196,0.75)';
        cx.textAlign = 'right';
        cx.fillText(T.midiToName(pitch), LABEL_W - 6, y + rowH * 0.72);
        cx.textAlign = 'left';
      }
    }
  };

  Editor.prototype._drawDrumRows = function (rowH) {
    const cx = this.cx;
    const rows = this.drumRows();
    cx.font = '10px ui-monospace, monospace';
    for (let r = 0; r < rows.length; r++) {
      const y = this.yOfRow(r);
      cx.fillStyle = r % 2 ? 'rgba(255,255,255,0.028)' : 'rgba(255,255,255,0.055)';
      cx.fillRect(LABEL_W, y, this.w - LABEL_W, rowH - 0.5);
      cx.fillStyle = 'rgba(200,190,225,0.85)';
      cx.textAlign = 'right';
      cx.fillText(DRUM_LABEL[rows[r]] || rows[r], LABEL_W - 6, y + rowH * 0.68);
      cx.textAlign = 'left';
    }
  };

  Editor.prototype._drawNotes = function (rowH, color) {
    const cx = this.cx;
    const song = this.getSong();
    const evs = song.tracks[this.track] || [];
    const start = this.startBeat(), end = start + this.spanBeats();
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      if (e.t + e.d < start || e.t > end) continue;
      const row = this.rowOfPitch(Math.round(e.p));
      if (row < 0 || row >= this.rows) continue;
      const x = this.xOfBeat(Math.max(e.t, start));
      const x2 = this.xOfBeat(Math.min(e.t + e.d, end));
      const y = this.yOfRow(row);
      const h = Math.max(3, rowH - 2);
      cx.fillStyle = color;
      cx.globalAlpha = 0.35 + 0.55 * Math.min(1, e.v);
      cx.fillRect(x, y + 1, Math.max(3, x2 - x), h);
      cx.globalAlpha = 1;
      cx.strokeStyle = 'rgba(0,0,0,0.5)';
      cx.lineWidth = 1;
      cx.strokeRect(Math.round(x) + 0.5, Math.round(y + 1) + 0.5, Math.max(3, x2 - x) - 1, h - 1);
    }
  };

  Editor.prototype._drawDrumHits = function (rowH) {
    const cx = this.cx;
    const song = this.getSong();
    const rows = this.drumRows();
    const evs = song.tracks.drums || [];
    const start = this.startBeat(), end = start + this.spanBeats();
    const stepW = (this.w - LABEL_W) / (this.spanBeats() / STEP_BEATS);
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      if (e.t < start || e.t >= end) continue;
      const r = rows.indexOf(e.inst);
      if (r < 0) continue;
      const x = this.xOfBeat(e.t);
      const y = this.yOfRow(r);
      cx.fillStyle = e.inst === 'kick' ? '#ff2d95' : e.inst === 'snare' || e.inst === 'clap' ? '#ff7ac0' : '#c76bd8';
      cx.globalAlpha = 0.4 + 0.6 * Math.min(1, e.v);
      cx.fillRect(x + 1, y + 2, Math.max(3, stepW - 2), Math.max(3, rowH - 4));
      cx.globalAlpha = 1;
    }
  };

  /* ------------------------------------------------------------------ *
   * Editing
   * ------------------------------------------------------------------ */

  Editor.prototype.noteAt = function (beat, pitch) {
    const evs = this.getSong().tracks[this.track] || [];
    for (let i = evs.length - 1; i >= 0; i--) {
      const e = evs[i];
      if (Math.round(e.p) === pitch && beat >= e.t - 0.02 && beat <= e.t + e.d) return e;
    }
    return null;
  };

  Editor.prototype.drumAt = function (beat, inst) {
    const evs = this.getSong().tracks.drums || [];
    const half = STEP_BEATS / 2;
    for (let i = 0; i < evs.length; i++) {
      if (evs[i].inst === inst && Math.abs(evs[i].t - beat) < half) return evs[i];
    }
    return null;
  };

  Editor.prototype.remove = function (ev) {
    const arr = this.getSong().tracks[this.track];
    const i = arr.indexOf(ev);
    if (i >= 0) arr.splice(i, 1);
  };

  Editor.prototype._pos = function (e) {
    const rect = this.canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
  };

  Editor.prototype._bind = function () {
    const self = this;

    this.canvas.addEventListener('pointerdown', function (e) {
      const song = self.getSong();
      if (!song) return;
      e.preventDefault();
      const p = self._pos(e);
      if (p.x < LABEL_W) return;
      /* Once you start editing, stop chasing the playhead. Otherwise the view
         scrolls out from under you mid-edit and the next click lands in a
         different bar than the one you were looking at. */
      if (self.follow && self.player.playing) {
        self.follow = false;
        if (self.onFollowOff) self.onFollowOff();
      }
      self.canvas.setPointerCapture(e.pointerId);
      self.pushHistory();
      self._painted = {};
      if (self.isDrums()) self._drumDown(p);
      else self._noteDown(p);
      self.draw();
    });

    this.canvas.addEventListener('pointermove', function (e) {
      if (!self._drag && !self._painted) return;
      const p = self._pos(e);
      if (self.isDrums()) self._drumMove(p);
      else self._noteMove(p);
      self.draw();
    });

    function end(e) {
      if (self._drag || self._painted) {
        self._drag = null;
        self._painted = null;
        self.onChange();
      }
      try { self.canvas.releasePointerCapture(e.pointerId); } catch (err) { /* already gone */ }
    }
    this.canvas.addEventListener('pointerup', end);
    this.canvas.addEventListener('pointercancel', end);
  };

  Editor.prototype._noteDown = function (p) {
    const beat = this.beatOfX(p.x);
    const row = this.rowOfY(p.y);
    if (row < 0 || row >= this.rows) return;
    const pitch = this.pitchOfRow(row);
    const hit = this.noteAt(beat, pitch);

    if (this.tool === 'erase') {
      if (hit) this.remove(hit);
      return;
    }
    if (hit) {
      // Near the right edge means resize, anywhere else means move.
      const rightEdge = this.xOfBeat(hit.t + hit.d);
      this._drag = (p.x > rightEdge - 10)
        ? { mode: 'resize', ev: hit }
        : { mode: 'move', ev: hit, grabBeat: beat - hit.t };
      return;
    }
    const t = this.quantize(beat);
    const ev = { t: t, d: this.noteLen, p: this.snapPitch(pitch), v: 0.8 };
    this.getSong().tracks[this.track].push(ev);
    this._drag = { mode: 'resize', ev: ev };
    this.audition(ev.p);
  };

  Editor.prototype._noteMove = function (p) {
    const d = this._drag;
    if (!d) return;
    const beat = this.beatOfX(p.x);
    if (d.mode === 'resize') {
      d.ev.d = Math.max(this.snap, this.quantize(beat - d.ev.t + this.snap * 0.5));
    } else {
      const row = this.rowOfY(p.y);
      if (row >= 0 && row < this.rows) {
        const next = this.snapPitch(this.pitchOfRow(row));
        if (next !== d.ev.p) { d.ev.p = next; this.audition(next); }
      }
      d.ev.t = this.quantize(beat - d.grabBeat);
    }
    const total = this.getSong().totalBeats;
    if (d.ev.t + d.ev.d > total) d.ev.t = Math.max(0, total - d.ev.d);
  };

  Editor.prototype._drumDown = function (p) {
    const rows = this.drumRows();
    const r = this.rowOfY(p.y);
    if (r < 0 || r >= rows.length) return;
    const inst = rows[r];
    const beat = Math.round(this.beatOfX(p.x) / STEP_BEATS) * STEP_BEATS;
    const key = inst + '@' + beat;
    this._painted[key] = true;
    const hit = this.drumAt(beat, inst);
    if (this.tool === 'erase') {
      if (hit) {
        const arr = this.getSong().tracks.drums;
        arr.splice(arr.indexOf(hit), 1);
      }
      return;
    }
    if (hit) {
      const arr = this.getSong().tracks.drums;
      arr.splice(arr.indexOf(hit), 1);      // tap an existing hit to clear it
    } else {
      this.getSong().tracks.drums.push({ t: beat, d: 0.25, p: 60, v: 0.85, inst: inst });
      this.audition(60, inst);
    }
  };

  Editor.prototype._drumMove = function (p) {
    const rows = this.drumRows();
    const r = this.rowOfY(p.y);
    if (r < 0 || r >= rows.length) return;
    const inst = rows[r];
    const beat = Math.round(this.beatOfX(p.x) / STEP_BEATS) * STEP_BEATS;
    const key = inst + '@' + beat;
    if (this._painted[key]) return;         // dragging paints each cell once
    this._painted[key] = true;
    const hit = this.drumAt(beat, inst);
    const arr = this.getSong().tracks.drums;
    if (this.tool === 'erase') {
      if (hit) arr.splice(arr.indexOf(hit), 1);
    } else if (!hit) {
      arr.push({ t: beat, d: 0.25, p: 60, v: 0.85, inst: inst });
      this.audition(60, inst);
    }
  };

  /* ------------------------------------------------------------------ *
   * Navigation
   * ------------------------------------------------------------------ */

  Editor.prototype.setTrack = function (name) {
    this.track = name;
    this.refit();
  };

  Editor.prototype.scrollTo = function (bar) {
    this.startBar = Math.max(0, Math.min(this.maxStartBar(), Math.round(bar)));
  };

  /** Keep the playhead in view while the song plays. */
  Editor.prototype.followPlayhead = function () {
    if (!this.follow || !this.player.playing) return;
    const bar = Math.floor(this.player.currentBeat() / BEATS_PER_BAR);
    if (bar < this.startBar || bar >= this.startBar + this.bars) {
      this.scrollTo(Math.floor(bar / this.bars) * this.bars);
      return true;
    }
    return false;
  };

  /** Sound a note as it is drawn — but not over the top of playback. */
  Editor.prototype.audition = function (pitch, inst) {
    if (this.player.playing) return;
    if (this.player.audition) this.player.audition(this.track, pitch, inst);
  };

  Editor.prototype.clearTrack = function () {
    this.pushHistory();
    const arr = this.getSong().tracks[this.track];
    arr.length = 0;
    this.onChange();
  };

  global.Editor = Editor;
  global.Editor.DRUM_ROWS = DRUM_ROWS;
})(window);
