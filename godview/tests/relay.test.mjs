/*
 * GOD'S EYE VIEW - the planes relay, tested without a network.
 *
 *   node godview/tests/relay.test.mjs
 *
 * The relay (godview/relay/worker.js) runs as a Cloudflare Worker, which is
 * the standard fetch/Request/Response API that Node also has. The flight
 * services are replaced by a fake fetch, so every path is checked: who may use
 * the relay, bad input, each source in turn, the remembered last answer, both
 * failing, and the /check status page.
 */
import worker from '../relay/worker.js';

const SITE = 'https://mazoo85.github.io';
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) + ', expected ' + JSON.stringify(expected));
  }
}
function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function serviceOf(url) {
  if (url.includes('adsb.lol')) return 'adsb.lol';
  if (url.includes('opensky-network.org')) return 'opensky';
  if (url.includes('adsb.fi')) return 'adsb.fi';
  return 'other';
}

/* answers: { 'adsb.lol': {status, body} | 'throw', 'opensky': ..., 'adsb.fi': ... } */
function fakeNetwork(answers) {
  const asked = [];
  globalThis.fetch = async (url, init) => {
    asked.push({ url: String(url), ua: init && init.headers && init.headers['User-Agent'] });
    const a = answers[serviceOf(String(url))];
    if (!a || a === 'throw') throw new Error('network down');
    return new Response(typeof a.body === 'string' ? a.body : JSON.stringify(a.body), { status: a.status || 200 });
  };
  return asked;
}

const ask = (path, { origin = SITE, method = 'GET' } = {}) =>
  worker.fetch(new Request('https://relay.example.workers.dev' + path, {
    method: method,
    headers: origin ? { Origin: origin } : {}
  }));

const PLANE = { hex: 'abc123', flight: 'UAL1 ', lat: 40.6, lon: -73.8, alt_baro: 30000, gs: 450, track: 90, seen_pos: 1 };
const OPENSKY_NOW = 1792563000;
const OPENSKY = {
  time: OPENSKY_NOW,
  states: [
    ['3c6444', 'DLH9LF  ', 'Germany', OPENSKY_NOW - 2, OPENSKY_NOW, -73.5, 40.5, 10668.0, false, 231.5, 45.0, 0, null, 10900, '1000', false, 0],
    ['4b1805', 'SWR12   ', 'Switzerland', OPENSKY_NOW - 5, OPENSKY_NOW, -73.9, 40.6, null, true, 5.1, 120.0, null, null, null, null, false, 0],
    ['nopos1', 'NOPOS   ', 'Nowhere', null, OPENSKY_NOW, null, null, 9000, false, 200, 10, 0, null, null, null, false, 0]
  ]
};

/* Each test uses its own place, so the remembered answers of one test never
 * leak into another. */
let place = 0;
const nextPlace = () => { place += 1; return '/planes?lat=' + (place) + '.0&lng=' + (place) + '.0'; };

test('it asks adsb.lol first and passes the aircraft on, for our site only', async () => {
  const asked = fakeNetwork({ 'adsb.lol': { body: { ac: [PLANE] } } });
  const res = await ask('/planes?lat=40.64&lng=-73.78');
  eq(res.status, 200);
  eq(res.headers.get('Access-Control-Allow-Origin'), SITE);
  const body = await res.json();
  eq(body.source, 'adsb.lol');
  eq(body.ac.length, 1);
  eq(body.ac[0].hex, 'abc123');
  eq(asked.length, 1, 'only one service asked');
  eq(asked[0].url, 'https://api.adsb.lol/v2/point/40.6/-73.8/250', 'rounded to ~10 km');
  assert(/GodsEyeView-relay/.test(asked[0].ua), 'it says who is asking');
});

test('OpenSky fills in when adsb.lol refuses, in the same shape as adsb.lol', async () => {
  fakeNetwork({ 'adsb.lol': { status: 429, body: {} }, opensky: { body: OPENSKY } });
  const body = await (await ask(nextPlace())).json();
  eq(body.source, 'OpenSky');
  eq(body.ac.length, 2, 'the row with no position is dropped');
  const dlh = body.ac[0];
  eq(dlh.hex, '3c6444');
  eq(dlh.flight, 'DLH9LF  ');
  eq(dlh.lat, 40.5);
  eq(dlh.lon, -73.5);
  eq(dlh.alt_baro, 35000, '10668 m in feet');
  eq(dlh.gs, 450, '231.5 m/s in knots');
  eq(dlh.track, 45);
  eq(dlh.seen_pos, 2, 'seconds since its last position');
  eq(body.ac[1].alt_baro, 'ground', 'on the ground reads as adsb.lol does');
});

