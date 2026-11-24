// Terra Desk on iPhone (and any browser): the window.td bridge the desktop page used (preload.js), done in the page
// itself. Data services are called straight from the browser, with the same allow-list, polite gaps between
// requests, caching and fallbacks as the desktop app. Settings and built data stay on the phone. Loaded before core.js.
import { lightning } from './glm.js';
import { BATTLES_URL, VOLCANOES_URL, battlesFromSparql, volcanoesFromWfs } from './databuild.js';

export const VERSION = '1.1.0-iphone.1';
// adsb.lol does not answer web pages, so planes come through the GOD'S EYE VIEW planes relay (godview/relay/),
// which serves pages on mazoo85.github.io and asks adsb.lol, then OpenSky, then adsb.fi.
export const RELAY_HOST = 'godview-planes.cody-collins85-cc.workers.dev';
export function relayUrl(url) {
  const m = String(url).match(/^https:\/\/api\.adsb\.lol\/v2\/point\/(-?[\d.]+)\/(-?[\d.]+)\/\d+$/);
  return m ? `https://${RELAY_HOST}/planes?lat=${m[1]}&lng=${m[2]}` : null;
}
const MIN = 60e3, HOUR = 60 * MIN, DAY = 24 * HOUR;
// host -> [gap ms between requests, ttl ms an answer is kept]
export const HOSTS = {
  'api.adsb.lol': [1100, 8000], 'll.thespacedevs.com': [4000, 15 * MIN], 'services.swpc.noaa.gov': [500, 5 * MIN],
  'volcanoes.usgs.gov': [500, 10 * MIN], 'volcano.si.edu': [500, HOUR], 'router.project-osrm.org': [1100, HOUR],
  'nominatim.openstreetmap.org': [1100, DAY], 'overpass-api.de': [2000, 30 * MIN], 'overpass.kumi.systems': [2000, 30 * MIN],
  'overpass.private.coffee': [2000, 30 * MIN], 'hiking.waymarkedtrails.org': [1000, DAY], 'annotations.allmaps.org': [300, DAY],
  'api.allmaps.org': [300, DAY], 'allmaps.xyz': [300, DAY], 'noaa-goes19.s3.amazonaws.com': [200, 30e3], 'noaa-goes18.s3.amazonaws.com': [200, 30e3],
  'celestrak.org': [1000, 2 * HOUR], [RELAY_HOST]: [1100, 8000], 'query.wikidata.org': [1000, 0], 'webservices.volcano.si.edu': [500, 0], 'www.solarsystemscope.com': [300, 0],
};
const MIRRORS = { 'overpass-api.de': ['overpass.kumi.systems', 'overpass.private.coffee'] };

// ------------------------------------------------------------------ small storage helpers (storage may be off)
const LS = {
  get(k, d) { try { const v = localStorage.getItem(k); return v == null ? d : JSON.parse(v); } catch { return d; } },
  set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); return true; } catch { return false; } },
};
const CACHE = 'terradesk-data-v1';
async function cacheGet(key) { try { const c = await caches.open(CACHE); const r = await c.match(key); return r || null; } catch { return null; } }
async function cachePut(key, body, type) { try { const c = await caches.open(CACHE); await c.put(key, new Response(body, { headers: { 'content-type': type || 'application/octet-stream' } })); return true; } catch { return false; } }

