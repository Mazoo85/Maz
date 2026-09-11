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
  const TRACKS = ['drums', 'bass', 'chords', 'arp', 'lead', 'counter', 'pad'];
  const LOOKAHEAD = 0.14;      // seconds of audio scheduled ahead of the clock
  const TICK_MS = 25;
  /* Global output trim. Each genre's fx.master is a per-style offset on top of
     this, measured so the styles sit at a comparable loudness. */
  const MASTER_TRIM = 0.42;

  /* Where each part sits across the stereo field. Kick, snare and bass hold the
     centre — everything low or structural does — and the rest opens out. The
     answering voice sits opposite the lead: two voices in the same place read
     as one thicker voice, but apart you hear them answering each other. */
  const TRACK_PAN = { drums: 0, bass: 0, chords: -0.12, arp: -0.3, lead: 0.14,
                      counter: -0.22, pad: 0.08 };

  /* How hard the kick ducks each part. This pumping is most of what makes house,
     synthwave and trap sound like themselves; it is barely there on lo-fi and
     absent from ambient. Rather than a real sidechain (Web Audio compressors have
     no sidechain input), the kick times are already in the score, so the duck is
     scheduled as gain automation exactly where the kick lands. */
  const DUCK_TARGETS = ['bass', 'chords', 'pad', 'arp', 'counter'];

  function defaultPresetName(song, track) {
    const g = song.genre;
    switch (track) {
      case 'bass':   return g.bass.preset;
      case 'chords': return g.chords.preset;
      case 'arp':    return g.arp.preset;
      case 'lead':   return g.lead.preset;
      case 'counter': return g.counter ? g.counter.preset : g.lead.preset;
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

  /* Sends ride on top of the fader: a muted or un-soloed track sends nothing,
     and the multiplier scales the amount each preset already asked for. */
  function sendGain(mix, name, which) {
    const m = (mix && mix[name]) || {};
    const amt = m[which] === undefined ? 1 : m[which];
    return gainFor(mix, name) * amt;
  }

  /* Chorus differs from the other two sends in where it is tapped. Reverb and
     delay are fed by each note directly, at the amount its preset asks for;
     the chorus is fed from the end of the track's own chain, so it hears the
     part crushed and EQ'd exactly as you shaped it — and a muted or un-soloed
     part sends nothing without having to be asked, because the fader it is
     tapped behind is already at zero. */
  function chorusGain(mix, name) {
    const m = (mix && mix[name]) || {};
    return m.cho || 0;
  }

  /**
   * How hard a part is squeezed, from one control.
   *
   * "Threshold, ratio, attack, release, knee, makeup" is six controls and one
   * decision — how much do you want this evened out — so they move together
   * along the line through that space a person actually wants to travel.
   *
   * Makeup is applied as a separate gain rather than left to the node, because
   * Chromium's DynamicsCompressorNode already adds its own: compounding the two
   * is exactly how the master bus once ended up flat.
   */
  function shapeCompressor(bus, amt, punch) {
    if (!bus || !bus.comp) return;
    const c = bus.comp;
    c.threshold.value = -6 - amt * 24 - Math.abs(punch) * 6;
    c.ratio.value = 1 + amt * 9 + Math.abs(punch) * 3;
    c.knee.value = 12 - amt * 6;
    /* Attack is what a transient shaper actually moves: let the front of a note
       through and it stays punchy, clamp it early and the attack is gone. */
    const base = 0.012 - amt * 0.008;
    c.attack.value = Math.max(0.0005, punch > 0 ? base * 3.5 : punch < 0 ? base * 0.15 : base);
    c.release.value = 0.12 + amt * 0.18;
    if (bus.compMakeup) bus.compMakeup.gain.value = 1 + amt * 0.45;
  }

  function mixField(mix, name, field, dflt) {
    const m = (mix && mix[name]) || {};
    return m[field] === undefined ? dflt : m[field];
  }

  /* ------------------------------------------------------------------ *
   * Automation
   *
   * Two lanes over the length of the song, each a list of {t (beats), v (0-1)}.
   * `filter` drives a lowpass across the whole mix — 1 is wide open, 0 is a
   * muffled 120 Hz — and `volume` is a plain fade. Between points the value
   * ramps; outside them it holds.
   *
   * This is what a fixed effect setting can never give you: a filter that opens
   * across eight bars into the chorus, or an ending that actually ends.
   * ------------------------------------------------------------------ */

  const LANES = {
    filter: { min: 120, max: 20000, log: true, neutral: 1 },
    volume: { min: 0, max: 1, log: false, neutral: 1 }
  };

  function laneValueAt(points, beat, neutral) {
    if (!points || !points.length) return neutral;
    if (beat <= points[0].t) return points[0].v;
    for (let i = 1; i < points.length; i++) {
      if (beat <= points[i].t) {
        const a = points[i - 1], b = points[i];
        const span = b.t - a.t;
        if (span <= 0) return b.v;
        return a.v + (b.v - a.v) * ((beat - a.t) / span);
      }
    }
    return points[points.length - 1].v;
  }

  function laneMap(name, v) {
    const L = LANES[name];
    const c = Math.max(0, Math.min(1, v));
    if (!L.log) return L.min + (L.max - L.min) * c;
    return L.min * Math.pow(L.max / L.min, c);
  }

  /**
   * Write one pass of the song's automation onto the graph.
   *
   * `originTime` is the context time of beat 0 of this pass, so the same
   * function serves live playback (called once per loop) and the offline
   * render (called once, at the render's own scheduling offset).
   */
  function applyAutomation(ctx, graph, song, originTime, fromBeat) {
    if (!graph || !graph.autoFilter) return;
    const spb = 60 / song.bpm;
    const auto = (song && song.automation) || {};
    const start = fromBeat || 0;
    const now = ctx.currentTime === undefined ? 0 : ctx.currentTime;

    ['filter', 'volume'].forEach(function (lane) {
      const param = lane === 'filter' ? graph.autoFilter.frequency : graph.autoGain.gain;
      const pts = (auto[lane] || []).slice().sort(function (a, b) { return a.t - b.t; });
      const neutral = LANES[lane].neutral;
      const ramp = lane === 'filter'
        ? function (v, t) { param.exponentialRampToValueAtTime(Math.max(1e-4, v), t); }
        : function (v, t) { param.linearRampToValueAtTime(v, t); };

      const t0 = Math.max(now, originTime + start * spb);
      param.cancelScheduledValues(t0);
      if (!pts.length) { param.setValueAtTime(laneMap(lane, neutral), t0); return; }

      param.setValueAtTime(laneMap(lane, laneValueAt(pts, start, neutral)), t0);
      for (let i = 0; i < pts.length; i++) {
        if (pts[i].t <= start) continue;
        const when = originTime + pts[i].t * spb;
        if (when <= t0) continue;
        ramp(laneMap(lane, pts[i].v), when);
      }
    });
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

    /* The automation filter sits *before* the limiter, so a sweep is still
       caught by the ceiling; the automation fader sits after it, because a
       fade to silence is not something to limit back up. */
    const autoFilter = ctx.createBiquadFilter();
    autoFilter.type = 'lowpass';
    autoFilter.frequency.value = 20000;
    autoFilter.Q.value = 0.9;

    const autoGain = ctx.createGain();
    autoGain.gain.value = 1;

    const out = ctx.createGain();
    out.gain.value = 0.98 * (masterVolume === undefined ? 1 : masterVolume);

    /* The click goes straight to the output. It is not part of the music, so it
       must not be swept by the filter lane, faded by the volume lane, ducked by
       the kick, or caught by the limiter — and it never reaches an export,
       because it is only ever scheduled during live playback. */
    const click = ctx.createGain();
    click.gain.value = 0.5;
    click.connect(out);

    /* Glue: one gentle compressor across the whole mix, which is what makes six
       separate parts sound like one performance rather than six things playing
       at once. Deliberately shallow — 2:1 at a high threshold, catching only
       the peaks — because anything heavier here is the flattening this chain
       was carefully built to avoid. */
    const glueAmt = song.glue === undefined ? 0 : Math.max(0, Math.min(1, song.glue));
    const glue = ctx.createDynamicsCompressor();
    glue.threshold.value = -10 - glueAmt * 8;
    glue.ratio.value = 1 + glueAmt * 1.6;
    glue.knee.value = 10;
    glue.attack.value = 0.02;
    glue.release.value = 0.25;
    const glueTrim = ctx.createGain();
    glueTrim.gain.value = 1 / (1 + glueAmt * 0.25);

    master.connect(glue).connect(glueTrim).connect(autoFilter)
      .connect(limiter).connect(safety).connect(autoGain).connect(out);
    out.connect(ctx.destination);

    let analyser = null;
    if (withAnalyser && ctx.createAnalyser) {
      analyser = ctx.createAnalyser();
      analyser.fftSize = 1024;
      analyser.smoothingTimeConstant = 0.75;
      autoGain.connect(analyser);
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

    /*
     * Delay bus with damped feedback, in two flavours that share one return.
     *
     * Both are built every time and only one is fed, so switching between them
     * is a gain change rather than a rebuild — and the echoes already in the
     * air ring out naturally instead of being cut off.
     */
    const spb = 60 / song.bpm;
    const delTime = Math.min(1.9, fx.delayTime * spb * 2);
    const delReturn = ctx.createGain();
    delReturn.gain.value = Math.min(1, fx.delay);
    delReturn.connect(master);
    const delPre = ctx.createGain();

    // Centred: one line feeding back on itself.
    const delMonoIn = ctx.createGain();
    const delay = ctx.createDelay(2.0);
    delay.delayTime.value = delTime;
    const fb = ctx.createGain();
    fb.gain.value = 0.34;
    const damp = ctx.createBiquadFilter();
    damp.type = 'lowpass';
    damp.frequency.value = 2800;
    delPre.connect(delMonoIn).connect(delay);
    delay.connect(damp).connect(fb).connect(delay);
    delay.connect(delReturn);

    /* Ping-pong: the input hits the left line, left feeds right, right feeds
       left again. Each line is hard-panned, so a single note walks across the
       room and back rather than sitting in the middle. Half the delay time
       each, so a round trip still lands on the same beat as the centred one. */
    const delPingIn = ctx.createGain();
    const dL = ctx.createDelay(2.0);
    const dR = ctx.createDelay(2.0);
    dL.delayTime.value = delTime / 2;
    dR.delayTime.value = delTime / 2;
    const pfb = ctx.createGain();
    pfb.gain.value = 0.38;
    const pdamp = ctx.createBiquadFilter();
    pdamp.type = 'lowpass';
    pdamp.frequency.value = 2800;
    const panL = Synth.panner(ctx, -0.85);
    const panR = Synth.panner(ctx, 0.85);
    delPre.connect(delPingIn).connect(dL);
    dL.connect(dR);
    dR.connect(pdamp).connect(pfb).connect(dL);
    if (panL && panR) {
      dL.connect(panL).connect(delReturn);
      dR.connect(panR).connect(delReturn);
    } else {
      dL.connect(delReturn);
      dR.connect(delReturn);
    }

    const ping = song.pingpong === undefined ? !!fx.pingpong : !!song.pingpong;
    delMonoIn.gain.value = ping ? 0 : 1;
    delPingIn.gain.value = ping ? 1 : 0;

    /*
     * Chorus bus: two short delay lines whose delay times wobble under slow
     * LFOs, panned apart. A copy of a sound arriving a few milliseconds late
     * and drifting in pitch is what "thick" means — it is the same trick as a
     * second player who cannot possibly be perfectly in time or in tune.
     */
    const choPre = ctx.createGain();
    const choReturn = ctx.createGain();
    choReturn.gain.value = 0.9;
    choReturn.connect(master);
    const choLfos = [];
    [[0.19, -0.7, 0.0115, 0.0033], [0.27, 0.7, 0.0163, 0.0027]].forEach(function (spec) {
      const d = ctx.createDelay(0.1);
      d.delayTime.value = spec[2];
      const lfo = ctx.createOscillator();
      lfo.type = 'sine';
      lfo.frequency.value = spec[0];
      const depth = ctx.createGain();
      depth.gain.value = spec[3];
      lfo.connect(depth).connect(d.delayTime);
      const pan = Synth.panner(ctx, spec[1]);
      choPre.connect(d);
      if (pan) d.connect(pan).connect(choReturn); else d.connect(choReturn);
      choLfos.push(lfo);
    });

    // Per-track sends, faders, placement and ducking
    const tracks = {};
    const duckDepth = fx.sidechain === undefined ? 0 : fx.sidechain;
    TRACKS.forEach(function (name) {
      const dry = ctx.createGain();
      const rev = ctx.createGain();
      const del = ctx.createGain();
      const cho = ctx.createGain();

      /* dry -> crush -> EQ -> [duck] -> [pan] -> master
         Crushing first: the EQ is then shaping the grit rather than the grit
         chewing up a carefully set tone. */
      let tail = dry;

      const crush = ctx.createWaveShaper();
      crush.curve = Synth.crushCurve(ctx, mixField(mix, name, 'crush', 0));
      crush.oversample = 'none';
      tail.connect(crush);
      tail = crush;

      const eqLow = ctx.createBiquadFilter();
      eqLow.type = 'lowshelf';
      eqLow.frequency.value = 220;
      eqLow.gain.value = mixField(mix, name, 'eqLow', 0);
      const eqMid = ctx.createBiquadFilter();
      eqMid.type = 'peaking';
      eqMid.frequency.value = 1200;
      eqMid.Q.value = 0.8;
      eqMid.gain.value = mixField(mix, name, 'eqMid', 0);
      const eqHigh = ctx.createBiquadFilter();
      eqHigh.type = 'highshelf';
      eqHigh.frequency.value = 3600;
      eqHigh.gain.value = mixField(mix, name, 'eqHigh', 0);
      tail.connect(eqLow).connect(eqMid).connect(eqHigh);
      tail = eqHigh;

      // A compressor and a transient shaper on every part; see shapeCompressor.
      const comp = ctx.createDynamicsCompressor();
      const compMakeup = ctx.createGain();
      tail.connect(comp).connect(compMakeup);
      tail = compMakeup;
      shapeCompressor({ comp: comp, compMakeup: compMakeup },
        mixField(mix, name, 'comp', 0), mixField(mix, name, 'punch', 0));

      tail.connect(cho);

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
      cho.connect(choPre);
      const t = {
        dry: dry, rev: rev, del: del, cho: cho, duck: duck,
        crush: crush, eqLow: eqLow, eqMid: eqMid, eqHigh: eqHigh,
        comp: comp, compMakeup: compMakeup
      };
      tracks[name] = t;
      dry.gain.value = gainFor(mix, name);
      rev.gain.value = sendGain(mix, name, 'rev');
      del.gain.value = sendGain(mix, name, 'del');
      cho.gain.value = chorusGain(mix, name);
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
      autoFilter: autoFilter, autoGain: autoGain, click: click,
      glue: glue, glueTrim: glueTrim,
      delMonoIn: delMonoIn, delPingIn: delPingIn, choReturn: choReturn, choLfos: choLfos,
      duckDepth: duckDepth, duckRelease: Math.min(0.42, (60 / song.bpm) * 0.62)
    };
  }

  /* ------------------------------------------------------------------ *
   * Flatten the score into one time-ordered list
   * ------------------------------------------------------------------ */

  function flatten(song) {
    const flat = [];
    /* Swing and groove lean are applied here rather than written into the
       score, so this one place feeds live playback and the offline render
       alike — and moving the groove while you listen changes what you hear
       without rewriting a single note. */
    const feel = global.Composer.feelOf(song);
    const swing = global.Composer.swingTime;
    TRACKS.forEach(function (name) {
      const evs = song.tracks[name] || [];
      let prevPitch = null;
      for (let i = 0; i < evs.length; i++) {
        const e = evs[i];
        const item = { t: swing(e.t, feel.swing, feel.push), d: e.d, p: e.p, v: e.v,
                       inst: e.inst, track: name };
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

  /**
   * A click. Two pitches: higher on the first beat of the bar, so you can hear
   * where the bar is rather than only where the beats are.
   */
  function scheduleClick(ctx, graph, when, downbeat) {
    if (!graph || !graph.click) return;
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.value = downbeat ? 1600 : 1050;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, when);
    g.gain.exponentialRampToValueAtTime(downbeat ? 0.5 : 0.28, when + 0.002);
    g.gain.exponentialRampToValueAtTime(0.0001, when + 0.045);
    o.connect(g).connect(graph.click);
    o.start(when);
    o.stop(when + 0.06);
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
    TRACKS.forEach(function (t) {
      this.mix[t] = {
        volume: 1, muted: false, solo: false,
        rev: 1, del: 1, cho: 0,
        eqLow: 0, eqMid: 0, eqHigh: 0, crush: 0, comp: 0, punch: 0
      };
    }, this);
    this.volume = 0.85;
    this.metronome = false;
    this.countIn = false;
    this._clickBeat = 0;
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
    this._chorusStarted = false;
  };

  /* The chorus LFOs run for the life of the graph; like the tape bed they
     belong to playback, not to one auditioned note. */
  Player.prototype._startChorus = function () {
    if (!this.graph || this._chorusStarted) return;
    const t = this.ctx.currentTime;
    for (let i = 0; i < this.graph.choLfos.length; i++) {
      try { this.graph.choLfos[i].start(t); } catch (e) { /* already started */ }
    }
    this._chorusStarted = true;
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
    /* A count-in is simply the song starting a bar later: everything downstream
       already measures from `_originTime`, so nothing else has to know. */
    const lead = (this.metronome && this.countIn) ? (this.song.beatsPerBar || 4) * spb : 0;
    this._originTime = ctx.currentTime + 0.08 + lead - startBeat * spb;
    this._clickBeat = Math.floor(startBeat) - (lead ? (this.song.beatsPerBar || 4) : 0);
    this._startVinyl();
    this._startChorus();
    this._scheduleAutomation(0, startBeat);
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

    if (this.metronome) {
      const bpb = song.beatsPerBar || 4;
      let guardC = 0;
      while (guardC++ < 64) {
        const when = this._originTime + this._pass * song.totalBeats * spb + this._clickBeat * spb;
        if (when > horizon) break;
        if (when >= ctx.currentTime - 0.02) {
          const b = ((this._clickBeat % bpb) + bpb) % bpb;
          scheduleClick(ctx, this.graph, when, Math.abs(b) < 1e-6);
        }
        this._clickBeat++;
        if (this._clickBeat >= song.totalBeats) this._clickBeat = 0;
      }
    }
    const bright = this._brightness();
    let guard = 0;

    while (guard++ < 2000) {
      if (this._index >= this.flat.length) {
        const endTime = this._originTime + (this._pass + 1) * song.totalBeats * spb;
        if (this.loop) {
          this._pass++;
          this._index = 0;
          this._scheduleAutomation(this._pass, 0);
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
    if (opts.rev !== undefined) m.rev = opts.rev;
    if (opts.del !== undefined) m.del = opts.del;
    if (opts.cho !== undefined) m.cho = opts.cho;
    ['eqLow', 'eqMid', 'eqHigh', 'crush', 'comp', 'punch'].forEach(function (k) {
      if (opts[k] !== undefined) m[k] = opts[k];
    });

    this.applyMix();
  };

  /** Switch the echo between centred and bouncing left-right. */
  Player.prototype.setPingPong = function (on) {
    if (this.song) this.song.pingpong = !!on;
    if (!this.graph) return;
    const t = this.ctx.currentTime;
    this.graph.delMonoIn.gain.setTargetAtTime(on ? 0 : 1, t, 0.02);
    this.graph.delPingIn.gain.setTargetAtTime(on ? 1 : 0, t, 0.02);
  };

  /** Re-read the automation lanes after they have been edited. */
  Player.prototype.refreshAutomation = function () {
    if (!this.graph || !this.song) return;
    if (this.playing) this._scheduleAutomation(this._pass, this.currentBeat());
    else applyAutomation(this.ctx, this.graph, this.song, this.ctx.currentTime, 0);
  };

  Player.prototype._scheduleAutomation = function (pass, fromBeat) {
    if (!this.graph || !this.song) return;
    const spb = 60 / this.song.bpm;
    const origin = this._originTime + pass * this.song.totalBeats * spb;
    applyAutomation(this.ctx, this.graph, this.song, origin, fromBeat || 0);
  };

  /** Push the whole mix at the graph — solo changes every track, not just one. */
  Player.prototype.applyMix = function () {
    if (!this.graph) return;
    const self = this;
    const t = this.ctx.currentTime;
    const mix = this.mix;
    const graph = this.graph;
    TRACKS.forEach(function (name) {
      const bus = graph.tracks[name];
      if (!bus) return;
      // Only the fader nodes — `duck` carries its own automation.
      bus.dry.gain.setTargetAtTime(gainFor(mix, name), t, 0.02);
      bus.rev.gain.setTargetAtTime(sendGain(mix, name, 'rev'), t, 0.02);
      bus.del.gain.setTargetAtTime(sendGain(mix, name, 'del'), t, 0.02);
      bus.cho.gain.setTargetAtTime(chorusGain(mix, name), t, 0.02);
      bus.eqLow.gain.setTargetAtTime(mixField(mix, name, 'eqLow', 0), t, 0.02);
      bus.eqMid.gain.setTargetAtTime(mixField(mix, name, 'eqMid', 0), t, 0.02);
      bus.eqHigh.gain.setTargetAtTime(mixField(mix, name, 'eqHigh', 0), t, 0.02);
      // A curve cannot be ramped; swapping it is a single assignment.
      bus.crush.curve = Synth.crushCurve(self.ctx, mixField(mix, name, 'crush', 0));
      shapeCompressor(bus, mixField(mix, name, 'comp', 0), mixField(mix, name, 'punch', 0));
    });
    if (graph.glue) {
      const g = self.song && self.song.glue !== undefined ? self.song.glue : 0;
      graph.glue.threshold.value = -10 - g * 8;
      graph.glue.ratio.value = 1 + g * 1.6;
      graph.glueTrim.gain.value = 1 / (1 + g * 0.25);
    }
  };

  /** Click along with the music, and optionally count a bar in before it. */
  Player.prototype.setMetronome = function (on, countIn) {
    this.metronome = !!on;
    if (countIn !== undefined) this.countIn = !!countIn;
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

    for (let i = 0; i < graph.choLfos.length; i++) graph.choLfos[i].start(0);
    applyAutomation(ctx, graph, song, 0.05, 0);

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
      /* Carry every setting across, not just the fader: a stem should sound
         like the part sounds in the mix, EQ, crush and chorus included. */
      const solo = {};
      TRACKS.forEach(function (t) {
        const m = (mix && mix[t]) || {};
        const copy = {};
        for (const k in m) copy[k] = m[k];
        copy.volume = m.volume === undefined ? 1 : m.volume;
        copy.solo = false;
        copy.muted = t !== name;
        solo[t] = copy;
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
    applyAutomation: applyAutomation,
    laneValueAt: laneValueAt,
    LANES: LANES,
    flatten: flatten,
    presetFor: presetFor,
    defaultPresetName: defaultPresetName,
    TRACKS: TRACKS
  };
})(window);
