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

/* --- the instrument palette --- */
(function () {
  const P = Genres.PRESETS, L = Genres.PRESET_LABEL, GR = Genres.PRESET_GROUPS;
  const names = Object.keys(P);
  check(names.length >= 50, 'a broad palette of instruments (' + names.length + ')');

  // Nothing may appear in a picker that does not exist, and nothing unlabelled.
  const unlabelled = names.filter(function (n) { return !L[n]; });
  check(unlabelled.length === 0, 'every instrument has a name a person would recognise' +
    (unlabelled.length ? ': ' + unlabelled.join(', ') : ''));
  const ghosts = [];
  Object.keys(GR).forEach(function (g) {
    GR[g].forEach(function (n) { if (!P[n]) ghosts.push(g + '/' + n); });
  });
  check(ghosts.length === 0, 'and every picker offers only instruments that exist' +
    (ghosts.length ? ': ' + ghosts.join(', ') : ''));

  // Every genre must name instruments that exist, in every part.
  const broken = [];
  Object.keys(Genres.GENRES).forEach(function (g) {
    ['bass', 'chords', 'arp', 'lead', 'pad', 'counter'].forEach(function (part) {
      const cfg = Genres.GENRES[g][part];
      if (!cfg) return;
      if (cfg.preset && !P[cfg.preset]) broken.push(g + '.' + part + '=' + cfg.preset);
      (cfg.alts || []).forEach(function (a) { if (!P[a]) broken.push(g + '.' + part + ' alt ' + a); });
    });
  });
  check(broken.length === 0, 'and every style names instruments that exist' +
    (broken.length ? ': ' + broken.join(', ') : ''));

  // Every synthesis model is actually used by something.
  const kinds = {};
  names.forEach(function (n) { kinds[P[n].kind || 'subtractive'] = true; });
  check(Object.keys(kinds).length >= 6,
    'several different ways of making a sound (' + Object.keys(kinds).sort().join(', ') + ')');
  check(!!kinds.string, 'including a physically modelled one');

  /* Moving a part an octave has to refuse when it would leave the range a real
     instrument could play, rather than writing notes nobody can hear. */
  const s = Composer.compose({ seed: 'OCT-1', genre: 'lofi', length: 'short' });
  const before = s.tracks.lead.map(function (e) { return e.p; });
  check(Composer.shiftOctave(s, 'lead', 1), 'a part can be moved up an octave');
  check(s.tracks.lead.every(function (e, i) { return e.p === before[i] + 12; }),
    'and every note moves by exactly twelve semitones');
  check(Composer.shiftOctave(s, 'lead', -1), 'and back down again');
  check(s.tracks.lead.every(function (e, i) { return e.p === before[i]; }), 'landing where it started');

  let guard = 0;
  while (Composer.shiftOctave(s, 'lead', 1) && guard++ < 20) { /* climb */ }
  check(guard < 20, 'it refuses to climb out of hearing (' + guard + ' octaves)');
  check(s.tracks.lead.every(function (e) { return e.p <= 108; }), 'and nothing ends up above the piano');

  check(Composer.shiftOctave(s, 'drums', 1) === false, 'drums have no octave to move');
})();

/* --- every drum a genre writes survives being saved --- */
(function () {
  /* A saved song stores each drum as its index in one list, so a drum missing
     from that list packs as -1 and comes back as whatever index -1 unpacks to.
     `rim` was missing for its whole life: seven genres write sidestick
     patterns and every one of them came back from a save as a kick drum.
     Nothing else in the program had any reason to notice. */
  const used = {};
  Object.keys(Genres.GENRES).forEach(function (gid) {
    const d = Genres.GENRES[gid].drums;
    if (!d) return;
    ['intro', 'groove', 'full', 'fill'].forEach(function (sec) {
      if (!d[sec]) return;
      Object.keys(d[sec]).forEach(function (inst) { used[inst] = gid; });
    });
  });
  const names = Object.keys(used);
  check(names.length > 10, 'the genres between them use a lot of drums (' + names.length + ')');

  /* Round-trip a song carrying one note of every drum any genre asks for. */
  const s = Composer.compose({ seed: 'DRUMPACK', genre: 'lofi', length: 'short' });
  s.tracks.drums = names.map(function (inst, i) {
    return { t: i * 0.25, d: 0.25, p: 60, v: 0.8, inst: inst };
  });
  const back = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(s))));
  const lost = [];
  names.forEach(function (inst, i) {
    const got = back.tracks.drums[i];
    if (!got || got.inst !== inst) {
      lost.push(inst + '→' + (got ? got.inst : 'gone') + ' (used by ' + used[inst] + ')');
    }
  });
  check(lost.length === 0,
    'every drum a genre writes comes back from a save as itself (' + lost.join(', ') + ')');

  /* And the same list has to be append-only, since the index is the saved
     value: reordering it would rewrite the drums in every song already saved. */
  const order = Composer.DRUM_INSTS;
  check(order[0] === 'kick' && order[1] === 'snare' && order[2] === 'clap' &&
        order[3] === 'hh' && order[4] === 'oh' && order[5] === 'ride' &&
        order[6] === 'tom' && order[7] === 'conga' && order[8] === 'perc' &&
        order[9] === 'shaker' && order[10] === 'tamb' && order[11] === 'cowbell' &&
        order[12] === 'crash' && order[13] === 'riser' && order[14] === 'impact',
    'the first fifteen drum slots are still what older saves think they are');
})();

