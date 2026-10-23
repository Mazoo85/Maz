/*
 * GOD'S EYE VIEW — the planes relay, tested without a network.
 *
 *   node godview/tests/relay.test.mjs
 *
 * The relay (godview/relay/worker.js) runs as a Cloudflare Worker, which is
 * the standard fetch/Request/Response API that Node also has. The flight
 * services are replaced by a fake fetch, so every path is checked: who may use
 * the relay, bad input, the primary source, the fallback, and both failing.
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

/* answers: { 'adsb.fi': {status, body} | 'throw', 'adsb.lol': ... } */
function fakeNetwork(answers) {
  const asked = [];
  globalThis.fetch = async (url, init) => {
    asked.push({ url: String(url), ua: init && init.headers && init.headers['User-Agent'] });
    const key = String(url).includes('adsb.fi') ? 'adsb.fi' : 'adsb.lol';
    const a = answers[key];
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

const PLANE = { hex: 'abc123', flight: 'UAL1 ', lat: 40.6, lon: -73.8, alt_baro: 30000, gs: 450, track: 90 };

test('it asks adsb.fi first and passes the aircraft on, for our site only', async () => {
  const asked = fakeNetwork({ 'adsb.fi': { body: { now: 1, aircraft: [PLANE] } } });
  const res = await ask('/planes?lat=40.64&lng=-73.78');
  eq(res.status, 200);
  eq(res.headers.get('Access-Control-Allow-Origin'), SITE);
  const body = await res.json();
  eq(body.source, 'adsb.fi');
  eq(body.ac.length, 1);
  eq(body.ac[0].hex, 'abc123');
  eq(asked.length, 1, 'only one service asked');
  eq(asked[0].url, 'https://opendata.adsb.fi/api/v2/lat/40.6/lon/-73.8/dist/250', 'rounded to ~10 km');
  assert(/GodsEyeView-relay/.test(asked[0].ua), 'it says who is asking');
});

test('adsb.lol fills in when adsb.fi refuses or sends nonsense', async () => {
  fakeNetwork({ 'adsb.fi': { status: 429, body: {} }, 'adsb.lol': { body: { ac: [PLANE] } } });
  let body = await (await ask('/planes?lat=40&lng=-73')).json();
  eq(body.source, 'adsb.lol');
  fakeNetwork({ 'adsb.fi': { body: '<html>oops</html>' }, 'adsb.lol': { body: { ac: [] } } });
  body = await (await ask('/planes?lat=40&lng=-73')).json();
  eq(body.source, 'adsb.lol', 'a page that is not JSON counts as a failure');
  fakeNetwork({ 'adsb.fi': 'throw', 'adsb.lol': { body: { ac: [PLANE] } } });
  body = await (await ask('/planes?lat=40&lng=-73')).json();
  eq(body.source, 'adsb.lol', 'a network error counts as a failure');
});

test('both services failing is a clear 502, still readable by our page', async () => {
  fakeNetwork({ 'adsb.fi': { status: 500, body: {} }, 'adsb.lol': 'throw' });
  const res = await ask('/planes?lat=40&lng=-73');
  eq(res.status, 502);
  eq(res.headers.get('Access-Control-Allow-Origin'), SITE);
  const body = await res.json();
  eq(body.details.length, 2);
});

test('other sites cannot use it, and nothing is fetched for them', async () => {
  const asked = fakeNetwork({ 'adsb.fi': { body: { aircraft: [] } } });
  const res = await ask('/planes?lat=40&lng=-73', { origin: 'https://someone-else.example' });
  eq(res.status, 403);
  eq(res.headers.get('Access-Control-Allow-Origin'), null);
  eq(asked.length, 0);
  eq((await ask('/planes?lat=40&lng=-73', { origin: null })).status, 403, 'no origin at all');
});

test('bad coordinates, wrong paths and wrong methods are refused', async () => {
  const asked = fakeNetwork({ 'adsb.fi': { body: { aircraft: [] } } });
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
console.log("\nGOD'S EYE VIEW — planes relay");
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
