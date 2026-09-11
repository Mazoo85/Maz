/*
 * composer.js — writes the song.
 *
 * Given a seed, a genre and a mood it decides the key, tempo, chord
 * progression and arrangement, then composes each part (drums, bass, chords,
 * arp, lead, pad) as a list of note events measured in beats.
 *
 * Nothing here makes a sound; it only produces the score. engine.js plays it,
 * midi.js exports it.
 *
 * Event shape: { t: startBeat, d: durationBeats, p: midiPitch, v: velocity, inst? }
 */
(function (global) {
  'use strict';

  const T = global.Theory;
  const G = global.Genres;
  const BEATS_PER_BAR = 4;           // the default, and what 4/4 songs use
  const STEPS_PER_BAR = 16;          // sixteenth-note grid
  const STEP_BEATS = 0.25;           // one sixteenth, in beats — true in any meter

  /* ------------------------------------------------------------------ *
   * Time signatures
   *
   * A beat is a quarter note and a step is a sixteenth, in every meter. What
   * changes is how many of them make a bar, and — the part that actually
   * matters musically — where the weight falls inside it.
   *
   *   backbeats  where a snare or clap belongs
   *   accents    where a bar restarts its pulse: 7/8 is 2+2+3, not seven evens
   *   pulse      the natural subdivision for hats, in steps
   * ------------------------------------------------------------------ */

  const METERS = {
    '4/4': { beats: 4,   steps: 16, backbeats: [4, 12],  accents: [0, 8],     pulse: 2,
             name: 'four to the bar' },
    '3/4': { beats: 3,   steps: 12, backbeats: [4, 8],   accents: [0],        pulse: 2,
             name: 'waltz time' },
    '6/8': { beats: 3,   steps: 12, backbeats: [6],      accents: [0, 6],     pulse: 2,
             name: 'six-eight' },
    '5/4': { beats: 5,   steps: 20, backbeats: [4, 12],  accents: [0, 12],    pulse: 2,
             name: 'five to the bar' },
    '7/8': { beats: 3.5, steps: 14, backbeats: [6],      accents: [0, 4, 8],  pulse: 2,
             name: 'seven-eight' }
  };

  function meterOf(song) { return METERS[song && song.meter] || METERS['4/4']; }
  /** Beats in one bar of this song. A quarter note everywhere; the count varies. */
  function bpb(song) { return meterOf(song).beats; }
  function spb_(song) { return meterOf(song).steps; }

  /* Instruments fall into three jobs, and each one moves differently when the
     bar changes length. */
  const PULSE_INSTS = ['hh', 'oh', 'shaker', 'ride', 'tamb', 'perc', 'conga', 'cowbell'];
  const BACKBEAT_INSTS = ['snare', 'clap', 'rim'];

  /**
   * Rewrite a 16-step 4/4 drum row for another meter.
   *
   * Truncating the string would be simpler and wrong: the snare on beat 4 of a
   * 4/4 bar is the thing that falls off the end, and a backbeat is not
   * decoration. So each instrument is moved according to what it is for —
   * hats keep their pulse, snares land on the new bar's backbeats, and
   * everything anchored to a downbeat is mirrored onto each accent group.
   */
  function adaptRow(row, inst, meter) {
    const n = meter.steps;
    if (n === 16) return row;

    if (PULSE_INSTS.indexOf(inst) >= 0) {
      // Periodic by nature: tile it and cut to length.
      let out = row;
      while (out.length < n) out += row;
      return out.slice(0, n);
    }

    const chars = row.split('');
    const loudest = chars.filter(function (c) { return c !== '.'; })
      .sort(function (a, b) { return (VEL_CHAR[b] || 0) - (VEL_CHAR[a] || 0); })[0];
    if (!loudest) return '.'.repeat(n);

    if (BACKBEAT_INSTS.indexOf(inst) >= 0) {
      const out = '.'.repeat(n).split('');
      meter.backbeats.forEach(function (step) { if (step < n) out[step] = loudest; });
      // Keep any ghost notes that still land inside the shorter bar.
      for (let i = 0; i < Math.min(n, row.length); i++) {
        const c = row.charAt(i);
        if (c !== '.' && (VEL_CHAR[c] || 0) < 0.6 && out[i] === '.') out[i] = c;
      }
      return out.join('');
    }

    /* Anchored: kick, tom, crash, riser, impact. Take the figure from the first
       accent group and restate it at each of the new bar's accents, so a 5/4
       bar gets its kick at 1 and at the start of its second group rather than
       an arbitrary slice of a 4/4 pattern. */
    const groupLen = Math.max(1, Math.round(n / meter.accents.length));
    const figure = row.slice(0, Math.min(groupLen, row.length));
    const out = '.'.repeat(n).split('');
    meter.accents.forEach(function (start) {
      for (let i = 0; i < figure.length && start + i < n; i++) {
        if (figure.charAt(i) !== '.') out[start + i] = figure.charAt(i);
      }
    });
    return out.join('');
  }

  /** A whole pattern (one object of instrument rows) rewritten for a meter. */
  function adaptPattern(pattern, meter) {
    if (meter.steps === 16) return pattern;
    const out = {};
    Object.keys(pattern).forEach(function (inst) {
      out[inst] = adaptRow(pattern[inst], inst, meter);
    });
    return out;
  }

  /* ------------------------------------------------------------------ *
   * Titles — flavour, not function.
   * ------------------------------------------------------------------ */
  const TITLE_A = ['Slow', 'Neon', 'Paper', 'Glass', 'Late', 'Quiet', 'Hollow', 'Velvet', 'Distant',
    'Amber', 'Northern', 'Broken', 'Soft', 'Electric', 'Second', 'Empty', 'Golden', 'Silver',
    'Midnight', 'Winter', 'Rust', 'Static', 'Weightless', 'Half', 'Low'];
  const TITLE_B = ['Signal', 'Harbor', 'Lantern', 'Orbit', 'Static', 'Machine', 'Avenue', 'Summer',
    'Ghost', 'Circuit', 'Window', 'Tide', 'Drive', 'Letter', 'Engine', 'Garden', 'Frequency',
    'Weather', 'Mirror', 'Season', 'Horizon', 'Traffic', 'Daylight', 'Dial Tone', 'Motion'];

  function makeTitle(rng) {
    return rng.pick(TITLE_A) + ' ' + rng.pick(TITLE_B);
  }

  /* ------------------------------------------------------------------ *
   * Rhythm cells — one bar of sixteenth-note onsets.
   * ------------------------------------------------------------------ */
  const CELLS = {
    sparse:   [[0], [0, 8], [8], [0, 10], [4, 12]],
    calm:     [[0, 6], [0, 8, 12], [0, 4, 10], [2, 8], [0, 8, 14]],
    medium:   [[0, 4, 8, 12], [0, 3, 8, 11], [0, 4, 6, 12], [0, 2, 8, 10, 14], [0, 6, 8, 14]],
    busy:     [[0, 2, 4, 6, 8, 10, 12, 14], [0, 2, 3, 6, 8, 10, 12, 14], [0, 3, 6, 8, 11, 14],
               [0, 2, 4, 7, 8, 10, 12, 15], [0, 1, 2, 6, 8, 9, 10, 14]],
    offbeat:  [[2, 6, 10, 14], [2, 6, 8, 14], [3, 6, 11, 14]]
  };

  function pickCell(rng, density) {
    if (density < 0.3) return rng.pick(CELLS.sparse);
    if (density < 0.5) return rng.pick(CELLS.calm);
    if (density < 0.72) return rng.pick(CELLS.medium);
    return rng.pick(CELLS.busy);
  }

  /* ------------------------------------------------------------------ *
   * Structure
   * ------------------------------------------------------------------ */

  const ENERGY = { intro: 0.35, verse: 0.65, chorus: 1.0, bridge: 0.5, outro: 0.3 };

  function planStructure(rng, targetBars) {
    const sections = [];
    const introBars = targetBars >= 48 ? 8 : 4;
    const outroBars = targetBars >= 48 ? 8 : 4;
    let remaining = Math.max(16, targetBars - introBars - outroBars);

    sections.push({ type: 'intro', bars: introBars });

    const seq = ['verse', 'chorus', 'verse', 'bridge', 'chorus', 'chorus'];
    let i = 0;
    while (remaining >= 8) {
      sections.push({ type: seq[i % seq.length], bars: 8 });
      remaining -= 8;
      i++;
    }
    if (remaining >= 4) sections.push({ type: 'verse', bars: 4 });

    sections.push({ type: 'outro', bars: outroBars });

    // Human-readable labels: Verse 1, Chorus 2, ...
    const counts = {};
    let bar = 0;
    for (let s = 0; s < sections.length; s++) {
      const sec = sections[s];
      counts[sec.type] = (counts[sec.type] || 0) + 1;
      const cap = sec.type.charAt(0).toUpperCase() + sec.type.slice(1);
      const multi = sections.filter(function (x) { return x.type === sec.type; }).length > 1;
      sec.name = multi ? cap + ' ' + counts[sec.type] : cap;
      sec.startBar = bar;
      sec.energy = ENERGY[sec.type];
      bar += sec.bars;
    }
    return sections;
  }

  /* ------------------------------------------------------------------ *
   * Harmony
   * ------------------------------------------------------------------ */

  function pickScale(rng, genre, mood) {
    let pool = genre.scales.slice();
    const bias = mood.scaleBias;
    pool = pool.map(function (entry) {
      const id = entry[0], w = entry[1];
      const sc = T.SCALES[id];
      let weight = w;
      if (bias === 'bright' && !sc.minorish) weight *= 2.2;
      if (bias === 'bright' && sc.minorish) weight *= 0.6;
      if (bias === 'dark' && sc.minorish) weight *= 2.2;
      if (bias === 'dark' && !sc.minorish) weight *= 0.4;
      if (bias === 'minorish' && sc.minorish) weight *= 1.6;
      return [id, weight];
    });
    return rng.weighted(pool);
  }

  /* ------------------------------------------------------------------ *
   * Key changes
   *
   * A section can sit in a different key from the one the song started in.
   * `sec.keyShift` is semitones from the song's root, so transposing the whole
   * song still works (it moves the root, not the shifts) and rearranging moves
   * a modulation along with the section it belongs to.
   * ------------------------------------------------------------------ */

  /** Root pitch class in force at a given beat. */
  function keyRootAt(song, beat) {
    const sec = sectionOf(song, beat);
    const shift = sec && sec.keyShift ? sec.keyShift : 0;
    return ((song.rootPc + shift) % 12 + 12) % 12;
  }

  /* The gear change: lift the last big section a step or a semitone. Common
     enough in pop to be a cliché, which is exactly why its absence was
     noticeable. Only worth doing when there is a section late enough for the
     lift to feel like an arrival. */
  function planKeyChange(rng, song, genre) {
    song.sections.forEach(function (sec) { sec.keyShift = 0; });
    if (!genre.modulates || song.sections.length < 4) return;
    if (!rng.chance(genre.modulates)) return;

    // The last chorus, or failing that the last full-strength section.
    let at = -1;
    for (let i = song.sections.length - 1; i >= 0; i--) {
      if (song.sections[i].type === 'chorus') { at = i; break; }
    }
    if (at < 0) {
      for (let i = song.sections.length - 1; i >= 0; i--) {
        if (song.sections[i].energy >= 0.9) { at = i; break; }
      }
    }
    if (at < 2) return;                       // too early to be an arrival

    const shift = rng.weighted([[1, 2], [2, 3], [3, 1]]);
    for (let i = at; i < song.sections.length; i++) song.sections[i].keyShift = shift;
    song.keyChange = { atBar: song.sections[at].startBar, semitones: shift };
  }

  /** True when this chord is the last one before the section ends. */
  function lastBarOfChord(bar, bars, sec) {
    return bar + bars < sec.startBar + sec.bars;
  }

  function buildHarmony(rng, song, genre, mood) {
    const beatsPerBar = bpb(song);
    const scaleSteps = song.scaleSteps;
    const rootMidi = T.midi(song.rootPc, 4);        // chord construction octave
    const progression = rng.pick(genre.progressions);
    const bridgeProg = rng.pick(genre.progressions);
    const shapeMain = rng.weighted(genre.chordShapes);

    const timeline = [];
    let prevVoicing = null;
    let prevBassPc = null;
    const inversionChance = genre.inversions === undefined ? 0.18 : genre.inversions;

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      // A modulated section is built in its own key, so every part follows.
      const sectionRoot = T.midi(((song.rootPc + (sec.keyShift || 0)) % 12 + 12) % 12, 4);
      let prog = sec.type === 'bridge' ? bridgeProg : progression;
      // Open and close on the tonic so the song feels anchored, even when the
      // progression itself starts somewhere else (a ii-V-I, say).
      if (sec.type === 'intro' || sec.type === 'outro') {
        const tonicAt = prog.indexOf(0);
        if (tonicAt > 0) prog = prog.slice(tonicAt).concat(prog.slice(0, tonicAt));
      }
      const barsPerChord = song.barsPerChord && song.barsPerChord > 0
        ? Math.min(song.barsPerChord, sec.bars)
        : (sec.type === 'intro' || sec.type === 'outro' || sec.type === 'ambientish'
          ? Math.max(genre.barsPerChord[0], 2)
          : rng.chance(0.5) ? genre.barsPerChord[0] : genre.barsPerChord[1]);
      /* Borrowed chords are colour: plenty in a jazz or gospel bridge, none in
         an intro that is meant to sit still. */
      const borrowChance = (genre.borrow || 0) *
        (sec.type === 'intro' || sec.type === 'outro' ? 0 : sec.energy >= 0.9 ? 1.2 : 0.7);

      sec.chords = [];
      let bar = sec.startBar;
      let step = 0;
      let turnaround = false;      // the next chord is a one-bar pivot
      while (bar < sec.startBar + sec.bars) {
        let degree = prog[step % prog.length];
        /* Cadence. A progression left to cycle ends a section wherever the loop
           happens to stop, which is why sections used to run into each other
           without ever sounding finished. The last chord of a section is chosen
           for where the music is going: a chorus or an ending lands home, a
           verse or a bridge stops on the dominant and leans forward. */
        const atEnd = bar + Math.min(barsPerChord, sec.startBar + sec.bars - bar)
                      >= sec.startBar + sec.bars;
        if (atEnd && rng.chance(0.75)) {
          if (sec.type === 'chorus' || sec.type === 'outro' || sec.type === 'intro') degree = 0;
          else if (scaleSteps.length > 4) degree = 4;         // the dominant
        }

        // Simpler shapes in low-energy sections, richer in the chorus.
        let shape = shapeMain;
        // A suspension is colour, not a harmony. Left as the song-wide shape it
        // produces a progression that never resolves anywhere.
        if (shape === 'sus2' || shape === 'sus4') {
          shape = rng.chance(0.28) ? shapeMain : (rng.chance(0.5) ? 'triad' : 'seventh');
        }
        if (sec.energy <= 0.4 && rng.chance(0.4)) shape = 'triad';
        if (sec.type === 'chorus' && rng.chance(mood.extBias)) {
          shape = shape === 'triad' ? 'seventh' : shape;
        }
        // Some shapes collapse on some degrees (a sus4 whose fourth is a
        // tritone, say). Fall back through simpler shapes until one holds up.
        let pitches = null;
        const candidates = [shape, 'seventh', 'triad'];
        for (let ci = 0; ci < candidates.length; ci++) {
          const p = T.sweetenChord(T.buildChord(scaleSteps, sectionRoot, degree, candidates[ci]));
          if (T.chordIsSound(p)) { pitches = p; shape = candidates[ci]; break; }
        }
        if (!pitches) {
          const r = T.degreePitch(scaleSteps, sectionRoot, degree);
          pitches = [r, r + 7];           // last resort: a bare fifth always works
          shape = 'power';
        }
        let bars = Math.min(barsPerChord, sec.startBar + sec.bars - bar);

        /* Turnaround: the last bar of a phrase pivots onto the dominant and
           hands you back to the top. Without one, a section is the same few
           chords repeated; with one, the phrase has a hinge you can hear.
           Measured in four-bar phrases, which is what the sections here
           actually are — an eight-bar rule would never fire inside an
           eight-bar section, which is every section this composer writes. */
        const into = bar - sec.startBar;
        const PHRASE = 4;
        if (turnaround) {
          degree = scaleSteps.length > 4 ? 4 : 0;
          bars = 1;
          turnaround = false;
        } else if (sec.bars >= 8 && bars >= 2 && bars < PHRASE &&
                   into + bars < sec.bars &&
                   (into + bars) % PHRASE === 0 && rng.chance(0.45)) {
          /* Only a chord shorter than the phrase can give up its last bar. A
             chord that *is* the whole phrase is not a candidate — splitting it
             would mean nobody who asks for one change every four bars ever gets
             one. */
          bars -= 1;
          turnaround = true;
        }
        const isSectionEnd = bar + bars >= sec.startBar + sec.bars;

        /* A secondary dominant: the chord a fifth above where we are going,
           made major with a flat seventh whether or not the key contains those
           notes. It is the strongest pull in tonal music and it cannot be built
           by stacking scale degrees, which is why every progression here used
           to sound like it never left home. */
        let borrowed = null;
        const nextDegree = prog[(step + 1) % prog.length];
        if (lastBarOfChord(bar, bars, sec) && borrowChance > 0 && rng.chance(borrowChance) &&
            nextDegree !== degree) {
          const targetRoot = T.degreePitch(scaleSteps, sectionRoot, nextDegree);
          const domRoot = targetRoot - 5;          // a fifth above the target
          const dom = [domRoot, domRoot + 4, domRoot + 7, domRoot + 10];
          if (T.chordIsSound(dom)) { pitches = dom; borrowed = 'V/' + (nextDegree + 1); shape = 'seventh'; }
        }

        /* Inversion. Putting the third or fifth in the bass is what lets a bass
           line walk down under held harmony instead of jumping to each root —
           which is the whole reason slash chords exist. Only taken when it
           actually moves less than the root would. */
        let bassNote = pitches[0];
        if (!borrowed && !isSectionEnd && inversionChance > 0 && pitches.length >= 3 &&
            rng.chance(inversionChance) && prevBassPc !== null) {
          const options = [pitches[1], pitches[2]];
          const dist = function (p) {
            const d = Math.abs((((p - prevBassPc) % 12) + 12) % 12);
            return Math.min(d, 12 - d);
          };
          const rootMove = dist(pitches[0]);
          options.forEach(function (p) { if (dist(p) < rootMove) bassNote = p; });
        }
        prevBassPc = bassNote;

        const voicing = T.voiceChord(pitches, prevVoicing, genre.chords.octaveLow, genre.chords.octaveHigh);
        prevVoicing = voicing;

        const chord = {
          startBeat: bar * beatsPerBar,
          durBeats: bars * beatsPerBar,
          bar: bar,
          bars: bars,
          degree: degree,
          shape: shape,
          pitches: pitches,
          voicing: voicing,
          rootPitch: pitches[0],
          bassPitch: bassNote,
          name: T.chordName(pitches) +
            (((bassNote % 12) + 12) % 12 !== ((pitches[0] % 12) + 12) % 12
              ? '/' + T.NOTE_NAMES[((bassNote % 12) + 12) % 12] : ''),
          roman: borrowed || T.romanNumeral(scaleSteps, degree, pitches),
          borrowed: !!borrowed,
          section: sec.name
        };
        timeline.push(chord);
        sec.chords.push(chord);
        bar += bars;
        step++;
      }
    }
    song.progression = progression;
    song.chords = timeline;
  }

  function chordAt(song, beat) {
    const list = song.chords;
    for (let i = 0; i < list.length; i++) {
      if (beat >= list[i].startBeat && beat < list[i].startBeat + list[i].durBeats) return list[i];
    }
    return list[list.length - 1];
  }

  /* ------------------------------------------------------------------ *
   * Feel — swing and humanising, applied once at the end.
   * ------------------------------------------------------------------ */

  /* ------------------------------------------------------------------ *
   * Feel
   *
   * Two different things used to be baked into the note times together, and
   * they do not belong together.
   *
   * Humanising is part of the written performance: a few thousandths of a beat
   * and a little louder or quieter, decided by the part's own seed. It stays in
   * the score, because re-rolling the part is what should change it.
   *
   * Swing is not. It is how the score is *played* — the same notes, pushed late
   * on the offbeats — so it belongs at playback, where it can be moved while
   * you listen instead of only when a part is rewritten. `swingTime` below is
   * applied by the scheduler, the offline render and the MIDI writer alike, so
   * what you hear, what you export and what you save all swing identically.
   * ------------------------------------------------------------------ */

  /** Where a beat lands once the groove has had its say. */
  function swingTime(t, swing, push) {
    let out = t;
    if (swing > 0) {
      const eighth = Math.round(t / 0.5);
      if (Math.abs(t - eighth * 0.5) < 1e-6 && eighth % 2 === 1) out += swing * 0.5;
    }
    if (push) {
      /* Push and pull: a groove is not only where the offbeats sit but how the
         sixteenths lean. A positive value drags behind the beat, a negative one
         leans into it. */
      const step = Math.round(t * 4) % 4;
      out += (push[step] || 0);
    }
    return out < 0 ? 0 : out;
  }

  function applyFeel(events, song, rng, amount) {
    for (let i = 0; i < events.length; i++) {
      const e = events[i];
      if (amount > 0) {
        const loose = song.humanise === undefined ? 1 : song.humanise;
        e.t += (rng.next() - 0.5) * 0.02 * amount * loose;
        e.v = Math.max(0.08, Math.min(1, e.v + (rng.next() - 0.5) * 0.12 * amount * loose));
      }
      if (e.t < 0) e.t = 0;
    }
    events.sort(function (a, b) { return a.t - b.t; });
    return events;
  }

  /* ------------------------------------------------------------------ *
   * Grooves
   *
   * A borrowed feel. `swing` is how far the offbeat eighths sit late; `push` is
   * a per-sixteenth lean in beats, which is the part that gives a groove its
   * character — an MPC pushes the second sixteenth, and the drunk feel drags
   * the second and fourth by different amounts so the bar never quite settles.
   * ------------------------------------------------------------------ */

  const GROOVES = {
    straight: { name: 'Straight',   swing: 0,    push: null },
    light:    { name: 'Light swing', swing: 0.12, push: null },
    swung:    { name: 'Swung',      swing: 0.3,  push: null },
    hard:     { name: 'Hard swing', swing: 0.42, push: null },
    mpc:      { name: 'MPC',        swing: 0.24, push: [0, 0.012, 0, 0.006] },
    dilla:    { name: 'Drunk',      swing: 0.18, push: [0, 0.03, -0.012, 0.022] },
    pushed:   { name: 'Pushed',     swing: 0.08, push: [0, -0.014, -0.008, -0.014] },
    laidback: { name: 'Laid back',  swing: 0.14, push: [0, 0.02, 0.014, 0.02] }
  };

  function grooveOf(song) {
    if (song && song.groove && GROOVES[song.groove]) return GROOVES[song.groove];
    return null;
  }

  /** Swing and lean for this song, whether from a groove or its own settings. */
  function feelOf(song) {
    const g = grooveOf(song);
    return {
      swing: g ? g.swing : (song.swing || 0),
      push: g ? g.push : null
    };
  }

  /* ------------------------------------------------------------------ *
   * Drums
   * ------------------------------------------------------------------ */

  const VEL_CHAR = { X: 1.0, x: 0.85, o: 0.45 };

  /** The first step a backbeat instrument actually plays in this pattern. */
  function firstBackbeatStep(pattern, inst) {
    const row = pattern[inst] || '';
    for (let i = 0; i < row.length; i++) if (row.charAt(i) !== '.') return i;
    return -1;
  }

  /**
   * A fill, drawn fresh each time.
   *
   * Four shapes, because the seam between two sections is the one bar a
   * listener is most likely to notice, and hearing the identical figure at
   * every seam turns a signpost into wallpaper. The genre's own written fill
   * stays in the pool so its character survives.
   */
  function makeFill(rng, written, energy, meter) {
    const n = 16;
    const blank = function () { return '.'.repeat(n).split(''); };
    const pick = rng.weighted([
      ['written', written ? 3 : 0],
      ['roll', 3],
      ['toms', 2],
      ['stutter', 2],
      ['drop', energy >= 0.8 ? 2 : 1]
    ]);
    if (pick === 'written' && written) return written;

    const out = {};
    if (pick === 'roll') {
      // A snare roll that tightens as it climbs.
      const snare = blank();
      const from = rng.pick([4, 6, 8]);
      for (let i = from; i < n; i++) snare[i] = i >= n - 4 ? 'X' : (i % 2 ? 'o' : 'x');
      out.snare = snare.join('');
      const kick = blank(); kick[0] = 'x';
      out.kick = kick.join('');
    } else if (pick === 'toms') {
      // A run down the toms, landing on the crash of the next bar.
      const tom = blank();
      [8, 10, 11, 12, 14].forEach(function (i) { tom[i] = i >= 12 ? 'X' : 'x'; });
      out.tom = tom.join('');
      const kick = blank(); kick[0] = 'x'; kick[8] = 'x';
      out.kick = kick.join('');
      const snare = blank(); snare[4] = 'x';
      out.snare = snare.join('');
    } else if (pick === 'stutter') {
      // The kit keeps going, the hats trip over themselves.
      const hh = blank();
      for (let i = 0; i < n; i++) hh[i] = i >= 12 ? 'X' : (i % 2 ? 'o' : 'x');
      out.hh = hh.join('');
      const kick = blank(); kick[0] = 'x'; kick[6] = 'x'; kick[10] = 'x';
      out.kick = kick.join('');
      const snare = blank(); snare[4] = 'x'; snare[12] = 'x'; snare[14] = 'o';
      out.snare = snare.join('');
    } else {
      /* Everything stops, then three hits. Taking the kit away is a louder fill
         than adding to it. */
      const snare = blank();
      [10, 12, 14].forEach(function (i) { snare[i] = 'X'; });
      out.snare = snare.join('');
      const kick = blank(); kick[0] = 'X';
      out.kick = kick.join('');
    }
    return out;
  }

  function patternForEnergy(drums, energy) {
    if (energy <= 0.4) return drums.intro || drums.groove || {};
    if (energy >= 0.95) return drums.full || drums.groove || {};
    return drums.groove || {};
  }

  function composeDrums(song, rng) {
    const meter = meterOf(song);
    const beatsPerBar = meter.beats;
    const genre = song.genre;
    const events = [];
    const drums = genre.drums;

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      const halfTime = !!sec.halfTime;
      const base = adaptPattern(patternForEnergy(drums, sec.energy), meter);
      const isLast = s === song.sections.length - 1;

      const next = song.sections[s + 1];
      // A lift into a bigger section is worth announcing.
      const buildsInto = genre.builds && next && next.energy > sec.energy + 0.25;

      for (let b = 0; b < sec.bars; b++) {
        const bar = sec.startBar + b;
        const barBeat = bar * beatsPerBar;
        const lastBarOfSection = b === sec.bars - 1;

        /* The bar before a chorus: pull the kit out from under the track, run a
           snare roll that tightens as it climbs, and sweep a riser over the top.
           Taking things away is what makes the next bar land. */
        if (buildsInto && lastBarOfSection && sec.bars >= 4) {
          events.push({ t: barBeat, d: 0.25, p: 60, v: 0.95, inst: 'kick' });
          const hits = rng.chance(0.5) ? 16 : 8;
          const step = beatsPerBar / hits;
          for (let i = 0; i < hits; i++) {
            events.push({
              t: barBeat + i * step,
              d: Math.min(0.2, step * 0.8),
              p: 60,
              v: 0.3 + 0.6 * (i / (hits - 1)),
              inst: 'snare'
            });
          }
          events.push({ t: barBeat, d: beatsPerBar, p: 60, v: 0.75, inst: 'riser' });
          continue;
        }

        // The downbeat it builds to.
        if (b === 0 && genre.builds && sec.energy >= 0.95 && s > 0 &&
            song.sections[s - 1].energy < sec.energy - 0.25) {
          events.push({ t: barBeat, d: 2, p: 60, v: 0.85, inst: 'impact' });
        }

        const fillBar = !isLast && lastBarOfSection && sec.bars >= 4 && rng.chance(0.85);
        /* One fill per genre meant every section ended the same way, which is
           the opposite of what a fill is for — it is supposed to mark the seam,
           and a seam you have heard eight times stops marking anything. */
        const pattern = fillBar
          ? adaptPattern(makeFill(rng, drums.fill, sec.energy, meter), meter)
          : base;

        Object.keys(pattern).forEach(function (inst) {
          const row = pattern[inst];
          for (let i = 0; i < row.length; i++) {
            const c = row.charAt(i);
            if (c === '.') continue;
            let vel = VEL_CHAR[c] || 0.7;

            // Drop the odd hit in sparse sections so it breathes.
            if (sec.energy < 0.5 && vel < 0.6 && rng.chance(0.35)) continue;
            /* Half time: the kit plays at half speed under music that has not
               slowed down. Across two bars the kick keeps the first and the
               backbeat lands in the second, so the pattern the style already
               wrote is stretched rather than replaced — and the hats keep the
               original pulse, which is what makes it feel heavy rather than
               simply slower. */
            if (halfTime) {
              if (inst === 'kick' && bar % 2 === 1) continue;
              if (BACKBEAT_INSTS.indexOf(inst) >= 0) {
                if (bar % 2 === 0) continue;
                if (i !== firstBackbeatStep(pattern, inst)) continue;
              }
            }
            // Occasional extra kick/snare ghost in high energy.
            vel *= 0.85 + sec.energy * 0.2;

            const t = barBeat + i * STEP_BEATS;
            events.push({ t: t, d: 0.25, p: 60, v: Math.min(1, vel), inst: inst });

            // Trap-style hat rolls.
            if (genre.hatRolls && inst === 'hh' && sec.energy > 0.5 && rng.chance(0.08)) {
              const sub = rng.pick([2, 3, 4]);
              for (let k = 1; k < sub; k++) {
                events.push({
                  t: t + (STEP_BEATS / sub) * k, d: 0.12, p: 60,
                  v: Math.min(1, vel * (0.55 + 0.12 * k)), inst: 'hh'
                });
              }
            }
          }
        });

        /* Ghost notes: the quiet in-between hits that make a groove breathe.
           A drummer's left hand never stops moving, and a pattern written only
           as accents is a machine playing the accents. */
        if (!fillBar && sec.energy >= 0.55 && drums.ghosts !== false) {
          const spots = [3, 7, 11, 15].filter(function (x) { return x < meter.steps; });
          for (let g = 0; g < spots.length; g++) {
            if (!rng.chance(0.22 * sec.energy)) continue;
            events.push({
              t: barBeat + spots[g] * STEP_BEATS,
              d: 0.2, p: 60,
              v: 0.16 + rng.next() * 0.12,
              inst: 'snare'
            });
          }
        }

        // Crash the downbeat when a big section starts.
        if (b === 0 && sec.energy >= 0.95) {
          events.push({ t: barBeat, d: 1, p: 60, v: 0.7, inst: 'crash' });
        }
      }
    }
    return applyFeel(events, song, rng, 0.7);
  }

  /* ------------------------------------------------------------------ *
   * Bass
   * ------------------------------------------------------------------ */

  function bassPitch(chord, octave) {
    // A slash chord puts a chord tone other than the root under the harmony.
    const src = chord.bassPitch === undefined ? chord.rootPitch : chord.bassPitch;
    const pc = ((src % 12) + 12) % 12;
    return T.midi(pc, octave);
  }

  function composeBass(song, rng) {
    const beatsPerBar = bpb(song);
    const genre = song.genre;
    const style = genre.bass.style;
    const oct = genre.bass.octave;
    const events = [];

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      if (sec.type === 'intro' && rng.chance(0.55)) continue;   // let the intro breathe
      const energy = sec.energy;

      for (let b = 0; b < sec.bars; b++) {
        const bar = sec.startBar + b;
        const barBeat = bar * beatsPerBar;
        const chord = chordAt(song, barBeat);
        const root = bassPitch(chord, oct);
        const fifth = root + 7;
        const octaveUp = root + 12;
        const vel = 0.62 + energy * 0.25;

        /* Every figure below is written in fractions of the bar it is in, not
           in a fixed count of eighths. A bassline that assumes eight eighth
           notes writes a whole extra beat into a three-beat bar — which spills
           into the next one, and past the end of the song at the last. */
        const eighths = Math.round(beatsPerBar * 2);
        const quarters = Math.floor(beatsPerBar);
        const last = beatsPerBar - 0.5;                 // the final eighth of the bar

        if (style === 'root8') {
          for (let i = 0; i < eighths; i++) {
            const t = barBeat + i * 0.5;
            let p = root;
            if (i === eighths - 1 && rng.chance(0.3)) p = fifth;
            if (i % 4 === 2 && rng.chance(0.18)) p = octaveUp;
            events.push({ t: t, d: 0.45, p: p, v: vel * (i % 2 === 0 ? 1 : 0.82) });
          }
        } else if (style === 'pulse8') {
          for (let i = 0; i < eighths; i++) {
            const p = i % 4 === 3 ? octaveUp : root;
            events.push({ t: barBeat + i * 0.5, d: 0.42, p: p, v: vel * (i % 2 === 0 ? 1 : 0.8) });
          }
        } else if (style === 'offbeat') {
          events.push({ t: barBeat, d: 0.4, p: root, v: vel });
          for (let i = 0; i < quarters; i++) {
            const t = barBeat + i + 0.5;
            if (t >= barBeat + beatsPerBar) break;
            let p = root;
            if (rng.chance(0.22)) p = fifth;
            if (rng.chance(0.12)) p = octaveUp;
            events.push({ t: t, d: 0.42, p: p, v: vel * 0.95 });
          }
        } else if (style === 'walk') {
          events.push({ t: barBeat, d: Math.min(1.4, beatsPerBar * 0.4), p: root, v: vel });
          if (beatsPerBar >= 3 && rng.chance(0.7)) {
            events.push({ t: barBeat + 1.5, d: 0.5, p: root + (rng.chance(0.5) ? 7 : 12), v: vel * 0.75 });
          }
          events.push({ t: barBeat + Math.min(2, beatsPerBar - 1), d: 1.0,
                        p: rng.chance(0.6) ? root : fifth, v: vel * 0.9 });
          if (rng.chance(0.45)) {
            const next = chordAt(song, barBeat + beatsPerBar);
            const target = bassPitch(next, oct);
            const approach = target + (rng.chance(0.5) ? -1 : 1);
            events.push({ t: barBeat + last, d: 0.5, p: approach, v: vel * 0.7 });
          }
        } else if (style === 'whole') {
          if (barBeat === chord.startBeat) {
            events.push({ t: barBeat, d: chord.durBeats, p: root, v: vel * 0.9 });
          }
        } else if (style === 'sustain') {
          if (b % 2 === 0) {
            events.push({ t: barBeat, d: beatsPerBar - 0.5, p: root, v: vel });
            if (rng.chance(0.5)) {
              events.push({ t: barBeat + beatsPerBar - 0.25, d: 0.25, p: root + 12, v: vel * 0.7 });
            }
          } else if (rng.chance(0.6)) {
            events.push({ t: barBeat + 1.5, d: Math.max(0.5, beatsPerBar - 1.5),
                          p: rng.chance(0.4) ? fifth : root, v: vel * 0.85 });
          }
        } else if (style === 'slide808') {
          events.push({ t: barBeat, d: beatsPerBar * (rng.chance(0.5) ? 0.625 : 0.44),
                        p: root, v: vel, glide: b > 0 });
          if (rng.chance(0.6)) {
            events.push({ t: barBeat + beatsPerBar * 0.625, d: 1.0, p: root, v: vel * 0.85 });
          }
          if (energy > 0.8 && rng.chance(0.35)) {
            events.push({ t: barBeat + last, d: 0.5, p: root + (rng.chance(0.5) ? 7 : 12),
                          v: vel * 0.8, glide: true });
          }
        }
      }
    }
    return applyFeel(events, song, rng, 0.4);
  }

  /* ------------------------------------------------------------------ *
   * Chords / keys
   * ------------------------------------------------------------------ */

  function composeChords(song, rng) {
    const beatsPerBar = bpb(song);
    const genre = song.genre;
    const style = genre.chords.style;
    const events = [];

    for (let c = 0; c < song.chords.length; c++) {
      const chord = song.chords[c];
      const sec = sectionOf(song, chord.startBeat);
      if (!sec) continue;
      if (sec.type === 'intro' && style === 'stab' && rng.chance(0.5)) continue;
      const vel = 0.45 + sec.energy * 0.3;
      const voicing = chord.voicing;

      if (style === 'pad' || style === 'swell') {
        for (let i = 0; i < voicing.length; i++) {
          events.push({ t: chord.startBeat, d: chord.durBeats - 0.05, p: voicing[i], v: vel * (i === 0 ? 1 : 0.9) });
        }
      } else if (style === 'stab') {
        for (let bar = 0; bar < chord.bars; bar++) {
          const barBeat = chord.startBeat + bar * beatsPerBar;
          /* Offbeat stabs, one per beat when the section is at full strength
             and every other beat otherwise — counted from the bar's own length
             rather than assuming four of them. */
          const hits = [];
          const every = sec.energy >= 0.95 ? 1 : 2;
          for (let q = 0; q + 0.5 < beatsPerBar; q += every) hits.push(q + 0.5);
          for (let h = 0; h < hits.length; h++) {
            if (rng.chance(0.18)) continue;
            for (let i = 0; i < voicing.length; i++) {
              events.push({ t: barBeat + hits[h], d: 0.4, p: voicing[i], v: vel * 0.9 });
            }
          }
        }
      } else if (style === 'keys') {
        for (let bar = 0; bar < chord.bars; bar++) {
          const barBeat = chord.startBeat + bar * beatsPerBar;
          const hits = [0];
          const mid = [1.5, 2.5, 2.75].filter(function (x) { return x < beatsPerBar; });
          if (mid.length && rng.chance(0.75)) hits.push(rng.pick(mid));
          if (beatsPerBar - 0.5 > 1 && rng.chance(0.35)) hits.push(beatsPerBar - 0.5);
          for (let h = 0; h < hits.length; h++) {
            const roll = rng.range(0, 0.035);       // gentle hand-rolled feel
            for (let i = 0; i < voicing.length; i++) {
              events.push({
                t: barBeat + hits[h] + roll * i,
                d: h === 0 ? 1.8 : 0.9,
                p: voicing[i],
                v: vel * (h === 0 ? 1 : 0.78) * (i === 0 ? 1 : 0.92)
              });
            }
          }
        }
      } else if (style === 'arpChord') {
        const tones = voicing.slice();
        const steps = Math.round(chord.durBeats / 0.25);
        for (let i = 0; i < steps; i++) {
          const p = tones[i % tones.length];
          events.push({ t: chord.startBeat + i * 0.25, d: 0.22, p: p, v: vel * (i % 4 === 0 ? 1 : 0.8) });
        }
      }
    }
    return applyFeel(events, song, rng, 0.3);
  }

  function composePad(song, rng) {
    const events = [];
    for (let c = 0; c < song.chords.length; c++) {
      const chord = song.chords[c];
      const sec = sectionOf(song, chord.startBeat);
      if (!sec || !sec.parts.pad) continue;
      const vel = 0.3 + sec.energy * 0.25;
      const voicing = chord.voicing;
      for (let i = 0; i < voicing.length; i++) {
        if (i > 3) break;
        events.push({ t: chord.startBeat, d: chord.durBeats + 0.5, p: voicing[i] - 12, v: vel });
      }
    }
    return events;
  }

  /* ------------------------------------------------------------------ *
   * Arpeggio
   * ------------------------------------------------------------------ */

  /**
   * A riff: one figure, invented once, restated on every chord.
   *
   * This is not an arpeggio. An arpeggio runs whatever notes the chord happens
   * to contain, so it changes shape every time the harmony does — which is why
   * it decorates rather than hooks. A riff keeps its rhythm and its intervals
   * and moves bodily to each new root, and that repetition is the hook. Most
   * rock and funk is built on one.
   */
  function makeRiff(rng, stepsPerBar, density) {
    const figure = [];
    const slots = [];
    for (let i = 0; i < stepsPerBar; i += 2) slots.push(i);
    const count = Math.max(3, Math.round(slots.length * (0.45 + density * 0.4)));
    const chosen = rng.shuffle(slots).slice(0, count).sort(function (a, b) { return a - b; });

    // Always land on the downbeat: a riff that doesn't is a fill.
    if (chosen[0] !== 0) chosen.unshift(0);

    let degree = 0;
    for (let i = 0; i < chosen.length; i++) {
      if (i > 0) {
        // Small steps, occasionally a leap, and pulled back toward the root.
        const move = rng.weighted([[0, 2], [1, 3], [-1, 3], [2, 2], [-2, 2], [4, 1], [-4, 1]]);
        degree = Math.max(-4, Math.min(7, degree + move));
        if (Math.abs(degree) > 5 && rng.chance(0.6)) degree = rng.chance(0.5) ? 0 : 2;
      } else {
        degree = 0;
      }
      const next = i + 1 < chosen.length ? chosen[i + 1] : stepsPerBar;
      figure.push({
        step: chosen[i],
        dur: Math.max(1, Math.min(4, next - chosen[i])),
        degree: degree,
        accent: chosen[i] === 0 || chosen[i] % 4 === 0
      });
    }
    return figure;
  }

  function composeArp(song, rng) {
    const beatsPerBar = bpb(song);
    const stepsPerBar = spb_(song);
    const genre = song.genre;
    const rate = genre.arp.rate;
    const octave = genre.arp.octave;
    const events = [];
    const shape = rng.pick(['up', 'up', 'updown', 'down', 'upoct']);

    /* Styles built on repetition play a riff instead; the figure is invented
       once for the whole song, which is what makes it recognisable. */
    const asRiff = rng.chance(genre.riff || 0);
    if (asRiff) {
      const figure = makeRiff(rng, stepsPerBar, genre.arp.chance || 0.5);
      for (let c = 0; c < song.chords.length; c++) {
        const chord = song.chords[c];
        const sec = sectionOf(song, chord.startBeat);
        if (!sec || !sec.parts.arp) continue;
        const rootPc = ((chord.rootPitch % 12) + 12) % 12;
        const root = T.midi(rootPc, octave - 1);
        const vel = 0.34 + sec.energy * 0.3;

        for (let b = 0; b < chord.bars; b++) {
          const barBeat = chord.startBeat + b * beatsPerBar;
          for (let i = 0; i < figure.length; i++) {
            const n = figure[i];
            const t = barBeat + n.step * STEP_BEATS;
            if (t >= song.totalBeats) break;
            events.push({
              t: t,
              d: Math.max(0.12, n.dur * STEP_BEATS * 0.9),
              p: T.degreePitch(song.scaleSteps, root, n.degree),
              v: vel * (n.accent ? 1 : 0.8)
            });
          }
        }
      }
      return applyFeel(events, song, rng, 0.25);
    }

    for (let c = 0; c < song.chords.length; c++) {
      const chord = song.chords[c];
      const sec = sectionOf(song, chord.startBeat);
      if (!sec || !sec.parts.arp) continue;

      const pcs = chord.pitches.map(function (p) { return ((p % 12) + 12) % 12; });
      let tones = pcs.map(function (pc) { return T.midi(pc, octave - 1); });
      tones.sort(function (a, b) { return a - b; });
      // Keep it rising rather than folding back on itself.
      for (let i = 1; i < tones.length; i++) if (tones[i] < tones[i - 1]) tones[i] += 12;

      let order = tones.slice();
      if (shape === 'down') order = tones.slice().reverse();
      if (shape === 'updown') order = tones.concat(tones.slice(1, -1).reverse());
      if (shape === 'upoct') order = tones.concat(tones.map(function (p) { return p + 12; }));

      const steps = Math.round(chord.durBeats / rate);
      const vel = 0.3 + sec.energy * 0.3;
      for (let i = 0; i < steps; i++) {
        if (rng.chance(0.05)) continue;
        events.push({
          t: chord.startBeat + i * rate,
          d: Math.max(0.1, rate * 0.9),
          p: order[i % order.length],
          v: vel * (i % 4 === 0 ? 1 : 0.82)
        });
      }
    }
    return applyFeel(events, song, rng, 0.25);
  }

  /* ------------------------------------------------------------------ *
   * Lead melody — motif first, then variations of it.
   * ------------------------------------------------------------------ */

  function makeMotif(rng, density, bars, stepsPerBar) {
    stepsPerBar = stepsPerBar || STEPS_PER_BAR;
    const notes = [];
    for (let b = 0; b < bars; b++) {
      // Rhythm cells are written on a sixteen-step bar; in a shorter bar the
      // onsets that fall off the end are simply not played.
      const cell = pickCell(rng, density).filter(function (x) { return x < stepsPerBar; });
      for (let i = 0; i < cell.length; i++) {
        notes.push({ step: b * stepsPerBar + cell[i] });
      }
    }
    // Durations run to the next onset, capped so phrases stay articulate.
    for (let i = 0; i < notes.length; i++) {
      const next = i + 1 < notes.length ? notes[i + 1].step : bars * stepsPerBar;
      notes[i].dur = Math.min(next - notes[i].step, 8);
    }
    // Contour: a small random walk in scale steps, mostly stepwise.
    let cur = 0;
    for (let i = 0; i < notes.length; i++) {
      notes[i].contour = cur;
      const move = rng.weighted([[-2, 1.2], [-1, 3], [0, 1.4], [1, 3], [2, 1.2], [3, 0.5], [-3, 0.5]]);
      cur += move;
      if (cur > 7) cur -= 5;
      if (cur < -5) cur += 5;
    }
    return notes;
  }

  function transformMotif(rng, motif, kind) {
    const out = motif.map(function (n) { return { step: n.step, dur: n.dur, contour: n.contour }; });
    if (kind === 'transpose') {
      const by = rng.pick([-2, -1, 1, 2]);
      for (let i = 0; i < out.length; i++) out[i].contour += by;
    } else if (kind === 'invert') {
      for (let i = 0; i < out.length; i++) out[i].contour = -out[i].contour;
    } else if (kind === 'tail') {
      // Same opening, new ending — the classic question/answer phrase.
      const half = Math.floor(out.length / 2);
      let cur = out[half] ? out[half].contour : 0;
      for (let i = half; i < out.length; i++) {
        out[i].contour = cur;
        cur += rng.weighted([[-2, 1], [-1, 3], [1, 2], [2, 1]]);
      }
    } else if (kind === 'thin') {
      return out.filter(function (n, i) { return i % 2 === 0 || rng.chance(0.4); });
    }
    return out;
  }

  /**
   * Let a phrase land. A melody that never stops for breath reads as a stream
   * of notes rather than a line: this clears the last beat, then leans the
   * final note onto a chord tone and holds it, which is what a cadence is.
   */
  function cadence(song, phraseEvents, endBeat, breathBeats) {
    if (!phraseEvents.length) return phraseEvents;
    const breath = endBeat - (breathBeats === undefined ? 0.75 : breathBeats);
    let kept = phraseEvents.filter(function (e) { return e.t < breath; });
    if (!kept.length) kept = [phraseEvents[0]];

    const last = kept[kept.length - 1];
    const chord = chordAt(song, last.t);
    last.p = T.nearestChordTone(last.p, chord.pitches);
    last.d = Math.max(last.d, Math.min(1.5, breath - last.t));
    last.v = Math.min(1, last.v * 1.05);
    return kept;
  }

  function composeLead(song, rng, opts) {
    const beatsPerBar = bpb(song);
    const stepsPerBar = spb_(song);
    const genre = song.genre;
    const octave = genre.lead.octave;
    const density = Math.max(0.15, Math.min(0.95, genre.lead.density + song.mood.density));
    const events = [];
    const motifBars = 2;

    // A motif handed in came from notes the user drew: develop that idea
    // everywhere instead of inventing three of our own.
    const given = opts && opts.motif;
    const verseMotif = makeMotif(rng, density * 0.85, motifBars, stepsPerBar);
    const motifs = given ? {
      verse: given,
      chorus: transformMotif(rng, given, 'transpose'),
      bridge: transformMotif(rng, given, 'invert')
    } : {
      verse: verseMotif,
      /* The chorus quotes the verse instead of starting again. The rhythm is
         what the ear recognises, so it survives; the contour is what makes it a
         different phrase, so that is what moves. An unrelated chorus reads as a
         different track spliced in. The bridge is the one place a genuinely new
         idea belongs. */
      chorus: transformMotif(rng, verseMotif, rng.pick(['transpose', 'tail', 'invert'])),
      bridge: makeMotif(rng, density * 0.7, motifBars, stepsPerBar)
    };

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      if (!sec.parts.lead) continue;
      const baseMotif = motifs[sec.type] || motifs.verse;
      const phrases = Math.floor(sec.bars / motifBars);

      for (let ph = 0; ph < phrases; ph++) {
        let motif;
        if (ph % 4 === 3) motif = transformMotif(rng, baseMotif, rng.pick(['tail', 'transpose', 'invert']));
        else if (ph % 2 === 1) motif = transformMotif(rng, baseMotif, rng.pick(['tail', 'thin', 'transpose']));
        else motif = baseMotif;

        const phraseBeat = (sec.startBar + ph * motifBars) * beatsPerBar;
        const phraseEvents = [];

        for (let i = 0; i < motif.length; i++) {
          const n = motif[i];
          const t = phraseBeat + n.step * STEP_BEATS;
          if (t >= song.totalBeats) continue;
          if (rng.chance(genre.lead.restBias * 0.35)) continue;   // leave some air

          const chord = chordAt(song, t);
          const center = T.midi(((chord.rootPitch % 12) + 12) % 12, octave);
          let pitch = T.degreePitch(song.scaleSteps, center, n.contour);
          const strong = n.step % 4 === 0;
          // Snap to the key in force here, which is not the opening key once
          // the song has modulated.
          const localRoot = T.midi(keyRootAt(song, t), octave);
          pitch = strong
            ? T.nearestChordTone(pitch, chord.pitches)
            : T.snapToScale(pitch, song.scaleSteps, localRoot);

          // Keep the melody in a singable window.
          while (pitch > localRoot + 16) pitch -= 12;
          while (pitch < localRoot - 8) pitch += 12;

          phraseEvents.push({
            t: t,
            d: Math.max(0.2, n.dur * STEP_BEATS * 0.95),
            p: pitch,
            v: (0.55 + sec.energy * 0.3) * (strong ? 1 : 0.85)
          });
        }

        /* Every phrase gets a breath; every second one gets a proper close.
           A singer has to inhale, and the silence is what makes a line a line
           rather than a stream of notes — it is also what leaves the second
           voice somewhere to answer. */
        const phraseEnd = phraseBeat + motifBars * beatsPerBar;
        const closing = ph % 2 === 1;
        const finished = cadence(song, phraseEvents, phraseEnd, closing ? 1.75 : 0.9);
        Array.prototype.push.apply(events, finished);
      }
    }
    return applyFeel(events, song, rng, 0.5);
  }

  /* ------------------------------------------------------------------ *
   * Learning from the person using it
   *
   * This is the loop that makes the editor and the composer one program
   * rather than two: notes someone drew by hand come back in as a motif, and
   * the composer develops that idea across the whole song the same way it
   * develops one of its own.
   * ------------------------------------------------------------------ */

  /** Nearest scale-degree index for a pitch, counting octaves (so 7 = an octave up). */
  function pitchToDegree(song, pitch) {
    const steps = song.scaleSteps;
    const rootMidi = T.midi(song.rootPc, 4);
    const rel = pitch - rootMidi;
    const oct = Math.floor(rel / 12);
    const pc = ((rel % 12) + 12) % 12;
    let best = 0, bestD = 99;
    for (let i = 0; i < steps.length; i++) {
      const d = Math.abs(steps[i] - pc);
      if (d < bestD) { bestD = d; best = i; }
    }
    return oct * steps.length + best;
  }

  /**
   * Turn hand-drawn notes into a motif: their rhythm, and the shape of the
   * line as scale steps away from its first note. Returns null when there is
   * not enough to work with.
   */
  function motifFromEvents(song, events, bars) {
    bars = bars || 2;
    if (!events || events.length < 2) return null;
    const span = bars * bpb(song);

    // Take the busiest window of `bars` bars — that is where the idea is.
    let bestStart = 0, bestCount = 0;
    for (let b = 0; b * bpb(song) < song.totalBeats; b += bars) {
      const start = b * bpb(song);
      let n = 0;
      for (let i = 0; i < events.length; i++) {
        if (events[i].t >= start && events[i].t < start + span) n++;
      }
      if (n > bestCount) { bestCount = n; bestStart = start; }
    }
    if (bestCount < 2) return null;

    const win = events
      .filter(function (e) { return e.t >= bestStart && e.t < bestStart + span; })
      .sort(function (a, b) { return a.t - b.t; });

    const base = pitchToDegree(song, win[0].p);
    return win.map(function (e) {
      return {
        step: Math.max(0, Math.round((e.t - bestStart) / STEP_BEATS)),
        dur: Math.max(1, Math.round(e.d / STEP_BEATS)),
        contour: pitchToDegree(song, e.p) - base
      };
    });
  }

  /* ------------------------------------------------------------------ *
   * Changing a song you already have
   *
   * The score is pitches and beats, so moving a song to another key is
   * arithmetic rather than a rewrite — and keeping the song you liked beats
   * rolling the dice again hoping for one as good.
   * ------------------------------------------------------------------ */

  const MELODIC = ['bass', 'chords', 'arp', 'lead', 'pad'];

  function transpose(song, semitones) {
    if (!semitones) return song;

    // Refuse a move that would push a part off the ends of the keyboard.
    let lo = 127, hi = 0;
    MELODIC.forEach(function (t) {
      (song.tracks[t] || []).forEach(function (e) {
        if (e.p < lo) lo = e.p;
        if (e.p > hi) hi = e.p;
      });
    });
    if (hi > 0 && (lo + semitones < 16 || hi + semitones > 104)) return null;

    song.rootPc = ((song.rootPc + semitones) % 12 + 12) % 12;
    song.keyName = T.NOTE_NAMES[song.rootPc] + ' ' + T.SCALES[song.scaleId].name;
    song.transposed = (song.transposed || 0) + semitones;

    song.chords.forEach(function (c) {
      c.pitches = c.pitches.map(function (p) { return p + semitones; });
      c.voicing = c.voicing.map(function (p) { return p + semitones; });
      c.rootPitch += semitones;
      c.name = T.chordName(c.pitches);
    });
    MELODIC.forEach(function (t) {
      (song.tracks[t] || []).forEach(function (e) { e.p += semitones; });
    });
    return song;
  }

  /** Retime without rewriting: every event is in beats already. */
  function setTempo(song, bpm) {
    song.bpm = Math.max(40, Math.min(220, Math.round(bpm)));
    song.duration = song.totalBeats * (60 / song.bpm);
    return song;
  }

  /* ------------------------------------------------------------------ *
   * Changing a chord
   *
   * The hard part is not building the new chord — it is that four other parts
   * were written against the old one. Regenerating them would throw away the
   * rhythm, and any notes drawn by hand. So every part keeps its timing exactly
   * and only its pitches are moved onto the new harmony: a bass note that was
   * the fifth stays the fifth, the third note of a voicing stays the third, and
   * a melody note that was leaning on a chord tone leans on the nearest new one.
   * ------------------------------------------------------------------ */

  function buildChordFor(song, degree, shape, atBeat) {
    // In the local key: a song that has modulated is not in its opening key any
    // more, and building a chord from the old root would be out of tune with
    // everything around it.
    const rootMidi = T.midi(atBeat === undefined ? song.rootPc : keyRootAt(song, atBeat), 4);
    const candidates = [shape || 'triad', 'seventh', 'triad'];
    for (let i = 0; i < candidates.length; i++) {
      const p = T.sweetenChord(T.buildChord(song.scaleSteps, rootMidi, degree, candidates[i]));
      if (T.chordIsSound(p)) return { pitches: p, shape: candidates[i] };
    }
    const r = T.degreePitch(song.scaleSteps, rootMidi, degree);
    return { pitches: [r, r + 7], shape: 'power' };
  }

  /** Move a whole part up or down an octave, if there is room for it. */
  function shiftOctave(song, part, dir) {
    const evs = song.tracks[part];
    if (!evs || !evs.length || part === 'drums') return false;
    const by = dir > 0 ? 12 : -12;
    // Stay inside a range a real instrument could play.
    const lo = Math.min.apply(null, evs.map(function (e) { return e.p; })) + by;
    const hi = Math.max.apply(null, evs.map(function (e) { return e.p; })) + by;
    if (lo < 16 || hi > 108) return false;
    evs.forEach(function (e) { e.p += by; });
    return true;
  }

  /**
   * Thin a part out or fill it in.
   *
   * Both directions have to respect what the part is *for*, which is why this
   * is not simply "delete random notes" / "add random notes". Thinning keeps
   * what falls on a beat and what was played hard, because that is the skeleton
   * a listener is following. Filling adds notes between existing ones, in key
   * and quieter than what they sit between, so the shape stays and only the
   * detail changes.
   */
  function adjustDensity(song, part, dir) {
    const evs = song.tracks[part];
    if (!evs || !evs.length) return false;
    const isDrums = part === 'drums';

    const onBeat = function (e) { return Math.abs(e.t - Math.round(e.t)) < 0.08; };

    if (dir < 0) {
      /* Loud is relative. An absolute velocity threshold does nothing to a part
         that is loud throughout — which is exactly what a part looks like after
         it has been filled in — so the cut is taken from this part's own
         spread: keep what lands on a beat, plus the loudest third. */
      const sorted = evs.map(function (e) { return e.v; }).sort(function (a, b) { return a - b; });
      const cut = sorted[Math.floor(sorted.length * 0.66)];
      let keep = evs.filter(function (e) { return onBeat(e) || e.v >= cut; });

      /* If that removed nothing, the part is all downbeats and all loud. Thin
         it by taking out every second offbeat instead, so the button always
         does something or honestly says it cannot. */
      if (keep.length === evs.length) {
        let n = 0;
        keep = evs.filter(function (e) {
          if (onBeat(e)) return true;
          return (n++ % 2) === 0;
        });
      }
      if (keep.length === evs.length) return false;
      // Never thin a part out of existence.
      if (keep.length < Math.max(4, evs.length * 0.25)) return false;
      song.tracks[part] = keep;
      return true;
    }

    const added = [];
    for (let i = 0; i < evs.length; i++) {
      const e = evs[i];
      const next = evs[i + 1];
      const room = next ? next.t - e.t : e.d;
      /* Gauge the gap, not the note. Every drum hit is a quarter of a beat
         long by design, so a rule that needed a longer note could never add a
         single drum — which is exactly what it did. */
      if (room < 0.45) continue;
      if (!isDrums && e.d < 0.3) continue;
      const t = e.t + room / 2;
      if (t >= song.totalBeats) continue;

      if (isDrums) {
        // A ghost note, not another accent: the point is detail, not more noise.
        added.push({ t: t, d: 0.25, p: 60, v: Math.max(0.2, e.v * 0.45),
                     inst: e.inst === 'kick' ? 'hh' : e.inst });
      } else {
        const target = next ? next.p : e.p;
        const step = target === e.p ? 2 : (target > e.p ? 1 : -1) * 2;
        const p = T.snapToScale(e.p + step, song.scaleSteps,
                                T.midi(keyRootAt(song, t), 4));
        added.push({ t: t, d: Math.min(room / 2, e.d) * 0.8, p: p, v: e.v * 0.8 });
      }
    }
    if (!added.length) return false;
    song.tracks[part] = evs.concat(added).sort(function (a, b) { return a.t - b.t; });
    return true;
  }

  /** Move `pitch` from one voicing onto the matching place in another. */
  function remapByRank(pitch, from, to) {
    if (!from.length || !to.length) return pitch;
    let best = 0, bestD = 1e9;
    for (let i = 0; i < from.length; i++) {
      const d = Math.abs(from[i] - pitch);
      if (d < bestD) { bestD = d; best = i; }
    }
    const octave = Math.round((pitch - from[best]) / 12);
    return to[Math.min(best, to.length - 1)] + octave * 12;
  }

  function setChordDegree(song, index, degree) {
    const chord = song.chords[index];
    if (!chord) return false;

    const built = buildChordFor(song, degree, chord.shape, chord.startBeat);
    const prev = index > 0 ? song.chords[index - 1].voicing : null;
    const genre = song.genre;
    const voicing = T.voiceChord(built.pitches, prev,
      genre.chords.octaveLow, genre.chords.octaveHigh);

    const oldPitches = chord.pitches.slice();
    const oldVoicing = chord.voicing.slice();
    const oldRoot = chord.bassPitch === undefined ? chord.rootPitch : chord.bassPitch;

    chord.degree = degree;
    chord.shape = built.shape;
    chord.pitches = built.pitches;
    chord.voicing = voicing;
    chord.rootPitch = built.pitches[0];
    // Choosing a chord by hand resets it to root position; a slash chord is a
    // voicing decision and the picker is about which chord, not which inversion.
    chord.bassPitch = built.pitches[0];
    chord.borrowed = false;
    chord.name = T.chordName(built.pitches);
    chord.roman = T.romanNumeral(song.scaleSteps, degree, built.pitches);

    const from = chord.startBeat - 0.05;
    const to = chord.startBeat + chord.durBeats - 0.05;
    function inSpan(e) { return e.t >= from && e.t < to; }

    // Chords and pad were written straight from the voicing.
    ['chords', 'pad'].forEach(function (t) {
      (song.tracks[t] || []).forEach(function (e) {
        if (inSpan(e)) e.p = remapByRank(e.p, oldVoicing, voicing);
      });
    });

    // Bass keeps its role: a fifth stays a fifth, an octave stays an octave.
    (song.tracks.bass || []).forEach(function (e) {
      if (!inSpan(e)) return;
      e.p = e.p - oldRoot + chord.bassPitch;
    });

    // The arpeggio runs the chord tones, so move it tone for tone.
    (song.tracks.arp || []).forEach(function (e) {
      if (inSpan(e)) e.p = remapByRank(e.p, oldPitches, built.pitches);
    });

    /* The melody is the delicate one. A note that was sitting on a chord tone
       moves to the nearest new one; a passing note belongs to the scale, which
       has not changed, so it stays exactly where it was. */
    (song.tracks.lead || []).forEach(function (e) {
      if (!inSpan(e)) return;
      const wasChordTone = oldPitches.some(function (p) {
        return ((p - e.p) % 12 + 12) % 12 === 0;
      });
      if (wasChordTone) e.p = T.nearestChordTone(e.p, built.pitches);
    });

    return true;
  }

  /* ------------------------------------------------------------------ *
   * Arranging
   *
   * Sections are just spans of beats, so rearranging a song is: lift each
   * section's notes out with their times made relative, shuffle the blocks,
   * and lay them back down. Every part moves together because every part is
   * cut on the same boundaries.
   * ------------------------------------------------------------------ */

  function relabel(sections) {
    const counts = {};
    const totals = {};
    sections.forEach(function (s) { totals[s.type] = (totals[s.type] || 0) + 1; });
    sections.forEach(function (s) {
      counts[s.type] = (counts[s.type] || 0) + 1;
      const cap = s.type.charAt(0).toUpperCase() + s.type.slice(1);
      s.name = totals[s.type] > 1 ? cap + ' ' + counts[s.type] : cap;
    });
  }

  function extractBlocks(song) {
    return song.sections.map(function (sec) {
      const from = sec.startBar * bpb(song);
      const to = from + sec.bars * bpb(song);
      const tracks = {};
      Object.keys(song.tracks).forEach(function (t) {
        tracks[t] = song.tracks[t]
          .filter(function (e) { return e.t >= from - 0.05 && e.t < to - 0.05; })
          .map(function (e) {
            const c = {};
            for (const k in e) c[k] = e[k];
            c.t = e.t - from;
            return c;
          });
      });
      const chords = song.chords
        .filter(function (c) { return c.startBeat >= from - 0.05 && c.startBeat < to - 0.05; })
        .map(function (c) {
          const n = {};
          for (const k in c) n[k] = c[k];
          n.pitches = c.pitches.slice();
          n.voicing = c.voicing.slice();
          n.startBeat = c.startBeat - from;
          return n;
        });
      return {
        type: sec.type, bars: sec.bars, energy: sec.energy, keyShift: sec.keyShift || 0,
        halfTime: !!sec.halfTime,
        parts: sec.parts, tracks: tracks, chords: chords
      };
    });
  }

  function assemble(song, blocks) {
    const tracks = {};
    Object.keys(song.tracks).forEach(function (t) { tracks[t] = []; });
    const chords = [];
    const sections = [];
    let bar = 0;

    blocks.forEach(function (b) {
      const offset = bar * bpb(song);
      Object.keys(tracks).forEach(function (t) {
        (b.tracks[t] || []).forEach(function (e) {
          const c = {};
          for (const k in e) c[k] = e[k];
          c.t = e.t + offset;
          tracks[t].push(c);
        });
      });
      (b.chords || []).forEach(function (c) {
        const n = {};
        for (const k in c) n[k] = c[k];
        n.pitches = c.pitches.slice();
        n.voicing = c.voicing.slice();
        n.startBeat = c.startBeat + offset;
        n.bar = Math.round(n.startBeat / bpb(song));
        chords.push(n);
      });
      sections.push({
        type: b.type, bars: b.bars, energy: b.energy,
        parts: b.parts, keyShift: b.keyShift || 0, halfTime: !!b.halfTime,
        startBar: bar, chords: []
      });
      bar += b.bars;
    });

    Object.keys(tracks).forEach(function (t) {
      tracks[t].sort(function (a, b2) { return a.t - b2.t; });
    });
    chords.sort(function (a, b2) { return a.startBeat - b2.startBeat; });

    relabel(sections);
    sections.forEach(function (sec) {
      sec.name = sec.name;
      sec.chords = chords.filter(function (c) {
        return c.bar >= sec.startBar && c.bar < sec.startBar + sec.bars;
      });
      sec.chords.forEach(function (c) { c.section = sec.name; });
    });

    song.tracks = tracks;
    song.chords = chords;
    song.sections = sections;
    song.bars = bar;
    song.totalBeats = bar * bpb(song);
    song.duration = song.totalBeats * (60 / song.bpm);
    clampAutomation(song);
    return song;
  }

  /** Copy a section and drop the copy in straight after it. */
  function duplicateSection(song, index) {
    const blocks = extractBlocks(song);
    if (index < 0 || index >= blocks.length) return false;
    const copy = JSON.parse(JSON.stringify(blocks[index]));
    blocks.splice(index + 1, 0, copy);
    assemble(song, blocks);
    return true;
  }

  /** Remove a section. A song has to keep at least one. */
  function deleteSection(song, index) {
    const blocks = extractBlocks(song);
    if (blocks.length <= 1 || index < 0 || index >= blocks.length) return false;
    blocks.splice(index, 1);
    assemble(song, blocks);
    return true;
  }

  /** Swap a section with its neighbour. */
  function moveSection(song, index, delta) {
    const blocks = extractBlocks(song);
    const to = index + delta;
    if (index < 0 || index >= blocks.length || to < 0 || to >= blocks.length) return false;
    const tmp = blocks[index];
    blocks[index] = blocks[to];
    blocks[to] = tmp;
    assemble(song, blocks);
    return true;
  }

  /* ------------------------------------------------------------------ *
   * Saving a whole song
   *
   * A seed reproduces the song the generator wrote. It cannot reproduce the
   * song *you* ended up with — the notes you drew, the chord you swapped, the
   * sections you moved, the fade you added. So a save has to carry the score
   * itself.
   *
   * Two things keep that affordable. The genre and mood are whole objects of
   * settings, and both are rebuilt from their names on the way back in, so
   * nothing that a fresh compose can regenerate is stored. And every note goes
   * out as a bare array of numbers at the precision a note actually needs —
   * a ten-thousandth of a beat is well under a millisecond — which turns
   * roughly 85 KB of JSON per song into roughly 20.
   * ------------------------------------------------------------------ */

  const SAVE_VERSION = 2;
  /* Fixed order: a drum piece is stored as its index here. Only ever append to
     this list — renumbering it would silently turn old saves into nonsense. */
  const DRUM_INSTS = ['kick', 'snare', 'clap', 'hh', 'oh', 'ride', 'tom', 'conga',
                      'perc', 'shaker', 'tamb', 'cowbell', 'crash', 'riser', 'impact'];

  function r4(n) { return Math.round(n * 1e4) / 1e4; }
  function r3(n) { return Math.round(n * 1e3) / 1e3; }

  function packNote(e, isDrums) {
    const out = isDrums
      ? [r4(e.t), r4(e.d), DRUM_INSTS.indexOf(e.inst), r3(e.v)]
      : [r4(e.t), r4(e.d), e.p, r3(e.v)];
    if (e.glide) out.push(1);
    return out;
  }

  function unpackNote(a, isDrums) {
    const e = { t: a[0], d: a[1], p: isDrums ? 60 : a[2], v: a[3] };
    if (isDrums) e.inst = DRUM_INSTS[a[2]] || 'kick';
    if (a[4]) e.glide = true;
    return e;
  }

  /** Everything about this song that a fresh compose could not produce. */
  function packSong(song, extra) {
    const tracks = {};
    Object.keys(song.tracks).forEach(function (name) {
      const isDrums = name === 'drums';
      tracks[name] = song.tracks[name].map(function (e) { return packNote(e, isDrums); });
    });

    const packed = {
      v: SAVE_VERSION,
      seed: song.seed,
      genre: song.genreId,
      mood: song.moodId,
      title: song.title,
      bpm: song.bpm,
      swing: song.swing,
      rootPc: song.rootPc,
      scaleId: song.scaleId,
      keyName: song.keyName,
      meter: song.meter,
      bars: song.bars,
      totalBeats: song.totalBeats,
      pingpong: !!song.pingpong,
      groove: song.groove || '',
      humanise: song.humanise === undefined ? 1 : song.humanise,
      keyChange: song.keyChange || null,
      barsPerChord: song.barsPerChord || 0,
      presetOverride: song.presetOverride || {},
      automation: song.automation || { filter: [], volume: [] },
      progression: song.progression,
      partSeeds: song.partSeeds,
      sections: song.sections.map(function (s) {
        return { type: s.type, bars: s.bars, startBar: s.startBar, energy: s.energy,
                 name: s.name, parts: s.parts, keyShift: s.keyShift || 0,
                 halfTime: !!s.halfTime };
      }),
      chords: song.chords.map(function (c) {
        return { startBeat: r4(c.startBeat), durBeats: r4(c.durBeats), bar: c.bar, bars: c.bars,
                 degree: c.degree, shape: c.shape, rootPitch: c.rootPitch, name: c.name,
                 roman: c.roman, pitches: c.pitches.slice(), voicing: c.voicing.slice() };
      }),
      tracks: tracks
    };
    if (extra) for (const k in extra) packed[k] = extra[k];
    return packed;
  }

  /**
   * Rebuild a song from a save. The compose call is what puts the live genre
   * and mood objects back; everything stored then replaces what it wrote.
   */
  function unpackSong(p) {
    if (!p || p.v !== SAVE_VERSION) return null;

    const song = compose({
      seed: p.seed, genre: p.genre, mood: p.mood,
      key: p.rootPc, bpm: p.bpm, scale: p.scaleId, meter: p.meter
    });

    song.title = p.title;
    song.bpm = p.bpm;
    song.swing = p.swing;
    song.rootPc = p.rootPc;
    song.scaleId = p.scaleId;
    song.scaleSteps = T.SCALES[p.scaleId].steps;
    song.keyName = p.keyName;
    song.meter = METERS[p.meter] ? p.meter : '4/4';
    song.beatsPerBar = METERS[song.meter].beats;
    song.stepsPerBar = METERS[song.meter].steps;
    song.bars = p.bars;
    song.totalBeats = p.totalBeats;
    song.duration = p.totalBeats * (60 / p.bpm);
    song.pingpong = !!p.pingpong;
    song.groove = p.groove && GROOVES[p.groove] ? p.groove : '';
    song.humanise = p.humanise === undefined ? 1 : p.humanise;
    song.keyChange = p.keyChange || null;
    song.barsPerChord = p.barsPerChord || 0;
    song.presetOverride = p.presetOverride || {};
    song.automation = p.automation || { filter: [], volume: [] };
    if (p.progression) song.progression = p.progression;
    if (p.partSeeds) song.partSeeds = p.partSeeds;

    song.chords = p.chords.map(function (c) {
      const n = {};
      for (const k in c) n[k] = c[k];
      n.pitches = c.pitches.slice();
      n.voicing = c.voicing.slice();
      return n;
    });

    song.sections = p.sections.map(function (s) {
      return { type: s.type, bars: s.bars, startBar: s.startBar, energy: s.energy,
               name: s.name, parts: s.parts, keyShift: s.keyShift || 0,
               halfTime: !!s.halfTime, chords: [] };
    });
    // Sections keep their own view of the harmony; re-link it to the restored one.
    song.sections.forEach(function (sec) {
      sec.chords = song.chords.filter(function (c) {
        return c.bar >= sec.startBar && c.bar < sec.startBar + sec.bars;
      });
      sec.chords.forEach(function (c) { c.section = sec.name; });
    });

    song.tracks = {};
    Object.keys(p.tracks).forEach(function (name) {
      const isDrums = name === 'drums';
      song.tracks[name] = p.tracks[name].map(function (a) { return unpackNote(a, isDrums); });
    });

    ensureAutomation(song);
    clampAutomation(song);
    return song;
  }

  /* ------------------------------------------------------------------ *
   * Automation lanes
   *
   * A lane is a sorted list of {t: beats, v: 0-1}. The engine reads them; this
   * end owns keeping them tidy and offers the shapes people actually want, so
   * a fade-out is one button rather than an exercise in drawing straight lines.
   * ------------------------------------------------------------------ */

  const LANE_NAMES = ['filter', 'volume'];

  function ensureAutomation(song) {
    if (!song.automation) song.automation = {};
    LANE_NAMES.forEach(function (k) {
      if (!Array.isArray(song.automation[k])) song.automation[k] = [];
    });
    return song.automation;
  }

  /** Sort by time, clamp into range, and merge points that land on the spot. */
  function tidyLane(song, lane) {
    const auto = ensureAutomation(song);
    const end = song.totalBeats;
    const pts = auto[lane]
      .map(function (p) {
        return { t: Math.max(0, Math.min(end, p.t)), v: Math.max(0, Math.min(1, p.v)) };
      })
      .sort(function (a, b) { return a.t - b.t; });
    const out = [];
    for (let i = 0; i < pts.length; i++) {
      if (out.length && Math.abs(pts[i].t - out[out.length - 1].t) < 1e-6) out[out.length - 1] = pts[i];
      else out.push(pts[i]);
    }
    auto[lane] = out;
    return out;
  }

  /* After the arrangement changes, the song is a different length. Points past
     the new end are dropped rather than squeezed: a fade-out written for bar 40
     is about the ending, and stretching it somewhere else would be a guess. */
  function clampAutomation(song) {
    ensureAutomation(song);
    LANE_NAMES.forEach(function (lane) {
      song.automation[lane] = song.automation[lane].filter(function (p) {
        return p.t <= song.totalBeats + 1e-6;
      });
      tidyLane(song, lane);
    });
  }

  function addPoint(song, lane, t, v) {
    ensureAutomation(song);
    if (LANE_NAMES.indexOf(lane) < 0) return null;
    const pt = { t: Math.max(0, Math.min(song.totalBeats, t)), v: Math.max(0, Math.min(1, v)) };
    song.automation[lane].push(pt);
    tidyLane(song, lane);
    return pt;
  }

  function removePoint(song, lane, index) {
    ensureAutomation(song);
    const pts = song.automation[lane];
    if (!pts || index < 0 || index >= pts.length) return false;
    pts.splice(index, 1);
    return true;
  }

  function clearLane(song, lane) {
    ensureAutomation(song);
    if (LANE_NAMES.indexOf(lane) < 0) return false;
    song.automation[lane] = [];
    return true;
  }

  /** Beat where a section starts, or the end of the song if there isn't one. */
  function beatOfSection(song, match) {
    for (let i = 0; i < song.sections.length; i++) {
      if (match(song.sections[i], i)) return song.sections[i].startBar * bpb(song);
    }
    return -1;
  }

  /**
   * The four moves worth having as one tap. Each is written against the song's
   * own arrangement — the build ends where the chorus actually starts — so they
   * land in the right place whatever the form turned out to be.
   */
  const SHAPES = {
    fadeIn: function (song) {
      const end = Math.min(song.totalBeats, bpb(song) * (song.sections[0] ? song.sections[0].bars : 4));
      clearLane(song, 'volume');
      addPoint(song, 'volume', 0, 0);
      addPoint(song, 'volume', end, 1);
      return 'volume';
    },
    fadeOut: function (song) {
      const last = song.sections[song.sections.length - 1];
      const start = last ? last.startBar * bpb(song) : Math.max(0, song.totalBeats - 16);
      clearLane(song, 'volume');
      addPoint(song, 'volume', start, 1);
      addPoint(song, 'volume', song.totalBeats, 0);
      return 'volume';
    },
    buildToChorus: function (song) {
      /* Build into the first chorus that has room in front of it. A song that
         opens on its chorus — which rearranging can easily produce — has
         nothing to build from, so fall back to the middle of the track. */
      const MIN_RUNWAY = bpb(song) * 2;
      const chorus = beatOfSection(song, function (s) {
        return s.type === 'chorus' && s.startBar * bpb(song) >= MIN_RUNWAY;
      });
      const target = chorus >= MIN_RUNWAY ? chorus : Math.floor(song.totalBeats / 2);
      const start = Math.max(0, target - bpb(song) * 8);
      clearLane(song, 'filter');
      addPoint(song, 'filter', start, 0.22);          // muffled, holding back
      addPoint(song, 'filter', target - 0.01, 1);     // wide open as it lands
      addPoint(song, 'filter', song.totalBeats, 1);
      return 'filter';
    },
    duckTheVerses: function (song) {
      clearLane(song, 'filter');
      song.sections.forEach(function (sec) {
        const from = sec.startBar * bpb(song);
        const to = from + sec.bars * bpb(song);
        const open = sec.type === 'chorus' ? 1 : sec.type === 'intro' || sec.type === 'outro' ? 0.45 : 0.72;
        addPoint(song, 'filter', from, open);
        addPoint(song, 'filter', Math.min(song.totalBeats, to - 0.01), open);
      });
      return 'filter';
    }
  };

  function applyShape(song, name) {
    const fn = SHAPES[name];
    if (!fn) return null;
    const lane = fn(song);
    tidyLane(song, lane);
    return lane;
  }

  /* ------------------------------------------------------------------ *
   * Assembly
   * ------------------------------------------------------------------ */

  function sectionOf(song, beat) {
    const bar = Math.floor(beat / bpb(song));
    for (let i = 0; i < song.sections.length; i++) {
      const s = song.sections[i];
      if (bar >= s.startBar && bar < s.startBar + s.bars) return s;
    }
    return song.sections[song.sections.length - 1];
  }

  function assignParts(song, rng) {
    const genre = song.genre;
    for (let i = 0; i < song.sections.length; i++) {
      const sec = song.sections[i];
      const e = sec.energy;
      sec.parts = {
        drums: true,
        bass: true,
        chords: true,
        pad: e >= 0.5 || sec.type === 'intro' || sec.type === 'outro',
        arp: rng.chance(genre.arp.chance * (0.4 + e)),
        lead: sec.type === 'chorus' ? true
            : sec.type === 'verse' ? rng.chance(0.75)
            : sec.type === 'bridge' ? rng.chance(0.6)
            : sec.type === 'outro' ? rng.chance(0.4)
            : rng.chance(0.25),
        // The answering voice needs something to answer, and room to do it in.
        counter: false
      };
      if (sec.type === 'intro') { sec.parts.lead = sec.parts.lead && rng.chance(0.4); }
      /* The counter-melody answers the lead, so it only appears where the lead
         does, and only where there is room for two voices without them
         tripping over each other. */
      sec.parts.counter = sec.parts.lead && e >= 0.6 &&
        rng.chance(genre.counter ? genre.counter.chance : 0);
      /* Half time belongs to a section that wants weight rather than speed —
         a bridge that drops, or a chorus that lands heavier than the verse. */
      sec.halfTime = (sec.type === 'bridge' || sec.type === 'chorus') &&
        sec.bars >= 8 && rng.chance(genre.halfTime || 0);
    }
  }

  /* ------------------------------------------------------------------ *
   * Counter-melody — the second voice
   *
   * Not a harmony line doubling the lead a third below, which is what a naive
   * second part turns into: two voices moving in parallel read as one thicker
   * voice. This one answers. It plays in the gaps the lead leaves, moves the
   * opposite way to the phrase it is answering, and shuts up the moment the
   * lead comes back in.
   *
   * That is also why it is composed last and reads the lead directly: there is
   * no way to write call and response without knowing what the call was.
   * ------------------------------------------------------------------ */

  function composeCounter(song, rng) {
    const genre = song.genre;
    if (!genre.counter) return [];
    const lead = song.tracks.lead || [];
    if (!lead.length) return [];

    const octave = genre.counter.octave || ((genre.lead.octave || 5) - 1);
    const events = [];
    /* Just under the shortest breath the lead leaves at a phrase end (0.9
       beats), so an ordinary phrase break counts as somewhere to answer. Set it
       any higher and the second voice only ever speaks at the big closes. */
    const MIN_GAP = 0.75;

    song.sections.forEach(function (sec) {
      if (!sec.parts || !sec.parts.counter) return;
      const from = sec.startBar * bpb(song);
      const to = from + sec.bars * bpb(song);
      const inSec = lead.filter(function (e) { return e.t >= from && e.t < to; })
                        .sort(function (a, b) { return a.t - b.t; });
      if (inSec.length < 2) return;

      for (let i = 0; i < inSec.length; i++) {
        const call = inSec[i];
        const callEnd = call.t + call.d;
        const nextAt = i + 1 < inSec.length ? inSec[i + 1].t : to;
        let gap = nextAt - callEnd;
        /* A long held note is an opportunity, not an obstacle: the lead sitting
           still is exactly when a second voice moving underneath is audible as
           a second voice. */
        if (gap < MIN_GAP && call.d >= 1.5) gap = nextAt - (call.t + 0.5);
        if (gap < MIN_GAP) continue;
        if (rng.chance(0.15)) continue;                // not every gap wants filling

        /* Which way did the call move? Answer the other way — contrary motion
           is what makes two lines read as two lines. */
        const prev = i > 0 ? inSec[i - 1].p : call.p;
        const rising = call.p >= prev;
        const dir = rising ? -1 : 1;

        const chord = chordAt(song, callEnd + 0.01);
        const heldUnder = call.d >= 1.5 && nextAt - callEnd < MIN_GAP;
        const start = (heldUnder ? call.t + 0.5 : callEnd) + Math.min(0.5, gap * 0.25);
        const room = Math.max(0.5, nextAt - start - 0.15);
        const count = room >= 2.5 ? 3 : room >= 1.5 ? 2 : 1;
        const step = room / count;

        let pitch = T.nearestChordTone(T.midi(keyRootAt(song, start), octave), chord.pitches);
        for (let n = 0; n < count; n++) {
          const t = start + n * step;
          if (t >= to - 0.05) break;
          if (n > 0) {
            pitch = T.snapToScale(pitch + dir * rng.intRange(1, 3), song.scaleSteps,
                                  T.midi(keyRootAt(song, t), octave));
          }
          // Land the last note of the answer on a chord tone, like a cadence.
          if (n === count - 1) pitch = T.nearestChordTone(pitch, chordAt(song, t).pitches);
          const lo = T.midi(keyRootAt(song, t), octave) - 7;
          const hi = lo + 19;
          while (pitch > hi) pitch -= 12;
          while (pitch < lo) pitch += 12;

          events.push({
            t: t,
            d: Math.max(0.25, step * 0.85),
            p: pitch,
            v: (0.42 + sec.energy * 0.2) * (n === 0 ? 1 : 0.9)
          });
        }
      }
    });

    return applyFeel(events, song, rng, 0.3);
  }

  const LENGTHS = { short: 75, medium: 135, long: 200 };

  function compose(opts) {
    opts = opts || {};
    const seed = opts.seed || T.randomSeed();
    const rng = new T.Rng(seed);

    const genreId = opts.genre && G.GENRES[opts.genre] ? opts.genre : rng.pick(Object.keys(G.GENRES));
    const moodId = opts.mood && G.MOODS[opts.mood] ? opts.mood : rng.pick(Object.keys(G.MOODS));
    const genre = G.GENRES[genreId];
    const mood = Object.assign({ id: moodId }, G.MOODS[moodId]);

    const song = {
      seed: seed,
      title: makeTitle(rng),
      genreId: genreId,
      genre: genre,
      moodId: moodId,
      mood: mood,
      beatsPerBar: BEATS_PER_BAR
    };

    /* Time signature. The genre proposes; an explicit request wins. Everything
       downstream measures bars through the meter rather than assuming four. */
    song.meter = (opts.meter && METERS[opts.meter])
      ? opts.meter
      : rng.weighted(genre.meters || [['4/4', 1]]);
    song.beatsPerBar = METERS[song.meter].beats;
    song.stepsPerBar = METERS[song.meter].steps;

    // Tempo
    let bpm = opts.bpm && opts.bpm > 0
      ? opts.bpm
      : Math.round(rng.range(genre.bpm[0], genre.bpm[1]) + mood.bpm);
    song.bpm = Math.max(40, Math.min(200, bpm));
    song.swing = genre.swing;
    /* Feel. The groove is a playback setting and can be moved while you listen;
       looseness is written into the notes, so it takes effect when a part is
       written or re-rolled. */
    song.groove = opts.groove && GROOVES[opts.groove] ? opts.groove : '';
    song.humanise = opts.humanise === undefined ? 1 : Math.max(0, Math.min(2, opts.humanise));

    // Key
    song.rootPc = (typeof opts.key === 'number' && opts.key >= 0) ? opts.key : rng.int(12);
    song.scaleId = opts.scale && T.SCALES[opts.scale] ? opts.scale : pickScale(rng, genre, mood);
    song.scaleSteps = T.SCALES[song.scaleId].steps;
    song.keyName = T.NOTE_NAMES[song.rootPc] + ' ' + T.SCALES[song.scaleId].name;

    // Length → bar count, rounded to whole 4-bar blocks.
    const targetSec = LENGTHS[opts.length] || LENGTHS.medium;
    const barsPerSec = song.bpm / 60 / song.beatsPerBar;
    let bars = Math.round(targetSec * barsPerSec / 4) * 4;
    bars = Math.max(24, Math.min(112, bars));

    song.sections = planStructure(rng, bars);
    planKeyChange(rng, song, genre);
    song.bars = song.sections.reduce(function (a, s) { return a + s.bars; }, 0);
    song.totalBeats = song.bars * song.beatsPerBar;
    song.duration = song.totalBeats * (60 / song.bpm);

    /* How often the harmony turns over. Zero means the style decides. Slow
       chords feel grand and fast ones feel busy, and it is the single biggest
       lever on whether a track sounds patient or restless. */
    song.barsPerChord = opts.barsPerChord > 0 ? opts.barsPerChord : 0;

    /* Draw a different instrument for some parts each time. The genre still
       decides the style; this decides which of its instruments turn up, so two
       songs in the same style are not the same four sounds twice. */
    song.presetOverride = {};

    /* Effects start out doing nothing. A song you have just generated should
       sound the way the style intends; automation is something you add. */
    song.automation = { filter: [], volume: [] };
    song.pingpong = !!genre.fx.pingpong;
    ['bass', 'chords', 'arp', 'lead', 'pad', 'counter'].forEach(function (part) {
      const cfg = genre[part];
      if (cfg && cfg.alts && cfg.alts.length && rng.chance(0.55)) {
        song.presetOverride[part] = rng.pick(cfg.alts);
      }
    });

    assignParts(song, rng);
    buildHarmony(rng, song, genre, mood);

    song.partSeeds = {
      drums: seed + ':drums', bass: seed + ':bass', chords: seed + ':chords',
      arp: seed + ':arp', lead: seed + ':lead', pad: seed + ':pad',
      counter: seed + ':counter'
    };

    song.tracks = {};
    // Counter last: it answers the lead, so the lead has to exist first.
    ['drums', 'bass', 'chords', 'arp', 'lead', 'pad', 'counter'].forEach(function (p) {
      song.tracks[p] = generatePart(song, p, song.partSeeds[p]);
    });

    return song;
  }

  const PART_FN = {
    drums: composeDrums, bass: composeBass, chords: composeChords,
    arp: composeArp, lead: composeLead, pad: composePad, counter: composeCounter
  };

  function generatePart(song, part, seed, opts) {
    const rng = new T.Rng(seed);
    return PART_FN[part](song, rng, opts);
  }

  /** Re-roll a single part with a fresh sub-seed, leaving the rest of the song intact. */
  function rerollPart(song, part, opts) {
    const nonce = Math.floor(Math.random() * 1e6);
    song.partSeeds[part] = song.seed + ':' + part + ':' + nonce;
    song.tracks[part] = generatePart(song, part, song.partSeeds[part], opts);
    return song.tracks[part];
  }

  /**
   * Develop what the user drew: read a motif out of the part's current notes,
   * then rewrite the part across the whole song from that idea. Returns false
   * when there is not enough drawn to learn from.
   */
  function developPart(song, part) {
    const motif = motifFromEvents(song, song.tracks[part] || []);
    if (!motif) return false;
    /* Asking for an idea to be developed is asking for it to carry the song, so
       let the part play wherever the arrangement can hold it. Without this the
       motif only lands in sections that already happened to have this part, and
       a good idea can end up appearing twice. The intro and outro stay sparse. */
    for (let i = 0; i < song.sections.length; i++) {
      const sec = song.sections[i];
      if (sec.parts && sec.energy >= 0.5) sec.parts[part] = true;
    }
    rerollPart(song, part, { motif: motif });
    return true;
  }

  global.Composer = {
    compose: compose,
    rerollPart: rerollPart,
    developPart: developPart,
    transpose: transpose,
    setTempo: setTempo,
    setChordDegree: setChordDegree,
    duplicateSection: duplicateSection,
    deleteSection: deleteSection,
    moveSection: moveSection,
    extractBlocks: extractBlocks,
    generatePart: generatePart,
    motifFromEvents: motifFromEvents,
    pitchToDegree: pitchToDegree,
    chordAt: chordAt,
    sectionOf: sectionOf,
    swingTime: swingTime,
    feelOf: feelOf,
    GROOVES: GROOVES,
    keyRootAt: keyRootAt,
    adjustDensity: adjustDensity,
    shiftOctave: shiftOctave,
    packSong: packSong,
    unpackSong: unpackSong,
    SAVE_VERSION: SAVE_VERSION,
    ensureAutomation: ensureAutomation,
    clampAutomation: clampAutomation,
    addPoint: addPoint,
    removePoint: removePoint,
    clearLane: clearLane,
    tidyLane: tidyLane,
    applyShape: applyShape,
    SHAPES: SHAPES,
    LANE_NAMES: LANE_NAMES,
    BEATS_PER_BAR: BEATS_PER_BAR,
    METERS: METERS,
    meterOf: meterOf,
    beatsPerBar: bpb,
    LENGTHS: LENGTHS
  };
})(window);
