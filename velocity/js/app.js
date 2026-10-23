/*
 * VELOCITY — the page.
 *
 * Screens, touch / keyboard / tilt input, the HUD, sound and the game loop.
 * Physics runs on a fixed step; rendering runs whenever the browser draws.
 * Started by index.html once Three.js has loaded.
 */
(function (root) {
  'use strict';

  const Track = root.VelocityTrack, Physics = root.VelocityPhysics, Race = root.VelocityRace, Scene = root.VelocityScene;
  const SETTINGS_KEY = 'velocity.settings';
  const STEP = 1 / 120;
  const $ = (id) => document.getElementById(id);

  const storage = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* private mode: play without records */ } },
  };

  function loadSettings() {
    const d = { carKey: 'comet', steering: 'arrows', muted: false };
    try { return Object.assign(d, JSON.parse(storage.get(SETTINGS_KEY) || '{}')); } catch (e) { return d; }
  }

  function ordinal(n) { const s = ['th', 'st', 'nd', 'rd'], v = n % 100; return n + (s[(v - 20) % 10] || s[v] || s[0]); }

  // --- Sound: a synthesised engine, a nitro hiss, thumps for hits. ---
  function makeSound() {
    let ctx = null, master, engine, engineGain, filter, hiss, hissGain, noiseBuf;
    let muted = false;
    function init() {
      if (ctx) return;
      const AC = root.AudioContext || root.webkitAudioContext; if (!AC) return;
      ctx = new AC();
      master = ctx.createGain(); master.gain.value = muted ? 0 : 1; master.connect(ctx.destination);
      engine = ctx.createOscillator(); engine.type = 'sawtooth'; engine.frequency.value = 50;
      filter = ctx.createBiquadFilter(); filter.type = 'lowpass'; filter.frequency.value = 400;
      engineGain = ctx.createGain(); engineGain.gain.value = 0;
      engine.connect(filter); filter.connect(engineGain); engineGain.connect(master); engine.start();
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
      const d = noiseBuf.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      hiss = ctx.createBufferSource(); hiss.buffer = noiseBuf; hiss.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 2500; bp.Q.value = 0.7;
      hissGain = ctx.createGain(); hissGain.gain.value = 0;
      hiss.connect(bp); bp.connect(hissGain); hissGain.connect(master); hiss.start();
    }
    function burst(gain, seconds, freq) {
      if (!ctx) return;
      const src = ctx.createBufferSource(); src.buffer = noiseBuf;
      const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = freq;
      const g = ctx.createGain(); g.gain.setValueAtTime(gain, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + seconds);
      src.connect(f); f.connect(g); g.connect(master); src.start(); src.stop(ctx.currentTime + seconds);
    }
    return {
      init,
      resume() { if (ctx && ctx.state === 'suspended') ctx.resume(); },
      setMuted(m) { muted = m; if (master) master.gain.value = m ? 0 : 1; },
      update(car, racing) {
        if (!ctx) return;
        const sp = car ? car.speed : 0;
        const t = ctx.currentTime;
        engine.frequency.setTargetAtTime(45 + sp * 2.4 + (car && car.nitroActive ? 40 : 0), t, 0.05);
        filter.frequency.setTargetAtTime(300 + sp * 25, t, 0.05);
        engineGain.gain.setTargetAtTime(racing && car && !car.wrecked ? 0.07 : 0, t, 0.1);
        hissGain.gain.setTargetAtTime(car && car.nitroActive ? 0.12 : 0, t, 0.05);
      },
      crash() { burst(0.6, 0.5, 600); },
      bump() { burst(0.25, 0.15, 900); },
      blip() { if (!ctx) return; const o = ctx.createOscillator(); const g = ctx.createGain(); o.frequency.setValueAtTime(600, ctx.currentTime); o.frequency.exponentialRampToValueAtTime(1400, ctx.currentTime + 0.12); g.gain.setValueAtTime(0.12, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.2); o.connect(g); g.connect(master); o.start(); o.stop(ctx.currentTime + 0.2); },
    };
  }

  function start() {
    const THREE = root.THREE;
    const track = Track.build();
    const settings = loadSettings();
    const saveSettings = () => storage.set(SETTINGS_KEY, JSON.stringify(settings));
    const mobile = /Android|iPhone|iPad|iPod|Mobile/i.test(navigator.userAgent) || (navigator.maxTouchPoints || 0) > 1;
    const canvas = $('view'), fx = $('fx'), fxc = fx.getContext('2d');
    const scene = Scene.create({ THREE, canvas, track, mobile });
    const sound = makeSound();
    sound.setMuted(settings.muted);

    let state = 'title';   // title | cars | race | pause | results
    let race = null, player = null, goTimer = 0, finishDelay = 0, toastTimer = 0, wasWrecked = false, tiltAvailable = false, tiltSteer = 0;
    const keys = {}, held = {};
    let nitroTap = false;

    // --- Screens ---
    const screens = { title: $('screen-title'), cars: $('screen-cars'), pause: $('screen-pause'), results: $('screen-results') };
    function show(name) {
      state = name;
      for (const k in screens) screens[k].hidden = k !== name;
      const racing = name === 'race' || name === 'pause' || name === 'results';
      $('hud').hidden = !racing;
      $('touch').hidden = name !== 'race';
      canvas.style.visibility = racing ? 'visible' : 'hidden';
      if (name === 'title') renderTitle();
    }
    function renderTitle() {
      const rec = Race.loadRecords(storage);
      $('best').textContent = rec.bestTime == null ? 'No best time yet' : 'Best: ' + Race.formatTime(rec.bestTime) + (rec.carKey && Physics.CARS[rec.carKey] ? ' in the ' + Physics.CARS[rec.carKey].name : '');
      renderSettingsButtons();
    }
    function renderSettingsButtons() {
      for (const id of ['btn-sound', 'btn-sound2']) { $(id).textContent = 'Sound: ' + (settings.muted ? 'off' : 'on'); $(id).setAttribute('aria-pressed', String(!settings.muted)); }
      for (const id of ['btn-steer', 'btn-steer2']) $(id).textContent = 'Steering: ' + (settings.steering === 'tilt' ? 'tilt' : 'arrows');
    }
    function toggleSound() { settings.muted = !settings.muted; sound.setMuted(settings.muted); saveSettings(); renderSettingsButtons(); }
    function toggleSteering() {
      if (settings.steering === 'tilt') { settings.steering = 'arrows'; saveSettings(); renderSettingsButtons(); return; }
      const DOE = root.DeviceOrientationEvent;
      if (!DOE) { toast('Tilt is not available on this device'); return; }
      const done = (ok) => { if (ok) { settings.steering = 'tilt'; saveSettings(); } else toast('Tilt was not allowed'); renderSettingsButtons(); };
      if (typeof DOE.requestPermission === 'function') DOE.requestPermission().then((r) => done(r === 'granted')).catch(() => done(false));
      else done(true);
    }
    function toast(text, seconds) { $('toast').textContent = text; $('toast').hidden = false; toastTimer = seconds || 1.6; }

    // --- Car picker ---
    const list = $('car-list');
    function renderCars() {
      list.innerHTML = '';
      for (const key of Physics.ORDER) {
        const c = Physics.CARS[key];
        const el = document.createElement('button');
        el.className = 'car' + (key === settings.carKey ? ' selected' : '');
        el.type = 'button';
        const stat = (label, v) => '<div class="stat"><span>' + label + '</span><div class="bar"><i style="width:' + Math.round(v * 100) + '%"></i></div></div>';
        el.innerHTML = '<div class="swatch" style="background:#' + c.color.toString(16).padStart(6, '0') + '"></div><div class="name">' + c.name + '</div><div class="blurb">' + c.blurb + '</div>' +
          stat('Speed', c.topSpeed / 72) + stat('Launch', c.accel / 21) + stat('Handling', c.handling / 1.4) + stat('Nitro', (c.nitroPower + c.nitroGain) / 2.6);
        el.addEventListener('click', () => { settings.carKey = key; saveSettings(); renderCars(); });
        list.appendChild(el);
      }
    }

    // --- Race lifecycle ---
    function newRace() {
      race = Race.create({ track, playerCar: settings.carKey, storage });
      player = Race.playerEntry(race).car;
      scene.setCars(race.entries.map((e) => e.car));
      goTimer = 0; finishDelay = 0; wasWrecked = false;
      $('countdown').hidden = false; $('toast').hidden = true;
      for (const k in held) held[k] = false;
      show('race');
      sound.init(); sound.resume();
      scene.render(0, { player, cars: race.entries.map((e) => e.car), time: 0 });
    }
    function pause() { if (state !== 'race') return; show('pause'); sound.update(player, false); }
    function resume() { Race.resume(race); $('countdown').hidden = false; show('race'); sound.resume(); }
    function showResults() {
      const r = race.results, rec = race.records;
      $('res-place').textContent = ordinal(r.place) + (r.place === 1 ? ' — Winner!' : '');
      $('res-new').hidden = !r.newBest;
      $('res-total').textContent = Race.formatTime(r.total);
      $('res-lap').textContent = Race.formatTime(r.bestLap);
      $('res-record').textContent = Race.formatTime(rec.bestTime);
      show('results');
    }

    // --- Input ---
    root.addEventListener('keydown', (e) => {
      if (e.repeat) return;
      keys[e.code] = true;
      if (e.code === 'Space') { nitroTap = true; e.preventDefault(); }
      if (e.code === 'Escape' || e.code === 'KeyP') { if (state === 'race') pause(); else if (state === 'pause') resume(); }
      sound.init();
    });
    root.addEventListener('keyup', (e) => { keys[e.code] = false; });
    for (const btn of document.querySelectorAll('.touch-btn')) {
      const action = btn.dataset.action;
      const down = (e) => { e.preventDefault(); held[action] = true; btn.classList.add('held'); if (action === 'nitro') nitroTap = true; sound.init(); sound.resume(); };
      const up = (e) => { e.preventDefault(); held[action] = false; btn.classList.remove('held'); };
      btn.addEventListener('pointerdown', down);
      btn.addEventListener('pointerup', up); btn.addEventListener('pointercancel', up); btn.addEventListener('pointerleave', up);
      btn.addEventListener('contextmenu', (e) => e.preventDefault());
    }
    root.addEventListener('deviceorientation', (e) => {
      if (e.gamma == null && e.beta == null) return;
      tiltAvailable = true;
      const o = (screen.orientation && screen.orientation.type) || '';
      let v;
      if (o.startsWith('landscape')) v = (e.beta || 0) * (o === 'landscape-secondary' ? -1 : 1);
      else v = e.gamma || 0;
      tiltSteer = Math.max(-1, Math.min(1, v / 22));
    });
    function playerInput() {
      let steer = ((keys.ArrowRight || keys.KeyD || held.right) ? 1 : 0) - ((keys.ArrowLeft || keys.KeyA || held.left) ? 1 : 0);
      if (steer === 0 && settings.steering === 'tilt' && tiltAvailable) steer = tiltSteer;
      const brake = !!(keys.ArrowDown || keys.KeyS || keys.ShiftLeft || keys.ShiftRight || held.drift);
      const inp = { throttle: 1, steer, drift: brake, nitro: nitroTap };
      nitroTap = false;
      return inp;
    }

    // --- Buttons ---
    $('btn-race').addEventListener('click', () => { renderCars(); show('cars'); sound.init(); });
    $('btn-back').addEventListener('click', () => show('title'));
    $('btn-go').addEventListener('click', newRace);
    $('btn-pause').addEventListener('click', pause);
    $('btn-resume').addEventListener('click', resume);
    $('btn-restart').addEventListener('click', newRace);
    $('btn-quit').addEventListener('click', () => show('title'));
    $('btn-again').addEventListener('click', newRace);
    $('btn-change').addEventListener('click', () => { renderCars(); show('cars'); });
    $('btn-title').addEventListener('click', () => show('title'));
    $('btn-sound').addEventListener('click', toggleSound); $('btn-sound2').addEventListener('click', toggleSound);
    $('btn-steer').addEventListener('click', toggleSteering); $('btn-steer2').addEventListener('click', toggleSteering);
    document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });
    root.addEventListener('resize', () => { scene.resize(); fx.width = fx.clientWidth; fx.height = fx.clientHeight; });

    // --- HUD ---
    function updateHud() {
      const me = Race.playerEntry(race);
      $('hud-pos').firstElementChild.textContent = me.place;
      $('hud-pos').lastElementChild.textContent = '/ ' + race.entries.length;
      $('hud-lap').children[1].textContent = Race.lapNumber(me, race.laps);
      $('hud-lap').children[2].textContent = '/ ' + race.laps;
      $('hud-time').textContent = Race.formatTime(race.phase === 'countdown' && me.crossings === 0 ? 0 : Race.currentLapTime(race, me));
      $('hud-speed').firstElementChild.textContent = Math.round(player.speed * 3.6);
      $('nitro-fill').style.width = Math.round(player.nitro * 100) + '%';
      $('nitro-fill').parentElement.classList.toggle('full', player.nitro >= 0.999);
      const cd = $('countdown');
      if (race.phase === 'countdown') { cd.hidden = false; cd.textContent = String(Math.ceil(race.countdown)); }
      else if (goTimer > 0) { cd.hidden = false; cd.textContent = 'GO!'; }
      else cd.hidden = true;
    }
    function drawSpeedLines(dt) {
      const w = fx.width, h = fx.height;
      fxc.clearRect(0, 0, w, h);
      if (!player || !player.nitroActive || state !== 'race') return;
      fxc.strokeStyle = 'rgba(255,255,255,0.35)'; fxc.lineWidth = 2;
      const cx = w / 2, cy = h / 2;
      for (let i = 0; i < 28; i++) {
        const a = Math.random() * Math.PI * 2, r0 = Math.min(w, h) * (0.25 + Math.random() * 0.3), r1 = r0 + 40 + Math.random() * 120;
        fxc.beginPath(); fxc.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); fxc.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1); fxc.stroke();
      }
    }

    // --- The loop ---
    let last = performance.now(), acc = 0;
    function frame(now) {
      requestAnimationFrame(frame);
      const dt = Math.min(0.1, (now - last) / 1000); last = now;
      if (!race) return;
      const cars = race.entries.map((e) => e.car);
      if (state === 'race') {
        const wasPhase = race.phase;
        const slow = player.wrecked && player.wreckTimer > Physics.WRECK_TIME - 0.8 ? 0.35 : 1;
        acc += dt * slow;
        while (acc >= STEP) { Race.update(race, STEP, playerInput()); acc -= STEP; }
        if (wasPhase === 'countdown' && race.phase === 'racing') goTimer = 0.8;
        goTimer -= dt; toastTimer -= dt; if (toastTimer <= 0) $('toast').hidden = true;
        if (player.wrecked && !wasWrecked) { sound.crash(); toast('WRECKED', 1.5); }
        else if (player.bumped) sound.bump();
        if (player.nearMiss) { sound.blip(); toast('NEAR MISS +nitro', 0.8); player.nearMiss = false; }
        wasWrecked = player.wrecked;
        if (race.phase === 'finished') { finishDelay += dt; if (finishDelay > 1.5) showResults(); }
        updateHud();
        sound.update(player, true);
        scene.render(dt * slow, { player, cars, time: race.time });
      } else if (state === 'pause' || state === 'results') {
        scene.render(0, { player, cars, time: race.time });
      }
      drawSpeedLines(dt);
    }

    root.VelocityApp.inspect = () => ({ screen: state, race, player, track });   // for the smoke test
    fx.width = fx.clientWidth; fx.height = fx.clientHeight;
    scene.resize();
    renderCars();
    show('title');
    requestAnimationFrame(frame);
  }

  root.VelocityApp = { start, inspect: null };
})(typeof self !== 'undefined' ? self : this);
