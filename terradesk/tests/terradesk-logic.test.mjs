// Terra Desk for iPhone: checks that need no browser. Every file parses, every file the page and the offline
// helper ask for is here, the home-screen icons and manifest are right, nothing private is published, and the
// window.td bridge (data services, planes relay, ships, saving, lightning, first-use data) behaves, driven against
// fakes. Plain Node, no dependencies:   node terradesk/tests/terradesk-logic.test.mjs
import assert from 'assert';
import fs from 'fs';
import path from 'path';
import os from 'os';
import http from 'http';
import crypto from 'crypto';
import { spawnSync } from 'child_process';
import { fileURLToPath, pathToFileURL } from 'url';
import { createRequire } from 'module';

const APP = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const CESIUM = 'https://cdn.jsdelivr.net/npm/cesium@1.147.0/Build/Cesium/';
let n = 0; const t = async (name, fn) => { await fn(); n++; console.log('ok  ', name); };
const walk = d => fs.readdirSync(d, { withFileTypes: true }).flatMap(e => e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]);
const jsFiles = walk(path.join(APP, 'js')).filter(f => f.endsWith('.js'));
const read = f => fs.readFileSync(path.join(APP, f), 'utf8');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const pngSize = f => { const b = fs.readFileSync(path.join(APP, f)); assert.equal(b.toString('latin1', 1, 4), 'PNG', f); return [b.readUInt32BE(16), b.readUInt32BE(20)]; };

// ------------------------------------------------------------------ files
await t('every script parses (modules as modules), the offline helper too', () => {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'tdchk'));
  for (const f of [...jsFiles, path.join(APP, 'sw.js')]) {
    const src = fs.readFileSync(f, 'utf8');
    const copy = path.join(tmp, path.basename(f) + (/^\s*(import|export)\s/m.test(src) ? '.mjs' : '.cjs'));
    fs.writeFileSync(copy, src);
    const r = spawnSync(process.execPath, ['--check', copy], { encoding: 'utf8' });
    assert.equal(r.status, 0, f + '\n' + r.stderr);
  }
});

await t('every import between scripts points to a file that is here', () => {
  for (const f of [...jsFiles, path.join(APP, 'h5wasm/hdf5_hl.js')]) {
    for (const m of fs.readFileSync(f, 'utf8').matchAll(/(?:import|export)\s[^'"]*?from\s*['"]([^'"]+)['"]|import\(\s*['"]([^'"]+)['"]\s*\)|import\s+['"]([^'"]+)['"]/g)) {
      const spec = m[1] || m[2] || m[3];
      if (spec.startsWith('.')) assert.ok(fs.existsSync(path.resolve(path.dirname(f), spec)), `${path.relative(APP, f)} imports ${spec}`);
    }
  }
});