/* --- every scale, and every chord built on every degree of it --- */
(function () {
  const ids = Object.keys(Theory.SCALES);
  check(ids.length >= 40, 'there are a lot of scales (' + ids.length + ')');

  let badShape = 0, badParent = 0, dupName = 0;
  const names = {};
  ids.forEach(function (id) {
    const sc = Theory.SCALES[id];
    const st = sc.steps;
    if (!sc.name || typeof sc.minorish !== 'boolean') badShape++;
    if (st[0] !== 0 || st.length < 5 || st.length > 12) badShape++;
    for (let i = 1; i < st.length; i++) {
      if (st[i] <= st[i - 1] || st[i] > 11 || st[i] !== Math.round(st[i])) badShape++;
    }
    if (sc.chords && !Theory.SCALES[sc.chords]) badParent++;
    if (names[sc.name]) dupName++;
    names[sc.name] = true;
  });
  check(badShape === 0, 'every scale is a rising run of whole semitones from the root');

  /* The picker is built from the groups, so a scale left out of them is a
     scale that exists in the code and nowhere a person can reach. Nothing else
     in the app would complain. */
  const grouped = {};
  let dupGroup = 0, ghost = [];
  Theory.SCALE_GROUPS.forEach(function (g) {
    g.ids.forEach(function (id) {
      if (grouped[id]) dupGroup++;
      grouped[id] = true;
      if (!Theory.SCALES[id]) ghost.push(id);
    });
  });
  const missing = ids.filter(function (id) { return !grouped[id]; });
  check(missing.length === 0, 'every scale is reachable from the picker (' + missing.join(', ') + ')');
  check(ghost.length === 0, 'and the picker lists no scale that does not exist (' + ghost.join(', ') + ')');
  check(dupGroup === 0, 'with none of them listed twice');
  check(badParent === 0, 'every scale that borrows its chords names a scale that exists');
  check(dupName === 0, 'and no two scales share a name');

  /* Chords are built by stacking every other note, which only makes triads out
     of a seven-note scale. Anything else has to say where its harmony comes
     from, or the chord track is fourths and clusters. */
  let unparented = [];
  ids.forEach(function (id) {
    const sc = Theory.SCALES[id];
    if (sc.steps.length !== 7 && !sc.chords) unparented.push(id);
  });
  check(unparented.length === 0,
    'every scale that is not seven notes long borrows its harmony (' +
    unparented.join(', ') + ')');
  check(Theory.chordStepsFor('blues').length === 7,
    'so a blues melody is harmonised with seven-note chords');
  check(Theory.chordStepsFor('major') === Theory.SCALES.major.steps,
    'and a seven-note scale uses its own notes');

  /* Every shape on every degree of every scale. What matters is not that each
     one is usable — plenty are not, which is what chordIsSound is for — but
     that every one that *is* accepted has a name a musician would recognise
     and no note doubled at the octave inside it. */
  const shapes = Object.keys(Theory.CHORD_SHAPES);
  check(shapes.length >= 12, 'there are a lot of chord shapes (' + shapes.length + ')');
  let built = 0, sound = 0, badName = 0, notRising = 0, clash = 0;
  ids.forEach(function (id) {
    const steps = Theory.chordStepsFor(id);
    for (let deg = 0; deg < steps.length; deg++) {
      shapes.forEach(function (sh) {
        const p = Theory.sweetenChord(Theory.buildChord(steps, 60, deg, sh));
        built++;
        for (let i = 1; i < p.length; i++) if (p[i] <= p[i - 1]) notRising++;
        const nm = Theory.chordName(p);
        if (!nm || /undefined|NaN|null/.test(nm) || nm.length > 12) badName++;
        if (!Theory.chordIsSound(p)) return;
        sound++;
        /* The clash test has to look at the notes as stacked, not at their
           pitch classes: a major seventh chord holds a B against a C and is
           the most consonant chord there is, because they are eleven semitones
           apart. It is one semitone — or thirteen, which is the same harshness
           an octave up — that no voicing survives. */
        for (let i = 0; i < p.length; i++) {
          for (let j = i + 1; j < p.length; j++) {
            const gap = Math.abs(p[j] - p[i]);
            if (gap === 1 || gap === 13) { clash++; i = p.length; break; }
          }
        }
      });
    }
  });
  check(built > 3000, 'every shape on every degree of every scale was built (' + built + ')');
  check(notRising === 0, 'every chord comes out in rising order');
  check(badName === 0, 'and every one of them gets a readable name');
  check(sound > built * 0.5, 'most of them are usable (' + sound + ' of ' + built + ')');
  check(clash === 0, 'and none of them is voiced with a semitone or a minor ninth in it');

  /* The names themselves, against chords whose names are not a matter of
     opinion. This is the part that would silently rot if the naming rules
     were ever rearranged. */
  const NAMED = [
    [[0, 7], 'C5'], [[0, 4, 7], 'C'], [[0, 3, 7], 'Cm'],
    [[0, 3, 6], 'Cdim'], [[0, 4, 8], 'Caug'],
    [[0, 2, 7], 'Csus2'], [[0, 5, 7], 'Csus4'],
    [[0, 4, 7, 9], 'C6'], [[0, 3, 7, 9], 'Cm6'], [[0, 4, 7, 9, 14], 'C6/9'],
    [[0, 4, 7, 11], 'Cmaj7'], [[0, 4, 7, 10], 'C7'], [[0, 3, 7, 10], 'Cm7'],
    [[0, 3, 7, 11], 'Cm(maj7)'], [[0, 3, 6, 10], 'Cm7♭5'], [[0, 3, 6, 9], 'Cdim7'],
    [[0, 4, 8, 10], 'Caug7'], [[0, 4, 7, 14], 'Cadd9'],
    [[0, 4, 7, 10, 14], 'C9'], [[0, 4, 7, 11, 14], 'Cmaj9'],
    [[0, 3, 7, 10, 14], 'Cm9'], [[0, 3, 7, 10, 14, 17], 'Cm11'],
    [[0, 4, 7, 10, 14, 21], 'C13'], [[0, 4, 7, 11, 14, 21], 'Cmaj13'],
    [[0, 5, 7, 10], 'C7sus4'], [[0, 7, 11], 'Cmaj7'], [[0, 7, 10], 'C7']
  ];
  const wrong = NAMED.filter(function (c) {
    return Theory.chordName(c[0].map(function (x) { return x + 60; })) !== c[1];
  });
  check(wrong.length === 0, 'the standard chords are named the standard way (' +
    wrong.map(function (c) {
      return Theory.chordName(c[0].map(function (x) { return x + 60; })) + '≠' + c[1];
    }).join(', ') + ')');

  /* An eleventh over a major third is the one clash that theory books allow on
     paper and no player ever voices. */
  check(!Theory.chordIsSound([60, 64, 67, 70, 74, 77]),
    'an eleventh over a major third is refused');
  check(Theory.chordIsSound([60, 63, 67, 70, 74, 77]),
    'but over a minor third it is the everyday m11');

  /* And a whole song in every one of them, because a scale that cannot be
     composed in is not a scale this program has. */
  let broke = [];
  ids.forEach(function (id) {
    let song;
    try {
      song = Composer.compose({ seed: 'SCALE-' + id, genre: 'lofi', scale: id, length: 'short' });
    } catch (e) {
      broke.push(id + ' threw ' + e.message);
      return;
    }
    if (song.scaleId !== id) { broke.push(id + ' did not take'); return; }
    if (!song.chords || !song.chords.length) { broke.push(id + ' has no chords'); return; }
    let notes = 0, outOfRange = 0, past = 0;
    Object.keys(song.tracks).forEach(function (t) {
      song.tracks[t].forEach(function (e) {
        notes++;
        if (e.p < 12 || e.p > 108) outOfRange++;
        /* Onset, not end: a note may start inside the song and ring out past
           it — the render leaves a tail for exactly that. What must never
           happen is a note *starting* after the song is over. */
        if (e.t >= song.totalBeats) past++;
      });
    });
    if (notes < 20) broke.push(id + ' is nearly empty (' + notes + ')');
    if (outOfRange) broke.push(id + ' wrote ' + outOfRange + ' unplayable notes');
    if (past) broke.push(id + ' wrote ' + past + ' notes past the end');
    if (song.chords.some(function (c) { return !c.name; })) broke.push(id + ' has a nameless chord');
  });
  check(broke.length === 0, 'a whole song can be written in every scale (' +
    broke.slice(0, 4).join('; ') + ')');
})();

