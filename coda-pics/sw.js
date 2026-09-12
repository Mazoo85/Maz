/*
 * CODA PICS — the offline worker.
 *
 * The app already runs without a network once its files are in the browser:
 * there is no model to fetch and no API to call. This is what makes that true
 * after you close the tab — add CODA PICS to your home screen and it opens and
 * paints on a plane, in a tunnel, with the wifi off.
 *
 * Network first, cache second. The cache is a fallback, never the source of
 * truth, so a published change reaches you on the next load while you are
 * online — the opposite ordering is how an installed page gets stuck on a
 * version from weeks ago with no way to tell.
 */
'use strict';

var CACHE = 'coda-pics-v1';

/* The whole app: its page, its look, its five engine files, its icons — and
 * the shared arcade nav, which lives outside this worker's scope but is still
 * worth having offline so the way back out keeps working. */
var SHELL = [
  './',
  'index.html',
  'css/style.css',
  'js/lexicon.js',
  'js/prompt.js',
  'js/subjects.js',
  'js/paint.js',
  'js/finish.js',
  'js/app.js',
  'manifest.webmanifest',
  'icons/icon-192.png',
  'icons/icon-512.png',
  'icons/icon-maskable-512.png',
  'icons/apple-touch-icon.png',
  '../shared/maz-nav.js',
  '../shared/projects.js'
];

self.addEventListener('install', function (event) {
  event.waitUntil(
    caches.open(CACHE).then(function (cache) {
      /* One missing file must not fail the whole install, or a single renamed
       * asset would leave the app with no offline copy at all. */
      return Promise.all(SHELL.map(function (url) {
        return cache.add(new Request(url, { cache: 'reload' }))['catch'](function () {});
      }));
    }).then(function () { return self.skipWaiting(); })
  );
});

self.addEventListener('activate', function (event) {
  event.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(keys.map(function (k) {
        return k === CACHE ? null : caches['delete'](k);
      }));
    }).then(function () { return self.clients.claim(); })
  );
});

self.addEventListener('fetch', function (event) {
  var req = event.request;
  if (req.method !== 'GET') return;
  if (new URL(req.url).origin !== self.location.origin) return;

  event.respondWith(
    fetch(req).then(function (res) {
      if (res && res.ok) {
        var copy = res.clone();
        caches.open(CACHE).then(function (c) { c.put(req, copy); });
      }
      return res;
    })['catch'](function () {
      return caches.match(req).then(function (hit) {
        /* A navigation that misses the cache still has somewhere to land. */
        return hit || (req.mode === 'navigate' ? caches.match('index.html') : undefined);
      });
    })
  );
});
