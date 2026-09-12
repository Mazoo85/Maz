/*
 * SCRIPT FORGE — the sound of the place.
 * --------------------------------------
 * The film had a score, a cut hit, and a voice per character, and no WORLD. The
 * rain fell in silence. Nobody's feet made a sound walking across a car park.
 * A ward, a ship and a forest were all equally quiet, which is a thing the ear
 * notices even when the eye does not — a picture with no room tone under it
 * reads as unfinished in a way most people cannot name.
 *
 * This is not music and it is not the score. The score is SONG FORGE's job and
 * stays that way (see CLAUDE.md); this is the other half of a soundtrack: the
 * bed the picture sits on, and the noises the picture itself makes. It plays
 * into the effects bus, alongside the voices, and is ducked by nothing —
 * ambience under dialogue is how a film sounds, not a problem to be fixed.
 *
 * Everything here is generated: filtered noise, shaped. There are no samples in
 * this repository and there is not going to be a downloader in it either.
 *
 * The decisions — which bed for which set, how hard the rain is, when a foot
 * lands — are pure functions with no audio in them at all, so they are checked
 * in Node like the rest of the film. Only `Ambience` needs a browser.
 *
 * Exposed as window.FilmWorldSound (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* What a room sounds like when nothing is happening in it.
   *
   *   hum    a centre frequency, in Hz, for the resonant part of the bed
   *   level  how loud, against the voices at 1.0
   *   air    how much unfiltered hiss sits over the hum
   *
   * These are the fifteen buildable sets. A set nobody listed falls back to a
   * plain quiet room rather than to silence, because silence is a decision and
   * a missing entry is not.
   */
  var ROOM_TONE = {
    ward:       { hum: 120, level: 0.045, air: 0.30 },   // strip lights and machines
    corridor:   { hum: 96,  level: 0.040, air: 0.26 },
    industrial: { hum: 64,  level: 0.075, air: 0.22 },   // something big, running
    ship:       { hum: 52,  level: 0.085, air: 0.18 },   // engines through a hull
    office:     { hum: 108, level: 0.035, air: 0.30 },
    kitchen:    { hum: 132, level: 0.030, air: 0.34 },
    room:       { hum: 90,  level: 0.022, air: 0.30 },
    bar:        { hum: 84,  level: 0.045, air: 0.38 },
    chapel:     { hum: 72,  level: 0.020, air: 0.16 },   // stone, and a long tail
    vehicle:    { hum: 58,  level: 0.070, air: 0.40 },   // road under the floor
    lighthouse: { hum: 68,  level: 0.055, air: 0.44 },   // wind against glass
    woods:      { hum: 200, level: 0.038, air: 0.62 },   // leaves, high and soft
    field:      { hum: 170, level: 0.032, air: 0.66 },
    street:     { hum: 78,  level: 0.050, air: 0.46 },
    water:      { hum: 88,  level: 0.060, air: 0.58 }
  };

  var DEFAULT_TONE = { hum: 95, level: 0.025, air: 0.30 };

  /* What hangs in the air, as sound. The kinds match film-weather.js exactly,
   * because a picture with rain in it and no rain under it is worse than one
   * with neither.
   *
   *   density  how busy the texture is, 0..1
   *   tilt     the filter's centre, in Hz: rain is bright, fog is not
   *   level    against the room tone
   */
  var WEATHER_SOUND = {
    rain:    { density: 0.85, tilt: 2400, level: 0.085 },
    dust:    { density: 0.25, tilt: 900,  level: 0.022 },
    fog:     { density: 0.10, tilt: 320,  level: 0.030 },
    haze:    { density: 0.12, tilt: 420,  level: 0.024 },
    shimmer: { density: 0.30, tilt: 1400, level: 0.020 },
    embers:  { density: 0.35, tilt: 1800, level: 0.028 },
    none:    { density: 0,    tilt: 800,  level: 0 }
  };

  function clamp01(v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

  function toneFor(setKey) {
    return ROOM_TONE[setKey] || DEFAULT_TONE;
  }

  function weatherSoundFor(kind) {
    return WEATHER_SOUND[kind] || WEATHER_SOUND.none;
  }

  /* How loud the world is in this shot.
   *
   * Two rules, and they are both about the dialogue. A shot with a line in it
   * pulls the world down, because the world is not what anybody is listening to
   * — and a tense shot pushes it up, because a room gets louder when you are
   * frightened of it. Neither is large: a bed that moves a lot stops being a bed
   * and starts being an effect.
   */
  function levelFor(shot) {
    var tension = shot && shot.mood != null ? clamp01(shot.mood) : 0.4;
    var speaking = !!(shot && shot.kind === 'line' && shot.speaker);
    return (speaking ? 0.55 : 1) * (0.8 + tension * 0.45);
  }

  /* -------------------------------------------------------------- footsteps
   *
   * The figures already walk on the 'push' beats, on a stride rate the player
   * owns. Feet have to land on that same stride or the sound is somebody else
   * walking, somewhere else — so this TAKES the rate rather than choosing one.
   */
  /* The stride rate the PLAYER walks figures at (film-player.js WALK_RATE).
   * Kept only as a fallback: footfallsFor takes the rate as an argument and the
   * player passes its own, so the feet cannot drift out of step with the legs by
   * somebody editing one number and not the other. */
  var WALK_RATE = 0.85;

  /* The times, within a shot, at which a foot hits the floor. Two per stride:
   * left, right. Returns an empty list for a shot nobody walks through, which
   * is most of them.
   */
  function footfallsFor(shot, rate) {
    if (!shot || shot.beat !== 'push') return [];
    if (!shot.characters || !shot.characters.length) return [];
    if (shot.framing === 'insert') return [];
    var stride = 1 / (rate || WALK_RATE);
    var out = [];
    // Half a stride apart, and the first one is half a step in rather than on
    // the cut: a foot landing exactly on a cut sounds like the edit, not a foot.
    for (var t = stride * 0.5; t < shot.duration; t += stride * 0.5) {
      out.push({ at: t, foot: out.length % 2 === 0 ? 'left' : 'right' });
    }
    return out;
  }

  /* How a footstep sounds where it lands. Outdoors is softer and duller than a
   * hard floor, which is most of what tells you whether somebody is inside. */
  var STEP_SURFACE = {
    woods:  { cutoff: 700,  level: 0.16, decay: 0.10 },
    field:  { cutoff: 800,  level: 0.14, decay: 0.09 },
    water:  { cutoff: 900,  level: 0.20, decay: 0.14 },
    street: { cutoff: 2600, level: 0.26, decay: 0.07 },
    corridor: { cutoff: 3000, level: 0.30, decay: 0.16 },   // and it rings
    ward:   { cutoff: 2800, level: 0.24, decay: 0.14 },
    industrial: { cutoff: 2200, level: 0.28, decay: 0.18 },
    chapel: { cutoff: 2400, level: 0.22, decay: 0.26 }       // a long tail, in stone
  };

  var DEFAULT_SURFACE = { cutoff: 1800, level: 0.20, decay: 0.08 };

  function surfaceFor(setKey) {
    return STEP_SURFACE[setKey] || DEFAULT_SURFACE;
  }

  /* In a sentence, for the "why" panel. */
  function describe(setKey, weatherKind) {
    var tone = toneFor(setKey);
    var weather = weatherSoundFor(weatherKind);
    var bed = tone.hum < 80 ? 'a low rumble' : (tone.hum > 150 ? 'a high, soft hiss' : 'a steady hum');
    return weather.level > 0
      ? 'The ' + setKey + ' is ' + bed + ' at ' + Math.round(tone.hum) + ' Hz, with ' + weatherKind + ' over it.'
      : 'The ' + setKey + ' is ' + bed + ' at ' + Math.round(tone.hum) + ' Hz.';
  }

  /* ------------------------------------------------------------- the audio
   *
   * One noise source, shaped two ways, plus a filtered click per footfall.
   * Nothing here runs until something calls `new Ambience(...)`, so this file
   * loads in Node.
   */
  function noiseBuffer(ctx, seconds) {
    var frames = Math.floor(ctx.sampleRate * seconds);
    var buffer = ctx.createBuffer(1, frames, ctx.sampleRate);
    var data = buffer.getChannelData(0);
    // A plain xorshift rather than Math.random: a film rebuilt is the same film,
    // and that has to include the hiss.
    var s = 0x9e3779b9;
    for (var i = 0; i < frames; i++) {
      s ^= s << 13; s >>>= 0;
      s ^= s >> 17;
      s ^= s << 5; s >>>= 0;
      data[i] = (s / 4294967296) * 2 - 1;
    }
    return buffer;
  }

  function Ambience(ctx, destination) {
    this.ctx = ctx;
    this.out = ctx.createGain();
    this.out.gain.value = 0;
    this.out.connect(destination);

    this.buffer = noiseBuffer(ctx, 4);

    // The bed: noise through a resonant band, for the room's own note.
    this.bedSource = ctx.createBufferSource();
    this.bedSource.buffer = this.buffer;
    this.bedSource.loop = true;
    this.bedFilter = ctx.createBiquadFilter();
    this.bedFilter.type = 'bandpass';
    this.bedFilter.Q.value = 1.4;
    this.bedGain = ctx.createGain();
    this.bedGain.gain.value = 0;
    this.bedSource.connect(this.bedFilter);
    this.bedFilter.connect(this.bedGain);
    this.bedGain.connect(this.out);

    // The air: the same noise, opened up, for what is hanging in it.
    this.airSource = ctx.createBufferSource();
    this.airSource.buffer = this.buffer;
    this.airSource.loop = true;
    this.airFilter = ctx.createBiquadFilter();
    this.airFilter.type = 'lowpass';
    this.airFilter.frequency.value = 1200;
    this.airGain = ctx.createGain();
    this.airGain.gain.value = 0;
    this.airSource.connect(this.airFilter);
    this.airFilter.connect(this.airGain);
    this.airGain.connect(this.out);

    this.started = false;
    this.scheduled = [];
  }

  Ambience.prototype.start = function () {
    if (this.started) return;
    this.started = true;
    this.bedSource.start();
    this.airSource.start();
    this.out.gain.setValueAtTime(1, this.ctx.currentTime);
  };

  /* Move the bed to this set and this weather. Everything glides: a room tone
   * that steps between scenes is a different sound arriving, not a different
   * room. */
  Ambience.prototype.enter = function (setKey, weatherKind, shot) {
    if (!this.started) return;
    var tone = toneFor(setKey);
    var weather = weatherSoundFor(weatherKind);
    var level = levelFor(shot);
    var now = this.ctx.currentTime;
    var glide = 0.45;

    this.bedFilter.frequency.setTargetAtTime(tone.hum, now, glide);
    this.bedGain.gain.setTargetAtTime(tone.level * level, now, glide);

    // The air carries both the room's own hiss and whatever the weather adds.
    this.airFilter.frequency.setTargetAtTime(weather.tilt, now, glide);
    this.airGain.gain.setTargetAtTime((tone.air * 0.05 + weather.level) * level, now, glide);
  };

  /* One foot, landing. */
  Ambience.prototype.step = function (when, setKey, strength) {
    var ctx = this.ctx;
    var surface = surfaceFor(setKey);
    var src = ctx.createBufferSource();
    src.buffer = this.buffer;
    src.loop = false;
    var filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = surface.cutoff;
    var gain = ctx.createGain();
    var level = surface.level * (strength == null ? 1 : strength);
    gain.gain.setValueAtTime(0, when);
    gain.gain.linearRampToValueAtTime(level, when + 0.006);
    gain.gain.exponentialRampToValueAtTime(0.0001, when + surface.decay);
    src.connect(filter); filter.connect(gain); gain.connect(this.out);
    // Start somewhere random-but-fixed in the buffer so every step is not the
    // same click.
    src.start(when, (when * 7.3) % 3.5, surface.decay + 0.02);
    src.stop(when + surface.decay + 0.05);
  };

  Ambience.prototype.stop = function () {
    if (!this.started) return;
    this.started = false;
    try { this.bedSource.stop(); this.airSource.stop(); } catch (e) { /* already stopped */ }
    try { this.out.disconnect(); } catch (e) { /* already gone */ }
  };

  var API = {
    ROOM_TONE: ROOM_TONE,
    WEATHER_SOUND: WEATHER_SOUND,
    STEP_SURFACE: STEP_SURFACE,
    DEFAULT_TONE: DEFAULT_TONE,
    DEFAULT_SURFACE: DEFAULT_SURFACE,
    WALK_RATE: WALK_RATE,
    toneFor: toneFor,
    weatherSoundFor: weatherSoundFor,
    surfaceFor: surfaceFor,
    levelFor: levelFor,
    footfallsFor: footfallsFor,
    describe: describe,
    Ambience: Ambience
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWorldSound = API;
})(typeof window !== 'undefined' ? window : globalThis);
