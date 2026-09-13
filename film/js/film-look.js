/* film-look.js — which of the two renderers draws this film.
 *
 * SCRIPT FORGE has two renderers now and they read the same reel. The FLAT one paints a shot as three
 * planes at three parallax rates with silhouettes between them, and it is what has always been here.
 * The 3D one stages the shot instead — a room with a floor and walls, bodies built to human
 * proportions standing in it, and a camera with a focal length — and it is the same C++ that renders a
 * film on a build machine, compiled to WebAssembly and running inside this page.
 *
 * Everything else in the app calls Look.drawFrame and does not care which is which. That is the whole
 * design: the reel is the film, the renderer is a look, and swapping one for the other is a setting
 * rather than a different program.
 *
 * The 3D module is 147 KB and is fetched ONLY when somebody asks for it. Nobody who never touches the
 * control pays for it, and once it has been fetched it is cached like any other file, so it works with
 * the wifi off from then on — which is the promise the whole app is built around.
 */
(function (root) {
  'use strict';

  var FlatPlayer = root.FilmPlayer;

  /* Where the module lives, relative to the page. */
  var WASM_DIR = 'wasm/';

  var mod = null;          /* the instantiated WebAssembly module */
  var loading = null;      /* the in-flight promise, so two requests share one fetch */
  var failed = '';         /* why it could not be had, if it could not */
  var loadedReel = null;   /* which reel the module currently holds */

  var current = 'flat';

  /* How hard to work. Playing has to keep up with the film; writing a file does not.
   *
   * A frame at 480 across takes about 23 ms at PLAY and about 57 at KEEP, so PLAY leaves room at
   * twelve frames a second on a machine several times slower than the one those were measured on. */
  var PLAY = { supersample: 1, shadows: 1 };
  var KEEP = { supersample: 2, shadows: 2 };

  /* ------------------------------------------------------------------ the module */

  function scriptUrl() {
    /* Relative to this script, so the app works from a sub-path as well as from the root. */
    var here = document.currentScript && document.currentScript.src;
    if (!here) return WASM_DIR + 'film3d.js';
    return here.replace(/js\/film-look\.js.*$/, '') + WASM_DIR + 'film3d.js';
  }

  function loadModule() {
    if (mod) return Promise.resolve(mod);
    if (loading) return loading;
    loading = new Promise(function (resolve, reject) {
      if (typeof WebAssembly !== 'object') {
        reject(new Error('this browser has no WebAssembly'));
        return;
      }
      var tag = document.createElement('script');
      tag.src = scriptUrl();
      tag.onload = function () {
        if (typeof root.MazFilm3D !== 'function') {
          reject(new Error('the 3D renderer did not load'));
          return;
        }
        root.MazFilm3D().then(resolve, reject);
      };
      tag.onerror = function () { reject(new Error('the 3D renderer could not be fetched')); };
      document.head.appendChild(tag);
    }).then(function (m) {
      mod = {
        raw: m,
        load: m.cwrap('maz3d_load', 'number', ['string']),
        errorAt: m.cwrap('maz3d_error', 'number', []),
        render: m.cwrap('maz3d_render', 'number',
                        ['number', 'number', 'number', 'number', 'number'])
      };
      return mod;
    }, function (err) {
      failed = err && err.message ? err.message : String(err);
      loading = null;
      throw err;
    });
    return loading;
  }

  /* Hand the module a reel. Cheap to call again with the same one. */
  function giveReel(reel) {
    if (!mod || !reel) return false;
    if (loadedReel === reel) return true;
    /* The same document the ⬇ .reel.json button writes, so the renderer in the page is fed exactly
     * what the renderer on a build machine is fed. */
    var text = root.FilmReel && root.FilmReel.toJson ? root.FilmReel.toJson(reel) : JSON.stringify(reel);
    if (!mod.load(text)) {
      failed = mod.raw.UTF8ToString(mod.errorAt()) || 'the reel was refused';
      loadedReel = null;
      return false;
    }
    loadedReel = reel;
    return true;
  }

  /* ------------------------------------------------------------------ drawing */

  function draw3D(ctx, width, height, reel, time, opts) {
    if (!mod || !giveReel(reel)) return false;
    var how = (opts && opts.keep) ? KEEP : PLAY;
    var ptr = mod.render(time || 0, width, height, how.supersample, how.shadows);
    if (!ptr) return false;
    var bytes = mod.raw.HEAPU8.subarray(ptr, ptr + width * height * 4);
    var image = ctx.createImageData(width, height);
    image.data.set(bytes);
    ctx.putImageData(image, 0, 0);
    return true;
  }

  /* One frame of the film, in whichever look is on. Falls back to the flat renderer rather than
   * showing nothing: a film that will not play is worse than a film that plays flat. */
  function drawFrame(ctx, width, height, reel, time, opts) {
    if (current === '3d' && draw3D(ctx, width, height, reel, time, opts)) return '3d';
    FlatPlayer.drawFrame(ctx, width, height, reel, time, opts);
    return 'flat';
  }

  /* ------------------------------------------------------------------ the setting */

  function looks() {
    return [
      { id: 'flat', name: 'Flat', note: 'Painted planes and silhouettes. Always ready.' },
      { id: '3d', name: '3D', note: 'Rooms with floors, bodies in proportion, real shadows.' }
    ];
  }

  function chosen() { return current; }

  /* Switch looks. Returns a promise so the caller can say "loading" while the module arrives; asking
   * for flat resolves at once and never touches the network. */
  function choose(id) {
    if (id !== '3d') {
      current = 'flat';
      return Promise.resolve('flat');
    }
    return loadModule().then(function () {
      current = '3d';
      return '3d';
    });
  }

  function ready() { return !!mod; }
  function problem() { return failed; }

  /* How wide the 3D look is allowed to draw.
   *
   * Every pixel is worked out on the processor, so this is a real budget rather than a preference. A
   * film at twelve frames a second has 83 milliseconds a frame. Measured in a browser on a desktop:
   * 640 across took 43 ms and 480 took 27. Forty-three would be fine here and is not fine on a phone,
   * which is two or three times slower — so the ceiling is 480, which leaves room for a phone to be
   * three times slower than this machine and still play the film.
   *
   * Nothing is lost by it: the film is letterboxed 2.35:1 and scaled up to fill whatever it is shown
   * in, and 480 across is the size these films have been rendered at all along. */
  var MAX_WIDTH = 480;

  function playWidth(available) {
    return Math.max(240, Math.min(Math.round(available || MAX_WIDTH), MAX_WIDTH));
  }

  var API = {
    drawFrame: drawFrame,
    looks: looks,
    chosen: chosen,
    choose: choose,
    ready: ready,
    problem: problem,
    playWidth: playWidth,
    MAX_WIDTH: MAX_WIDTH,
    PLAY: PLAY,
    KEEP: KEEP
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmLook = API;
})(typeof window !== 'undefined' ? window : this);
