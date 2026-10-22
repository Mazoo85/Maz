/*
 * The sound of the city, made on the fly with Web Audio (no sound files):
 * the low hum of traffic, a siren somewhere now and then, birdsong by day,
 * the wild parrots' squawks when their flock passes, and the beat of the
 * LAPD helicopter's rotor when it's close.
 *
 * Browsers only allow sound after the player has touched the page, so it
 * starts on the first click, tap or key.
 */

export function makeAudio({ enabled = true } = {}) {
  let ctx = null, master = null, hum = null, humGain = null, heliGain = null, heliLfo = null, on = enabled;
  let nextSiren = 30 + Math.random() * 40, nextChirp = 3, t = 0;

  function start() {
    if (ctx || !on) return;
    try { ctx = new AudioContext(); } catch { return; }
    master = ctx.createGain(); master.gain.value = 0.9; master.connect(ctx.destination);
    // brown noise: the city's hum
    const len = ctx.sampleRate * 4, buf = ctx.createBuffer(1, len, ctx.sampleRate), d = buf.getChannelData(0);
    let last = 0;
    for (let i = 0; i < len; i++) { last = (last + 0.02 * (Math.random() * 2 - 1)) / 1.02; d[i] = last * 3.5; }
    hum = ctx.createBufferSource(); hum.buffer = buf; hum.loop = true;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 420;
    humGain = ctx.createGain(); humGain.gain.value = 0.05;
    hum.connect(lp).connect(humGain).connect(master); hum.start();
    // the helicopter: the same noise, band-passed, chopped by the rotor
    const heli = ctx.createBufferSource(); heli.buffer = buf; heli.loop = true;
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 180; bp.Q.value = 0.8;
    const chop = ctx.createGain(); chop.gain.value = 0.5;
    heliLfo = ctx.createOscillator(); heliLfo.frequency.value = 11.5;
    const lfoAmt = ctx.createGain(); lfoAmt.gain.value = 0.5;
    heliLfo.connect(lfoAmt).connect(chop.gain);
    heliGain = ctx.createGain(); heliGain.gain.value = 0;
    heli.connect(bp).connect(chop).connect(heliGain).connect(master); heli.start(); heliLfo.start();
  }
  for (const ev of ['pointerdown', 'keydown', 'touchstart']) window.addEventListener(ev, start, { once: false, passive: true });

  function siren() {
    const now = ctx.currentTime, dur = 5 + Math.random() * 4;
    const o = ctx.createOscillator(), g = ctx.createGain(), pan = ctx.createStereoPanner();
    o.type = 'sawtooth';
    for (let k = 0; k < dur / 1.6; k++) { o.frequency.setValueAtTime(650, now + k * 1.6); o.frequency.linearRampToValueAtTime(1350, now + k * 1.6 + 0.8); o.frequency.linearRampToValueAtTime(650, now + k * 1.6 + 1.6); }
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 1800;
    g.gain.setValueAtTime(0, now); g.gain.linearRampToValueAtTime(0.012, now + 1.5); g.gain.linearRampToValueAtTime(0, now + dur);
    pan.pan.value = Math.random() * 2 - 1;
    o.connect(lp).connect(g).connect(pan).connect(master); o.start(now); o.stop(now + dur);
  }
  function chirp(kind) {
    const now = ctx.currentTime, n = kind === 'parrot' ? 3 : 2 + Math.floor(Math.random() * 4);
    const pan = ctx.createStereoPanner(); pan.pan.value = Math.random() * 1.6 - 0.8; pan.connect(master);
    for (let k = 0; k < n; k++) {
      const o = ctx.createOscillator(), g = ctx.createGain(), t0 = now + k * (kind === 'parrot' ? 0.22 : 0.12);
      o.type = kind === 'parrot' ? 'square' : 'sine';
      const f = kind === 'parrot' ? 1400 + Math.random() * 500 : 2600 + Math.random() * 1800;
      o.frequency.setValueAtTime(f, t0); o.frequency.exponentialRampToValueAtTime(f * (kind === 'parrot' ? 0.6 : 1.35), t0 + 0.09);
      g.gain.setValueAtTime(0, t0); g.gain.linearRampToValueAtTime(kind === 'parrot' ? 0.01 : 0.007, t0 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t0 + 0.1);
      o.connect(g).connect(pan); o.start(t0); o.stop(t0 + 0.12);
    }
  }

  return {
    setEnabled(v) { on = v; if (master) master.gain.value = v ? 0.9 : 0; if (v) start(); },
    /* night 0..1; heliDist in metres (or Infinity); parrotsNear: a flock overhead */
    update(dt, { night = 0, heliDist = Infinity, parrotsNear = false } = {}) {
      if (!ctx || !on) return;
      t += dt;
      humGain.gain.value = 0.05 - 0.02 * night;
      heliGain.gain.value = heliDist < 900 ? 0.09 * Math.pow(1 - heliDist / 900, 2) : 0;
      if ((nextSiren -= dt) < 0) { siren(); nextSiren = 45 + Math.random() * 90; }
      if ((nextChirp -= dt) < 0) {
        if (parrotsNear) chirp('parrot'); else if (night < 0.3) chirp('bird');
        nextChirp = parrotsNear ? 0.6 + Math.random() : 2 + Math.random() * 6;
      }
    }
  };
}
