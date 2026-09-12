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

  /**
   * How strong a part's colour effect is, from one control.
   *
   * All three colours are built at graph time and blended dry-to-wet here, so
   * the slider works while the song is playing. Only the *kind* needs the
   * graph rebuilt, because that is a different set of nodes.
   *
   * Each kind gets harder as well as louder: at 100% the ring tone climbs, the
   * folder drives further past full scale and the wah gets narrower and wider-
   * swinging. A blend alone would only ever fade between two fixed sounds.
   */
  function shapeColour(bits, amt) {
    if (!bits) return;
    const a = Math.max(0, Math.min(1, amt));
    bits.dry.gain.value = 1 - a;
    bits.wet.gain.value = a;
    if (bits.pre) bits.pre.gain.value = 1 + a * 6;
    if (bits.osc) bits.osc.frequency.value = 140 + a * 340;
    if (bits.filter) bits.filter.Q.value = 4 + a * 6;
    if (bits.depth) bits.depth.gain.value = 900 * a;
  }

  /**
   * Point the delay send at one of the four lines.
   *
   * All four are built every time and only one is fed, so changing flavour is
   * a gain change rather than a rebuild — and the echoes already in the air
   * ring out naturally instead of being cut off mid-repeat. Ping-pong stays a
   * separate switch because it is a property of the plain digital line, not a
   * flavour of its own: a tape echo that bounced would be a tape echo nobody
   * has ever heard.
   */
  function routeDelay(ins, kind, ping, ctx) {
    const want = {
      mono: kind === 'tape' || kind === 'multi' ? 0 : (ping ? 0 : 1),
      ping: kind === 'tape' || kind === 'multi' ? 0 : (ping ? 1 : 0),
      tape: kind === 'tape' ? 1 : 0,
      multi: kind === 'multi' ? 1 : 0
    };
    Object.keys(want).forEach(function (k) {
      if (!ins[k]) return;
      if (ctx) ins[k].gain.setTargetAtTime(want[k], ctx.currentTime, 0.02);
      else ins[k].gain.value = want[k];
    });
  }

  /**
   * How long a ducked part takes to breathe back up after a kick.
   *
   * Tempo-relative, so the pump stays in time at any BPM, and scaled by the
   * song's own speed control: slow is a long swell, fast is a tight click of a
   * pump. The ceiling stops a slow song from ducking into the next kick.
   */
  function duckReleaseFor(song) {
    const speed = song.duckSpeed === undefined ? 0.5 : Math.max(0, Math.min(1, song.duckSpeed));
    const base = (60 / song.bpm) * 0.62;
    return Math.min(0.6, base * (1.6 - speed * 1.25));
  }

  /**
   * The rhythmic gate: chop a part into even pieces on a fixed grid.
   *
   * Not a noise gate. A noise gate exists to remove hiss and microphone bleed
   * between the notes of a recording, and there is neither in a mix that was
   * synthesised from a score — every part is already silent when it is not
   * playing. What people actually reach for a gate to do is this: cut a held
   * pad into a pulse. So that is what this does.
   *
   * Open for the first half of each step and shut for the second, with short
   * ramps on both edges — a hard corner in a gain curve is a click.
   */
  function scheduleChop(graph, song, mix, when, stepDur) {
    for (let i = 0; i < TRACKS.length; i++) {
      const name = TRACKS[i];
      const amt = mixField(mix, name, 'chop', 0);
      const bus = graph.tracks[name];
      if (!bus || !bus.gate || amt <= 0) continue;
      const floor = Math.max(0, 1 - amt);
      const edge = Math.min(0.006, stepDur * 0.12);
      const half = stepDur * 0.5;
      const g = bus.gate.gain;
      g.cancelScheduledValues(when);
      g.setValueAtTime(floor, when);
      g.linearRampToValueAtTime(1, when + edge);
      g.setValueAtTime(1, when + half - edge);
      g.linearRampToValueAtTime(floor, when + half);
    }
  }

  /** Steps per beat for the chop grid: quarters, eighths or sixteenths. */
  function chopStepsPerBeat(song) {
    const r = song.chopRate;
    return r === 1 || r === 4 ? r : 2;
  }

  function anyChop(mix) {
    for (let i = 0; i < TRACKS.length; i++) {
      if (mixField(mix, TRACKS[i], 'chop', 0) > 0) return true;
    }
    return false;
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

    /*
     * Master tone and imaging, at the end of the chain.
     *
     * Mono bass first: low frequencies carry most of the energy, and when the
     * two channels disagree down there a club system cancels them into mush.
     * Splitting off everything below the crossover, summing it to one signal
     * and putting it back in the middle is what keeps the low end solid — and
     * it has to happen before the width control, or widening undoes it.
     */
    const preMaster = ctx.createGain();
    let masterHead = preMaster;

    const monoBass = song.monoBass === undefined ? 0 : Math.max(0, Math.min(1, song.monoBass));
    if (monoBass > 0 && ctx.createChannelSplitter) {
      /*
       * Done in mid/side, not by splitting the band and putting it back.
       *
       * A lowpass and a highpass at the same frequency do not sum back to what
       * went in — they disagree in phase around the crossover, which measured
       * as a 16% loss above it and, absurdly, *more* stereo difference than
       * before. Mid/side has no such seam: the mid is never touched at all, and
       * only the side has its low end removed. That is exactly the intent —
       * take the stereo out of the bass, leave the bass alone.
       */
      const sp = ctx.createChannelSplitter(2);
      const mg = ctx.createChannelMerger(2);
      masterHead.connect(sp);

      const mid = ctx.createGain(); mid.gain.value = 0.5;
      sp.connect(mid, 0); sp.connect(mid, 1);

      const sPos = ctx.createGain(); sPos.gain.value = 0.5;
      const sNeg = ctx.createGain(); sNeg.gain.value = -0.5;
      sp.connect(sPos, 0); sp.connect(sNeg, 1);
      const side = ctx.createGain();
      sPos.connect(side); sNeg.connect(side);

      // The side's low end, removed in proportion to the amount asked for.
      const sideHigh = ctx.createBiquadFilter();
      sideHigh.type = 'highpass';
      sideHigh.frequency.value = 120;
      const cut = ctx.createGain(); cut.gain.value = monoBass;
      const keep = ctx.createGain(); keep.gain.value = 1 - monoBass;
      const sideOut = ctx.createGain();
      side.connect(sideHigh).connect(cut).connect(sideOut);
      side.connect(keep).connect(sideOut);

      // L = mid + side, R = mid - side.
      const inv = ctx.createGain(); inv.gain.value = -1;
      mid.connect(mg, 0, 0); mid.connect(mg, 0, 1);
      sideOut.connect(mg, 0, 0);
      sideOut.connect(inv).connect(mg, 0, 1);
      masterHead = mg;
    }

    /* Stereo width, done as mid/side. The mid is what both channels agree on
       and the side is what they do not; scaling the side is the only way to
       widen a mix without smearing what is meant to be centred. */
    const widthAmt = song.width === undefined ? 1 : Math.max(0, Math.min(2, song.width));
    if (Math.abs(widthAmt - 1) > 0.01 && ctx.createChannelSplitter) {
      const sp = ctx.createChannelSplitter(2);
      const mg = ctx.createChannelMerger(2);
      const mid = ctx.createGain(); mid.gain.value = 0.5;
      const sideA = ctx.createGain(); sideA.gain.value = 0.5 * widthAmt;
      const sideB = ctx.createGain(); sideB.gain.value = -0.5 * widthAmt;
      masterHead.connect(sp);
      // L = mid + side, R = mid - side, rebuilt from the two channels.
      sp.connect(mid, 0); sp.connect(mid, 1);
      sp.connect(sideA, 0); sp.connect(sideB, 1);
      mid.connect(mg, 0, 0); mid.connect(mg, 0, 1);
      sideA.connect(mg, 0, 0); sideB.connect(mg, 0, 0);
      const negA = ctx.createGain(); negA.gain.value = -1;
      const negB = ctx.createGain(); negB.gain.value = -1;
      sideA.connect(negA).connect(mg, 0, 1);
      sideB.connect(negB).connect(mg, 0, 1);
      masterHead = mg;
    }

    /* Master EQ: the final word on tone, after everything else has had its
       say. Three bands, the same shape as the per-part one. */
    const mLow = ctx.createBiquadFilter();
    mLow.type = 'lowshelf';
    mLow.frequency.value = 180;
    mLow.gain.value = song.mEqLow || 0;
    const mMid = ctx.createBiquadFilter();
    mMid.type = 'peaking';
    mMid.frequency.value = 1000;
    mMid.Q.value = 0.7;
    mMid.gain.value = song.mEqMid || 0;
    const mHigh = ctx.createBiquadFilter();
    mHigh.type = 'highshelf';
    mHigh.frequency.value = 4000;
    mHigh.gain.value = song.mEqHigh || 0;
    masterHead.connect(mLow).connect(mMid).connect(mHigh);
    masterHead = mHigh;

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

    master.connect(preMaster);
    masterHead.connect(glue).connect(glueTrim).connect(autoFilter)
      .connect(limiter).connect(safety).connect(autoGain).connect(out);
    out.connect(ctx.destination);

    let analyser = null;
    if (withAnalyser && ctx.createAnalyser) {
      analyser = ctx.createAnalyser();
      analyser.fftSize = 1024;
      analyser.smoothingTimeConstant = 0.75;
      autoGain.connect(analyser);
    }

    /*
     * Reverb bus, in four characters.
     *
     * `size` sets how long the room is, adjustable rather than fixed per genre.
     * The other three are shapes the tail is put through:
     *
     *   room     the plain decaying tail
     *   gated    cut off abruptly part way through — the eighties snare, which
     *            is not a short reverb but a long one with the end chopped off,
     *            and sounds nothing like the former
     *   reverse  the tail played backwards, so it swells into the note instead
     *            of trailing away from it
     *   shimmer  a second copy an octave up, fed by playing the same impulse at
     *            double rate, which is how a pitch-shifted tail is built when
     *            there is no pitch shifter to hand
     */
    const revSize = song.revSize === undefined
      ? (song.genreId === 'ambient' ? 4.2 : 2.6)
      : Math.max(0.3, Math.min(6, song.revSize));
    const revKind = song.revKind || 'room';

    const convolver = ctx.createConvolver();
    convolver.buffer = Synth.reverbImpulse(ctx, revSize, 2.4, revKind);
    const revReturn = ctx.createGain();
    revReturn.gain.value = Math.min(1, fx.reverb * moodRev);
    const revPre = ctx.createGain();
    const revDamp = ctx.createBiquadFilter();
    revDamp.type = 'lowpass';
    revDamp.frequency.value = revKind === 'shimmer' ? 7200 : 5200;
    revPre.connect(revDamp).connect(convolver).connect(revReturn).connect(master);

    if (revKind === 'shimmer') {
      /* A real octave above the tail. Squaring a signal doubles its frequency
         and adds a steady offset, so the tail is squared, the offset is removed
         by a highpass, and the result is convolved again to smear it back into
         a tail of its own. Squaring also makes quiet signals far quieter, hence
         the makeup gain. */
      /* Squared *before* the reverb, not after it. Squaring is a multiplication,
         so it scales with the square of the input: applied to a tail already
         down at 0.005 it produces 0.000025, which is inaudible whatever makeup
         follows — measured as literally no change to the output. Applied to the
         send at note level it gives a real signal, which is then given a tail
         of its own. */
      const shaper = ctx.createWaveShaper();
      shaper.curve = Synth.octaveCurve(ctx);
      shaper.oversample = '2x';
      const upTone = ctx.createBiquadFilter();
      upTone.type = 'highpass';
      upTone.frequency.value = 700;            // takes the offset out
      const upMakeup = ctx.createGain();
      upMakeup.gain.value = 9;
      const up = ctx.createConvolver();
      up.buffer = Synth.reverbImpulse(ctx, Math.max(0.5, revSize * 0.7), 2.2, 'room');
      const upGain = ctx.createGain();
      upGain.gain.value = 0.9;
      revDamp.connect(shaper).connect(upTone).connect(upMakeup)
        .connect(up).connect(upGain).connect(revReturn);
    }

    /*
     * Delay bus with damped feedback, in two flavours that share one return.
     *
     * Both are built every time and only one is fed, so switching between them
     * is a gain change rather than a rebuild — and the echoes already in the
     * air ring out naturally instead of being cut off.
     */
    const spb = 60 / song.bpm;
    /* Delay time as a musical division rather than a fixed genre setting: a
       dotted eighth is the sound of half the records ever made, and it was not
       reachable. */
    const delDiv = song.delDiv === undefined ? fx.delayTime : song.delDiv;
    const delTime = Math.min(1.9, delDiv * spb * 2);
    const delFb = song.delFb === undefined ? 0.34 : Math.max(0, Math.min(0.85, song.delFb));
    const delReturn = ctx.createGain();
    delReturn.gain.value = Math.min(1, fx.delay);
    delReturn.connect(master);
    const delPre = ctx.createGain();

    // Centred: one line feeding back on itself.
    const delMonoIn = ctx.createGain();
    const delay = ctx.createDelay(2.0);
    delay.delayTime.value = delTime;
    const fb = ctx.createGain();
    fb.gain.value = delFb;
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
    // Capped short of runaway: each ping-pong round trip is two delay lines.
    pfb.gain.value = Math.min(0.82, delFb * 1.12);
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

    /*
     * Tape: the same line, but every repeat comes back darker, softer and very
     * slightly out of tune.
     *
     * That last part is the whole character. A tape echo's playback head reads
     * a loop of tape whose speed is never exactly constant, so each repeat is
     * bent a little in pitch — slow drift (wow) and a faster quiver (flutter).
     * Two LFOs on the delay time do the same thing here, because changing how
     * long a delay is *is* changing the speed the sound comes off it.
     *
     * The feedback path is filtered and saturated rather than merely quieter,
     * so repeats lose their top and round over instead of just fading. A
     * lowpass inside a feedback loop is safe at this scale — the flanger's
     * instability came from a loop only milliseconds long, where the loop gain
     * compounds hundreds of times a second.
     */
    const delTapeIn = ctx.createGain();
    const tape = ctx.createDelay(2.0);
    tape.delayTime.value = delTime;
    const tdamp = ctx.createBiquadFilter();
    tdamp.type = 'lowpass';
    tdamp.frequency.value = 1900;
    const tsat = ctx.createWaveShaper();
    tsat.curve = Synth.softClipCurve(ctx);
    tsat.oversample = '2x';
    const tfb = ctx.createGain();
    tfb.gain.value = delFb;
    const delLfos = [];
    // Wow: a slow drift. Flutter: a faster quiver, shallower by an order.
    [[0.7, 0.0022], [5.4, 0.00035]].forEach(function (spec) {
      const lfo = ctx.createOscillator();
      lfo.type = 'sine';
      lfo.frequency.value = spec[0];
      const depth = ctx.createGain();
      depth.gain.value = spec[1];
      lfo.connect(depth).connect(tape.delayTime);
      delLfos.push(lfo);
    });
    delPre.connect(delTapeIn).connect(tape);
    tape.connect(tdamp).connect(tsat).connect(tfb).connect(tape);
    tape.connect(delReturn);

    /* Multi-tap: three fixed taps at rising delays and falling levels, spread
       across the stereo field, with no feedback at all. A feedback delay
       repeats one rhythm getting quieter; this plays a pattern — which is why
       it is a separate flavour rather than a setting on the others. */
    const delMultiIn = ctx.createGain();
    delPre.connect(delMultiIn);
    [[0.5, 0.75, -0.8], [1.0, 0.5, 0.8], [1.5, 0.32, 0]].forEach(function (spec) {
      const tap = ctx.createDelay(3.0);
      tap.delayTime.value = Math.min(2.9, delTime * spec[0]);
      const g = ctx.createGain();
      g.gain.value = spec[1];
      const p = Synth.panner(ctx, spec[2]);
      delMultiIn.connect(tap).connect(g);
      if (p) g.connect(p).connect(delReturn); else g.connect(delReturn);
    });

    const ping = song.pingpong === undefined ? !!fx.pingpong : !!song.pingpong;
    const delIns = { mono: delMonoIn, ping: delPingIn, tape: delTapeIn, multi: delMultiIn };
    routeDelay(delIns, song.delKind || 'digital', ping, null);

    /*
     * Modulation bus: flanger, phaser and rotary.
     *
     * All three are the same idea at different scales, which is why they share
     * a bus and a send. A flanger is a very short delay swept through the
     * comb-filtering range and fed back on itself. A phaser is a stack of
     * allpass filters swept instead — no delay, so no comb, just moving
     * notches. A rotary is a slow pan plus the tremolo a spinning horn makes.
     */
    const modPre = ctx.createGain();
    const modReturn = ctx.createGain();
    modReturn.gain.value = 1;
    modReturn.connect(master);
    const modLfos = [];
    const modKind = song.modFx || 'flanger';

    if (modKind === 'phaser') {
      let chain = modPre;
      const sweep = ctx.createGain();
      sweep.gain.value = 800;
      const plfo = ctx.createOscillator();
      plfo.type = 'sine';
      plfo.frequency.value = 0.35;
      plfo.connect(sweep);
      for (let i = 0; i < 6; i++) {
        const ap = ctx.createBiquadFilter();
        ap.type = 'allpass';
        ap.frequency.value = 300 + i * 420;
        ap.Q.value = 0.6;
        sweep.connect(ap.frequency);
        chain.connect(ap);
        chain = ap;
      }
      chain.connect(modReturn);
      modLfos.push(plfo);
    } else if (modKind === 'rotary') {
      const rpan = Synth.panner(ctx, 0, true);        // it is going to move
      const trem = ctx.createGain();
      trem.gain.value = 0.7;
      const rlfo = ctx.createOscillator();
      rlfo.type = 'sine';
      rlfo.frequency.value = 5.4;                 // the fast rotor
      const rdepth = ctx.createGain();
      rdepth.gain.value = 0.3;
      rlfo.connect(rdepth).connect(trem.gain);
      if (rpan && rpan.pan) {
        const pd = ctx.createGain();
        pd.gain.value = 0.8;
        rlfo.connect(pd).connect(rpan.pan);
      }
      modPre.connect(trem);
      if (rpan) trem.connect(rpan).connect(modReturn); else trem.connect(modReturn);
      modLfos.push(rlfo);
    } else {
      /* Flanger. The feedback path deliberately contains no filter: a
         BiquadFilterNode inside a Web Audio feedback cycle is unstable in this
         engine, measured growing without bound — the same finding that decided
         how the plucked strings are built. A plain gain under 1 is safe. */
      const fd = ctx.createDelay(0.02);
      fd.delayTime.value = 0.003;
      const flfo = ctx.createOscillator();
      flfo.type = 'sine';
      flfo.frequency.value = 0.25;
      const fdepth = ctx.createGain();
      fdepth.gain.value = 0.0022;
      flfo.connect(fdepth).connect(fd.delayTime);
      const ffb = ctx.createGain();
      ffb.gain.value = 0.55;
      modPre.connect(fd);
      fd.connect(ffb).connect(fd);
      fd.connect(modReturn);
      /* Only the delayed half is returned. A flanger needs a dry copy to comb
         against, and the track already sends one straight to the master — so
         adding another here would just double the level of everything sent. */
      modLfos.push(flfo);
    }

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
    /* The style sets how hard the kick pumps, but it is a taste control as much
       as a genre one, so the song's own setting wins where there is one. */
    const duckDepth = song.sidechain === undefined
      ? (fx.sidechain === undefined ? 0 : fx.sidechain)
      : Math.max(0, Math.min(1, song.sidechain));
    TRACKS.forEach(function (name) {
      const dry = ctx.createGain();
      const rev = ctx.createGain();
      const del = ctx.createGain();
      const cho = ctx.createGain();
      const modSend = ctx.createGain();

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

      /*
       * Colour: ring modulation, wave folding and an envelope-following filter.
       *
       * Three deliberately extreme tone effects on one control, because nobody
       * wants three sliders each of which is silent at zero. `colourKind` picks
       * which; the amount blends it against the untouched signal, so the same
       * slider goes from "not doing anything" to "ruined" with everything
       * useful in between.
       */
      const colourAmt = mixField(mix, name, 'colour', 0);
      const colourKind = song.colourFx || 'ring';
      const colourDry = ctx.createGain();
      const colourWetG = ctx.createGain();
      const colourJoin = ctx.createGain();
      const colourBits = { kind: colourKind, dry: colourDry, wet: colourWetG };
      let colourOsc = null;
      tail.connect(colourDry).connect(colourJoin);

      if (colourKind === 'fold') {
        /* Wave folding: drive the signal past full scale and fold it back
           rather than clipping it, which adds harmonics that were never in
           the original instead of merely squaring off the ones that were. */
        const pre = ctx.createGain();
        const folder = ctx.createWaveShaper();
        folder.curve = Synth.foldCurve(ctx);
        folder.oversample = '4x';
        const post = ctx.createGain();
        post.gain.value = 0.6;
        tail.connect(pre).connect(folder).connect(post).connect(colourWetG);
        colourBits.pre = pre;
      } else if (colourKind === 'wah') {
        /* An envelope-following filter, done as a resonant sweep the part
           drives: a real follower needs to read the signal back, which this
           engine cannot do in a graph, so the sweep is driven by an LFO slow
           enough to read as the part opening and closing. */
        const wah = ctx.createBiquadFilter();
        wah.type = 'bandpass';
        wah.frequency.value = 700;
        const wlfo = ctx.createOscillator();
        wlfo.type = 'sine';
        wlfo.frequency.value = 1.6;
        const wdepth = ctx.createGain();
        wlfo.connect(wdepth).connect(wah.frequency);
        colourOsc = wlfo;
        colourBits.filter = wah;
        colourBits.depth = wdepth;
        tail.connect(wah).connect(colourWetG);
      } else {
        /* Ring modulation: multiply the part by a fixed tone. Web Audio has
           no multiplier, but a gain node *is* one — its gain is an audio-rate
           parameter, so driving it with an oscillator multiplies the two. */
        const ring = ctx.createGain();
        ring.gain.value = 0;
        const rosc = ctx.createOscillator();
        rosc.type = 'sine';
        rosc.connect(ring.gain);
        colourOsc = rosc;
        colourBits.osc = rosc;
        tail.connect(ring).connect(colourWetG);
      }
      colourWetG.connect(colourJoin);
      shapeColour(colourBits, colourAmt);
      tail = colourJoin;

      // A compressor and a transient shaper on every part; see shapeCompressor.
      const comp = ctx.createDynamicsCompressor();
      const compMakeup = ctx.createGain();
      tail.connect(comp).connect(compMakeup);
      tail = compMakeup;
      shapeCompressor({ comp: comp, compMakeup: compMakeup },
        mixField(mix, name, 'comp', 0), mixField(mix, name, 'punch', 0));

      tail.connect(cho);
      tail.connect(modSend);

      /* Built for every duckable part whether or not the style pumps, so that
         turning the pump up on a song that started with none is a gain change
         rather than a rebuild. A gain node sitting at 1 costs nothing. */
      let duck = null;
      if (DUCK_TARGETS.indexOf(name) >= 0) {
        duck = ctx.createGain();
        duck.gain.value = 1;
        tail.connect(duck);
        tail = duck;
      }

      /* The rhythmic gate — "chop". Its own node rather than a setting on the
         duck, because the two are scheduled from different clocks: the duck
         fires wherever the kick happens to land, and this fires on a fixed
         grid whether anything is playing or not. */
      const gate = ctx.createGain();
      gate.gain.value = 1;
      tail.connect(gate);
      tail = gate;
      /* A part that is going to be swept needs a panner even when it starts in
         the middle — otherwise there is nothing for the sweep to move. */
      const autoPanAmt = mixField(mix, name, 'autopan', 0);
      const pan = Synth.panner(ctx, TRACK_PAN[name] || 0, autoPanAmt > 0);
      if (pan) { tail.connect(pan); tail = pan; }

      /* Auto-pan sweeps the part across the stereo field, around the place it
         already sits rather than from the middle — so a part panned left
         stays a left-ish part that moves. */
      let panLfo = null;
      if (autoPanAmt > 0 && pan && pan.pan) {
        panLfo = ctx.createOscillator();
        panLfo.type = 'sine';
        panLfo.frequency.value = 0.5;
        const pd = ctx.createGain();
        pd.gain.value = autoPanAmt * (1 - Math.abs(TRACK_PAN[name] || 0));
        panLfo.connect(pd).connect(pan.pan);
      }

      tail.connect(master);

      rev.connect(revPre);
      del.connect(delPre);
      cho.connect(choPre);
      modSend.connect(modPre);
      const t = {
        dry: dry, rev: rev, del: del, cho: cho, duck: duck, gate: gate,
        crush: crush, eqLow: eqLow, eqMid: eqMid, eqHigh: eqHigh,
        comp: comp, compMakeup: compMakeup, modSend: modSend, panLfo: panLfo,
        colourOsc: colourOsc, colour: colourBits
      };
      tracks[name] = t;
      dry.gain.value = gainFor(mix, name);
      rev.gain.value = sendGain(mix, name, 'rev');
      del.gain.value = sendGain(mix, name, 'del');
      cho.gain.value = chorusGain(mix, name);
      modSend.gain.value = mixField(mix, name, 'mod', 0);
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
      mEqLow: mLow, mEqMid: mMid, mEqHigh: mHigh,
      delMonoIn: delMonoIn, delPingIn: delPingIn, delIns: delIns, delLfos: delLfos,
      choReturn: choReturn, choLfos: choLfos,
      modReturn: modReturn, modLfos: modLfos,
      duckDepth: duckDepth, duckRelease: duckReleaseFor(song)
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
        rev: 1, del: 1, cho: 0, mod: 0, autopan: 0, colour: 0, chop: 0,
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
    const lfos = this.graph.choLfos.concat(this.graph.modLfos, this.graph.delLfos || []);
    TRACKS.forEach(function (n) {
      const b = this.graph.tracks[n];
      if (b && b.panLfo) lfos.push(b.panLfo);
      if (b && b.colourOsc) lfos.push(b.colourOsc);
    }, this);
    for (let i = 0; i < lfos.length; i++) {
      try { lfos[i].start(t); } catch (e) { /* already started */ }
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
    /* Counted from the song's origin, not from now, so starting halfway through
       lands the gate on the same grid it would have been on all along. */
    this._chopStep = Math.max(0, Math.floor(startBeat * chopStepsPerBeat(this.song)));
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
    /* The chop grid runs on its own counter rather than off the notes, the way
       the metronome does: a gate that only fired where a note started would be
       following the part instead of cutting it. */
    if (anyChop(this.mix)) {
      const stepDur = spb / chopStepsPerBeat(song);
      /* The counter only advances while something is being chopped, so turning
         the chop on mid-song leaves it pointing at a step long past. Taking
         whichever is later — the counter or the step the clock is actually on —
         means it catches up in one go instead of grinding through the gap. */
      const nowStep = Math.floor((ctx.currentTime - this._originTime) / stepDur);
      if (nowStep > this._chopStep) this._chopStep = nowStep;
      let guardG = 0;
      while (guardG++ < 256) {
        const when = this._originTime + this._chopStep * stepDur;
        if (when > horizon) break;
        if (when >= ctx.currentTime - 0.02) {
          scheduleChop(this.graph, song, this.mix, when, stepDur);
        }
        this._chopStep++;
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
    if (opts.mod !== undefined) m.mod = opts.mod;
    if (opts.autopan !== undefined) m.autopan = opts.autopan;
    if (opts.colour !== undefined) m.colour = opts.colour;
    if (opts.chop !== undefined) m.chop = opts.chop;
    ['eqLow', 'eqMid', 'eqHigh', 'crush', 'comp', 'punch'].forEach(function (k) {
      if (opts[k] !== undefined) m[k] = opts[k];
    });

    this.applyMix();
  };

  /** Switch the echo between centred and bouncing left-right. */
  Player.prototype.setPingPong = function (on) {
    if (this.song) this.song.pingpong = !!on;
    if (!this.graph) return;
    routeDelay(this.graph.delIns, (this.song && this.song.delKind) || 'digital', !!on, this.ctx);
  };

  /**
   * How hard the kick pumps everything else, and how fast it recovers.
   *
   * Both are read fresh on every kick rather than baked into the graph, so
   * they move while the song plays — including up from nothing on a style that
   * does not pump at all, because the gain node is always there waiting.
   */
  Player.prototype.setSidechain = function (depth, speed) {
    if (this.song) {
      if (depth !== undefined) this.song.sidechain = Math.max(0, Math.min(1, depth));
      if (speed !== undefined) this.song.duckSpeed = Math.max(0, Math.min(1, speed));
    }
    if (!this.graph || !this.song) return;
    if (depth !== undefined) this.graph.duckDepth = this.song.sidechain;
    this.graph.duckRelease = duckReleaseFor(this.song);
    /* A part left held down by the last kick before the pump was turned off
       would never come back up on its own. */
    if (this.graph.duckDepth <= 0) {
      const t = this.ctx.currentTime;
      const self = this;
      DUCK_TARGETS.forEach(function (n) {
        const bus = self.graph.tracks[n];
        if (!bus || !bus.duck) return;
        bus.duck.gain.cancelScheduledValues(t);
        bus.duck.gain.setTargetAtTime(1, t, 0.02);
      });
    }
  };

  /** Switch the echo between digital, tape and multi-tap. */
  Player.prototype.setDelayKind = function (kind) {
    if (this.song) this.song.delKind = kind;
    if (!this.graph) return;
    const ping = this.song ? !!this.song.pingpong : false;
    routeDelay(this.graph.delIns, kind, ping, this.ctx);
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
      bus.modSend.gain.setTargetAtTime(mixField(mix, name, 'mod', 0), t, 0.02);
      bus.eqLow.gain.setTargetAtTime(mixField(mix, name, 'eqLow', 0), t, 0.02);
      bus.eqMid.gain.setTargetAtTime(mixField(mix, name, 'eqMid', 0), t, 0.02);
      bus.eqHigh.gain.setTargetAtTime(mixField(mix, name, 'eqHigh', 0), t, 0.02);
      // A curve cannot be ramped; swapping it is a single assignment.
      bus.crush.curve = Synth.crushCurve(self.ctx, mixField(mix, name, 'crush', 0));
      shapeColour(bus.colour, mixField(mix, name, 'colour', 0));
      /* Turning the chop off has to put the gate back at 1 by hand: the grid
         stops writing new envelopes, and whatever value the last one left
         behind would otherwise hold the part down for good. */
      if (bus.gate && mixField(mix, name, 'chop', 0) <= 0) {
        bus.gate.gain.cancelScheduledValues(t);
        bus.gate.gain.setTargetAtTime(1, t, 0.02);
      }
      shapeCompressor(bus, mixField(mix, name, 'comp', 0), mixField(mix, name, 'punch', 0));
    });
    if (graph.mEqLow && self.song) {
      graph.mEqLow.gain.setTargetAtTime(self.song.mEqLow || 0, t, 0.02);
      graph.mEqMid.gain.setTargetAtTime(self.song.mEqMid || 0, t, 0.02);
      graph.mEqHigh.gain.setTargetAtTime(self.song.mEqHigh || 0, t, 0.02);
    }
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

    graph.choLfos.concat(graph.modLfos, graph.delLfos || []).forEach(function (o) { o.start(0); });
    TRACKS.forEach(function (n) {
      const b = graph.tracks[n];
      if (b && b.panLfo) b.panLfo.start(0);
      if (b && b.colourOsc) b.colourOsc.start(0);
    });
    applyAutomation(ctx, graph, song, 0.05, 0);

    if (anyChop(mix)) {
      const stepDur = spb / chopStepsPerBeat(song);
      const steps = Math.ceil((song.totalBeats * spb) / stepDur);
      for (let s = 0; s < steps; s++) scheduleChop(graph, song, mix, s * stepDur + 0.05, stepDur);
    }

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
