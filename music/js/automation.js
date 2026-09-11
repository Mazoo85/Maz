/*
 * automation.js — the lane you draw effect moves on.
 *
 * Everything else in this app is a setting: one value that holds for the whole
 * song. This is the one place where a value *travels*. You draw a line across
 * the length of the track and the mix follows it — a filter opening across the
 * eight bars before the chorus, an ending that actually ends.
 *
 * The whole song is always on screen. That is deliberate: these are shapes
 * measured in sections, not in sixteenths, and a lane you have to scroll is a
 * lane whose shape you cannot see.
 */
(function (global) {
  'use strict';

  const C = global.Composer;
  const BEATS_PER_BAR = 4;          // fallback for a song that has no meter set
  const PAD_L = 34;          // room for the axis labels
  const PAD_R = 10;
  const PAD_T = 10;
  const PAD_B = 18;
  const HIT = 11;            // how near a point you must click to grab it

  const LANE_INFO = {
    filter: {
      label: 'Filter', color: '#00e5ff',
      top: 'open', bottom: 'muffled',
      describe: function (v) { return Math.round(v * 100) + '% open'; }
    },
    volume: {
      label: 'Volume', color: '#6bff8f',
      top: 'full', bottom: 'silent',
      describe: function (v) { return Math.round(v * 100) + '%'; }
    }
  };

  function Automation(opts) {
    this.canvas = opts.canvas;
    this.cx = this.canvas.getContext('2d');
    this.getSong = opts.getSong;
    this.onChange = opts.onChange || function () {};
    this.onBeforeChange = opts.onBeforeChange || function () {};
    this.getBeat = opts.getBeat || function () { return -1; };

    this.lane = 'filter';
    this.w = 0;
    this.h = 0;
    this._drag = null;
    this._bind();
  }

  Automation.prototype.setLane = function (lane) {
    if (!LANE_INFO[lane]) return;
    this.lane = lane;
    this.draw();
  };

  Automation.prototype.points = function () {
    const song = this.getSong();
    if (!song) return [];
    return C.ensureAutomation(song)[this.lane];
  };

  /* ------------------------------------------------------------------ *
   * Geometry
   * ------------------------------------------------------------------ */

  Automation.prototype.resize = function () {
    const dpr = global.devicePixelRatio || 1;
    const rect = this.canvas.getBoundingClientRect();
    this.w = Math.max(1, Math.floor(rect.width));
    this.h = Math.max(1, Math.floor(rect.height));
    this.canvas.width = Math.floor(this.w * dpr);
    this.canvas.height = Math.floor(this.h * dpr);
    this.cx.setTransform(dpr, 0, 0, dpr, 0, 0);
  };

  Automation.prototype.plotW = function () { return Math.max(1, this.w - PAD_L - PAD_R); };
  Automation.prototype.plotH = function () { return Math.max(1, this.h - PAD_T - PAD_B); };

  Automation.prototype.xOfBeat = function (b) {
    const song = this.getSong();
    const total = song ? song.totalBeats : 1;
    return PAD_L + (b / total) * this.plotW();
  };
  Automation.prototype.beatOfX = function (x) {
    const song = this.getSong();
    const total = song ? song.totalBeats : 1;
    return ((x - PAD_L) / this.plotW()) * total;
  };
  Automation.prototype.yOfValue = function (v) { return PAD_T + (1 - v) * this.plotH(); };
  Automation.prototype.valueOfY = function (y) {
    return Math.max(0, Math.min(1, 1 - (y - PAD_T) / this.plotH()));
  };

  Automation.prototype._pos = function (ev) {
    const r = this.canvas.getBoundingClientRect();
    const p = ev.touches && ev.touches[0] ? ev.touches[0] : ev;
    return { x: p.clientX - r.left, y: p.clientY - r.top };
  };

  /** Index of the point nearest a screen position, if it is near enough. */
  Automation.prototype._hit = function (x, y) {
    const pts = this.points();
    let best = -1, bestD = HIT * HIT;
    for (let i = 0; i < pts.length; i++) {
      const dx = this.xOfBeat(pts[i].t) - x;
      const dy = this.yOfValue(pts[i].v) - y;
      const d = dx * dx + dy * dy;
      if (d <= bestD) { bestD = d; best = i; }
    }
    return best;
  };

  /* ------------------------------------------------------------------ *
   * Input
   *
   * Click empty space to add a point, drag one to move it, and click a point
   * you already have with the right mouse button — or tap it twice — to take
   * it away again.
   * ------------------------------------------------------------------ */

  Automation.prototype._bind = function () {
    const self = this;
    const c = this.canvas;

    function down(ev) {
      const song = self.getSong();
      if (!song) return;
      ev.preventDefault();
      const p = self._pos(ev);
      const hit = self._hit(p.x, p.y);

      // Right-click or a second tap on the same point removes it.
      const wantsDelete = (ev.button === 2) ||
        (hit >= 0 && self._lastHit === hit && (Date.now() - self._lastHitAt) < 420);
      if (hit >= 0 && wantsDelete) {
        self.onBeforeChange();
        C.removePoint(song, self.lane, hit);
        self._lastHit = -1;
        self.draw();
        self.onChange();
        return;
      }

      self.onBeforeChange();
      let index = hit;
      if (index < 0) {
        const pt = C.addPoint(song, self.lane, self.beatOfX(p.x), self.valueOfY(p.y));
        index = self.points().indexOf(pt);
      }
      self._lastHit = index;
      self._lastHitAt = Date.now();
      self._drag = { index: index };
      self.draw();
      self.onChange();
    }

    function move(ev) {
      if (!self._drag) return;
      const song = self.getSong();
      if (!song) return;
      ev.preventDefault();
      const p = self._pos(ev);
      const pts = self.points();
      const pt = pts[self._drag.index];
      if (!pt) return;
      pt.t = Math.max(0, Math.min(song.totalBeats, self.beatOfX(p.x)));
      pt.v = self.valueOfY(p.y);
      // Keep the list sorted under the finger, and keep hold of the same point.
      C.tidyLane(song, self.lane);
      self._drag.index = self.points().indexOf(pt);
      self.draw();
      self.onChange();
    }

    function up() {
      if (!self._drag) return;
      self._drag = null;
      const song = self.getSong();
      if (song) C.tidyLane(song, self.lane);
      self.draw();
      self.onChange();
    }

    c.addEventListener('mousedown', down);
    c.addEventListener('touchstart', down, { passive: false });
    global.addEventListener('mousemove', move);
    c.addEventListener('touchmove', move, { passive: false });
    global.addEventListener('mouseup', up);
    c.addEventListener('touchend', up);
    c.addEventListener('contextmenu', function (ev) { ev.preventDefault(); });
  };

  /* ------------------------------------------------------------------ *
   * Drawing
   * ------------------------------------------------------------------ */

  Automation.prototype.draw = function () {
    const song = this.getSong();
    const cx = this.cx;
    if (!cx) return;
    if (!this.w || !this.h) this.resize();
    cx.clearRect(0, 0, this.w, this.h);
    if (!song) return;

    const info = LANE_INFO[this.lane];
    const pw = this.plotW(), ph = this.plotH();

    cx.fillStyle = 'rgba(0,0,0,0.3)';
    cx.fillRect(PAD_L, PAD_T, pw, ph);

    // Section bands, so you can see where the chorus is while you draw.
    cx.font = '9px ui-monospace, monospace';
    song.sections.forEach(function (sec, i) {
      const bpb = song.beatsPerBar || BEATS_PER_BAR;
      const x0 = this.xOfBeat(sec.startBar * bpb);
      const x1 = this.xOfBeat((sec.startBar + sec.bars) * bpb);
      if (sec.type === 'chorus') {
        cx.fillStyle = 'rgba(255,45,149,0.11)';
        cx.fillRect(x0, PAD_T, x1 - x0, ph);
      } else if (i % 2) {
        cx.fillStyle = 'rgba(255,255,255,0.028)';
        cx.fillRect(x0, PAD_T, x1 - x0, ph);
      }
      cx.strokeStyle = 'rgba(255,255,255,0.10)';
      cx.beginPath();
      cx.moveTo(Math.round(x0) + 0.5, PAD_T);
      cx.lineTo(Math.round(x0) + 0.5, PAD_T + ph);
      cx.stroke();
      if (x1 - x0 > 26) {
        cx.fillStyle = 'rgba(162,151,196,0.85)';
        cx.fillText((sec.name || sec.type).slice(0, 8), x0 + 3, PAD_T + ph + 12);
      }
    }, this);

    // Halfway guide
    cx.strokeStyle = 'rgba(255,255,255,0.10)';
    cx.setLineDash([3, 4]);
    cx.beginPath();
    cx.moveTo(PAD_L, this.yOfValue(0.5));
    cx.lineTo(PAD_L + pw, this.yOfValue(0.5));
    cx.stroke();
    cx.setLineDash([]);

    cx.fillStyle = 'rgba(162,151,196,0.9)';
    cx.fillText(info.top, 2, PAD_T + 8);
    cx.fillText(info.bottom, 2, PAD_T + ph - 1);

    const pts = this.points();
    const neutral = 1;

    // The line itself: flat at neutral when nothing is drawn.
    cx.strokeStyle = info.color;
    cx.lineWidth = 2;
    cx.beginPath();
    if (!pts.length) {
      const y = this.yOfValue(neutral);
      cx.moveTo(PAD_L, y);
      cx.lineTo(PAD_L + pw, y);
    } else {
      cx.moveTo(PAD_L, this.yOfValue(pts[0].v));
      for (let i = 0; i < pts.length; i++) cx.lineTo(this.xOfBeat(pts[i].t), this.yOfValue(pts[i].v));
      cx.lineTo(PAD_L + pw, this.yOfValue(pts[pts.length - 1].v));
    }
    cx.stroke();

    // Shade under the line — it reads as a level far faster than a bare line.
    if (pts.length) {
      cx.lineTo(PAD_L + pw, PAD_T + ph);
      cx.lineTo(PAD_L, PAD_T + ph);
      cx.closePath();
      cx.fillStyle = 'rgba(255,255,255,0.06)';
      cx.fill();
    }

    for (let i = 0; i < pts.length; i++) {
      const x = this.xOfBeat(pts[i].t), y = this.yOfValue(pts[i].v);
      cx.fillStyle = info.color;
      cx.beginPath();
      cx.arc(x, y, 5, 0, Math.PI * 2);
      cx.fill();
      cx.fillStyle = '#0b0714';
      cx.beginPath();
      cx.arc(x, y, 2, 0, Math.PI * 2);
      cx.fill();
    }

    // Playhead
    const beat = this.getBeat();
    if (beat >= 0) {
      const x = this.xOfBeat(beat);
      cx.strokeStyle = '#ffc857';
      cx.lineWidth = 1.5;
      cx.beginPath();
      cx.moveTo(x, PAD_T);
      cx.lineTo(x, PAD_T + ph);
      cx.stroke();
    }

    cx.strokeStyle = 'rgba(255,255,255,0.12)';
    cx.lineWidth = 1;
    cx.strokeRect(PAD_L + 0.5, PAD_T + 0.5, pw - 1, ph - 1);
  };

  Automation.prototype.describe = function () {
    const pts = this.points();
    const info = LANE_INFO[this.lane];
    if (!pts.length) return info.label + ': flat — nothing moving yet.';
    return info.label + ': ' + pts.length + ' point' + (pts.length === 1 ? '' : 's') +
      ', from ' + info.describe(pts[0].v) + ' to ' + info.describe(pts[pts.length - 1].v) + '.';
  };

  global.Automation = Automation;
  global.Automation.LANE_INFO = LANE_INFO;
})(window);
