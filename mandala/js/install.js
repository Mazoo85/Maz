/*
 * MANDALA MAKER — install as an app
 * ---------------------------------
 * Turns on offline support, and shows an "Install app" button wherever the
 * browser can install it (Chrome and Edge on a computer, Chrome on Android).
 * Elsewhere the browser's own menu does it — Safari: File → Add to Dock, or
 * Share → Add to Home Screen — so the button simply stays hidden.
 *
 * Kept apart from app.js: the claude.ai version of the page cannot be
 * installed and does not load this file.
 */
(function () {
  'use strict';

  if ('serviceWorker' in navigator && window.isSecureContext) {
    window.addEventListener('load', function () {
      navigator.serviceWorker.register('sw.js').catch(function () { /* still works online */ });
    });
  }

  var button = document.getElementById('installApp');
  if (!button) return;
  var offer = null;

  window.addEventListener('beforeinstallprompt', function (e) {
    e.preventDefault();
    offer = e;
    button.hidden = false;
  });

  button.addEventListener('click', function () {
    if (!offer) return;
    offer.prompt();
    offer.userChoice.then(function () {
      offer = null;
      button.hidden = true;
    });
  });

  window.addEventListener('appinstalled', function () {
    offer = null;
    button.hidden = true;
  });
})();
