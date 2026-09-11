/*
 * Composition tests — no browser, no audio.
 *
 * Loads the pure logic modules into a sandbox and checks every genre × mood ×
 * length combination: the harmony tiles the timeline with no gaps, no chord
 * contradicts itself, every note is in range, a seed always reproduces the same
 * song, and the MIDI export is a real file.
 *
 *   node music/tests/music-logic.test.js
 */
'use strict';

const fs = require('fs');
const vm = require('vm');
const path = require('path');

const JS_DIR = path.join(__dirname, '..', 'js');

/* A minimal browser-ish sandbox: the modules only need `window` and, for the
 * exporter, a stand-in Blob. */
const sandbox = { console: console };
sandbox.window = sandbox;
sandbox.Blob = class Blob {
  constructor(parts, opts) {
    this.parts = parts;
    this.type = opts && opts.type;
    this.size = parts.reduce((a, p) => a + (p.byteLength || p.length || 0), 0);
  }
};
sandbox.document = {
  createElement: () => ({ style: {}, click() {}, appendChild() {} }),
  body: { appendChild() {}, removeChild() {} }
};
sandbox.URL = { createObjectURL: () => 'blob:test', revokeObjectURL() {} };
sandbox.setTimeout = setTimeout;
vm.createContext(sandbox);

['theory.js', 'genres.js', 'composer.js', 'export.js'].forEach(function (f) {
  vm.runInContext(fs.readFileSync(path.join(JS_DIR, f), 'utf8'), sandbox, { filename: f });
});

const { Theory, Genres, Composer, Exporter } = sandbox;

let failures = 0;
let checks = 0;
function check(cond, msg) {
  checks++;
  if (!cond) { failures++; console.error('  FAIL: ' + msg); }
}

const LENGTHS = ['short', 'medium', 'long'];
let songCount = 0;

Object.keys(Genres.GENRES).forEach(function (gid) {
  Object.keys(Genres.MOODS).forEach(function (mid) {
    LENGTHS.forEach(function (len) {
      const seed = gid + '-' + mid + '-' + len;
      const song = Composer.compose({ seed: seed, genre: gid, mood: mid, length: len });
      songCount++;

      /* --- shape --- */
      check(song.bpm >= 40 && song.bpm <= 200, seed + ': bpm out of range (' + song.bpm + ')');
      check(song.bars >= 24, seed + ': too few bars (' + song.bars + ')');
      check(song.duration > 20, seed + ': too short (' + song.duration + 's)');
      check(song.sections.length >= 3, seed + ': too few sections');
      check(song.sections[0].type === 'intro', seed + ': does not open with an intro');
      check(song.sections[song.sections.length - 1].type === 'outro', seed + ': does not end with an outro');

      /* --- harmony tiles the whole song --- */
      let cursor = 0;
      song.chords.forEach(function (c) {
        check(Math.abs(c.startBeat - cursor) < 1e-6,
          seed + ': chord gap at beat ' + c.startBeat + ' (expected ' + cursor + ')');
        cursor = c.startBeat + c.durBeats;
      });
      check(Math.abs(cursor - song.totalBeats) < 1e-6,
        seed + ': harmony ends at ' + cursor + ', song is ' + song.totalBeats + ' beats');

      /* --- no chord contradicts itself --- */
      song.chords.forEach(function (c) {
        const root = c.pitches[0];
        const set = {};
        c.pitches.forEach(function (x) { set[((x - root) % 12 + 12) % 12] = true; });
        check(!(set[6] && set[7]), seed + ': ' + c.name + ' has both a flat and natural fifth');
        check(!(set[3] && set[4]), seed + ': ' + c.name + ' has both thirds');
        check(!(set[10] && set[11]), seed + ': ' + c.name + ' has both sevenths');
        check(c.voicing.length === c.pitches.length, seed + ': voicing dropped notes from ' + c.name);
        const spread = c.voicing[c.voicing.length - 1] - c.voicing[0];
        check(spread <= 28, seed + ': ' + c.name + ' voiced across ' + spread + ' semitones');
      });

      /* --- every note is playable --- */
      Object.keys(song.tracks).forEach(function (tr) {
        song.tracks[tr].forEach(function (e) {
          check(isFinite(e.t) && e.t >= 0, seed + '/' + tr + ': bad start ' + e.t);
          check(isFinite(e.d) && e.d > 0, seed + '/' + tr + ': bad duration ' + e.d);
          check(isFinite(e.v) && e.v > 0 && e.v <= 1, seed + '/' + tr + ': bad velocity ' + e.v);
          /* No slack. This used to allow four beats of overrun, which is exactly
             how a bassline writing eight eighth-notes into a three-beat bar
             went unnoticed. A note that starts after the song has ended is a
             bug in every meter. */
          check(e.t < song.totalBeats, seed + '/' + tr + ': event past the end (' +
            e.t.toFixed(3) + ' of ' + song.totalBeats + ')');
          if (tr === 'drums') check(typeof e.inst === 'string', seed + '/drums: event has no instrument');
          else check(isFinite(e.p) && e.p >= 12 && e.p <= 108, seed + '/' + tr + ': pitch out of range ' + e.p);
        });
      });

      /* --- the parts that carry a song must exist --- */
      check(song.tracks.bass.length > 0, seed + ': no bass');
      check(song.tracks.chords.length > 0, seed + ': no chords');
      if (gid !== 'ambient') check(song.tracks.drums.length > 0, seed + ': no drums');

      /* --- same seed, same song --- */
      const again = Composer.compose({ seed: seed, genre: gid, mood: mid, length: len });
      check(again.title === song.title && again.bpm === song.bpm && again.rootPc === song.rootPc,
        seed + ': settings are not deterministic');
      check(JSON.stringify(again.tracks.lead) === JSON.stringify(song.tracks.lead),
        seed + ': the melody changed between identical seeds');
      check(JSON.stringify(again.tracks.drums) === JSON.stringify(song.tracks.drums),
        seed + ': the drums changed between identical seeds');

      /* --- MIDI export --- */
      const midi = Exporter.buildMidi(song);
      check(midi.size > 200, seed + ': midi file suspiciously small (' + midi.size + ' bytes)');
      const bytes = midi.parts[0];
      check(bytes[0] === 0x4d && bytes[1] === 0x54 && bytes[2] === 0x68 && bytes[3] === 0x64,
        seed + ': midi does not start with MThd');
    });
  });
});

