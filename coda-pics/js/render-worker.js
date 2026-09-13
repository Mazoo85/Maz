/*
 * CODA PICS — the render worker.
 *
 * Painting a 2560 × 1440 picture is a few hundred milliseconds of solid
 * arithmetic, and on the main thread that is a few hundred milliseconds where
 * the page cannot scroll, the button cannot un-press, and a phone looks broken.
 * This runs the identical engine off to one side and hands back the finished
 * pixels.
 *
 * "Identical" is the important word: it loads the very same five files the page
 * does, so a picture painted here and a picture painted on the main thread are
 * the same picture. The app falls back to the main thread wherever workers or
 * OffscreenCanvas are missing, and the two paths must never disagree.
 */
'use strict';

importScripts('lexicon.js', 'prompt.js', 'subjects.js', 'paint.js', 'finish.js');

self.addEventListener('message', function (event) {
  var msg = event.data || {};
  if (!msg.spec || !msg.w || !msg.h) return;

  try {
    var canvas = new OffscreenCanvas(msg.w, msg.h);
    var ctx = canvas.getContext('2d');
    var palette = self.CodaPaint.render(ctx, msg.w, msg.h, msg.spec);
    self.CodaFinish.apply(ctx, msg.w, msg.h, msg.spec, palette);
    var bitmap = canvas.transferToImageBitmap();
    self.postMessage({ id: msg.id, bitmap: bitmap }, [bitmap]);
  } catch (err) {
    /* The page paints it itself rather than showing nothing. */
    self.postMessage({ id: msg.id, error: String(err && err.message || err) });
  }
});
