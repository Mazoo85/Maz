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
  const BEATS_PER_BAR = 4;
  const STEPS_PER_BAR = 16;          // sixteenth-note grid
  const STEP_BEATS = BEATS_PER_BAR / STEPS_PER_BAR;

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

  function buildHarmony(rng, song, genre, mood) {
    const scaleSteps = song.scaleSteps;
    const rootMidi = T.midi(song.rootPc, 4);        // chord construction octave
    const progression = rng.pick(genre.progressions);
    const bridgeProg = rng.pick(genre.progressions);
    const shapeMain = rng.weighted(genre.chordShapes);

    const timeline = [];
    let prevVoicing = null;

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      let prog = sec.type === 'bridge' ? bridgeProg : progression;
      // Open and close on the tonic so the song feels anchored, even when the
      // progression itself starts somewhere else (a ii-V-I, say).
      if (sec.type === 'intro' || sec.type === 'outro') {
        const tonicAt = prog.indexOf(0);
        if (tonicAt > 0) prog = prog.slice(tonicAt).concat(prog.slice(0, tonicAt));
      }
      const barsPerChord = sec.type === 'intro' || sec.type === 'outro' || sec.type === 'ambientish'
        ? Math.max(genre.barsPerChord[0], 2)
        : rng.chance(0.5) ? genre.barsPerChord[0] : genre.barsPerChord[1];

      sec.chords = [];
      let bar = sec.startBar;
      let step = 0;
      while (bar < sec.startBar + sec.bars) {
        const degree = prog[step % prog.length];
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
          const p = T.sweetenChord(T.buildChord(scaleSteps, rootMidi, degree, candidates[ci]));
          if (T.chordIsSound(p)) { pitches = p; shape = candidates[ci]; break; }
        }
        if (!pitches) {
          const r = T.degreePitch(scaleSteps, rootMidi, degree);
          pitches = [r, r + 7];           // last resort: a bare fifth always works
          shape = 'power';
        }
        const voicing = T.voiceChord(pitches, prevVoicing, genre.chords.octaveLow, genre.chords.octaveHigh);
        prevVoicing = voicing;

        const bars = Math.min(barsPerChord, sec.startBar + sec.bars - bar);
        const chord = {
          startBeat: bar * BEATS_PER_BAR,
          durBeats: bars * BEATS_PER_BAR,
          bar: bar,
          bars: bars,
          degree: degree,
          shape: shape,
          pitches: pitches,
          voicing: voicing,
          rootPitch: pitches[0],
          name: T.chordName(pitches),
          roman: T.romanNumeral(scaleSteps, degree, pitches),
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

  function applyFeel(events, song, rng, amount) {
    const swing = song.swing;
    for (let i = 0; i < events.length; i++) {
      const e = events[i];
      if (swing > 0) {
        const eighth = Math.round(e.t / 0.5);
        if (Math.abs(e.t - eighth * 0.5) < 1e-6 && eighth % 2 === 1) e.t += swing * 0.5;
      }
      if (amount > 0) {
        e.t += (rng.next() - 0.5) * 0.02 * amount;
        e.v = Math.max(0.08, Math.min(1, e.v + (rng.next() - 0.5) * 0.12 * amount));
      }
      if (e.t < 0) e.t = 0;
    }
    events.sort(function (a, b) { return a.t - b.t; });
    return events;
  }

  /* ------------------------------------------------------------------ *
   * Drums
   * ------------------------------------------------------------------ */

  const VEL_CHAR = { X: 1.0, x: 0.85, o: 0.45 };

  function patternForEnergy(drums, energy) {
    if (energy <= 0.4) return drums.intro || drums.groove || {};
    if (energy >= 0.95) return drums.full || drums.groove || {};
    return drums.groove || {};
  }

  function composeDrums(song, rng) {
    const genre = song.genre;
    const events = [];
    const drums = genre.drums;

    for (let s = 0; s < song.sections.length; s++) {
      const sec = song.sections[s];
      const base = patternForEnergy(drums, sec.energy);
      const isLast = s === song.sections.length - 1;

      for (let b = 0; b < sec.bars; b++) {
        const bar = sec.startBar + b;
        const barBeat = bar * BEATS_PER_BAR;
        const lastBarOfSection = b === sec.bars - 1;
        const fillBar = !isLast && lastBarOfSection && sec.bars >= 4 && rng.chance(0.85);
        const pattern = fillBar && drums.fill ? drums.fill : base;

        Object.keys(pattern).forEach(function (inst) {
          const row = pattern[inst];
          for (let i = 0; i < row.length; i++) {
            const c = row.charAt(i);
            if (c === '.') continue;
            let vel = VEL_CHAR[c] || 0.7;

            // Drop the odd hit in sparse sections so it breathes.
            if (sec.energy < 0.5 && vel < 0.6 && rng.chance(0.35)) continue;
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
    const pc = ((chord.rootPitch % 12) + 12) % 12;
    return T.midi(pc, octave);
  }

  function composeBass(song, rng) {
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
        const barBeat = bar * BEATS_PER_BAR;
        const chord = chordAt(song, barBeat);
        const root = bassPitch(chord, oct);
        const fifth = root + 7;
        const octaveUp = root + 12;
        const vel = 0.62 + energy * 0.25;

        if (style === 'root8') {
          for (let i = 0; i < 8; i++) {
            const t = barBeat + i * 0.5;
            let p = root;
            if (i === 7 && rng.chance(0.3)) p = fifth;
            if (i % 4 === 2 && rng.chance(0.18)) p = octaveUp;
            events.push({ t: t, d: 0.45, p: p, v: vel * (i % 2 === 0 ? 1 : 0.82) });
          }
        } else if (style === 'pulse8') {
          for (let i = 0; i < 8; i++) {
            const p = i % 4 === 3 ? octaveUp : root;
            events.push({ t: barBeat + i * 0.5, d: 0.42, p: p, v: vel * (i % 2 === 0 ? 1 : 0.8) });
          }
        } else if (style === 'offbeat') {
          events.push({ t: barBeat, d: 0.4, p: root, v: vel });
          for (let i = 0; i < 4; i++) {
            const t = barBeat + i + 0.5;
            let p = root;
            if (rng.chance(0.22)) p = fifth;
            if (rng.chance(0.12)) p = octaveUp;
            events.push({ t: t, d: 0.42, p: p, v: vel * 0.95 });
          }
        } else if (style === 'walk') {
          events.push({ t: barBeat, d: 1.4, p: root, v: vel });
          if (rng.chance(0.7)) events.push({ t: barBeat + 1.5, d: 0.5, p: root + (rng.chance(0.5) ? 7 : 12), v: vel * 0.75 });
          events.push({ t: barBeat + 2, d: 1.0, p: rng.chance(0.6) ? root : fifth, v: vel * 0.9 });
          if (rng.chance(0.45)) {
            const next = chordAt(song, barBeat + BEATS_PER_BAR);
            const target = bassPitch(next, oct);
            const approach = target + (rng.chance(0.5) ? -1 : 1);
            events.push({ t: barBeat + 3.5, d: 0.5, p: approach, v: vel * 0.7 });
          }
        } else if (style === 'whole') {
          if (barBeat === chord.startBeat) {
            events.push({ t: barBeat, d: chord.durBeats, p: root, v: vel * 0.9 });
          }
        } else if (style === 'sustain') {
          if (b % 2 === 0) {
            events.push({ t: barBeat, d: 3.5, p: root, v: vel });
            if (rng.chance(0.5)) events.push({ t: barBeat + 3.75, d: 0.25, p: root + 12, v: vel * 0.7 });
          } else if (rng.chance(0.6)) {
            events.push({ t: barBeat + 1.5, d: 2.0, p: rng.chance(0.4) ? fifth : root, v: vel * 0.85 });
          }
        } else if (style === 'slide808') {
          events.push({ t: barBeat, d: rng.chance(0.5) ? 2.5 : 1.75, p: root, v: vel, glide: b > 0 });
          if (rng.chance(0.6)) events.push({ t: barBeat + 2.5, d: 1.0, p: root, v: vel * 0.85 });
          if (energy > 0.8 && rng.chance(0.35)) events.push({ t: barBeat + 3.5, d: 0.5, p: root + (rng.chance(0.5) ? 7 : 12), v: vel * 0.8, glide: true });
        }
      }
    }
    return applyFeel(events, song, rng, 0.4);
  }

  /* ------------------------------------------------------------------ *
   * Chords / keys
   * ------------------------------------------------------------------ */

  function composeChords(song, rng) {
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
          const barBeat = chord.startBeat + bar * BEATS_PER_BAR;
          const hits = sec.energy >= 0.95 ? [0.5, 1.5, 2.5, 3.5] : [0.5, 2.5];
          for (let h = 0; h < hits.length; h++) {
            if (rng.chance(0.18)) continue;
            for (let i = 0; i < voicing.length; i++) {
              events.push({ t: barBeat + hits[h], d: 0.4, p: voicing[i], v: vel * 0.9 });
            }
          }
        }
      } else if (style === 'keys') {
        for (let bar = 0; bar < chord.bars; bar++) {
          const barBeat = chord.startBeat + bar * BEATS_PER_BAR;
          const hits = [0];
          if (rng.chance(0.75)) hits.push(rng.pick([1.5, 2.5, 2.75]));
          if (rng.chance(0.35)) hits.push(3.5);
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

  function composeArp(song, rng) {
    const genre = song.genre;
    const rate = genre.arp.rate;
    const octave = genre.arp.octave;
    const events = [];
    const shape = rng.pick(['up', 'up', 'updown', 'down', 'upoct']);

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

  function makeMotif(rng, density, bars) {
    const notes = [];
    for (let b = 0; b < bars; b++) {
      const cell = pickCell(rng, density);
      for (let i = 0; i < cell.length; i++) {
        notes.push({ step: b * STEPS_PER_BAR + cell[i] });
      }
    }
    // Durations run to the next onset, capped so phrases stay articulate.
    for (let i = 0; i < notes.length; i++) {
      const next = i + 1 < notes.length ? notes[i + 1].step : bars * STEPS_PER_BAR;
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

  function composeLead(song, rng) {
    const genre = song.genre;
    const octave = genre.lead.octave;
    const density = Math.max(0.15, Math.min(0.95, genre.lead.density + song.mood.density));
    const events = [];
    const motifBars = 2;

    const motifs = {
      verse: makeMotif(rng, density * 0.85, motifBars),
      chorus: makeMotif(rng, density, motifBars),
      bridge: makeMotif(rng, density * 0.7, motifBars)
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

        const phraseBeat = (sec.startBar + ph * motifBars) * BEATS_PER_BAR;

        for (let i = 0; i < motif.length; i++) {
          const n = motif[i];
          const t = phraseBeat + n.step * STEP_BEATS;
          if (t >= song.totalBeats) continue;
          if (rng.chance(genre.lead.restBias * 0.35)) continue;   // leave some air

          const chord = chordAt(song, t);
          const center = T.midi(((chord.rootPitch % 12) + 12) % 12, octave);
          let pitch = T.degreePitch(song.scaleSteps, center, n.contour);
          const strong = n.step % 4 === 0;
          pitch = strong
            ? T.nearestChordTone(pitch, chord.pitches)
            : T.snapToScale(pitch, song.scaleSteps, T.midi(song.rootPc, octave));

          // Keep the melody in a singable window.
          while (pitch > T.midi(song.rootPc, octave) + 16) pitch -= 12;
          while (pitch < T.midi(song.rootPc, octave) - 8) pitch += 12;

          events.push({
            t: t,
            d: Math.max(0.2, n.dur * STEP_BEATS * 0.95),
            p: pitch,
            v: (0.55 + sec.energy * 0.3) * (strong ? 1 : 0.85)
          });
        }
      }
    }
    return applyFeel(events, song, rng, 0.5);
  }

  /* ------------------------------------------------------------------ *
   * Assembly
   * ------------------------------------------------------------------ */

  function sectionOf(song, beat) {
    const bar = Math.floor(beat / BEATS_PER_BAR);
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
            : rng.chance(0.25)
      };
      if (sec.type === 'intro') { sec.parts.lead = sec.parts.lead && rng.chance(0.4); }
    }
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

    // Tempo
    let bpm = opts.bpm && opts.bpm > 0
      ? opts.bpm
      : Math.round(rng.range(genre.bpm[0], genre.bpm[1]) + mood.bpm);
    song.bpm = Math.max(40, Math.min(200, bpm));
    song.swing = genre.swing;

    // Key
    song.rootPc = (typeof opts.key === 'number' && opts.key >= 0) ? opts.key : rng.int(12);
    song.scaleId = opts.scale && T.SCALES[opts.scale] ? opts.scale : pickScale(rng, genre, mood);
    song.scaleSteps = T.SCALES[song.scaleId].steps;
    song.keyName = T.NOTE_NAMES[song.rootPc] + ' ' + T.SCALES[song.scaleId].name;

    // Length → bar count, rounded to whole 4-bar blocks.
    const targetSec = LENGTHS[opts.length] || LENGTHS.medium;
    const barsPerSec = song.bpm / 60 / BEATS_PER_BAR;
    let bars = Math.round(targetSec * barsPerSec / 4) * 4;
    bars = Math.max(24, Math.min(112, bars));

    song.sections = planStructure(rng, bars);
    song.bars = song.sections.reduce(function (a, s) { return a + s.bars; }, 0);
    song.totalBeats = song.bars * BEATS_PER_BAR;
    song.duration = song.totalBeats * (60 / song.bpm);

    assignParts(song, rng);
    buildHarmony(rng, song, genre, mood);

    song.partSeeds = {
      drums: seed + ':drums', bass: seed + ':bass', chords: seed + ':chords',
      arp: seed + ':arp', lead: seed + ':lead', pad: seed + ':pad'
    };

    song.tracks = {};
    ['drums', 'bass', 'chords', 'arp', 'lead', 'pad'].forEach(function (p) {
      song.tracks[p] = generatePart(song, p, song.partSeeds[p]);
    });

    return song;
  }

  const PART_FN = {
    drums: composeDrums, bass: composeBass, chords: composeChords,
    arp: composeArp, lead: composeLead, pad: composePad
  };

  function generatePart(song, part, seed) {
    const rng = new T.Rng(seed);
    return PART_FN[part](song, rng);
  }

  /** Re-roll a single part with a fresh sub-seed, leaving the rest of the song intact. */
  function rerollPart(song, part) {
    const nonce = Math.floor(Math.random() * 1e6);
    song.partSeeds[part] = song.seed + ':' + part + ':' + nonce;
    song.tracks[part] = generatePart(song, part, song.partSeeds[part]);
    return song.tracks[part];
  }

  global.Composer = {
    compose: compose,
    rerollPart: rerollPart,
    generatePart: generatePart,
    chordAt: chordAt,
    sectionOf: sectionOf,
    BEATS_PER_BAR: BEATS_PER_BAR,
    LENGTHS: LENGTHS
  };
})(window);