/* --- harmony lines and octave doubling --- */
(function () {
  /* A harmony is not a fixed number of semitones. A third in a major key is
     four semitones from the first degree and three from the second, and that
     bending to stay in the key is the entire difference between a harmony line
     and a detuned copy. So the test that matters is not "is it a third" but
     "is it *both* sizes of third, in the right places". */
  const s = Composer.compose({ seed: 'HARM-1', genre: 'lofi', length: 'medium' });
  const before = s.tracks.lead.map(function (e) { return { t: e.t, d: e.d, p: e.p, v: e.v }; });
  check(before.length > 4, 'there is a tune to harmonise (' + before.length + ' notes)');

  check(Composer.harmonise(s, 'lead', 2), 'a part can be given a harmony line');
  const after = s.tracks.lead;
  check(after.length > before.length, 'which adds notes (' + before.length +
    ' → ' + after.length + ')');

  /* Every original note must survive untouched: a harmony adds a voice, it
     does not rewrite the tune. */
  const kept = before.every(function (o) {
    return after.some(function (e) {
      return e.t === o.t && e.p === o.p && e.d === o.d && e.v === o.v;
    });
  });
  check(kept, 'and leaves every note of the original exactly where it was');

  /* Pick out the added notes by difference rather than by position, so the
     measurement cannot accidentally pair a note with another *original* note
     that happens to sit above it. */
  const wasThere = {};
  before.forEach(function (o) { wasThere[o.t.toFixed(4) + ':' + o.p] = true; });
  const added = after.filter(function (e) { return !wasThere[e.t.toFixed(4) + ':' + e.p]; });
  check(added.length + before.length === after.length,
    'the new notes are all additions, nothing was replaced');

  const scaleSet = {};
  s.scaleSteps.forEach(function (st) { scaleSet[st] = true; });
  const sizes = {};
  let allInKey = true, allQuieter = true, allPaired = true;
  added.forEach(function (a) {
    const root = Composer.keyRootAt(s, a.t);
    const pc = (((a.p - root) % 12) + 12) % 12;
    if (!scaleSet[pc]) allInKey = false;
    const src = before.filter(function (o) { return o.t === a.t; })
      .sort(function (x, y) { return Math.abs(a.p - x.p) - Math.abs(a.p - y.p); })[0];
    if (!src) { allPaired = false; return; }
    sizes[a.p - src.p] = (sizes[a.p - src.p] || 0) + 1;
    if (a.v >= src.v) allQuieter = false;
  });
  check(allPaired, 'every harmony note belongs to a note of the tune');
  check(allInKey, 'every harmony note is in the key');
  check(allQuieter, 'and sits under the tune rather than level with it');
  const kinds = Object.keys(sizes).map(Number).sort(function (a, b) { return a - b; });
  /* Three or four semitones, and never two. The tune contains chromatic notes
     — the third of a secondary dominant, passing notes — and a harmony
     measured from the scale degree nearest to one of those can land a whole
     tone away, which is a second and sounds like a mistake. */
  check(kinds.every(function (k) { return k === 3 || k === 4; }),
    'every interval is a third, major or minor, never a second (' + kinds.join(', ') + ')');
  check(kinds.length > 1,
    'and both sizes occur, which is what makes it diatonic rather than a fixed shift (' +
    kinds.map(function (k) { return k + '×' + sizes[k]; }).join(', ') + ')');

  /* Doubling an octave is the plain version of the same idea, and must not be
     confused with moving the part: the original stays. */
  const d = Composer.compose({ seed: 'HARM-2', genre: 'house', length: 'short' });
  const bassBefore = d.tracks.bass.map(function (e) { return { t: e.t, p: e.p }; });
  check(Composer.doubleOctave(d, 'bass', -1), 'a part can be doubled an octave lower');
  check(bassBefore.every(function (o) {
    return d.tracks.bass.some(function (e) { return e.t === o.t && e.p === o.p; });
  }), 'with the original part still there');
  check(bassBefore.every(function (o) {
    return o.p - 12 < 16 || d.tracks.bass.some(function (e) {
      return e.t === o.t && e.p === o.p - 12;
    });
  }), 'and a copy twelve semitones below every note that had room for one');
  check(d.tracks.bass.every(function (e) { return e.p >= 16 && e.p <= 108; }),
    'nothing falls off the end of the keyboard');
  check(d.tracks.bass.every(function (e) { return e.t + e.d <= d.totalBeats; }),
    'and nothing runs past the end of the song');

  check(Composer.harmonise(d, 'drums', 2) === false, 'drums have no tune to harmonise');
  check(Composer.doubleOctave(d, 'drums', -1) === false, 'and none to double');
  check(Composer.harmonise(d, 'nosuchpart', 2) === false, 'an unknown part is refused');
})();

