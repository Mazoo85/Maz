/*
 * GOD'S EYE VIEW - the planes relay (version 2).
 *
 * Every free flight-data service answers a server but refuses a web page
 * (no Access-Control-Allow-Origin for anyone else's site), so the PLANES layer
 * asks this tiny relay instead. It runs as a free Cloudflare Worker and asks,
 * in turn: adsb.lol, then OpenSky, then adsb.fi, and hands the first answer
 * back with permission for GOD'S EYE VIEW's site only. Cloudflare's shared
 * addresses get "too many requests" now and then, so the last good answer for
 * each area is remembered for two minutes and used if every source refuses.
 *
 *   GET /planes?lat=40.6&lng=-73.8   ->   { "source": "adsb.lol", "ac": [ ... ] }
 *   GET /check                       ->   what each source says right now
 *
 * Self-contained on purpose: the whole file is pasted into the Cloudflare
 * dashboard as it is. Setup steps are in README.md beside it.
 */

/* The only sites allowed to use this relay. */
const ALLOWED_ORIGINS = ['https://mazoo85.github.io'];

const RADIUS_NM = 250;
const UPSTREAM_TIMEOUT_MS = 8000;
const REMEMBER_MS = 2 * 60 * 1000;
const USER_AGENT = 'GodsEyeView-relay (+https://github.com/Mazoo85/Maz/tree/main/godview)';

/* OpenSky answers for a box, in metres and m/s; this turns its rows into the
 * same shape adsb.lol uses, so the page reads every source the same way. */
function openskyBox(lat, lng) {
  const dLat = RADIUS_NM * 1.852 / 111.32;
  const dLng = dLat / Math.max(0.05, Math.cos(lat * Math.PI / 180));
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  return 'lamin=' + clamp(lat - dLat, -90, 90).toFixed(2) + '&lomin=' + clamp(lng - dLng, -180, 180).toFixed(2) +
    '&lamax=' + clamp(lat + dLat, -90, 90).toFixed(2) + '&lomax=' + clamp(lng + dLng, -180, 180).toFixed(2);
}

function openskyAircraft(json) {
  if (!json || !('states' in json)) return null;
  const now = Number(json.time);
  const out = [];
  for (const s of json.states || []) {
    if (!Array.isArray(s) || typeof s[5] !== 'number' || typeof s[6] !== 'number') continue;
    out.push({
      hex: s[0],
      flight: s[1] || '',
      lat: s[6],
      lon: s[5],
      alt_baro: s[8] === true ? 'ground' : (typeof s[7] === 'number' ? Math.round(s[7] * 3.28084) : null),
      gs: typeof s[9] === 'number' ? Math.round(s[9] * 1.943844 * 10) / 10 : null,
      track: s[10],
      seen_pos: typeof s[3] === 'number' && Number.isFinite(now) ? now - s[3] : null
    });
  }
  return out;
}

const SOURCES = [
  {
    name: 'adsb.lol',
    url: (lat, lng) => `https://api.adsb.lol/v2/point/${lat}/${lng}/${RADIUS_NM}`,
    aircraft: (json) => json && json.ac
  },
  {
    name: 'OpenSky',
    url: (lat, lng) => 'https://opensky-network.org/api/states/all?' + openskyBox(lat, lng),
    aircraft: openskyAircraft
  },
  {
    name: 'adsb.fi',
    url: (lat, lng) => `https://opendata.adsb.fi/api/v2/lat/${lat}/lon/${lng}/dist/${RADIUS_NM}`,
    aircraft: (json) => json && json.aircraft
  }
];

/* The last good answer for each area, kept while this copy of the relay is
 * running (Cloudflare reuses a running copy for many requests). */
const remembered = new Map();

function remember(key, answer) {
  remembered.delete(key);
  remembered.set(key, { at: Date.now(), answer: answer });
  if (remembered.size > 500) remembered.delete(remembered.keys().next().value);
}

function recall(key) {
  const r = remembered.get(key);
  return r && Date.now() - r.at < REMEMBER_MS ? r.answer : null;
}

function corsHeaders(origin) {
  return {
    'Access-Control-Allow-Origin': origin,
    'Access-Control-Allow-Methods': 'GET, OPTIONS',
    'Access-Control-Max-Age': '86400',
    Vary: 'Origin'
  };
}

function reply(status, body, origin) {
  return new Response(JSON.stringify(body), {
    status: status,
    headers: Object.assign(
      { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' },
      origin ? corsHeaders(origin) : {}
    )
  });
}

/* A coordinate rounded to one decimal place (about 10 km), or null if it is
 * not a real latitude/longitude. */
function coordinate(value, limit) {
  if (value === null || value === '') return null;
  const n = Number(value);
  if (!Number.isFinite(n) || Math.abs(n) > limit) return null;
  return Math.round(n * 10) / 10;
}

async function askSource(source, lat, lng) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), UPSTREAM_TIMEOUT_MS);
  try {
    const res = await fetch(source.url(lat, lng), {
      headers: { 'User-Agent': USER_AGENT, Accept: 'application/json' },
      signal: controller.signal
    });
    if (!res.ok) throw new Error(source.name + ' answered ' + res.status);
    let json;
    try {
      json = await res.json();
    } catch (e) {
      throw new Error(source.name + ' sent something that is not JSON');
    }
    const list = source.aircraft(json);
    if (!Array.isArray(list)) throw new Error(source.name + ' sent no aircraft list');
    return list;
  } catch (e) {
    if (e && e.name === 'AbortError') throw new Error(source.name + ' took too long');
    throw e;
  } finally {
    clearTimeout(timer);
  }
}

/* A status page anyone can open: what each source says right now about
 * New York, and which Cloudflare location this ran in. No plane data. */
async function check(request) {
  const results = [];
  for (const source of SOURCES) {
    try {
      const ac = await askSource(source, 40.6, -73.8);
      results.push({ source: source.name, ok: true, aircraft: ac.length });
    } catch (e) {
      results.push({ source: source.name, ok: false, error: String(e && e.message || e) });
    }
  }
  return reply(200, { relay: 'version 2', cloudflare: (request.cf && request.cf.colo) || null, sources: results }, null);
}

export default {
  async fetch(request) {
    const origin = request.headers.get('Origin');
    const allowed = ALLOWED_ORIGINS.includes(origin) ? origin : null;
    const url = new URL(request.url);

    if (request.method === 'OPTIONS') {
      return new Response(null, { status: allowed ? 204 : 403, headers: allowed ? corsHeaders(allowed) : {} });
    }
    if (request.method !== 'GET') return reply(405, { error: 'only GET' }, allowed);
    if (url.pathname === '/check') return check(request);
    if (url.pathname !== '/planes') {
      return reply(404, { error: 'ask /planes?lat=..&lng=.. or /check' }, allowed);
    }
    if (!allowed) return reply(403, { error: 'this relay only serves GOD\'S EYE VIEW' }, null);

    const lat = coordinate(url.searchParams.get('lat'), 90);
    const lng = coordinate(url.searchParams.get('lng'), 180);
    if (lat === null || lng === null) return reply(400, { error: 'lat and lng must be real coordinates' }, allowed);
    const key = lat + ',' + lng;

    const failures = [];
    for (const source of SOURCES) {
      try {
        const answer = { source: source.name, ac: await askSource(source, lat, lng) };
        remember(key, answer);
        return reply(200, answer, allowed);
      } catch (e) {
        failures.push(String(e && e.message || e));
      }
    }
    const last = recall(key);
    if (last) return reply(200, Object.assign({ stale: true }, last), allowed);
    return reply(502, { error: 'no flight source answered', details: failures }, allowed);
  }
};