await t('the page loads its own files by relative paths and the globe library from one pinned version', () => {
  const html = read('index.html');
  const refs = [...html.matchAll(/(?:src|href)="([^"#]+)"/g)].map(m => m[1]);
  assert.ok(refs.length >= 8, refs.join());
  for (const r of refs) {
    if (/^https:/.test(r)) { assert.ok(r.startsWith(CESIUM), r); continue; }
    assert.ok(!r.startsWith('/'), 'relative, so it works under /Maz/terradesk/: ' + r);
    assert.ok(fs.existsSync(path.join(APP, r)), r);
  }
  assert.ok(html.includes(`window.CESIUM_BASE_URL = '${CESIUM}'`));
  assert.ok(html.indexOf('js/td-web.js') < html.indexOf('js/core.js'), 'the bridge loads before the app');
  for (const m of read('styles.css').matchAll(/url\(["']?([^"')]+)["']?\)/g)) if (!/^(data:|https?:)/.test(m[1])) assert.ok(fs.existsSync(path.join(APP, m[1])), 'css ' + m[1]);
});

await t('it can be added to the iPhone home screen and opens full screen', () => {
  const html = read('index.html');
  for (const s of ['name="apple-mobile-web-app-capable" content="yes"', 'name="apple-mobile-web-app-title" content="Terra Desk"',
    'name="apple-mobile-web-app-status-bar-style" content="black-translucent"', 'rel="apple-touch-icon" href="apple-touch-icon.png"',
    'rel="manifest" href="manifest.webmanifest"', 'viewport-fit=cover', 'name="theme-color"'])
    assert.ok(html.includes(s), s);
  assert.deepEqual(pngSize('apple-touch-icon.png'), [180, 180]);
  const m = JSON.parse(read('manifest.webmanifest'));
  assert.equal(m.display, 'standalone'); assert.equal(m.start_url, './'); assert.equal(m.scope, './');
  for (const i of m.icons) { const [w, h] = pngSize(i.src); assert.equal(`${w}x${h}`, i.sizes, i.src); }
  assert.ok(m.icons.some(i => i.sizes === '512x512') && m.icons.some(i => i.sizes === '192x192'));
});

await t('the iPhone layout keeps clear of the notch and the home bar', () => {
  const css = read('styles.css');
  for (const s of ['env(safe-area-inset-top)', 'env(safe-area-inset-bottom)', '@media (max-width: 820px)']) assert.ok(css.includes(s), s);
  assert.ok(/\.offline\s*\{/.test(css), 'a readable message if the globe library cannot load');
});

await t('every world map and the Earth maps are here, as JPEG pictures', () => {
  const worlds = [...read('js/layers/worlds.js').matchAll(/\['\w+', '[^']+', '(\w+)'/g)].map(m => m[1]);
  assert.equal(worlds.length, 9, worlds.join());
  for (const f of [...worlds.map(w => `assets/worlds/${w}.jpg`), 'assets/base/day.jpg', 'assets/base/night.jpg']) {
    const b = fs.readFileSync(path.join(APP, f)); assert.ok(b[0] === 0xff && b[1] === 0xd8 && b.length > 50e3, f);
  }
});

await t('the battle and volcano lists are bundled and read the way the layers expect', () => {
  const b = JSON.parse(read('data/battles.json')), v = JSON.parse(read('data/volcanoes.json'));
  assert.ok(b.length > 5000 && v.length > 1000, `${b.length} battles, ${v.length} volcanoes`);
  for (const x of b.slice(0, 50)) assert.ok(x.id && x.name && isFinite(x.lat) && isFinite(x.lon) && Number.isInteger(x.year) && Array.isArray(x.war));
  for (const x of v.slice(0, 50)) assert.ok(x.n && x.name && isFinite(x.lat) && isFinite(x.lon));
  assert.ok(b.some(x => /Waterloo/.test(x.name)) && v.some(x => /Helens/.test(x.name)));
  const r = JSON.parse(read('data/aircraft-registry.json')); assert.ok(r.length > 400 && r.some(x => /F-35/.test(x.system)));
});

await t('nothing private is published: no keys, tokens, passwords or e-mail addresses', () => {
  for (const f of walk(APP).filter(f => /\.(js|html|css|json|webmanifest|md)$/.test(f) && !f.includes(path.join('data', '')) && !f.includes('h5wasm') && !f.includes(path.join('tests', '')))) {
    const s = fs.readFileSync(f, 'utf8');
    assert.ok(!/[A-Za-z0-9._%+-]+@(gmail|yahoo|outlook|hotmail|icloud)\.com/i.test(s), 'e-mail in ' + f);
    assert.ok(!/(api[_-]?key|apikey|token|secret|password)\s*[:=]\s*['"][A-Za-z0-9_\-]{12,}['"]/i.test(s), 'secret in ' + f);
    assert.ok(!/gh[pousr]_[A-Za-z0-9]{30,}|AKIA[0-9A-Z]{16}/.test(s), 'credential in ' + f);
  }
  assert.match(read('js/td-web.js'), /aisKey: ''/);
});

// ------------------------------------------------------------------ data made on the phone (if a bundled list is ever missing)
const DB = await import(pathToFileURL(path.join(APP, 'js/databuild.js')));
const sparql = rows => ({ results: { bindings: rows } });
const row = (id, name, lon, lat, date, war, article) => Object.fromEntries(Object.entries({
  b: { value: 'http://www.wikidata.org/entity/' + id }, bLabel: name && { value: name }, coord: { value: `Point(${lon} ${lat})` },
  date: date && { value: date }, warLabel: war && { value: war }, article: article && { value: article } }).filter(([, v]) => v));

await t('battles are made from Wikidata the way the desktop build made them', () => {
  const b = DB.battlesFromSparql(sparql([
    row('Q48314', 'Battle of Waterloo', 4.4122, 50.68, '+1815-06-18T00:00:00Z', 'Hundred Days', 'https://en.wikipedia.org/wiki/Battle_of_Waterloo'),
    row('Q48314', 'Battle of Waterloo', 4.4122, 50.68, '+1815-06-18T00:00:00Z', 'Napoleonic Wars'),
    row('Q1', 'Battle of Marathon', 23.9783, 38.1186, '-0490-09-12T00:00:00Z'),
    row('Q2', 'Q2', 0, 0, '+1900-01-01T00:00:00Z'), row('Q4', 'Battle off the map', 300, 0, '+1900-01-01T00:00:00Z'),
  ]));
  assert.deepEqual(b.map(x => x.name), ['Battle of Marathon', 'Battle of Waterloo']);
  assert.deepEqual(b[1].war, ['Hundred Days', 'Napoleonic Wars']); assert.equal(b[0].date, '-0490-09-12');
});

await t('volcanoes are made from the Smithsonian list the way the desktop build made them', () => {
  const v = DB.volcanoesFromWfs({ features: [
    { geometry: { coordinates: [-122.18, 46.2] }, properties: { Volcano_Number: 321050, Volcano_Name: 'St. Helens', Country: 'United States', Primary_Volcano_Type: 'Stratovolcano', Last_Eruption_Year: 2008, Elevation: 2549 } },
    { geometry: null, properties: { Volcano_Name: 'no place' } }] });
  assert.deepEqual(v, [{ n: 321050, name: 'St. Helens', country: 'United States', type: 'Stratovolcano', last: 2008, elev: 2549, lat: 46.2, lon: -122.18 }]);
});

// ------------------------------------------------------------------ a fake Safari around window.td
const store = new Map(), cacheStore = new Map();
globalThis.window = globalThis;
globalThis.location = new URL('https://mazoo85.github.io/Maz/terradesk/index.html');
globalThis.localStorage = { getItem: k => store.has(k) ? store.get(k) : null, setItem: (k, v) => store.set(k, String(v)), removeItem: k => store.delete(k) };
globalThis.caches = { open: async name => { if (!cacheStore.has(name)) cacheStore.set(name, new Map()); const m = cacheStore.get(name);
  return { match: async k => { const r = m.get(String(k.url || k)); return r ? r.clone() : undefined; }, put: async (k, r) => { m.set(String(k.url || k), r); } }; } };
const elems = {}, clicked = [];
const el = id => (elems[id] = elems[id] || { hidden: true, click() { el(id.replace('Close', '')).hidden = true; } });
globalThis.document = { getElementById: el, body: { appendChild() {}, classList: { s: new Set(), contains(c) { return this.s.has(c); }, remove(c) { this.s.delete(c); }, add(c) { this.s.add(c); } } },
  createElement: () => ({ click() { clicked.push(this.download); }, remove() {} }) };
const nav = { onLine: true };
Object.defineProperty(globalThis, 'navigator', { value: nav, configurable: true, writable: true });
let opened = []; globalThis.open = (u, w) => { opened.push([u, w]); };
const net = []; let answer = () => new Response('', { status: 404 });
globalThis.fetch = async (u, init) => { const url = String(u.url || u); net.push({ url, init }); return answer(url, init); };

const TD = await import(pathToFileURL(path.join(APP, 'js/td-web.js')));
const td = window.td;

await t('window.td has every call the page makes', () => {
  const used = new Set(jsFiles.flatMap(f => [...fs.readFileSync(f, 'utf8').matchAll(/\btd\.([a-zA-Z]+)\(/g)].map(m => m[1])));
  assert.ok(used.size >= 8);
  for (const k of used) assert.equal(typeof td[k], 'function', 'td.' + k);
});

await t('it starts with the default settings and says it is the iPhone build', async () => {
  const d = await td.init();
  assert.equal(d.platform, 'iphone'); assert.equal(d.version, TD.VERSION);
  assert.equal(d.settings.imagery, 'esri'); assert.equal(d.settings.hasAis, false);
});

await t('settings are kept on the phone, keys are shown masked and a masked key is never saved over the real one', async () => {
  let r = await td.setSettings({ aisKey: 'abcdef123456WXYZ', units: 'imperial' });
  assert.deepEqual(r, { ok: true, hasAis: true, hasOpensky: false });
  const page = (await td.init()).settings;
  assert.equal(page.aisKey, '••••WXYZ'); assert.equal(page.units, 'imperial');
  await td.setSettings({ aisKey: '••••WXYZ' });
  assert.equal(JSON.parse(store.get('terradesk.settings')).aisKey, 'abcdef123456WXYZ');
  assert.equal(TD.loadSettings().units, 'imperial');
  await td.setSettings({ aisKey: '' }); assert.equal((await td.init()).settings.hasAis, false);
});

await t('only Terra Desk’s own data services are asked, and only over https', async () => {
  net.length = 0;
  assert.match((await td.fetch('https://example.com/x')).error, /not one of Terra Desk's data services/);
  assert.match((await td.fetch('http://services.swpc.noaa.gov/x')).error, /not one of/);
  assert.equal(net.length, 0);
});

await t('answers come back as text with status, and pictures as base64', async () => {
  answer = url => url.endsWith('.png') ? new Response(new Uint8Array([1, 2, 3]), { headers: { 'content-type': 'image/png' } }) : new Response('{"a":1}', { headers: { 'content-type': 'application/json' } });
  const r = await td.fetch('https://services.swpc.noaa.gov/json/a.json');
  assert.equal(r.ok, true); assert.equal(r.status, 200); assert.equal(r.body, '{"a":1}'); assert.match(r.type, /json/);
  const p = await td.fetch('https://allmaps.xyz/a.png', { binary: true });
  assert.equal(p.body, Buffer.from([1, 2, 3]).toString('base64'));
  const req = net.at(-1); assert.equal(req.init.mode, 'cors'); assert.equal(req.init.credentials, 'omit');
});

await t('each service is asked politely: one at a time, with its gap, and a fresh answer is reused', async () => {
  net.length = 0; answer = () => new Response('ok');
  const t0 = Date.now();
  await Promise.all([td.fetch('https://services.swpc.noaa.gov/g1'), td.fetch('https://services.swpc.noaa.gov/g2'), td.fetch('https://services.swpc.noaa.gov/g3')]);
  assert.ok(Date.now() - t0 >= 950, 'three requests 500 ms apart took ' + (Date.now() - t0));
  const before = net.length;
  const again = await td.fetch('https://services.swpc.noaa.gov/g1');
  assert.equal(again.cached, true); assert.equal(net.length, before, 'served from the kept answer');
  const [a, b] = await Promise.all([td.fetch('https://celestrak.org/same'), td.fetch('https://celestrak.org/same')]);
  assert.equal(net.filter(x => x.url === 'https://celestrak.org/same').length, 1, 'two asks at once, one request'); assert.equal(a.body, b.body);
});

await t('with no signal it uses the last answer, or says plainly that it could not connect', async () => {
  answer = () => new Response('fresh');
  await td.fetch('https://nominatim.openstreetmap.org/search?q=Paris');
  answer = () => { throw new TypeError('Load failed'); };
  const r = await td.fetch('https://nominatim.openstreetmap.org/search?q=Paris', { fresh: true });
  assert.equal(r.body, 'fresh'); assert.equal(r.stale, true);
  const e = await td.fetch('https://nominatim.openstreetmap.org/search?q=Nowhere');
  assert.match(e.error, /could not be reached from this phone/);
  nav.onLine = false; const off = await td.fetch('https://nominatim.openstreetmap.org/search?q=Elsewhere'); nav.onLine = true;
  assert.equal(off.error, 'No internet connection.');
  answer = () => new Response('', { status: 503 });
  assert.equal((await td.fetch('https://nominatim.openstreetmap.org/search?q=Paris', { fresh: true })).body, 'fresh', 'a server error keeps the last answer too');
});

await t('long-lived answers survive closing the app; live ones do not', async () => {
  answer = () => new Response('kept');
  await td.fetch('https://hiking.waymarkedtrails.org/api/x');
  await td.fetch('https://api.allmaps.org/maps?x=1');
  const kept = JSON.parse(store.get('terradesk.cache'));
  assert.ok(Object.keys(kept).some(k => k.includes('waymarkedtrails')));
  answer = () => new Response('live'); await td.fetch('https://noaa-goes19.s3.amazonaws.com/?list');
  assert.ok(!Object.keys(JSON.parse(store.get('terradesk.cache'))).some(k => k.includes('noaa-goes19')));
});

await t('a service that refuses web pages is not asked at all', async () => {
  net.length = 0;
  assert.match((await td.fetch('https://volcano.si.edu/news/WeeklyVolcanoRSS.xml')).error, /does not answer web pages/);
  assert.equal(net.length, 0);
});

await t('a busy Overpass server hands over to its mirrors', async () => {
  net.length = 0;
  answer = url => url.includes('overpass-api.de') ? new Response('busy', { status: 429 }) : url.includes('kumi') ? new Response('', { status: 504 }) : new Response('{"elements":[]}');
  const r = await td.fetch('https://overpass-api.de/api/interpreter', { method: 'POST', body: 'data=x', headers: { 'content-type': 'application/x-www-form-urlencoded' } });
  assert.equal(r.body, '{"elements":[]}');
  assert.deepEqual(net.map(x => new URL(x.url).hostname), ['overpass-api.de', 'overpass.kumi.systems', 'overpass.private.coffee']);
  assert.equal(net[2].init.method, 'POST'); assert.equal(net[2].init.body, 'data=x');
});

await t('planes come through the GOD’S EYE VIEW relay in the shape the aircraft layer reads', async () => {
  assert.equal(TD.relayUrl('https://api.adsb.lol/v2/point/51.470/-0.450/120'), `https://${TD.RELAY_HOST}/planes?lat=51.470&lng=-0.450`);
  assert.equal(TD.relayUrl('https://api.adsb.lol/v2/mil'), null);
  net.length = 0;
  answer = url => new Response(JSON.stringify({ source: 'adsb.lol', ac: [{ hex: 'abc', lat: 51.4, lon: -0.4, alt_baro: 3000 }] }));
  const r = await td.fetch('https://api.adsb.lol/v2/point/51.470/-0.450/120');
  assert.equal(new URL(net[0].url).hostname, TD.RELAY_HOST); assert.equal(net[0].init.headers['x-api-key'], undefined);
  assert.deepEqual(JSON.parse(r.body).ac.map(a => a.hex), ['abc']);
  answer = () => new Response(JSON.stringify({ error: 'no flight source answered' }), { status: 502 });
  assert.match((await td.fetch('https://api.adsb.lol/v2/point/10.000/10.000/50')).error, /HTTP 502/);
  assert.match((await td.fetch('https://api.adsb.lol/v2/mil')).error, /not available on iPhone yet/);
});

// ------------------------------------------------------------------ ships: a small WebSocket server standing in for aisstream.io
function wsServer(onMessage) {
  const socks = new Set();
  const srv = http.createServer();
  srv.on('upgrade', (req, sock) => {
    const accept = crypto.createHash('sha1').update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
    sock.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
    socks.add(sock); sock.on('close', () => socks.delete(sock)); sock.on('error', () => {});
    const send = text => { const p = Buffer.from(text); const h = p.length < 126 ? Buffer.from([0x81, p.length]) : Buffer.from([0x81, 126, p.length >> 8, p.length & 255]); sock.write(Buffer.concat([h, p])); };
    let buf = Buffer.alloc(0);
    sock.on('data', d => {
      buf = Buffer.concat([buf, d]);
      while (buf.length >= 2) {
        let len = buf[1] & 127, off = 2; if (len === 126) { len = buf.readUInt16BE(2); off = 4; }
        if (buf.length < off + 4 + len) return;
        const mask = buf.subarray(off, off + 4), body = Buffer.from(buf.subarray(off + 4, off + 4 + len)).map((b, i) => b ^ mask[i % 4]);
        const op = buf[0] & 15; buf = buf.subarray(off + 4 + len);
        if (op === 8) { sock.end(); return; }
        if (op === 1) onMessage(body.toString(), send, sock);
      }
    });
  });
  return new Promise(r => srv.listen(0, '127.0.0.1', () => r({ url: `ws://127.0.0.1:${srv.address().port}`, srv, socks })));
}

await t('Ship Watch subscribes with your key and area, and ships arrive in batches', async () => {
  const subs = [];
  const s = await wsServer((text, send) => {
    subs.push(JSON.parse(text));
    send(JSON.stringify({ MessageType: 'PositionReport', MetaData: { MMSI: 123, ShipName: 'EVER GIVEN  ', latitude: 30, longitude: 32 }, Message: { PositionReport: { Sog: 10, Cog: 90, TrueHeading: 91 } } }));
    send(JSON.stringify({ MessageType: 'ShipStaticData', MetaData: { MMSI: 123, ShipName: 'EVER GIVEN', latitude: 30, longitude: 32 }, Message: { ShipStaticData: { Type: 70, Destination: 'ROTTERDAM ', CallSign: 'H3RC', Dimension: { A: 200, B: 200 } } } }));
  });
  TD.ais.url = s.url;
  const statuses = [], ships = [];
  td.on('ais-status', x => statuses.push(x.status)); td.on('ais', b => ships.push(...b));
  assert.equal((await td.aisStart([[29, 31], [31, 33]])).status, 'nokey');
  await td.setSettings({ aisKey: 'k-1234567890' });
  await td.aisStart([[29, 31], [31, 33]]);
  for (let i = 0; i < 40 && ships.length < 2; i++) await sleep(100);
  assert.equal(subs[0].APIKey, 'k-1234567890'); assert.deepEqual(subs[0].BoundingBoxes, [[[29, 31], [31, 33]]]);
  assert.deepEqual(statuses.slice(0, 3), ['nokey', 'connecting', 'live']);
  assert.equal(ships[0].name, 'EVER GIVEN'); assert.equal(ships[0].heading, 91); assert.equal(ships[1].dest, 'ROTTERDAM'); assert.equal(ships[1].len, 400);
  await td.aisBox([[0, 0], [1, 1]]); for (let i = 0; i < 20 && subs.length < 2; i++) await sleep(50);
  assert.deepEqual(subs[1].BoundingBoxes, [[[0, 0], [1, 1]]], 'a new area is sent on the open connection');
  for (const so of s.socks) so.destroy();                                 // the connection drops…
  for (let i = 0; i < 80 && !statuses.slice(3).includes('live'); i++) await sleep(100);
  assert.ok(statuses.includes('reconnecting') && statuses.lastIndexOf('live') > 2, statuses.join());   // …and comes back by itself
  await td.aisStop(); assert.equal(statuses.at(-1), 'off');
  s.srv.close();
});

await t('a refused key or an unreachable ship service says so instead of spinning', async () => {
  const s = await wsServer((text, send) => send(JSON.stringify({ error: 'Api Key Is Not Valid' })));
  TD.ais.url = s.url;
  const got = []; td.on('ais-status', x => got.push(x));
  await td.aisStart(null);
  for (let i = 0; i < 30 && !got.some(x => x.status === 'error'); i++) await sleep(100);
  assert.equal(got.find(x => x.status === 'error').message, 'Api Key Is Not Valid');
  await td.aisStop(); s.srv.close();
  TD.ais.url = 'ws://127.0.0.1:9'; got.length = 0;
  await td.aisStart(null);
  for (let i = 0; i < 50 && !got.some(x => x.status === 'error'); i++) await sleep(100);
  assert.match(got.find(x => x.status === 'error').message, /would not connect/);
  await td.aisStop(); await td.setSettings({ aisKey: '' });
});

// ------------------------------------------------------------------ saving, links, back
await t('saving opens the iPhone share sheet (Save to Files) with the right files', async () => {
  const shared = [];
  nav.canShare = ({ files }) => files.every(f => f instanceof File); nav.share = async o => { shared.push(o); };
  assert.deepEqual(await td.saveFile({ name: 'trip.gpx', data: '<gpx/>' }), { file: 'trip.gpx' });
  assert.equal(shared[0].files[0].name, 'trip.gpx'); assert.equal(await shared[0].files[0].text(), '<gpx/>');
  const r = await td.saveFolder({ name: 'Model', files: [{ name: 'height.png', data: Buffer.from([137, 80]).toString('base64'), encoding: 'base64' }, { name: 'readme.txt', data: 'hi' }] });
  assert.deepEqual(r, { dir: 'Model' });
  assert.deepEqual([...new Uint8Array(await shared[1].files[0].arrayBuffer())], [137, 80]); assert.equal(shared[1].files[0].type, 'image/png');
  nav.share = async () => { const e = new Error('cancel'); e.name = 'AbortError'; throw e; };
  assert.deepEqual(await td.saveFile({ name: 'a.txt', data: 'x' }), { canceled: true });
  delete nav.canShare; delete nav.share;
  assert.deepEqual(await td.saveFile({ name: 'b.txt', data: 'x' }), { file: 'b.txt' }); assert.equal(clicked.at(-1), 'b.txt', 'other browsers download it');
});

await t('outside links open in Safari; anything that is not a web address is ignored', async () => {
  opened = [];
  await td.openExternal('https://www.wikidata.org/wiki/Q48314'); await td.openExternal('javascript:alert(1)');
  assert.deepEqual(opened, [['https://www.wikidata.org/wiki/Q48314', '_blank']]);
});

await t('close the card, then the panel, then the menu', () => {
  el('detail').hidden = false; el('panel').hidden = false; document.body.classList.add('menu');
  assert.equal(window.tdBack(), true); assert.equal(el('detail').hidden, true);
  assert.equal(window.tdBack(), true); assert.equal(el('panel').hidden, true);
  assert.equal(window.tdBack(), true); assert.equal(window.tdBack(), false);
});

// ------------------------------------------------------------------ first-use data, if a bundled list is missing
await t('a bundled list is served as it is', async () => {
  net.length = 0; answer = url => url.endsWith('data/battles.json') ? new Response('[{"id":"Q1"}]') : new Response('', { status: 404 });
  assert.deepEqual(await (await fetch('data/battles.json')).json(), [{ id: 'Q1' }]);
  assert.equal(net.length, 1);
});

await t('a missing list is downloaded once, built, kept and served from the phone afterwards', async () => {
  const rows = []; for (let i = 0; i < 1200; i++) rows.push(row('Q' + (10 + i), 'Battle ' + i, 1, 2, '+1700-01-01T00:00:00Z'));
  net.length = 0;
  answer = url => url.startsWith('https://query.wikidata.org/') ? new Response(JSON.stringify(sparql(rows))) : new Response('', { status: 404 });
  const [r1, r2] = await Promise.all([fetch('data/battles.json'), fetch(new URL('data/battles.json', location.href).href)]);
  assert.equal((await r1.json()).length, 1200); assert.equal((await r2.json()).length, 1200);
  const wd = net.filter(x => x.url.startsWith('https://query.wikidata.org/'));
  assert.equal(wd.length, 1, 'one download for two asks'); assert.equal(wd[0].url, DB.BATTLES_URL);
  answer = () => { throw new TypeError('offline'); };
  assert.equal((await (await fetch('data/battles.json')).json()).length, 1200, 'kept on the phone');
  assert.equal(await (await fetch('https://example.org/a').catch(() => new Response('outside'))).text(), 'outside');
});

await t('a failed or short download says so instead of drawing nothing', async () => {
  answer = () => { throw new TypeError('offline'); };
  await assert.rejects(fetch('data/volcanoes.json'), /Could not download the volcano list.*internet/);
  answer = url => url.includes('volcano.si.edu') ? new Response('{"features":[]}') : new Response('', { status: 404 });
  await assert.rejects(fetch('data/volcanoes.json'), /looked incomplete/);
});

await t('lightning reads NOAA’s file list and says plainly when NOAA cannot be reached', async () => {
  answer = () => { throw new TypeError('offline'); };
  assert.match((await td.lightning()).error, /could not be reached/);
  answer = () => new Response('<ListBucketResult></ListBucketResult>');
  assert.deepEqual((await td.lightning()).flashes, []);
  assert.ok(net.some(x => x.url.startsWith('https://noaa-goes19.s3.amazonaws.com/?list-type=2&prefix=GLM-L2-LCFA/')));
});

// ------------------------------------------------------------------ the offline helper (service worker)
await t('the offline helper keeps the app, the globe library and maps, never live data', async () => {
  const src = read('sw.js');
  const core = JSON.parse(src.match(/const CORE = (\[[\s\S]*?\]);/)[1].replace(/'/g, '"'));
  for (const f of core) if (f !== './') assert.ok(fs.existsSync(path.join(APP, f)), 'CORE ' + f);
  // Run it in a fake worker.
  const listeners = {}, sc = new Map();
  const cachesFake = { open: async name => { if (!sc.has(name)) sc.set(name, new Map()); const m = sc.get(name);
    return { match: async (k, o) => { let key = String(k.url || k); if (o && o.ignoreSearch) key = key.split('?')[0]; const r = m.get(key); return r ? r.clone() : undefined; },
      put: async (k, r) => { m.set(String(k.url || k), r); }, addAll: async l => { for (const f of l) m.set(new URL(f, 'https://mazoo85.github.io/Maz/terradesk/sw.js').href, new Response('core ' + f)); },
      keys: async () => [...m.keys()].map(url => ({ url })), delete: async k => m.delete(String(k.url || k)) }; },
    keys: async () => [...sc.keys()], delete: async k => sc.delete(k) };
  let online = true; const asked = [];
  const ctx = { self: { addEventListener: (e, f) => { listeners[e] = f; }, skipWaiting() {}, clients: { claim() {} } }, caches: cachesFake,
    location: new URL('https://mazoo85.github.io/Maz/terradesk/sw.js'), URL, Response, Promise, setTimeout, console,
    fetch: async r => { asked.push(String(r.url || r)); if (!online) throw new TypeError('offline'); return new Response('net ' + (r.url || r)); } };
  new Function(...Object.keys(ctx), src)(...Object.values(ctx));
  const wait = []; listeners.install({ waitUntil: p => wait.push(p) }); await Promise.all(wait);
  assert.ok((await (await cachesFake.open('terradesk-1.1.0-iphone.1-shell')).keys()).length >= core.length);
  const go = async (url, mode = 'cors', method = 'GET') => { let res = null; listeners.fetch({ request: { url, mode, method }, respondWith: p => { res = p; } }); return res ? (await res).text() : null; };
  assert.equal(await go('https://mazoo85.github.io/Maz/terradesk/js/core.js'), 'net https://mazoo85.github.io/Maz/terradesk/js/core.js');
  online = false;
  assert.equal(await go('https://mazoo85.github.io/Maz/terradesk/js/core.js'), 'net https://mazoo85.github.io/Maz/terradesk/js/core.js', 'the kept copy offline');
  assert.equal(await go('https://mazoo85.github.io/Maz/terradesk/?x=1', 'navigate'), 'core ./', 'the app opens offline');
  online = true;
  const tile = 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/3/2/1';
  await go(tile); online = false; assert.equal(await go(tile), 'net ' + tile, 'a map tile you have seen shows offline'); online = true;
  assert.equal(await go(CESIUM + 'Cesium.js'), 'net ' + CESIUM + 'Cesium.js');
  for (const live of ['https://godview-planes.cody-collins85-cc.workers.dev/planes?lat=1&lng=2', 'https://services.swpc.noaa.gov/json/x.json', 'https://mazoo85.github.io/Maz/earth/index.html'])
    assert.equal(await go(live), null, 'left alone: ' + live);
  assert.equal(await go('https://mazoo85.github.io/Maz/terradesk/x', 'cors', 'POST'), null);
});

// ------------------------------------------------------------------ the desktop's own checks that still apply
await t('earth maths (shared with Earth Explorer)', () => {
  const G = createRequire(import.meta.url)(path.join(APP, 'js/geo.js'));
  assert.ok(Math.abs(G.distance([-0.1276, 51.5072], [2.3522, 48.8566]) - 343500) < 2000);
  assert.deepEqual(G.parseCoordinates('48.8584, 2.2945'), { lat: 48.8584, lng: 2.2945 });
});

console.log(`\n${n} tests passed`);
process.exit(0);
