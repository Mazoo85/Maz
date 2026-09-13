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

var CACHE = 'coda-pics-v2';

/* The whole app: its page, its look, its engine files, its icons — and
 * the shared arcade nav, which lives outside this worker's scope but is still
 * worth having offline so the way back out keeps working. */
var SHELL = [
  './',
  'index.html',
  'css/style.css',
  'js/lexicon.js',
  'js/photo.js',
  'js/prompt.js',
  'js/subjects.js',
  'js/paint.js',
  'js/finish.js',
  'js/gphotos.js',
  'js/app.js',
  'js/render-worker.js',
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
  var url = new URL(req.url);
  if (url.origin !== self.location.origin) return;
  /* A sign-in comes back as index.html?code=… and that code is a secret with
   * one use in it. Answer such a request, never keep a copy of it. */
  var keep = !url.search;

  event.respondWith(
    fetch(req).then(function (res) {
      if (keep && res && res.ok) {
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
