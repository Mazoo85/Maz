// Service worker for NAME FORGE. The app already runs entirely in the page —
// the word banks are plain scripts, nothing is fetched while you roll — so
// caching the handful of files it is made of is all it takes to make it work
// with no signal at all, and installable to a home screen.
//
// Bump CACHE whenever any file in ASSETS changes, or browsers will keep
// serving the old copy: activate deletes every cache that is not this one.
const CACHE = 'name-forge-v1';

// The app's own files, plus the two shared arcade files the page pulls in.
// Those live outside this worker's scope, which stops it controlling pages
// there but not caching and serving them for a page it does control.
const ASSETS = [
  './',
  './index.html',
  './manifest.webmanifest',
  './css/style.css',
  './js/words.js',
  './js/generator.js',
  './js/app.js',
  './icons/icon-192.png',
  './icons/icon-512.png',
  './icons/icon-180.png',
  '../shared/maz-nav.js',
  '../shared/projects.js'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE)
      // One miss must not fail the whole install and leave the app with no
      // offline copy at all, so each file is added on its own.
      .then((cache) => Promise.all(
        ASSETS.map((url) => cache.add(url).catch(() => undefined))
      ))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const req = event.request;
  if (req.method !== 'GET') return;
  event.respondWith(
    caches.match(req, { ignoreSearch: true }).then((cached) => {
      if (cached) return cached;
      return fetch(req).catch(() => {
        // Offline and never cached: a navigation still gets the app itself
        // rather than the browser's error page.
        if (req.mode === 'navigate') return caches.match('./index.html');
        return Response.error();
      });
    })
  );
});