/* --- builds: the bar before a chorus should empty out, then land --- */
['synthwave', 'house', 'trap', 'dnb'].forEach(function (gid) {
  const song = Composer.compose({ seed: 'BUILD-' + gid, genre: gid, length: 'medium' });
  const drums = song.tracks.drums;
  let checkedOne = false;

  for (let i = 0; i < song.sections.length - 1; i++) {
    const sec = song.sections[i];
    const next = song.sections[i + 1];
    // The real drop is the one into a chorus; that is where an impact belongs.
    if (next.energy < 0.95 || next.energy <= sec.energy + 0.25 || sec.bars < 4) continue;

    const buildBar = sec.startBar + sec.bars - 1;
    /* Events are humanised by a few thousandths of a beat, so a hit written on
       the barline can land just before it. Nudge the window rather than miss it. */
    const from = buildBar * 4 - 0.05, to = from + 4;
    const inBuild = drums.filter(function (e) { return e.t >= from && e.t < to; });
    const kinds = {};
    inBuild.forEach(function (e) { kinds[e.inst] = (kinds[e.inst] || 0) + 1; });

    check(kinds.riser > 0, gid + ': a riser sweeps the bar before ' + next.name);
    check(!kinds.hh && !kinds.oh, gid + ': the kit drops out under the build into ' + next.name +
      ' (found ' + JSON.stringify(kinds) + ')');
    check((kinds.snare || 0) >= 8, gid + ': a snare roll climbs into ' + next.name +
      ' (' + (kinds.snare || 0) + ' hits)');

    const roll = inBuild.filter(function (e) { return e.inst === 'snare'; })
      .sort(function (a, b) { return a.t - b.t; });
    check(roll.length >= 4 && roll[roll.length - 1].v > roll[0].v + 0.2,
      gid + ': and it grows (' + roll[0].v.toFixed(2) + ' → ' + roll[roll.length - 1].v.toFixed(2) + ')');

    const landing = drums.filter(function (e) {
      return e.t >= next.startBar * 4 - 0.05 && e.t < next.startBar * 4 + 1 && e.inst === 'impact';
    });
    check(landing.length > 0, gid + ': the drop lands on an impact at ' + next.name);
    checkedOne = true;
    break;
  }
  check(checkedOne, gid + ': has a build into a chorus to check');
});

/* --- melodies breathe: every second phrase clears its last beat --- */
['synthwave', 'lofi', 'cinematic', 'house'].forEach(function (gid) {
  const song = Composer.compose({ seed: 'PHRASE-' + gid, genre: gid, length: 'medium' });
  const lead = song.tracks.lead;
  if (!lead.length) return;

  let closed = 0, total = 0;
  song.sections.forEach(function (sec) {
    if (!sec.parts.lead) return;
    for (let ph = 1; ph < Math.floor(sec.bars / 2); ph += 2) {
      const end = (sec.startBar + (ph + 1) * 2) * 4;
      total++;
      // Nothing should start in the last three quarters of a closing phrase.
      const inBreath = lead.filter(function (e) { return e.t > end - 0.7 && e.t < end - 0.05; });
      if (!inBreath.length) closed++;
    }
  });
  check(total === 0 || closed / total > 0.8,
    gid + ': closing phrases leave room to breathe (' + closed + '/' + total + ')');
});

/* --- styles that should never do that, do not --- */
['lofi', 'ambient', 'chiptune'].forEach(function (gid) {
  const song = Composer.compose({ seed: 'NOBUILD-' + gid, genre: gid, length: 'medium' });
  const odd = song.tracks.drums.filter(function (e) {
    return e.inst === 'riser' || e.inst === 'impact';
  });
  check(odd.length === 0, gid + ': no risers or impacts, which would be absurd here');
});

/* --- transpose: same song, different key --- */
(function () {
  const song = Composer.compose({ seed: 'KEY-1', genre: 'synthwave', mood: 'driving' });
  const beforeRoot = song.rootPc;
  const beforeLead = song.tracks.lead.map(function (e) { return e.p; });
  const beforeTimes = song.tracks.lead.map(function (e) { return e.t; });
  const beforeChordName = song.chords[0].name;

  check(Composer.transpose(song, 3) !== null, 'transpose up a minor third is accepted');
  check(song.rootPc === (beforeRoot + 3) % 12, 'the key moves (' + beforeRoot + ' → ' + song.rootPc + ')');
  check(song.tracks.lead.every(function (e, i) { return e.p === beforeLead[i] + 3; }),
    'every melody note moves by exactly the same amount');
  check(song.tracks.lead.every(function (e, i) { return e.t === beforeTimes[i]; }),
    'and nothing moves in time');
  check(song.chords[0].name !== beforeChordName, 'chord names follow the key (' +
    beforeChordName + ' → ' + song.chords[0].name + ')');

  // Intervals are the whole point: a transposed song must be the same music.
  const gaps = song.tracks.lead.map(function (e, i) {
    return i ? e.p - song.tracks.lead[i - 1].p : 0;
  });
  const wasGaps = beforeLead.map(function (p, i) { return i ? p - beforeLead[i - 1] : 0; });
  check(JSON.stringify(gaps) === JSON.stringify(wasGaps), 'the intervals are untouched');

  // And it refuses to run a part off the end of the keyboard.
  const extreme = Composer.compose({ seed: 'KEY-2', genre: 'trap' });
  check(Composer.transpose(extreme, 60) === null, 'an absurd transpose is refused, not clipped');
})();