/* --- feel: swing, grooves, fills, ghosts and half time --- */
(function () {
  /* Swing is no longer written into the score, so the score should be straight
     and swing should show up only when the feel is applied. That separation is
     the whole point: it makes the groove something you can move while you
     listen rather than something that needs a part rewritten. */
  const s = Composer.compose({ seed: 'FEEL-1', genre: 'lofi', length: 'medium' });
  check(s.swing > 0, 'lo-fi is a swung style (' + s.swing + ')');

  const offbeats = s.tracks.drums.filter(function (e) {
    return Math.abs((e.t % 1) - 0.5) < 0.06;
  });
  check(offbeats.length > 4, 'there are offbeats in the score to look at');
  check(offbeats.every(function (e) { return Math.abs((e.t % 1) - 0.5) < 0.06; }),
    'and the score itself is straight — swing is not baked into the notes');

  const straight = Composer.swingTime(1.5, 0, null);
  const swung = Composer.swingTime(1.5, 0.3, null);
  check(straight === 1.5, 'with no swing an offbeat stays where it is');
  check(swung > 1.5 && swung < 2, 'with swing it is pushed late (' + swung + ')');
  check(Composer.swingTime(1, 0.3, null) === 1, 'and the downbeat never moves');

  // A groove overrides the style's own swing, and adds its lean.
  Object.keys(Composer.GROOVES).forEach(function (id) {
    const g = Object.assign({}, s, { groove: id });
    const feel = Composer.feelOf(g);
    check(feel.swing === Composer.GROOVES[id].swing, id + ': the groove sets the swing');
    const moved = Composer.swingTime(0.25, feel.swing, feel.push);
    check(moved >= 0, id + ': and a sixteenth lands somewhere real (' + moved.toFixed(3) + ')');
  });
  check(Composer.feelOf(s).swing === s.swing, 'with no groove chosen, the style plays as written');

  // Looseness is written in, so it has to change the notes.
  const tight = Composer.compose({ seed: 'FEEL-2', genre: 'funk', humanise: 0 });
  const loose = Composer.compose({ seed: 'FEEL-2', genre: 'funk', humanise: 2 });
  const drift = function (song) {
    const d = song.tracks.drums.map(function (e) {
      return Math.abs(e.t * 4 - Math.round(e.t * 4));
    });
    return d.reduce(function (a, b) { return a + b; }, 0) / d.length;
  };
  check(drift(tight) < 1e-6, 'dead straight really is dead straight (' + drift(tight).toFixed(6) + ')');
  check(drift(loose) > drift(tight), 'and loose drifts off the grid (' + drift(loose).toFixed(4) + ')');

  // Both survive a save.
  const withFeel = Composer.compose({ seed: 'FEEL-3', genre: 'jazz', humanise: 1.5 });
  withFeel.groove = 'dilla';
  const back = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(withFeel))));
  check(back.groove === 'dilla' && back.humanise === 1.5, 'groove and looseness survive a save');

  /* Fills should differ from one another. A fill marks a seam, and a seam you
     have heard eight times has stopped marking anything. */
  const long = Composer.compose({ seed: 'FILL-1', genre: 'rock', length: 'long' });
  const shapes = long.sections.slice(0, -1).map(function (sec) {
    const from = (sec.startBar + sec.bars - 1) * long.beatsPerBar;
    return long.tracks.drums
      .filter(function (e) { return e.t >= from && e.t < from + long.beatsPerBar; })
      .map(function (e) { return e.inst + Math.round((e.t - from) * 4); }).sort().join(' ');
  }).filter(function (x) { return x.length > 0; });
  check(shapes.length >= 4, 'there are seams to look at (' + shapes.length + ')');
  const distinct = {};
  shapes.forEach(function (x) { distinct[x] = true; });
  check(Object.keys(distinct).length >= shapes.length * 0.6,
    'and the fills are not all the same (' + Object.keys(distinct).length +
    ' distinct of ' + shapes.length + ')');

  // Ghost notes: quiet, and quiet on purpose.
  const ghosted = Composer.compose({ seed: 'GHOST-1', genre: 'funk', length: 'medium' });
  const snares = ghosted.tracks.drums.filter(function (e) { return e.inst === 'snare'; });
  const ghosts = snares.filter(function (e) { return e.v < 0.32; });
  check(ghosts.length > 0, 'a groove gets ghost notes (' + ghosts.length + ' of ' + snares.length + ')');
  check(ghosts.length < snares.length * 0.6, 'but they do not take over from the backbeat');

  /* Half time: the backbeat happens half as often while the hats keep the
     original pulse. Measured against a normal section of the same song. */
  let ht = null;
  for (let i = 0; i < 40 && !ht; i++) {
    const song = Composer.compose({ seed: 'HALF-' + i, genre: 'drill', length: 'medium' });
    const half = song.sections.filter(function (x) { return x.halfTime && x.parts.drums; })[0];
    const norm = song.sections.filter(function (x) {
      return !x.halfTime && x.energy >= 0.6 && x.parts.drums;
    })[0];
    if (half && norm) ht = { song: song, half: half, norm: norm };
  }
  check(!!ht, 'a style that drops into half time does so');
  if (ht) {
    const per = function (sec, inst) {
      const from = sec.startBar * ht.song.beatsPerBar;
      const to = from + sec.bars * ht.song.beatsPerBar;
      return ht.song.tracks.drums.filter(function (e) {
        return e.inst === inst && e.t >= from && e.t < to && e.v >= 0.5;
      }).length / sec.bars;
    };
    const halfSnare = per(ht.half, 'snare'), normSnare = per(ht.norm, 'snare');
    check(normSnare > 0, 'the normal section has a backbeat to compare against');
    check(halfSnare > 0, 'half time still has a backbeat at all (' + halfSnare.toFixed(2) + '/bar)');
    check(halfSnare < normSnare * 0.75,
      'and it lands half as often (' + halfSnare.toFixed(2) + ' vs ' + normSnare.toFixed(2) + ' per bar)');
    check(per(ht.half, 'hh') >= per(ht.norm, 'hh') * 0.8,
      'while the hats keep the original pulse — that is what makes it heavy rather than slow');
    const saved = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(ht.song))));
    check(saved.sections.filter(function (x) { return x.halfTime; }).length ===
          ht.song.sections.filter(function (x) { return x.halfTime; }).length,
      'and half time survives a save');
  }
})();

