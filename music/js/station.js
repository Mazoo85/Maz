/*
 * station.js — the Station: a looping groovebox built into Song Forge.
 *
 * Song Forge writes whole songs. The Station is the other way of working with
 * the same music: one bar or two, looping, with every drum laid out as a row of
 * pads you can hit. It is for finding a groove rather than finishing a track.
 *
 * Two things make it part of this program rather than a second program bolted
 * on.
 *
 * The first is that it makes no sound of its own. A loop here is a *song* —
 * a very short one, with looping switched on — handed to the same player
 * everything else uses, so it comes out through the same drum kits, the same
 * effects rack and the same mixer. There is one audio engine in this app and
 * this is not a second one.
 *
 * The second is that the AI fills it in. A step sequencer normally starts empty
 * and waits for you to program it; this one reads the song you already have and
 * lays its groove out on the pads, and you change what you want to change.
 */
(function (global) {
  'use strict';

  const T = global.Theory;

  /* The nine drums the rack shows, in the order a drummer would sit at them.
     These are Song Forge's own instrument names, so a pattern written here is
     the same kind of data the composer writes. */
  const ROWS = [
    { id: 'kick',    name: 'Kick' },
    { id: 'snare',   name: 'Snare' },
    { id: 'hh',      name: 'Closed Hat' },
    { id: 'oh',      name: 'Open Hat' },
    { id: 'clap',    name: 'Clap' },
    { id: 'tom',     name: 'Tom' },
    { id: 'rim',     name: 'Rimshot' },
    { id: 'cowbell', name: 'Cowbell' },
    { id: 'ride',    name: 'Ride' }
  ];

  const STEP_BEATS = 0.25;          // a step is a sixteenth note
  /* Off, on, accent. An accent is not just louder — a drummer's accent is what
     tells you where the bar is, and one velocity for every hit is most of what
     makes a programmed pattern sound programmed. */
  const VEL = { 1: 0.7, 2: 1 };
  const LADDER = 8;                 // rows in the note grid

  function emptyPattern(steps) {
    const drums = {};
    ROWS.forEach(function (r) { drums[r.id] = new Array(steps).fill(0); });
    return {
      steps: steps,
      drums: drums,
      notes: new Array(steps).fill(null),   // a ladder row index, or null
      vol: {}, mute: {}
    };
  }

  function make(steps) {
    const p = emptyPattern(steps || 16);
    ROWS.forEach(function (r) { p.vol[r.id] = 1; p.mute[r.id] = false; });
    return p;
  }

  /** Resize a pattern, keeping what fits and repeating it to fill the rest. */
  function resize(p, steps) {
    const out = make(steps);
    out.vol = p.vol; out.mute = p.mute;
    ROWS.forEach(function (r) {
      for (let i = 0; i < steps; i++) out.drums[r.id][i] = p.drums[r.id][i % p.steps] || 0;
    });
    for (let i = 0; i < steps; i++) out.notes[i] = p.notes[i % p.steps];
    return out;
  }

  /**
   * The note ladder: eight rungs of the song's own scale from its own root.
   *
   * A fixed pentatonic would be simpler and would sound wrong the moment the
   * song is in anything but a minor key. Reading the song's scale means a note
   * placed on the grid is in the same key as everything else in the app, and
   * changing the song's key moves the ladder with it.
   */
  function ladder(song) {
    const root = T.midi(song.rootPc, 2);
    const out = [];
    for (let i = 0; i < LADDER; i++) out.push(T.degreePitch(song.scaleSteps, root, i));
    return out;
  }

  function nearestRung(pitch, rungs) {
    let best = 0, bestD = 1e9;
    for (let i = 0; i < rungs.length; i++) {
      /* Compared by pitch class, so a bass note two octaves below the ladder
         still lands on the rung it belongs to rather than on the bottom one. */
      const d = Math.min(
        Math.abs(pitch - rungs[i]),
        Math.abs(((pitch - rungs[i]) % 12 + 12) % 12),
        Math.abs(12 - ((pitch - rungs[i]) % 12 + 12) % 12)
      );
      if (d < bestD) { bestD = d; best = i; }
    }
    return best;
  }

  /**
   * Lay a song's own groove out on the pads.
   *
   * Read from the busiest section rather than from the top: bar one of most
   * songs is an intro holding half the kit back, and a groovebox loaded with an
   * intro looks broken. The loudest hits become accents, which is how the
   * pattern keeps the shape the composer gave it instead of flattening to one
   * velocity.
   */
  function fromSong(song, steps) {
    const p = make(steps);
    const rungs = ladder(song);
    const bpb = song.beatsPerBar || 4;
    const bars = Math.max(1, Math.round(steps * STEP_BEATS / bpb));

    let best = null;
    (song.sections || []).forEach(function (s) {
      if (s.bars < bars) return;
      if (!best || s.energy > best.energy) best = s;
    });
    const fromBeat = (best ? best.startBar : 0) * bpb;

    /* A velocity counts as an accent when it is near the top of what this
       pattern actually contains, not against a fixed number — a style that
       plays everything softly still has accents within itself. */
    const inWindow = function (e) {
      return e.t >= fromBeat - 1e-6 && e.t < fromBeat + steps * STEP_BEATS - 1e-6;
    };
    const drumEvents = (song.tracks.drums || []).filter(inWindow);
    let loud = 0;
    drumEvents.forEach(function (e) { if (e.v > loud) loud = e.v; });
    const accentAt = loud * 0.92;

    drumEvents.forEach(function (e) {
      if (!p.drums[e.inst]) return;                   // a drum the rack has no row for
      const step = Math.round((e.t - fromBeat) / STEP_BEATS);
      if (step < 0 || step >= steps) return;
      p.drums[e.inst][step] = e.v >= accentAt ? 2 : 1;
    });

    (song.tracks.bass || []).filter(inWindow).forEach(function (e) {
      const step = Math.round((e.t - fromBeat) / STEP_BEATS);
      if (step < 0 || step >= steps) return;
      p.notes[step] = nearestRung(e.p, rungs);
    });
    return p;
  }

  /**
   * Turn the pads back into a song the player can play.
   *
   * Built on top of the song already loaded, so the loop inherits its key,
   * tempo, kit, instruments and effects — the Station is a different view of
   * this song, not a different song. Everything but the two parts it shows is
   * emptied, because a groovebox that quietly kept playing a pad part you
   * cannot see would be lying about what it is.
   */
  function toSong(song, p) {
    const loop = {};
    for (const k in song) loop[k] = song[k];
    const beats = p.steps * STEP_BEATS;
    const rungs = ladder(song);

    loop.tracks = { drums: [], bass: [], chords: [], arp: [], lead: [], counter: [], pad: [] };
    ROWS.forEach(function (r) {
      if (p.mute[r.id]) return;
      for (let s = 0; s < p.steps; s++) {
        const v = p.drums[r.id][s];
        if (!v) continue;
        loop.tracks.drums.push({
          t: s * STEP_BEATS, d: STEP_BEATS, p: 60,
          /* Mute and level live in the pattern rather than in the mixer: the
             mixer has one fader for the whole kit, and the rack needs nine. */
          v: Math.max(0.02, VEL[v] * (p.vol[r.id] === undefined ? 1 : p.vol[r.id])),
          inst: r.id
        });
      }
    });
    for (let s = 0; s < p.steps; s++) {
      const rung = p.notes[s];
      if (rung == null) continue;
      /* Held until the next note or the end of the loop, so a line reads as a
         bassline rather than as a row of clicks. */
      let len = STEP_BEATS;
      for (let n = s + 1; n < p.steps; n++) {
        if (p.notes[n] != null) break;
        len += STEP_BEATS;
      }
      loop.tracks.bass.push({ t: s * STEP_BEATS, d: len, p: rungs[rung], v: 0.85 });
    }

    loop.totalBeats = beats;
    loop.bars = Math.max(1, Math.round(beats / (song.beatsPerBar || 4)));
    loop.duration = beats * (60 / song.bpm);
    loop.sections = [{ type: 'chorus', name: 'Loop', bars: loop.bars, startBar: 0,
                       energy: 1, parts: {}, keyShift: 0, halfTime: false }];
    loop.automation = { filter: [], volume: [] };
    loop.isStationLoop = true;
    return loop;
  }

  global.Station = {
    ROWS: ROWS,
    STEP_BEATS: STEP_BEATS,
    VEL: VEL,
    LADDER: LADDER,
    make: make,
    resize: resize,
    ladder: ladder,
    fromSong: fromSong,
    toSong: toSong
  };
})(window);
