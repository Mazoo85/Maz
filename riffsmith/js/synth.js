/*
 * RIFFSMITH — Synth & player
 * --------------------------
 * A few small Web Audio instruments and a player that loops a riff in time.
 *
 * The player schedules notes a little ahead of the clock (the standard Web
 * Audio pattern) so timing stays tight even when the page is busy. It reads
 * the current riff every time it schedules, so a riff can change while it
 * plays and the next notes are simply the new ones — no stop, no restart.
 *
 * Exposed as: window.RiffSynth.
 */
(function (root) {
  'use strict';

  var SOUNDS = [
    ['pluck', 'Pluck'], ['keys', 'Keys'], ['pad', 'Soft pad'], ['bass', 'Bass'],
    ['bell', 'Bell'], ['lead', 'Lead']
  ];

  function freq(midi) { return 440 * Math.pow(2, (midi - 69) / 12); }

  function Player() {
    this.ctx = null;
    this.master = null;
    this.riff = null;
    this.bpm = 120;
    this.sound = 'pluck';
    this.loop = true;
    this.playing = false;
    this.timer = null;
    this.onStop = null;
  }

  Player.prototype.ensure = function () {
    if (this.ctx) return this.ctx;
    var AC = root.AudioContext || root.webkitAudioContext;
    if (!AC) return null;
    var ctx = this.ctx = new AC();
    var comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -14;
    comp.ratio.value = 4;
    this.master = ctx.createGain();
    this.master.gain.value = 0.55;
    this.master.connect(comp);
    comp.connect(ctx.destination);
    return ctx;
  };

  Player.prototype.tickDur = function () { return 60 / this.bpm / this.riff.ppq; };

  // Where the playhead is, in ticks from the start of the riff (looped).
  Player.prototype.position = function () {
    if (!this.playing || !this.ctx || !this.riff) return null;
    var ticks = (this.ctx.currentTime - this.t0) / this.tickDur();
    if (ticks < 0) return 0;
    return this.loop ? ticks % this.riff.length : Math.min(ticks, this.riff.length);
  };

  Player.prototype.setTempo = function (bpm) {
    if (this.playing && this.ctx) {
      // Keep the playhead where it is: only the speed from here on changes.
      var now = this.ctx.currentTime;
      var ticks = (now - this.t0) / this.tickDur();
      var schedAhead = this.schedTick - ticks;
      this.bpm = bpm;
      this.t0 = now - ticks * this.tickDur();
      this.schedTick = ticks + Math.max(0, schedAhead);
    } else {
      this.bpm = bpm;
    }
  };

  Player.prototype.start = function () {
    var ctx = this.ensure();
    if (!ctx || !this.riff) return false;
    if (ctx.state === 'suspended') ctx.resume();
    this.playing = true;
    this.t0 = ctx.currentTime + 0.06;
    this.schedTick = 0;
    var self = this;
    this.timer = setInterval(function () { self.schedule(); }, 25);
    this.schedule();
    return true;
  };

  Player.prototype.stop = function () {
    this.playing = false;
    if (this.timer) clearInterval(this.timer);
    this.timer = null;
    if (this.ctx && this.voiceBus) {
      // Cut what is still ringing.
      var bus = this.voiceBus;
      bus.gain.setTargetAtTime(0, this.ctx.currentTime, 0.02);
      setTimeout(function () { bus.disconnect(); }, 300);
      this.voiceBus = null;
    }
  };

  Player.prototype.bus = function () {
    if (!this.voiceBus) {
      this.voiceBus = this.ctx.createGain();
      this.voiceBus.connect(this.master);
    }
    return this.voiceBus;
  };

  Player.prototype.schedule = function () {
    if (!this.playing) return;
    var riff = this.riff;
    var ctx = this.ctx;
    var td = this.tickDur();
    var endTick = (ctx.currentTime + 0.15 - this.t0) / td;
    var L = riff.length;
    if (!this.loop && this.schedTick >= L) {
      var tail = ctx.currentTime - this.t0 - L * td;
      if (tail > 0.4) {
        this.stop();
        if (this.onStop) this.onStop();
      }
      return;
    }
    var from = this.schedTick;
    var to = this.loop ? endTick : Math.min(endTick, L);
    if (to <= from) return;
    var firstLoop = Math.floor(from / L);
    var lastLoop = Math.floor(to / L);
    for (var k = firstLoop; k <= lastLoop; k++) {
      var base = k * L;
      for (var i = 0; i < riff.notes.length; i++) {
        var n = riff.notes[i];
        var abs = base + n.t;
        if (abs >= from && abs < to) {
          this.voice(n.pitch, this.t0 + abs * td, n.len * td, n.vel, n.pan);
        }
      }
    }
    this.schedTick = to;
  };

  // Hear one note now (used when tapping the piano roll's keyboard).
  Player.prototype.audition = function (pitch) {
    var ctx = this.ensure();
    if (!ctx) return;
    if (ctx.state === 'suspended') ctx.resume();
    var td = 60 / this.bpm;
    this.voice(pitch, ctx.currentTime + 0.01, td * 0.9, 0.8, 0);
  };

  Player.prototype.voice = function (pitch, when, dur, vel, pan) {
    var ctx = this.ctx;
    var out = ctx.createGain();
    var dest = this.playing ? this.bus() : this.master;
    if (ctx.createStereoPanner) {
      var p = ctx.createStereoPanner();
      p.pan.value = pan || 0;
      out.connect(p);
      p.connect(dest);
    } else {
      out.connect(dest);
    }
    var f = freq(pitch);
    var v = 0.12 + 0.28 * vel;
    var g = out.gain;
    var oscs = [];
    var stopAt;
    function osc(type, fr, detune) {
      var o = ctx.createOscillator();
      o.type = type;
      o.frequency.value = fr;
      if (detune) o.detune.value = detune;
      oscs.push(o);
      return o;
    }
    function lowpass(cut, q) {
      var flt = ctx.createBiquadFilter();
      flt.type = 'lowpass';
      flt.frequency.value = cut;
      flt.Q.value = q || 0.7;
      return flt;
    }
    var end = when + Math.max(0.03, dur);
    g.setValueAtTime(0, when);

    switch (this.sound) {
      case 'keys': {
        var a = osc('triangle', f), b = osc('sine', f * 2);
        var bg = ctx.createGain(); bg.gain.value = 0.25;
        a.connect(out); b.connect(bg); bg.connect(out);
        g.linearRampToValueAtTime(v, when + 0.006);
        g.setTargetAtTime(v * 0.45, when + 0.006, 0.25);
        g.setTargetAtTime(0, end, 0.12);
        stopAt = end + 0.8;
        break;
      }
      case 'pad': {
        var lp = lowpass(1400);
        lp.connect(out);
        osc('sawtooth', f, -8).connect(lp);
        osc('sawtooth', f, 8).connect(lp);
        osc('sine', f / 2).connect(lp);
        g.linearRampToValueAtTime(v * 0.6, when + 0.25);
        g.setTargetAtTime(0, Math.max(end, when + 0.25), 0.35);
        stopAt = Math.max(end, when + 0.25) + 2;
        break;
      }
      case 'bass': {
        var bl = lowpass(Math.min(2000, f * 6), 3);
        bl.frequency.setValueAtTime(Math.min(3000, f * 10), when);
        bl.frequency.setTargetAtTime(f * 2, when, 0.08);
        bl.connect(out);
        osc('square', f).connect(bl);
        osc('sine', f / 2).connect(out);
        g.linearRampToValueAtTime(v * 1.1, when + 0.004);
        g.setTargetAtTime(v * 0.7, when + 0.004, 0.2);
        g.setTargetAtTime(0, end, 0.04);
        stopAt = end + 0.4;
        break;
      }
      case 'bell': {
        var car = osc('sine', f);
        var mod = osc('sine', f * 3.5);
        var idx = ctx.createGain();
        idx.gain.setValueAtTime(f * 2.5, when);
        idx.gain.setTargetAtTime(0, when, 0.4);
        mod.connect(idx); idx.connect(car.frequency);
        car.connect(out);
        g.linearRampToValueAtTime(v * 0.9, when + 0.003);
        g.setTargetAtTime(0, when + 0.003, Math.max(0.35, dur * 0.8));
        stopAt = when + Math.max(2.5, dur * 4);
        break;
      }
      case 'lead': {
        var ll = lowpass(2600, 2);
        ll.connect(out);
        osc('square', f).connect(ll);
        osc('sawtooth', f, 6).connect(ll);
        g.linearRampToValueAtTime(v * 0.7, when + 0.01);
        g.setTargetAtTime(0, end, 0.06);
        stopAt = end + 0.5;
        break;
      }
      default: { // pluck
        var pl = lowpass(f * 8, 4);
        pl.frequency.setValueAtTime(Math.min(12000, 1200 + 6000 * vel), when);
        pl.frequency.setTargetAtTime(f * 1.5, when, 0.09);
        pl.connect(out);
        osc('sawtooth', f).connect(pl);
        osc('sawtooth', f, 9).connect(pl);
        g.linearRampToValueAtTime(v, when + 0.003);
        g.setTargetAtTime(0, when + 0.003, 0.22);
        g.setTargetAtTime(0, end, 0.05);
        stopAt = when + 1.6;
      }
    }
    oscs.forEach(function (o) { o.start(when); o.stop(stopAt); });
    oscs[0].onended = function () { out.disconnect(); };
  };

  root.RiffSynth = { Player: Player, SOUNDS: SOUNDS };
})(window);
