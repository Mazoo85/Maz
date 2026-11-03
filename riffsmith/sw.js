/*
 * RIFFSMITH — offline support.
 * Keeps a copy of the app so it opens with no internet once installed.
 * Network first, so a new version is picked up whenever you're online; the
 * saved copy is used only when the network can't be reached.
 */
'use strict';

var CACHE = 'riffsmith-v2';
var FILES = [
  './', './index.html', './css/style.css', './js/riff.js', './js/synth.js', './js/app.js',
  './manifest.webmanifest', './icons/icon.svg', './icons/icon-192.png', './icons/icon-512.png'
];

self.addEventListener('install', function (e) {
  e.waitUntil(caches.open(CACHE).then(function (c) { return c.addAll(FILES); }));
  self.skipWaiting();
});

self.addEventListener('activate', function (e) {
  e.waitUntil(caches.keys().then(function (keys) {
    return Promise.all(keys.filter(function (k) { return k !== CACHE; })
      .map(function (k) { return caches.delete(k); }));
  }).then(function () { return self.clients.claim(); }));
});

self.addEventListener('fetch', function (e) {
  if (e.request.method !== 'GET') return;
  e.respondWith(fetch(e.request).then(function (res) {
    if (res && res.ok && new URL(e.request.url).origin === location.origin) {
      var copy = res.clone();
      caches.open(CACHE).then(function (c) { c.put(e.request, copy); });
    }
    return res;
  }).catch(function () {
    return caches.match(e.request, { ignoreSearch: true }).then(function (hit) {
      return hit || caches.match('./index.html');
    });
  }));
});
