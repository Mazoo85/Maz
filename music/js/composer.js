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

      const next = song.sections[s + 1];
      // A lift into a bigger section is worth announcing.
      const buildsInto = genre.builds && next && next.energy > sec.energy + 0.25;

      for (let b = 0; b < sec.bars; b++) {
        const bar = sec.startBar + b;
        const barBeat = bar * BEATS_PER_BAR;
        const lastBarOfSection = b === sec.bars - 1;

        /* The bar before a chorus: pull the kit out from under the track, run a
           snare roll that tightens as it climbs, and sweep a riser over the top.
           Taking things away is what makes the next bar land. */
        if (buildsInto && lastBarOfSection && sec.bars >= 4) {
          events.push({ t: barBeat, d: 0.25, p: 60, v: 0.95, inst: 'kick' });
          const hits = rng.chance(0.5) ? 16 : 8;
          const step = BEATS_PER_BAR / hits;
          for (let i = 0; i < hits; i++) {
            events.push({
              t: barBeat + i * step,
              d: Math.min(0.2, step * 0.8),
              p: 60,
              v: 0.3 + 0.6 * (i / (hits - 1)),
              inst: 'snare'
            });
          }
          events.push({ t: barBeat, d: BEATS_PER_BAR, p: 60, v: 0.75, inst: 'riser' });
          continue;
        }

        // The downbeat it builds to.
        if (b === 0 && genre.builds && sec.energy >= 0.95 && s > 0 &&
            song.sections[s - 1].energy < sec.energy - 0.25) {
          events.push({ t: barBeat, d: 2, p: 60, v: 0.85, inst: 'impact' });
        }

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

  /**
   * Let a phrase land. A melody that never stops for breath reads as a stream
   * of notes rather than a line: this clears the last beat, then leans the
   * final note onto a chord tone and holds it, which is what a cadence is.
   */
  function cadence(song, phraseEvents, endBeat) {
    if (!phraseEvents.length) return phraseEvents;
    const breath = endBeat - 0.75;
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
    const genre = song.genre;
    const octave = genre.lead.octave;
    const density = Math.max(0.15, Math.min(0.95, genre.lead.density + song.mood.density));
    const events = [];
    const motifBars = 2;

    // A motif handed in came from notes the user drew: develop that idea
    // everywhere instead of inventing three of our own.
    const given = opts && opts.motif;
    const motifs = given ? {
      verse: given,
      chorus: transformMotif(rng, given, 'transpose'),
      bridge: transformMotif(rng, given, 'invert')
    } : {
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
          pitch = strong
            ? T.nearestChordTone(pitch, chord.pitches)
            : T.snapToScale(pitch, song.scaleSteps, T.midi(song.rootPc, octave));

          // Keep the melody in a singable window.
          while (pitch > T.midi(song.rootPc, octave) + 16) pitch -= 12;
          while (pitch < T.midi(song.rootPc, octave) - 8) pitch += 12;

          phraseEvents.push({
            t: t,
            d: Math.max(0.2, n.dur * STEP_BEATS * 0.95),
            p: pitch,
            v: (0.55 + sec.energy * 0.3) * (strong ? 1 : 0.85)
          });
        }

        // Every second phrase closes: four bars of line, then room to breathe.
        const phraseEnd = phraseBeat + motifBars * BEATS_PER_BAR;
        const finished = (ph % 2 === 1) ? cadence(song, phraseEvents, phraseEnd) : phraseEvents;
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
    const span = bars * BEATS_PER_BAR;

    // Take the busiest window of `bars` bars — that is where the idea is.
    let bestStart = 0, bestCount = 0;
    for (let b = 0; b * BEATS_PER_BAR < song.totalBeats; b += bars) {
      const start = b * BEATS_PER_BAR;
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

  function buildChordFor(song, degree, shape) {
    const rootMidi = T.midi(song.rootPc, 4);
    const candidates = [shape || 'triad', 'seventh', 'triad'];
    for (let i = 0; i < candidates.length; i++) {
      const p = T.sweetenChord(T.buildChord(song.scaleSteps, rootMidi, degree, candidates[i]));
      if (T.chordIsSound(p)) return { pitches: p, shape: candidates[i] };
    }
    const r = T.degreePitch(song.scaleSteps, rootMidi, degree);
    return { pitches: [r, r + 7], shape: 'power' };
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

    const built = buildChordFor(song, degree, chord.shape);
    const prev = index > 0 ? song.chords[index - 1].voicing : null;
    const genre = song.genre;
    const voicing = T.voiceChord(built.pitches, prev,
      genre.chords.octaveLow, genre.chords.octaveHigh);

    const oldPitches = chord.pitches.slice();
    const oldVoicing = chord.voicing.slice();
    const oldRoot = chord.rootPitch;

    chord.degree = degree;
    chord.shape = built.shape;
    chord.pitches = built.pitches;
    chord.voicing = voicing;
    chord.rootPitch = built.pitches[0];
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
      e.p = e.p - oldRoot + chord.rootPitch;
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
      const from = sec.startBar * BEATS_PER_BAR;
      const to = from + sec.bars * BEATS_PER_BAR;
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
        type: sec.type, bars: sec.bars, energy: sec.energy,
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
      const offset = bar * BEATS_PER_BAR;
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
        n.bar = Math.round(n.startBeat / BEATS_PER_BAR);
        chords.push(n);
      });
      sections.push({
        type: b.type, bars: b.bars, energy: b.energy,
        parts: b.parts, startBar: bar, chords: []
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
    song.totalBeats = bar * BEATS_PER_BAR;
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
      if (match(song.sections[i], i)) return song.sections[i].startBar * BEATS_PER_BAR;
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
      const end = Math.min(song.totalBeats, BEATS_PER_BAR * (song.sections[0] ? song.sections[0].bars : 4));
      clearLane(song, 'volume');
      addPoint(song, 'volume', 0, 0);
      addPoint(song, 'volume', end, 1);
      return 'volume';
    },
    fadeOut: function (song) {
      const last = song.sections[song.sections.length - 1];
      const start = last ? last.startBar * BEATS_PER_BAR : Math.max(0, song.totalBeats - 16);
      clearLane(song, 'volume');
      addPoint(song, 'volume', start, 1);
      addPoint(song, 'volume', song.totalBeats, 0);
      return 'volume';
    },
    buildToChorus: function (song) {
      /* Build into the first chorus that has room in front of it. A song that
         opens on its chorus — which rearranging can easily produce — has
         nothing to build from, so fall back to the middle of the track. */
      const MIN_RUNWAY = BEATS_PER_BAR * 2;
      const chorus = beatOfSection(song, function (s) {
        return s.type === 'chorus' && s.startBar * BEATS_PER_BAR >= MIN_RUNWAY;
      });
      const target = chorus >= MIN_RUNWAY ? chorus : Math.floor(song.totalBeats / 2);
      const start = Math.max(0, target - BEATS_PER_BAR * 8);
      clearLane(song, 'filter');
      addPoint(song, 'filter', start, 0.22);          // muffled, holding back
      addPoint(song, 'filter', target - 0.01, 1);     // wide open as it lands
      addPoint(song, 'filter', song.totalBeats, 1);
      return 'filter';
    },
    duckTheVerses: function (song) {
      clearLane(song, 'filter');
      song.sections.forEach(function (sec) {
        const from = sec.startBar * BEATS_PER_BAR;
        const to = from + sec.bars * BEATS_PER_BAR;
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

    /* Draw a different instrument for some parts each time. The genre still
       decides the style; this decides which of its instruments turn up, so two
       songs in the same style are not the same four sounds twice. */
    song.presetOverride = {};

    /* Effects start out doing nothing. A song you have just generated should
       sound the way the style intends; automation is something you add. */
    song.automation = { filter: [], volume: [] };
    song.pingpong = !!genre.fx.pingpong;
    ['bass', 'chords', 'arp', 'lead', 'pad'].forEach(function (part) {
      const cfg = genre[part];
      if (cfg && cfg.alts && cfg.alts.length && rng.chance(0.55)) {
        song.presetOverride[part] = rng.pick(cfg.alts);
      }
    });

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
    LENGTHS: LENGTHS
  };
})(window);
