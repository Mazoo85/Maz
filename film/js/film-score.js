/*
 * SCRIPT FORGE — the conductor.
 * -----------------------------
 * Turns a reel into a score request SONG FORGE can compose from, and a duck
 * envelope that keeps the music under the dialogue.
 *
 * Pure logic: a reel goes in, plain data comes out. No audio and no DOM, so the
 * musical shape of a film is checkable in Node the way its edit already is.
 *
 * Exposed as window.FilmScore (and module.exports for the tests).
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

  var API = {
    MUSIC_FOR: MUSIC_FOR,
    BEATS_PER_BAR: BEATS_PER_BAR,
    BLOCK_BARS: BLOCK_BARS,
    blockSeconds: blockSeconds,
    cutTimes: cutTimes,
    chooseBpm: chooseBpm
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmScore = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
