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
