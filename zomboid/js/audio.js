/* =============================================================================
 *  ZOMBOID: ANCHORAGE  —  AUDIO  (WebAudio chiptune / Genesis-FM flavored SFX)
 * ========================================================================== */
(function (global) {
  'use strict';

  let ctx = null;
  let master = null;
  let musicGain = null;
  let enabled = true;

  function init() {
    if (ctx) return;
    const AC = global.AudioContext || global.webkitAudioContext;
    if (!AC) { enabled = false; return; }
    ctx = new AC();
    master = ctx.createGain();
    master.gain.value = 0.45;
    master.connect(ctx.destination);
    musicGain = ctx.createGain();
    musicGain.gain.value = 0.18;
    musicGain.connect(master);
  }

  function resume() { if (ctx && ctx.state === 'suspended') ctx.resume(); }

  function blip(freq, dur, type, gain, slideTo) {
    if (!enabled || !ctx) return;
    const o = ctx.createOscillator();
    const g = ctx.createGain();
    o.type = type || 'square';
    o.frequency.setValueAtTime(freq, ctx.currentTime);
    if (slideTo) o.frequency.exponentialRampToValueAtTime(slideTo, ctx.currentTime + dur);
    g.gain.setValueAtTime(0.0001, ctx.currentTime);
    g.gain.exponentialRampToValueAtTime(gain || 0.3, ctx.currentTime + 0.005);
    g.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + dur);
    o.connect(g); g.connect(master);
    o.start(); o.stop(ctx.currentTime + dur + 0.02);
  }

  function noise(dur, gain, hp) {
    if (!enabled || !ctx) return;
    const n = Math.floor(ctx.sampleRate * dur);
    const buf = ctx.createBuffer(1, n, ctx.sampleRate);
    const d = buf.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / n);
    const src = ctx.createBufferSource(); src.buffer = buf;
    const g = ctx.createGain(); g.gain.value = gain || 0.3;
    const f = ctx.createBiquadFilter(); f.type = 'highpass'; f.frequency.value = hp || 600;
    src.connect(f); f.connect(g); g.connect(master);
    src.start();
  }

  const SFX = {
    hit:    () => { noise(0.12, 0.4, 400); blip(140, 0.1, 'square', 0.25, 60); },
    swing:  () => blip(520, 0.08, 'triangle', 0.18, 240),
    gun:    () => { noise(0.18, 0.6, 200); blip(90, 0.14, 'sawtooth', 0.4, 40); },
    shotgun:() => { noise(0.32, 0.7, 120); blip(70, 0.2, 'sawtooth', 0.5, 30); },
    pickup: () => { blip(660, 0.06, 'square', 0.25); setTimeout(() => blip(990, 0.08, 'square', 0.25), 60); },
    open:   () => blip(330, 0.1, 'square', 0.2, 520),
    hurt:   () => { blip(220, 0.18, 'sawtooth', 0.35, 110); noise(0.1, 0.3, 300); },
    death:  () => { blip(330, 0.5, 'sawtooth', 0.4, 60); noise(0.4, 0.3, 200); },
    eat:    () => blip(300, 0.12, 'sine', 0.2, 360),
    drink:  () => blip(420, 0.12, 'sine', 0.2, 300),
    select: () => blip(740, 0.06, 'square', 0.25),
    start:  () => { [523, 659, 784, 1046].forEach((f, i) => setTimeout(() => blip(f, 0.12, 'square', 0.3), i * 90)); },
    zgroan: () => blip(120 + Math.random() * 40, 0.4, 'sawtooth', 0.12, 70),
    sega:   () => {
      // mock "SEEE-GAAA" rising chord
      [262, 330, 392].forEach((f) => blip(f, 0.9, 'sawtooth', 0.18, f * 1.5));
      setTimeout(() => [392, 494, 587].forEach((f) => blip(f, 0.7, 'square', 0.16)), 450);
    },
  };

  /* ---- the city's soundtrack, written by SONG FORGE ----
   *
   * This used to be an eight-step bassline against an eight-step lead, looping
   * for as long as you played and knowing nothing about the game. SONG FORGE
   * already writes whole arranged songs in the browser with no dependencies, so
   * ZOMBOID consumes it (declared as music/soundtrack in shared/exchange.json)
   * and gets music that does not repeat and follows the night and the horde.
   *
   * It plays into `musicGain`, the same bus the old loop used, so the L key and
   * every volume decision in this file still apply untouched — and the sound
   * effects above, which are ZOMBOID's own character, are not touched at all.
   *
   * The genre is chiptune, which is what the old loop was reaching for.
   */
  let soundtrack = null;

  function startMusic() {
    if (!enabled || !ctx) return;
    if (!global.MazSoundtrack) return;   // the page failed to load it: play on, silently
    if (!soundtrack) {
      soundtrack = global.MazSoundtrack.create({
        context: ctx,
        destination: musicGain,
        genre: 'chiptune',
        mood: 'dark',
        volume: 1        // musicGain already sets the level for this bus
      });
    }
    soundtrack.start();
  }

  function stopMusic() {
    if (soundtrack) soundtrack.stop();
  }

  /* Called from the world update every frame. `setMood` ignores the mood it is
   * already playing, so the caller does not have to track what changed. */
  function setMusicMood(mood) {
    if (soundtrack) soundtrack.setMood(mood);
  }

  function nowPlaying() {
    return soundtrack ? soundtrack.current() : null;
  }

  function toggle() { enabled = !enabled; if (!enabled) stopMusic(); else startMusic(); return enabled; }

  global.AUDIO = {
    init, resume, SFX, startMusic, stopMusic, setMusicMood, nowPlaying, toggle,
    isEnabled: () => enabled
  };
})(typeof window !== 'undefined' ? window : this);
