/*
 * SCRIPT FORGE — the score.
 * -------------------------
 * Two buses feed the master: a music bus, which SONG FORGE composes and plays
 * into via `startScore()`, and an effects bus carrying everything the film
 * makes itself — a pulse that arrives when the story gets tense, a hit on
 * every cut between scenes, and a voice per character — pitched blips, the
 * way a game speaks, so the dialogue has a rhythm and a register without a
 * voice actor. The music bus is ducked under the dialogue by `applyDuck()`.
 *
 * The master feeds the speakers *and* a MediaStreamDestination, so the
 * recorded film has exactly the sound you heard.
 *
 *   var score = new FilmScore.Score(reel);
 *   score.start(); score.startScore(reel);
 *   score.enterShot(shot); score.tick(timeInFilm); score.stop();
 *
 * Exposed as window.FilmScore. Nothing runs at load, so this file is safe to
 * require in Node — only `new Score()` needs a browser.
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});
  var WEATHER = root.FilmWeather || (typeof require !== 'undefined' ? require('./film-weather.js') : {});
  var WORLD = root.FilmWorldSound || (typeof require !== 'undefined' ? require('./world-sound.js') : {});

  /* Which genres keep a pulse going under the picture (read by `tick`). The
   * fast, driving ones do; the ones that live on silence do not, and only get
   * one when the scene itself turns tense. */
  var MUSIC = {
    drama:    { pulse: false },
    thriller: { pulse: true },
    horror:   { pulse: false },
    comedy:   { pulse: true },
    romance:  { pulse: false },
    scifi:    { pulse: true },
    mystery:  { pulse: false },
    fantasy:  { pulse: false },
    heist:    { pulse: true },
    western:  { pulse: false }
  };

  var MUSIC_LEVEL = 0.55;   // where the score sits under the dialogue

  function audioContextClass() {
    return root.AudioContext || root.webkitAudioContext || null;
  }

  function supported() {
    return !!audioContextClass();
  }

  function Score(reel, opts) {
    opts = opts || {};
    var Ctx = audioContextClass();
    if (!Ctx) throw new Error('This browser has no Web Audio.');

    this.reel = reel;
    this.music = MUSIC[reel.genre] || MUSIC.drama;
    this.ctx = opts.context || new Ctx();

    this.master = this.ctx.createGain();
    this.master.gain.value = opts.volume == null ? 0.85 : opts.volume;

    // Everything is heard *and* recorded from the same node.
    this.speakers = this.ctx.createGain();
    this.speakers.gain.value = 1;
    this.master.connect(this.speakers);
    this.speakers.connect(this.ctx.destination);

    this.streamDestination = this.ctx.createMediaStreamDestination
      ? this.ctx.createMediaStreamDestination()
      : null;
    if (this.streamDestination) this.master.connect(this.streamDestination);

    // Two buses: the score, and everything the film makes itself. Both feed the
    // master, which already reaches the speakers and the recorder.
    this.musicBus = this.ctx.createGain();
    this.musicBus.gain.value = MUSIC_LEVEL;
    this.musicBus.connect(this.master);

    this.effectsBus = this.ctx.createGain();
    this.effectsBus.gain.value = 1;
    this.effectsBus.connect(this.master);

    this.player = null;          // SONG FORGE's Player, once a score exists
    this.usingRealScore = false;
    this.duckPoints = null;

    this.started = false;
    this.pulseNext = 0;
    this.currentShot = null;
    this.blipTimers = [];

    // The sound of the place: a room tone per set, whatever is hanging in the
    // air, and feet. Not music, and not the score -- the score is SONG FORGE's
    // job and stays that way. This is the bed the picture sits on, and it goes
    // on the effects bus next to the voices.
    this.world = null;
    this.walkRate = opts.walkRate || null;
  }

  Score.prototype.start = function () {
    if (this.started) return;
    this.started = true;
    if (this.ctx.state === 'suspended' && this.ctx.resume) this.ctx.resume();
    if (!this.world && WORLD.Ambience) {
      this.world = new WORLD.Ambience(this.ctx, this.effectsBus);
      this.world.start();
    }
  };

  /* Called when the film cuts to a new shot. */
  Score.prototype.enterShot = function (shot, timeInFilm) {
    if (!this.started) return;
    var tension = shot.mood == null ? 0.4 : shot.mood;

    // A new scene gets a soft hit, so cuts land.
    if (!this.currentShot || this.currentShot.scene !== shot.scene) {
      this.hit(0.5 + tension * 0.5);
    }

    // ...and so does a turn of the object's arc, which is the film reacting to
    // what is actually on screen rather than to the clock. This lives on the
    // effects bus on purpose: making the MUSIC react would mean composing in
    // here, and the music is SONG FORGE's job (CLAUDE.md).
    if (shot.objectBeat && (!this.currentShot || this.currentShot.objectBeat !== shot.objectBeat)) {
      this.hit(0.32 + tension * 0.3);
    }

    this.currentShot = shot;

    // Move the world to this room, and walk anybody who is walking.
    if (this.world) {
      var weatherKind = WEATHER.forShot
        ? WEATHER.forShot(this.reel.genre, shot.time, shot.set)
        : 'none';
      this.world.enter(shot.set, weatherKind, shot);
      var now = this.ctx.currentTime;
      var setKey = shot.set;
      var self = this;
      WORLD.footfallsFor(shot, this.walkRate).forEach(function (fall) {
        // Left and right are not the same weight -- everybody favours a side,
        // and identical footfalls read as a metronome rather than a person.
        self.world.step(now + fall.at, setKey, fall.foot === 'left' ? 1 : 0.86);
      });
    }

    // Speak the line.
    this.clearBlips();
    if (shot.kind === 'line' && shot.speaker) {
      this.speak(shot.caption, this.reel.voices[shot.speaker], shot.duration);
    }
  };

  /* A low hit plus a breath of noise — the sound of a cut. */
  Score.prototype.hit = function (strength) {
    var now = this.ctx.currentTime;
    var osc = this.ctx.createOscillator();
    var gain = this.ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(120, now);
    osc.frequency.exponentialRampToValueAtTime(38, now + 0.5);
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(0.18 * strength, now + 0.02);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.9);
    osc.connect(gain);
    gain.connect(this.effectsBus);
    osc.start(now);
    osc.stop(now + 1.0);

    var noise = this.noiseBurst(0.5, 0.10 * strength, 1400);
    if (noise) noise.start(now);
  };

  Score.prototype.noiseBurst = function (seconds, level, cutoff) {
    var ctx = this.ctx;
    var frames = Math.floor(ctx.sampleRate * seconds);
    if (!frames) return null;
    var buffer = ctx.createBuffer(1, frames, ctx.sampleRate);
    var data = buffer.getChannelData(0);
    for (var i = 0; i < frames; i++) {
      data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / frames, 2.5);
    }
    var src = ctx.createBufferSource();
    src.buffer = buffer;
    var filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = cutoff || 1200;
    var gain = ctx.createGain();
    gain.gain.value = level;
    src.connect(filter);
    filter.connect(gain);
    gain.connect(this.effectsBus);
    return src;
  };

  /* ------------------------------------------------------------- voices
   * One blip per syllable-ish, at the character's own pitch, riding a little
   * up and down so a line has shape instead of being a flat beep. */
  /* Vowels, as the two resonances that distinguish them.
   *
   * A voice is a buzz at the speaker's pitch, filtered by the shape of the
   * mouth making it. Those two peaks — the first and second formants — are most
   * of what tells one vowel from another, and they sit at roughly the same
   * frequencies whatever the speaker's pitch, which is why a low voice and a
   * high one saying "ee" both sound like "ee".
   *
   * Approximate mid-range adult values in Hz. Precision is not the point: the
   * point is that "I can't" and "Say it" stop coming out identical.
   */
  var FORMANTS = {
    a: [730, 1090],   // father
    e: [530, 1840],   // bed
    i: [270, 2290],   // see
    o: [570, 840],    // law
    u: [300, 870]     // boot
  };

  Score.prototype.speak = function (text, voice, seconds) {
    if (!voice) return;
    var syllables = PARSE.syllablesFor(text);
    var vowels = PARSE.vowelsFor(text, syllables);
    var span = Math.max(0.4, (seconds || 2) * 0.78);
    var gap = span / syllables;
    var now = this.ctx.currentTime;

    for (var i = 0; i < syllables; i++) {
      this.blip(now + i * gap, voice, i / syllables, vowels[i]);
    }
  };

  Score.prototype.blip = function (when, voice, through, vowel) {
    var ctx = this.ctx;
    var pair = FORMANTS[vowel] || FORMANTS.a;
    var osc = ctx.createOscillator();
    var gain = ctx.createGain();

    // Two resonances in parallel over one buzz — the mouth shape, not a single
    // fixed bandpass. The second is quieter, as it is in a real voice.
    var f1 = ctx.createBiquadFilter();
    f1.type = 'bandpass';
    f1.frequency.value = pair[0];
    f1.Q.value = 6;
    var f2 = ctx.createBiquadFilter();
    f2.type = 'bandpass';
    f2.frequency.value = pair[1];
    f2.Q.value = 9;
    var g2 = ctx.createGain();
    g2.gain.value = 0.55;

    // A statement falls at the end; a question rises. Cheap, and it works.
    var contour = 1 + Math.sin(through * Math.PI) * 0.10 - through * 0.06;
    // A sawtooth has the harmonics the formants need something to bite on; a
    // square is hollow between them and the vowel does not come through.
    osc.type = 'sawtooth';
    osc.frequency.value = voice.pitch * contour;

    gain.gain.setValueAtTime(0.0001, when);
    gain.gain.exponentialRampToValueAtTime(0.20, when + 0.018);
    gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.105);

    osc.connect(f1);
    osc.connect(f2);
    f2.connect(g2);
    f1.connect(gain);
    g2.connect(gain);
    gain.connect(this.effectsBus);
    osc.start(when);
    osc.stop(when + 0.14);
    this.blipTimers.push(osc);
  };

  Score.prototype.clearBlips = function () {
    var now = this.ctx.currentTime;
    this.blipTimers.forEach(function (osc) {
      try { osc.stop(now); } catch (e) { /* already stopped */ }
    });
    this.blipTimers = [];
  };

  /* Called every frame: keeps the pulse going under the tense stretches. */
  Score.prototype.tick = function (timeInFilm) {
    if (!this.started || !this.currentShot) return;
    var tension = this.currentShot.mood == null ? 0.4 : this.currentShot.mood;
    if (!this.music.pulse && tension < 0.55) return;

    var period = 0.75 - tension * 0.22;
    if (timeInFilm < this.pulseNext) return;
    this.pulseNext = timeInFilm + period;

    var now = this.ctx.currentTime;
    var osc = this.ctx.createOscillator();
    var gain = this.ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(96, now);
    osc.frequency.exponentialRampToValueAtTime(48, now + 0.14);
    gain.gain.setValueAtTime(0.0001, now);
    gain.gain.exponentialRampToValueAtTime(0.10 + tension * 0.10, now + 0.01);
    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.28);
    osc.connect(gain);
    gain.connect(this.effectsBus);
    osc.start(now);
    osc.stop(now + 0.3);
  };

  /* Ask SONG FORGE for a score for this film and start it playing into the
   * music bus. Returns true if a real score is playing, false if the film is
   * carrying on without one. */
  Score.prototype.startScore = function (reel) {
    var Forge = root.Composer, Play = root.Engine, Conductor = root.FilmConductor;
    if (!Forge || !Play || !Conductor) return false;

    try {
      var genreRange = root.Genres && root.Genres.GENRES[Conductor.MUSIC_FOR[reel.genre].genre];
      var req = Conductor.request(reel, { bpmRange: genreRange && genreRange.bpm });
      var song = Forge.compose({
        genre: req.genre, mood: req.mood, seed: req.seed,
        bpm: req.bpm, seconds: req.seconds, sections: req.sections
      });

      this.player = new Play.Player({ context: this.ctx, destination: this.musicBus });
      this.player.loop = false;
      this.player.load(song);
      this.duckPoints = Conductor.duckEnvelope(reel);
      this.usingRealScore = true;
      return true;
    } catch (e) {
      this.player = null;
      this.usingRealScore = false;
      return false;
    }
  };

  /* The duck envelope is a list of level changes in *film* time. Scheduling is
   * in audio-context time, so it is laid down relative to where playback is
   * starting from — and re-laid every time the film plays or is scrubbed,
   * otherwise a scrub leaves the ducking pointing at the wrong moments.
   *
   * Every transition is *anchored*. A `linearRampToValueAtTime` on its own
   * ramps from the previous automation event, however long ago that was, so a
   * bare list of ramps makes the level glide continuously: down across the
   * whole gap before a line and back up across the line itself — quietest the
   * instant someone starts speaking, loudest by the time they finish, which is
   * the opposite of ducking. Pinning the held level with `setValueAtTime` at
   * the moment each ramp begins gives the shape the film wants: flat at full
   * between lines, a DUCK_LEAD dip into each line, flat and low through it, a
   * DUCK_TAIL rise after it. */
  Score.prototype.applyDuck = function (fromFilmSeconds) {
    var Conductor = root.FilmConductor;
    if (!this.duckPoints || !Conductor) return;
    var bus = this.musicBus.gain;
    var base = MUSIC_LEVEL;
    var now = this.ctx.currentTime;
    var offset = fromFilmSeconds || 0;
    var points = this.duckPoints;
    var i, point;

    bus.cancelScheduledValues(now);

    // Start at whatever the level should be at this moment in the film — the
    // film can be played from the middle of a line, and that starts ducked.
    var level = base;
    for (i = 0; i < points.length; i++) {
      if (points[i].t <= offset) level = base * points[i].gain;
    }
    bus.setValueAtTime(level, now);

    for (i = 0; i < points.length; i++) {
      point = points[i];
      if (point.t <= offset) continue;
      var target = base * point.gain;
      // Dropping into a line takes DUCK_LEAD; coming back out takes DUCK_TAIL.
      var startAt = now + (point.t - offset);
      var endAt = startAt + (point.gain < 1 ? Conductor.DUCK_LEAD : Conductor.DUCK_TAIL);
      // Two lines can sit close enough that the rise after the first is still
      // climbing when the next drop is due. The next transition always wins.
      var next = points[i + 1];
      if (next) endAt = Math.min(endAt, now + (next.t - offset));

      // Hold the level we have been sitting at, *then* move.
      bus.setValueAtTime(level, startAt);
      if (endAt > startAt) bus.linearRampToValueAtTime(target, endAt);
      else bus.setValueAtTime(target, startAt);
      level = target;
    }
  };

  Score.prototype.stop = function () {
    if (this.world) { this.world.stop(); this.world = null; }
    // The music player is stopped unconditionally: `close()` calls through
    // here, and a score that was never `start()`ed can still have a player
    // loaded and running — an early return would leave it playing.
    if (this.player) this.player.stop();
    if (!this.started) return;
    this.clearBlips();
    this.started = false;
    this.currentShot = null;
    this.pulseNext = 0;
  };

  Score.prototype.close = function () {
    this.stop();
    var ctx = this.ctx;
    setTimeout(function () {
      if (ctx.close) ctx.close();
    }, 1000);
  };

  /* Silence the speakers without silencing the recording — used when someone
   * records a film and would rather not sit through it out loud. */
  Score.prototype.mute = function (muted) {
    this.speakers.gain.value = muted ? 0 : 1;
  };

  var API = { Score: Score, MUSIC: MUSIC, supported: supported, FORMANTS: FORMANTS };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmScore = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
