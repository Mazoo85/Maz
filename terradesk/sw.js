// Terra Desk offline support. Keeps the app, the globe library and the maps you have looked at, so the app opens
// and the parts you have used work without the internet. Live data (ships, planes, launches…) is never cached
// here; the page keeps its own short-lived copies of those.
const VERSION = 'terradesk-1.1.0-iphone.1';
const SHELL = `${VERSION}-shell`, LIB = 'terradesk-lib-cesium-1.147.0', TILES = 'terradesk-tiles';
const MAX_TILES = 1500;
const CORE = ['./', 'index.html', 'styles.css', 'manifest.webmanifest', 'apple-touch-icon.png', 'icon-192.png', 'icon.png',
  'js/td-web.js', 'js/core.js', 'js/geo.js', 'js/glm.js', 'js/databuild.js', 'assets/base/day.jpg', 'assets/base/night.jpg'];
const TILE_HOSTS = ['server.arcgisonline.com', 's3.amazonaws.com', 'allmaps.xyz'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(SHELL).then(c => c.addAll(CORE)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', e => {
  e.waitUntil(caches.keys().then(keys => Promise.all(keys
    .filter(k => k.startsWith('terradesk-') && k.endsWith('-shell') && k !== SHELL).map(k => caches.delete(k))))
    .then(() => self.clients.claim()));
});

async function trim(cache) {
  const keys = await cache.keys();
  for (let i = 0; i < keys.length - MAX_TILES; i++) await cache.delete(keys[i]);
}
async function cacheFirst(req, name, limit) {
  const c = await caches.open(name);
  const hit = await c.match(req);
  if (hit) return hit;
  const res = await fetch(req);
  if (res.ok) { c.put(req, res.clone()).then(() => limit && trim(c)).catch(() => {}); }
  return res;
}
// The app's own pages and code: the newest copy when the network answers within a few seconds, else the kept one.
async function networkFirst(req) {
  const c = await caches.open(SHELL);
  const net = fetch(req).then(res => { if (res.ok) c.put(req, res.clone()).catch(() => {}); return res; });
  const kept = c.match(req, { ignoreSearch: true });
  const slow = new Promise(r => setTimeout(r, 4000)).then(() => kept);
  try {
    const first = await Promise.race([net, slow]);
    if (first) return first;
    return await net;
  } catch (e) {
    const hit = await kept;
    if (hit) return hit;
    throw e;
  }
}
// Pictures and data files: the kept copy straight away, refreshed in the background.
async function keptThenRefresh(req) {
  const c = await caches.open(SHELL);
  const hit = await c.match(req);
  const net = fetch(req).then(res => { if (res.ok) c.put(req, res.clone()).catch(() => {}); return res; });
  if (hit) { net.catch(() => {}); return hit; }
  return net;
}

self.addEventListener('fetch', e => {
  const req = e.request;
  if (req.method !== 'GET') return;
  const u = new URL(req.url);
  if (u.origin === location.origin) {
    if (!u.pathname.startsWith(new URL('./', location.href).pathname)) return;   // only Terra Desk's own files
    const code = req.mode === 'navigate' || /\.(html|js|css|webmanifest)$|\/$/.test(u.pathname);
    e.respondWith(code ? networkFirst(req) : keptThenRefresh(req));
  } else if (u.hostname === 'cdn.jsdelivr.net' && u.pathname.startsWith('/npm/cesium@')) {
    e.respondWith(cacheFirst(req, LIB, 0));
  } else if (TILE_HOSTS.includes(u.hostname) && /\.(png|jpe?g)$|\/tile\//.test(u.pathname)) {
    e.respondWith(cacheFirst(req, TILES, MAX_TILES));
  }
});
