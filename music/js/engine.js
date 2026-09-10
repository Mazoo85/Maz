/*
 * engine.js — turns a composed song into sound.
 *
 * Builds the mixer graph (per-track gains, reverb and delay buses, master
 * compression), runs a look-ahead scheduler for live playback, and can render
 * the whole song offline for WAV export using exactly the same code path.
 */
(function (global) {
  'use strict';

  const Synth = global.Synth;
  const TRACKS = ['drums', 'bass', 'chords', 'arp', 'lead', 'pad'];
  const LOOKAHEAD = 0.14;      // seconds of audio scheduled ahead of the clock
  const TICK_MS = 25;
  /* Global output trim. Each genre's fx.master is a per-style offset on top of
     this, measured so the styles sit at a comparable loudness. */
  const MASTER_TRIM = 0.42;

  /* Where each part sits across the stereo field. Kick, snare and bass hold the
     centre — everything low or structural does — and the rest opens out. */
  const TRACK_PAN = { drums: 0, bass: 0, chords: -0.12, arp: -0.3, lead: 0.14, pad: 0.08 };

  /* How hard the kick ducks each part. This pumping is most of what makes house,
     synthwave and trap sound like themselves; it is barely there on lo-fi and
     absent from ambient. Rather than a real sidechain (Web Audio compressors have
     no sidechain input), the kick times are already in the score, so the duck is
     scheduled as gain automation exactly where the kick lands. */
  const DUCK_TARGETS = ['bass', 'chords', 'pad', 'arp'];

  function defaultPresetName(song, track) {
    const g = song.genre;
    switch (track) {
      case 'bass':   return g.bass.preset;
      case 'chords': return g.chords.preset;
      case 'arp':    return g.arp.preset;
      case 'lead':   return g.lead.preset;
      case 'pad':    return g.pad.preset;
      default:       return null;
    }
  }

  function presetFor(song, track) {
    const P = global.Genres.PRESETS;
    const chosen = song.presetOverride && song.presetOverride[track];
    if (chosen && P[chosen]) return P[chosen];
    const name = defaultPresetName(song, track);
    return name ? P[name] : null;
  }

  /* ------------------------------------------------------------------ *
   * Mixer graph — identical for live and offline contexts.
   * ------------------------------------------------------------------ */

  /** A soloed track silences the others; muting still wins over being soloed. */
  function gainFor(mix, name) {
    const m = (mix && mix[name]) || { volume: 1, muted: false, solo: false };
    if (m.muted) return 0;
    let anySolo = false;
    for (const k in mix) if (mix[k] && mix[k].solo) { anySolo = true; break; }
    if (anySolo && !m.solo) return 0;
    return m.volume;
  }

  function buildGraph(ctx, song, mix, withAnalyser, masterVolume) {
    const fx = song.genre.fx;
    const moodRev = song.mood.reverb || 1;

    /*
     * Master chain.
     *
     * Levels are set at the source: MASTER_TRIM times the genre's own trim puts
     * the raw mix at roughly -1 dB peak and -16 dB RMS, so nothing downstream
     * has much work to do. That matters, because a DynamicsCompressorNode in
     * Chromium applies automatic makeup gain — give it a low threshold and it
     * quietly hands back everything it took, squashing the track flat. So there
     * is exactly one dynamics stage, set high enough that it only catches stray
     * transients, followed by a soft-clip curve that cannot output past ±1.
     */
    const master = ctx.createGain();
    master.gain.value = fx.master * MASTER_TRIM;

    const limiter = ctx.createDynamicsCompressor();
    limiter.threshold.value = -1;
    limiter.knee.value = 0;
    limiter.ratio.value = 20;
    limiter.attack.value = 0.002;
    limiter.release.value = 0.08;

    const safety = ctx.createWaveShaper();
    safety.curve = Synth.softClipCurve(ctx);
    safety.oversample = '4x';

    const out = ctx.createGain();
    out.gain.value = 0.98 * (masterVolume === undefined ? 1 : masterVolume);

    master.connect(limiter).connect(safety).connect(out);
    out.connect(ctx.destination);

    let analyser = null;
    if (withAnalyser && ctx.createAnalyser) {
      analyser = ctx.createAnalyser();
      analyser.fftSize = 1024;
      analyser.smoothingTimeConstant = 0.75;
      safety.connect(analyser);
    }

    // Reverb bus
    const convolver = ctx.createConvolver();
    convolver.buffer = Synth.reverbImpulse(ctx, song.genreId === 'ambient' ? 4.2 : 2.6, 2.4);
    const revReturn = ctx.createGain();
    revReturn.gain.value = Math.min(1, fx.reverb * moodRev);
    const revPre = ctx.createGain();
    const revDamp = ctx.createBiquadFilter();
    revDamp.type = 'lowpass';
    revDamp.frequency.value = 5200;
    revPre.connect(revDamp).connect(convolver).connect(revReturn).connect(master);

    // Delay bus with damped feedback
    const delay = ctx.createDelay(2.0);
    const spb = 60 / song.bpm;
    delay.delayTime.value = Math.min(1.9, fx.delayTime * spb * 2);
    const fb = ctx.createGain();
    fb.gain.value = 0.34;
    const damp = ctx.createBiquadFilter();
    damp.type = 'lowpass';
    damp.frequency.value = 2800;
    const delReturn = ctx.createGain();
    delReturn.gain.value = Math.min(1, fx.delay);
    const delPre = ctx.createGain();
    delPre.connect(delay);
    delay.connect(damp).connect(fb).connect(delay);
    delay.connect(delReturn).connect(master);

    // Per-track sends, faders, placement and ducking
    const tracks = {};
    const duckDepth = fx.sidechain === undefined ? 0 : fx.sidechain;
    TRACKS.forEach(function (name) {
      const dry = ctx.createGain();
      const rev = ctx.createGain();
      const del = ctx.createGain();

      // dry -> [duck] -> [pan] -> master
      let tail = dry;
      let duck = null;
      if (duckDepth > 0 && DUCK_TARGETS.indexOf(name) >= 0) {
        duck = ctx.createGain();
        duck.gain.value = 1;
        tail.connect(duck);
        tail = duck;
      }
      const pan = Synth.panner(ctx, TRACK_PAN[name] || 0);
      if (pan) { tail.connect(pan); tail = pan; }
      tail.connect(master);

      rev.connect(revPre);
      del.connect(delPre);
      const t = { dry: dry, rev: rev, del: del, duck: duck };
      tracks[name] = t;
      const v = gainFor(mix, name);
      dry.gain.value = v;
      rev.gain.value = v;
      del.gain.value = v;
    });

    // Vinyl / tape bed
    let vinyl = null;
    if (fx.vinyl > 0) {
      vinyl = ctx.createBufferSource();
      vinyl.buffer = Synth.vinylBuffer(ctx);
      vinyl.loop = true;
      const vg = ctx.createGain();
      vg.gain.value = fx.vinyl * 0.09;
      const lp = ctx.createBiquadFilter();
      lp.type = 'lowpass';
      lp.frequency.value = 7000;
      vinyl.connect(lp).connect(vg).connect(master);
    }

    return {
      master: master, limiter: limiter, analyser: analyser, tracks: tracks,
      revReturn: revReturn, delReturn: delReturn, vinyl: vinyl, out: out,
      duckDepth: duckDepth, duckRelease: Math.min(0.42, (60 / song.bpm) * 0.62)
    };
  }

  /* ------------------------------------------------------------------ *
   * Flatten the score into one time-ordered list
   * ------------------------------------------------------------------ */

  function flatten(song) {
    const flat = [];
    TRACKS.forEach(function (name) {
      const evs = song.tracks[name] || [];
      let prevPitch = null;
      for (let i = 0; i < evs.length; i++) {
        const e = evs[i];
        const item = { t: e.t, d: e.d, p: e.p, v: e.v, inst: e.inst, track: name };
        if (name === 'bass' && e.glide && prevPitch !== null) item.glideFrom = prevPitch;
        if (name === 'bass') prevPitch = e.p;
        flat.push(item);
      }
    });
    flat.sort(function (a, b) { return a.t - b.t; });
    return flat;
  }

  /* ------------------------------------------------------------------ *
   * Scheduling one event
   * ------------------------------------------------------------------ */

  function scheduleEvent(ctx, graph, song, ev, when, brightness) {
    const bus = graph.tracks[ev.track];
    if (!bus) return;
    if (ev.track === 'drums') {
      Synth.playDrum(ctx, bus, when, ev.inst, ev.v, song.genre.drums.kit, ev.d * (60 / song.bpm));
      if (ev.inst === 'kick' && graph.duckDepth > 0) duck(graph, when, ev.v);
      return;
    }
    const preset = presetFor(song, ev.track);
    if (!preset) return;
    const spb = 60 / song.bpm;
    const freq = global.Theory.midiToFreq(ev.p);
    const extra = { brightness: brightness };
    if (ev.glideFrom) extra.glideFrom = global.Theory.midiToFreq(ev.glideFrom);
    Synth.playNote(ctx, bus, when, Math.max(0.05, ev.d * spb), freq, preset, ev.v, extra);
  }

  /**
   * Duck the sustained parts under a kick: drop instantly, breathe back up.
   * The recovery is tempo-relative, so the pump stays in time at any BPM.
   */
  function duck(graph, when, vel) {
    const depth = graph.duckDepth * (0.65 + 0.35 * Math.min(1, vel === undefined ? 1 : vel));
    const floor = Math.max(0.02, 1 - depth);
    const release = graph.duckRelease;
    for (let i = 0; i < DUCK_TARGETS.length; i++) {
      const bus = graph.tracks[DUCK_TARGETS[i]];
      if (!bus || !bus.duck) continue;
      const g = bus.duck.gain;
      g.cancelScheduledValues(when);
      g.setValueAtTime(floor, when);
      g.linearRampToValueAtTime(1, when + release);
    }
  }

  /* ------------------------------------------------------------------ *
   * The live player
   * ------------------------------------------------------------------ */

  function Player() {
    this.ctx = null;
    this.song = null;
    this.graph = null;
    this.flat = [];
    this.playing = false;
    this.loop = true;
    this.mix = {};
    TRACKS.forEach(function (t) { this.mix[t] = { volume: 1, muted: false, solo: false }; }, this);
    this.volume = 0.85;
    this._timer = null;
    this._index = 0;
    this._pass = 0;
    this._originTime = 0;
    this._pausedBeat = 0;
    this.onEnd = null;
  }

  Player.prototype.ensureContext = function () {
    if (!this.ctx) {
      const AC = global.AudioContext || global.webkitAudioContext;
      if (!AC) return null;
      this.ctx = new AC();
    }
    if (this.ctx.state === 'suspended') this.ctx.resume();
    return this.ctx;
  };

  Player.prototype.load = function (song) {
    this.stop();
    this.song = song;
    this.flat = flatten(song);
    this._pausedBeat = 0;
    this.graph = null;
  };

  /** Rebuild the event list after a part has been re-rolled. */
  Player.prototype.refresh = function () {
    if (!this.song) return;
    const beat = this.currentBeat();
    this.flat = flatten(this.song);
    if (this.playing) {
      this._index = this._indexForBeat(beat % this.song.totalBeats);
    }
  };

  Player.prototype._brightness = function () {
    return (this.song.genre.fx.brightness || 1) * (this.song.mood.brightness || 1);
  };

  Player.prototype._buildGraph = function () {
    this.graph = buildGraph(this.ctx, this.song, this.mix, true, this.volume);
    this._vinylStarted = false;
  };

  /** The tape bed belongs to playback, not to a single auditioned note. */
  Player.prototype._startVinyl = function () {
    if (!this.graph || !this.graph.vinyl || this._vinylStarted) return;
    try {
      this.graph.vinyl.start(this.ctx.currentTime);
      this._vinylStarted = true;
    } catch (e) { /* already started */ }
  };

  /**
   * Play a single note or hit right now, through its own track, so drawing in
   * the editor makes a sound instead of leaving you guessing until playback.
   */
  Player.prototype.audition = function (track, pitch, inst) {
    if (!this.song) return;
    const ctx = this.ensureContext();
    if (!ctx) return;
    if (!this.graph) this._buildGraph();
    const bus = this.graph.tracks[track];
    if (!bus) return;
    const when = ctx.currentTime + 0.012;
    if (track === 'drums') {
      Synth.playDrum(ctx, bus, when, inst, 0.85, this.song.genre.drums.kit, 0.6);
      return;
    }
    const preset = presetFor(this.song, track);
    if (!preset) return;
    Synth.playNote(ctx, bus, when, 0.4, global.Theory.midiToFreq(pitch), preset, 0.8,
      { brightness: this._brightness() });
  };

  Player.prototype._indexForBeat = function (beat) {
    let lo = 0, hi = this.flat.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (this.flat[mid].t < beat) lo = mid + 1; else hi = mid;
    }
    return lo;
  };

  Player.prototype.play = function (fromBeat) {
    if (!this.song) return;
    const ctx = this.ensureContext();
    if (!ctx) return;
    if (this.playing) this.stop(true);

    if (!this.graph) this._buildGraph();

    const startBeat = fromBeat === undefined ? this._pausedBeat : fromBeat;
    const spb = 60 / this.song.bpm;
    this._pass = 0;
    this._index = this._indexForBeat(startBeat);
    this._originTime = ctx.currentTime + 0.08 - startBeat * spb;
    this._startVinyl();
    this.playing = true;

    const self = this;
    this._tick();
    this._timer = setInterval(function () { self._tick(); }, TICK_MS);
  };

  Player.prototype._tick = function () {
    if (!this.playing) return;
    const ctx = this.ctx;
    const song = this.song;
    const spb = 60 / song.bpm;
    const horizon = ctx.currentTime + LOOKAHEAD;
    const bright = this._brightness();
    let guard = 0;

    while (guard++ < 2000) {
      if (this._index >= this.flat.length) {
        const endTime = this._originTime + (this._pass + 1) * song.totalBeats * spb;
        if (this.loop) {
          this._pass++;
          this._index = 0;
          continue;
        }
        if (ctx.currentTime > endTime + 0.5) {
          this.stop();
          this._pausedBeat = 0;
          if (this.onEnd) this.onEnd();
        }
        break;
      }
      const ev = this.flat[this._index];
      const absBeat = this._pass * song.totalBeats + ev.t;
      const when = this._originTime + absBeat * spb;
      if (when > horizon) break;
      if (when >= ctx.currentTime - 0.05) {
        scheduleEvent(ctx, this.graph, song, ev, Math.max(when, ctx.currentTime), bright);
      }
      this._index++;
    }
  };

  /** Position within the song, in beats. */
  Player.prototype.currentBeat = function () {
    if (!this.song) return 0;
    if (!this.playing) return this._pausedBeat;
    const spb = 60 / this.song.bpm;
    const abs = (this.ctx.currentTime - this._originTime) / spb;
    const b = abs % this.song.totalBeats;
    return b < 0 ? 0 : b;
  };

  Player.prototype.stop = function (keepPosition) {
    if (this._timer) { clearInterval(this._timer); this._timer = null; }
    if (this.playing && keepPosition) this._pausedBeat = this.currentBeat();
    this.playing = false;
    if (this.graph) {
      // Silence anything already scheduled, then drop the graph.
      try {
        const t = this.ctx.currentTime;
        this.graph.master.gain.cancelScheduledValues(t);
        this.graph.master.gain.setValueAtTime(this.graph.master.gain.value, t);
        this.graph.master.gain.linearRampToValueAtTime(0, t + 0.06);
        const old = this.graph;
        setTimeout(function () {
          try { if (old.vinyl) old.vinyl.stop(); } catch (e) { /* noop */ }
          try { old.out.disconnect(); } catch (e) { /* noop */ }
        }, 120);
      } catch (e) { /* noop */ }
      this.graph = null;
    }
  };

  Player.prototype.pause = function () {
    if (!this.playing) return;
    this.stop(true);
  };

  Player.prototype.seek = function (beat) {
    const wasPlaying = this.playing;
    this._pausedBeat = Math.max(0, Math.min(this.song ? this.song.totalBeats - 0.01 : 0, beat));
    if (wasPlaying) {
      this.stop();
      this.play(this._pausedBeat);
    }
  };

  Player.prototype.setTrack = function (name, opts) {
    const m = this.mix[name];
    if (!m) return;
    if (opts.volume !== undefined) m.volume = opts.volume;
    if (opts.muted !== undefined) m.muted = opts.muted;
    if (opts.solo !== undefined) m.solo = opts.solo;
    this.applyMix();
  };

  /** Push the whole mix at the graph — solo changes every track, not just one. */
  Player.prototype.applyMix = function () {
    if (!this.graph) return;
    const t = this.ctx.currentTime;
    const mix = this.mix;
    const graph = this.graph;
    TRACKS.forEach(function (name) {
      const bus = graph.tracks[name];
      if (!bus) return;
      const v = gainFor(mix, name);
      // Only the fader nodes — `duck` carries its own automation.
      ['dry', 'rev', 'del'].forEach(function (k) {
        bus[k].gain.setTargetAtTime(v, t, 0.02);
      });
    });
  };

  Player.prototype.setVolume = function (v) {
    this.volume = Math.max(0, Math.min(1.2, v));
    if (this.graph && this.ctx) {
      this.graph.out.gain.setTargetAtTime(0.98 * this.volume, this.ctx.currentTime, 0.03);
    }
  };

  /**
   * Retime the song under the playhead. The delay line is tuned to the tempo,
   * so the graph has to be rebuilt; playback picks up from the same beat.
   */
  Player.prototype.setTempo = function (bpm) {
    if (!this.song) return;
    const beat = this.currentBeat();
    const wasPlaying = this.playing;
    global.Composer.setTempo(this.song, bpm);
    this.stop();
    this._pausedBeat = Math.min(beat, this.song.totalBeats - 0.01);
    if (wasPlaying) this.play(this._pausedBeat);
  };

  Player.prototype.level = function () {
    if (!this.graph || !this.graph.analyser) return 0;
    const a = this.graph.analyser;
    if (!this._buf || this._buf.length !== a.frequencyBinCount) {
      this._buf = new Uint8Array(a.frequencyBinCount);
    }
    a.getByteTimeDomainData(this._buf);
    let peak = 0;
    for (let i = 0; i < this._buf.length; i++) {
      const v = Math.abs(this._buf[i] - 128) / 128;
      if (v > peak) peak = v;
    }
    return peak;
  };

  Player.prototype.spectrum = function (target) {
    if (!this.graph || !this.graph.analyser) return null;
    const a = this.graph.analyser;
    if (!this._fbuf || this._fbuf.length !== a.frequencyBinCount) {
      this._fbuf = new Uint8Array(a.frequencyBinCount);
    }
    a.getByteFrequencyData(this._fbuf);
    return this._fbuf;
  };

  /* ------------------------------------------------------------------ *
   * Offline render (for WAV export)
   * ------------------------------------------------------------------ */

  function renderOffline(song, mix, onProgress) {
    const OAC = global.OfflineAudioContext || global.webkitOfflineAudioContext;
    if (!OAC) return Promise.reject(new Error('Offline rendering is not supported in this browser.'));

    const rate = 44100;
    const spb = 60 / song.bpm;
    const tail = 3.5;
    const length = Math.ceil((song.totalBeats * spb + tail) * rate);
    const ctx = new OAC(2, length, rate);
    const graph = buildGraph(ctx, song, mix, false);
    if (graph.vinyl) graph.vinyl.start(0);

    const flat = flatten(song);
    const bright = (song.genre.fx.brightness || 1) * (song.mood.brightness || 1);
    for (let i = 0; i < flat.length; i++) {
      const ev = flat[i];
      scheduleEvent(ctx, graph, song, ev, ev.t * spb + 0.05, bright);
      if (onProgress && i % 200 === 0) onProgress(i / flat.length);
    }
    return ctx.startRendering();
  }

  /**
   * Render each part on its own — one pass per track with everything else
   * muted. Web Audio gives an offline context a single output, so separate
   * stems mean separate renders; the cost is time, and the payoff is a folder
   * you can open in any other music program and mix by hand.
   */
  function renderStems(song, mix, onProgress) {
    const parts = TRACKS.filter(function (t) { return (song.tracks[t] || []).length > 0; });
    const out = [];
    let i = 0;

    function next() {
      if (i >= parts.length) return Promise.resolve(out);
      const name = parts[i];
      const solo = {};
      TRACKS.forEach(function (t) {
        const m = (mix && mix[t]) || { volume: 1, muted: false };
        solo[t] = { volume: m.volume, muted: t !== name };
      });
      if (onProgress) onProgress(i / parts.length, name);
      return renderOffline(song, solo).then(function (buf) {
        out.push({ name: name, buffer: buf });
        i++;
        return next();
      });
    }
    return next();
  }

  global.Engine = {
    Player: Player,
    renderOffline: renderOffline,
    renderStems: renderStems,
    buildGraph: buildGraph,
    flatten: flatten,
    presetFor: presetFor,
    defaultPresetName: defaultPresetName,
    TRACKS: TRACKS
  };
})(window);