/* --- tempo: retimed, not rewritten --- */
(function () {
  const song = Composer.compose({ seed: 'TEMPO-1', genre: 'house' });
  const beats = JSON.stringify(song.tracks.drums.map(function (e) { return e.t; }));
  const wasDuration = song.duration;
  const wasBpm = song.bpm;

  Composer.setTempo(song, 90);
  check(JSON.stringify(song.tracks.drums.map(function (e) { return e.t; })) === beats,
    'changing tempo does not touch a single note');
  check(song.bpm === 90, 'the tempo is what we asked for');
  check(Math.abs(song.duration - song.totalBeats * 60 / 90) < 0.01,
    'and the length follows from it (' + wasDuration.toFixed(0) + 's at ' + wasBpm +
    ' → ' + song.duration.toFixed(0) + 's at 90)');
  check(song.duration > wasDuration, 'slower really is longer');

  // Both ends are clamped to something a person can actually play.
  Composer.setTempo(song, 5);
  check(song.bpm === 40, 'a silly slow tempo is clamped (' + song.bpm + ')');
  Composer.setTempo(song, 9000);
  check(song.bpm === 220, 'a silly fast one too (' + song.bpm + ')');
})();

/* --- changing a chord: harmony moves, rhythm does not --- */
(function () {
  const song = Composer.compose({ seed: 'CHORD-1', genre: 'lofi', length: 'medium' });
  const idx = 2;
  const chord = song.chords[idx];
  const from = chord.startBeat - 0.05, to = chord.startBeat + chord.durBeats - 0.05;
  function span(t) {
    return song.tracks[t].filter(function (e) { return e.t >= from && e.t < to; });
  }
  const beforeName = chord.name;
  const beforeTimes = {};
  const beforeCounts = {};
  ['chords', 'bass', 'arp', 'lead', 'pad'].forEach(function (t) {
    beforeTimes[t] = span(t).map(function (e) { return e.t + ':' + e.d; }).join('|');
    beforeCounts[t] = span(t).length;
  });
  const bassIntervals = span('bass').map(function (e) { return e.p - chord.rootPitch; });
  const outsideBefore = song.tracks.lead
    .filter(function (e) { return e.t < from; })
    .map(function (e) { return e.p; }).join(',');

  const newDegree = (chord.degree + 3) % 7;
  check(Composer.setChordDegree(song, idx, newDegree), 'a chord can be changed');
  check(song.chords[idx].name !== beforeName,
    'the chord itself changes (' + beforeName + ' → ' + song.chords[idx].name + ')');
  check(song.chords[idx].degree === newDegree, 'to the degree we asked for');

  ['chords', 'bass', 'arp', 'lead', 'pad'].forEach(function (t) {
    check(span(t).length === beforeCounts[t], t + ': no notes gained or lost');
    check(span(t).map(function (e) { return e.t + ':' + e.d; }).join('|') === beforeTimes[t],
      t + ': every rhythm is untouched');
  });

  const afterIntervals = span('bass').map(function (e) { return e.p - song.chords[idx].rootPitch; });
  check(JSON.stringify(afterIntervals) === JSON.stringify(bassIntervals),
    'the bass keeps its role against the new root');

  check(song.tracks.lead.filter(function (e) { return e.t < from; })
    .map(function (e) { return e.p; }).join(',') === outsideBefore,
    'and nothing outside the chord moves at all');

  // Whatever it lands on has to be a real chord.
  const c = song.chords[idx];
  const set = {};
  c.pitches.forEach(function (x) { set[((x - c.pitches[0]) % 12 + 12) % 12] = true; });
  check(!(set[6] && set[7]) && !(set[3] && set[4]), 'the new chord does not contradict itself');
  check(Composer.setChordDegree(song, 999, 0) === false, 'a chord that is not there is refused');
})();

