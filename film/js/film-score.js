/*
 * SCRIPT FORGE — the conductor.
 * -----------------------------
 * Turns a reel into a score request SONG FORGE can compose from, and a duck
 * envelope that keeps the music under the dialogue.
 *
 * Pure logic: a reel goes in, plain data comes out. No audio and no DOM, so the
 * musical shape of a film is checkable in Node the way its edit already is.
 *
 * Exposed as window.FilmConductor (and module.exports for the tests) —
 * not window.FilmScore, which film-audio.js's *audio* Score already claims
 * for its own API; sharing that name would silently clobber whichever of the
 * two loads second.
 */
(function (root) {
  'use strict';

  /* Which of SONG FORGE's genres and moods each kind of film is scored with. */
  var MUSIC_FOR = {
    drama:    { genre: 'cinematic', mood: 'chill' },
    thriller: { genre: 'cinematic', mood: 'driving' },
    horror:   { genre: 'ambient',   mood: 'dark' },
    comedy:   { genre: 'lofi',      mood: 'uplifting' },
    romance:  { genre: 'lofi',      mood: 'dreamy' },
    scifi:    { genre: 'synthwave', mood: 'dreamy' },
    mystery:  { genre: 'cinematic', mood: 'dark' },
    fantasy:  { genre: 'cinematic', mood: 'dreamy' },
    heist:    { genre: 'dnb',       mood: 'driving' },
    western:  { genre: 'cinematic', mood: 'chill' }
  };

  var BEATS_PER_BAR = 4;      // SONG FORGE's own bar length
  var BLOCK_BARS = 4;         // sections are placed on four-bar blocks

  function blockSeconds(bpm) {
    return (BEATS_PER_BAR * BLOCK_BARS * 60) / bpm;
  }

  /* Where the film cuts from one scene to the next. The title card belongs to
   * the first scene and the end card to the last, so neither makes a cut. */
  function cutTimes(reel) {
    var seen = {};
    var cuts = [];
    reel.shots.forEach(function (shot) {
      if (!shot.scene || seen[shot.scene]) return;
      seen[shot.scene] = true;
      if (cuts.length || shot.scene > 1) cuts.push(shot.start);
    });
    return cuts;
  }

  /* Pick the tempo inside the genre's range whose four-bar boundaries land
   * closest to the actual cuts. Ties go to the slower tempo, so the choice is
   * stable for a given film. */
  function chooseBpm(reel, range) {
    var low = Math.round(range && range[0] ? range[0] : 72);
    var high = Math.round(range && range[1] ? range[1] : 108);
    if (high < low) { var swap = low; low = high; high = swap; }

    var cuts = cutTimes(reel);
    var best = low;
    var bestError = Infinity;
    for (var bpm = low; bpm <= high; bpm++) {
      var block = blockSeconds(bpm);
      var error = 0;
      for (var i = 0; i < cuts.length; i++) {
        error += Math.abs(cuts[i] - Math.round(cuts[i] / block) * block);
      }
      if (error < bestError - 1e-9) { bestError = error; best = bpm; }
    }
    return best;
  }

  /* What each story beat sounds like structurally. */
  var SECTION_TYPE = {
    open: 'intro', spark: 'verse', push: 'verse', turn: 'bridge',
    crisis: 'chorus', choice: 'bridge', after: 'outro'
  };

  /* One entry per scene: when it starts, how long it runs, which beat it is. */
  function scenesOf(reel) {
    var scenes = [];
    var byNumber = {};
    reel.shots.forEach(function (shot) {
      if (!shot.scene) return;               // title and end cards
      if (!byNumber[shot.scene]) {
        byNumber[shot.scene] = { scene: shot.scene, start: shot.start, end: 0, beat: shot.beat, mood: shot.mood };
        scenes.push(byNumber[shot.scene]);
      }
      byNumber[shot.scene].end = shot.start + shot.duration;
      if (shot.mood > byNumber[shot.scene].mood) byNumber[shot.scene].mood = shot.mood;
    });
    if (scenes.length) {
      scenes[0].start = 0;                   // the title card belongs to scene one
      scenes[scenes.length - 1].end = reel.duration;   // and the end card to the last
    }
    return scenes;
  }

  /* The difference between a song playing under a film and a score: the band
   * arrives as the story tightens, and stands down for the ending. */
  function partsFor(energy, isFinal) {
    if (isFinal) {
      return { drums: false, bass: false, chords: true, arp: false, lead: false, pad: true };
    }
    return {
      pad: true,
      chords: true,
      bass: energy >= 0.3,
      drums: energy >= 0.5,
      arp: energy > 0.7,
      lead: energy > 0.7
    };
  }

  function sectionPlan(reel, bpm) {
    var barSeconds = (BEATS_PER_BAR * 60) / bpm;
    var scenes = scenesOf(reel);

    var plan = scenes.map(function (scene, index) {
      var blocks = Math.max(1, Math.round((scene.end - scene.start) / (barSeconds * BLOCK_BARS)));
      var energy = Math.max(0, Math.min(1, scene.mood));
      var isFinal = index === scenes.length - 1;
      return {
        type: SECTION_TYPE[scene.beat] || 'verse',
        bars: blocks * BLOCK_BARS,
        energy: energy,
        scene: scene.scene,
        last: isFinal,
        parts: partsFor(energy, isFinal)
      };
    });

    // Rounding can leave the music a block short. It may run long; it may never
    // run out before the picture does.
    var totalSeconds = function () {
      return plan.reduce(function (bars, s) { return bars + s.bars; }, 0) * barSeconds;
    };
    var guard = 0;
    while (plan.length && totalSeconds() < reel.duration && guard++ < 200) {
      plan[plan.length - 1].bars += BLOCK_BARS;
    }
    return plan;
  }

  var DUCK_GAIN = 0.35;    // how far the music drops under a line
  var DUCK_LEAD = 0.25;    // seconds before the line it starts dropping
  var DUCK_TAIL = 0.20;    // seconds after the line before it comes back

  /* The reel knows exactly when every line is spoken, so the ducking is
   * arithmetic rather than a side-chain: a sorted list of level changes. */
  function duckEnvelope(reel) {
    var spans = [];
    reel.shots.forEach(function (shot) {
      if (shot.kind !== 'line') return;
      spans.push({
        from: Math.max(0, shot.start - DUCK_LEAD),
        to: shot.start + shot.duration + DUCK_TAIL
      });
    });
    spans.sort(function (a, b) { return a.from - b.from; });

    // Lines that run into each other stay down rather than pumping between them.
    var merged = [];
    spans.forEach(function (span) {
      var last = merged[merged.length - 1];
      if (last && span.from <= last.to) {
        if (span.to > last.to) last.to = span.to;
      } else {
        merged.push({ from: span.from, to: span.to });
      }
    });

    var points = [{ t: 0, gain: 1 }];
    merged.forEach(function (span) {
      points.push({ t: span.from, gain: DUCK_GAIN });
      points.push({ t: span.to, gain: 1 });
    });
    return points;
  }

  var DEFAULT_BPM_RANGE = [72, 108];

  /* Assemble everything above into the one object SONG FORGE composes from. */
  function request(reel, opts) {
    opts = opts || {};
    var music = MUSIC_FOR[reel.genre] || MUSIC_FOR.drama;
    var bpm = chooseBpm(reel, opts.bpmRange || DEFAULT_BPM_RANGE);
    return {
      genre: music.genre,
      mood: music.mood,
      // A film and its score share a lineage without sharing a number, so the
      // music is stable per film but is not the same draw as the picture.
      seed: (reel.seed ^ 0x5f356495) >>> 0,
      seconds: reel.duration,
      bpm: bpm,
      sections: sectionPlan(reel, bpm)
    };
  }

  var API = {
    MUSIC_FOR: MUSIC_FOR,
    BEATS_PER_BAR: BEATS_PER_BAR,
    BLOCK_BARS: BLOCK_BARS,
    blockSeconds: blockSeconds,
    cutTimes: cutTimes,
    chooseBpm: chooseBpm,
    SECTION_TYPE: SECTION_TYPE,
    scenesOf: scenesOf,
    partsFor: partsFor,
    sectionPlan: sectionPlan,
    DEFAULT_BPM_RANGE: DEFAULT_BPM_RANGE,
    request: request,
    duckEnvelope: duckEnvelope,
    DUCK_GAIN: DUCK_GAIN,
    DUCK_LEAD: DUCK_LEAD,
    DUCK_TAIL: DUCK_TAIL
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmConductor = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
