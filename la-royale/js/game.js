/*
 * LA Royale — the game loop.
 *
 * Ride the blimp across Los Angeles, jump, skydive, open your chute, land
 * anywhere, then walk, swim, or take a car. The full map (M) lets you drop
 * back in anywhere. This first version is the explorable map; combat, loot
 * and the closing circle come later (see README).
 */

import * as THREE from '../vendor/three.module.min.js';
import { createWorld, collideBody, locationAt, rayClear, clamp, lerp } from './world.js';
import { buildScene } from './render.js';
import { loadRealWorld, buildRealScene } from './realcity.js';
import { makeHud } from './hud.js';
import { makePlayer, makeCar, makeBlimp, CAR_TYPES } from './models.js';
import { makeAmbient } from './ambient.js';

const $ = (id) => document.getElementById(id);
const isTouch = matchMedia('(pointer: coarse)').matches;

const SETTINGS_KEY = 'la-royale-settings';
const settings = Object.assign(
  { quality: isTouch ? 'low' : 'medium', sensitivity: 1, invertY: false, dayLength: 24, startHour: 16.5, map: 'classic' },
  safeRead()
);
function safeRead() { try { return JSON.parse(localStorage.getItem(SETTINGS_KEY)) || {}; } catch { return {}; } }
function saveSettings() { try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch { /* private mode */ } }

/* ---------------------------------------------------------------- loading */

const loadBar = $('loadBar'), loadText = $('loadText');
const step = (pct, text) => new Promise((r) => { loadBar.style.width = `${pct}%`; loadText.textContent = text; setTimeout(r, 30); });

/* Which map: the hand-built one, or real LA at true scale from OpenStreetMap
 * (when its data has been built: tools/build-tiles.mjs). ?map=real or
 * ?map=classic picks one for this visit; ?data=fixture loads the made-up
 * test data instead. */
const params = new URLSearchParams(location.search);
const realBase = params.get('data') === 'fixture' ? 'data/fixture/' : 'data/real/';
async function realAvailable() {
  try { return (await fetch(`${realBase}manifest.json`, { method: 'HEAD' })).ok; } catch { return false; }
}

async function boot() {
  const hasReal = await realAvailable();
  const real = hasReal && (params.get('map') || settings.map) === 'real';
  await step(5, real ? 'Downloading real Los Angeles…' : 'Surveying the Los Angeles basin…');
  const W = real ? await loadRealWorld(realBase, (t) => step(15, t)) : createWorld({ seed: 1985 });
  if (!real) await step(35, `Painting ${W.blocks.length.toLocaleString()} city blocks and the beaches…`);
  const canvas = $('scene');
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: settings.quality !== 'low', powerPreference: 'high-performance', logarithmicDepthBuffer: true });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, settings.quality === 'high' ? 2 : settings.quality === 'medium' ? 1.5 : 1));
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 0.62;
  renderer.shadowMap.enabled = settings.quality !== 'low';
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  await step(55, `Raising ${W.buildings.length.toLocaleString()} buildings and planting ${W.trees.length.toLocaleString()} trees…`);
  const R = real ? buildRealScene(W, renderer, { quality: settings.quality }) : buildScene(W, renderer, { quality: settings.quality });
  if (real) await R.preload(W.title.x, W.title.z, (d, n) => { loadBar.style.width = `${55 + (30 * d) / Math.max(1, n)}%`; loadText.textContent = `Raising the real buildings of Downtown… ${d} of ${n} km²`; });
  await step(85, 'Gassing up the blimp…');
  const game = makeGame(W, R, renderer);
  renderer.compile(R.scene, game.camera);
  await step(100, 'Ready.');
  $('loading').hidden = true;
  $('title').hidden = false;
  const c = W.manifest?.counts;
  $('titleStats').textContent = real
    ? `Real LA, true scale: ${W.manifest.name} · ${c.buildings.toLocaleString()} real buildings · ${c.roads.toLocaleString()} streets · ${W.pois.length} landmarks · Map data © OpenStreetMap contributors, via Overture Maps`
    : `${W.buildings.length.toLocaleString()} buildings · ${W.trees.length.toLocaleString()} trees · ${W.pois.length} landmarks · ${W.vehicles.length} cars to drive`;
  const realBtn = $('realBtn');
  realBtn.hidden = !hasReal;
  realBtn.textContent = real ? 'Back to the classic map' : 'Real LA · true scale (beta)';
  realBtn.addEventListener('click', () => {
    settings.map = real ? 'classic' : 'real'; saveSettings();
    const u = new URL(location.href); u.searchParams.delete('map'); location.href = u.toString();
  });
  game.start();
  window.__laRoyale = game;   // for tests and the curious
}

/* ------------------------------------------------------------------- game */