// ------------------------------------------------------------------ settings (same masking as the desktop app)
export const DEFAULTS = { aisKey: '', openskyId: '', openskySecret: '', adsbKey: '', imagery: 'esri', labels: true, terrain: true, exaggeration: 1.2, lighting: false, units: 'metric', layers: [], camera: null, places: [], exportDir: '' };
export function loadSettings() { return Object.assign({}, DEFAULTS, LS.get('terradesk.settings', {})); }
export function forPage(s) {
  const o = Object.assign({}, s);
  o.aisKey = s.aisKey ? '••••' + s.aisKey.slice(-4) : ''; o.openskySecret = s.openskySecret ? '••••' : ''; o.adsbKey = s.adsbKey ? '••••' + s.adsbKey.slice(-4) : '';
  o.hasAis = !!s.aisKey; o.hasOpensky = !!(s.openskyId && s.openskySecret);
  return o;
}
let settings = loadSettings();
function setSettings(patch) {
  for (const [k, v] of Object.entries(patch || {})) { if (typeof v === 'string' && v.startsWith('••••')) continue; settings[k] = v; }
  LS.set('terradesk.settings', settings);
  if ('aisKey' in (patch || {}) && !String(patch.aisKey).startsWith('••••')) ais.setKey(settings.aisKey);
  return { ok: true, hasAis: !!settings.aisKey, hasOpensky: !!(settings.openskyId && settings.openskySecret) };
}

// ------------------------------------------------------------------ fetch for the page's data services
const lastHit = {}, mem = new Map(), inflight = new Map();
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function waitTurn(host, gap) {
  const now = Date.now(), next = (lastHit[host] || 0) + gap;
  lastHit[host] = Math.max(now, next);
  if (next > now) await sleep(next - now);
}
const b64 = buf => { const u = new Uint8Array(buf); let s = ''; for (let i = 0; i < u.length; i += 0x8000) s += String.fromCharCode.apply(null, u.subarray(i, i + 0x8000)); return btoa(s); };
const longCache = LS.get('terradesk.cache', {});
function saveLong() {
  const keys = Object.keys(longCache);
  while (keys.length > 120) delete longCache[keys.shift()];          // keep the newest; localStorage is small
  if (!LS.set('terradesk.cache', longCache)) { for (const k of keys.slice(0, keys.length >> 1)) delete longCache[k]; LS.set('terradesk.cache', longCache); }
}
export let rawFetch = (...a) => nativeFetch(...a);
export function _setRawFetch(f) { rawFetch = f; }               // tests

// Services that answer apps but refuse web pages; asking them from Safari only fills its error log.
export const PAGE_REFUSED = { 'volcano.si.edu': 'The Smithsonian weekly volcano report does not answer web pages.' };
async function fetchOne(url, init) {
  const u = new URL(url), rule = HOSTS[u.hostname];
  if (PAGE_REFUSED[u.hostname]) return { error: PAGE_REFUSED[u.hostname] };
  if (!rule || u.protocol !== 'https:') return { error: `${u.hostname} is not one of Terra Desk's data services` };
  const method = (init.method || 'GET').toUpperCase(), body = init.body == null ? null : String(init.body);
  const key = method + ' ' + url + (body ? ' ' + body.length + ':' + body.slice(0, 200) + body.slice(-200) : '');
  const c = mem.get(key) || longCache[key];
  if (c && rule[1] > 0 && Date.now() - c.at < rule[1] && !init.fresh) return Object.assign({}, c.res, { cached: true });
  if (inflight.has(key)) return inflight.get(key);
  const p = (async () => {
    await waitTurn(u.hostname, rule[0]);
    const headers = Object.assign({ accept: init.accept || '*/*' }, init.headers || {});
    let r;
    try {
      const ctl = new AbortController(), timer = setTimeout(() => ctl.abort(), init.timeout || 60000);
      try { r = await rawFetch(url, { method, headers, body, signal: ctl.signal, mode: 'cors', credentials: 'omit' }); }
      finally { clearTimeout(timer); }
    } catch (e) {
      if (c) return Object.assign({}, c.res, { cached: true, stale: true });
      return { error: navigator.onLine === false ? 'No internet connection.' : `${u.hostname} could not be reached from this phone.` };
    }
    const res = { status: r.status, ok: r.ok, type: r.headers.get('content-type') || '', binary: !!init.binary };
    res.body = init.binary ? b64(await r.arrayBuffer()) : await r.text();
    if (r.ok && rule[1] > 0) {
      const e = { at: Date.now(), res };
      mem.set(key, e); if (mem.size > 400) mem.delete(mem.keys().next().value);
      if (rule[1] >= 10 * MIN && !init.binary && res.body.length < 300000) { longCache[key] = e; saveLong(); }
    } else if (!r.ok && c) return Object.assign({}, c.res, { cached: true, stale: true });
    return res;
  })();
  inflight.set(key, p);
  try { return await p; } finally { inflight.delete(key); }
}
async function tdFetch(url, init = {}) {
  const relay = relayUrl(url);
  if (relay) {
    const r = await fetchOne(relay, {});
    if (r.error || !r.ok) return r.error ? r : { error: `The planes service answered HTTP ${r.status}.` };
    try { const j = JSON.parse(r.body); return Object.assign({}, r, { body: JSON.stringify({ ac: j.ac || [], stale: !!j.stale, source: j.source }) }); }
    catch { return { error: 'The planes service sent something unreadable.' }; }
  }
  if (/^https:\/\/api\.adsb\.lol\/v2\/mil/.test(url)) return { error: 'Worldwide military aircraft are not available on iPhone yet. Choose "Aircraft around where I\'m looking".' };
  let r;
  try { r = await fetchOne(url, init); } catch (e) { return { error: e.message }; }
  const host = new URL(url).hostname;
  for (const m of MIRRORS[host] || []) {
    if (!(r.error || r.status === 429 || r.status >= 500)) break;
    const alt = new URL(url); alt.hostname = m;
    try { r = await fetchOne(alt.toString(), init); } catch { /* next */ }
  }
  if (MIRRORS[host] && (r.error || r.status === 429 || r.status >= 500) && !r.stale)
    return { error: 'The map data service (Overpass) and its backups are busy or down right now. Try again in a few minutes.' };
  return r;
}

