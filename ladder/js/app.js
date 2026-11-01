/*
 * Ladder 24 — the page: panel, knobs, keyboard, wheels, scope, effects,
 * patches and MIDI. The sound itself comes from js/dsp.js.
 */
(function () {
  'use strict';

  const LL = window.LadderLogic;
  const DSP = window.LadderDSP;
  const STORE_CURRENT = 'ladder24.current';
  const STORE_PATCHES = 'ladder24.patches';

  function load(key) {
    try {
      return JSON.parse(localStorage.getItem(key));
    } catch (e) {
      return null;
    }
  }

  function store(key, value) {
    try {
      localStorage.setItem(key, JSON.stringify(value));
    } catch (e) {
      /* private window: the patch just won't be remembered */
    }
  }

  const $ = (sel) => document.querySelector(sel);

  function el(tag, attrs, children) {
    const node = document.createElement(tag);
    if (attrs) {
      for (const k of Object.keys(attrs)) {
        if (k === 'class') node.className = attrs[k];
        else if (k === 'text') node.textContent = attrs[k];
        else if (k === 'html') node.innerHTML = attrs[k];
        else node.setAttribute(k, attrs[k]);
      }
    }
    for (const c of children || []) node.appendChild(c);
    return node;
  }

  // ---------------------------------------------------------------- state

  const state = {
    params: LL.defaults(),
    patchName: 'Init',
    patchKind: 'factory', // factory | user
    edited: false,
    userPatches: [],
    octave: 0,
  };

  // ---------------------------------------------------------------- panel layout

  const SECTIONS = [
    { id: 'controllers', title: 'Controllers', rows: [['glide', 'bendRange'], ['voiceMode'], ['unisonDetune', 'spread'], ['kit', 'kitId']] },
    {
      id: 'oscillators',
      title: 'Oscillator bank',
      rows: [
        { tag: '1', ids: ['o1Wave', 'o1Range'] },
        { tag: '2', ids: ['o2Wave', 'o2Range', 'o2Tune'] },
        { tag: '3', ids: ['o3Wave', 'o3Range', 'o3Tune'] },
        { ids: ['pw', 'pitchEnv', 'o3KeyOff'] },
        { ids: ['fm', 'fold', 'sync'] },
      ],
    },
    { id: 'mixer', title: 'Mixer', rows: [['m1', 'm2'], ['m3', 'noise'], ['drive']] },
    {
      id: 'filter',
      col: 'contours',
      title: 'Ladder filter · 24 dB/oct',
      rows: [['filterMode'], ['cutoff', 'res', 'fEnvAmt', 'keyTrack'], ['fA', 'fD', 'fS', 'fR']],
    },
    { id: 'loudness', col: 'contours', title: 'Loudness contour', rows: [['aA', 'aD', 'aS', 'aR'], ['velAmp', 'velFilter']] },
    {
      id: 'modulation',
      title: 'Modulation',
      rows: [['modSource', 'lfoShape'], ['lfoRate', 'lfoSync', 'lfoDiv'], ['modDepth', 'modPitch', 'modFilter', 'modPw']],
    },
    {
      id: 'cables',
      title: 'Patch cables',
      rows: [1, 2, 3, 4].map((i) => ({ tag: String(i), ids: ['mod' + i + 'Src', 'mod' + i + 'Dst', 'mod' + i + 'Amt'] })),
    },
    { id: 'arp', title: 'Arpeggiator', rows: [['arpOn', 'latch'], ['arpMode'], ['arpOct', 'arpGate'], ['arpDiv']] },
    {
      id: 'fx',
      title: 'Echo & space',
      rows: [['delaySync', 'delayDiv'], ['delayTime', 'delayFb', 'delayMix'], ['reverbSize', 'reverbMix', 'springMix', 'master']],
    },
    { id: 'output', title: 'Output', custom: true },
  ];

  const BIG = new Set(['cutoff', 'res']);

  const ICONS = {
    tri: 'M1 10 L6.5 2 L12 10 L17.5 2 L23 10',
    saw: 'M1 10 L11 2 V10 L21 2 V10 H23',
    ramp: 'M1 10 V2 L11 10 V2 L21 10 H23',
    square: 'M1 10 V2 H6 V10 H12 V2 H17 V10 H23',
    pulse: 'M1 10 V2 H3.5 V10 H12 V2 H14.5 V10 H23',
    sine: 'M1 6 Q3.75 -1 6.5 6 T12 6 T17.5 6 T23 6',
    sh: 'M1 8 H5 V3 H9 V10 H13 V5 H17 V9 H21 V4 H23',
  };
  const ICON_ENUMS = new Set(['o1Wave', 'o2Wave', 'o3Wave', 'lfoShape']);

  const controls = {}; // id -> { update(value) }

  function icon(name) {
    return (
      '<svg viewBox="0 0 24 12" aria-hidden="true"><path d="' +
      ICONS[name] +
      '" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" stroke-linecap="round"/></svg>'
    );
  }

  // Angle on the dial for a knob position: -135° (min) to +135° (max).
  const angleOf = (norm) => -135 + 270 * norm;

  function polar(r, deg) {
    const a = (deg * Math.PI) / 180;
    return [50 + r * Math.sin(a), 50 - r * Math.cos(a)];
  }

  function arcPath(r, from, to) {
    if (Math.abs(to - from) < 0.5) return '';
    const a = Math.min(from, to);
    const b = Math.max(from, to);
    const [x1, y1] = polar(r, a);
    const [x2, y2] = polar(r, b);
    return 'M' + x1.toFixed(2) + ' ' + y1.toFixed(2) + ' A' + r + ' ' + r + ' 0 ' + (b - a > 180 ? 1 : 0) + ' 1 ' + x2.toFixed(2) + ' ' + y2.toFixed(2);
  }

  function knobControl(p) {
    let ticks = '';
    const count = p.step ? Math.round((p.max - p.min) / p.step) + 1 : 11;
    for (let i = 0; i < count; i++) {
      const deg = angleOf(i / (count - 1));
      const [x1, y1] = polar(45, deg);
      const [x2, y2] = polar(50, deg);
      const centre = p.bipolar && i === (count - 1) / 2;
      ticks +=
        '<line x1="' + x1.toFixed(2) + '" y1="' + y1.toFixed(2) + '" x2="' + x2.toFixed(2) + '" y2="' + y2.toFixed(2) +
        '" class="tick' + (centre ? ' tick-centre' : '') + '"/>';
    }
    const id = 'k-' + p.id;
    const knob = el('div', {
      class: 'knob',
      id,
      role: 'slider',
      tabindex: '0',
      'aria-label': p.label,
      'aria-valuemin': String(p.min),
      'aria-valuemax': String(p.max),
      html:
        '<svg class="dial" viewBox="0 0 100 100" aria-hidden="true">' + ticks +
        '<path class="arc-track" d="' + arcPath(41, -135, 135) + '"/><path class="arc"/></svg>' +
        '<span class="knob-skirt"></span><span class="knob-cap"></span><span class="knob-pointer"><i></i></span>',
    });
    const value = el('output', { class: 'ctl-value', for: id });
    const wrap = el('div', { class: 'ctl ctl-knob' + (BIG.has(p.id) ? ' big' : ''), 'data-id': p.id }, [
      el('label', { class: 'ctl-label', for: id, text: p.label }),
      knob,
      value,
    ]);
    const arc = knob.querySelector('.arc');
    const pointer = knob.querySelector('.knob-pointer');
    let norm = 0;

    function update(v) {
      norm = LL.toNorm(p, v);
      const deg = angleOf(norm);
      pointer.style.transform = 'rotate(' + deg + 'deg)';
      const start = p.bipolar ? 0 : -135;
      arc.setAttribute('d', arcPath(41, start, deg));
      const text = LL.formatParam(p.id, v);
      value.textContent = text;
      knob.setAttribute('aria-valuenow', String(v));
      knob.setAttribute('aria-valuetext', text);
    }

    function setNorm(n) {
      setParam(p.id, LL.fromNorm(p, n));
    }

    let drag = null;
    knob.addEventListener('pointerdown', (e) => {
      if (e.button !== 0) return;
      e.preventDefault();
      knob.focus({ preventScroll: true });
      knob.setPointerCapture(e.pointerId);
      drag = { x: e.clientX, y: e.clientY, norm, id: e.pointerId };
      wrap.classList.add('turning');
    });
    knob.addEventListener('pointermove', (e) => {
      if (!drag || e.pointerId !== drag.id) return;
      const travel = e.shiftKey ? 900 : 200;
      const d = (drag.y - e.clientY + (e.clientX - drag.x)) / travel;
      setNorm(drag.norm + d);
    });
    const end = (e) => {
      if (drag && e.pointerId === drag.id) {
        drag = null;
        wrap.classList.remove('turning');
      }
    };
    knob.addEventListener('pointerup', end);
    knob.addEventListener('pointercancel', end);
    knob.addEventListener('dblclick', () => setParam(p.id, p.def));
    knob.addEventListener('wheel', (e) => {
      if (document.activeElement !== knob) return;
      e.preventDefault();
      const stepN = p.step ? p.step / (p.max - p.min) : 0.01;
      setNorm(norm + (e.deltaY < 0 ? stepN : -stepN) * (e.shiftKey ? 0.2 : 1));
    }, { passive: false });
    knob.addEventListener('keydown', (e) => {
      const stepN = p.step ? p.step / (p.max - p.min) : e.shiftKey ? 0.1 : 0.01;
      let n = null;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') n = norm + stepN;
      else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') n = norm - stepN;
      else if (e.key === 'PageUp') n = norm + 0.1;
      else if (e.key === 'PageDown') n = norm - 0.1;
      else if (e.key === 'Home') n = 0;
      else if (e.key === 'End') n = 1;
      if (n === null) return;
      e.preventDefault();
      e.stopPropagation();
      if (p.step) setParam(p.id, LL.clampParam(p.id, LL.fromNorm(p, norm) + Math.sign(n - norm) * p.step));
      else setNorm(n);
    });
    return { node: wrap, update };
  }

  // A choice with many options (like the 30 drum kits) is a menu, not a row of buttons.
  function menuControl(p) {
    const id = 'm-' + p.id;
    const sel = el('select', { id, class: 'ctl-menu' });
    for (const opt of p.options) sel.appendChild(el('option', { value: opt, text: LL.OPTION_LABELS[opt] || opt }));
    sel.addEventListener('change', () => setParam(p.id, sel.value));
    const wrap = el('div', { class: 'ctl ctl-enum', 'data-id': p.id }, [el('label', { class: 'ctl-label', for: id, text: p.label }), sel]);
    return { node: wrap, update: (v) => (sel.value = v) };
  }

  function enumControl(p) {
    if (p.menu || p.options.length > 8) return menuControl(p);
    const group = el('div', { class: 'seg' + (ICON_ENUMS.has(p.id) ? ' seg-icons' : ''), role: 'radiogroup', 'aria-label': p.label });
    const buttons = p.options.map((opt) => {
      const label = LL.OPTION_LABELS[opt] || opt;
      const b = el('button', {
        type: 'button',
        class: 'seg-btn',
        role: 'radio',
        id: 'e-' + p.id + '-' + opt.replace('/', '-').replace('.', 'd').replace(/[^\w-]/g, ''),
        'aria-label': label,
        title: label,
        html: ICONS[opt] && ICON_ENUMS.has(p.id) ? icon(opt) : '',
      });
      if (!b.innerHTML) b.textContent = label;
      b.addEventListener('click', () => setParam(p.id, opt));
      group.appendChild(b);
      return b;
    });
    const wrap = el('div', { class: 'ctl ctl-enum', 'data-id': p.id }, [el('span', { class: 'ctl-label', text: p.label }), group]);
    function update(v) {
      p.options.forEach((opt, i) => {
        buttons[i].setAttribute('aria-checked', String(opt === v));
        buttons[i].tabIndex = opt === v ? 0 : -1;
      });
    }
    group.addEventListener('keydown', (e) => {
      const dir = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
      if (!dir) return;
      e.preventDefault();
      e.stopPropagation();
      const i = (p.options.indexOf(state.params[p.id]) + dir + p.options.length) % p.options.length;
      setParam(p.id, p.options[i]);
      buttons[i].focus();
    });
    return { node: wrap, update };
  }

  function boolControl(p) {
    const b = el('button', {
      type: 'button',
      class: 'rocker',
      role: 'switch',
      id: 'b-' + p.id,
      html: '<span class="rocker-led" aria-hidden="true"></span><span class="rocker-body" aria-hidden="true"><i></i></span><span class="rocker-label"></span>',
    });
    b.querySelector('.rocker-label').textContent = p.label;
    b.addEventListener('click', () => setParam(p.id, !state.params[p.id]));
    const wrap = el('div', { class: 'ctl ctl-bool', 'data-id': p.id }, [b]);
    return { node: wrap, update: (v) => b.setAttribute('aria-checked', String(!!v)) };
  }

  function buildPanel() {
    const deck = $('#deck');
    for (const sec of SECTIONS) {
      const body = el('div', { class: 'sec-body' });
      const section = el('section', { class: 'sec sec-' + sec.id, 'aria-labelledby': 'h-' + sec.id }, [
        el('h2', { class: 'sec-title', id: 'h-' + sec.id, text: sec.title }),
        body,
      ]);
      if (sec.custom) {
        body.appendChild($('#output-tpl').content.cloneNode(true));
      } else {
        for (const row of sec.rows) {
          const ids = Array.isArray(row) ? row : row.ids;
          const r = el('div', { class: 'row' });
          if (row.tag) r.appendChild(el('span', { class: 'row-tag', text: row.tag, title: 'Oscillator ' + row.tag }));
          for (const id of ids) {
            const p = LL.BY_ID[id];
            const c = p.type === 'knob' ? knobControl(p) : p.type === 'enum' ? enumControl(p) : boolControl(p);
            controls[id] = c;
            r.appendChild(c.node);
          }
          body.appendChild(r);
        }
      }
      if (sec.col) {
        let col = deck.querySelector('.col-' + sec.col);
        if (!col) col = deck.appendChild(el('div', { class: 'col col-' + sec.col }));
        col.appendChild(section);
      } else deck.appendChild(section);
    }
  }

  function refreshControls() {
    for (const id of Object.keys(controls)) controls[id].update(state.params[id]);
    document.body.classList.toggle('arp-on', !!state.params.arpOn);
    kitChanged();
  }

  // In a drum kit every key has its own drum, so the keys show their names and
  // the sound-shaping sections step back (they do not change the kit).
  let kitShown = null;
  let kitSent = null;
  function kitChanged() {
    const on = !!state.params.kit;
    const shown = on ? state.params.kitId : '';
    document.body.classList.toggle('kit-on', on);
    if (kitShown !== shown && keyEls.size) buildKeys();
    kitShown = shown;
    sendKit();
  }

  function sendKit() {
    if (!node || !state.params.kit || kitSent === state.params.kitId) return;
    kitSent = state.params.kitId;
    send({ type: 'kit', kit: LL.kitDrums(kitSent) });
  }

  // ---------------------------------------------------------------- params

  let saveTimer = 0;

  function setParam(id, raw, quiet) {
    const value = LL.clampParam(id, raw);
    if (value === undefined || state.params[id] === value) return;
    state.params[id] = value;
    if (controls[id]) controls[id].update(value);
    const p = LL.BY_ID[id];
    if (p.fx) applyFx(id, value);
    else send({ type: 'params', params: { [id]: value } });
    if (id === 'arpOn') document.body.classList.toggle('arp-on', !!value);
    if (id === 'kit' || id === 'kitId') kitChanged();
    if (id === 'delayDiv' && !state.params.delaySync) setParam('delaySync', true, quiet);
    if (id === 'lfoDiv' && !state.params.lfoSync) setParam('lfoSync', true, quiet);
    if (TEMPO_LINKED.has(id)) tempoChanged();
    if (!quiet && !state.edited) {
      state.edited = true;
      renderPatchName();
    }
    clearTimeout(saveTimer);
    saveTimer = setTimeout(remember, 400);
  }

  // Settings whose effect depends on the tempo, and the readouts that show it.
  const TEMPO_LINKED = new Set(['bpm', 'delaySync', 'delayDiv', 'delayTime', 'lfoSync', 'lfoDiv', 'lfoRate']);

  const echoSeconds = () =>
    state.params.delaySync ? (60 / state.params.bpm) * DSP.BEATS[state.params.delayDiv] : state.params.delayTime;

  function tempoChanged() {
    const p = state.params;
    if (fx) {
      const t = Math.min(3.9, echoSeconds());
      fx.dl.delayTime.setTargetAtTime(t, ctx.currentTime, 0.05);
      fx.dr.delayTime.setTargetAtTime(t, ctx.currentTime, 0.05);
    }
    // A synced setting takes over from its free knob: dim the knob and show the time it is really using.
    const echo = controls.delayTime.node;
    echo.classList.toggle('synced', !!p.delaySync);
    echo.querySelector('.ctl-value').textContent = p.delaySync
      ? Math.round(echoSeconds() * 1000) + ' ms'
      : LL.formatParam('delayTime', p.delayTime);
    const lfo = controls.lfoRate.node;
    lfo.classList.toggle('synced', !!p.lfoSync);
    lfo.querySelector('.ctl-value').textContent = p.lfoSync
      ? LL.formatParam('lfoRate', p.bpm / 60 / DSP.BEATS[p.lfoDiv])
      : LL.formatParam('lfoRate', p.lfoRate);
    controls.delayDiv.node.classList.toggle('synced-off', !p.delaySync);
    controls.lfoDiv.node.classList.toggle('synced-off', !p.lfoSync);
  }

  function remember() {
    store(STORE_CURRENT, { params: state.params, name: state.patchName, kind: state.patchKind, edited: state.edited });
  }

  function applyPatch(params, name, kind) {
    const bpm = state.params.bpm;
    state.params = LL.sanitizePatch(params);
    if (holdTempo) state.params.bpm = bpm;
    state.patchName = name;
    state.patchKind = kind;
    state.edited = false;
    refreshControls();
    send({ type: 'params', params: LL.engineParams(state.params) });
    for (const p of LL.PARAMS) if (p.fx) applyFx(p.id, state.params[p.id]);
    renderPatchName();
    tempoChanged();
    remember();
  }

  // ---------------------------------------------------------------- tempo

  let holdTempo = !!load('ladder24.holdTempo');
  const taps = [];

  function wireTempo() {
    const box = $('#tempo');
    const show = (v) => {
      $('#tempo-value').textContent = String(Math.round(v));
      box.setAttribute('aria-valuenow', String(v));
      box.setAttribute('aria-valuetext', Math.round(v) + ' beats per minute');
    };
    controls.bpm = { node: box, update: show };
    const nudge = (d) => setParam('bpm', state.params.bpm + d);
    $('#tempo-down').addEventListener('click', () => nudge(-1));
    $('#tempo-up').addEventListener('click', () => nudge(1));
    let drag = null;
    box.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      box.focus({ preventScroll: true });
      box.setPointerCapture(e.pointerId);
      drag = { y: e.clientY, x: e.clientX, v: state.params.bpm, id: e.pointerId };
    });
    box.addEventListener('pointermove', (e) => {
      if (!drag || drag.id !== e.pointerId) return;
      setParam('bpm', drag.v + (drag.y - e.clientY + e.clientX - drag.x) / (e.shiftKey ? 8 : 2));
    });
    const end = () => (drag = null);
    box.addEventListener('pointerup', end);
    box.addEventListener('pointercancel', end);
    box.addEventListener('keydown', (e) => {
      const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 10, PageDown: -10 }[e.key];
      if (!d) return;
      e.preventDefault();
      e.stopPropagation();
      nudge(d);
    });
    box.addEventListener('wheel', (e) => {
      if (document.activeElement !== box) return;
      e.preventDefault();
      nudge(e.deltaY < 0 ? 1 : -1);
    }, { passive: false });

    // Tap along: the average gap between the last few taps sets the tempo.
    $('#tap').addEventListener('click', () => {
      const now = performance.now();
      if (taps.length && now - taps[taps.length - 1] > 2000) taps.length = 0;
      taps.push(now);
      if (taps.length > 5) taps.shift();
      if (taps.length >= 2) {
        const gap = (taps[taps.length - 1] - taps[0]) / (taps.length - 1);
        setParam('bpm', 60000 / gap);
      }
    });

    const hold = $('#tempo-hold');
    const showHold = () => hold.setAttribute('aria-pressed', String(holdTempo));
    hold.addEventListener('click', () => {
      holdTempo = !holdTempo;
      store('ladder24.holdTempo', holdTempo);
      showHold();
      setStatus(holdTempo ? 'Tempo stays at ' + state.params.bpm + ' BPM when you change patch' : 'Each patch brings its own tempo');
    });
    showHold();
    show(state.params.bpm);
  }

  // A light on every beat, so you can see the tempo the echo and arpeggiator follow.
  function beatLight() {
    const led = $('#beat-led');
    if (!ctx || ctx.state !== 'running') {
      led.classList.remove('on');
      return;
    }
    const beats = (ctx.currentTime * state.params.bpm) / 60;
    const beat = Math.floor(beats);
    led.classList.toggle('on', beats - beat < 0.15);
    led.classList.toggle('downbeat', beat % 4 === 0);
  }

  // ---------------------------------------------------------------- audio

  let ctx = null;
  let node = null;
  let fx = null;
  let powering = null;
  let workletReady = false;
  let recorder = null;
  const pending = [];

  function send(msg) {
    if (node) node.port.postMessage(msg);
    else if (msg.type !== 'params') pending.push(msg);
  }

  function power() {
    if (ctx) {
      if (ctx.state !== 'running') ctx.resume();
      return powering;
    }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) {
      setStatus('This browser has no Web Audio. Try a current Chrome, Edge, Firefox or Safari.', true);
      return Promise.resolve();
    }
    ctx = new AC({ latencyHint: 'interactive' });
    ctx.resume(); // must start inside the tap or click
    setPowerUi('starting');
    powering = (async () => {
      try {
        node = await workletEngine(ctx).catch(() => mainThreadEngine(ctx));
        node.port.onmessage = (e) => {
          if (e.data.type === 'voices') showVoices(e.data.mask);
          else if (e.data.type === 'step') showStep(e.data.step);
        };
        kitSent = null;
        sendKit();
        fx = buildFx(ctx, node);
        recorder = buildRecorder(ctx);
        fx.analyser.disconnect();
        fx.analyser.connect(recorder);
        recorder.connect(ctx.destination);
        drumKitSent = null;
        sendSong();
        for (const p of LL.PARAMS) if (p.fx) applyFx(p.id, state.params[p.id]);
        tempoChanged();
        send({ type: 'wheel', value: wheels.mod.value });
        while (pending.length) node.port.postMessage(pending.shift());
        await ctx.resume();
        setPowerUi('on');
        initMidi();
      } catch (err) {
        setPowerUi('off');
        setStatus(err.message || 'The sound engine could not start.', true);
        ctx = null;
        node = null;
      }
    })();
    return powering;
  }

  // The engine on its own audio thread: the best timing, and the normal path.
  async function workletEngine(c) {
    if (!c.audioWorklet) throw new Error('no AudioWorklet');
    const url = URL.createObjectURL(new Blob([DSP.source], { type: 'application/javascript' }));
    await c.audioWorklet.addModule(url);
    workletReady = true;
    return new AudioWorkletNode(c, 'ladder-24', {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
      processorOptions: { params: LL.engineParams(state.params) },
    });
  }

  // Fallback when the browser will not start a worklet (a page opened straight
  // from disk, or a locked-down frame): the same engine on the page's thread,
  // with a little more latency.
  function mainThreadEngine(c) {
    if (!c.createScriptProcessor) {
      throw new Error('This browser cannot run the sound engine. Try a current Chrome, Edge, Firefox or Safari.');
    }
    const synth = new DSP.Synth(c.sampleRate, LL.engineParams(state.params));
    const sp = c.createScriptProcessor(1024, 0, 2);
    const port = { onmessage: null, postMessage: (m) => synth.handle(m) };
    let lastMask = -1;
    let lastStep = -1;
    sp.onaudioprocess = (e) => {
      const L = e.outputBuffer.getChannelData(0);
      const R = e.outputBuffer.getChannelData(1);
      L.fill(0);
      R.fill(0);
      for (let i = 0; i < L.length; i += 128) synth.render(L.subarray(i, i + 128), R.subarray(i, i + 128), 128);
      const mask = synth.activeMask();
      if (mask !== lastMask && port.onmessage) {
        lastMask = mask;
        port.onmessage({ data: { type: 'voices', mask } });
      }
      if (synth.stepNow !== lastStep && port.onmessage) {
        lastStep = synth.stepNow;
        port.onmessage({ data: { type: 'step', step: lastStep } });
      }
    };
    sp.port = port;
    return sp;
  }

  function impulse(c, seconds) {
    const len = Math.max(1, Math.round(c.sampleRate * seconds));
    const buf = c.createBuffer(2, len, c.sampleRate);
    const pre = Math.round(c.sampleRate * 0.012);
    for (let ch = 0; ch < 2; ch++) {
      const d = buf.getChannelData(ch);
      let lp = 0;
      for (let i = pre; i < len; i++) {
        const t = (i - pre) / (len - pre);
        lp += ((Math.random() * 2 - 1) - lp) * (0.55 - 0.4 * t); // darker as it decays
        d[i] = lp * Math.exp(-6.9 * t);
      }
    }
    return buf;
  }

  function buildFx(c, src) {
    const sum = c.createGain();
    src.connect(sum);

    // Ping-pong echo with a darkening filter in the loop, like a tape or bucket-brigade unit.
    const echoIn = c.createGain();
    const dl = c.createDelay(4);
    const dr = c.createDelay(4);
    const tone = c.createBiquadFilter();
    tone.type = 'lowpass';
    tone.frequency.value = 3000;
    const feedback = c.createGain();
    const merge = c.createChannelMerger(2);
    const echoWet = c.createGain();
    src.connect(echoIn);
    echoIn.connect(dl);
    dl.connect(dr);
    dr.connect(tone);
    tone.connect(feedback);
    feedback.connect(dl);
    dl.connect(merge, 0, 0);
    dr.connect(merge, 0, 1);
    merge.connect(echoWet);
    echoWet.connect(sum);

    const revIn = c.createGain();
    const conv = c.createConvolver();
    const revWet = c.createGain();
    src.connect(revIn);
    echoWet.connect(revIn);
    revIn.connect(conv);
    conv.connect(revWet);
    revWet.connect(sum);

    // Spring reverb, like the one built into the Grandmother.
    const springIn = c.createGain();
    const spring = c.createConvolver();
    const springWet = c.createGain();
    const [sl, sr] = DSP.springImpulse(c.sampleRate, 2.5);
    const sb = c.createBuffer(2, sl.length, c.sampleRate);
    sb.copyToChannel(sl, 0);
    sb.copyToChannel(sr, 1);
    spring.buffer = sb;
    src.connect(springIn);
    springIn.connect(spring);
    spring.connect(springWet);
    springWet.connect(sum);

    const limiter = c.createDynamicsCompressor();
    limiter.threshold.value = -8;
    limiter.knee.value = 6;
    limiter.ratio.value = 10;
    limiter.attack.value = 0.003;
    limiter.release.value = 0.2;
    const master = c.createGain();
    const analyser = c.createAnalyser();
    analyser.fftSize = 2048;
    sum.connect(limiter);
    limiter.connect(master);
    master.connect(analyser);
    analyser.connect(c.destination);
    return { dl, dr, feedback, echoWet, conv, revWet, springWet, master, analyser, revTimer: 0 };
  }

  function applyFx(id, v) {
    if (!fx) return;
    const t = ctx.currentTime;
    switch (id) {
      case 'delayFb':
        fx.feedback.gain.setTargetAtTime(v, t, 0.02);
        break;
      case 'delayMix':
        fx.echoWet.gain.setTargetAtTime(v * 0.8, t, 0.02);
        break;
      case 'springMix':
        fx.springWet.gain.setTargetAtTime(v * 1.2, t, 0.02);
        break;
      case 'reverbMix':
        fx.revWet.gain.setTargetAtTime(v * 0.9, t, 0.02);
        break;
      case 'reverbSize':
        clearTimeout(fx.revTimer);
        fx.revTimer = setTimeout(() => {
          fx.conv.buffer = impulse(ctx, v);
        }, fx.conv.buffer ? 150 : 0);
        break;
      case 'master':
        fx.master.gain.setTargetAtTime(v * v * 1.6, t, 0.02);
        break;
      default:
        break;
    }
  }

  function setPowerUi(mode) {
    const b = $('#power');
    b.setAttribute('aria-checked', String(mode === 'on'));
    b.classList.toggle('starting', mode === 'starting');
    document.body.classList.toggle('powered', mode === 'on');
    if (mode === 'on') setStatus('');
  }

  let statusTimer = 0;
  function setStatus(text, isError) {
    const s = $('#status');
    s.textContent = text;
    s.classList.toggle('error', !!isError);
    clearTimeout(statusTimer);
    if (text && !isError) statusTimer = setTimeout(() => (s.textContent = ''), 3500);
  }

  // ---------------------------------------------------------------- notes

  let lastPlayed = 48;
  const holds = new Map(); // note -> how many things hold it (mouse, keys, MIDI)
  const sustained = new Set();
  let sustain = false;
  const keyEls = new Map();

  function press(note, vel) {
    if (note < 0 || note > 127) return;
    if (!ctx) power();
    else if (ctx.state !== 'running') ctx.resume();
    const n = (holds.get(note) || 0) + 1;
    holds.set(note, n);
    sustained.delete(note);
    if (n === 1) send({ type: 'on', note, vel });
    lastPlayed = note;
    if (seqRec) recordStep(note);
    paintKey(note);
  }

  function release(note) {
    const n = (holds.get(note) || 0) - 1;
    if (n > 0) {
      holds.set(note, n);
      return;
    }
    holds.delete(note);
    if (sustain) sustained.add(note);
    else send({ type: 'off', note });
    paintKey(note);
  }

  function setSustain(on) {
    sustain = on;
    if (!on) {
      for (const n of sustained) send({ type: 'off', note: n });
      sustained.clear();
    }
  }

  function panic() {
    holds.clear();
    sustained.clear();
    send({ type: 'panic' });
    for (const k of keyEls.values()) k.classList.remove('down');
  }

  function paintKey(note) {
    const k = keyEls.get(note);
    if (k) k.classList.toggle('down', holds.has(note));
  }

  // ---------------------------------------------------------------- keyboard

  const keysEl = () => $('#keys');
  // The computer keys start at C3; a wide on-screen keyboard reaches further down too.
  const baseNote = () => 48 + state.octave * 12;
  const KEY_LETTER = {};
  for (const k of Object.keys(LL.KEYMAP)) KEY_LETTER[LL.KEYMAP[k]] = k === ';' ? ';' : k === "'" ? "'" : k.toUpperCase();

  function buildKeys() {
    const box = keysEl();
    const width = box.clientWidth || 600;
    const whites = Math.max(8, Math.min(36, Math.floor(width / (width < 520 ? 40 : 34))));
    box.innerHTML = '';
    keyEls.clear();
    const base = baseNote() - 12 * Math.max(0, Math.floor((whites / 7 - 2) / 2));
    const typedBase = baseNote();
    let w = 0;
    let note = base;
    const blacks = [];
    while (w < whites) {
      if (LL.isBlack(note)) {
        blacks.push({ note, at: w });
      } else {
        const k = el('div', { class: 'key white', 'data-note': String(note) });
        if (note % 12 === 0) k.appendChild(el('span', { class: 'key-name', text: LL.noteName(note) }));
        const letter = KEY_LETTER[note - typedBase];
        if (letter) k.appendChild(el('span', { class: 'key-letter', text: letter }));
        drumLabel(k, note);
        box.appendChild(k);
        keyEls.set(note, k);
        w++;
      }
      note++;
    }
    for (const b of blacks) {
      const k = el('div', { class: 'key black', 'data-note': String(b.note) });
      k.style.left = 'calc(' + (b.at / whites) * 100 + '% - ' + (0.62 / whites) * 50 + '%)';
      k.style.width = (0.62 / whites) * 100 + '%';
      const letter = KEY_LETTER[b.note - typedBase];
      if (letter) k.appendChild(el('span', { class: 'key-letter', text: letter }));
      drumLabel(k, b.note);
      box.appendChild(k);
      keyEls.set(b.note, k);
    }
    $('#octave-label').textContent = LL.noteName(base) + '–' + LL.noteName(note - 1);
    for (const n of holds.keys()) paintKey(n);
  }

  function drumLabel(key, note) {
    if (!state.params.kit) return;
    const d = LL.kitLayout(state.params.kitId)[DSP.kitSlot(note)];
    key.appendChild(el('span', { class: 'key-drum', text: d.short }));
    key.title = d.name;
    key.classList.add('drum');
  }

  function noteAt(x, y) {
    const hit = document.elementFromPoint(x, y);
    const key = hit && hit.closest ? hit.closest('.key') : null;
    if (!key) return null;
    const r = key.getBoundingClientRect();
    const vel = Math.min(1, Math.max(0.25, 0.3 + (0.7 * (y - r.top)) / r.height));
    return { note: Number(key.dataset.note), vel };
  }

  function wireKeys() {
    const box = keysEl();
    const pointers = new Map();
    box.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      box.setPointerCapture(e.pointerId);
      const hit = noteAt(e.clientX, e.clientY);
      pointers.set(e.pointerId, hit ? hit.note : null);
      if (hit) press(hit.note, hit.vel);
    });
    box.addEventListener('pointermove', (e) => {
      if (!pointers.has(e.pointerId)) return;
      const hit = noteAt(e.clientX, e.clientY);
      const was = pointers.get(e.pointerId);
      const now = hit ? hit.note : null;
      if (now === was) return;
      if (was !== null) release(was);
      if (hit) press(hit.note, hit.vel);
      pointers.set(e.pointerId, now);
    });
    const up = (e) => {
      if (!pointers.has(e.pointerId)) return;
      const was = pointers.get(e.pointerId);
      if (was !== null) release(was);
      pointers.delete(e.pointerId);
    };
    box.addEventListener('pointerup', up);
    box.addEventListener('pointercancel', up);
    box.addEventListener('contextmenu', (e) => e.preventDefault());

    const typed = new Map(); // keyboard key -> note it started
    const typing = (e) => {
      const t = e.target;
      return t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable);
    };
    window.addEventListener('keydown', (e) => {
      if (e.ctrlKey || e.metaKey || e.altKey || typing(e)) return;
      const key = e.key.toLowerCase();
      if (e.code === 'Space') {
        e.preventDefault();
        if (!e.repeat) togglePlay();
        return;
      }
      if (e.key === 'Escape') {
        panic();
        return;
      }
      if (e.repeat) {
        if (key in LL.KEYMAP) e.preventDefault();
        return;
      }
      if (key === 'z' || key === 'x') {
        shiftOctave(key === 'z' ? -1 : 1);
        return;
      }
      if (key in LL.KEYMAP && !typed.has(key)) {
        e.preventDefault();
        const note = baseNote() + LL.KEYMAP[key];
        typed.set(key, note);
        press(note, 0.85);
      }
    });
    window.addEventListener('keyup', (e) => {
      const key = e.key.toLowerCase();
      if (typed.has(key)) {
        release(typed.get(key));
        typed.delete(key);
      }
    });
    window.addEventListener('blur', () => {
      for (const n of typed.values()) release(n);
      typed.clear();
    });

    $('#oct-down').addEventListener('click', () => shiftOctave(-1));
    $('#oct-up').addEventListener('click', () => shiftOctave(1));
    let resizeTimer = 0;
    new ResizeObserver(() => {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(buildKeys, 60);
    }).observe(box);
  }

  function shiftOctave(d) {
    const next = Math.max(-3, Math.min(3, state.octave + d));
    if (next === state.octave) return;
    state.octave = next;
    buildKeys();
  }

  // ---------------------------------------------------------------- wheels

  const wheels = {
    bend: { value: 0, spring: true, min: -1 },
    mod: { value: 0, spring: false, min: 0 },
  };

  function setWheel(name, v, fromMidi) {
    const w = wheels[name];
    w.value = Math.max(w.min, Math.min(1, v));
    const elW = $('#wheel-' + name);
    const pos = w.min < 0 ? (w.value + 1) / 2 : w.value;
    elW.style.setProperty('--pos', String(pos));
    elW.setAttribute('aria-valuenow', w.value.toFixed(2));
    send({ type: name === 'bend' ? 'bend' : 'wheel', value: w.value });
    if (!fromMidi && name === 'mod') store('ladder24.mod', w.value);
  }

  function wireWheels() {
    for (const name of Object.keys(wheels)) {
      const w = wheels[name];
      const elW = $('#wheel-' + name);
      let drag = null;
      elW.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        elW.setPointerCapture(e.pointerId);
        drag = { y: e.clientY, v: w.value, id: e.pointerId };
        elW.classList.add('held');
      });
      elW.addEventListener('pointermove', (e) => {
        if (!drag || drag.id !== e.pointerId) return;
        const h = elW.clientHeight || 100;
        setWheel(name, drag.v + ((drag.y - e.clientY) / h) * (1 - w.min));
      });
      const end = (e) => {
        if (!drag || drag.id !== e.pointerId) return;
        drag = null;
        elW.classList.remove('held');
        if (w.spring) setWheel(name, 0);
      };
      elW.addEventListener('pointerup', end);
      elW.addEventListener('pointercancel', end);
      elW.addEventListener('keydown', (e) => {
        const d = e.key === 'ArrowUp' ? 0.1 : e.key === 'ArrowDown' ? -0.1 : 0;
        if (!d) return;
        e.preventDefault();
        e.stopPropagation();
        setWheel(name, w.value + d);
      });
      elW.addEventListener('keyup', () => {
        if (w.spring) setWheel(name, 0);
      });
    }
    const m = load('ladder24.mod');
    setWheel('bend', 0);
    setWheel('mod', typeof m === 'number' ? m : 0);
  }

  // ---------------------------------------------------------------- MIDI

  let midiAccess = null;

  async function initMidi() {
    const chip = $('#midi');
    if (midiAccess || !navigator.requestMIDIAccess) return;
    try {
      midiAccess = await navigator.requestMIDIAccess();
    } catch (e) {
      chip.textContent = 'MIDI could not start here';
      chip.hidden = false;
      return;
    }
    const hook = () => {
      const names = [];
      for (const input of midiAccess.inputs.values()) {
        input.onmidimessage = onMidi;
        names.push(input.name);
      }
      chip.hidden = false;
      chip.textContent = names.length ? 'MIDI · ' + names.join(', ') : 'No MIDI keyboard plugged in';
      chip.classList.toggle('live', names.length > 0);
    };
    midiAccess.onstatechange = hook;
    hook();
  }

  function onMidi(e) {
    const [status, a, b] = e.data;
    const kind = status & 0xf0;
    if (kind === 0x90 && b > 0) press(a, b / 127);
    else if (kind === 0x80 || (kind === 0x90 && b === 0)) release(a);
    else if (kind === 0xe0) setWheel('bend', ((b << 7) | a) / 8192 - 1, true);
    else if (kind === 0xb0) {
      if (a === 1) setWheel('mod', b / 127, true);
      else if (a === 64) setSustain(b >= 64);
      else if (a === 120 || a === 123) panic();
    }
  }

  // ---------------------------------------------------------------- scope & voice lights

  function showVoices(mask) {
    document.querySelectorAll('.voice-led').forEach((led, i) => led.classList.toggle('on', !!(mask & (1 << i))));
  }

  function startScope() {
    const canvas = $('#scope');
    const g = canvas.getContext('2d');
    const css = getComputedStyle(document.documentElement);
    const trace = css.getPropertyValue('--trace').trim() || '#ffb04a';
    const grid = css.getPropertyValue('--grid').trim() || '#1e2b24';
    let buf = null;
    function frame() {
      const dpr = window.devicePixelRatio || 1;
      const w = canvas.clientWidth;
      const h = canvas.clientHeight;
      if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
        canvas.width = Math.round(w * dpr);
        canvas.height = Math.round(h * dpr);
      }
      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      g.clearRect(0, 0, w, h);
      g.strokeStyle = grid;
      g.lineWidth = 1;
      g.beginPath();
      for (let i = 1; i < 8; i++) {
        const x = Math.round((w * i) / 8) + 0.5;
        g.moveTo(x, 0);
        g.lineTo(x, h);
      }
      for (let i = 1; i < 4; i++) {
        const y = Math.round((h * i) / 4) + 0.5;
        g.moveTo(0, y);
        g.lineTo(w, y);
      }
      g.stroke();
      g.strokeStyle = trace;
      g.lineWidth = 1.6;
      g.shadowColor = trace;
      g.shadowBlur = 6;
      g.beginPath();
      if (fx) {
        if (!buf) buf = new Float32Array(fx.analyser.fftSize);
        fx.analyser.getFloatTimeDomainData(buf);
        let start = 0;
        for (let i = 1; i < buf.length / 2; i++) {
          if (buf[i - 1] < 0 && buf[i] >= 0) {
            start = i;
            break;
          }
        }
        const span = Math.min(900, buf.length - start);
        for (let i = 0; i < span; i++) {
          const x = (i / (span - 1)) * w;
          const y = h / 2 - buf[start + i] * h * 0.45;
          if (i) g.lineTo(x, y);
          else g.moveTo(x, y);
        }
      } else {
        g.moveTo(0, h / 2);
        g.lineTo(w, h / 2);
      }
      g.stroke();
      g.shadowBlur = 0;
      beatLight();
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  }

  // ---------------------------------------------------------------- patches

  function allPatches() {
    return LL.PRESETS.map((p) => ({ name: p.name, group: p.group, params: p.params, kind: 'factory', about: p.about })).concat(
      state.userPatches.map((p) => ({ name: p.name, group: 'Your patches', params: p.params, kind: 'user' }))
    );
  }

  // Two menus: a section (Bass, Lead, ...) and the sounds in it. The section
  // shown follows the sound that is playing. Two extra views: search results
  // while something is typed in the search box, and your favourites (★).
  let viewGroup = null;
  const FAV = '★ Favourites';
  const FOUND = 'Search results';
  let favourites = new Set(load('ladder24.favourites') || []);
  const favKey = (p) => p.kind + ':' + p.name;

  function renderPatchList() {
    const list = allPatches();
    const at = currentIndex();
    const query = $('#search').value.trim();
    let shown;
    if (query) {
      viewGroup = FOUND;
      shown = LL.searchPatches(list, query);
    } else {
      if (viewGroup === FOUND || !viewGroup) viewGroup = at >= 0 ? list[at].group : list[0].group;
      if (viewGroup === FAV && !favourites.size) viewGroup = at >= 0 ? list[at].group : list[0].group;
      if (viewGroup !== FAV && at >= 0) viewGroup = list[at].group;
      shown = list.map((p, i) => i).filter((i) => (viewGroup === FAV ? favourites.has(favKey(list[i])) : list[i].group === viewGroup));
    }
    const counts = new Map();
    for (const p of list) counts.set(p.group, (counts.get(p.group) || 0) + 1);
    const sections = $('#section-select');
    sections.innerHTML = '';
    if (query) sections.appendChild(el('option', { value: FOUND, text: shown.length + ' found' }));
    if (favourites.size) sections.appendChild(el('option', { value: FAV, text: FAV + ' (' + favourites.size + ')' }));
    for (const [g, n] of counts) sections.appendChild(el('option', { value: g, text: g + ' (' + n + ')' }));
    sections.value = viewGroup;
    const sel = $('#patch-select');
    sel.innerHTML = '';
    for (const i of shown) sel.appendChild(el('option', { value: String(i), text: list[i].name }));
    if (at < 0 || !shown.includes(at)) {
      // The sound playing isn't in this list (loaded from a code, or outside the search).
      const o = el('option', { value: '', text: query && !shown.length ? 'Nothing matches “' + query + '”' : state.patchName });
      o.disabled = true;
      sel.prepend(o);
      sel.value = '';
    } else sel.value = String(at);
    $('#edited').hidden = !state.edited;
    $('#delete-patch').hidden = state.patchKind !== 'user';
    const fav = $('#fav');
    const isFav = at >= 0 && favourites.has(favKey(list[at]));
    fav.setAttribute('aria-pressed', String(isFav));
    fav.title = isFav ? 'Remove from favourites' : 'Add to favourites';
    $('#about').textContent = at >= 0 && list[at].about ? list[at].about : '';
  }

  function currentIndex() {
    return allPatches().findIndex((p) => p.name === state.patchName && p.kind === state.patchKind);
  }

  const renderPatchName = renderPatchList;

  function choosePatch(i) {
    const list = allPatches();
    const p = list[(i + list.length) % list.length];
    applyPatch(p.params, p.name, p.kind);
    if (!state.params.arpOn) panic();
  }

  // ‹ › move through favourites or search results while one is showing, otherwise through every sound.
  function stepPatch(dir) {
    const sel = $('#patch-select');
    const shown = [...sel.options].filter((o) => o.value !== '').map((o) => Number(o.value));
    const at = currentIndex();
    if ((viewGroup === FAV || viewGroup === FOUND) && shown.length) {
      const pos = shown.indexOf(at);
      choosePatch(shown[(pos + dir + shown.length) % shown.length] ?? shown[0]);
    } else choosePatch(at + dir);
  }

  function saveUserPatches() {
    store(STORE_PATCHES, state.userPatches);
  }

  function openDrawer(mode) {
    const d = $('#drawer');
    const same = !d.hidden && d.dataset.mode === mode;
    d.hidden = same;
    d.dataset.mode = mode;
    $('#drawer-save').hidden = mode !== 'save';
    $('#drawer-share').hidden = mode !== 'share';
    $('#drawer-msg').textContent = '';
    if (same) return;
    if (mode === 'save') {
      const name = $('#patch-name');
      name.value = state.patchKind === 'user' ? state.patchName : state.patchName + (state.edited ? ' (mine)' : '');
      name.focus();
      name.select();
    } else {
      $('#patch-code').value = LL.encodePatch(state.patchName, state.params);
    }
  }

  function wirePatches() {
    state.userPatches = (load(STORE_PATCHES) || [])
      .filter((p) => p && typeof p.name === 'string')
      .map((p) => ({ name: p.name.slice(0, 40), params: LL.sanitizePatch(p.params) }));
    renderPatchList();
    $('#patch-select').addEventListener('change', (e) => choosePatch(Number(e.target.value)));
    $('#section-select').addEventListener('change', (e) => {
      const g = e.target.value;
      if (g === FOUND) return;
      $('#search').value = '';
      viewGroup = g;
      const list = allPatches();
      const first = g === FAV ? list.findIndex((p) => favourites.has(favKey(p))) : list.findIndex((p) => p.group === g);
      choosePatch(first);
    });
    $('#search').addEventListener('input', renderPatchList);
    $('#search').addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        e.target.value = '';
        renderPatchList();
      } else if (e.key === 'Enter') {
        const first = LL.searchPatches(allPatches(), e.target.value)[0];
        if (first !== undefined) choosePatch(first);
      }
    });
    $('#fav').addEventListener('click', () => {
      const list = allPatches();
      const at = currentIndex();
      if (at < 0) {
        setStatus('Save this sound first, then it can be a favourite.');
        return;
      }
      const key = favKey(list[at]);
      if (favourites.has(key)) favourites.delete(key);
      else favourites.add(key);
      store('ladder24.favourites', [...favourites]);
      setStatus(favourites.has(key) ? '★ Added “' + list[at].name + '” to favourites' : 'Removed “' + list[at].name + '” from favourites');
      renderPatchList();
    });
    $('#patch-prev').addEventListener('click', () => stepPatch(-1));
    $('#patch-next').addEventListener('click', () => stepPatch(1));
    $('#save-btn').addEventListener('click', () => openDrawer('save'));
    $('#share-btn').addEventListener('click', () => openDrawer('share'));
    $('#drawer-close').addEventListener('click', () => ($('#drawer').hidden = true));

    $('#save-form').addEventListener('submit', (e) => {
      e.preventDefault();
      const name = $('#patch-name').value.trim().slice(0, 40);
      if (!name) {
        $('#drawer-msg').textContent = 'Give the patch a name first.';
        return;
      }
      const params = Object.assign({}, state.params);
      const at = state.userPatches.findIndex((p) => p.name === name);
      if (at >= 0) state.userPatches[at].params = params;
      else state.userPatches.push({ name, params });
      saveUserPatches();
      state.patchName = name;
      state.patchKind = 'user';
      state.edited = false;
      renderPatchList();
      remember();
      $('#drawer').hidden = true;
      setStatus(at >= 0 ? 'Updated “' + name + '”' : 'Saved “' + name + '”');
    });

    $('#delete-patch').addEventListener('click', () => {
      const d = $('#drawer');
      d.hidden = false;
      d.dataset.mode = 'delete';
      $('#drawer-save').hidden = true;
      $('#drawer-share').hidden = true;
      const msg = $('#drawer-msg');
      msg.innerHTML = '';
      msg.append('Delete “' + state.patchName + '” for good? ');
      const yes = el('button', { type: 'button', class: 'btn btn-danger', id: 'confirm-delete', text: 'Delete' });
      const no = el('button', { type: 'button', class: 'btn', id: 'cancel-delete', text: 'Keep it' });
      yes.addEventListener('click', () => {
        const name = state.patchName;
        state.userPatches = state.userPatches.filter((p) => p.name !== name);
        saveUserPatches();
        d.hidden = true;
        state.patchKind = 'factory';
        state.patchName = 'Init';
        state.edited = true;
        renderPatchList();
        remember();
        setStatus('Deleted “' + name + '”');
      });
      no.addEventListener('click', () => (d.hidden = true));
      msg.append(yes, ' ', no);
    });

    $('#copy-code').addEventListener('click', () => {
      const area = $('#patch-code');
      const done = () => ($('#drawer-msg').textContent = 'Copied. Paste it anywhere to share this sound.');
      const fallback = () => {
        area.focus();
        area.select();
        $('#drawer-msg').textContent = 'Selected. Press Ctrl+C (or ⌘C) to copy.';
      };
      try {
        navigator.clipboard.writeText(area.value).then(done, fallback);
      } catch (e) {
        fallback();
      }
    });

    $('#load-form').addEventListener('submit', (e) => {
      e.preventDefault();
      try {
        const got = LL.decodePatch($('#paste-code').value);
        applyPatch(got.params, got.name, 'loaded');
        state.edited = true;
        renderPatchName();
        $('#paste-code').value = '';
        $('#drawer').hidden = true;
        setStatus('Loaded “' + got.name + '”. Save it to keep it.');
      } catch (err) {
        $('#drawer-msg').textContent = err.message;
      }
    });
  }

  // ---------------------------------------------------------------- studio: sequencer, drum grid, theremin

  const SEQ_MAX = 32;
  const song = loadSong();

  function exampleSong() {
    let seed = 7;
    const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
    const steps = LL.randomMelody(16, 48, rnd).concat(new Array(SEQ_MAX - 16).fill(null));
    return { steps, length: 16, gate: 0.5, swing: 0, drums: LL.beatRows(LL.STARTER_BEATS[1]), kitId: 'Classic 808' };
  }

  // The pattern you're working on is kept in this browser, separate from the sound.
  function loadSong() {
    const base = exampleSong();
    const saved = load('ladder24.song');
    if (!saved || typeof saved !== 'object') return base;
    const steps = Array.isArray(saved.steps) ? saved.steps.slice(0, SEQ_MAX) : base.steps;
    while (steps.length < SEQ_MAX) steps.push(null);
    return {
      steps: steps.map((st) =>
        st && Number.isFinite(st.note) ? { note: Math.max(0, Math.min(127, Math.round(st.note))), accent: !!st.accent, slide: !!st.slide } : null
      ),
      length: [8, 16, 32].includes(saved.length) ? saved.length : 16,
      gate: Number.isFinite(saved.gate) ? Math.max(0.1, Math.min(1, saved.gate)) : 0.5,
      swing: Number.isFinite(saved.swing) ? Math.max(0, Math.min(0.5, saved.swing)) : 0,
      drums: LL.DRUM_ROWS.map((r, i) =>
        Array.from({ length: 16 }, (_, j) => {
          const v = saved.drums && saved.drums[i] && saved.drums[i][j];
          return v === 1 || v === 2 ? v : 0;
        })
      ),
      kitId: LL.KIT_NAMES.includes(saved.kitId) ? saved.kitId : base.kitId,
    };
  }

  let songTimer = 0;
  function songChanged() {
    sendSong();
    clearTimeout(songTimer);
    songTimer = setTimeout(() => store('ladder24.song', song), 300);
  }

  let drumKitSent = null;
  function sendSong() {
    if (!node) return;
    send({ type: 'pattern', pattern: { length: song.length, steps: song.steps.slice(0, song.length), swing: song.swing, gate: song.gate } });
    send({ type: 'drums', drums: { length: 16, rows: LL.DRUM_ROWS.map((r, i) => ({ slot: r.slot, steps: song.drums[i] })) } });
    if (drumKitSent !== song.kitId) {
      drumKitSent = song.kitId;
      send({ type: 'drumkit', kit: LL.kitDrums(song.kitId) });
    }
  }

  let playing = false;
  function togglePlay(force) {
    const next = typeof force === 'boolean' ? force : !playing;
    if (next && !ctx) power();
    else if (next && ctx.state !== 'running') ctx.resume();
    playing = next;
    send({ type: 'transport', running: playing });
    const b = $('#play');
    b.setAttribute('aria-pressed', String(playing));
    b.textContent = playing ? '■ Stop' : '▶ Play';
    if (!playing) showStep(-1);
  }

  function showStep(step) {
    document.querySelectorAll('.playing').forEach((c) => c.classList.remove('playing'));
    if (step < 0) return;
    const s = document.querySelector('.seq-step[data-i="' + (step % song.length) + '"]');
    if (s) s.classList.add('playing');
    document.querySelectorAll('.drum-cell[data-j="' + (step % 16) + '"]').forEach((c) => c.classList.add('playing'));
  }

  // ---- step sequencer

  let seqRec = false;
  let cursor = 0;

  function recordStep(note) {
    song.steps[cursor] = { note, accent: false, slide: false };
    cursor = (cursor + 1) % song.length;
    renderSeq();
    songChanged();
  }

  function renderSeq() {
    const grid = $('#seq-grid');
    grid.innerHTML = '';
    grid.style.setProperty('--cols', String(Math.min(16, song.length)));
    for (let i = 0; i < song.length; i++) {
      const st = song.steps[i];
      const cell = el('div', { class: 'seq-step' + (st ? ' on' : '') + (i === cursor ? ' cursor' : '') + (i % 4 === 0 ? ' beat' : ''), 'data-i': String(i) });
      const noteBtn = el('button', {
        type: 'button',
        class: 'seq-note',
        id: 'seq-note-' + i,
        'aria-label': 'Step ' + (i + 1) + ': ' + (st ? LL.noteName(st.note) : 'rest'),
        text: st ? LL.noteName(st.note) : '·',
      });
      wireStepNote(noteBtn, i);
      const acc = el('button', { type: 'button', class: 'seq-flag', id: 'seq-acc-' + i, title: 'Accent: play this step louder', text: 'A' });
      const sl = el('button', { type: 'button', class: 'seq-flag', id: 'seq-slide-' + i, title: 'Slide into the next step', text: 'S' });
      acc.setAttribute('aria-pressed', String(!!(st && st.accent)));
      sl.setAttribute('aria-pressed', String(!!(st && st.slide)));
      acc.disabled = sl.disabled = !st;
      acc.addEventListener('click', () => {
        st.accent = !st.accent;
        renderSeq();
        songChanged();
      });
      sl.addEventListener('click', () => {
        st.slide = !st.slide;
        renderSeq();
        songChanged();
      });
      cell.append(noteBtn, el('div', { class: 'seq-flags' }, [acc, sl]));
      grid.appendChild(cell);
    }
    $('#seq-rec').setAttribute('aria-pressed', String(seqRec));
  }

  // Click a step to switch it on or off; drag up or down (or use the arrow keys) to change its note.
  function wireStepNote(btn, i) {
    let drag = null;
    const setNote = (n) => {
      if (!song.steps[i]) song.steps[i] = { note: lastPlayed, accent: false, slide: false };
      song.steps[i].note = Math.max(24, Math.min(96, n));
      btn.textContent = LL.noteName(song.steps[i].note);
      songChanged();
    };
    btn.addEventListener('pointerdown', (e) => {
      btn.setPointerCapture(e.pointerId);
      drag = { y: e.clientY, note: song.steps[i] ? song.steps[i].note : lastPlayed, moved: false };
    });
    btn.addEventListener('pointermove', (e) => {
      if (!drag) return;
      const d = Math.round((drag.y - e.clientY) / 8);
      if (d !== 0 || drag.moved) {
        drag.moved = true;
        setNote(drag.note + d);
      }
    });
    btn.addEventListener('pointerup', () => {
      if (drag && !drag.moved) {
        song.steps[i] = song.steps[i] ? null : { note: lastPlayed, accent: false, slide: false };
        songChanged();
      }
      drag = null;
      cursor = i;
      renderSeq();
      const again = document.getElementById('seq-note-' + i);
      if (again) again.focus({ preventScroll: true });
    });
    btn.addEventListener('keydown', (e) => {
      const d = { ArrowUp: 1, ArrowDown: -1 }[e.key];
      if (!d) return;
      e.preventDefault();
      e.stopPropagation();
      setNote((song.steps[i] ? song.steps[i].note : lastPlayed) + d * (e.shiftKey ? 12 : 1));
      renderSeq();
      document.getElementById('seq-note-' + i).focus({ preventScroll: true });
    });
  }

  // ---- drum grid

  function renderDrums() {
    const grid = $('#drum-grid');
    grid.innerHTML = '';
    const layout = LL.kitLayout(song.kitId);
    LL.DRUM_ROWS.forEach((row, r) => {
      grid.appendChild(el('span', { class: 'drum-name', text: layout[row.slot].name }));
      for (let j = 0; j < 16; j++) {
        const v = song.drums[r][j];
        const b = el('button', {
          type: 'button',
          class: 'drum-cell' + (v === 1 ? ' on' : v === 2 ? ' on accent' : '') + (j % 4 === 0 ? ' beat' : ''),
          id: 'drum-' + r + '-' + j,
          'data-j': String(j),
          'aria-label': layout[row.slot].name + ', step ' + (j + 1) + ': ' + (v === 2 ? 'accent' : v ? 'on' : 'off'),
        });
        b.addEventListener('click', () => {
          song.drums[r][j] = (song.drums[r][j] + 1) % 3;
          renderDrums();
          songChanged();
          if (!playing && song.drums[r][j] && node) {
            if (!ctx || ctx.state !== 'running') power();
          }
        });
        grid.appendChild(b);
      }
    });
  }

  // ---- theremin pad: left to right is pitch (three octaves from C3), bottom to top is volume

  function wireTheremin() {
    const pad = $('#theremin');
    const dot = $('#theremin-dot');
    const readout = $('#theremin-note');
    let held = null;
    const at = (e) => {
      const r = pad.getBoundingClientRect();
      const x = Math.min(1, Math.max(0, (e.clientX - r.left) / r.width));
      const y = Math.min(1, Math.max(0, (e.clientY - r.top) / r.height));
      let note = 48 + x * 36;
      if ($('#theremin-snap').getAttribute('aria-pressed') === 'true') note = Math.round(note);
      dot.style.left = x * 100 + '%';
      dot.style.top = y * 100 + '%';
      readout.textContent = LL.noteName(Math.round(note)) + (Math.abs(note - Math.round(note)) > 0.05 ? (note > Math.round(note) ? ' +' : ' −') : '');
      return { note, level: Math.max(0.03, 1 - y) };
    };
    pad.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      if (!ctx) power();
      else if (ctx.state !== 'running') ctx.resume();
      pad.setPointerCapture(e.pointerId);
      held = e.pointerId;
      pad.classList.add('held');
      const p = at(e);
      send({ type: 'theremin', on: true, note: p.note, level: p.level });
    });
    pad.addEventListener('pointermove', (e) => {
      if (held !== e.pointerId) return;
      const p = at(e);
      send({ type: 'theremin', on: true, note: p.note, level: p.level });
    });
    const end = (e) => {
      if (held !== e.pointerId) return;
      held = null;
      pad.classList.remove('held');
      send({ type: 'theremin', on: false });
    };
    pad.addEventListener('pointerup', end);
    pad.addEventListener('pointercancel', end);
    $('#theremin-snap').addEventListener('click', (e) => {
      const b = e.currentTarget;
      b.setAttribute('aria-pressed', String(b.getAttribute('aria-pressed') !== 'true'));
    });
  }

  // ---- recording

  let recording = false;
  let recChunks = [];
  let recStart = 0;
  let recTimer = 0;
  let recUrl = null;

  function buildRecorder(c) {
    if (workletReady) {
      const n = new AudioWorkletNode(c, 'ladder-recorder', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
      n.port.onmessage = (e) => {
        if (recording) recChunks.push(e.data);
      };
      return n;
    }
    const sp = c.createScriptProcessor(4096, 2, 2);
    let on = false;
    sp.onaudioprocess = (e) => {
      const l = e.inputBuffer.getChannelData(0);
      const r = e.inputBuffer.numberOfChannels > 1 ? e.inputBuffer.getChannelData(1) : l;
      e.outputBuffer.getChannelData(0).set(l);
      e.outputBuffer.getChannelData(1).set(r);
      if (on && recording) recChunks.push({ l: new Float32Array(l), r: new Float32Array(r) });
    };
    sp.port = { postMessage: (m) => (on = !!m.recording) };
    return sp;
  }

  const clock = (sec) => Math.floor(sec / 60) + ':' + String(Math.floor(sec % 60)).padStart(2, '0');

  async function toggleRecord() {
    const b = $('#rec');
    if (!recording) {
      await power();
      if (!recorder) return;
      recChunks = [];
      recording = true;
      recorder.port.postMessage({ recording: true });
      recStart = performance.now();
      b.setAttribute('aria-pressed', 'true');
      b.textContent = '● 0:00';
      recTimer = setInterval(() => (b.textContent = '● ' + clock((performance.now() - recStart) / 1000)), 250);
      setStatus('Recording everything you play. Press the button again to stop.');
      return;
    }
    recording = false;
    recorder.port.postMessage({ recording: false });
    clearInterval(recTimer);
    b.setAttribute('aria-pressed', 'false');
    b.textContent = '● Rec';
    const frames = recChunks.reduce((n, c) => n + c.l.length, 0);
    if (!frames) {
      setStatus('Nothing was recorded. Press Rec, play, then press it again.', true);
      return;
    }
    const L = new Float32Array(frames);
    const R = new Float32Array(frames);
    let at = 0;
    for (const c of recChunks) {
      L.set(c.l, at);
      R.set(c.r, at);
      at += c.l.length;
    }
    recChunks = [];
    const wav = LL.encodeWav(L, R, ctx.sampleRate);
    if (recUrl) URL.revokeObjectURL(recUrl);
    recUrl = URL.createObjectURL(new Blob([wav], { type: 'audio/wav' }));
    const stamp = new Date().toISOString().slice(0, 16).replace('T', ' ').replace(':', '.');
    const name = 'Ladder 24 - ' + state.patchName.replace(/[\\/:*?"<>|]/g, '') + ' - ' + stamp + '.wav';
    const box = $('#take');
    box.hidden = false;
    $('#take-audio').src = recUrl;
    $('#take-info').textContent = 'Recording, ' + clock(frames / ctx.sampleRate) + ' long.';
    const save = $('#take-save');
    save.href = recUrl;
    save.download = name;
    setStatus('Recorded ' + clock(frames / ctx.sampleRate) + '. Listen back, then save it as a WAV file.');
  }

  function wireStudio() {
    // Tabs
    const tabs = [...document.querySelectorAll('.studio-tab')];
    const show = (id) => {
      for (const t of tabs) {
        const on = t.dataset.tab === id;
        t.setAttribute('aria-selected', String(on));
        $('#' + t.dataset.tab).hidden = !on;
      }
      store('ladder24.tab', id);
    };
    tabs.forEach((t) => t.addEventListener('click', () => show(t.dataset.tab)));
    show(tabs.some((t) => t.dataset.tab === load('ladder24.tab')) ? load('ladder24.tab') : 'tab-seq');

    $('#play').addEventListener('click', () => togglePlay());
    const swing = $('#swing');
    swing.value = String(Math.round(song.swing * 100));
    swing.addEventListener('input', () => {
      song.swing = Number(swing.value) / 100;
      $('#swing-value').textContent = swing.value + '%';
      songChanged();
    });
    $('#swing-value').textContent = swing.value + '%';

    // Step sequencer controls
    const len = $('#seq-length');
    len.value = String(song.length);
    len.addEventListener('change', () => {
      song.length = Number(len.value);
      cursor = Math.min(cursor, song.length - 1);
      renderSeq();
      songChanged();
    });
    const gate = $('#seq-gate');
    gate.value = String(Math.round(song.gate * 100));
    gate.addEventListener('input', () => {
      song.gate = Number(gate.value) / 100;
      songChanged();
    });
    $('#seq-rec').addEventListener('click', () => {
      seqRec = !seqRec;
      if (seqRec) setStatus('Step record: each key you play fills the next step. Rest leaves a gap.');
      renderSeq();
    });
    $('#seq-rest').addEventListener('click', () => {
      song.steps[cursor] = null;
      cursor = (cursor + 1) % song.length;
      renderSeq();
      songChanged();
    });
    $('#seq-random').addEventListener('click', () => {
      const root = 36 + 12 * Math.max(0, Math.min(2, Math.floor((lastPlayed - 36) / 12)));
      const fresh = LL.randomMelody(song.length, root);
      for (let i = 0; i < song.length; i++) song.steps[i] = fresh[i];
      renderSeq();
      songChanged();
    });
    $('#seq-clear').addEventListener('click', () => {
      song.steps.fill(null);
      cursor = 0;
      renderSeq();
      songChanged();
    });

    // Drum grid controls
    const kit = $('#drum-kit');
    for (const n of LL.KIT_NAMES) kit.appendChild(el('option', { value: n, text: n }));
    kit.value = song.kitId;
    kit.addEventListener('change', () => {
      song.kitId = kit.value;
      renderDrums();
      songChanged();
    });
    const starter = $('#drum-starter');
    for (const b of LL.STARTER_BEATS) starter.appendChild(el('option', { value: b.name, text: b.name }));
    starter.addEventListener('change', () => {
      const beat = LL.STARTER_BEATS.find((b) => b.name === starter.value);
      if (beat) {
        song.drums = LL.beatRows(beat);
        renderDrums();
        songChanged();
        setStatus('Loaded the ' + beat.name + ' beat. Press Play.');
      }
      starter.value = '';
    });
    $('#drum-clear').addEventListener('click', () => {
      song.drums = LL.DRUM_ROWS.map(() => new Array(16).fill(0));
      renderDrums();
      songChanged();
    });

    wireTheremin();
    $('#rec').addEventListener('click', toggleRecord);
    $('#take-close').addEventListener('click', () => ($('#take').hidden = true));
    // The desktop program and ordinary browsers save the WAV through the link. Inside
    // the claude.ai viewer that link is blocked, so the WAV goes out zipped through
    // the viewer's own save prompt (it doesn't take .wav files).
    $('#take-save').addEventListener('click', async (e) => {
      if (location.protocol === 'ladder:') return;
      const downloads = window.claude && window.claude.use ? await window.claude.use('downloads').catch(() => null) : null;
      if (!downloads) return; // a plain browser: the link's own download works
      e.preventDefault();
      const link = e.currentTarget;
      try {
        const wav = new Uint8Array(await (await fetch(link.href)).arrayBuffer());
        await downloads.save({ filename: link.download.replace(/\.wav$/, '.zip'), data: LL.zipOne(link.download, wav) });
        setStatus('Saved. Open the .zip to get the WAV.');
      } catch (err) {
        if (err && err.code === 'declined') return;
        setStatus('This view cannot save files. Use the Ladder 24 desktop program to save recordings.', true);
      }
    });
    if (location.protocol !== 'ladder:') $('#take-note').hidden = false;
    // Ask the viewer for the save capability early, so it is ready when Save is pressed.
    if (window.claude && window.claude.use) window.claude.use('downloads').catch(() => null);
    renderSeq();
    renderDrums();
  }

  // ---------------------------------------------------------------- start

  function start(saved) {
    buildPanel();
    const last = (saved && saved.params ? saved : null) || load(STORE_CURRENT);
    if (last && last.params) {
      state.params = LL.sanitizePatch(last.params);
      state.patchName = typeof last.name === 'string' ? last.name : 'Init';
      state.patchKind = last.kind === 'user' || last.kind === 'loaded' ? last.kind : 'factory';
      state.edited = !!last.edited;
    } else {
      const first = LL.PRESETS.find((p) => p.name === 'Glide Lead');
      state.params = LL.sanitizePatch(first.params);
      state.patchName = first.name;
    }
    wireTempo();
    refreshControls();
    tempoChanged();
    wirePatches();
    wireWheels();
    wireKeys();
    buildKeys();
    wireStudio();
    startScope();

    $('#power').addEventListener('click', () => {
      if (ctx && ctx.state === 'running') {
        panic();
        ctx.suspend();
        setPowerUi('off');
      } else power();
    });
    $('#panic').addEventListener('click', panic);
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        for (const n of [...holds.keys()]) {
          holds.set(n, 1);
          release(n);
        }
      }
    });

    const hot = window.claude && window.claude.hot;
    if (hot && hot.snapshot) {
      hot.snapshot(() => ({ params: state.params, name: state.patchName, kind: state.patchKind, edited: state.edited }));
    }
  }

  const hot = window.claude && window.claude.hot;
  if (hot && hot.ready) hot.ready(start);
  else start(hot && hot.data ? hot.data : null);
})();
