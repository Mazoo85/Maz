/*
 * Boot: load the data and the id map, build the simulation and the globe, wire
 * the interface, run the loop. Nothing in here knows game rules; it passes
 * commands to the sim and paints what the sim says.
 */
import { loadData } from './data/load.js';
import { newGame } from './sim/state.js';
import { applyCommand, validateCommand } from './sim/commands.js';
import { tick } from './sim/tick.js';
import { hourToDate, isNewMonth, SPEED_HOURS_PER_SECOND } from './sim/clock.js';
import { weatherAt } from './sim/weather.js';
import { createGlobe } from './view/globe.js';
import { createCamera } from './view/camera.js';
import { paintColors, paintInfo, paintFlags } from './view/mapmodes.js';
import { attachInput } from './view/input.js';
import { loadModels, createOverlays } from './view/overlays.js';
import { createTopbar } from './view/panels/topbar.js';
import { showProvince, showState, showCountry, showSaves, closePanel, toast } from './view/panels/panel.js';
import { createSearch } from './view/panels/search.js';
import { listSaves, saveLocal, loadLocal, deleteLocal, autosave, exportFile, importFile } from './view/saves.js';

const boot = document.getElementById('boot');
const say = (msg) => { boot.textContent = msg; };
const fail = (e) => { console.error(e); boot.classList.add('error'); boot.textContent = 'War Room could not start.\n\n' + (e && e.message || e); };

function loadImage(url) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('could not load ' + url));
    img.src = url;
  });
}

