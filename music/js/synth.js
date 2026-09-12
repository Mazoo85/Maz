/*
 * synth.js — every sound in the app is generated from scratch with the Web
 * Audio API. No samples, no downloads, nothing to load.
 *
 * Each function takes an AudioContext so the exact same code can run live
 * (AudioContext) or faster-than-realtime for export (OfflineAudioContext).
 */
(function (global) {
  'use strict';

  const EPS = 0.0001;

  /* ------------------------------------------------------------------ *
   * Shared per-context resources
   * ------------------------------------------------------------------ */

  function noiseBuffer(ctx) {
    if (ctx._mazNoise) return ctx._mazNoise;
    const len = Math.floor(ctx.sampleRate * 2);
    const buf = ctx.createBuffer(1, len, ctx.sampleRate);
    const d = buf.getChannelData(0);
    let seed = 12345;
    for (let i = 0; i < len; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7fffffff;
      d[i] = (seed / 0x3fffffff) - 1;
    }
    ctx._mazNoise = buf;
    return buf;
  }

  /**
   * A room tail built from decaying noise, in four shapes.
   *
   * `room` is the plain one. `gated` is the same tail cut off part way through,
   * which is what the eighties snare actually is — a long reverb with the end
   * chopped, not a short reverb, and the two sound nothing alike. `reverse`
   * runs the envelope backwards so the tail swells into the note rather than
   * trailing away from it.
   */
  function reverbImpulse(ctx, seconds, decay, shape) {
    shape = shape || 'room';
    const key = '_mazIR' + shape + Math.round(seconds * 10) + '_' + Math.round(decay * 10);
    if (ctx[key]) return ctx[key];
    const rate = ctx.sampleRate;
    const len = Math.max(1, Math.floor(rate * seconds));
    const buf = ctx.createBuffer(2, len, rate);
    let seed = 987654321;
    const gateAt = Math.floor(len * 0.28);
    for (let ch = 0; ch < 2; ch++) {
      const d = buf.getChannelData(ch);
      for (let i = 0; i < len; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        const n = (seed / 0x3fffffff) - 1;
        const t = i / len;
        let env;
        if (shape === 'reverse') {
          // Grows instead of decaying, with a hard stop at the note itself.
          env = Math.pow(t, decay * 0.6) * Math.min(1, (1 - t) * 40);
        } else {
          env = Math.pow(1 - t, decay) * Math.min(1, t * 40);
          if (shape === 'gated') {
            if (i > gateAt) {
              // A short fade rather than a click, then nothing.
              const past = (i - gateAt) / (rate * 0.008);
              env *= Math.max(0, 1 - past);
            } else {
              env = Math.min(1, t * 40) * 0.9;     // flat while the gate is open
            }
          }
        }
        d[i] = n * env;
      }
    }
    ctx[key] = buf;
    return buf;
  }

  /*
   * A transparent output ceiling for the master bus — not an overdrive.
   * Everything below `knee` passes through untouched; above it the curve bends
   * over so the output can never leave ±1 whatever the mix throws at it.
   * (`driveCurve` below is the opposite: deliberate saturation for instruments.
   * Using it here squashed the crest factor from 7 to 1.3 — a brick wall.)
   */
  function softClipCurve(ctx) {
    if (ctx._mazSoftClip) return ctx._mazSoftClip;
    const knee = 0.8;
    const n = 2048;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i * 2) / (n - 1) - 1;
      const a = Math.abs(x);
      const y = a <= knee ? a : knee + (1 - knee) * Math.tanh((a - knee) / (1 - knee));
      curve[i] = x < 0 ? -y : y;
    }
    ctx._mazSoftClip = curve;
    return curve;
  }

  /*
   * Instrument saturation. The curve keeps full-scale at full scale but lifts
   * quieter parts by (1 + k), so `k` is a loudness control as much as a tone
   * control: at k = 10 a driven bass comes back about 20 dB hotter than the
   * clean one and swamps the mix. Keep the scaling gentle.
   */
  function driveCurve(ctx, amount) {
    const k = Math.max(0.001, amount) * 3;
    const key = '_mazCurve' + Math.round(k * 100);
    if (ctx[key]) return ctx[key];
    const n = 1024;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i * 2) / n - 1;
      curve[i] = ((1 + k) * x) / (1 + k * Math.abs(x));
    }
    ctx[key] = curve;
    return curve;
  }

  /*
   * Bit crushing. A digital sample is a number with a fixed number of bits;
   * throw bits away and the smooth curve becomes a staircase, and the error
   * between the two is the crunch you hear. Rounding every input level to one
   * of `2^bits` steps is exactly that, and a waveshaper does it in one node.
   *
   * The shaper is deliberately NOT oversampled. Oversampling exists to suppress
   * the aliasing a hard curve creates — and here that aliasing, the grit and
   * the ringing sidebands, is the entire point.
   *
   * At amount 0 this is 16-bit: a staircase far finer than the curve's own
   * resolution, so it passes audio through unchanged.
   */
  function crushCurve(ctx, amount) {
    const a = Math.max(0, Math.min(1, amount));
    const bits = 16 - a * 14;                     // 16 bits (clean) → 2 bits (wrecked)
    const key = '_mazCrush' + Math.round(bits * 100);
    if (ctx[key]) return ctx[key];
    const levels = Math.pow(2, bits - 1);
    const n = 8192;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i * 2) / (n - 1) - 1;
      curve[i] = Math.max(-1, Math.min(1, Math.round(x * levels) / levels));
    }
    ctx[key] = curve;
    return curve;
  }

  /**
   * Wave folding. Past full scale the signal turns back on itself instead of
   * flattening, so where clipping only squares off the harmonics a sound
   * already has, folding adds a whole series that was never there.
   */
  function foldCurve(ctx) {
    if (ctx._mazFold) return ctx._mazFold;
    const n = 4096;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i * 4) / (n - 1) - 2;          // -2..2, so there is room to fold
      curve[i] = Math.sin(x * Math.PI * 0.5);
    }
    ctx._mazFold = curve;
    return curve;
  }

  /**
   * Frequency doubling by squaring.
   *
   * Squaring a sine gives (1 - cos 2wt)/2 — a steady offset plus the octave
   * above. Remove the offset with a highpass and what is left is genuinely an
   * octave up, which is how a shimmer is built when there is no pitch shifter
   * to hand. (Convolving an already-convolved signal a second time, which is
   * the obvious thing to try, smears the tail without raising it at all: it
   * measured a 0.4% brightness change, which is to say none.)
   *
   * The output is proportional to the square of the input, so quiet signals
   * come back much quieter; the caller makes that up.
   */
  function octaveCurve(ctx) {
    if (ctx._mazOctave) return ctx._mazOctave;
    const n = 2048;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i * 2) / (n - 1) - 1;
      curve[i] = x * x;
    }
    ctx._mazOctave = curve;
    return curve;
  }

  /* ------------------------------------------------------------------ *
   * Waveforms
   *
   * A filter can only take away what the oscillator already has, so every
   * subtractive patch built on a sawtooth ends up a cousin of every other one.
   * These are harmonic recipes — amplitude per harmonic — turned into custom
   * oscillator shapes, which is where genuinely different instruments start.
   * ------------------------------------------------------------------ */

  const HARMONICS = {
    // Drawbars: strong fundamental, hollow even harmonics.
    organ:   [1, 0.5, 0.35, 0.25, 0, 0.18, 0, 0.12],
    // Bright and buzzy, the way a horn section sits on top of a mix.
    brass:   [1, 0.7, 0.5, 0.35, 0.26, 0.2, 0.15, 0.1, 0.07],
    // Odd harmonics only — the hollow woodiness of a stopped pipe or reed.
    reed:    [1, 0, 0.5, 0, 0.3, 0, 0.2, 0, 0.12],
    // A gentle rolloff that reads as a plucked string.
    guitar:  [1, 0.6, 0.45, 0.3, 0.22, 0.16, 0.12, 0.09, 0.07, 0.05],
    // Sparse and ringing.
    glass:   [1, 0, 0.3, 0, 0.15, 0, 0.08, 0, 0.05, 0, 0.03],

    /* Bowed strings: a dense harmonic series that keeps going, which is what
       makes a section sound like many players rather than one oscillator. */
    violin:  [1, 0.8, 0.62, 0.5, 0.42, 0.34, 0.28, 0.22, 0.18, 0.14, 0.11, 0.09],
    cello:   [1, 0.72, 0.55, 0.36, 0.3, 0.22, 0.17, 0.12, 0.09, 0.07],
    // Nearly a sine with a breath of second and third: a flute is almost pure.
    flute:   [1, 0.12, 0.07, 0.02, 0.01],
    // Odd harmonics only, the cylindrical bore of a clarinet.
    clarinet:[1, 0.02, 0.55, 0.02, 0.32, 0.01, 0.2, 0, 0.12, 0, 0.07],
    // Nasal and bright — the oboe's strong upper partials are its voice.
    oboe:    [0.6, 1, 0.85, 0.7, 0.6, 0.45, 0.35, 0.25, 0.18, 0.12],
    // A trombone is a trumpet with the top taken off.
    trombone:[1, 0.75, 0.55, 0.38, 0.25, 0.16, 0.1, 0.06],
    // Sax sits between reed and brass: odd harmonics, but not only odd.
    sax:     [1, 0.45, 0.62, 0.3, 0.4, 0.2, 0.26, 0.13, 0.16, 0.08],
    // Muted trumpet: thin, buzzy, all upper mid.
    muted:   [0.35, 0.7, 1, 0.8, 0.6, 0.42, 0.3, 0.2, 0.12],
    // Sitar: strong odd partials plus the buzz of the sympathetic strings.
    sitar:   [1, 0.5, 0.8, 0.35, 0.65, 0.28, 0.5, 0.22, 0.4, 0.18, 0.3, 0.14],
    // Koto: sparse, wooden, quick to fade.
    koto:    [1, 0.35, 0.5, 0.18, 0.22, 0.1, 0.12, 0.06],
    // Kalimba: a tine, so mostly fundamental with a metallic third.
    kalimba: [1, 0.08, 0.3, 0.05, 0.12, 0.03, 0.05],
    // Steel drum: inharmonic-feeling, dominated by the octave and twelfth.
    steel:   [1, 0.62, 0.44, 0.12, 0.3, 0.08, 0.14, 0.05, 0.08],
    // Accordion reeds: buzzy and even, like a small organ with teeth.
    accordion:[1, 0.62, 0.48, 0.4, 0.3, 0.26, 0.2, 0.16, 0.12, 0.1],
    // Banjo: bright, thin, all attack.
    banjo:   [0.8, 1, 0.7, 0.6, 0.45, 0.36, 0.28, 0.22, 0.16, 0.12]
  };

  function periodicWave(ctx, name) {
    const key = '_mazWave_' + name;
    if (ctx[key]) return ctx[key];
    const amps = HARMONICS[name] || HARMONICS.guitar;
    const real = new Float32Array(amps.length + 1);
    const imag = new Float32Array(amps.length + 1);
    for (let i = 0; i < amps.length; i++) imag[i + 1] = amps[i];
    ctx[key] = ctx.createPeriodicWave(real, imag, { disableNormalization: false });
    return ctx[key];
  }

  /* ------------------------------------------------------------------ *
   * Envelopes
   * ------------------------------------------------------------------ */

  function adsr(param, t, dur, peak, env) {
    const a = Math.max(0.001, env.a);
    const d = Math.max(0.005, env.d);
    const s = env.s === undefined ? 0.6 : env.s;
    const r = Math.max(0.01, env.r);
    const sus = Math.max(EPS, peak * s);
    const attackEnd = t + a;
    const decayEnd = attackEnd + d;
    const relStart = Math.max(t + Math.max(dur, 0.04), attackEnd + 0.005);

    param.setValueAtTime(EPS, t);
    param.linearRampToValueAtTime(Math.max(EPS, peak), attackEnd);
    if (decayEnd < relStart) {
      param.exponentialRampToValueAtTime(sus, decayEnd);
      param.setValueAtTime(sus, relStart);
    } else {
      const frac = (relStart - attackEnd) / d;
      const v = Math.max(EPS, peak * Math.pow(sus / Math.max(EPS, peak), frac));
      param.exponentialRampToValueAtTime(v, relStart);
    }
    param.exponentialRampToValueAtTime(EPS, relStart + r);
    return relStart + r;
  }

  function percEnv(param, t, peak, decay) {
    param.setValueAtTime(Math.max(EPS, peak), t);
    param.exponentialRampToValueAtTime(EPS, t + decay);
    return t + decay;
  }

  /** StereoPannerNode where available; a plain gain elsewhere (mono, but audible). */
  /**
   * A panner, or null when one would do nothing.
   *
   * Returning null for a centred part is a real saving — most parts sit in the
   * middle and a node that does nothing still costs something on every sample.
   * But it means a *moving* pan has to ask for one explicitly, because a part
   * that starts centred is exactly the case where "no panner needed" is wrong:
   * there is nothing to modulate. Pass `force` when the pan is going to move.
   */
  function panner(ctx, pan, force) {
    if (!ctx.createStereoPanner) return null;
    if (!pan && !force) return null;
    const p = ctx.createStereoPanner();
    p.pan.value = Math.max(-1, Math.min(1, pan || 0));
    return p;
  }

  function noiseSource(ctx, t, dur) {
    const src = ctx.createBufferSource();
    src.buffer = noiseBuffer(ctx);
    src.loop = true;
    // Vary the read position so repeated hits are not bit-identical.
    src.playbackRate.value = 0.8 + ((t * 7919) % 100) / 250;
    src.start(t, ((t * 37) % 1.5));
    src.stop(t + dur + 0.05);
    return src;
  }

  /* ------------------------------------------------------------------ *
   * Melodic voices
   * ------------------------------------------------------------------ */

  /**
   * out    — { dry, rev, del } destination gain nodes
   * preset — see genres.js PRESETS
   */
  /*
   * Round robin. Two identical notes in a row are a machine; a player never
   * repeats anything exactly. This derives a small, *deterministic* variation
   * from the note's own start time, so the same song always sounds the same
   * while consecutive notes never do.
   */
  function roundRobin(t) {
    const x = Math.sin(t * 12.9898 + 4.1414) * 43758.5453;
    return (x - Math.floor(x)) - 0.5;          // -0.5 .. 0.5
  }

  function playNote(ctx, out, t, dur, freq, preset, vel, extra) {
    const kind = preset.kind || 'subtractive';
    const amp = ctx.createGain();
    amp.gain.value = 0;

    const rr = roundRobin(t);
    // A few cents either way, and a touch of level: enough to stop the
    // repetition reading as a copy, far too little to read as out of tune.
    freq = freq * (1 + rr * 0.0016);

    let tail;
    const peak = (preset.gain || 0.5) * (vel === undefined ? 0.8 : vel);
    const nodes = [];

    if (kind === 'epiano') {
      // Two sines an octave apart plus a short attack partial: Rhodes-ish.
      const tone = preset.tone === undefined ? 0.5 : preset.tone;
      const partials = [[1, 1], [2, 0.32 + tone * 0.3], [4, 0.08 * tone]];
      for (let i = 0; i < partials.length; i++) {
        const o = ctx.createOscillator();
        o.type = 'sine';
        o.frequency.value = freq * partials[i][0];
        const g = ctx.createGain();
        g.gain.value = partials[i][1];
        o.connect(g).connect(amp);
        nodes.push(o);
      }
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'bell') {
      const carrier = ctx.createOscillator();
      carrier.type = 'sine';
      carrier.frequency.value = freq;
      const mod = ctx.createOscillator();
      mod.type = 'sine';
      mod.frequency.value = freq * 3.51;
      const modGain = ctx.createGain();
      percEnv(modGain.gain, t, freq * 2.2, Math.max(0.2, preset.amp.d * 0.5));
      mod.connect(modGain).connect(carrier.frequency);
      carrier.connect(amp);
      nodes.push(carrier, mod);
      tail = adsr(amp.gain, t, dur, peak * 0.9, preset.amp);
    } else if (kind === 'fm') {
      /* Two-operator FM. The modulator's depth falls away as the note sounds,
         which is why FM reads as "struck" — bright at the attack, mellow after.
         Ratio decides the character: whole numbers ring, odd ratios clang. */
      const ratio = preset.ratio === undefined ? 2 : preset.ratio;
      const index = preset.index === undefined ? 1.6 : preset.index;
      const carrier = ctx.createOscillator();
      carrier.type = preset.carrier || 'sine';
      carrier.frequency.value = freq;
      const mod = ctx.createOscillator();
      mod.type = 'sine';
      mod.frequency.value = freq * ratio;
      const modGain = ctx.createGain();
      const decay = preset.indexDecay || Math.max(0.15, preset.amp.d * 0.6);
      modGain.gain.setValueAtTime(freq * index, t);
      modGain.gain.exponentialRampToValueAtTime(Math.max(EPS, freq * index * 0.05), t + decay);
      mod.connect(modGain).connect(carrier.frequency);
      carrier.connect(amp);
      nodes.push(carrier, mod);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'choir') {
      /* Formants: three fixed resonances that sit where a vowel sits, so the
         pitch moves under them and the tone reads as a voice rather than a
         synth. These are roughly an "aah". */
      const formants = preset.formants || [[730, 1], [1090, 0.5], [2440, 0.25]];
      const src = ctx.createOscillator();
      src.type = 'sawtooth';
      src.frequency.value = freq;
      const src2 = ctx.createOscillator();
      src2.type = 'sawtooth';
      src2.frequency.value = freq;
      src2.detune.value = preset.detune || 9;
      nodes.push(src, src2);

      const mixIn = ctx.createGain();
      mixIn.gain.value = 0.5;
      src.connect(mixIn);
      src2.connect(mixIn);

      for (let i = 0; i < formants.length; i++) {
        const bp = ctx.createBiquadFilter();
        bp.type = 'bandpass';
        bp.frequency.value = formants[i][0];
        bp.Q.value = preset.formantQ || 7;
        const fg = ctx.createGain();
        fg.gain.value = formants[i][1];
        mixIn.connect(bp).connect(fg).connect(amp);
      }
      // A little breath keeps it from sounding like a filter sweep.
      if (preset.breath) {
        const n = noiseSource(ctx, t, dur + 0.3);
        const bp = ctx.createBiquadFilter();
        bp.type = 'bandpass';
        bp.frequency.value = 2200;
        bp.Q.value = 0.8;
        const bg = ctx.createGain();
        bg.gain.value = preset.breath;
        n.connect(bp).connect(bg).connect(amp);
      }
      if (preset.vibrato) {
        const v = preset.vibrato;
        const lfo = ctx.createOscillator();
        lfo.type = 'sine';
        lfo.frequency.value = v.rate || 4.8;
        const depth = ctx.createGain();
        depth.gain.setValueAtTime(0, t);
        depth.gain.linearRampToValueAtTime(v.depth || 6, t + (v.delay || 0.3));
        lfo.connect(depth);
        depth.connect(src.detune);
        depth.connect(src2.detune);
        nodes.push(lfo);
      }
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'mallet') {
      /* Struck bar. The partials of a marimba are not a harmonic series — they
         sit near 1, 4 and 10 — which is exactly why it sounds wooden and not
         like a sine with a fast envelope. */
      const partials = preset.partials || [[1, 1, 1], [3.9, 0.4, 0.45], [9.2, 0.16, 0.22]];
      for (let i = 0; i < partials.length; i++) {
        const o = ctx.createOscillator();
        o.type = 'sine';
        o.frequency.value = freq * partials[i][0];
        const g = ctx.createGain();
        percEnv(g.gain, t, partials[i][1], Math.max(0.08, preset.amp.d * partials[i][2]));
        o.connect(g).connect(amp);
        nodes.push(o);
      }
      // The knock of the mallet itself.
      const n = noiseSource(ctx, t, 0.03);
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.value = 2000;
      const ng = ctx.createGain();
      percEnv(ng.gain, t, 0.12, 0.02);
      n.connect(hp).connect(ng).connect(amp);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'pluck') {
      const o1 = ctx.createOscillator(); o1.type = 'sawtooth'; o1.frequency.value = freq;
      const o2 = ctx.createOscillator(); o2.type = 'triangle'; o2.frequency.value = freq * 2;
      const g2 = ctx.createGain(); g2.gain.value = 0.35;
      const filt = ctx.createBiquadFilter();
      filt.type = 'lowpass';
      filt.Q.value = 4;
      const top = Math.min(14000, freq * (6 + (preset.tone || 0.5) * 8));
      filt.frequency.setValueAtTime(top, t);
      filt.frequency.exponentialRampToValueAtTime(Math.max(120, freq * 1.6), t + 0.18);
      o1.connect(filt); o2.connect(g2).connect(filt);
      filt.connect(amp);
      nodes.push(o1, o2);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'string') {
      /*
       * Karplus-Strong: simulate the string instead of imitating it.
       *
       * A burst of noise fills a buffer one wavelength long, and every sample
       * after that is the average of the two samples one wavelength earlier,
       * scaled a little under 1. The noise is the pluck; the wavelength is the
       * string's round trip; the averaging is the energy the high harmonics
       * lose on every trip, which is why a real string turns from bright to
       * mellow as it rings — and why the low notes ring longer than the high
       * ones here without being told to.
       *
       * It is computed into a buffer rather than built as a delay line feeding
       * back through a filter, which is the textbook Web Audio arrangement and
       * does not work: a BiquadFilterNode inside a feedback cycle is unstable
       * in this engine, and was measured growing to 10^34 at a feedback of 0.9
       * — while the identical loop without the filter sat at 0.66 and behaved.
       * Doing the arithmetic directly is both honest to the algorithm and the
       * only version that stays bounded.
       */
      const ring = Math.max(0.2, preset.ring || 2.4);
      const rate = ctx.sampleRate;
      const seconds = Math.min(4, Math.max(0.35, Math.min(ring, dur + (preset.amp.r || 0.4) + 0.2)));
      const n = Math.max(64, Math.floor(rate * seconds));
      const period = Math.max(2, Math.round(rate / Math.max(20, freq)));

      const buf = ctx.createBuffer(1, n, rate);
      const d = buf.getChannelData(0);

      // The pluck. A deterministic noise, so the same note sounds the same.
      let seed = (Math.round(freq * 100) ^ 0x9e37) >>> 0;
      for (let i = 0; i < period; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        d[i] = (seed / 0x3fffffff) - 1;
      }

      /* One round trip's loss. `tone` is how brightly it is picked: a higher
         value keeps more of each trip, so it rings longer and keeps its top. */
      const loss = Math.min(0.999, Math.pow(0.001, (period / rate) / ring));
      const bright = 0.5 + (preset.tone === undefined ? 0.5 : preset.tone) * 0.28;
      for (let i = period; i < n; i++) {
        d[i] = loss * (bright * d[i - period] + (1 - bright) * d[i - period - 1 < 0 ? 0 : i - period - 1]);
      }

      const src = ctx.createBufferSource();
      src.buffer = buf;

      const body = ctx.createBiquadFilter();
      body.type = 'peaking';
      body.frequency.value = preset.body || 320;    // the resonance of the box
      body.Q.value = 0.9;
      body.gain.value = 4;

      src.connect(body).connect(amp);
      nodes.push(src);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else if (kind === 'eight08') {
      const o = ctx.createOscillator();
      o.type = 'sine';
      const glideFrom = extra && extra.glideFrom;
      if (glideFrom && preset.glide) {
        o.frequency.setValueAtTime(glideFrom, t);
        o.frequency.exponentialRampToValueAtTime(freq, t + preset.glide);
      } else {
        o.frequency.setValueAtTime(freq * 1.6, t);
        o.frequency.exponentialRampToValueAtTime(freq, t + 0.03);
      }
      o.connect(amp);
      nodes.push(o);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    } else {
      // Subtractive: detuned oscillator stack → filter with its own envelope.
      const filt = ctx.createBiquadFilter();
      const f = preset.filter || { type: 'lowpass', freq: 1200, q: 1, env: 0, decay: 0.2, sustain: 0.4 };
      filt.type = f.type || 'lowpass';
      filt.Q.value = f.q || 1;
      const bright = (extra && extra.brightness) || 1;
      /* Play a note harder and it should open up, not just get louder — this is
         most of why a static synth line sounds mechanical. */
      const vAmt = 0.55 + 0.45 * (vel === undefined ? 0.8 : vel);
      const base = Math.min(16000, Math.max(60, f.freq * bright));
      const top = Math.min(17000, base + (f.env || 0) * bright * vAmt);
      filt.frequency.setValueAtTime(base, t);
      if (f.env) {
        filt.frequency.linearRampToValueAtTime(top, t + Math.max(0.002, f.attack || 0.005));
        filt.frequency.exponentialRampToValueAtTime(
          Math.max(60, base + (f.env * (f.sustain === undefined ? 0.3 : f.sustain)) * bright * vAmt),
          t + (f.attack || 0.005) + (f.decay || 0.2));
      }

      // Width: alternate oscillators left and right so a stack becomes a spread.
      let leftPan = null, rightPan = null;
      if (preset.width) {
        leftPan = panner(ctx, -preset.width);
        rightPan = panner(ctx, preset.width);
        if (leftPan) leftPan.connect(filt);
        if (rightPan) rightPan.connect(filt);
      }

      const oscs = preset.osc || [{ type: 'sawtooth', detune: 0, gain: 1, octave: 0 }];
      const pitched = [];
      for (let i = 0; i < oscs.length; i++) {
        const spec = oscs[i];
        const o = ctx.createOscillator();
        if (spec.wave) o.setPeriodicWave(periodicWave(ctx, spec.wave));
        else o.type = spec.type;
        o.frequency.value = freq * Math.pow(2, spec.octave || 0);
        o.detune.value = spec.detune || 0;
        const g = ctx.createGain();
        g.gain.value = spec.gain === undefined ? 1 : spec.gain;
        const side = (i % 2 === 0 ? leftPan : rightPan);
        o.connect(g).connect(side || filt);
        nodes.push(o);
        pitched.push(o);
      }
      if (preset.sub) {
        const o = ctx.createOscillator();
        o.type = 'sine';
        o.frequency.value = freq / 2;
        const g = ctx.createGain();
        g.gain.value = preset.sub;
        o.connect(g).connect(filt);       // sub stays centred
        nodes.push(o);
        pitched.push(o);
      }

      // Vibrato — a delayed swell, the way a player leans into a held note.
      if (preset.vibrato && dur > 0.25) {
        const v = preset.vibrato;
        const lfo = ctx.createOscillator();
        lfo.type = 'sine';
        lfo.frequency.value = v.rate || 5.2;
        const depth = ctx.createGain();
        depth.gain.setValueAtTime(0, t);
        depth.gain.linearRampToValueAtTime(v.depth || 7, t + (v.delay || 0.22));
        lfo.connect(depth);
        for (let i = 0; i < pitched.length; i++) depth.connect(pitched[i].detune);
        nodes.push(lfo);
      }

      // Slow filter drift, so a long pad never sits still.
      if (preset.filterLfo) {
        const fl = preset.filterLfo;
        const lfo = ctx.createOscillator();
        lfo.type = 'sine';
        lfo.frequency.value = fl.rate || 0.15;
        const depth = ctx.createGain();
        depth.gain.value = fl.depth || 300;
        lfo.connect(depth).connect(filt.frequency);
        nodes.push(lfo);
      }

      filt.connect(amp);
      tail = adsr(amp.gain, t, dur, peak, preset.amp);
    }

    /* Tremolo sits after the envelope, on the level itself — the shimmer of a
       vibraphone's rotating discs or a rotary speaker. */
    let node = amp;
    if (preset.tremolo) {
      const tr = preset.tremolo;
      const trem = ctx.createGain();
      trem.gain.value = 1 - (tr.depth || 0.25);
      const lfo = ctx.createOscillator();
      lfo.type = 'sine';
      lfo.frequency.value = tr.rate || 5;
      const dep = ctx.createGain();
      dep.gain.value = tr.depth || 0.25;
      lfo.connect(dep).connect(trem.gain);
      amp.connect(trem);
      node = trem;
      lfo.start(t);
      lfo.stop(tail + 0.05);
    }
    if (preset.drive) {
      const shaper = ctx.createWaveShaper();
      shaper.curve = driveCurve(ctx, preset.drive);
      const post = ctx.createGain();
      post.gain.value = 1 / (1 + preset.drive);
      node.connect(shaper).connect(post);
      node = post;
    }

    /*
     * Velocity layers. Level alone is not how an instrument gets louder: a
     * string hit harder is brighter and rougher as well, and one played softly
     * loses its top before it loses its volume. So a hard note picks up a
     * little saturation and a soft one is rolled off — which is most of what
     * separates a played part from a sequenced one.
     */
    const v = vel === undefined ? 0.8 : vel;
    if (v > 0.8 && kind !== 'string') {
      const shaper = ctx.createWaveShaper();
      shaper.curve = driveCurve(ctx, (v - 0.8) * 0.9);
      const post = ctx.createGain();
      post.gain.value = 1 / (1 + (v - 0.8) * 0.9);
      node.connect(shaper).connect(post);
      node = post;
    } else if (v < 0.5) {
      const soft = ctx.createBiquadFilter();
      soft.type = 'lowpass';
      soft.frequency.value = 900 + v * 9000;
      soft.Q.value = 0.6;
      node.connect(soft);
      node = soft;
    }

    node.connect(out.dry);
    const send = preset.send || {};
    if (send.rev && out.rev) {
      const g = ctx.createGain(); g.gain.value = send.rev;
      node.connect(g).connect(out.rev);
    }
    if (send.del && out.del) {
      const g = ctx.createGain(); g.gain.value = send.del;
      node.connect(g).connect(out.del);
    }

    const stopAt = tail + 0.05;
    for (let i = 0; i < nodes.length; i++) {
      nodes[i].start(t);
      nodes[i].stop(stopAt);
    }
    return stopAt;
  }

  /* ------------------------------------------------------------------ *
   * Drum kits
   * ------------------------------------------------------------------ */

  const KITS = {
    lofi: {
      kick:  { f0: 105, f1: 44, pDec: 0.06, dec: 0.4, gain: 1.0, click: 0.1, drive: 0.3 },
      snare: { tone: 180, dec: 0.16, noise: 0.75, hp: 900, bp: 1500, gain: 0.62 },
      hh:    { dec: 0.028, hp: 6500, gain: 0.2 },
      oh:    { dec: 0.22, hp: 6000, gain: 0.18 },
      perc:  { f: 620, dec: 0.09, gain: 0.24 },
      crash: { dec: 1.1, hp: 5000, gain: 0.2 }
    },
    electro: {
      kick:  { f0: 150, f1: 48, pDec: 0.05, dec: 0.42, gain: 1.05, click: 0.25, drive: 0.25 },
      snare: { tone: 200, dec: 0.19, noise: 0.85, hp: 1200, bp: 1900, gain: 0.7 },
      clap:  { dec: 0.24, bp: 1400, gain: 0.7 },
      hh:    { dec: 0.032, hp: 8000, gain: 0.24 },
      oh:    { dec: 0.3, hp: 7000, gain: 0.22 },
      perc:  { f: 900, dec: 0.07, gain: 0.22 },
      crash: { dec: 1.5, hp: 5500, gain: 0.26 },
      tom:   { f0: 220, f1: 90, dec: 0.35, gain: 0.55 }
    },
    house: {
      kick:  { f0: 130, f1: 45, pDec: 0.035, dec: 0.34, gain: 1.1, click: 0.18, drive: 0.3 },
      snare: { tone: 210, dec: 0.16, noise: 0.8, hp: 1400, bp: 2000, gain: 0.6 },
      clap:  { dec: 0.26, bp: 1500, gain: 0.72 },
      hh:    { dec: 0.026, hp: 9000, gain: 0.22 },
      oh:    { dec: 0.34, hp: 7500, gain: 0.24 },
      perc:  { f: 1100, dec: 0.06, gain: 0.2 },
      crash: { dec: 1.4, hp: 6000, gain: 0.22 }
    },
    soft: {
      kick:  { f0: 90, f1: 40, pDec: 0.09, dec: 0.6, gain: 0.7, click: 0.02, drive: 0 },
      snare: { tone: 160, dec: 0.2, noise: 0.5, hp: 700, bp: 1200, gain: 0.35 },
      hh:    { dec: 0.05, hp: 5000, gain: 0.12 },
      oh:    { dec: 0.4, hp: 4500, gain: 0.12 },
      perc:  { f: 480, dec: 0.25, gain: 0.16 },
      shaker:{ dec: 0.04, hp: 9000, gain: 0.1 },
      crash: { dec: 2.2, hp: 4000, gain: 0.14 }
    },
    epic: {
      kick:  { f0: 120, f1: 42, pDec: 0.08, dec: 0.55, gain: 1.0, click: 0.06, drive: 0.15 },
      snare: { tone: 190, dec: 0.35, noise: 0.7, hp: 800, bp: 1600, gain: 0.75 },
      tom:   { f0: 180, f1: 70, dec: 0.5, gain: 0.7 },
      hh:    { dec: 0.04, hp: 7000, gain: 0.16 },
      perc:  { f: 400, dec: 0.2, gain: 0.3 },
      crash: { dec: 2.4, hp: 4500, gain: 0.3 }
    },
    chip: {
      kick:  { f0: 180, f1: 55, pDec: 0.03, dec: 0.16, gain: 0.9, click: 0.1, drive: 0.1 },
      snare: { tone: 260, dec: 0.1, noise: 1.0, hp: 2000, bp: 3000, gain: 0.55 },
      hh:    { dec: 0.02, hp: 9000, gain: 0.18 },
      oh:    { dec: 0.14, hp: 8000, gain: 0.16 },
      perc:  { f: 1400, dec: 0.04, gain: 0.18 },
      crash: { dec: 0.6, hp: 7000, gain: 0.18 }
    },
    break: {
      kick:  { f0: 140, f1: 46, pDec: 0.04, dec: 0.3, gain: 1.05, click: 0.3, drive: 0.35 },
      snare: { tone: 230, dec: 0.17, noise: 0.9, hp: 1500, bp: 2200, gain: 0.72 },
      hh:    { dec: 0.024, hp: 9500, gain: 0.22 },
      oh:    { dec: 0.26, hp: 8000, gain: 0.2 },
      perc:  { f: 1000, dec: 0.05, gain: 0.2 },
      crash: { dec: 1.3, hp: 6000, gain: 0.24 }
    },
    trap: {
      kick:  { f0: 120, f1: 38, pDec: 0.05, dec: 0.5, gain: 1.1, click: 0.15, drive: 0.3 },
      snare: { tone: 200, dec: 0.22, noise: 0.85, hp: 1300, bp: 1900, gain: 0.75 },
      clap:  { dec: 0.22, bp: 1500, gain: 0.6 },
      hh:    { dec: 0.022, hp: 10000, gain: 0.2 },
      oh:    { dec: 0.2, hp: 8500, gain: 0.18 },
      perc:  { f: 1200, dec: 0.05, gain: 0.18 },
      crash: { dec: 1.6, hp: 6000, gain: 0.2 }
    },

    /* An acoustic kit is the opposite of a drum machine in every parameter:
       the kick has a long woody decay instead of a synthetic click, the snare
       is mostly noise across a wide band rather than a tuned tone, and the
       cymbals ring for seconds. */
    /* Boom bap: a kit heard through a sampler.
     *
     * The kick is short and round rather than long and deep — a record played
     * back, not a sine wave — and the snare is mostly crack. Both are driven,
     * because the sound is a loop pushed hard into twelve-bit hardware, and the
     * hats are small and dry so the snare has all the room. */
    boombap: {
      kick:  { f0: 120, f1: 52, pDec: 0.045, dec: 0.3, gain: 1.0, click: 0.18, drive: 0.45 },
      snare: { tone: 210, dec: 0.2, noise: 0.95, hp: 1100, bp: 2200, gain: 0.82 },
      rim:   { f: 1900, dec: 0.035, gain: 0.44 },
      hh:    { dec: 0.026, hp: 7600, gain: 0.2 },
      oh:    { dec: 0.2, hp: 6400, gain: 0.18 },
      ride:  { dec: 1.1, hp: 5400, gain: 0.15 },
      tom:   { f0: 190, f1: 78, dec: 0.4, gain: 0.6 },
      conga: { f0: 310, f1: 205, dec: 0.26, gain: 0.38 },
      tamb:  { dec: 0.07, hp: 8800, gain: 0.14 },
      perc:  { f: 700, dec: 0.1, gain: 0.26 },
      shaker:{ dec: 0.045, hp: 9800, gain: 0.11 },
      clap:  { dec: 0.2, bp: 1600, gain: 0.6 },
      crash: { dec: 1.6, hp: 4600, gain: 0.22 }
    },

    acoustic: {
      kick:  { f0: 95, f1: 48, pDec: 0.11, dec: 0.5, gain: 0.95, click: 0.12, drive: 0.08 },
      snare: { tone: 185, dec: 0.24, noise: 0.9, hp: 800, bp: 1700, gain: 0.72 },
      rim:   { f: 1700, dec: 0.04, gain: 0.4 },
      hh:    { dec: 0.045, hp: 6800, gain: 0.2 },
      oh:    { dec: 0.4, hp: 5600, gain: 0.2 },
      ride:  { dec: 1.5, hp: 5200, gain: 0.18 },
      tom:   { f0: 200, f1: 82, dec: 0.55, gain: 0.68 },
      conga: { f0: 300, f1: 200, dec: 0.3, gain: 0.4 },
      tamb:  { dec: 0.09, hp: 8500, gain: 0.16 },
      perc:  { f: 520, dec: 0.14, gain: 0.24 },
      shaker:{ dec: 0.05, hp: 9500, gain: 0.12 },
      crash: { dec: 2.6, hp: 4200, gain: 0.26 }
    },

    /* Jazz: brushes and sticks on a small kit. Everything quieter, the ride
       doing the work the hats do everywhere else. */
    jazz: {
      kick:  { f0: 88, f1: 46, pDec: 0.1, dec: 0.42, gain: 0.6, click: 0.04, drive: 0 },
      snare: { tone: 195, dec: 0.18, noise: 0.72, hp: 950, bp: 1900, gain: 0.44 },
      rim:   { f: 1800, dec: 0.03, gain: 0.42 },
      hh:    { dec: 0.05, hp: 6000, gain: 0.14 },
      oh:    { dec: 0.34, hp: 5200, gain: 0.14 },
      ride:  { dec: 1.9, hp: 4800, gain: 0.24 },
      tom:   { f0: 210, f1: 96, dec: 0.42, gain: 0.5 },
      perc:  { f: 600, dec: 0.12, gain: 0.18 },
      crash: { dec: 2.2, hp: 4000, gain: 0.2 }
    },

    /* Rock: hit hard. A big tuned snare, hats with weight, cymbals loud
       enough to be part of the arrangement rather than decoration. */
    rock: {
      kick:  { f0: 110, f1: 50, pDec: 0.07, dec: 0.4, gain: 1.15, click: 0.28, drive: 0.3 },
      snare: { tone: 205, dec: 0.26, noise: 0.95, hp: 900, bp: 1800, gain: 0.9 },
      hh:    { dec: 0.05, hp: 7200, gain: 0.3 },
      oh:    { dec: 0.42, hp: 6000, gain: 0.3 },
      ride:  { dec: 1.6, hp: 5400, gain: 0.22 },
      tom:   { f0: 190, f1: 76, dec: 0.5, gain: 0.85 },
      perc:  { f: 700, dec: 0.1, gain: 0.24 },
      crash: { dec: 2.4, hp: 4400, gain: 0.38 }
    },

    /* The 909: the sound of techno. A long tuned kick with a sharp attack
       transient, a snare that is more tone than noise, and bright metallic
       hats with almost no decay. */
    nine09: {
      kick:  { f0: 165, f1: 42, pDec: 0.028, dec: 0.46, gain: 1.18, click: 0.34, drive: 0.42 },
      snare: { tone: 238, dec: 0.14, noise: 0.6, hp: 1600, bp: 2400, gain: 0.66 },
      clap:  { dec: 0.3, bp: 1250, gain: 0.76 },
      hh:    { dec: 0.018, hp: 10500, gain: 0.26 },
      oh:    { dec: 0.36, hp: 8200, gain: 0.26 },
      ride:  { dec: 1.1, hp: 7000, gain: 0.16 },
      tom:   { f0: 230, f1: 88, dec: 0.28, gain: 0.5 },
      cowbell: { f: 540, dec: 0.18, gain: 0.3 },
      perc:  { f: 1300, dec: 0.05, gain: 0.2 },
      crash: { dec: 1.5, hp: 6200, gain: 0.24 }
    },

    /* Latin hand percussion: no kick to speak of, congas and shakers carrying
       the pattern, everything dry and close. */
    latin: {
      kick:  { f0: 100, f1: 52, pDec: 0.08, dec: 0.34, gain: 0.8, click: 0.08, drive: 0.1 },
      snare: { tone: 220, dec: 0.12, noise: 0.6, hp: 1400, bp: 2300, gain: 0.5 },
      rim:   { f: 2100, dec: 0.03, gain: 0.5 },
      conga: { f0: 330, f1: 215, dec: 0.26, gain: 0.6 },
      tamb:  { dec: 0.08, hp: 9000, gain: 0.22 },
      cowbell: { f: 620, dec: 0.16, gain: 0.34 },
      shaker:{ dec: 0.045, hp: 10000, gain: 0.2 },
      hh:    { dec: 0.03, hp: 8800, gain: 0.16 },
      oh:    { dec: 0.22, hp: 7600, gain: 0.16 },
      perc:  { f: 880, dec: 0.07, gain: 0.26 },
      crash: { dec: 1.4, hp: 5200, gain: 0.2 }
    }
  };

  function kitFor(id) { return KITS[id] || KITS.electro; }

  /* Kick and snare hold the centre; everything else sits off to one side, the
     way a kit does in front of you. */
  const DRUM_PAN = {
    kick: 0, snare: 0, clap: 0.06, hh: 0.24, oh: 0.2,
    tom: -0.28, perc: -0.32, shaker: 0.34, crash: -0.18, rim: 0.26,
    riser: 0, impact: 0, ride: 0.3, tamb: -0.26, cowbell: 0.18, conga: -0.24
  };

  function playDrum(ctx, out, t, inst, vel, kitId, dur) {
    const kit = kitFor(kitId);
    vel = vel === undefined ? 0.8 : vel;

    const pan = panner(ctx, DRUM_PAN[inst] || 0);
    if (pan) pan.connect(out.dry);

    function toOut(node, revAmt) {
      node.connect(pan || out.dry);
      if (revAmt && out.rev) {
        const g = ctx.createGain(); g.gain.value = revAmt;
        node.connect(g).connect(out.rev);
      }
    }

    if (inst === 'kick') {
      const p = kit.kick;
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(p.f0, t);
      o.frequency.exponentialRampToValueAtTime(p.f1, t + p.pDec);
      const g = ctx.createGain();
      percEnv(g.gain, t, p.gain * vel, p.dec);
      let node = g;
      o.connect(g);
      if (p.drive) {
        const sh = ctx.createWaveShaper();
        sh.curve = driveCurve(ctx, p.drive);
        const post = ctx.createGain(); post.gain.value = 0.9;
        g.connect(sh).connect(post);
        node = post;
      }
      toOut(node, 0.04);
      o.start(t); o.stop(t + p.dec + 0.1);

      if (p.click) {
        const n = noiseSource(ctx, t, 0.03);
        const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 2500;
        const cg = ctx.createGain();
        percEnv(cg.gain, t, p.click * vel, 0.02);
        n.connect(hp).connect(cg);
        toOut(cg, 0);
      }
      return;
    }

    if (inst === 'snare' || inst === 'rim') {
      const p = (inst === 'rim' && kit.rim) ? Object.assign({}, kit.snare, kit.rim) : kit.snare;
      const dec = inst === 'rim' ? (kit.rim && kit.rim.dec ? kit.rim.dec : 0.05) : p.dec;
      const n = noiseSource(ctx, t, dec + 0.05);
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = p.bp; bp.Q.value = 0.7;
      const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = p.hp;
      const ng = ctx.createGain();
      percEnv(ng.gain, t, p.gain * vel * p.noise, dec);
      n.connect(bp).connect(hp).connect(ng);
      toOut(ng, 0.18);

      const o = ctx.createOscillator();
      o.type = 'triangle';
      o.frequency.setValueAtTime(p.tone, t);
      o.frequency.exponentialRampToValueAtTime(p.tone * 0.6, t + dec);
      const og = ctx.createGain();
      percEnv(og.gain, t, p.gain * vel * 0.5, dec * 0.7);
      o.connect(og);
      toOut(og, 0.12);
      o.start(t); o.stop(t + dec + 0.08);
      return;
    }

    if (inst === 'clap') {
      const p = kit.clap || kit.snare;
      for (let i = 0; i < 3; i++) {
        const off = i * 0.011;
        const n = noiseSource(ctx, t + off, 0.05);
        const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = p.bp || 1500; bp.Q.value = 1.2;
        const g = ctx.createGain();
        percEnv(g.gain, t + off, (p.gain || 0.6) * vel * 0.6, 0.035);
        n.connect(bp).connect(g);
        toOut(g, 0.2);
      }
      const n = noiseSource(ctx, t + 0.033, (p.dec || 0.2) + 0.05);
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = (p.bp || 1500) * 0.9; bp.Q.value = 0.8;
      const g = ctx.createGain();
      percEnv(g.gain, t + 0.033, (p.gain || 0.6) * vel, p.dec || 0.2);
      n.connect(bp).connect(g);
      toOut(g, 0.25);
      return;
    }

    if (inst === 'hh' || inst === 'oh' || inst === 'shaker') {
      const p = inst === 'hh' ? kit.hh : inst === 'oh' ? (kit.oh || kit.hh) : (kit.shaker || kit.hh);
      const n = noiseSource(ctx, t, p.dec + 0.05);
      const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = p.hp;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = p.hp * 1.3; bp.Q.value = 0.6;
      const g = ctx.createGain();
      percEnv(g.gain, t, p.gain * vel, p.dec);
      n.connect(hp).connect(bp).connect(g);
      toOut(g, inst === 'oh' ? 0.12 : 0.05);
      return;
    }

    if (inst === 'tom') {
      const p = kit.tom || { f0: 200, f1: 80, dec: 0.35, gain: 0.6 };
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(p.f0, t);
      o.frequency.exponentialRampToValueAtTime(p.f1, t + p.dec);
      const g = ctx.createGain();
      percEnv(g.gain, t, p.gain * vel, p.dec);
      o.connect(g);
      toOut(g, 0.2);
      o.start(t); o.stop(t + p.dec + 0.1);
      return;
    }

    if (inst === 'crash') {
      const p = kit.crash || { dec: 1.4, hp: 5500, gain: 0.24 };
      const n = noiseSource(ctx, t, p.dec + 0.1);
      const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = p.hp;
      const g = ctx.createGain();
      percEnv(g.gain, t, p.gain * vel, p.dec);
      n.connect(hp).connect(g);
      toOut(g, 0.4);
      return;
    }

    /* A riser is the sound of a bar being taken away: noise climbing through a
       bandpass while the level swells, so the drop lands on something. */
    if (inst === 'riser') {
      const len = Math.max(0.4, dur || 1.6);
      const n = noiseSource(ctx, t, len + 0.1);
      const bp = ctx.createBiquadFilter();
      bp.type = 'bandpass';
      bp.Q.value = 1.6;
      bp.frequency.setValueAtTime(320, t);
      bp.frequency.exponentialRampToValueAtTime(7200, t + len);
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.setValueAtTime(200, t);
      hp.frequency.exponentialRampToValueAtTime(2600, t + len);
      const g = ctx.createGain();
      g.gain.setValueAtTime(EPS, t);
      g.gain.exponentialRampToValueAtTime(Math.max(EPS, 0.3 * vel), t + len * 0.92);
      g.gain.exponentialRampToValueAtTime(EPS, t + len + 0.06);
      n.connect(bp).connect(hp).connect(g);
      toOut(g, 0.35);
      return;
    }

    /* An impact is the landing: a low boom under a bright splash. */
    if (inst === 'impact') {
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(90, t);
      o.frequency.exponentialRampToValueAtTime(32, t + 0.55);
      const og = ctx.createGain();
      percEnv(og.gain, t, 0.95 * vel, 0.7);
      o.connect(og);
      toOut(og, 0.18);
      o.start(t); o.stop(t + 0.8);

      const n = noiseSource(ctx, t, 1.4);
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.value = 3800;
      const ng = ctx.createGain();
      percEnv(ng.gain, t, 0.3 * vel, 1.2);
      n.connect(hp).connect(ng);
      toOut(ng, 0.5);
      return;
    }

    if (inst === 'ride') {
      /* A jazz ride rings for two seconds and a 909 ride is a short metallic
         tick, so this reads the kit like every other piece rather than sounding
         the same on all of them. */
      const p = kit.ride || { dec: 0.75, hp: 6000, gain: 0.16 };
      const dec = p.dec || 0.75;
      const n = noiseSource(ctx, t, dec + 0.15);
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.value = p.hp || 6000;
      const bp = ctx.createBiquadFilter();
      bp.type = 'bandpass';
      bp.frequency.value = (p.hp || 6000) * 1.5;
      bp.Q.value = 0.5;
      const g = ctx.createGain();
      percEnv(g.gain, t, (p.gain || 0.16) * vel, dec);
      n.connect(hp).connect(bp).connect(g);
      // The bell of the ride, which is what makes it a ride and not a long hat.
      const o = ctx.createOscillator();
      o.type = 'triangle';
      o.frequency.value = p.bell || 2400;
      const og = ctx.createGain();
      percEnv(og.gain, t, (p.gain || 0.16) * 0.38 * vel, dec * 0.46);
      o.connect(og);
      toOut(g, 0.2);
      toOut(og, 0.2);
      o.start(t); o.stop(t + dec * 0.5 + 0.1);
      return;
    }

    if (inst === 'tamb') {
      // Several short noise bursts: a tambourine is many jingles, not one.
      const p = kit.tamb || { dec: 0.09, hp: 7000, gain: 0.1 };
      for (let i = 0; i < 4; i++) {
        const off = i * 0.006;
        const n = noiseSource(ctx, t + off, (p.dec || 0.09) + 0.06);
        const hp = ctx.createBiquadFilter();
        hp.type = 'highpass';
        hp.frequency.value = (p.hp || 7000) + i * 900;
        const g = ctx.createGain();
        percEnv(g.gain, t + off, (p.gain || 0.1) * vel, (p.dec || 0.09) + i * 0.02);
        n.connect(hp).connect(g);
        toOut(g, 0.14);
      }
      return;
    }

    if (inst === 'cowbell') {
      // Two detuned squares through a bandpass — the classic recipe.
      const p = kit.cowbell || { f: 540, dec: 0.28, gain: 0.22 };
      const base = p.f || 540;
      const dec = p.dec || 0.28;
      [base, base * 1.48].forEach(function (f, i) {
        const o = ctx.createOscillator();
        o.type = 'square';
        o.frequency.value = f;
        const bp = ctx.createBiquadFilter();
        bp.type = 'bandpass';
        bp.frequency.value = base * 4.8;
        bp.Q.value = 1.2;
        const g = ctx.createGain();
        percEnv(g.gain, t, (p.gain || 0.22) * (i ? 0.73 : 1) * vel, dec);
        o.connect(bp).connect(g);
        toOut(g, 0.16);
        o.start(t); o.stop(t + dec + 0.07);
      });
      return;
    }

    if (inst === 'conga') {
      const p = kit.conga || { f0: 340, f1: 215, dec: 0.24, gain: 0.5 };
      const dec = p.dec || 0.24;
      const o = ctx.createOscillator();
      o.type = 'sine';
      o.frequency.setValueAtTime(p.f0 || 340, t);
      o.frequency.exponentialRampToValueAtTime(p.f1 || 215, t + dec * 0.67);
      const g = ctx.createGain();
      percEnv(g.gain, t, (p.gain || 0.5) * vel, dec);
      o.connect(g);
      toOut(g, 0.22);
      o.start(t); o.stop(t + dec + 0.11);
      const n = noiseSource(ctx, t, 0.03);
      const hp = ctx.createBiquadFilter();
      hp.type = 'highpass';
      hp.frequency.value = 1800;
      const ng = ctx.createGain();
      percEnv(ng.gain, t, 0.08 * vel, 0.02);
      n.connect(hp).connect(ng);
      toOut(ng, 0);
      return;
    }

    // perc — a short tuned blip
    const p = kit.perc || { f: 800, dec: 0.06, gain: 0.2 };
    const o = ctx.createOscillator();
    o.type = 'triangle';
    o.frequency.setValueAtTime(p.f, t);
    o.frequency.exponentialRampToValueAtTime(p.f * 0.7, t + p.dec);
    const g = ctx.createGain();
    percEnv(g.gain, t, p.gain * vel, p.dec);
    o.connect(g);
    toOut(g, 0.25);
    o.start(t); o.stop(t + p.dec + 0.06);
  }

  /* ------------------------------------------------------------------ *
   * Vinyl / tape noise bed
   * ------------------------------------------------------------------ */

  function vinylBuffer(ctx) {
    if (ctx._mazVinyl) return ctx._mazVinyl;
    const rate = ctx.sampleRate;
    const len = Math.floor(rate * 4);
    const buf = ctx.createBuffer(1, len, rate);
    const d = buf.getChannelData(0);
    let seed = 24680;
    function rnd() { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x3fffffff - 1; }
    for (let i = 0; i < len; i++) d[i] = rnd() * 0.12;
    // Crackle: sparse decaying pops.
    for (let k = 0; k < 320; k++) {
      const pos = Math.floor(Math.abs(rnd()) * (len - 500));
      const amp = 0.2 + Math.abs(rnd()) * 0.7;
      const dur = 40 + Math.floor(Math.abs(rnd()) * 200);
      for (let i = 0; i < dur; i++) {
        d[pos + i] += rnd() * amp * (1 - i / dur);
      }
    }
    ctx._mazVinyl = buf;
    return buf;
  }

  global.Synth = {
    playNote: playNote,
    playDrum: playDrum,
    reverbImpulse: reverbImpulse,
    periodicWave: periodicWave,
    HARMONICS: HARMONICS,
    driveCurve: driveCurve,
    softClipCurve: softClipCurve,
    crushCurve: crushCurve,
    octaveCurve: octaveCurve,
    foldCurve: foldCurve,
    noiseBuffer: noiseBuffer,
    vinylBuffer: vinylBuffer,
    panner: panner,
    DRUM_PAN: DRUM_PAN,
    KITS: KITS
  };
})(window);