function makeGame(W, R, renderer) {
  const scene = R.scene;
  const camera = new THREE.PerspectiveCamera(62, 1, 0.3, 14000);
  const hud = makeHud(W, R.groundCanvas);
  const ambient = makeAmbient(W, scene, { quality: settings.quality });

  /* the player */
  const player = makePlayer();
  scene.add(player.root);
  const body = { x: 0, y: 0, z: 0, vx: 0, vy: 0, vz: 0, r: 0.42, h: 1.8, step: 0.55, onGround: false, groundY: 0 };
  let state = 'title';        // title | blimp | freefall | chute | walk | swim | drive
  let facing = 0;             // the way the body faces
  let moveSpeed = 0;

  /* the camera rig */
  const cam = { yaw: 0, pitch: -0.25, dist: 6.5, distTarget: 6.5, x: 0, y: 0, z: 0 };

  /* the blimp */
  // (on the real map it flies clear of the Hollywood Hills)
  const blimp = { mesh: makeBlimp(), from: [0, 0], to: [0, 0], t: 0, len: 1, alt: W.real ? W.heights.reduce((a, b) => Math.max(a, b), 0) + 280 : 560, speed: 48 };
  scene.add(blimp.mesh);
  function newBlimpPath() {
    const a = Math.random() * Math.PI * 2;
    const off = (Math.random() - 0.5) * W.half * 0.8;
    const nx = Math.cos(a), nz = Math.sin(a);
    const cx = -nz * off, cz = nx * off;
    blimp.from = [cx - nx * W.half * 1.1, cz - nz * W.half * 1.1];
    blimp.to = [cx + nx * W.half * 1.1, cz + nz * W.half * 1.1];
    blimp.len = Math.hypot(blimp.to[0] - blimp.from[0], blimp.to[1] - blimp.from[1]);
    blimp.t = 0;
  }
  function blimpPos() {
    const f = blimp.t / blimp.len;
    return [lerp(blimp.from[0], blimp.to[0], f), blimp.alt, lerp(blimp.from[1], blimp.to[1], f)];
  }

  /* the cars */
  const cars = W.vehicles.map((v, i) => {
    const m = makeCar(v.type, i);
    scene.add(m.root);
    const c = { ...v, model: m, T: m.T, yaw: v.ry, speed: 0, steer: 0, y: W.groundHeight(v.x, v.z), vy: 0, hop: 0, siren: false, pitch: 0, roll: 0 };
    placeCar(c, 0);
    return c;
  });
  let car = null;

  /* ------------------------------------------------------------ input */
  const keys = new Set();
  const pressed = new Set();
  window.addEventListener('keydown', (e) => {
    if (e.target && e.target.tagName === 'INPUT') return;
    if (!keys.has(e.code)) pressed.add(e.code);
    keys.add(e.code);
    if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab'].includes(e.code)) e.preventDefault();
  });
  window.addEventListener('keyup', (e) => keys.delete(e.code));
  window.addEventListener('blur', () => keys.clear());
  const down = (...codes) => codes.some((c) => keys.has(c));
  const hit = (...codes) => codes.some((c) => pressed.has(c));

  const canvas = renderer.domElement;
  canvas.addEventListener('click', () => {
    if (!isTouch && state !== 'title' && !hud.mapOpen && document.pointerLockElement !== canvas) canvas.requestPointerLock?.();
  });
  document.addEventListener('mousemove', (e) => {
    if (document.pointerLockElement !== canvas) return;
    look(e.movementX, e.movementY, 0.0024);
  });
  canvas.addEventListener('wheel', (e) => { cam.distTarget = clamp(cam.distTarget * (e.deltaY > 0 ? 1.12 : 0.89), 2.5, 40); }, { passive: true });
  function look(dx, dy, k) {
    cam.yaw -= dx * k * settings.sensitivity;
    cam.pitch -= dy * k * settings.sensitivity * (settings.invertY ? -1 : 1);
    cam.pitch = clamp(cam.pitch, -1.35, 0.9);
  }

  /* touch: a stick on the left, look by dragging on the right */
  const touch = { stick: null, look: null, mx: 0, mz: 0, sprint: false };
  if (isTouch) {
    const stickEl = $('stick'), knob = $('knob');
    canvas.addEventListener('touchstart', (e) => {
      for (const t of e.changedTouches) {
        if (t.clientX < window.innerWidth * 0.45 && !touch.stick) {
          touch.stick = { id: t.identifier, x: t.clientX, y: t.clientY };
          stickEl.style.left = `${t.clientX}px`; stickEl.style.top = `${t.clientY}px`; stickEl.classList.add('on');
        } else if (!touch.look) touch.look = { id: t.identifier, x: t.clientX, y: t.clientY };
      }
      e.preventDefault();
    }, { passive: false });
    canvas.addEventListener('touchmove', (e) => {
      for (const t of e.changedTouches) {
        if (touch.stick && t.identifier === touch.stick.id) {
          let dx = t.clientX - touch.stick.x, dy = t.clientY - touch.stick.y;
          const l = Math.hypot(dx, dy), max = 46;
          if (l > max) { dx *= max / l; dy *= max / l; }
          knob.style.transform = `translate(${dx}px, ${dy}px)`;
          touch.mx = dx / max; touch.mz = dy / max;
        } else if (touch.look && t.identifier === touch.look.id) {
          look(t.clientX - touch.look.x, t.clientY - touch.look.y, 0.006);
          touch.look.x = t.clientX; touch.look.y = t.clientY;
        }
      }
      e.preventDefault();
    }, { passive: false });
    const end = (e) => {
      for (const t of e.changedTouches) {
        if (touch.stick && t.identifier === touch.stick.id) { touch.stick = null; touch.mx = touch.mz = 0; knob.style.transform = ''; stickEl.classList.remove('on'); }
        if (touch.look && t.identifier === touch.look.id) touch.look = null;
      }
    };
    canvas.addEventListener('touchend', end);
    canvas.addEventListener('touchcancel', end);
    const btn = (id, code) => {
      const el = $(id);
      el.addEventListener('touchstart', (e) => { e.preventDefault(); pressed.add(code); keys.add(code); el.classList.add('on'); }, { passive: false });
      el.addEventListener('touchend', (e) => { e.preventDefault(); keys.delete(code); el.classList.remove('on'); }, { passive: false });
    };
    btn('tJump', 'Space'); btn('tUse', 'KeyE'); btn('tMap', 'KeyM'); btn('tHorn', 'KeyH');
    $('tSprint').addEventListener('touchstart', (e) => { e.preventDefault(); touch.sprint = !touch.sprint; $('tSprint').classList.toggle('on', touch.sprint); }, { passive: false });
  }

  /* -------------------------------------------------------- the moves */
  function inputDir() {
    let ix = 0, iz = 0;
    if (down('KeyW', 'ArrowUp')) iz -= 1;
    if (down('KeyS', 'ArrowDown')) iz += 1;
    if (down('KeyA', 'ArrowLeft')) ix -= 1;
    if (down('KeyD', 'ArrowRight')) ix += 1;
    ix += touch.mx; iz += touch.mz;
    const l = Math.hypot(ix, iz);
    if (l > 1) { ix /= l; iz /= l; }
    // relative to where the camera looks
    const s = Math.sin(cam.yaw), c = Math.cos(cam.yaw);
    return { x: ix * c + iz * s, z: -ix * s + iz * c, mag: Math.min(1, l) };
  }

  function startDrop() {
    newBlimpPath();
    state = 'blimp';
    const [x, y, z] = blimpPos();
    Object.assign(body, { x, y, z, vx: 0, vy: 0, vz: 0 });
    cam.distTarget = cam.dist = 110;
    cam.yaw = Math.atan2(-(blimp.to[0] - blimp.from[0]), -(blimp.to[1] - blimp.from[1])) + 0.5;
    cam.pitch = -0.3;
    prompt('SPACE · Jump out of the blimp', 'Jump');
    setStatus('Pick your spot and jump — the blimp crosses the whole map.');
  }

  function jumpFrom(x, y, z) {
    Object.assign(body, { x, y, z, vx: 0, vy: 0, vz: 0 });
    state = 'freefall';
    cam.distTarget = 9;
    prompt('SPACE · Open parachute', 'Chute');
    setStatus('');
  }

  function dropAt(m) {
    hud.closeMap();
    if (car) leaveCar();
    jumpFrom(m.x, Math.max(W.groundHeight(m.x, m.z), 0) + 650, m.z);
    hud.clearMarker();
  }

  function enterCar(c) {
    car = c;
    state = 'drive';
    player.root.visible = false;
    cam.distTarget = 10;
    prompt(`E · Get out${c.T.hop ? ' · SPACE · Hop' : c.T.police ? ' · F · Siren' : ''}`, 'Exit');
    hud.showLocation({ poi: c.T.name, district: 'You got a ride' }, true);
  }
  function leaveCar() {
    const c = car;
    car = null;
    const sx = Math.sin(c.yaw), cx = Math.cos(c.yaw);
    // step out on the driver's side, or the other if that's blocked
    for (const side of [1, -1]) {
      const x = c.x - sx * (c.T.w / 2 + 1.2) * side, z = c.z - cx * (c.T.w / 2 + 1.2) * side;
      Object.assign(body, { x, z, y: Math.max(c.y, W.groundHeight(x, z)) + 0.2, vx: 0, vy: 0, vz: 0 });
      collideBody(W, body);
      if (Math.hypot(body.x - x, body.z - z) < 0.5) break;
    }
    state = 'walk';
    player.root.visible = true;
    cam.distTarget = 6.5;
    prompt('');
  }

  /* ----------------------------------------------------- per-state step */
  const GRAV = 24;
  function stepPlayer(dt) {
    const d = inputDir();
    const sprint = down('ShiftLeft', 'ShiftRight') || touch.sprint;
    const water = W.waterLevelAt(body.x, body.z);

    if (state === 'freefall' || state === 'chute') {
      const chute = state === 'chute';
      const hs = chute ? 12 : 26;
      body.vx = lerp(body.vx, d.x * hs, dt * (chute ? 1.6 : 1.2));
      body.vz = lerp(body.vz, d.z * hs, dt * (chute ? 1.6 : 1.2));
      const dive = !chute && down('KeyW', 'ArrowUp') ? 1.25 : 1;
      body.vy = chute ? lerp(body.vy, -8, dt * 2.5) : Math.max(body.vy - GRAV * dt, -50 * dive);
      body.x += body.vx * dt; body.y += body.vy * dt; body.z += body.vz * dt;
      clampToMap(body);
      const ground = Math.max(W.groundHeight(body.x, body.z), water ?? -99);
      if (!chute && (hit('Space') || body.y - ground < 110)) { state = 'chute'; prompt(''); }
      collideBody(W, body);
      if (water !== null && body.y < water - 0.9) { state = 'swim'; body.y = water - 1.1; body.vy = 0; }
      else if (body.onGround) { state = 'walk'; prompt(''); cam.distTarget = 6.5; }
      if (Math.hypot(body.vx, body.vz) > 1) facing = Math.atan2(body.vx, body.vz);
      moveSpeed = 0;
      return;
    }

    if (state === 'swim') {
      const sp = sprint ? 4.2 : 2.8;
      body.vx = lerp(body.vx, d.x * sp, dt * 3); body.vz = lerp(body.vz, d.z * sp, dt * 3);
      body.x += body.vx * dt; body.z += body.vz * dt;
      clampToMap(body);
      const lvl = W.waterLevelAt(body.x, body.z);
      const g = W.groundHeight(body.x, body.z);
      body.y = lvl !== null ? lvl - 1.1 : g;
      body.vy = 0;
      collideBody(W, body);
      if (lvl === null || g > (lvl ?? 0) - 1.1) { state = 'walk'; body.y = Math.max(body.y, g); }
      moveSpeed = Math.hypot(body.vx, body.vz);
      if (moveSpeed > 0.3) facing = Math.atan2(body.vx, body.vz);
      return;
    }

    // on foot
    const sp = d.mag * (sprint ? 9.5 : 5.2);
    const accel = body.onGround ? 14 : 3;
    body.vx = lerp(body.vx, d.x * sp, Math.min(1, dt * accel));
    body.vz = lerp(body.vz, d.z * sp, Math.min(1, dt * accel));
    if (body.onGround && hit('Space')) { body.vy = 8.2; body.onGround = false; }
    body.vy -= GRAV * dt;
    body.x += body.vx * dt; body.y += body.vy * dt; body.z += body.vz * dt;
    clampToMap(body);
    collideBody(W, body);
    ambient.push(body, body.r);
    if (water !== null && body.y < water - 1.15) { state = 'swim'; body.y = water - 1.1; }
    moveSpeed = Math.hypot(body.vx, body.vz);
    if (moveSpeed > 0.4) facing = Math.atan2(body.vx, body.vz);

    // a car close by?
    const near = nearestCar();
    if (near && near.d < 4.2) prompt(`E · Drive the ${near.c.T.name}`, 'Drive');
    else if (curPrompt.startsWith('E · Drive')) prompt('');
    if (near && near.d < 4.2 && hit('KeyE')) enterCar(near.c);
  }

  function nearestCar() {
    let best = null;
    for (const c of cars) {
      const d = Math.hypot(c.x - body.x, c.z - body.z);
      if (Math.abs(c.y - body.y) < 3 && (!best || d < best.d)) best = { c, d };
    }
    return best;
  }

  /* An arcade car: grip, drift a little, climb kerbs and ramps, bounce off walls. */
  const carBody = { x: 0, y: 0, z: 0, vx: 0, vy: 0, vz: 0, r: 1.6, h: 1.4, step: 0.75, onGround: true, groundY: 0, hitWall: 0 };
  function stepCar(c, dt, drive) {
    const T = c.T;
    let throttle = 0, steer = 0;
    if (drive) {
      if (down('KeyW', 'ArrowUp')) throttle += 1;
      if (down('KeyS', 'ArrowDown')) throttle -= 1;
      if (down('KeyA', 'ArrowLeft')) steer += 1;
      if (down('KeyD', 'ArrowRight')) steer -= 1;
      throttle -= touch.mz; steer -= touch.mx;
      throttle = clamp(throttle, -1, 1); steer = clamp(steer, -1, 1);
    }
    const brake = drive && down('Space') && !T.hop;
    if (throttle > 0) c.speed += (c.speed < 0 ? 30 : T.accel) * throttle * dt;
    else if (throttle < 0) c.speed += (c.speed > 0 ? 30 : 8) * throttle * dt;
    else c.speed *= Math.pow(c.onGround === false ? 0.995 : 0.55, dt);
    if (brake) c.speed *= Math.pow(0.05, dt);
    c.speed = clamp(c.speed, -8, T.top);
    c.steer = lerp(c.steer, steer, Math.min(1, dt * 6));
    const turn = c.steer * clamp(c.speed / 8, -1, 1) * (1 - Math.min(0.6, Math.abs(c.speed) / T.top * 0.6)) * 1.05;
    c.yaw += turn * dt * (brake ? 1.5 : 1);
    const fx = -Math.sin(c.yaw), fz = -Math.cos(c.yaw);
    // the car's nose points along +x of the model; yaw is the model's rotation.y
    const dx = Math.cos(c.yaw), dz = -Math.sin(c.yaw);
    void fx; void fz;
    const nx = c.x + dx * c.speed * dt, nz = c.z + dz * c.speed * dt;
    if (W.isSea(nx, nz) && W.groundHeight(nx, nz) < -0.6) { c.speed *= -0.2; }
    else { c.x = nx; c.z = nz; }
    c.x = clamp(c.x, -W.half + 5, W.half - 5); c.z = clamp(c.z, -W.half + 5, W.half - 5);
    Object.assign(carBody, { x: c.x, y: c.y, z: c.z, vx: dx * c.speed, vz: dz * c.speed, vy: c.vy, hitWall: 0 });
    carBody.vy -= GRAV * dt;
    carBody.y += carBody.vy * dt;
    collideBody(W, carBody);
    if (drive && ambient.push(carBody, 1.1)) { c.speed *= Math.pow(0.15, dt); carBody.hitWall = Math.max(carBody.hitWall, 1); }
    if (carBody.hitWall > 3) c.speed *= 0.35;
    else if (carBody.hitWall > 0) c.speed *= Math.pow(0.4, dt);
    c.x = carBody.x; c.z = carBody.z; c.y = carBody.y; c.vy = carBody.vy; c.onGround = carBody.onGround;
    if (T.hop && drive && hit('Space') && c.onGround) c.hop = 1;
    if (T.police && drive && hit('KeyF')) c.siren = !c.siren;
    placeCar(c, dt);
  }

  function placeCar(c, dt) {
    const m = c.model;
    const L = c.T.len * 0.4, Wd = c.T.w * 0.45;
    const dx = Math.cos(c.yaw), dz = -Math.sin(c.yaw);
    const h = (ox, oz) => Math.max(W.groundHeight(c.x + ox, c.z + oz), c.y - 0.6);
    const front = h(dx * L, dz * L), back = h(-dx * L, -dz * L);
    const left = h(-dz * Wd, dx * Wd), right = h(dz * Wd, -dx * Wd);
    const tp = c.onGround !== false ? Math.atan2(front - back, L * 2) : c.pitch;
    const tr = c.onGround !== false ? Math.atan2(left - right, Wd * 2) : c.roll;
    c.pitch = lerp(c.pitch, clamp(tp, -0.5, 0.5), Math.min(1, dt * 10));
    c.roll = lerp(c.roll, clamp(tr, -0.4, 0.4), Math.min(1, dt * 10));
    m.root.position.set(c.x, c.y, c.z);
    m.root.rotation.set(0, c.yaw, 0, 'YXZ');
    m.root.rotateZ(c.pitch);
    m.root.rotateX(-c.roll);
    for (const w of m.wheels) {
      w.spin.rotation.z -= (c.speed * dt) / m.radius;
      if (w.front) w.steer.rotation.y = c.steer * 0.45;
    }
    if (c.hop > 0) {
      c.hop = Math.max(0, c.hop - dt * 1.6);
      m.chassis.position.y = Math.sin(c.hop * Math.PI) * 1.4;
      m.chassis.rotation.z = Math.sin(c.hop * Math.PI * 2) * 0.12;
    } else { m.chassis.position.y = 0; m.chassis.rotation.z = 0; }
    const s = m.root.userData.siren;
    if (s) {
      const on = c.siren ? Math.floor(clock * 6) % 2 : -1;
      s.red.visible = on !== 1; s.blue.visible = on !== 0;
    }
  }

  function clampToMap(b) {
    b.x = clamp(b.x, -W.half + 4, W.half - 4);
    b.z = clamp(b.z, -W.half + 4, W.half - 4);
  }

  /* ----------------------------------------------------------- camera */
  const tmp = new THREE.Vector3();
  function updateCamera(dt) {
    let tx, ty, tz;
    if (state === 'blimp') { const [bx, by, bz] = blimpPos(); tx = bx; ty = by - 6; tz = bz; }
    else if (state === 'drive') { tx = car.x; ty = car.y + 2.2; tz = car.z; }
    else { tx = body.x; ty = state === 'swim' ? (W.waterLevelAt(body.x, body.z) ?? body.y) + 0.9 : body.y + 1.6; tz = body.z; }
    if (state === 'drive' && !keysLooking()) {
      // swing round behind the car when you're not steering the view
      const behind = car.yaw - Math.PI / 2;
      let dy = behind - cam.yaw;
      while (dy > Math.PI) dy -= Math.PI * 2;
      while (dy < -Math.PI) dy += Math.PI * 2;
      if (Math.abs(car.speed) > 2) cam.yaw += dy * Math.min(1, dt * 1.8);
    }
    cam.dist = lerp(cam.dist, cam.distTarget, Math.min(1, dt * 3));
    const cp = Math.cos(cam.pitch), sp = Math.sin(cam.pitch);
    const back = tmp.set(Math.sin(cam.yaw) * cp, -sp, Math.cos(cam.yaw) * cp);
    const shoulder = state === 'walk' || state === 'swim' ? 0.55 : 0;
    const ox = Math.cos(cam.yaw) * shoulder, oz = -Math.sin(cam.yaw) * shoulder;
    let d = cam.dist;
    if (state !== 'blimp') {
      const f = rayClear(W, tx + ox, ty, tz + oz, tx + ox + back.x * d, ty + back.y * d, tz + oz + back.z * d, 0.3);
      d = Math.max(1.2, d * f);
    }
    let cx = tx + ox + back.x * d, cy = ty + back.y * d, cz = tz + oz + back.z * d;
    const g = W.groundHeight(cx, cz) + 0.6;
    if (cy < g) cy = g;
    const water = W.waterLevelAt(cx, cz);
    if (water !== null && cy < water + 0.5) cy = water + 0.5;
    camera.position.set(cx, cy, cz);
    camera.lookAt(tx + ox, ty, tz + oz);
  }
  let lookTimer = 0;
  document.addEventListener('mousemove', () => { if (document.pointerLockElement === canvas) lookTimer = 1.2; });
  const keysLooking = () => lookTimer > 0 || !!touch.look;

  /* -------------------------------------------------------------- HUD */
  let curPrompt = '';
  function prompt(text, touchLabel) {
    curPrompt = text;
    $('prompt').textContent = text;
    $('prompt').hidden = !text;
    if (touchLabel) $('tJump').textContent = touchLabel === 'Drive' || touchLabel === 'Exit' ? 'Jump' : touchLabel;
    else $('tJump').textContent = 'Jump';
  }
  function setStatus(t) { $('status').textContent = t; $('status').hidden = !t; }

  let hours = settings.startHour, clock = 0, locTimer = 0, fpsAcc = 0, fpsN = 0, fps = 60;
  const fmtTime = (h) => {
    const hh = Math.floor(h) % 24, mm = Math.floor((h % 1) * 60);
    return `${((hh + 11) % 12) + 1}:${String(mm).padStart(2, '0')} ${hh < 12 ? 'AM' : 'PM'}`;
  };

  function hudFrame(dt) {
    const p = state === 'drive' ? car : state === 'blimp' ? { x: blimpPos()[0], z: blimpPos()[2] } : body;
    hud.drawCompass(cam.yaw);
    hud.drawMinimap(p.x, p.z, state === 'drive' ? car.yaw - Math.PI / 2 : state === 'blimp' ? cam.yaw : facing + Math.PI, {
      vehicles: cars.filter((c) => c !== car),
      marker: hud.marker,
      radius: state === 'blimp' || state === 'freefall' ? 1100 : state === 'drive' ? 600 : 380
    });
    locTimer -= dt;
    if (locTimer <= 0) {
      locTimer = 0.3;
      if (state !== 'title') hud.showLocation(locationAt(W, p.x, p.z));
      const ll = unprojectLL(p.x, p.z);
      $('coords').textContent = `${gridRef(p.x, p.z)} · ${ll}`;
    }
    hud.tick(dt);
    const alt = state === 'blimp' ? blimp.alt : body.y - Math.max(W.groundHeight(body.x, body.z), 0);
    $('meter').hidden = !(state === 'drive' || state === 'freefall' || state === 'chute' || state === 'blimp');
    if (state === 'drive') $('meter').innerHTML = `<b>${Math.round(Math.abs(car.speed) * 2.237)}</b> mph`;
    else if (state !== 'walk' && state !== 'swim') $('meter').innerHTML = `<b>${Math.max(0, Math.round(alt))}</b> m`;
    $('clock').textContent = fmtTime(hours);
    fpsAcc += dt; fpsN++;
    if (fpsAcc > 0.5) {
      fps = Math.round(fpsN / fpsAcc); fpsAcc = 0; fpsN = 0;
      $('fps').textContent = `${fps} fps · ${Math.round(pixelRatio * 100)}% resolution`;
      // keep it smooth: drop the resolution when frames get slow, raise it back when there's room
      if (fps < 38 && pixelRatio > 0.55) setPixelRatio(pixelRatio * 0.85);
      else if (fps > 57 && pixelRatio < maxRatio) setPixelRatio(Math.min(maxRatio, pixelRatio * 1.08));
    }
  }
  const gridRef = (x, z) => `${'ABCDEFGH'[clamp(Math.floor(((x + W.half) * 8) / W.size), 0, 7)]}${clamp(Math.floor(((z + W.half) * 8) / W.size), 0, 7) + 1}`;
  function unprojectLL(x, z) {
    const [lat, lon] = W.toLatLon ? W.toLatLon(x, z) : [34.02 - z / 17600, -118.355 + x / 14600];
    return `${lat.toFixed(3)}°N ${Math.abs(lon).toFixed(3)}°W`;
  }

  /* ------------------------------------------------------- main loop */
  let last = performance.now(), running = false;
  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    update(dt);
    R.render(camera);
    if (state !== 'title') hudFrame(dt);
    if (hud.mapOpen) hud.drawMap();
    if (running) requestAnimationFrame(frame);
  }

  /* One step of the world: input, physics, animation, camera. */
  function update(dt) {
    clock += dt;
    lookTimer -= dt;
    hours = (hours + (dt * 24) / (settings.dayLength * 60)) % 24;
    R.setTime(hours);

    if (hit('KeyM') && state !== 'title') toggleMap();
    if (hit('KeyT')) hours = (Math.floor(hours / 3) * 3 + 3) % 24;
    if (hit('KeyC')) cam.distTarget = cam.distTarget > 8 ? (state === 'drive' ? 10 : 6.5) : 18;
    if (hit('KeyH') && state === 'drive') horn();
    if (hit('KeyF') && state !== 'drive') $('fps').hidden = !$('fps').hidden;
    if (hit('Escape') && hud.mapOpen) toggleMap();

    if (!hud.mapOpen || state === 'blimp') {
      if (state === 'blimp') {
        blimp.t += blimp.speed * dt;
        const [x, y, z] = blimpPos();
        if (hit('Space') || blimp.t >= blimp.len - 300) jumpFrom(x, y - 14, z);
      } else if (state === 'drive') {
        stepCar(car, dt, true);
        if (hit('KeyE') && Math.abs(car.speed) < 12) leaveCar();
      } else if (state !== 'title') {
        stepPlayer(dt);
      }
    }
    // idle cars settle where they are; recently driven ones coast to a stop
    for (const c of cars) if (c !== car && (Math.abs(c.speed) > 0.05 || c.hop > 0 || !c.onGround)) stepCar(c, dt, false);

    // the blimp drifts across the sky even when you're not on it
    if (state !== 'blimp') { blimp.t += blimp.speed * dt * 0.6; if (blimp.t > blimp.len) newBlimpPath(); }
    const [bx, by, bz] = blimpPos();
    blimp.mesh.position.set(bx, by, bz);
    blimp.mesh.rotation.y = Math.atan2(-(blimp.to[1] - blimp.from[1]), blimp.to[0] - blimp.from[0]);
    blimp.mesh.userData.prop.rotation.x += dt * 20;

    if (state !== 'drive' && state !== 'blimp' && state !== 'title') {
      player.root.visible = true;
      player.root.position.set(body.x, body.y, body.z);
      player.root.rotation.y = facing;
      player.animate(dt, state === 'walk' && !body.onGround && body.vy < -3 ? 'air' : state, moveSpeed);
    } else if (state === 'blimp' || state === 'title') player.root.visible = false;

    if (state === 'title') { cam.yaw += dt * 0.04; titleCamera(); }
    else updateCamera(dt);
    const focus = state === 'drive' ? car : state === 'blimp' ? { x: bx, y: by, z: bz } : state === 'title' ? { x: camera.position.x, y: 0, z: camera.position.z } : body;
    ambient.update(dt, state === 'drive' ? car : body, R.uniforms.uNight.value);
    R.update(dt, clock, camera, focus);
    pressed.clear();
  }

  function titleCamera() {
    if (W.title) {
      const t = W.title, cx = t.x + Math.sin(cam.yaw) * t.r, cz = t.z + Math.cos(cam.yaw) * t.r;
      camera.position.set(cx, W.groundHeight(cx, cz) + t.h, cz);
      camera.lookAt(t.look[0], W.groundHeight(t.look[0], t.look[2]) + t.look[1], t.look[2]);
      return;
    }
    const r = 900;
    const cx = -200 + Math.sin(cam.yaw) * r, cz = -700 + Math.cos(cam.yaw) * r;
    camera.position.set(cx, Math.max(W.groundHeight(cx, cz), 0) + 260, cz);
    camera.lookAt(500, 60, -900);
  }

  function toggleMap() {
    if (hud.mapOpen) { hud.closeMap(); return; }
    if (document.pointerLockElement) document.exitPointerLock();
    hud.openMap({
      player: state === 'drive' ? car : state === 'blimp' ? { x: blimpPos()[0], z: blimpPos()[2] } : body,
      yaw: state === 'drive' ? car.yaw - Math.PI / 2 : facing + Math.PI,
      vehicles: cars,
      blimp: state === 'blimp' ? blimp : null,
      locate: (x, z) => locationAt(W, x, z)
    }, dropAt, state !== 'blimp');
  }

  let audio = null;
  function horn() {
    try {
      audio ||= new AudioContext();
      const o = audio.createOscillator(), o2 = audio.createOscillator(), g = audio.createGain();
      const lowrider = car && car.T.hop;
      o.type = 'square'; o2.type = 'square';
      o.frequency.value = lowrider ? 392 : 350; o2.frequency.value = lowrider ? 494 : 440;
      g.gain.setValueAtTime(0.06, audio.currentTime);
      g.gain.exponentialRampToValueAtTime(0.001, audio.currentTime + 0.45);
      o.connect(g); o2.connect(g); g.connect(audio.destination);
      o.start(); o2.start(); o.stop(audio.currentTime + 0.45); o2.stop(audio.currentTime + 0.45);
    } catch { /* no audio, no horn */ }
  }

  const maxRatio = Math.min(window.devicePixelRatio || 1, settings.quality === 'high' ? 2 : settings.quality === 'medium' ? 1.5 : 1);
  let pixelRatio = maxRatio;
  function setPixelRatio(r) { pixelRatio = r; renderer.setPixelRatio(r); resize(); }
  function resize() {
    const w = window.innerWidth, h = window.innerHeight;
    renderer.setSize(w, h, false);
    R.setSize(w, h);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  window.addEventListener('resize', resize);
  resize();

  /* ------------------------------------------------------ title & menu */
  const showHud = () => { $('title').hidden = true; $('hud').hidden = false; $('touch').hidden = !isTouch; };
  $('playBtn').addEventListener('click', () => {
    showHud();
    startDrop();
    if (!isTouch) canvas.requestPointerLock?.();
  });
  $('mapBtn').addEventListener('click', () => {
    showHud();
    state = 'walk';
    const s = W.start || { x: -2060, z: 240 };
    Object.assign(body, { x: s.x, z: s.z, y: W.groundHeight(s.x, s.z) });
    hud.openMap({ player: body, yaw: 0, vehicles: cars, blimp: null, locate: (x, z) => locationAt(W, x, z) }, dropAt, false);
  });
  $('quality').value = settings.quality;
  $('quality').addEventListener('change', (e) => { settings.quality = e.target.value; saveSettings(); location.reload(); });
  $('sens').value = settings.sensitivity;
  $('sens').addEventListener('input', (e) => { settings.sensitivity = Number(e.target.value); saveSettings(); });
  $('invert').checked = settings.invertY;
  $('invert').addEventListener('change', (e) => { settings.invertY = e.target.checked; saveSettings(); });
  $('helpBtn').addEventListener('click', () => { $('help').hidden = !$('help').hidden; });
  $('helpClose').addEventListener('click', () => { $('help').hidden = true; });
  $('mapClose').addEventListener('click', () => hud.closeMap());

  return {
    camera,
    start() { running = true; last = performance.now(); requestAnimationFrame(frame); },
    // for automated checks
    debug: {
      W, body, cars, cam, R, CAR_TYPES, startDrop, dropAt, enterCar, leaveCar,
      get state() { return state; },
      get car() { return car; },
      /* run the world forward without drawing, holding some keys down */
      simulate(seconds, hold = [], tap = []) {
        for (const k of hold) keys.add(k);
        for (const k of tap) pressed.add(k);
        for (let t = 0; t < seconds; t += 1 / 30) {
          update(1 / 30);
          for (const k of hold) keys.add(k);
        }
        for (const k of hold) keys.delete(k);
      },
      setHours: (h) => { hours = h; },
      /* stand somewhere, looking a given way (yaw 0 = north) */
      place(x, z, yaw = 0, pitch = -0.2, dist = 6.5) {
        if (car) leaveCar();
        showHud(); hud.closeMap();
        state = 'walk';
        Object.assign(body, { x, z, y: Math.max(W.groundHeight(x, z), 0) + 0.5, vx: 0, vy: 0, vz: 0 });
        collideBody(W, body);
        Object.assign(cam, { yaw, pitch, dist, distTarget: dist });
        prompt(''); setStatus('');
      }
    }
  };
}

boot().catch((err) => {
  console.error(err);
  $('loadText').textContent = `Something went wrong: ${err.message}`;
});