/* --- arranging: sections move as whole blocks, nothing is lost --- */
(function () {
  function totalNotes(song) {
    return Object.keys(song.tracks).reduce(function (a, t) { return a + song.tracks[t].length; }, 0);
  }
  function tiles(song, label) {
    // Every section must butt up against the next with no gap or overlap.
    let bar = 0, ok = true;
    song.sections.forEach(function (sec) {
      if (sec.startBar !== bar) ok = false;
      bar += sec.bars;
    });
    check(ok, label + ': sections tile the song with no gaps');
    check(bar === song.bars, label + ': the bar count matches the sections (' + bar + ' vs ' + song.bars + ')');
    check(Math.abs(song.totalBeats - song.bars * song.beatsPerBar) < 1e-6,
      label + ': beats match bars');
    let inRange = true;
    Object.keys(song.tracks).forEach(function (t) {
      song.tracks[t].forEach(function (e) { if (e.t < -0.1 || e.t >= song.totalBeats) inRange = false; });
    });
    check(inRange, label + ': every note lands inside the song');
    let chordsTile = true, cursor = 0;
    song.chords.forEach(function (c) {
      if (Math.abs(c.startBeat - cursor) > 0.001) chordsTile = false;
      cursor = c.startBeat + c.durBeats;
    });
    check(chordsTile && Math.abs(cursor - song.totalBeats) < 0.001,
      label + ': the harmony still tiles the whole song');
  }

  const song = Composer.compose({ seed: 'ARRANGE-1', genre: 'synthwave', length: 'medium' });
  const startSections = song.sections.length;
  const startBars = song.bars;
  const startNotes = totalNotes(song);
  tiles(song, 'as composed');

  // Duplicate a chorus.
  const chorusAt = song.sections.findIndex(function (s2) { return s2.type === 'chorus'; });
  const chorusBars = song.sections[chorusAt].bars;
  const chorusNotes = Object.keys(song.tracks).reduce(function (a, t) {
    const from = song.sections[chorusAt].startBar * 4;
    return a + song.tracks[t].filter(function (e) {
      return e.t >= from - 0.05 && e.t < from + chorusBars * 4 - 0.05;
    }).length;
  }, 0);
  check(Composer.duplicateSection(song, chorusAt), 'a chorus can be duplicated');
  check(song.sections.length === startSections + 1, 'the song gains a section');
  check(song.bars === startBars + chorusBars, 'and gains its bars (' + startBars + ' → ' + song.bars + ')');
  check(totalNotes(song) === startNotes + chorusNotes,
    'the copy brings its notes with it (+' + chorusNotes + ')');
  check(song.sections[chorusAt + 1].type === 'chorus', 'the copy lands right after the original');
  tiles(song, 'after duplicating');

  // Delete it again and we should be back where we started.
  check(Composer.deleteSection(song, chorusAt + 1), 'and it can be deleted again');
  check(song.bars === startBars, 'back to the original length');
  check(totalNotes(song) === startNotes, 'and the original note count');
  tiles(song, 'after deleting');

  // Reorder.
  const before2 = song.sections.map(function (s2) { return s2.type; }).join(',');
  check(Composer.moveSection(song, 1, 1), 'a section can be moved');
  const after2 = song.sections.map(function (s2) { return s2.type; }).join(',');
  check(before2 !== after2, 'the running order changes (' + before2 + ' → ' + after2 + ')');
  check(totalNotes(song) === startNotes, 'moving loses nothing');
  tiles(song, 'after moving');

  // Guard rails.
  check(Composer.moveSection(song, 0, -1) === false, 'the first section cannot move up');
  check(Composer.moveSection(song, song.sections.length - 1, 1) === false, 'nor the last one down');
  const tiny = Composer.compose({ seed: 'ARRANGE-2', genre: 'lofi', length: 'short' });
  while (tiny.sections.length > 1) Composer.deleteSection(tiny, 0);
  check(Composer.deleteSection(tiny, 0) === false, 'a song cannot be emptied of every section');
  check(tiny.bars > 0, 'and it still has bars left');
})();

