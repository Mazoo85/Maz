/*
 * SONG FORGE — the soundtrack surface other projects use.
 * ------------------------------------------------------
 * Published as `music/soundtrack` in shared/exchange.json.
 *
 * SONG FORGE's own page is a studio: compose, inspect, re-roll a part, export.
 * A game wants none of that. It wants "play something that suits this, and
 * change it when the action changes", and it wants that through the audio
 * context and the volume control it already has, so its own mute button still
 * works and its sound effects and the music cannot fight each other.
 *
 * So this is deliberately small. It composes with Composer, plays with
 * Engine.Player, and exposes six methods. Everything else SONG FORGE can do
 * stays on SONG FORGE's page.
 *
 * Why a game should want it: ZOMBOID looped an eight-step bassline against an
 * eight-step lead, and DEAD SECTOR walked a six-note scale. One fixed pattern
 * each, repeating for as long as you play, knowing nothing about the game. A
 * composed song is minutes long, arranged into sections, and can be swapped for
 * another in the same genre when the mood of the game changes — which costs
 * 1-14ms, measured, so it can happen mid-play without dropping a frame.
 *
 * Exposed as window.MazSoundtrack (and module.exports for tests).
 */
(function (root) {
  'use strict';

  var Composer = root.Composer ||
    (typeof require !== 'undefined' ? require('./composer.js') : null);
  var Engine = root.Engine ||
    (typeof require !== 'undefined' ? require('./engine.js') : null);
  var Genres = root.Genres ||
    (typeof require !== 'undefined' ? require('./genres.js') : null);

  /* Seconds to fade out the old track and in the new one on a mood change.
   * Long enough not to sound like a cut, short enough that the music has caught
   * up with the game before the player wonders why it hasn't. */
  var CROSSFADE = 1.2;

  function genreExists(id) {
    return Boolean(Genres && Genres.GENRES && Genres.GENRES[id]);
  }

  function moodExists(id) {
    return Boolean(Genres && Genres.MOODS && Genres.MOODS[id]);
  }

  /** Every genre and mood a caller may ask for, so a game can offer a choice. */
  function options() {
    return {
      genres: Genres && Genres.GENRES ? Object.keys(Genres.GENRES) : [],
      moods: Genres && Genres.MOODS ? Object.keys(Genres.MOODS) : []
    };
  }

  /**
   * A soundtrack for one game.
   *
   * opts:
   *   context     an AudioContext to share with the game's own sound effects.
   *               Omitted, one is made on the first start().
   *   destination a node to play into — pass the game's music bus and its
   *               existing volume and mute controls keep working untouched.
   *   genre       one of options().genres. An unknown one is ignored rather
   *               than thrown: a game should never crash over its background
   *               music.
   *   mood        one of options().moods.
   *   volume      0..1, applied on top of whatever `destination` does.
   *   seed        fixed seed for a reproducible track, mostly for tests.
   */
  function create(opts) {
    opts = opts || {};

    var state = {
      genre: genreExists(opts.genre) ? opts.genre : 'chiptune',
      mood: moodExists(opts.mood) ? opts.mood : 'driving',
      volume: typeof opts.volume === 'number' ? opts.volume : 0.5,
      seed: opts.seed || null
    };

    var ctx = opts.context || null;
    var destination = opts.destination || null;
    var gain = null;        // our own fader, between the player and `destination`
    var player = null;
    var song = null;
    var playing = false;

    function ensureContext() {
      if (ctx) return ctx;
      var AC = root.AudioContext || root.webkitAudioContext;
      if (!AC) return null;          // no WebAudio: every method below no-ops
      ctx = new AC();
      return ctx;
    }

    function ensureGain() {
      if (gain || !ctx) return gain;
      gain = ctx.createGain();
      gain.gain.value = state.volume;
      gain.connect(destination || ctx.destination);
      return gain;
    }

    function composeCurrent() {
      if (!Composer) return null;
      return Composer.compose({
        genre: state.genre,
        mood: state.mood,
        seed: state.seed || undefined,
        length: 'long'          // a game plays for a while; give it room before it repeats
      });
    }

    function startPlayer(fromGain) {
      if (!Engine || !ctx) return false;
      ensureGain();
      song = composeCurrent();
      if (!song) return false;
      player = new Engine.Player({ context: ctx, destination: gain });
      player.loop = true;
      player.load(song);
      if (typeof fromGain === 'number' && gain) {
        // Coming in from a crossfade: start silent and rise.
        gain.gain.cancelScheduledValues(ctx.currentTime);
        gain.gain.setValueAtTime(fromGain, ctx.currentTime);
        gain.gain.linearRampToValueAtTime(state.volume, ctx.currentTime + CROSSFADE / 2);
      }
      player.play();
      playing = true;
      return true;
    }

    function stopPlayer() {
      if (player) player.stop();
      player = null;
      playing = false;
    }

    return {
      /** Begin playing. Safe to call more than once; safe with no WebAudio. */
      start: function () {
        if (playing) return true;
        if (!ensureContext()) return false;
        if (ctx.state === 'suspended' && ctx.resume) ctx.resume();
        return startPlayer();
      },

      /** Stop, releasing the scheduler. start() composes a fresh track. */
      stop: function () {
        stopPlayer();
      },

      /**
       * Move the music to a different mood — the one call a game makes while
       * it is running. Composes a new track in the same genre and crossfades.
       * Asking for the mood already playing does nothing, so this is safe to
       * call every frame from game state without thinking about it.
       */
      setMood: function (mood) {
        if (!moodExists(mood) || mood === state.mood) return false;
        state.mood = mood;
        if (!playing) return true;
        var from = gain ? gain.gain.value : state.volume;
        if (gain && ctx) {
          gain.gain.cancelScheduledValues(ctx.currentTime);
          gain.gain.setValueAtTime(from, ctx.currentTime);
          gain.gain.linearRampToValueAtTime(0.0001, ctx.currentTime + CROSSFADE / 2);
        }
        stopPlayer();
        startPlayer(0.0001);
        return true;
      },

      /** Change genre. Restarts the track if one is playing. */
      setGenre: function (genre) {
        if (!genreExists(genre) || genre === state.genre) return false;
        state.genre = genre;
        if (playing) { stopPlayer(); startPlayer(); }
        return true;
      },

      setVolume: function (v) {
        state.volume = Math.max(0, Math.min(1, Number(v) || 0));
        if (gain && ctx) gain.gain.setValueAtTime(state.volume, ctx.currentTime);
        return state.volume;
      },

      isPlaying: function () { return playing; },

      /** What is playing now — genre, mood, and the title SONG FORGE gave it. */
      current: function () {
        return {
          genre: state.genre,
          mood: state.mood,
          volume: state.volume,
          title: song ? song.title : null,
          bpm: song ? song.bpm : null,
          playing: playing
        };
      }
    };
  }

  var API = { create: create, options: options, CROSSFADE: CROSSFADE };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.MazSoundtrack = API;
})(typeof window !== 'undefined' ? window : this);
