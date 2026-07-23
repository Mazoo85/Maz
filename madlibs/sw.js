/*
 * MadLibs Story Forge — service worker
 * ------------------------------------
 * Precaches the full app shell so the app installs and runs fully offline.
 * Everything is same-origin and static, so a simple cache-first strategy with
 * a network fallback is all we need. Bump CACHE_VERSION whenever any cached
 * asset changes so clients pick up the new build.
 */
const CACHE_VERSION = 'v1';
const CACHE_NAME = 'madlibs-forge-' + CACHE_VERSION;

// Paths are relative to the service worker's scope (the madlibs/ folder).
const APP_SHELL = [
  './',
  './index.html',
  './css/style.css',
  './js/dictionary.js',
  './js/templates.js',
  './js/generator.js',
  './js/app.js',
  './manifest.webmanifest',
  './icons/icon-192.png',
  './icons/icon-512.png',
  './icons/icon-maskable-192.png',
  './icons/icon-maskable-512.png',
  './icons/apple-touch-icon-180.png'
];

self.addEventListener('install', function (event) {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then(function (cache) { return cache.addAll(APP_SHELL); })
      .then(function () { return self.skipWaiting(); })
  );
});

self.addEventListener('activate', function (event) {
  event.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(keys.map(function (key) {
        if (key !== CACHE_NAME) return caches.delete(key);
      }));
    }).then(function () { return self.clients.claim(); })
  );
});

self.addEventListener('fetch', function (event) {
  var req = event.request;
  if (req.method !== 'GET') return;

  // For page navigations, fall back to the cached shell when offline.
  if (req.mode === 'navigate') {
    event.respondWith(
      fetch(req).catch(function () {
        return caches.match('./index.html');
      })
    );
    return;
  }

  // Cache-first for everything else, filling the cache as we go.
  event.respondWith(
    caches.match(req).then(function (cached) {
      return cached || fetch(req).then(function (res) {
        if (res && res.status === 200 && res.type === 'basic') {
          var copy = res.clone();
          caches.open(CACHE_NAME).then(function (cache) { cache.put(req, copy); });
        }
        return res;
      });
    })
  );
});
