/*
 * GOD'S EYE VIEW - the planes relay.
 *
 * Every free flight-data service answers a server but refuses a web page
 * (no Access-Control-Allow-Origin for anyone else's site), so the PLANES layer
 * asks this tiny relay instead. It runs as a free Cloudflare Worker: it asks
 * adsb.fi for the aircraft around a point (adsb.lol if adsb.fi fails), and
 * hands the answer back with permission for GOD'S EYE VIEW's site only.
 *
 *   GET /planes?lat=40.6&lng=-73.8   ->   { "source": "adsb.fi", "ac": [ ... ] }
 *
 * Self-contained on purpose: the whole file is pasted into the Cloudflare
 * dashboard as it is. Setup steps are in README.md beside it.
 */

/* The only sites allowed to use this relay. */
const ALLOWED_ORIGINS = ['https://mazoo85.github.io'];

const RADIUS_NM = 250;
const UPSTREAM_TIMEOUT_MS = 8000;
const USER_AGENT = 'GodsEyeView-relay (+https://github.com/Mazoo85/Maz/tree/main/godview)';

const SOURCES = [
  {
    name: 'adsb.fi',
    url: (lat, lng) => `https://opendata.adsb.fi/api/v2/lat/${lat}/lon/${lng}/dist/${RADIUS_NM}`,
    aircraft: (json) => json && json.aircraft
  },
  {
    name: 'adsb.lol',
    url: (lat, lng) => `https://api.adsb.lol/v2/point/${lat}/${lng}/${RADIUS_NM}`,
    aircraft: (json) => json && json.ac
  }
];

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
    const list = source.aircraft(await res.json());
    if (!Array.isArray(list)) throw new Error(source.name + ' sent no aircraft list');
    return list;
  } finally {
    clearTimeout(timer);
  }
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
    if (url.pathname !== '/planes') {
      return reply(404, { error: 'ask /planes?lat=..&lng=..' }, allowed);
    }
    if (!allowed) return reply(403, { error: 'this relay only serves GOD\'S EYE VIEW' }, null);

    const lat = coordinate(url.searchParams.get('lat'), 90);
    const lng = coordinate(url.searchParams.get('lng'), 180);
    if (lat === null || lng === null) return reply(400, { error: 'lat and lng must be real coordinates' }, allowed);

    const failures = [];
    for (const source of SOURCES) {
      try {
        const ac = await askSource(source, lat, lng);
        return reply(200, { source: source.name, ac: ac }, allowed);
      } catch (e) {
        failures.push(String(e && e.message || e));
      }
    }
    return reply(502, { error: 'no flight source answered', details: failures }, allowed);
  }
};