/* --- riffs, turnarounds, quoting, and thinning a part out --- */
(function () {
  /* A riff is not an arpeggio. An arpeggio runs whatever notes the chord
     contains, so its shape changes with the harmony; a riff keeps its rhythm
     and its intervals and moves bodily to each new root. That difference is
     the whole point, so it is what gets measured. */
  let riffed = null;
  for (let i = 0; i < 40 && !riffed; i++) {
    const s = Composer.compose({ seed: 'RIFF-' + i, genre: 'rock', length: 'medium' });
    // A riff repeats the same rhythm in every bar it plays.
    const byBar = {};
    s.tracks.arp.forEach(function (e) {
      const b = Math.floor(e.t / s.beatsPerBar);
      (byBar[b] = byBar[b] || []).push(Math.round((e.t % s.beatsPerBar) * 4));
    });
    const shapes = Object.keys(byBar).map(function (b) { return byBar[b].join(','); });
    const counts = {};
    shapes.forEach(function (x) { counts[x] = (counts[x] || 0) + 1; });
    const top = Math.max.apply(null, Object.keys(counts).map(function (k) { return counts[k]; }));
    if (shapes.length >= 6 && top >= shapes.length * 0.4) riffed = { song: s, top: top, bars: shapes.length };
  }
  check(!!riffed, 'a riff-driven style produces a repeating figure');
  if (riffed) {
    check(riffed.top >= riffed.bars * 0.4,
      'the same rhythm comes back bar after bar (' + riffed.top + ' of ' + riffed.bars + ' bars)');

    // And it moves with the chord rather than staying put.
    const roots = {};
    riffed.song.chords.forEach(function (c) { roots[((c.rootPitch % 12) + 12) % 12] = true; });
    const heard = {};
    riffed.song.tracks.arp.forEach(function (e) { heard[((e.p % 12) + 12) % 12] = true; });
    check(Object.keys(heard).length >= 3, 'and it visits more than one pitch (' +
      Object.keys(heard).length + ' pitch classes)');
  }

  /* Turnarounds: a phrase hinge. The test that matters is that they can happen
     at all — the first attempt used eight-bar phrases, which never fire inside
     the eight-bar sections this composer actually writes. */
  let hinge = 0, longSections = 0;
  ['country', 'lofi', 'jazz', 'bossa'].forEach(function (g) {
    for (let i = 0; i < 8; i++) {
      const s = Composer.compose({ seed: 'HINGE-' + g + i, genre: g, length: 'long' });
      s.sections.forEach(function (sec) {
        if (sec.bars < 8 || !sec.chords.length) return;
        longSections++;
        const spans = sec.chords.map(function (c) { return c.bars; });
        if (spans.some(function (b) { return b === 1; }) &&
            spans.some(function (b) { return b > 1; })) hinge++;
      });
    }
  });
  check(longSections > 20, 'there are long sections to look at (' + longSections + ')');
  check(hinge > 0, 'phrases get a turnaround (' + hinge + ' of ' + longSections + ' sections)');

  /* The chorus should quote the verse. Same rhythm, different shape: identical
     would be a copy, unrelated would be a different song spliced in. */
  let quoted = 0, compared = 0;
  for (let i = 0; i < 12; i++) {
    const s = Composer.compose({ seed: 'QUOTE-' + i, genre: 'country', length: 'medium' });
    const v = s.sections.filter(function (x) { return x.type === 'verse' && x.parts.lead; })[0];
    const c = s.sections.filter(function (x) { return x.type === 'chorus' && x.parts.lead; })[0];
    if (!v || !c) continue;
    const rhythmOf = function (sec) {
      const from = sec.startBar * s.beatsPerBar;
      return s.tracks.lead
        .filter(function (e) { return e.t >= from && e.t < from + 4 * s.beatsPerBar; })
        .map(function (e) { return Math.round((e.t - from) * 4); });
    };
    const rv = rhythmOf(v), rc = rhythmOf(c);
    if (!rv.length || !rc.length) continue;
    compared++;
    /* Overlap, not equality. The motif's rhythm is shared, but what reaches
       the track is not: rests are dropped at random, phrase ends are trimmed,
       and the two sections play at different densities. Demanding identical
       onsets would be testing that none of that happens. */
    const shared = rv.filter(function (x) { return rc.indexOf(x) >= 0; }).length;
    if (shared >= Math.min(rv.length, rc.length) * 0.5) quoted++;
  }
  check(compared >= 6, 'there are verse/chorus pairs to compare (' + compared + ')');
  check(quoted >= compared * 0.5,
    'the chorus is built on the verse rhythm rather than a new one (' +
    quoted + ' of ' + compared + ')');

  /* Thinning and filling. Both have to keep the part recognisable: thinning
     keeps what falls on a beat, filling keeps every note that was there. */
  const d = Composer.compose({ seed: 'DENSITY-1', genre: 'funk', length: 'medium' });
  const beforeLead = d.tracks.lead.length;
  const originals = d.tracks.lead.map(function (e) { return e.t + ':' + e.p; });

  check(Composer.adjustDensity(d, 'lead', 1), 'a part can be filled in');
  check(d.tracks.lead.length > beforeLead,
    'and gets busier (' + beforeLead + ' → ' + d.tracks.lead.length + ')');
  const kept = d.tracks.lead.map(function (e) { return e.t + ':' + e.p; });
  check(originals.every(function (o) { return kept.indexOf(o) >= 0; }),
    'without losing a single note that was already there');
  let sorted = true;
  for (let i = 1; i < d.tracks.lead.length; i++) {
    if (d.tracks.lead[i].t < d.tracks.lead[i - 1].t) sorted = false;
  }
  check(sorted, 'and the part stays in time order');
  check(d.tracks.lead.every(function (e) { return e.t < d.totalBeats; }),
    'and nothing lands past the end of the song');

  const busy = d.tracks.lead.length;
  check(Composer.adjustDensity(d, 'lead', -1), 'and thinned out again');
  check(d.tracks.lead.length < busy,
    'getting simpler (' + busy + ' → ' + d.tracks.lead.length + ')');
  check(d.tracks.lead.length >= 4, 'but never thinned out of existence');

  // Drums thin and fill too, and keep their pieces.
  const dr = d.tracks.drums.length;
  Composer.adjustDensity(d, 'drums', 1);
  check(d.tracks.drums.length > dr, 'drums can be filled in as well');
  check(d.tracks.drums.every(function (e) { return !!e.inst; }),
    'and every hit still names a drum');

  check(Composer.adjustDensity(d, 'nosuchpart', 1) === false,
    'a part that does not exist is refused rather than crashing');
})();

