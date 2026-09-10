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

  var API = { MUSIC_FOR: MUSIC_FOR };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmScore = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