// ------------------------------------------------------------------ ships (aisstream.io), straight from the page
const handlers = {};
const emit = (ch, data) => { for (const fn of handlers[ch] || []) { try { fn(data); } catch (e) { console.error(e); } } };
export const ais = {
  url: 'wss://stream.aisstream.io/v0/stream', key: '', box: null, ws: null, status: 'off', batch: [], timer: 0, retry: 0,
  setStatus(s, message) { this.status = s; emit('ais-status', message ? { status: s, message } : { status: s }); },
  sub() { return JSON.stringify({ APIKey: this.key, BoundingBoxes: [this.box || [[-90, -180], [90, 180]]], FilterMessageTypes: ['PositionReport', 'ShipStaticData', 'StandardClassBPositionReport'] }); },
  start(box) { this.box = box; this.connect(); return { status: this.status }; },
  setBox(box) { this.box = box; if (this.ws && this.status === 'live') try { this.ws.send(this.sub()); } catch { /* reconnect resends */ } },
  stop(quiet) {
    clearInterval(this.timer); this.timer = 0; clearTimeout(this.retry); this.batch = [];
    if (this.ws) { const w = this.ws; this.ws = null; try { w.close(); } catch { /* gone */ } }
    if (!quiet) this.setStatus('off');
  },
  setKey(k) { this.key = k || ''; if (this.status !== 'off') this.connect(); },
  connect() {
    this.stop(true);
    if (!this.key) { this.setStatus('nokey'); return; }
    let w;
    try { w = new WebSocket(this.url); } catch (e) { this.setStatus('error', e.message); return; }
    this.ws = w; this.setStatus('connecting');
    let opened = false;
    w.onopen = () => { if (this.ws !== w) return; opened = true; try { w.send(this.sub()); this.setStatus('live'); } catch (e) { this.setStatus('error', e.message); } };
    w.onmessage = ev => { if (this.ws !== w) return; if (typeof ev.data === 'string') this.take(ev.data); else if (ev.data && ev.data.text) ev.data.text().then(t => { if (this.ws === w) this.take(t); }); };
    w.onerror = () => {             // Safari also sends close after this; some engines send only this
      if (this.ws !== w || opened) return;
      this.ws = null; this.setStatus('error', 'The ship service would not connect from this phone.');
    };
    w.onclose = () => {
      if (this.ws !== w) return;
      this.ws = null;
      if (this.status === 'off' || this.status === 'error') return;
      if (!opened) { this.setStatus('error', 'The ship service would not connect from this phone.'); return; }
      this.setStatus('reconnecting');
      this.retry = setTimeout(() => { if (this.status === 'reconnecting') this.connect(); }, 5000);
    };
    this.timer = setInterval(() => { if (this.batch.length) { const out = this.batch; this.batch = []; emit('ais', out); } }, 1000);
  },
  take(text) {
    let m; try { m = JSON.parse(text); } catch { return; }
    if (m.error) { this.setStatus('error', String(m.error)); return; }
    const meta = m.MetaData || {}, msg = m.Message || {}, pr = msg.PositionReport || msg.StandardClassBPositionReport, st = msg.ShipStaticData;
    const s = { mmsi: meta.MMSI, name: String(meta.ShipName || '').trim(), lat: meta.latitude, lon: meta.longitude };
    if (pr) { s.sog = pr.Sog; s.cog = pr.Cog; s.heading = pr.TrueHeading; }
    if (st) { s.type = st.Type; s.dest = String(st.Destination || '').trim(); s.callsign = String(st.CallSign || '').trim(); if (st.Dimension) s.len = (st.Dimension.A || 0) + (st.Dimension.B || 0); }
    s.t = Date.now();
    this.batch.push(s);
  },
};