/* --- the answering voice --- */
(function () {
  check(Composer.LANE_NAMES !== undefined, 'composer exports are intact');

  /* A counter-melody that moves with the lead is not a counter-melody — two
     voices in parallel read as one thicker voice. These checks are about
     separation: it plays where the lead is not, and it moves the other way. */
  let sample = null;
  for (let i = 0; i < 30 && !sample; i++) {
    const s = Composer.compose({ seed: 'ANSWER-' + i, genre: 'jazz', length: 'medium' });
    if (s.tracks.counter && s.tracks.counter.length >= 6) sample = s;
  }
  check(!!sample, 'a style that uses a second voice produces one');

  if (sample) {
    const lead = sample.tracks.lead;
    const counter = sample.tracks.counter;

    /* Almost nothing should start while the lead is mid-note. The exception is
       deliberate: a long held lead note is exactly when a second voice moving
       underneath is audible as a second voice. */
    const clashes = counter.filter(function (c) {
      return lead.some(function (l) {
        return c.t >= l.t - 0.05 && c.t < l.t + l.d - 0.1 && l.d < 1.5;
      });
    });
    check(clashes.length === 0,
      'the answer never starts on top of a short lead note (' + clashes.length + ' of ' +
      counter.length + ')');

    // It only appears in sections that asked for it.
    const wanted = {};
    sample.sections.forEach(function (sec) {
      if (sec.parts.counter) {
        for (let b = sec.startBar; b < sec.startBar + sec.bars; b++) wanted[b] = true;
      }
    });
    const strays = counter.filter(function (e) {
      return !wanted[Math.floor(e.t / sample.beatsPerBar)];
    });
    check(strays.length === 0, 'and only in the sections that called for it');

    // It stays in key and in a sane range.
    const inKey = counter.filter(function (e) {
      const set = {};
      sample.scaleSteps.forEach(function (st) {
        set[((Composer.keyRootAt(sample, e.t) + st) % 12 + 12) % 12] = true;
      });
      return set[((e.p % 12) + 12) % 12];
    });
    check(inKey.length / counter.length > 0.8,
      'it stays in key (' + Math.round(100 * inKey.length / counter.length) + '%)');
    const span = Math.max.apply(null, counter.map(function (e) { return e.p; })) -
                 Math.min.apply(null, counter.map(function (e) { return e.p; }));
    check(span <= 24, 'and inside a two-octave range (' + span + ' semitones)');

    // It has to survive everything else a part survives.
    const back = Composer.unpackSong(JSON.parse(JSON.stringify(Composer.packSong(sample))));
    check(back.tracks.counter.length === counter.length, 'the answer survives a save');
    Composer.rerollPart(sample, 'counter');
    check(Array.isArray(sample.tracks.counter), 'and can be re-rolled on its own');
  }

  /* The lead has to leave room, or there is nothing to answer into. Measure the
     silence: a melody with no gaps is a stream of notes, not a line. */
  const phrased = Composer.compose({ seed: 'BREATH-1', genre: 'country', length: 'medium' });
  const notes = phrased.tracks.lead.slice().sort(function (a, b) { return a.t - b.t; });
  let gaps = 0;
  for (let i = 1; i < notes.length; i++) {
    if (notes[i].t - (notes[i - 1].t + notes[i - 1].d) >= 0.7) gaps++;
  }
  check(notes.length > 20, 'there is a melody to measure (' + notes.length + ' notes)');
  check(gaps >= Math.floor(phrased.bars / 8),
    'the melody stops to breathe (' + gaps + ' gaps in ' + phrased.bars + ' bars)');

  // Every part the engine knows about must be one the composer can write.
  check(Genres.GENRES.jazz.counter !== undefined, 'genres describe their second voice');
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

  /* Find the hand-drawn note by its exact position. The composer nudges every
     note it writes a few thousandths off the grid, so a note sitting on a clean
     quarter of a beat is one nothing but a hand put there — and unlike a marker
     velocity, a time cannot be collided with by rounding. Its pitch and bar have
     legitimately moved (the song was transposed and rearranged after it was
     drawn), so those are read from the edited song rather than from what was
     typed in. */
  const clean = function (t) { return Math.abs(t * 4 - Math.round(t * 4)) < 1e-9; };
  const handA = a.tracks.lead.filter(function (e) { return clean(e.t) && e.v === 0.8; });
  const handB = b.tracks.lead.filter(function (e) { return clean(e.t) && Math.abs(e.v - 0.8) < 1e-9; });
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