/* --- key changes, borrowed chords, inversions and cadences --- */
(function () {
  /* Key change. Gospel modulates often enough to find one quickly; the point
     is that everything moves together, not just the chords. */
  let modulated = null;
  for (let i = 0; i < 60 && !modulated; i++) {
    const s = Composer.compose({ seed: 'MOD-' + i, genre: 'gospel', length: 'medium' });
    if (s.keyChange) modulated = s;
  }
  check(!!modulated, 'a style that modulates produces a key change');

  if (modulated) {
    const at = modulated.keyChange.atBar;
    const shift = modulated.keyChange.semitones;
    check(shift >= 1 && shift <= 3, 'the lift is a step or two (' + shift + ' semitones)');

    // Every section from the change onwards is in the new key, and none before.
    const before = modulated.sections.filter(function (x) { return x.startBar < at; });
    const after = modulated.sections.filter(function (x) { return x.startBar >= at; });
    check(before.every(function (x) { return !x.keyShift; }), 'nothing before it has moved');
    check(after.every(function (x) { return x.keyShift === shift; }), 'everything after it has');

    /* The real test: the chords either side must actually be in different keys.
       Compare the pitch-class sets — a modulation that only changed a label
       would pass a check on the label. */
    const pcsIn = function (secs) {
      const set = {};
      secs.forEach(function (sec) {
        sec.chords.forEach(function (c) {
          c.pitches.forEach(function (p) { set[((p % 12) + 12) % 12] = true; });
        });
      });
      return Object.keys(set).map(Number).sort(function (a, b) { return a - b; });
    };
    const pcBefore = pcsIn(before), pcAfter = pcsIn(after);
    check(pcBefore.join(',') !== pcAfter.join(','),
      'and the notes really do change key (' + pcBefore.join(' ') + ' → ' + pcAfter.join(' ') + ')');

    // The melody has to follow, or it is playing in the old key over the new one.
    const beatAt = at * modulated.beatsPerBar;
    const leadAfter = modulated.tracks.lead.filter(function (e) { return e.t >= beatAt; });
    if (leadAfter.length > 4) {
      const newScale = {};
      modulated.scaleSteps.forEach(function (st) {
        newScale[((modulated.rootPc + shift + st) % 12 + 12) % 12] = true;
      });
      const inKey = leadAfter.filter(function (e) { return newScale[((e.p % 12) + 12) % 12]; });
      check(inKey.length / leadAfter.length > 0.7,
        'and the melody moves with it (' + Math.round(100 * inKey.length / leadAfter.length) +
        '% of the notes after the change are in the new key)');
    }

    // A key change must survive a save and a transposition.
    const back = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(modulated))));
    check(back.sections.map(function (x) { return x.keyShift || 0; }).join(',') ===
          modulated.sections.map(function (x) { return x.keyShift || 0; }).join(','),
      'a key change survives a save');
  }

  /* Borrowed chords. A secondary dominant is a major triad with a flat seventh
     on a root the scale may not even contain — which is the point. */
  const jz = Composer.compose({ seed: 'BORROW-1', genre: 'jazz', length: 'long' });
  const borrowed = jz.chords.filter(function (c) { return c.borrowed; });
  check(borrowed.length > 0, 'a style that borrows produces borrowed chords (' + borrowed.length + ')');
  check(borrowed.every(function (c) {
    const r = c.pitches[0];
    const iv = c.pitches.map(function (p) { return ((p - r) % 12 + 12) % 12; }).sort(function (a, b) { return a - b; });
    return iv.join(',') === '0,4,7,10';
  }), 'and every one of them really is a dominant seventh');
  check(borrowed.every(function (c) { return Theory.chordIsSound(c.pitches); }),
    'and none of them contradicts itself');

  /* Cadences: a section should end where it is going, not wherever the loop
     stopped. Choruses land home; verses lean on the dominant. */
  let homeEndings = 0, chorusCount = 0;
  ['country', 'gospel', 'rock', 'disco'].forEach(function (g) {
    for (let i = 0; i < 6; i++) {
      const s = Composer.compose({ seed: 'CAD-' + g + i, genre: g, length: 'medium' });
      s.sections.forEach(function (sec) {
        if (sec.type !== 'chorus' || !sec.chords.length) return;
        chorusCount++;
        if (sec.chords[sec.chords.length - 1].degree === 0) homeEndings++;
      });
    }
  });
  check(chorusCount > 10, 'there are choruses to check (' + chorusCount + ')');
  check(homeEndings / chorusCount > 0.6,
    'most choruses end on the tonic (' + homeEndings + ' of ' + chorusCount + ')');

  /* Inversions. A slash chord must actually put a chord tone other than the
     root in the bass — and say so in its name. */
  let slash = null;
  for (let i = 0; i < 40 && !slash; i++) {
    const s = Composer.compose({ seed: 'INV-' + i, genre: 'gospel', length: 'medium' });
    slash = s.chords.filter(function (c) {
      return ((c.bassPitch % 12) + 12) % 12 !== ((c.rootPitch % 12) + 12) % 12;
    })[0];
    if (slash) slash._song = s;
  }
  check(!!slash, 'inversions happen');
  if (slash) {
    check(slash.name.indexOf('/') > 0, 'and are named as slash chords (' + slash.name + ')');
    check(slash.pitches.indexOf(slash.bassPitch) >= 0,
      'with a bass note that belongs to the chord');
    const bassPc = ((slash.bassPitch % 12) + 12) % 12;
    const under = slash._song.tracks.bass.filter(function (e) {
      return e.t >= slash.startBeat - 0.05 && e.t < slash.startBeat + slash.durBeats - 0.05;
    });
    check(under.length === 0 || under.every(function (e) {
      // Root, fifth and octave decorations are all built from the bass note.
      const rel = ((e.p - bassPc) % 12 + 12) % 12;
      return rel === 0 || rel === 7;
    }), 'and a bassline that actually plays it');
  }

  /* Chord rate: asking for one change every four bars must produce exactly
     that, not merely fewer changes. */
  [1, 2, 4].forEach(function (rate) {
    const s = Composer.compose({ seed: 'RATE-' + rate, genre: 'lofi', barsPerChord: rate, length: 'medium' });
    const spans = s.chords.map(function (c) { return c.bars; });
    check(spans.every(function (b) { return b === rate || b < rate; }),
      'chords every ' + rate + ' bar(s): nothing lasts longer');
    check(spans.filter(function (b) { return b === rate; }).length > spans.length * 0.7,
      'and almost all of them last exactly that');
  });

  // An explicit scale is honoured.
  const ph = Composer.compose({ seed: 'SCALE-1', genre: 'lofi', scale: 'phrygian' });
  check(ph.scaleId === 'phrygian', 'a requested scale is used');
  check(ph.keyName.indexOf('Phrygian') > 0, 'and named (' + ph.keyName + ')');

  // Re-rolling a part must never rewrite the harmony under it.
  const lock = Composer.compose({ seed: 'LOCK-1', genre: 'jazz', length: 'medium' });
  const chordsBefore = JSON.stringify(lock.chords);
  ['lead', 'bass', 'arp', 'drums', 'pad', 'chords'].forEach(function (t) {
    Composer.rerollPart(lock, t);
  });
  check(JSON.stringify(lock.chords) === chordsBefore,
    're-rolling every part leaves the chord progression exactly as it was');
})();