async function main() {
  const canvas = document.getElementById('globe');
  if (!canvas.getContext('webgl2') && !canvas.getContext('webgl')) throw new Error('This device cannot draw the map (no WebGL). See the README for device notes.');
  say('Reading the world of 1936…');
  const [data, idImage, reliefImage] = await Promise.all([
    loadData('./data/', async (url) => { const r = await fetch(url); if (!r.ok) throw new Error(r.status + ' ' + r.statusText); return r.text(); }),
    loadImage('./data/map/provinces.png'),
    loadImage('./data/map/relief.png').catch((e) => { console.warn(e.message); return null; }),
  ]);
  say('Setting up the globe…');
  const url = new URL(location.href);
  const seed = url.searchParams.get('seed') || String(Date.now());
  let state = newGame(data, { seed });
  try { if (listSaves().some((s) => s.name === 'autosave') && url.searchParams.get('fresh') === null) state = loadLocal('autosave', data); } catch (e) { console.warn('autosave not loaded:', e.message); }

  const globe = createGlobe({ canvas, idImage, reliefImage, provinceCount: data.provinceCount });
  const cam = createCamera(globe, canvas);
  const colors = new Uint8Array(128 * 128 * 4), info = new Uint8Array(128 * 128 * 4), flags = new Uint8Array(128 * 128 * 4);
  let mode = 'political';
  const repaint = () => { globe.setColors(paintColors(mode, state, data, colors)); };
  const reinfo = () => { globe.setInfo(paintInfo(state, data, info)); };
  reinfo(); repaint();
  globe.setFlags(flags);

  say('Placing the armies…');
  let overlays = null;
  try {
    const models = await loadModels('./models/', ['division']);
    overlays = createOverlays(globe, data, models);
    overlays.setArmies(state);
  } catch (e) { console.warn('counters not shown:', e.message); }

  // ---- commands -------------------------------------------------------------
  const send = (cmd) => {
    const reason = validateCommand(state, cmd);
    if (reason) { toast(reason); return false; }
    applyCommand(state, cmd);
    return true;
  };

  // ---- panels ---------------------------------------------------------------
  let selected = 0;
  const ctx = () => ({ state, data, send, weather: weatherAt(state, data, selected), onProvince: openProvince, onState: openState, onCountry: openCountry });
  function highlight(ids, dim) { globe.setFlags(paintFlags(ids, dim, data.provinceCount, flags)); }
  function openProvince(id, fly = true) {
    selected = id; globe.setSelected(id); highlight([], false);
    showProvince(id, ctx());
    if (fly) { const p = data.provinces[id]; cam.flyTo(p.lat, p.lon, Math.min(cam.distance(), 1.35)); }
  }
  function openState(id) {
    const st = data.stateById[id];
    selected = 0; globe.setSelected(0); highlight(st.provinces, false);
    showState(id, ctx());
    const p = data.provinces[st.capital_province]; cam.flyTo(p.lat, p.lon, Math.min(cam.distance(), 1.6));
  }
  function openCountry(tag) {
    const c = state.countries[tag];
    const ids = c.states.flatMap((sid) => data.stateById[sid].provinces);
    selected = 0; globe.setSelected(0); highlight(ids, true);
    showCountry(tag, ctx());
    const p = data.provinces[data.byTag[tag].capital_province]; cam.flyTo(p.lat, p.lon, Math.max(cam.distance(), 2.2));
  }
  function openSaves() {
    showSaves({
      list: listSaves(), current: state.player ? state.countries[state.player].name : 'My game',
      onSave: (name) => { try { saveLocal(name, state); toast('Saved "' + name + '"'); openSaves(); } catch (e) { toast(e.message, 5000); } },
      onLoad: (name) => { try { replaceState(loadLocal(name, data)); toast('Loaded "' + name + '"'); closePanel(); } catch (e) { toast(e.message, 5000); } },
      onDelete: (name) => { deleteLocal(name); openSaves(); },
      onExport: (name) => exportFile(name, state).catch((e) => toast(e.message, 5000)),
      onImport: (file) => importFile(file, data).then((s) => { replaceState(s); toast('Loaded ' + file.name); closePanel(); }).catch((e) => toast(e.message, 6000)),
    });
  }
  function replaceState(s) { state = s; reinfo(); repaint(); if (overlays) overlays.setArmies(state); topbar.update(state); }

  const topbar = createTopbar({
    send,
    onMode: (m) => { mode = m; repaint(); },
    onShowCountry: () => { if (state.player) openCountry(state.player); else toast('Pick a country: tap one on the map, then "Play as"'); },
    onShowSaves: openSaves,
  });
  createSearch(data, { onProvince: openProvince, onState: openState, onCountry: openCountry });
  attachInput(canvas, {
    onPick: (x, y) => { const id = globe.pick(x, y); if (id) openProvince(id, false); else { closePanel(); selected = 0; globe.setSelected(0); highlight([], false); } },
    onHover: (x, y) => { globe.setHover(x < 0 ? 0 : globe.pick(x, y)); },
  });
  window.addEventListener('resize', () => globe.resize());
  document.addEventListener('visibilitychange', () => { if (document.hidden) { try { autosave(state); } catch { /* ignore */ } } });

  // ---- the loop -------------------------------------------------------------
  let last = performance.now(), acc = 0, lastWeatherPaint = state.hour;
  function frame(now) {
    const dt = Math.min(0.25, (now - last) / 1000); last = now;
    const perSec = SPEED_HOURS_PER_SECOND[state.speed];
    if (perSec > 0) {
      acc += dt * perSec;
      const budget = performance.now() + 12;
      try {
        while (acc >= 1 && performance.now() < budget) {
          tick(state, data); acc -= 1;
          if (isNewMonth(state.hour)) { try { autosave(state); } catch { /* storage full; ignore */ } }
        }
      } catch (e) {
        state.speed = 0;
        console.error(e);
        toast('Something went wrong at ' + hourToDate(state.hour).year + '; the game is paused and your last save is safe.', 8000);
      }
      if (acc > perSec) acc = perSec;   // never try to catch up more than a second
    } else acc = 0;
    if (state.dirty.regions && mode === 'weather' && state.hour !== lastWeatherPaint) { repaint(); lastWeatherPaint = state.hour; state.dirty.regions = false; }
    const d = hourToDate(state.hour);
    globe.setSun(d.hour, d.dayOfYear);
    const level = cam.zoomLevel();
    globe.setBorderLevel(level === 'globe' ? 0 : level === 'continent' ? 1 : 2);
    cam.update(dt);
    if (overlays) overlays.update(cam.distance());
    topbar.update(state);
    globe.render();
    requestAnimationFrame(frame);
  }
  topbar.update(state); topbar.show();
  boot.hidden = true;
  requestAnimationFrame(frame);
  window.warroom = { get state() { return state; }, data, send, globe, cam, openProvince, openState, openCountry };
}

main().catch(fail);
