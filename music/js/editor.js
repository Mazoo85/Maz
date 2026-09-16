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
  const BEATS_PER_BAR = 4;          // fallback for a song that has no meter set
  const STEP_BEATS = 0.25;
  const LABEL_W = 52;
  const MIN_ROWS = 15;
  const MAX_ROWS = 34;
  const RULER_H = 16;
  const VEL_H = 58;              // the velocity strip, when it is showing

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
    /*
     * The selection holds references to the very event objects in
     * `song.tracks[...]`, not copies or indices. That is what lets a drag move
     * them by writing straight through — but it also means an undo, which puts
     * a whole new array of objects in place, leaves the selection pointing at
     * notes that are no longer in the song. Every path that replaces the track
     * clears it.
     */
    this.sel = [];
    this._marquee = null;
    this._clip = null;

    /* The velocity strip under the grid, and which drum row it is showing.
       A melodic part has one note per row-and-time, so the strip can simply
       follow the pointer; a drum grid stacks nine instruments at the same
       instant, so it has to be told which one you mean. */
    this.velLane = false;
    this.velRow = 0;
    this._velDrag = false;
    this._loopDrag = null;

    /* The working loop, in beats — null for "the whole song". The editor owns
       the markers because they are drawn here and dragged here; the player is
       told about them through `onLoop`. */
    this.loopFrom = null;
    this.loopTo = null;
    this.onLoop = opts.onLoop || function () {};
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
      duration: song.duration,
      automation: JSON.parse(JSON.stringify(song.automation || {}))
    };
  }

  function restoreSong(song, s) {
    song.tracks = s.tracks;
    song.sections = s.sections;
    song.chords = s.chords;
    song.bars = s.bars;
    song.totalBeats = s.totalBeats;
    song.duration = s.duration;
    if (s.automation) song.automation = JSON.parse(JSON.stringify(s.automation));
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
    /* Undo puts a fresh array of note objects in place, so anything the
       selection was holding is no longer part of the song. */
    this.clearSelection();

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
   * Selection
   *
   * Everything here works on whole groups of notes, which is the difference
   * between an editor you can fix a passage in and one you can only add to.
   * Drums are excluded on purpose: a drum grid has no pitch to transpose and
   * no length to stretch, and its cells are already one click each.
   * ------------------------------------------------------------------ */

  Editor.prototype.clearSelection = function () {
    if (this.sel.length) this.sel = [];
  };

  Editor.prototype.isSelected = function (ev) { return this.sel.indexOf(ev) >= 0; };

  Editor.prototype.selectAll = function () {
    if (this.isDrums()) return 0;
    const song = this.getSong();
    this.sel = (song.tracks[this.track] || []).slice();
    this.draw();
    return this.sel.length;
  };

  /** Everything inside a dragged box, by note rectangle rather than by onset. */
  Editor.prototype.selectInBox = function (box, add) {
    const song = this.getSong();
    if (!song || this.isDrums()) return 0;
    const evs = song.tracks[this.track] || [];
    const b0 = Math.min(box.beat0, box.beat1), b1 = Math.max(box.beat0, box.beat1);
    const p0 = Math.min(box.pitch0, box.pitch1), p1 = Math.max(box.pitch0, box.pitch1);
    const next = add ? this.sel.slice() : [];
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      /* A note counts as caught if any part of it is inside the box, not only
         its start — dragging across the middle of a long note should take it. */
      const overlaps = e.t < b1 && e.t + e.d > b0 && e.p >= p0 && e.p <= p1;
      if (overlaps && next.indexOf(e) < 0) next.push(e);
    }
    this.sel = next;
    return this.sel.length;
  };

  /** Keep only the notes that are still in the track. */
  Editor.prototype._pruneSelection = function () {
    const song = this.getSong();
    if (!song) { this.sel = []; return; }
    const evs = song.tracks[this.track] || [];
    this.sel = this.sel.filter(function (e) { return evs.indexOf(e) >= 0; });
  };

  /**
   * Move the selection in time and pitch.
   *
   * Checked as a group before anything moves: if one note of eight would fall
   * off the end of the song or off the keyboard, the whole move is refused.
   * Clamping each note on its own would silently squash the shape of a phrase
   * against the edge, which is worse than not moving.
   */
  Editor.prototype.nudgeSelection = function (byBeats, bySemis) {
    const song = this.getSong();
    if (!song || !this.sel.length) return false;
    const total = song.totalBeats;
    for (let i = 0; i < this.sel.length; i++) {
      const e = this.sel[i];
      const t = e.t + byBeats, p = e.p + bySemis;
      if (t < -1e-9 || t + e.d > total + 1e-9 || p < 12 || p > 108) return false;
    }
    for (let i = 0; i < this.sel.length; i++) {
      this.sel[i].t = Math.max(0, this.sel[i].t + byBeats);
      this.sel[i].p += bySemis;
    }
    if (bySemis && this.sel.length) this.audition(this.sel[0].p);
    this.draw();
    return true;
  };

  /**
   * Pull the selection onto the grid, by an amount.
   *
   * At 1 every note lands exactly on the nearest division; at 0.5 it moves
   * halfway there. Part-way is the useful setting — a passage played loosely
   * gets tightened without losing the feel that made it worth keeping.
   */
  Editor.prototype.quantizeSelection = function (amount) {
    if (!this.sel.length) return 0;
    const amt = amount === undefined ? 1 : Math.max(0, Math.min(1, amount));
    const grid = this.snap;
    let moved = 0;
    for (let i = 0; i < this.sel.length; i++) {
      const e = this.sel[i];
      const target = Math.round(e.t / grid) * grid;
      const next = e.t + (target - e.t) * amt;
      if (Math.abs(next - e.t) > 1e-9) moved++;
      e.t = Math.max(0, next);
    }
    this.draw();
    return moved;
  };

  Editor.prototype.deleteSelection = function () {
    const song = this.getSong();
    if (!song || !this.sel.length) return 0;
    const gone = this.sel.length;
    const sel = this.sel;
    song.tracks[this.track] = (song.tracks[this.track] || []).filter(function (e) {
      return sel.indexOf(e) < 0;
    });
    this.sel = [];
    this.draw();
    return gone;
  };

  /**
   * Copy the selection, as plain data rather than as references.
   *
   * Stored relative to the earliest note in it, so a paste lands as a shape
   * rather than at the absolute place it was cut from — which is what makes
   * pasting into a different bar, or a different part, do the obvious thing.
   */
  Editor.prototype.copySelection = function () {
    if (!this.sel.length) return 0;
    let first = Infinity;
    for (let i = 0; i < this.sel.length; i++) first = Math.min(first, this.sel[i].t);
    this._clip = this.sel.map(function (e) {
      return { t: e.t - first, d: e.d, p: e.p, v: e.v };
    }).sort(function (a, b) { return a.t - b.t; });
    return this._clip.length;
  };

  Editor.prototype.hasClipboard = function () { return !!(this._clip && this._clip.length); };

  /** Paste the clipboard starting at a beat, and select what landed. */
  Editor.prototype.pasteAt = function (beat) {
    const song = this.getSong();
    if (!song || this.isDrums() || !this.hasClipboard()) return 0;
    const at = this.quantize(Math.max(0, beat));
    const total = song.totalBeats;
    const made = [];
    for (let i = 0; i < this._clip.length; i++) {
      const c = this._clip[i];
      const t = at + c.t;
      if (t + c.d > total + 1e-9) continue;      // past the end: dropped, not squashed
      const ev = { t: t, d: c.d, p: c.p, v: c.v };
      song.tracks[this.track].push(ev);
      made.push(ev);
    }
    song.tracks[this.track].sort(function (a, b) { return a.t - b.t; });
    this.sel = made;
    this.draw();
    return made.length;
  };

  /**
   * Repeat a bar of this part immediately after itself.
   *
   * Works on whatever is in the bar rather than on the selection, because
   * "again" is a thing you want to say about a bar you are looking at, not
   * about notes you have first had to round up.
   */
  Editor.prototype.duplicateBar = function (bar) {
    const song = this.getSong();
    if (!song) return 0;
    const bpb = song.beatsPerBar || BEATS_PER_BAR;
    const from = bar * bpb;
    const to = from + bpb;
    if (to + bpb > song.totalBeats + 1e-9) return 0;
    const evs = song.tracks[this.track] || [];
    const made = [];
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      if (e.t < from - 1e-9 || e.t >= to - 1e-9) continue;
      const copy = {};
      for (const f in e) copy[f] = e[f];
      copy.t = e.t + bpb;
      // Do not let a long note spill past the bar it was copied into.
      copy.d = Math.min(e.d, song.totalBeats - copy.t);
      made.push(copy);
    }
    if (!made.length) return 0;
    song.tracks[this.track] = evs.concat(made).sort(function (a, b) { return a.t - b.t; });
    this.sel = this.isDrums() ? [] : made;
    this.draw();
    return made.length;
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

  /** Beats in a bar of the song being edited — three in a waltz, not four. */
  Editor.prototype.beatsPerBar = function () {
    const song = this.getSong();
    return (song && song.beatsPerBar) || BEATS_PER_BAR;
  };

  Editor.prototype.startBeat = function () { return this.startBar * this.beatsPerBar(); };
  Editor.prototype.spanBeats = function () { return this.bars * this.beatsPerBar(); };

  Editor.prototype.xOfBeat = function (b) {
    return LABEL_W + ((b - this.startBeat()) / this.spanBeats()) * (this.w - LABEL_W);
  };
  Editor.prototype.beatOfX = function (x) {
    return this.startBeat() + ((x - LABEL_W) / (this.w - LABEL_W)) * this.spanBeats();
  };
  /** Height of the velocity strip — zero when it is hidden, which is the
      only thing the rest of the geometry needs to know about it. */
  Editor.prototype.velH = function () { return this.velLane ? VEL_H : 0; };

  /** Where the note grid stops and the velocity strip begins. */
  Editor.prototype.gridBottom = function () { return this.h - this.velH(); };

  Editor.prototype.rowH = function () {
    const n = this.isDrums() ? this.drumRows().length : this.rows;
    return (this.gridBottom() - RULER_H) / Math.max(1, n);
  };
  Editor.prototype.yOfRow = function (r) { return RULER_H + r * this.rowH(); };
  Editor.prototype.rowOfY = function (y) { return Math.floor((y - RULER_H) / this.rowH()); };

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
    const bottom = this.gridBottom();

    cx.fillStyle = 'rgba(0,0,0,0.32)';
    cx.fillRect(LABEL_W, RULER_H, gridW, bottom - RULER_H);

    if (this.isDrums()) this._drawDrumRows(rowH);
    else this._drawPitchRows(rowH, color);

    // Beat and bar lines
    for (let b = 0; b <= beats; b += this.snap) {
      const x = this.xOfBeat(start + b);
      const onBar = Math.abs((b % this.beatsPerBar())) < 1e-6;
      const onBeat = Math.abs((b % 1)) < 1e-6;
      if (!onBeat && this.snap >= 0.5) continue;
      cx.strokeStyle = onBar ? 'rgba(255,255,255,0.30)'
        : onBeat ? 'rgba(255,255,255,0.14)' : 'rgba(255,255,255,0.055)';
      cx.lineWidth = 1;
      cx.beginPath();
      cx.moveTo(Math.round(x) + 0.5, RULER_H);
      cx.lineTo(Math.round(x) + 0.5, H);
      cx.stroke();
    }

    this._drawLoopRange(bottom);

    // Bar numbers and the section this window sits in
    cx.font = '10px ui-monospace, monospace';
    cx.textAlign = 'left';
    for (let b = 0; b < this.bars; b++) {
      const x = this.xOfBeat(start + b * this.beatsPerBar());
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

    if (this.velLane) this._drawVelLane(color);

    // Playhead
    const beat = this.player.currentBeat();
    if (beat >= start && beat <= start + beats) {
      const px = this.xOfBeat(beat);
      cx.strokeStyle = '#ffffff';
      cx.lineWidth = 1.4;
      cx.beginPath();
      cx.moveTo(px, RULER_H); cx.lineTo(px, H);
      cx.stroke();
    }
  };

  /**
   * The working loop: a bar on the ruler, and everything outside it dimmed.
   *
   * Dimming the rest rather than only marking the range is what makes it read
   * at a glance — you see which bars are live without reading two markers and
   * working out what is between them.
   */
  Editor.prototype._drawLoopRange = function (bottom) {
    if (this.loopFrom === null) return;
    const cx = this.cx;
    const start = this.startBeat(), end = start + this.spanBeats();
    const a = Math.max(this.loopFrom, start), b = Math.min(this.loopTo, end);
    cx.fillStyle = 'rgba(0,0,0,0.34)';
    if (this.loopFrom > start) {
      cx.fillRect(LABEL_W, RULER_H, this.xOfBeat(Math.min(this.loopFrom, end)) - LABEL_W,
                  bottom - RULER_H);
    }
    if (this.loopTo < end) {
      const x = this.xOfBeat(Math.max(this.loopTo, start));
      cx.fillRect(x, RULER_H, this.w - x, bottom - RULER_H);
    }
    if (b <= a) return;
    const x0 = this.xOfBeat(a), x1 = this.xOfBeat(b);
    cx.fillStyle = 'rgba(255,200,87,0.7)';
    cx.fillRect(x0, 0, Math.max(2, x1 - x0), 3);
    cx.fillStyle = 'rgba(255,200,87,0.22)';
    if (this.loopFrom >= start) cx.fillRect(x0, 0, 2, RULER_H);
    if (this.loopTo <= end) cx.fillRect(x1 - 2, 0, 2, RULER_H);
  };

  /**
   * The velocity strip: one stem per note, as tall as the note is loud.
   *
   * It shares the grid's time axis exactly, so a stem sits directly under the
   * note it belongs to and you never have to work out which is which.
   */
  Editor.prototype._drawVelLane = function (color) {
    const cx = this.cx;
    const song = this.getSong();
    const top = this.gridBottom();
    const pad = 4;
    const h = VEL_H - pad * 2;
    const W = this.w;

    cx.fillStyle = 'rgba(0,0,0,0.5)';
    cx.fillRect(LABEL_W, top, W - LABEL_W, VEL_H);
    cx.strokeStyle = 'rgba(255,255,255,0.16)';
    cx.beginPath();
    cx.moveTo(LABEL_W, Math.round(top) + 0.5);
    cx.lineTo(W, Math.round(top) + 0.5);
    cx.stroke();

    cx.font = '9px ui-monospace, monospace';
    cx.textAlign = 'right';
    cx.fillStyle = 'rgba(200,190,225,0.8)';
    cx.fillText(this.velLaneLabel(), LABEL_W - 6, top + VEL_H * 0.58);
    cx.textAlign = 'left';

    const evs = this.velLaneEvents();
    const start = this.startBeat(), end = start + this.spanBeats();
    const stepW = (W - LABEL_W) / (this.spanBeats() / STEP_BEATS);
    const barW = Math.max(3, Math.min(10, stepW - 2));
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      if (e.t < start || e.t > end) continue;
      const v = Math.max(0, Math.min(1, e.v));
      const x = this.xOfBeat(e.t);
      const bh = Math.max(2, v * h);
      cx.fillStyle = color;
      cx.globalAlpha = this.sel.length && this.isSelected(e) ? 1 : 0.72;
      cx.fillRect(x + 1, top + pad + (h - bh), barW, bh);
      cx.globalAlpha = 1;
    }
  };

  /** The notes the velocity strip is showing — one drum piece, or the part. */
  Editor.prototype.velLaneEvents = function () {
    const song = this.getSong();
    if (!song) return [];
    if (!this.isDrums()) return song.tracks[this.track] || [];
    const inst = this.velLaneInst();
    return (song.tracks.drums || []).filter(function (e) { return e.inst === inst; });
  };

  Editor.prototype.velLaneInst = function () {
    const rows = this.drumRows();
    return rows[Math.max(0, Math.min(rows.length - 1, this.velRow))];
  };

  Editor.prototype.velLaneLabel = function () {
    if (!this.isDrums()) return 'HOW HARD';
    const inst = this.velLaneInst();
    return (DRUM_LABEL[inst] || inst).toUpperCase();
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
      /* A selected note is ringed in white rather than tinted, so the ring
         reads the same over every part colour and over any velocity. */
      const picked = this.sel.length && this.isSelected(e);
      cx.strokeStyle = picked ? '#ffffff' : 'rgba(0,0,0,0.5)';
      cx.lineWidth = picked ? 2 : 1;
      cx.strokeRect(Math.round(x) + 0.5, Math.round(y + 1) + 0.5, Math.max(3, x2 - x) - 1, h - 1);
      cx.lineWidth = 1;
    }

    // The marquee itself, drawn over the notes it is catching.
    if (this._marquee) {
      const m = this._marquee;
      const mx = this.xOfBeat(Math.min(m.beat0, m.beat1));
      const mx2 = this.xOfBeat(Math.max(m.beat0, m.beat1));
      const r0 = this.rowOfPitch(Math.max(m.pitch0, m.pitch1));
      const r1 = this.rowOfPitch(Math.min(m.pitch0, m.pitch1));
      const my = this.yOfRow(Math.max(0, r0));
      const my2 = this.yOfRow(Math.min(this.rows - 1, r1)) + rowH;
      cx.fillStyle = 'rgba(255,255,255,0.09)';
      cx.fillRect(mx, my, mx2 - mx, my2 - my);
      cx.strokeStyle = 'rgba(255,255,255,0.65)';
      cx.setLineDash([4, 3]);
      cx.strokeRect(Math.round(mx) + 0.5, Math.round(my) + 0.5,
                    Math.round(mx2 - mx), Math.round(my2 - my));
      cx.setLineDash([]);
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

  /**
   * Capture the pointer, or carry on without it.
   *
   * `setPointerCapture` throws when the browser does not think that pointer is
   * active — and an exception here abandons the whole gesture before any of it
   * has run, so a drag that the browser merely could not capture becomes a
   * drag that does nothing at all. Capture is a convenience (it keeps the
   * drag alive when the pointer leaves the canvas), never a requirement.
   */
  Editor.prototype._capture = function (e) {
    try { this.canvas.setPointerCapture(e.pointerId); } catch (err) { /* uncapturable */ }
  };

  /**
   * Set the velocity of whatever is under the pointer in the strip.
   *
   * Only notes that start near the pointer count. Using the whole length of a
   * note instead would mean a held chord swallowed every stem behind it, and
   * you could never reach the notes underneath.
   */
  Editor.prototype.velocityAt = function (x, y) {
    const top = this.gridBottom();
    const pad = 4;
    const h = VEL_H - pad * 2;
    const v = Math.max(0.05, Math.min(1, 1 - (y - top - pad) / h));
    const beat = this.beatOfX(x);
    const stepW = (this.w - LABEL_W) / (this.spanBeats() / STEP_BEATS);
    const reach = this.beatOfX(LABEL_W + Math.max(6, stepW * 0.6)) - this.beatOfX(LABEL_W);
    const evs = this.velLaneEvents();
    let hit = 0;
    for (let i = 0; i < evs.length; i++) {
      if (Math.abs(evs[i].t - beat) <= reach) { evs[i].v = v; hit++; }
    }
    if (hit) this.draw();
    return hit;
  };

  /**
   * Move every note in this part earlier or later.
   *
   * A note pushed off either end of the song is dropped and one whose tail
   * runs past the end is trimmed. Refusing the whole shift instead — the rule
   * a selection uses — would block the commonest reason to want this: nudging
   * a part a sixteenth late so it sits behind the beat, where the final note
   * almost always ends exactly on the last bar line.
   */
  Editor.prototype.shiftTrack = function (byBeats) {
    const song = this.getSong();
    if (!song || !byBeats) return 0;
    const total = song.totalBeats;
    const kept = [];
    const evs = song.tracks[this.track] || [];
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      const t = e.t + byBeats;
      if (t < -1e-9 || t >= total - 1e-9) continue;
      e.t = t;
      if (e.t + e.d > total) e.d = Math.max(this.snap, total - e.t);
      kept.push(e);
    }
    song.tracks[this.track] = kept;
    this._pruneSelection();
    this.draw();
    return kept.length;
  };

  /* ------------------------------------------------------------------ *
   * The working loop
   * ------------------------------------------------------------------ */

  Editor.prototype.setLoop = function (fromBeat, toBeat) {
    const song = this.getSong();
    if (!song || fromBeat === null || toBeat === null || !(toBeat - fromBeat >= 1)) {
      this.loopFrom = this.loopTo = null;
    } else {
      this.loopFrom = Math.max(0, fromBeat);
      this.loopTo = Math.min(song.totalBeats, toBeat);
    }
    this.onLoop(this.loopFrom, this.loopTo);
    this.draw();
    return this.loopFrom !== null;
  };

  /** Loop exactly the bars on screen — the ones you are looking at. */
  Editor.prototype.loopVisible = function () {
    const bpb = this.beatsPerBar();
    return this.setLoop(this.startBar * bpb, (this.startBar + this.bars) * bpb);
  };

  Editor.prototype.clearLoop = function () { return this.setLoop(null, null); };
  Editor.prototype.hasLoop = function () { return this.loopFrom !== null; };

  Editor.prototype._bind = function () {
    const self = this;

    this.canvas.addEventListener('pointerdown', function (e) {
      const song = self.getSong();
      if (!song) return;
      e.preventDefault();
      const p = self._pos(e);
      if (p.x < LABEL_W) return;

      /* The ruler strip along the top sets the working loop. It is the one
         place on the canvas that was doing nothing, and dragging across bar
         numbers to pick bars needs no explaining. */
      if (p.y < RULER_H) {
        self._capture(e);
        const bpb = self.beatsPerBar();
        const bar = Math.floor(self.beatOfX(p.x) / bpb);
        self._loopDrag = { bar0: bar, bar1: bar };
        self.setLoop(bar * bpb, (bar + 1) * bpb);
        return;
      }

      /* Once you start editing, stop chasing the playhead. Otherwise the view
         scrolls out from under you mid-edit and the next click lands in a
         different bar than the one you were looking at. */
      if (self.follow && self.player.playing) {
        self.follow = false;
        if (self.onFollowOff) self.onFollowOff();
      }
      self._capture(e);

      if (self.velLane && p.y >= self.gridBottom()) {
        self.pushHistory();
        self._velDrag = true;
        self.velocityAt(p.x, p.y);
        return;
      }

      if (self.tool === 'select' && !self.isDrums()) {
        self._selectDown(p, e.shiftKey);
        self.draw();
        return;
      }

      self.pushHistory();
      self._painted = {};
      if (self.isDrums()) self._drumDown(p);
      else self._noteDown(p);
      self.draw();
    });

    this.canvas.addEventListener('pointermove', function (e) {
      if (self._loopDrag) {
        const p = self._pos(e);
        const bpb = self.beatsPerBar();
        const song = self.getSong();
        const maxBar = Math.max(0, Math.ceil(song.totalBeats / bpb) - 1);
        self._loopDrag.bar1 = Math.max(0, Math.min(maxBar,
          Math.floor(self.beatOfX(p.x) / bpb)));
        const a = Math.min(self._loopDrag.bar0, self._loopDrag.bar1);
        const b = Math.max(self._loopDrag.bar0, self._loopDrag.bar1) + 1;
        self.setLoop(a * bpb, b * bpb);
        return;
      }
      if (self._velDrag) {
        const p = self._pos(e);
        self.velocityAt(p.x, p.y);
        return;
      }
      if (self._marquee) {
        const p = self._pos(e);
        self._marquee.beat1 = self.beatOfX(p.x);
        self._marquee.pitch1 = self.pitchOfRow(
          Math.max(0, Math.min(self.rows - 1, self.rowOfY(p.y))));
        self.selectInBox(self._marquee, self._marquee.add);
        self.draw();
        return;
      }
      if (!self._drag && !self._painted) return;
      const p = self._pos(e);
      if (self._drag && self._drag.mode === 'group') { self._groupMove(p); self.draw(); return; }
      if (self.isDrums()) self._drumMove(p);
      else self._noteMove(p);
      self.draw();
    });

    function end(e) {
      if (self._loopDrag) {
        self._loopDrag = null;
      } else if (self._velDrag) {
        self._velDrag = null;
        self.onChange();
      } else if (self._marquee) {
        self._marquee = null;
        self.draw();
        if (self.onSelect) self.onSelect(self.sel.length);
      } else if (self._drag || self._painted) {
        self._drag = null;
        self._painted = null;
        self.onChange();
        if (self.onSelect) self.onSelect(self.sel.length);
      }
      self._loopDrag = null;
      self._velDrag = null;
      try { self.canvas.releasePointerCapture(e.pointerId); } catch (err) { /* already gone */ }
    }
    this.canvas.addEventListener('pointerup', end);
    this.canvas.addEventListener('pointercancel', end);
  };

  /**
   * A press in the select tool: either grab the selection, or start a box.
   *
   * Pressing a note that is already selected picks the whole group up — that
   * is the move you want after selecting eight notes. Pressing an unselected
   * note selects just that one and grabs it, so a single note still behaves
   * like a single note. Pressing empty space starts a marquee.
   */
  Editor.prototype._selectDown = function (p, addToSelection) {
    const beat = this.beatOfX(p.x);
    /* Clamped rather than refused: a row out of range would abandon the press
       and the tool would feel broken. Presses above the grid now belong to the
       ruler, so this is a boundary guard rather than an affordance — but a
       marquee still has to survive a rounding error at the very top or bottom
       row, and refusing one is a worse answer than starting it one row in. */
    const row = Math.max(0, Math.min(this.rows - 1, this.rowOfY(p.y)));
    const hit = this.noteAt(beat, this.pitchOfRow(row));

    if (hit) {
      if (addToSelection) {
        const at = this.sel.indexOf(hit);
        if (at >= 0) this.sel.splice(at, 1); else this.sel.push(hit);
        if (this.onSelect) this.onSelect(this.sel.length);
        return;
      }
      if (!this.isSelected(hit)) this.sel = [hit];
      this.pushHistory();
      /* Where the group started, so a drag moves everything by the same
         amount rather than snapping each note onto the pointer. */
      this._drag = {
        mode: 'group',
        grabBeat: beat,
        grabPitch: this.pitchOfRow(row),
        from: this.sel.map(function (e) { return { ev: e, t: e.t, p: e.p }; })
      };
      if (this.onSelect) this.onSelect(this.sel.length);
      return;
    }

    this._marquee = {
      beat0: beat, beat1: beat,
      pitch0: this.pitchOfRow(row), pitch1: this.pitchOfRow(row),
      add: !!addToSelection
    };
    if (!addToSelection) this.clearSelection();
  };

  Editor.prototype._groupMove = function (p) {
    const d = this._drag;
    if (!d || !d.from.length) return;
    const song = this.getSong();
    const row = this.rowOfY(p.y);
    const beat = this.beatOfX(p.x);
    let dBeat = this.quantize(beat - d.grabBeat);
    let dPitch = 0;
    if (row >= 0 && row < this.rows) dPitch = this.pitchOfRow(row) - d.grabPitch;

    /* Clamp the *group*, not each note: the shape has to survive the edges.
       Find how far the move can go before any member would fall off, and move
       everything by that much. */
    const total = song.totalBeats;
    for (let i = 0; i < d.from.length; i++) {
      const f = d.from[i];
      dBeat = Math.max(dBeat, -f.t);
      dBeat = Math.min(dBeat, total - (f.t + f.ev.d));
      dPitch = Math.max(dPitch, 12 - f.p);
      dPitch = Math.min(dPitch, 108 - f.p);
    }
    let sounded = false;
    for (let i = 0; i < d.from.length; i++) {
      const f = d.from[i];
      f.ev.t = f.t + dBeat;
      const next = f.p + dPitch;
      if (next !== f.ev.p) { f.ev.p = next; sounded = sounded || i === 0; }
    }
    if (sounded) this.audition(d.from[0].ev.p);
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
    /* The velocity strip follows the drum you last touched: tap a hat, then
       shape the hats. Anything else would need a second thing to point at. */
    this.velRow = r;
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
    /* A selection belongs to the part it was made in; carrying it across
       would leave it pointing at notes that are not on screen. */
    this.clearSelection();
    this.track = name;
    this.refit();
  };

  Editor.prototype.scrollTo = function (bar) {
    this.startBar = Math.max(0, Math.min(this.maxStartBar(), Math.round(bar)));
  };

  /** Keep the playhead in view while the song plays. */
  Editor.prototype.followPlayhead = function () {
    if (!this.follow || !this.player.playing) return;
    const bar = Math.floor(this.player.currentBeat() / this.beatsPerBar());
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