/* --- time signatures --- */
(function () {
  const meters = Object.keys(Composer.METERS);
  check(meters.length >= 5, 'more than one time signature exists (' + meters.join(' ') + ')');

  meters.forEach(function (m) {
    const info = Composer.METERS[m];
    /* Rock, deliberately: the backbeat check below needs a style that has one.
       Cinematic is kicks and toms with no snare at all, so measuring it would
       prove nothing about where a backbeat lands. */
    const song = Composer.compose({ seed: 'METER-' + m, genre: 'rock', meter: m, length: 'medium' });

    check(song.meter === m, m + ': the requested meter is the one used');
    check(song.beatsPerBar === info.beats, m + ': a bar is ' + info.beats + ' beats');
    check(Math.abs(song.totalBeats - song.bars * info.beats) < 1e-9,
      m + ': the song is a whole number of bars (' + song.bars + ' × ' + info.beats + ')');

    // Bars must tile the song with no gap and no overlap, whatever their length.
    let expected = 0, gapless = true;
    song.sections.forEach(function (sec) {
      if (sec.startBar !== expected) gapless = false;
      expected += sec.bars;
    });
    check(gapless && expected === song.bars, m + ': the sections tile the song exactly');

    // Every chord must start on a bar line and stop inside the song.
    const chordsAligned = song.chords.every(function (c) {
      return Math.abs(c.startBeat - c.bar * info.beats) < 1e-6 &&
             c.startBeat + c.durBeats <= song.totalBeats + 1e-6;
    });
    check(chordsAligned, m + ': every chord sits on a bar line');

    // Nothing may be written past the end of the last bar.
    let inRange = true, notes = 0;
    Object.keys(song.tracks).forEach(function (k) {
      song.tracks[k].forEach(function (e) {
        notes++;
        if (e.t < -0.06 || e.t >= song.totalBeats) inRange = false;
      });
    });
    check(notes > 50, m + ': the song has parts in it (' + notes + ' notes)');
    check(inRange, m + ': nothing is written past the final bar');

    /* The part that makes a meter a meter: where the weight falls. A backbeat
       instrument must land on this meter's backbeats and nowhere a 4/4 pattern
       would have put it by accident.
       Measured in a loud section, not in bar 0 — most styles write no snare at
       all in an intro, and a check with nothing to look at passes without
       proving anything. */
    const loud = song.sections.filter(function (x) { return x.energy >= 0.65; })[0];
    check(!!loud, m + ': the song has a full-strength section to measure');
    if (loud) {
      const from = loud.startBar * info.beats;
      const bar = song.tracks.drums.filter(function (e) {
        return e.t >= from - 1e-6 && e.t < from + info.beats - 1e-6;
      });
      const backbeat = bar.filter(function (e) {
        return (e.inst === 'snare' || e.inst === 'clap' || e.inst === 'rim') && e.v >= 0.6;
      });
      check(backbeat.length > 0, m + ': and that section has a backbeat to check');
      const wanted = info.backbeats.map(function (st) { return st * 0.25; });
      const onGrid = backbeat.every(function (e) {
        return wanted.some(function (w) { return Math.abs(e.t - from - w) < 0.12; });
      });
      check(onGrid, m + ': the backbeat lands where this meter puts it (beat ' +
        info.backbeats.map(function (st) { return st / 4 + 1; }).join(' and ') + ')');
    }
  });

  // A meter survives a save, because the bar length is not something to guess.
  const w = Composer.compose({ seed: 'METER-SAVE', genre: 'country', meter: '3/4' });
  const back = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(w))));
  check(back.meter === '3/4' && back.beatsPerBar === 3, 'a time signature survives a save');
  check(back.bars === w.bars && back.totalBeats === w.totalBeats,
    'and the song is still the same length afterwards');

  // Rearranging a waltz keeps it a waltz.
  Composer.duplicateSection(w, 1);
  check(Math.abs(w.totalBeats - w.bars * 3) < 1e-9,
    'rearranging keeps the bars three beats long');

  // An unknown meter falls back rather than producing nonsense.
  const odd = Composer.compose({ seed: 'METER-BAD', genre: 'lofi', meter: '13/16' });
  check(Composer.METERS[odd.meter] !== undefined, 'an unknown time signature falls back to a real one');

  // Every genre must be able to produce every meter it lists.
  Object.keys(Genres.GENRES).forEach(function (g) {
    const listed = (Genres.GENRES[g].meters || []).map(function (x) { return x[0]; });
    check(listed.length > 0, g + ': lists at least one time signature');
    check(listed.every(function (m) { return !!Composer.METERS[m]; }),
      g + ': lists only time signatures that exist (' + listed.join(' ') + ')');
  });
})();

