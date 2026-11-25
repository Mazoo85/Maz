// Asks each outside service Terra Desk uses whether it answers a page on mazoo85.github.io (the browser's CORS
// check), the way Safari will. Informational: prints a table and always exits 0. Needs the internet (runs in CI).
//   node terradesk/tests/probe-services.mjs
import https from 'https';
import crypto from 'crypto';
const ORIGIN = 'https://mazoo85.github.io';
const GETS = {
  'Aurora (NOAA SWPC)': 'https://services.swpc.noaa.gov/json/ovation_aurora_latest.json',
  'Launches (The Space Devs)': 'https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=1&mode=normal',
  'US volcano alerts (USGS)': 'https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes',
  'Weekly volcano report (Smithsonian)': 'https://volcano.si.edu/news/WeeklyVolcanoRSS.xml',
  'Road Trip (OSRM)': 'https://router.project-osrm.org/route/v1/driving/-104.99,39.74;-105.27,40.01?overview=false',
  'Search (Nominatim)': 'https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&q=Paris',
  'Trails and builder (Overpass)': 'https://overpass-api.de/api/interpreter?data=%5Bout%3Ajson%5D%3Bnode%281%29%3Bout%3B',
  'Old Maps (Allmaps)': 'https://api.allmaps.org/maps?limit=1',
  'Lightning (NOAA GOES)': 'https://noaa-goes19.s3.amazonaws.com/?list-type=2&max-keys=1',
  'Terrain (AWS)': 'https://s3.amazonaws.com/elevation-tiles-prod/terrarium/0/0/0.png',
  'Photo map (Esri)': 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/0/0/0',
  'Battles first-use (Wikidata)': 'https://query.wikidata.org/sparql?format=json&query=ASK%7B%7D',
  'Planes (GOD\'S EYE VIEW relay)': 'https://godview-planes.cody-collins85-cc.workers.dev/planes?lat=51.5&lng=-0.1',
  'Globe library (jsDelivr)': 'https://cdn.jsdelivr.net/npm/cesium@1.147.0/Build/Cesium/Cesium.js',
};
const ask = url => new Promise(res => {
  const req = https.get(url, { headers: { Origin: ORIGIN, 'User-Agent': 'TerraDesk-probe (github.com/Mazoo85/Maz)', Accept: '*/*' }, timeout: 20000 }, r => {
    r.resume(); res({ status: r.statusCode, acao: r.headers['access-control-allow-origin'] || '' });
  });
  req.on('timeout', () => { req.destroy(); res({ status: 'timeout', acao: '' }); });
  req.on('error', e => res({ status: e.code || e.message, acao: '' }));
});
// The ship feed is a WebSocket: browsers send their Origin and the server may refuse it.
const ws = () => new Promise(res => {
  const req = https.request('https://stream.aisstream.io/v0/stream', { headers: { Connection: 'Upgrade', Upgrade: 'websocket', 'Sec-WebSocket-Version': '13',
    'Sec-WebSocket-Key': crypto.randomBytes(16).toString('base64'), Origin: ORIGIN }, timeout: 20000 });
  req.on('upgrade', (r, sock) => {
    const p = Buffer.from(JSON.stringify({ APIKey: 'probe-not-a-real-key', BoundingBoxes: [[[0, 0], [1, 1]]] })), mask = crypto.randomBytes(4);
    sock.write(Buffer.concat([Buffer.from([0x81, 0x80 | p.length]), mask, p.map((b, i) => b ^ mask[i % 4])]));
    let got = ''; sock.on('data', d => { got += d.slice(2).toString(); });
    setTimeout(() => { sock.destroy(); res({ status: 101, said: got.slice(0, 120) || '(nothing)' }); }, 4000);
  });
  req.on('response', r => { r.resume(); res({ status: r.statusCode, said: 'refused the browser handshake' }); });
  req.on('timeout', () => { req.destroy(); res({ status: 'timeout' }); });
  req.on('error', e => res({ status: e.code || e.message }));
  req.end();
});
console.log(`Origin: ${ORIGIN}\n`);
for (const [name, url] of Object.entries(GETS)) {
  const r = await ask(url);
  const ok = r.acao === '*' || r.acao === ORIGIN;
  console.log(`${ok ? 'allows the page ' : 'REFUSES the page'}  ${String(r.status).padEnd(8)} ${name}  [${r.acao || 'no Access-Control-Allow-Origin'}]`);
}
const s = await ws();
console.log(`ship feed (aisstream.io WebSocket, browser handshake): ${s.status} ${s.said || ''}`);