test('adsb.fi is the last resort, and a page that is not JSON counts as a failure', async () => {
  fakeNetwork({ 'adsb.lol': { body: '<html>oops</html>' }, opensky: 'throw', 'adsb.fi': { body: { now: 1, aircraft: [PLANE] } } });
  const body = await (await ask(nextPlace())).json();
  eq(body.source, 'adsb.fi');
  eq(body.ac.length, 1);
});

test('when every source refuses, the last good answer for that area is used', async () => {
  const where = nextPlace();
  fakeNetwork({ 'adsb.lol': { body: { ac: [PLANE] } } });
  eq((await (await ask(where)).json()).source, 'adsb.lol');
  fakeNetwork({ 'adsb.lol': { status: 429, body: {} }, opensky: { status: 429, body: {} }, 'adsb.fi': { status: 403, body: {} } });
  const res = await ask(where);
  eq(res.status, 200);
  const body = await res.json();
  eq(body.source, 'adsb.lol');
  eq(body.stale, true, 'marked as a remembered answer');
  eq(body.ac.length, 1);
});

test('with nothing remembered, every source refusing is a clear 502 our page can still read', async () => {
  fakeNetwork({ 'adsb.lol': { status: 429, body: {} }, opensky: 'throw', 'adsb.fi': { status: 403, body: {} } });
  const res = await ask(nextPlace());
  eq(res.status, 502);
  eq(res.headers.get('Access-Control-Allow-Origin'), SITE);
  const body = await res.json();
  eq(body.details.length, 3);
});

test('/check reports what each source said, for anyone, without the planes', async () => {
  fakeNetwork({ 'adsb.lol': { status: 429, body: {} }, opensky: { body: OPENSKY }, 'adsb.fi': { status: 403, body: {} } });
  const res = await ask('/check', { origin: null });
  eq(res.status, 200);
  const body = await res.json();
  eq(body.sources.map((x) => x.source + ':' + (x.ok ? x.aircraft : x.error)).join(' | '),
    'adsb.lol:adsb.lol answered 429 | OpenSky:2 | adsb.fi:adsb.fi answered 403');
  assert(!('ac' in body), 'no plane data on the status page');
});

test('other sites cannot use it, and nothing is fetched for them', async () => {
  const asked = fakeNetwork({ 'adsb.lol': { body: { ac: [] } } });
  const res = await ask('/planes?lat=40&lng=-73', { origin: 'https://someone-else.example' });
  eq(res.status, 403);
  eq(res.headers.get('Access-Control-Allow-Origin'), null);
  eq(asked.length, 0);
  eq((await ask('/planes?lat=40&lng=-73', { origin: null })).status, 403, 'no origin at all');
});

test('bad coordinates, wrong paths and wrong methods are refused', async () => {
  const asked = fakeNetwork({ 'adsb.lol': { body: { ac: [] } } });
  eq((await ask('/planes?lat=95&lng=0')).status, 400, 'latitude past the pole');
  eq((await ask('/planes?lat=10&lng=-200')).status, 400, 'longitude off the map');
  eq((await ask('/planes?lat=abc&lng=1')).status, 400, 'not a number');
  eq((await ask('/planes?lng=1')).status, 400, 'missing latitude');
  eq((await ask('/other')).status, 404);
  eq((await ask('/planes?lat=1&lng=1', { method: 'POST' })).status, 405);
  eq(asked.length, 0, 'none of those reached a flight service');
});

test('a browser preflight from our site is allowed', async () => {
  const res = await ask('/planes?lat=1&lng=1', { method: 'OPTIONS' });
  eq(res.status, 204);
  eq(res.headers.get('Access-Control-Allow-Origin'), SITE);
  eq((await ask('/planes', { method: 'OPTIONS', origin: 'https://evil.example' })).status, 403);
});

let failed = 0;
console.log("\nGOD'S EYE VIEW - planes relay");
for (const [name, fn] of tests) {
  try {
    await fn();
    console.log('  ok   ' + name);
  } catch (e) {
    failed++;
    console.log('  FAIL ' + name + '\n       ' + e.message);
  }
}
console.log('\n' + (tests.length - failed) + '/' + tests.length + ' passed');
process.exit(failed ? 1 : 0);