/* --- saving and reloading a whole song --- */
(function () {
  /* The point of a save is that what comes back is what you had — not what the
     generator would write again from the same seed. So every check here is
     made after deliberately editing the song away from its generated form. */
  const a = Composer.compose({ seed: 'SAVE-1', genre: 'lofi', length: 'medium' });

  a.tracks.lead.push({ t: 4.5, d: 1, p: 72, v: 0.8 });          // a hand-drawn note
  a.tracks.drums.push({ t: 2.25, d: 0.25, p: 60, v: 0.9, inst: 'cowbell' });
  Composer.setChordDegree(a, 2, 3);                              // a swapped chord
  Composer.duplicateSection(a, 1);                               // a rearrangement
  Composer.applyShape(a, 'fadeOut');                             // automation
  Composer.transpose(a, 2);                                      // a key change
  a.presetOverride = { lead: 'bell', pad: 'glassPad' };
  a.pingpong = true;

  const wire = JSON.stringify(Composer.packSong(a, { mix: { lead: { volume: 0.5 } } }));
  const b = Composer.unpackSong(JSON.parse(wire));

  check(!!b, 'a saved song can be read back');
  check(b.title === a.title && b.seed === a.seed, 'it is the same song');
  check(b.bpm === a.bpm && b.rootPc === a.rootPc && b.keyName === a.keyName,
    'tempo and key survive (' + b.keyName + ' ' + b.bpm + ')');
  check(b.scaleId === a.scaleId, 'and the scale with them');
  check(b.bars === a.bars && b.totalBeats === a.totalBeats,
    'the rearranged length survives (' + b.bars + ' bars)');
  check(b.sections.map(function (x) { return x.name; }).join(',') ===
        a.sections.map(function (x) { return x.name; }).join(','),
    'the running order survives');
  check(b.chords.map(function (c) { return c.name; }).join(',') ===
        a.chords.map(function (c) { return c.name; }).join(','),
    'every chord survives, swapped one included');
  check(JSON.stringify(b.automation) === JSON.stringify(a.automation), 'the fade survives');
  check(b.pingpong === true, 'the ping-pong setting survives');
  check(b.presetOverride.lead === 'bell' && b.presetOverride.pad === 'glassPad',
    'the chosen instruments survive');

  // The score itself, note for note.
  let counts = true, drift = 0, pitches = true, insts = true, glides = true;
  Object.keys(a.tracks).forEach(function (k) {
    if (b.tracks[k].length !== a.tracks[k].length) { counts = false; return; }
    a.tracks[k].forEach(function (e, i) {
      const g = b.tracks[k][i];
      drift = Math.max(drift, Math.abs(e.t - g.t), Math.abs(e.d - g.d), Math.abs(e.v - g.v));
      if (k === 'drums') { if (e.inst !== g.inst) insts = false; }
      else if (e.p !== g.p) pitches = false;
      if (!!e.glide !== !!g.glide) glides = false;
    });
  });
  check(counts, 'every part comes back with the same number of notes');
  check(pitches, 'at the same pitches');
  check(insts, 'and the drums land on the same pieces');
  check(glides, 'and the sliding bass notes still slide');
  /* Stored at a ten-thousandth of a beat and a thousandth of a velocity: at
     120 BPM that is half a millisecond, which is inaudible and roughly a
     hundred times finer than the humanising already applied. */
  check(drift <= 0.001, 'and nothing has drifted audibly (worst ' + drift.toFixed(5) + ')');

  /* Find the hand-drawn note by its velocity: the composer humanises every note
     it writes, so a velocity of exactly 0.8 is one nothing but a hand could
     have set. Its pitch and bar have legitimately moved — the song was
     transposed and rearranged after it was drawn — so those are read from the
     edited song rather than from what was typed in. */
  const handA = a.tracks.lead.filter(function (e) { return e.v === 0.8; });
  const handB = b.tracks.lead.filter(function (e) { return Math.abs(e.v - 0.8) < 1e-9; });
  check(handA.length === 1, 'the hand-drawn note is identifiable in the edited song');
  check(handB.length === 1 && handB[0].p === handA[0].p &&
        Math.abs(handB[0].t - handA[0].t) <= 0.0001,
    'and it comes back at exactly the same pitch and beat');
  check(b.tracks.drums.some(function (e) { return e.inst === 'cowbell'; }),
    'and so is the cowbell');

  // The live genre and mood objects have to be rebuilt, not stored.
  check(!!b.genre && !!b.genre.fx && b.genreId === a.genreId, 'the genre is a working object again');
  check(!!b.mood && b.moodId === a.moodId, 'and so is the mood');
  check(wire.indexOf('"scaleSteps"') < 0, 'nothing regenerable is stored');

  // Anything the caller tacks on rides along untouched.
  const withMix = JSON.parse(wire);
  check(withMix.mix.lead.volume === 0.5, 'mixer settings ride along with the song');

  // A save must survive a round trip through storage more than once.
  const c = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(b))));
  check(c && c.bars === a.bars && c.chords.length === a.chords.length,
    'and saving the reloaded song again changes nothing');

  // A save from a future version must be refused, not half-read.
  const future = JSON.parse(wire);
  future.v = Composer.SAVE_VERSION + 1;
  check(Composer.unpackSong(future) === null, 'a save this version cannot read is refused outright');
  check(Composer.unpackSong(null) === null, 'and so is nothing at all');

  // Size matters: this goes into browser storage alongside 29 others.
  check(wire.length < 120000, 'a saved song is small enough to keep (' +
    Math.round(wire.length / 1024) + ' KB)');
})();

