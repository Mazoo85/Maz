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

const { Genres, Composer, Exporter } = sandbox;

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
          check(e.t < song.totalBeats + 4, seed + '/' + tr + ': event past the end (' + e.t + ')');
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
    check(Math.abs(song.totalBeats - song.bars * 4) < 1e-6, label + ': beats match bars');
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
