/*
 * CODA PICS — the app.
 * --------------------
 * Wires the page to the picture: read the box, paint, show what was understood,
 * and let people keep what they like. Nothing here knows how a dragon is drawn
 * — that is js/subjects.js — and nothing here decides what the words mean —
 * that is js/prompt.js. This file is only the handle on the door.
 *
 * The gallery stores the words and the seed, never the pixels: a kept picture
 * is repainted from its prompt, which is why a hundred of them cost a few
 * kilobytes and still come back exactly as they were.
 */
(function () {
  'use strict';

  var LEX = window.CodaLexicon;
  var PROMPT = window.CodaPrompt;
  var PAINT = window.CodaPaint;
  var FINISH = window.CodaFinish;

  var SIZES = {
    square: { w: 1024, h: 1024, label: 'square' },
    wide:   { w: 1280, h: 720,  label: 'wide' },
    tall:   { w: 832,  h: 1216, label: 'tall' },
    phone:  { w: 720,  h: 1280, label: 'phone wallpaper' }
  };

  var EXAMPLES = [
    'a red dragon over snowy mountains at sunset',
    'a lonely lighthouse in a storm, watercolour',
    'a fox in a pine forest at dawn',
    'a whale under a huge moon, woodblock',
    'neon city street in the rain',
    'an astronaut on a quiet desert planet',
    'a tiny cabin in falling snow, storybook',
    'a castle in the fog, film noir',
    'giant mushrooms in a glowing cave',
    'a hot air balloon over a canyon, poster'
  ];

  var STORE_KEY = 'codaPics.gallery.v1';
  var LAST_KEY = 'codaPics.last.v1';
  var MAX_KEPT = 24;

  var el = {};
  var current = null;             // the spec on screen right now
  var seed = 1;
  var busy = false;
  var painted = 0;                // how many finished pictures; the tests watch it

  /* ------------------------------------------------------------- storage
   * Private browsing, a full disk or a locked-down browser all make
   * localStorage throw. A gallery is a nicety; nothing here may take the app
   * down with it.
   */
  function load(key, fallback) {
    try {
      var raw = window.localStorage.getItem(key);
      return raw ? JSON.parse(raw) : fallback;
    } catch (e) { return fallback; }
  }

  function save(key, value) {
    try {
      window.localStorage.setItem(key, JSON.stringify(value));
      return true;
    } catch (e) { return false; }
  }

  /* ---------------------------------------------------------------- paint */

  function sizeOf() { return SIZES[el.shape.value] || SIZES.wide; }

  function draw(canvas, spec, w, h) {
    canvas.width = w;
    canvas.height = h;
    var ctx = canvas.getContext('2d');
    if (!ctx) return false;
    var palette = PAINT.render(ctx, w, h, spec);
    FINISH.apply(ctx, w, h, spec, palette);
    return true;
  }

  function repaint(newSeed) {
    if (busy) return;
    var text = el.prompt.value.trim();
    if (!text) {
      el.prompt.focus();
      setStatus('Tell me what to draw first — even two words will do.');
      return;
    }
    if (newSeed != null) seed = newSeed;

    busy = true;
    el.busy.hidden = false;
    el.placeholder.hidden = true;   // one overlay at a time, never both
    el.paint.disabled = el.reroll.disabled = true;

    var size = sizeOf();
    var spec = PROMPT.parse(text, { seed: seed, style: el.style.value });

    /* Let the "painting…" state reach the screen before the work starts, since
     * the paint itself blocks the main thread. A timer rather than a frame
     * callback on purpose: a browser throttles animation frames in a tab you
     * have switched away from, and a picture you asked for should still be
     * waiting when you come back. */
    void el.busy.offsetHeight;      // force the style change out now
    setTimeout(function () {
      var ok = false;
      try {
        ok = draw(el.canvas, spec, size.w, size.h);
      } catch (err) {
        setStatus('Something went wrong painting that one. Try another take.');
        if (window.console) console.error(err);
      }
      busy = false;
      el.busy.hidden = true;
      el.paint.disabled = el.reroll.disabled = false;
      if (!ok) {
        el.placeholder.hidden = !!current;
        return;
      }

      current = spec;
      el.outButtons.hidden = false;
      showReadout(spec, size);
      save(LAST_KEY, { prompt: text, seed: seed, style: el.style.value, shape: el.shape.value });
      el.canvas.dataset.painted = String(++painted);
    }, 20);
  }

  function setStatus(html) { el.status.innerHTML = html; }

  function showReadout(spec, size) {
    setStatus('<b>' + escapeHtml(PROMPT.describe(spec)) + '</b> · ' +
      size.w + ' × ' + size.h + ' · seed ' + spec.seed);

    el.readout.innerHTML = '';
    spec.read.forEach(function (item) {
      var tag = document.createElement('span');
      var guess = !item.word;
      tag.className = 'tag' + (guess ? ' guess' : '');
      tag.innerHTML = '<span>' + escapeHtml(item.category) + '</span> ' + escapeHtml(item.label) +
        (item.word ? ' <span>← “' + escapeHtml(item.word) + '”</span>' : '');
      el.readout.appendChild(tag);
    });
  }

  var escapeHtml = MazUtil.escapeHtml;

  /* -------------------------------------------------------------- keeping */

  function slug(text) {
    return String(text).toLowerCase().replace(/[^a-z0-9]+/g, '-')
      .replace(/^-|-$/g, '').slice(0, 40) || 'picture';
  }

  function download() {
    if (!current) return;
    var name = 'coda-pics-' + slug(current.prompt) + '-' + current.seed + '.png';
    if (el.canvas.toBlob) {
      el.canvas.toBlob(function (blob) {
        if (!blob) return;
        var url = URL.createObjectURL(blob);
        linkTo(url, name);
        setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
      }, 'image/png');
    } else {
      linkTo(el.canvas.toDataURL('image/png'), name);
    }
  }

  function linkTo(href, name) {
    var a = document.createElement('a');
    a.href = href;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }

  function keep() {
    if (!current) return;
    var kept = load(STORE_KEY, []);
    var entry = {
      prompt: current.prompt,
      seed: current.seed,
      style: el.style.value,
      shape: el.shape.value,
      at: Date.now()
    };
    kept = kept.filter(function (k) {
      return !(k.prompt === entry.prompt && k.seed === entry.seed && k.style === entry.style);
    });
    kept.unshift(entry);
    if (kept.length > MAX_KEPT) kept.length = MAX_KEPT;
    if (save(STORE_KEY, kept)) {
      renderGallery();
      setStatus('Kept in your gallery. It lives on this device only.');
    } else {
      setStatus('This browser will not let me store anything, so the gallery is off. ' +
        'Use <b>Save the picture</b> instead.');
    }
  }

  function renderGallery() {
    var kept = load(STORE_KEY, []);
    el.galleryWrap.hidden = kept.length === 0;
    el.gallery.innerHTML = '';

    kept.forEach(function (entry, i) {
      var size = SIZES[entry.shape] || SIZES.wide;
      var tw = 240;
      var th = Math.round(tw * size.h / size.w);

      var card = document.createElement('button');
      card.className = 'card';
      card.type = 'button';
      card.title = 'Bring back: ' + entry.prompt;

      var thumb = document.createElement('canvas');
      var spec = PROMPT.parse(entry.prompt, { seed: entry.seed, style: entry.style });
      try {
        draw(thumb, spec, tw, th);
      } catch (e) { /* a thumbnail is not worth failing over */ }
      card.appendChild(thumb);

      var label = document.createElement('span');
      label.className = 'label';
      label.textContent = entry.prompt;
      card.appendChild(label);

      card.addEventListener('click', function () {
        el.prompt.value = entry.prompt;
        el.style.value = entry.style;
        el.shape.value = entry.shape;
        repaint(entry.seed);
        window.scrollTo({ top: 0, behavior: 'smooth' });
      });

      var drop = document.createElement('span');
      drop.className = 'drop';
      drop.textContent = '✕';
      drop.title = 'Remove';
      drop.addEventListener('click', function (ev) {
        ev.stopPropagation();
        var list = load(STORE_KEY, []);
        list.splice(i, 1);
        save(STORE_KEY, list);
        renderGallery();
      });
      card.appendChild(drop);

      el.gallery.appendChild(card);
    });
  }

  /* ----------------------------------------------------------------- boot */

  function build() {
    LEX.STYLES.forEach(function (s) {
      var opt = document.createElement('option');
      opt.value = s.id;
      opt.textContent = s.label;
      el.style.appendChild(opt);
    });

    EXAMPLES.forEach(function (text) {
      var chip = document.createElement('button');
      chip.type = 'button';
      chip.className = 'chip';
      chip.textContent = text;
      chip.addEventListener('click', function () {
        el.prompt.value = text;
        repaint(1 + Math.floor(Math.random() * 9999));
      });
      el.examples.appendChild(chip);
    });
  }

  function start() {
    ['prompt', 'style', 'shape', 'examples', 'paint', 'reroll', 'surprise', 'canvas',
      'placeholder', 'busy', 'status', 'readout', 'outButtons', 'download', 'keep',
      'gallery', 'galleryWrap'].forEach(function (id) {
      el[id] = document.getElementById(id);
    });

    build();

    el.paint.addEventListener('click', function () { repaint(seed); });
    el.reroll.addEventListener('click', function () {
      repaint(1 + Math.floor(Math.random() * 999999));
    });
    el.surprise.addEventListener('click', function () {
      el.prompt.value = PROMPT.surprise(Date.now());
      repaint(1 + Math.floor(Math.random() * 999999));
    });
    el.download.addEventListener('click', download);
    el.keep.addEventListener('click', keep);

    /* Enter paints; shift+Enter still makes a new line. */
    el.prompt.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter' && !ev.shiftKey) {
        ev.preventDefault();
        repaint(seed);
      }
    });
    el.style.addEventListener('change', function () { if (current) repaint(seed); });
    el.shape.addEventListener('change', function () { if (current) repaint(seed); });

    var last = load(LAST_KEY, null);
    if (last && last.prompt) {
      el.prompt.value = last.prompt;
      if (last.style) el.style.value = last.style;
      if (last.shape) el.shape.value = last.shape;
      seed = last.seed || 1;
    }
    renderGallery();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', start);
  } else {
    start();
  }
})();