/* --- automation lanes --- */
(function () {
  const a = Composer.compose({ seed: 'AUTO-1', genre: 'house', length: 'medium' });
  check(a.automation && Array.isArray(a.automation.filter) && Array.isArray(a.automation.volume),
    'a new song starts with empty automation lanes');
  check(a.automation.filter.length === 0 && a.automation.volume.length === 0,
    'and nothing is moving until you ask for it');

  // Points are sorted and clamped however carelessly they go in.
  Composer.addPoint(a, 'volume', 40, 0.5);
  Composer.addPoint(a, 'volume', 8, 1);
  Composer.addPoint(a, 'volume', -20, 0.25);          // before the start
  Composer.addPoint(a, 'volume', a.totalBeats + 500, 3);  // past the end, over full
  const v = a.automation.volume;
  check(v.length === 4, 'every point is kept (' + v.length + ')');
  let sorted = true;
  for (let i = 1; i < v.length; i++) if (v[i].t < v[i - 1].t) sorted = false;
  check(sorted, 'and they are held in time order');
  check(v[0].t === 0, 'a point before the start is pulled to the start');
  check(v[v.length - 1].t === a.totalBeats, 'and one past the end is pulled to the end');
  check(v.every(function (p) { return p.v >= 0 && p.v <= 1; }), 'values stay between silent and full');

  check(Composer.addPoint(a, 'nonsense', 4, 0.5) === null, 'an unknown lane is refused');

  // A shape lands on the song's own arrangement, not on a guessed bar number.
  const b = Composer.compose({ seed: 'AUTO-2', genre: 'synthwave', length: 'medium' });
  const lane = Composer.applyShape(b, 'buildToChorus');
  check(lane === 'filter', 'the build writes into the filter lane');
  const chorus = b.sections.filter(function (x) { return x.type === 'chorus'; })[0];
  check(!!chorus, 'the test song has a chorus to build into');
  const pts = b.automation.filter;
  check(pts.length >= 3, 'the build has a shape to it (' + pts.length + ' points)');
  const chorusBeat = chorus.startBar * 4;
  const atChorus = pts.filter(function (p) { return Math.abs(p.t - chorusBeat) < 0.5; });
  check(atChorus.length === 1 && atChorus[0].v === 1,
    'and it arrives wide open exactly where the chorus starts');
  check(pts[0].v < 0.5, 'having started held back (' + pts[0].v + ')');

  // A song rearranged to open on its chorus has no room to build into it.
  const opener = Composer.compose({ seed: 'AUTO-5', genre: 'house', length: 'medium' });
  while (opener.sections.length > 1 && opener.sections[0].type !== 'chorus') {
    Composer.deleteSection(opener, 0);
  }
  if (opener.sections[0].type === 'chorus') {
    Composer.applyShape(opener, 'buildToChorus');
    const op = opener.automation.filter;
    check(op.length >= 3, 'a song opening on its chorus still gets a build (' + op.length + ')');
    check(op.every(function (p) { return p.t >= 0 && p.t <= opener.totalBeats; }),
      'and every point of it lands inside the song');
    check(op[0].t < op[op.length - 1].t, 'with somewhere to build from');
  }

  const c = Composer.compose({ seed: 'AUTO-3', genre: 'lofi', length: 'short' });
  Composer.applyShape(c, 'fadeOut');
  const f = c.automation.volume;
  check(f[f.length - 1].v === 0 && f[f.length - 1].t === c.totalBeats,
    'a fade-out reaches silence at the very end');
  check(f[0].v === 1, 'and starts from full');
  Composer.applyShape(c, 'fadeIn');
  check(c.automation.volume[0].v === 0 && c.automation.volume[0].t === 0,
    'a fade-in starts from silence at the very start');

  // Shortening the song must not leave automation hanging past the end.
  const d = Composer.compose({ seed: 'AUTO-4', genre: 'house', length: 'medium' });
  Composer.applyShape(d, 'fadeOut');
  const endBefore = d.totalBeats;
  Composer.deleteSection(d, d.sections.length - 1);
  check(d.totalBeats < endBefore, 'deleting a section shortened the song');
  check(d.automation.volume.every(function (p) { return p.t <= d.totalBeats; }),
    'and no automation point is left stranded past the new end');

  check(Composer.clearLane(d, 'volume') && d.automation.volume.length === 0,
    'clearing a lane empties it');
  check(d.automation.filter !== undefined, 'without disturbing the other one');
})();

/* --- re-rolling one part leaves the others alone --- */
const s = Composer.compose({ seed: 'REROLL-1', genre: 'synthwave', mood: 'driving' });
const beforeLead = JSON.stringify(s.tracks.lead);
const beforeBass = JSON.stringify(s.tracks.bass);
const beforeChords = JSON.stringify(s.chords);
Composer.rerollPart(s, 'lead');
check(JSON.stringify(s.tracks.bass) === beforeBass, 're-rolling the lead changed the bass');
check(JSON.stringify(s.chords) === beforeChords, 're-rolling the lead changed the harmony');
check(JSON.stringify(s.tracks.lead) !== beforeLead || s.tracks.lead.length === 0,
  're-rolling the lead produced an identical part');

/* --- explicit settings are honoured --- */
const fixed = Composer.compose({ seed: 'FIXED-1', genre: 'house', mood: 'dark', bpm: 123, key: 5 });
check(fixed.bpm === 123, 'requested tempo ignored (' + fixed.bpm + ')');
check(fixed.rootPc === 5, 'requested key ignored (' + fixed.rootPc + ')');
check(fixed.genreId === 'house' && fixed.moodId === 'dark', 'requested genre/mood ignored');

/* --- a short song really is shorter than a long one --- */
const shortSong = Composer.compose({ seed: 'LEN', genre: 'lofi', length: 'short' });
const longSong = Composer.compose({ seed: 'LEN', genre: 'lofi', length: 'long' });
check(longSong.bars > shortSong.bars, 'length setting has no effect');

const demo = Composer.compose({ seed: 'VELVET-7318', genre: 'lofi', mood: 'chill' });
console.log('sample : ' + demo.title + ' | ' + demo.keyName + ' | ' + demo.bpm + ' BPM | ' +
  demo.bars + ' bars | ' + Math.round(demo.duration) + 's');
console.log('form   : ' + demo.sections.map(function (x) { return x.name + '(' + x.bars + ')'; }).join(' '));
console.log('chords : ' + demo.chords.slice(0, 8).map(function (c) { return c.name; }).join(' '));
console.log('notes  : ' + Object.keys(demo.tracks).map(function (k) {
  return k + '=' + demo.tracks[k].length;
}).join(' '));
console.log('');
console.log(songCount + ' songs, ' + checks + ' assertions');
console.log(failures === 0 ? 'PASS' : failures + ' FAILURES');
process.exit(failures === 0 ? 0 : 1);
