/* =============================================================================
 *  NEON CELLS  —  sound
 *
 *  Every sound is synthesised on the spot with WebAudio: no files to load, no
 *  licences, nothing to go missing. Steel is filtered noise, magic is a swept
 *  oscillator, and the music is a slow four-note cycle in the biome's key that
 *  speeds up when you are in trouble.
 * ========================================================================== */
(function (global) {
  'use strict';

  let ctx = null;
  let master = null;
  let musicGain = null;
  let sfxGain = null;
  let enabled = true;
  let muted = false;
  let started = false;
  let musicTimer = null;
  let step = 0;
  let scale = [0, 3, 5, 7, 10];
  let root = 55;
  let intensity = 0;

  function init() {
    if (ctx) return;
    const AC = global.AudioContext || global.webkitAudioContext;
    if (!AC) { enabled = false; return; }
    try {
      ctx = new AC();
    } catch (e) {
      enabled = false;
      return;
    }
    master = ctx.createGain();
    master.gain.value = 0.5;
    master.connect(ctx.destination);

    musicGain = ctx.createGain();
    musicGain.gain.value = 0.16;
    musicGain.connect(master);

    sfxGain = ctx.createGain();
    sfxGain.gain.value = 0.5;
    sfxGain.connect(master);
  }

  function resume() {
    init();
    if (ctx && ctx.state === 'suspended') ctx.resume();
  }

  function ok() {
    return enabled && ctx && !muted;
  }

  function tone(opts) {
    if (!ok()) return;
    const o = ctx.createOscillator();
    const g = ctx.createGain();
    const t = ctx.currentTime;
    o.type = opts.type || 'square';
    o.frequency.setValueAtTime(opts.freq, t);
    if (opts.to) o.frequency.exponentialRampToValueAtTime(Math.max(20, opts.to), t + opts.dur);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(opts.gain || 0.25, t + (opts.attack || 0.005));
    g.gain.exponentialRampToValueAtTime(0.0001, t + opts.dur);
    o.connect(g);
    g.connect(opts.bus || sfxGain);
    o.start(t);
    o.stop(t + opts.dur + 0.03);
  }

  function noise(opts) {
    if (!ok()) return;
    const dur = opts.dur;
    const n = Math.max(1, Math.floor(ctx.sampleRate * dur));
    const buf = ctx.createBuffer(1, n, ctx.sampleRate);
    const data = buf.getChannelData(0);
    const curve = opts.curve || 2;
    for (let i = 0; i < n; i++) {
      data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / n, curve);
    }
    const src = ctx.createBufferSource();
    src.buffer = buf;
    const f = ctx.createBiquadFilter();
    f.type = opts.filter || 'highpass';
    f.frequency.value = opts.cut || 800;
    const g = ctx.createGain();
    g.gain.value = opts.gain || 0.3;
    src.connect(f);
    f.connect(g);
    g.connect(sfxGain);
    src.start();
  }

  const SFX = {
    swing:   () => { noise({ dur: 0.1, gain: 0.16, cut: 2400, curve: 3 }); tone({ freq: 640, to: 300, dur: 0.07, type: 'triangle', gain: 0.1 }); },
    heavy:   () => { noise({ dur: 0.2, gain: 0.22, cut: 900, curve: 2 }); tone({ freq: 150, to: 60, dur: 0.18, type: 'sawtooth', gain: 0.16 }); },
    hit:     () => { noise({ dur: 0.09, gain: 0.3, cut: 500, curve: 4 }); tone({ freq: 210, to: 90, dur: 0.08, type: 'square', gain: 0.16 }); },
    crit:    () => { noise({ dur: 0.14, gain: 0.38, cut: 400, curve: 3 }); tone({ freq: 880, to: 180, dur: 0.16, type: 'sawtooth', gain: 0.2 }); },
    block:   () => { noise({ dur: 0.12, gain: 0.26, cut: 3000, curve: 2 }); tone({ freq: 1200, to: 700, dur: 0.1, type: 'square', gain: 0.12 }); },
    parry:   () => { tone({ freq: 1800, to: 420, dur: 0.22, type: 'square', gain: 0.2 }); tone({ freq: 900, to: 300, dur: 0.2, type: 'triangle', gain: 0.14 }); },
    hurt:    () => { tone({ freq: 320, to: 90, dur: 0.24, type: 'sawtooth', gain: 0.26 }); noise({ dur: 0.18, gain: 0.2, cut: 300 }); },
    jump:    () => tone({ freq: 300, to: 560, dur: 0.09, type: 'square', gain: 0.1 }),
    land:    () => noise({ dur: 0.07, gain: 0.14, cut: 300, curve: 3 }),
    roll:    () => noise({ dur: 0.18, gain: 0.14, cut: 1400, curve: 1.4 }),
    bow:     () => { tone({ freq: 260, to: 700, dur: 0.07, type: 'triangle', gain: 0.12 }); noise({ dur: 0.06, gain: 0.1, cut: 2600 }); },
    bolt:    () => tone({ freq: 1400, to: 300, dur: 0.12, type: 'sawtooth', gain: 0.14 }),
    explode: () => { noise({ dur: 0.5, gain: 0.42, cut: 160, filter: 'lowpass', curve: 1.5 }); tone({ freq: 120, to: 35, dur: 0.4, type: 'sawtooth', gain: 0.24 }); },
    freeze:  () => { tone({ freq: 1500, to: 2600, dur: 0.3, type: 'sine', gain: 0.14 }); noise({ dur: 0.3, gain: 0.12, cut: 4000 }); },
    cell:    () => { tone({ freq: 920, dur: 0.06, type: 'square', gain: 0.12 }); setTimeout(() => tone({ freq: 1380, dur: 0.08, type: 'square', gain: 0.1 }), 55); },
    gold:    () => { tone({ freq: 1180, dur: 0.05, type: 'triangle', gain: 0.1 }); setTimeout(() => tone({ freq: 1560, dur: 0.07, type: 'triangle', gain: 0.09 }), 45); },
    pickup:  () => { tone({ freq: 520, to: 1040, dur: 0.16, type: 'square', gain: 0.14 }); },
    scroll:  () => { [0, 4, 7, 12].forEach((s, i) => setTimeout(() => tone({ freq: 330 * Math.pow(2, s / 12), dur: 0.3, type: 'triangle', gain: 0.14 }), i * 90)); },
    door:    () => { noise({ dur: 0.5, gain: 0.22, cut: 200, filter: 'lowpass', curve: 1 }); tone({ freq: 70, to: 140, dur: 0.5, type: 'sine', gain: 0.16 }); },
    locked:  () => tone({ freq: 180, to: 120, dur: 0.16, type: 'square', gain: 0.16 }),
    buy:     () => { [0, 7, 12].forEach((s, i) => setTimeout(() => tone({ freq: 440 * Math.pow(2, s / 12), dur: 0.14, type: 'square', gain: 0.12 }), i * 70)); },
    levelup: () => { [0, 5, 9, 12, 16].forEach((s, i) => setTimeout(() => tone({ freq: 392 * Math.pow(2, s / 12), dur: 0.32, type: 'triangle', gain: 0.15 }), i * 100)); },
    death:   () => { [0, -3, -7, -12].forEach((s, i) => setTimeout(() => tone({ freq: 220 * Math.pow(2, s / 12), dur: 0.7, type: 'sawtooth', gain: 0.2 }), i * 210)); },
    roar:    () => { tone({ freq: 110, to: 42, dur: 1.3, type: 'sawtooth', gain: 0.3 }); noise({ dur: 1.1, gain: 0.26, cut: 120, filter: 'lowpass', curve: 0.8 }); },
    win:     () => { [0, 4, 7, 12, 16, 19].forEach((s, i) => setTimeout(() => tone({ freq: 523 * Math.pow(2, s / 12), dur: 0.45, type: 'triangle', gain: 0.16 }), i * 130)); }
  };

  function play(name) {
    const fn = SFX[name];
    if (!fn) return;
    try { fn(); } catch (e) { /* a dead audio context must never kill a frame */ }
  }

  /* ------------------------------------------------------------- the music
   * A bass note and a sparse arpeggio, cycling. intensity (0..1) is how much
   * trouble the player is in, and it drives tempo and brightness.
   */
  function startMusic(biomeIndex) {
    init();
    if (!enabled) return;
    stopMusic();
    started = true;
    const modes = [
      [0, 2, 3, 7, 8],    // phrygian-ish, the prison
      [0, 2, 4, 7, 9],    // major pentatonic, the open promenade
      [0, 1, 5, 6, 10],   // sour, the sewers
      [0, 3, 5, 6, 10],   // the ossuary
      [0, 2, 3, 5, 8],    // ramparts
      [0, 1, 4, 6, 9]     // boss
    ];
    scale = modes[Math.min(modes.length - 1, biomeIndex || 0)];
    root = 55 * Math.pow(2, ((biomeIndex || 0) % 3) / 12);
    step = 0;
    tick();
  }

  function tick() {
    if (!started || !enabled || !ctx) return;
    const beat = 0.46 - 0.12 * intensity;

    if (!muted) {
      const bass = root * Math.pow(2, scale[step % scale.length] / 12);
      tone({ freq: bass, dur: beat * 1.9, type: 'triangle', gain: 0.16, bus: musicGain, attack: 0.02 });
      if (step % 2 === 0) {
        const lead = bass * 4 * Math.pow(2, scale[(step * 3) % scale.length] / 12);
        tone({ freq: lead, dur: beat * 0.8, type: 'square', gain: 0.05 + 0.05 * intensity, bus: musicGain });
      }
      if (intensity > 0.4 && step % 4 === 2) {
        noise({ dur: 0.14, gain: 0.08 * intensity, cut: 1800, curve: 3 });
      }
    }

    step++;
    musicTimer = global.setTimeout(tick, beat * 1000);
  }

  function stopMusic() {
    started = false;
    if (musicTimer) global.clearTimeout(musicTimer);
    musicTimer = null;
  }

  function setIntensity(v) {
    intensity = Math.max(0, Math.min(1, v));
  }

  function toggleMute() {
    muted = !muted;
    if (master) master.gain.value = muted ? 0 : 0.5;
    return muted;
  }

  function isMuted() {
    return muted;
  }

  const API = {
    init: init,
    resume: resume,
    play: play,
    startMusic: startMusic,
    stopMusic: stopMusic,
    setIntensity: setIntensity,
    toggleMute: toggleMute,
    isMuted: isMuted
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_AUDIO = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