// ------------------------------------------------------------------ saving files: the iPhone share sheet (Save to Files)
async function share(files, title) {
  const fl = files.map(f => new File([f.encoding === 'base64' ? Uint8Array.from(atob(f.data), c => c.charCodeAt(0)) : f.data], f.name, { type: f.name.endsWith('.png') ? 'image/png' : f.name.endsWith('.txt') ? 'text/plain' : 'application/octet-stream' }));
  if (navigator.canShare && navigator.canShare({ files: fl })) {
    try { await navigator.share({ files: fl, title }); return { ok: true }; }
    catch (e) { return e && e.name === 'AbortError' ? { canceled: true } : { error: e.message }; }
  }
  for (const f of fl) {     // other browsers: an ordinary download
    const a = document.createElement('a'); a.href = URL.createObjectURL(f); a.download = f.name;
    document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(a.href), 30000);
  }
  return { ok: true };
}

// ------------------------------------------------------------------ lightning (NOAA GOES), read here with h5wasm
let h5p = null, lightQueue = Promise.resolve();
async function lightningNow() {
  try {
    h5p = h5p || import('../h5wasm/hdf5_hl.js');                  // 4 MB: loaded only when Lightning is switched on
    const h5 = await h5p;
    return await lightning(h5, {
      list: async url => { const r = await tdFetch(url, {}); if (r.error) throw new Error(r.error); if (!r.ok) throw new Error('NOAA answered HTTP ' + r.status); return r.body; },
      bytes: async url => { try { const r = await rawFetch(url, { mode: 'cors', credentials: 'omit' }); return r.ok ? new Uint8Array(await r.arrayBuffer()) : null; } catch { return null; } },
    });
  } catch (e) { return { error: e.message }; }
}

