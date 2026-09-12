/*
 * The two things this host does differently, declared before the app loads.
 *
 * 1. It serves the page as a single file, so there is no worker script to
 *    fetch: the app paints on the main thread instead of failing a request.
 * 2. A page here cannot start its own download. The viewer has a save prompt
 *    of its own, and the app hands files to it — hiding the save buttons
 *    entirely where that is unavailable, rather than offering a button that
 *    quietly does nothing.
 */
window.CODA_NO_WORKER = true;

(function () {
  'use strict';
  var downloads = null;

  window.CODA_SAVE = function (blob, name, thenSay) {
    if (!downloads) { if (thenSay) thenSay(false); return; }
    downloads.save({ filename: name, data: blob }).then(function () {
      if (thenSay) thenSay(true);
    }, function (err) {
      if (thenSay) thenSay(false);
      if (err && err.code && err.code !== 'declined' && window.console) {
        console.warn('save refused:', err.code);
      }
    });
  };

  function saveButtons() {
    return [document.getElementById('download'), document.getElementById('exportGallery')]
      .filter(Boolean);
  }

  function hideSaving() { saveButtons().forEach(function (b) { b.hidden = true; }); }

  document.addEventListener('DOMContentLoaded', function () {
    /* The app builds its own controls on this same event, and this listener
     * runs first, so everything that reads them waits a beat. */
    setTimeout(function () {
      hideSaving();
      var box = document.getElementById('prompt');
      var paint = document.getElementById('paint');
      if (!box || !paint) return;
      /* An opening picture, so the first thing on screen is what the page does. */
      if (!box.value.trim()) {
        var chips = document.querySelectorAll('.examples .chip');
        if (chips.length) {
          box.value = chips[Math.floor(Math.random() * chips.length)].textContent;
        }
      }
      if (box.value.trim()) paint.click();
    }, 60);
  });

  if (window.claude && window.claude.use) {
    window.claude.use('downloads').then(function (d) {
      downloads = d;
      if (d) saveButtons().forEach(function (b) { b.hidden = false; });
    }, function () { /* stays hidden */ });
  }
})();
