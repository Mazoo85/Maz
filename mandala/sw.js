/*
 * MANDALA MAKER — offline support
 * -------------------------------
 * Keeps a copy of every file the app needs, so once it is installed it
 * opens with no internet at all. It serves the saved copy straight away and
 * quietly fetches a fresh one in the background, so an update made on the
 * website arrives the next time the app is opened.
 *
 * Bump VERSION whenever the file list changes; old copies are cleared out.
 */
var VERSION = 'mandala-v1';
var FILES = [
  './',
  'index.html',
  'css/style.css',
  'js/mandala.js',
  'js/app.js',
  'js/install.js',
  'manifest.webmanifest',
  'icons/icon-192.png',
  'icons/icon-512.png',
  'icons/maskable-512.png',
  'icons/apple-touch-icon.png'
];

self.addEventListener('install', function (event) {
  event.waitUntil(caches.open(VERSION).then(function (cache) { return cache.addAll(FILES); }));
  self.skipWaiting();
});

self.addEventListener('activate', function (event) {
  event.waitUntil(caches.keys().then(function (keys) {
    return Promise.all(keys.filter(function (k) { return k !== VERSION; }).map(function (k) {
      return caches.delete(k);
    }));
  }).then(function () { return self.clients.claim(); }));
});

self.addEventListener('fetch', function (event) {
  var req = event.request;
  if (req.method !== 'GET' || new URL(req.url).origin !== self.location.origin) return;
  event.respondWith(caches.open(VERSION).then(function (cache) {
    return cache.match(req, { ignoreSearch: true }).then(function (saved) {
      var fresh = fetch(req).then(function (res) {
        if (res && res.ok) cache.put(req, res.clone());
        return res;
      }).catch(function () { return saved; });
      return saved || fresh;
    });
  }));
});