window.td = {
  init: async () => ({ settings: forPage(settings), version: VERSION, platform: 'iphone' }),
  fetch: (url, init) => tdFetch(url, init || {}),
  opensky: async () => ({ error: 'OpenSky is not used in the phone app.' }),
  lightning: () => (lightQueue = lightQueue.then(lightningNow, lightningNow)),
  setSettings: async p => setSettings(p),
  aisStart: async box => {
    if (ais.ws && ais.key === settings.aisKey && ['connecting', 'live'].includes(ais.status)) { ais.box = box || null; ais.setBox(ais.box); return { status: ais.status }; }
    ais.key = settings.aisKey; return ais.start(box || null);
  },
  aisBox: async box => { ais.setBox(box); return true; },
  aisStop: async () => { ais.stop(false); return true; },
  saveFile: async o => { const r = await share([o], o.name); return r.ok ? { file: o.name } : r; },
  saveFolder: async o => { const r = await share(o.files, o.name); return r.ok ? { dir: o.name } : r; },
  reveal: async () => true,
  openPath: async () => '',
  openExternal: async u => { if (/^https?:\/\//.test(String(u))) window.open(u, '_blank', 'noopener'); return true; },
  on: (ch, fn) => { if (['ais', 'ais-status', 'link'].includes(ch)) (handlers[ch] = handlers[ch] || []).push(fn); },
};

// ------------------------------------------------------------------ data the desktop build bundled, made here once
const nativeFetch = window.fetch.bind(window);
const toast = (m, bad) => { try { window.terra.T.toast(m, bad); } catch { /* not ready */ } };
const BUILD = {
  'battles.json': { what: 'the battle list from Wikidata', url: BATTLES_URL, accept: 'application/sparql-results+json', make: battlesFromSparql, min: 1000 },
  'volcanoes.json': { what: 'the volcano list from the Smithsonian', url: VOLCANOES_URL, accept: 'application/json', make: volcanoesFromWfs, min: 500 },
};
const building = {};
async function build(name) {
  const b = BUILD[name];
  toast(`Downloading ${b.what} (first time only, this can take a minute)…`);
  let r;
  try {
    const ctl = new AbortController(), timer = setTimeout(() => ctl.abort(), 180000);
    try { r = await rawFetch(b.url, { headers: { accept: b.accept }, mode: 'cors', credentials: 'omit', signal: ctl.signal }); } finally { clearTimeout(timer); }
  } catch { throw new Error(`Could not download ${b.what}. It needs the internet the first time.`); }
  if (!r.ok) throw new Error(`Could not download ${b.what} (HTTP ${r.status}).`);
  const list = b.make(await r.json());
  if (list.length < b.min) throw new Error(`The download of ${b.what} looked incomplete (${list.length} rows). Try again later.`);
  const text = JSON.stringify(list);
  await cachePut('/terradesk-data/' + name, text, 'application/json');
  return text;
}
window.fetch = async (input, init) => {
  const url = typeof input === 'string' ? input : input && input.url;
  let path = '';
  try { const u = new URL(url, location.href); if (u.origin === location.origin) path = u.pathname; } catch { /* not a URL */ }
  const m = path.match(/\/data\/(battles\.json|volcanoes\.json)$/);
  if (!m) return nativeFetch(input, init);
  try { const r = await nativeFetch(input, init); if (r.ok) return r; } catch { /* offline and not cached: build or use the kept copy */ }
  const kept = await cacheGet('/terradesk-data/' + m[1]);
  if (kept) return kept;
  building[m[1]] = building[m[1]] || build(m[1]).finally(() => { delete building[m[1]]; });
  const text = await building[m[1]];
  return new Response(text, { status: 200, headers: { 'content-type': 'application/json' } });
};

// Close the top card, then the panel, then the menu (the Android back button's job; kept for parity).
window.tdBack = () => {
  const vis = id => { const e = document.getElementById(id); return e && !e.hidden; };
  if (vis('detail')) { document.getElementById('detailClose').click(); return true; }
  if (vis('panel')) { document.getElementById('panelClose').click(); return true; }
  if (document.body.classList.contains('menu')) { document.body.classList.remove('menu'); return true; }
  return false;
};

// Offline: the service worker keeps the app and its maps once they have been used.
if ('serviceWorker' in navigator && location.protocol === 'https:') navigator.serviceWorker.register('sw.js').catch(() => { /* fine without */ });
