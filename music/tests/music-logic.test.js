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

/* --- scoring to picture: an exact length and a supplied plan --- */
const plan = [
  { type: 'intro',  bars: 8,  energy: 0.15, parts: { pad: true, chords: true, bass: false, drums: false, arp: false, lead: false } },
  { type: 'chorus', bars: 16, energy: 0.9,  parts: { pad: true, chords: true, bass: true,  drums: true,  arp: true,  lead: true } },
  { type: 'outro',  bars: 8,  energy: 0.2,  parts: { pad: true, chords: true, bass: false, drums: false, arp: false, lead: false } }
];
const planned = Composer.compose({ genre: 'cinematic', mood: 'dark', seed: 'plan-7', bpm: 90, sections: plan });
check(planned.sections.length === 3, 'a supplied plan must be used as given (got ' + planned.sections.length + ' sections)');
check(planned.bars === 32, 'bar count must come from the plan (got ' + planned.bars + ')');
check(planned.sections[0].parts.drums === false, 'the supplied intro must stay drumless');
check(planned.sections[1].parts.lead === true, 'the supplied chorus must keep its lead');
check(!!planned.tracks.drums, 'a planned song still needs a drum track object');

const timed = Composer.compose({ genre: 'cinematic', mood: 'chill', seed: 'timed-11', seconds: 150 });
check(timed.duration >= 150, 'a song asked for 150s came back at ' + timed.duration.toFixed(1) + 's');
check(timed.duration < 170, 'a song asked for 150s overshot to ' + timed.duration.toFixed(1) + 's');

/* --- a fast genre must not clamp short of a long requested duration --- */
const longTrap = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'long-trap', seconds: 300 });
check(longTrap.duration >= 300, 'a fast genre asked for 300s came back at ' + longTrap.duration.toFixed(1) + 's');
check(longTrap.duration < 330, 'a fast genre asked for 300s overshot to ' + longTrap.duration.toFixed(1) + 's');

/* --- opts.seconds must be validated and bounded before any bar arithmetic:
 * only a finite number > 0 counts as an exact-duration request; NaN,
 * Infinity, negative values and strings must fall through to the
 * length-preset behaviour exactly, and a real request must be capped. --- */
const MAX_EXACT_SECONDS = Composer.MAX_EXACT_SECONDS;

let t0 = Date.now();
const infGuard = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'inf-guard', seconds: Infinity });
let elapsed = Date.now() - t0;
check(elapsed < 2000, 'compose with seconds:Infinity must return promptly, took ' + elapsed + 'ms');
check(infGuard.duration <= MAX_EXACT_SECONDS + 40,
  'seconds:Infinity must not blow past the cap (got ' + infGuard.duration.toFixed(1) + 's)');
const noSecondsInf = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'inf-guard' });
check(JSON.stringify(infGuard.sections) === JSON.stringify(noSecondsInf.sections),
  'seconds:Infinity must fall through to the length-preset path exactly (same seed, same result)');

const nanGuard = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'nan-guard', seconds: NaN });
const noSecondsNan = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'nan-guard' });
check(JSON.stringify(nanGuard.sections) === JSON.stringify(noSecondsNan.sections),
  'seconds:NaN must behave like no seconds was passed');

const negGuard = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'neg-guard', seconds: -50 });
const noSecondsNeg = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'neg-guard' });
check(JSON.stringify(negGuard.sections) === JSON.stringify(noSecondsNeg.sections),
  'a negative seconds must behave like no seconds was passed');

t0 = Date.now();
const hugeGuard = Composer.compose({ genre: 'trap', mood: 'driving', seed: 'huge-guard', seconds: 999999 });
elapsed = Date.now() - t0;
check(elapsed < 2000, 'compose with a huge finite seconds must return promptly, took ' + elapsed + 'ms');
check(hugeGuard.duration >= MAX_EXACT_SECONDS,
  'a capped exact-duration request must still not come back short (got ' + hugeGuard.duration.toFixed(1) + 's)');
check(hugeGuard.duration <= MAX_EXACT_SECONDS + 40,
  'a huge seconds request must come back capped, not enormous (got ' + hugeGuard.duration.toFixed(1) + 's)');

const before = Composer.compose({ genre: 'lofi', mood: 'chill', length: 'short', seed: 'unchanged-99' });
const after  = Composer.compose({ genre: 'lofi', mood: 'chill', length: 'short', seed: 'unchanged-99' });
check(JSON.stringify(after.sections) === JSON.stringify(before.sections),
  'composing without the new options must be unchanged');

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
