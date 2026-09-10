/*
 * SCRIPT FORGE — the score.
 * -------------------------
 * Everything you hear is generated: a chord bed that retunes as the film gets
 * tenser, a pulse that arrives when it does, a hit on every cut between scenes,
 * and a voice per character — pitched blips, the way a game speaks, so the
 * dialogue has a rhythm and a register without a voice actor.
 *
 * It all runs through one gain node, which feeds the speakers *and* a
 * MediaStreamDestination, so the recorded film has exactly the sound you heard.
 *
 *   var score = new FilmScore.Score(reel);
 *   score.start(); score.enterShot(shot); score.tick(timeInFilm); score.stop();
 *
 * Exposed as window.FilmScore. Nothing runs at load, so this file is safe to
 * require in Node — only `new Score()` needs a browser.
 */
(function (root) {
  'use strict';

  /* Each genre gets a root note, a chord shape and a voice. Tension opens the
   * filter, detunes the pad and brings in the pulse. */
  var MUSIC = {
    drama:    { root: 110.0, chord: [1, 1.5, 1.8, 2.4],        wave: 'sine',     cutoff: 900,  pulse: false },
    thriller: { root: 82.4,  chord: [1, 1.5, 2.02, 3.0],       wave: 'sawtooth', cutoff: 620,  pulse: true },
    horror:   { root: 73.4,  chord: [1, 1.414, 2.0, 2.83],     wave: 'sawtooth', cutoff: 480,  pulse: false },
    comedy:   { root: 146.8, chord: [1, 1.25, 1.5, 2.0],       wave: 'triangle', cutoff: 1600, pulse: true },
    romance:  { root: 130.8, chord: [1, 1.26, 1.5, 1.89],      wave: 'sine',     cutoff: 1200, pulse: false },
    scifi:    { root: 98.0,  chord: [1, 1.5, 2.0, 3.0],        wave: 'triangle', cutoff: 1100, pulse: true },
    mystery:  { root: 98.0,  chord: [1, 1.19, 1.5, 2.38],      wave: 'sine',     cutoff: 800,  pulse: false },
    fantasy:  { root: 123.5, chord: [1, 1.335, 1.5, 2.0],      wave: 'triangle', cutoff: 1300, pulse: false },
    heist:    { root: 87.3,  chord: [1, 1.5, 1.78, 2.0],       wave: 'sawtooth', cutoff: 700,  pulse: true },
    western:  { root: 110.0, chord: [1, 1.5, 2.0, 2.99],       wave: 'triangle', cutoff: 1000, pulse: false }
  };

  function audioContextClass() {
    return root.AudioContext || root.webkitAudioContext || null;
  }

  function supported() {
    return !!audioContextClass();
  }

  function Score(reel, opts) {
    opts = opts || {};
    var Ctx = audioContextClass();
    if (!Ctx) throw new Error('This browser has no Web Audio.');

    this.reel = reel;
    this.music = MUSIC[reel.genre] || MUSIC.drama;
    this.ctx = opts.context || new Ctx();

    this.master = this.ctx.createGain();
    this.master.gain.value = opts.volume == null ? 0.85 : opts.volume;

    // Everything is heard *and* recorded from the same node.
    this.speakers = this.ctx.createGain();
    this.speakers.gain.value = 1;
    this.master.connect(this.speakers);
    this.speakers.connect(this.ctx.destination);

    this.streamDestination = this.ctx.createMediaStreamDestination
      ? this.ctx.createMediaStreamDestination()
      : null;
    if (this.streamDestination) this.master.connect(this.streamDestination);

    // ---- the bed: a chord through a filter, with a slow breath on the level
    this.padGain = this.ctx.createGain();
    this.padGain.gain.value = 0;
    this.filter = this.ctx.createBiquadFilter();
    this.filter.type = 'lowpass';
    this.filter.frequency.value = this.music.cutoff;
    this.filter.Q.value = 1.2;
    this.padGain.connect(this.filter);
    this.filter.connect(this.master);

    this.oscs = [];
    this.started = false;
    this.pulseNext = 0;
    this.currentShot = null;
    this.blipTimers = [];
  }

  Score.prototype.start = function () {
    if (this.started) return;
    this.started = true;
    if (this.ctx.state === 'suspended' && this.ctx.resume) this.ctx.resume();

    var self = this;
    var now = this.ctx.currentTime;
    this.music.chord.forEach(function (ratio, i) {
      var osc = self.ctx.createOscillator();
      osc.type = self.music.wave;
      osc.frequency.value = self.music.root * ratio;
      // A pair of cents of detune per voice is what stops it sounding like a
      // test tone and starts it sounding like an instrument.
      osc.detune.value = (i - 1.5) * 6;
      var g = self.ctx.createGain();
      g.gain.value = i === 0 ? 0.34 : 0.20 / i;
      osc.connect(g);
      g.connect(self.padGain);
      osc.start(now);
      self.oscs.push({ osc: osc, gain: g, ratio: ratio });
    });
    this.padGain.gain.setTargetAtTime(0.20, now, 1.2);
  };

  /* Called when the film cuts to a new shot. */
  Score.prototype.enterShot = function (shot, timeInFilm) {
    if (!this.started) return;
    var now = this.ctx.currentTime;
    var tension = shot.mood == null ? 0.4 : shot.mood;

    // Tension opens the filter and lifts the bed.
    this.filter.frequency.setTargetAtTime(
      this.music.cutoff * (0.7 + tension * 1.9), now, 0.6);
    this.padGain.gain.setTargetAtTime(0.15 + tension * 0.16, now, 0.8);

    // A new scene gets a soft hit, so cuts land.
    if (!this.currentShot || this.currentShot.scene !== shot.scene) {
      this.hit(0.5 + tension * 0.5);
      // ...and the chord tilts a step for the new scene.
      var self = this;
      var lift = 1 + (shot.scene % 3) * 0.02 - tension * 0.03;
      this.oscs.forEach(function (v) {
        v.osc.frequency.setTargetAtTime(self.music.root * v.ratio * lift, now, 0.9);
      });
    }

    this.currentShot = shot;

    // Speak the line.
    this.clearBlips();
    if (shot.kind === 'line' && shot.speaker) {
      this.speak(shot.caption, this.reel.voices[shot.speaker], shot.duration);
    }
  };

  /* A low hit plus a breath of noise — the sound of a cut. */
  Score.prototype.hit = function (strength) {
    var now = this.ctx.currentTime;
    var osc = this.ctx.createOscillator();
    var gain = this.ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(120, now);
    osc.frequency.exponentialRampToValueAtTime(38, now + 0.5);
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(0.30 * strength, now + 0.02);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.9);
    osc.connect(gain);
    gain.connect(this.master);
    osc.start(now);
    osc.stop(now + 1.0);

    var noise = this.noiseBurst(0.5, 0.10 * strength, 1400);
    if (noise) noise.start(now);
  };

  Score.prototype.noiseBurst = function (seconds, level, cutoff) {
    var ctx = this.ctx;
    var frames = Math.floor(ctx.sampleRate * seconds);
    if (!frames) return null;
    var buffer = ctx.createBuffer(1, frames, ctx.sampleRate);
    var data = buffer.getChannelData(0);
    for (var i = 0; i < frames; i++) {
      data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / frames, 2.5);
    }
    var src = ctx.createBufferSource();
    src.buffer = buffer;
    var filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = cutoff || 1200;
    var gain = ctx.createGain();
    gain.gain.value = level;
    src.connect(filter);
    filter.connect(gain);
    gain.connect(this.master);
    return src;
  };

  /* ------------------------------------------------------------- voices
   * One blip per syllable-ish, at the character's own pitch, riding a little
   * up and down so a line has shape instead of being a flat beep. */
  Score.prototype.speak = function (text, voice, seconds) {
    if (!voice) return;
    var clean = String(text).replace(/[^A-Za-z0-9' ]/g, ' ');
    var syllables = Math.max(2, Math.round(clean.split(/\s+/).filter(Boolean).length * 1.7));
    var span = Math.max(0.4, (seconds || 2) * 0.78);
    var gap = span / syllables;
    var now = this.ctx.currentTime;

    for (var i = 0; i < syllables; i++) {
      this.blip(now + i * gap, voice, i / syllables, clean.charCodeAt(i % clean.length) || 65);
    }
  };

  Score.prototype.blip = function (when, voice, through, charCode) {
    var ctx = this.ctx;
    var osc = ctx.createOscillator();
    var gain = ctx.createGain();
    var filter = ctx.createBiquadFilter();
    filter.type = 'bandpass';
    filter.frequency.value = voice.pitch * 3.2;
    filter.Q.value = 1.6;

    // A statement falls at the end; a question rises. Cheap, and it works.
    var contour = 1 + Math.sin(through * Math.PI) * 0.10 - through * 0.06;
    var jitter = ((charCode % 7) - 3) * 0.012;
    osc.type = 'square';
    osc.frequency.value = voice.pitch * contour * (1 + jitter);

    gain.gain.setValueAtTime(0.0001, when);
    gain.gain.exponentialRampToValueAtTime(0.16, when + 0.012);
    gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.085);

    osc.connect(filter);
    filter.connect(gain);
    gain.connect(this.master);
    osc.start(when);
    osc.stop(when + 0.12);
    this.blipTimers.push(osc);
  };

  Score.prototype.clearBlips = function () {
    var now = this.ctx.currentTime;
    this.blipTimers.forEach(function (osc) {
      try { osc.stop(now); } catch (e) { /* already stopped */ }
    });
    this.blipTimers = [];
  };

  /* Called every frame: keeps the pulse going under the tense stretches. */
  Score.prototype.tick = function (timeInFilm) {
    if (!this.started || !this.currentShot) return;
    var tension = this.currentShot.mood == null ? 0.4 : this.currentShot.mood;
    if (!this.music.pulse && tension < 0.55) return;

    var period = 0.75 - tension * 0.22;
    if (timeInFilm < this.pulseNext) return;
    this.pulseNext = timeInFilm + period;

    var now = this.ctx.currentTime;
    var osc = this.ctx.createOscillator();
    var gain = this.ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(96, now);
    osc.frequency.exponentialRampToValueAtTime(48, now + 0.14);
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(0.10 + tension * 0.10, now + 0.01);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.28);
    osc.connect(gain);
    gain.connect(this.master);
    osc.start(now);
    osc.stop(now + 0.3);
  };

  Score.prototype.stop = function () {
    if (!this.started) return;
    var now = this.ctx.currentTime;
    this.clearBlips();
    this.padGain.gain.setTargetAtTime(0.0001, now, 0.25);
    var oscs = this.oscs;
    this.oscs = [];
    this.started = false;
    this.currentShot = null;
    this.pulseNext = 0;
    setTimeout(function () {
      oscs.forEach(function (v) {
        try { v.osc.stop(); } catch (e) { /* already stopped */ }
      });
    }, 900);
  };

  Score.prototype.close = function () {
    this.stop();
    var ctx = this.ctx;
    setTimeout(function () {
      if (ctx.close) ctx.close();
    }, 1000);
  };

  /* Silence the speakers without silencing the recording — used when someone
   * records a film and would rather not sit through it out loud. */
  Score.prototype.mute = function (muted) {
    this.speakers.gain.value = muted ? 0 : 1;
  };

  var API = { Score: Score, MUSIC: MUSIC, supported: supported };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmScore = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
