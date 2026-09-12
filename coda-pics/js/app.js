/*
 * CODA PICS — the app.
 * --------------------
 * Wires the page to the picture: read the box, paint, show what was understood,
 * and let people keep, share and save what they like. Nothing here knows how a
 * dragon is drawn — that is js/subjects.js — and nothing here decides what the
 * words mean — that is js/prompt.js. This file is only the handle on the door.
 *
 * The gallery stores the *finished scene*, not the words that produced it. That
 * distinction matters: the scene a prompt resolves to depends on the vocabulary
 * the app had at the time, so a picture kept today and repainted after a new
 * subject is added would otherwise come back as a different picture. Keeping
 * the resolved scene is what makes "kept" mean kept.
 */
(function () {
  'use strict';

  var LEX = window.CodaLexicon;
  var PROMPT = window.CodaPrompt;
  var PAINT = window.CodaPaint;
  var FINISH = window.CodaFinish;

  var SIZES = {
    square:    { w: 1024, h: 1024 },
    wide:      { w: 1280, h: 720 },
    tall:      { w: 832,  h: 1216 },
    phone:     { w: 720,  h: 1280 },
    bigsquare: { w: 2048, h: 2048 },
    hd:        { w: 2560, h: 1440 },
    phonehd:   { w: 1440, h: 2560 }
  };

  var EXAMPLES = [
    'a red dragon over snowy mountains at sunset',
    'a lonely lighthouse in a storm, watercolour',
    'a cat under a tree at night',
    'a whale under a huge moon, woodblock',
    'neon city street in the rain',
    'an astronaut on a quiet desert planet',
    'a tiny cabin in falling snow, storybook',
    'a castle behind the mountains, film noir',
    'giant mushrooms in a glowing cave',
    'a hot air balloon over a canyon, poster'
  ];

  var STORE_KEY = 'codaPics.gallery.v2';
  var LEGACY_KEY = 'codaPics.gallery.v1';
  var LAST_KEY = 'codaPics.last.v1';
  var MAX_KEPT = 24;

  var el = {};
  var current = null;             // the spec on screen right now
  var seed = 1;
  var busy = false;
  var painted = 0;

  /* ------------------------------------------------------------- storage */
  function load(key, fallback) {
    try {
      var raw = window.localStorage.getItem(key);
      return raw ? JSON.parse(raw) : fallback;
    } catch (e) { return fallback; }
  }

  function save(key, value) {
    try { window.localStorage.setItem(key, JSON.stringify(value)); return true; }
    catch (e) { return false; }
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

  /* ------------------------------------------------------ the render worker
   * Big pictures are painted off the main thread so the page stays alive while
   * they are. Everything here is optional: no worker, no OffscreenCanvas, or a
   * worker that fails for any reason at all, and the app paints it itself.
   */
  var worker = null;
  var workerJobs = {};
  var workerJobId = 0;
  var workerBroken = false;

  function getWorker() {
    if (workerBroken) return null;
    /* A host that serves this page as one file has no worker script to load.
     * It says so rather than letting the request fail and be recovered from. */
    if (window.CODA_NO_WORKER) { workerBroken = true; return null; }
    if (worker) return worker;
    if (typeof Worker === 'undefined' || typeof OffscreenCanvas === 'undefined') {
      workerBroken = true;
      return null;
    }
    try {
      worker = new Worker('js/render-worker.js');
      worker.onmessage = function (ev) {
        var job = workerJobs[ev.data && ev.data.id];
        if (!job) return;
        delete workerJobs[ev.data.id];
        if (ev.data.error || !ev.data.bitmap) { job.fail(); return; }
        job.done(ev.data.bitmap);
      };
      worker.onerror = function () {
        workerBroken = true;
        Object.keys(workerJobs).forEach(function (k) {
          var job = workerJobs[k];
          delete workerJobs[k];
          job.fail();
        });
        worker = null;
      };
    } catch (e) {
      workerBroken = true;
      return null;
    }
    return worker;
  }

  /* Paint into the visible canvas: a quick small version first so there is
   * something to look at, then the real one, from the worker where possible. */
  function drawProgressive(canvas, spec, w, h, whenDone) {
    var ctx;
    canvas.width = w;
    canvas.height = h;
    ctx = canvas.getContext('2d');
    if (!ctx) { whenDone(false); return; }

    /* The preview is the same picture at a quarter of the size — seeded
     * identically, so it is a genuine preview and not a different roll. */
    var pw = Math.max(160, Math.round(w / 4));
    var ph = Math.max(90, Math.round(h / 4));
    try {
      var small = document.createElement('canvas');
      if (draw(small, spec, pw, ph)) {
        ctx.imageSmoothingEnabled = true;
        ctx.drawImage(small, 0, 0, w, h);
      }
    } catch (e) { /* the full render below is what matters */ }

    var w2 = getWorker();
    if (!w2) {
      var ok = false;
      try { ok = draw(canvas, spec, w, h); } catch (e) { ok = false; }
      whenDone(ok);
      return;
    }

    var id = ++workerJobId;
    workerJobs[id] = {
      done: function (bitmap) {
        try {
          canvas.getContext('2d').drawImage(bitmap, 0, 0);
          if (bitmap.close) bitmap.close();
          whenDone(true);
        } catch (e) { whenDone(false); }
      },
      fail: function () {
        var made = false;
        try { made = draw(canvas, spec, w, h); } catch (e) { made = false; }
        whenDone(made);
      }
    };
    try {
      w2.postMessage({ id: id, spec: spec, w: w, h: h });
    } catch (e) {
      delete workerJobs[id];
      var drawn = false;
      try { drawn = draw(canvas, spec, w, h); } catch (e2) { drawn = false; }
      whenDone(drawn);
    }
  }

  function locksNow() {
    return {
      subject: el.lockSubject.checked,
      sky: el.lockSky.checked,
      land: el.lockLand.checked
    };
  }

  function specFor(text, useSeed) {
    var locks = locksNow();
    var any = locks.subject || locks.sky || locks.land;
    return PROMPT.parse(text, {
      seed: useSeed,
      style: el.style.value,
      locked: (any && current) ? PROMPT.holdLocks(current.seed, locks) : null
    });
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
    el.placeholder.hidden = true;
    el.paint.disabled = el.reroll.disabled = true;

    var size = sizeOf();
    var spec = specFor(text, seed);

    void el.busy.offsetHeight;
    setTimeout(function () {
      drawProgressive(el.canvas, spec, size.w, size.h, function (ok) {
        busy = false;
        el.busy.hidden = true;
        el.paint.disabled = el.reroll.disabled = false;
        if (!ok) {
          setStatus('Something went wrong painting that one. Try another take.');
          el.placeholder.hidden = !!current;
          return;
        }
        current = spec;
        el.outButtons.hidden = false;
        showReadout(spec, size);
        save(LAST_KEY, { prompt: text, seed: seed, style: el.style.value, shape: el.shape.value });
        el.canvas.dataset.painted = String(++painted);
      });
    }, 20);
  }

  function setStatus(html) { el.status.innerHTML = html; }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  /* A sentence a screen reader can read out. The app knows exactly what it
   * drew, so there is no excuse for the picture being a blank to anyone. */
  function altTextFor(spec) {
    var parts = ['An illustration of ' + PROMPT.describe(spec).replace(' · ', ', in the style: ')];
    if (spec.unknown && spec.unknown.length) {
      parts.push('The words ' + spec.unknown.join(', ') + ' were not understood.');
    }
    return parts.join('. ');
  }

  function showReadout(spec, size) {
    setStatus('<b>' + escapeHtml(PROMPT.describe(spec)) + '</b> · ' +
      size.w + ' × ' + size.h + ' · seed ' + spec.seed);
    el.canvas.setAttribute('aria-label', altTextFor(spec));

    el.readout.innerHTML = '';
    spec.read.forEach(function (item) {
      var tag = document.createElement('span');
      tag.className = 'tag' + (item.word ? '' : ' guess');
      var from = '';
      if (item.word && item.meant) {
        from = ' <span>← “' + escapeHtml(item.word) + '”, read as ' + escapeHtml(item.meant) + '</span>';
      } else if (item.word) {
        from = ' <span>← “' + escapeHtml(item.word) + '”</span>';
      }
      tag.innerHTML = '<span>' + escapeHtml(item.category) + '</span> ' + escapeHtml(item.label) + from;
      el.readout.appendChild(tag);
    });

    if (spec.unknown && spec.unknown.length) {
      el.unknown.hidden = false;
      el.unknown.innerHTML = 'I don’t know ' +
        spec.unknown.map(function (w) { return '<b>' + escapeHtml(w) + '</b>'; }).join(', ') +
        ' — so ' + (spec.unknown.length > 1 ? 'they are' : 'it is') + ' not in the picture.';
    } else {
      el.unknown.hidden = true;
    }
  }

  /* -------------------------------------------------------------- sharing
   * A picture is its prompt, its seed, its style and its shape — four short
   * values, so a link to one is a link, not an upload. */
  function shareLink() {
    if (!current) return location.href;
    var p = [
      'p=' + encodeURIComponent(current.prompt),
      's=' + current.seed,
      'y=' + encodeURIComponent(el.style.value),
      'z=' + encodeURIComponent(el.shape.value)
    ].join('&');
    return location.origin + location.pathname + '#' + p;
  }

  function readLink() {
    var hash = String(location.hash || '').replace(/^#/, '');
    if (!hash) return null;
    var out = {};
    hash.split('&').forEach(function (pair) {
      var bits = pair.split('=');
      if (bits.length === 2) out[bits[0]] = decodeURIComponent(bits[1]);
    });
    if (!out.p) return null;
    return {
      prompt: out.p,
      seed: parseInt(out.s, 10) || 1,
      style: out.y || 'auto',
      shape: SIZES[out.z] ? out.z : 'wide'
    };
  }

  function copyShare() {
    var url = shareLink();
    var done = function () { setStatus('Link copied. It paints this exact picture.'); };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(url).then(done, function () { prompt('Copy this link:', url); });
    } else {
      window.prompt('Copy this link:', url);
    }
  }

  /* -------------------------------------------------------------- keeping */
  function slug(text) {
    return String(text).toLowerCase().replace(/[^a-z0-9]+/g, '-')
      .replace(/^-|-$/g, '').slice(0, 40) || 'picture';
  }

  /* Handing a file to the person looking at the page. A plain link is right in
   * a browser; a host that sandboxes downloads sets window.CODA_SAVE instead
   * and takes the blob itself. */
  function saveFile(blob, name, thenSay) {
    if (typeof window.CODA_SAVE === 'function') {
      window.CODA_SAVE(blob, name, thenSay);
      return;
    }
    var url = URL.createObjectURL(blob);
    linkTo(url, name);
    setTimeout(function () { URL.revokeObjectURL(url); }, 4000);
    if (thenSay) thenSay(true);
  }

  function linkTo(href, name) {
    var a = document.createElement('a');
    a.href = href;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }

  function download() {
    if (!current) return;
    var name = 'coda-pics-' + slug(current.prompt) + '-' + current.seed + '.png';
    if (el.canvas.toBlob) {
      el.canvas.toBlob(function (blob) {
        if (!blob) return;
        saveFile(blob, name, function (ok) {
          if (ok) setStatus('Saved as <b>' + escapeHtml(name) + '</b>');
        });
      }, 'image/png');
    } else {
      linkTo(el.canvas.toDataURL('image/png'), name);
    }
  }

  function keep() {
    if (!current) return;
    var kept = load(STORE_KEY, []);
    var entry = {
      prompt: current.prompt,
      seed: current.seed,
      style: el.style.value,
      shape: el.shape.value,
      spec: current,                 // the resolved scene, so it cannot drift
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

  /* An entry kept before the gallery stored scenes has only its words; read
   * them again rather than losing the picture. */
  function specOfEntry(entry) {
    if (entry.spec && entry.spec.scene) return entry.spec;
    return PROMPT.parse(entry.prompt, { seed: entry.seed, style: entry.style });
  }

  function galleryEntries() {
    var kept = load(STORE_KEY, null);
    if (kept) return kept;
    var old = load(LEGACY_KEY, []);      // carried over once, from the old format
    if (old.length) save(STORE_KEY, old);
    return old;
  }

  function renderGallery() {
    var kept = galleryEntries();
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
      try {
        draw(thumb, specOfEntry(entry), tw, th);
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
        var list = galleryEntries();
        list.splice(i, 1);
        save(STORE_KEY, list);
        renderGallery();
      });
      card.appendChild(drop);

      el.gallery.appendChild(card);
    });
  }

  function exportGallery() {
    var kept = galleryEntries();
    if (!kept.length) { setStatus('There is nothing in your gallery yet.'); return; }
    var blob = new Blob([JSON.stringify({ app: 'coda-pics', version: 2, kept: kept }, null, 2)],
      { type: 'application/json' });
    saveFile(blob, 'coda-pics-gallery.json', function (ok) {
      if (ok) setStatus('Gallery saved. That file will bring these back on any device.');
    });
  }

  function importGallery(file) {
    if (!file) return;
    var reader = new FileReader();
    reader.onload = function () {
      var data = null;
      try { data = JSON.parse(String(reader.result)); } catch (e) { /* reported below */ }
      if (!data || !Array.isArray(data.kept)) {
        setStatus('That file is not a CODA PICS gallery.');
        return;
      }
      var merged = galleryEntries().concat(data.kept.filter(function (k) {
        return k && typeof k.prompt === 'string';
      }));
      var seen = {}, unique = [];
      merged.forEach(function (k) {
        var key = k.prompt + '|' + k.seed + '|' + k.style;
        if (seen[key]) return;
        seen[key] = true;
        unique.push(k);
      });
      if (unique.length > MAX_KEPT) unique.length = MAX_KEPT;
      save(STORE_KEY, unique);
      renderGallery();
      setStatus('Loaded ' + data.kept.length + ' from that file.');
    };
    reader.readAsText(file);
  }

  /* ------------------------------------------------------------ six takes */
  function showSix() {
    var text = el.prompt.value.trim();
    if (!text) { el.prompt.focus(); return; }
    el.sheet.hidden = false;
    el.sheetGrid.innerHTML = '';
    var size = sizeOf();
    var tw = 240, th = Math.round(tw * size.h / size.w);

    for (var i = 0; i < 6; i++) {
      (function (n) {
        var s = 1 + Math.floor(Math.random() * 999999);
        var spec = specFor(text, s);
        var card = document.createElement('button');
        card.className = 'card';
        card.type = 'button';
        card.title = 'Paint this one full size';
        var c = document.createElement('canvas');
        try { draw(c, spec, tw, th); } catch (e) { /* skip this one */ }
        card.appendChild(c);
        var label = document.createElement('span');
        label.className = 'label';
        label.textContent = 'seed ' + s;
        card.appendChild(label);
        card.addEventListener('click', function () {
          repaint(s);
          window.scrollTo({ top: 0, behavior: 'smooth' });
        });
        el.sheetGrid.appendChild(card);
        void n;
      })(i);
    }
    setStatus('Six takes on the same words. Tap one to paint it full size.');
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
    ['prompt', 'style', 'shape', 'examples', 'paint', 'reroll', 'six', 'surprise',
      'canvas', 'placeholder', 'busy', 'status', 'readout', 'unknown', 'outButtons',
      'download', 'keep', 'share', 'sheet', 'sheetGrid', 'gallery', 'galleryWrap',
      'exportGallery', 'importGallery', 'importFile',
      'lockSubject', 'lockSky', 'lockLand'].forEach(function (id) {
      el[id] = document.getElementById(id);
    });

    build();

    el.paint.addEventListener('click', function () { repaint(seed); });
    el.reroll.addEventListener('click', function () {
      repaint(1 + Math.floor(Math.random() * 999999));
    });
    el.six.addEventListener('click', showSix);
    el.surprise.addEventListener('click', function () {
      el.prompt.value = PROMPT.surprise(Date.now());
      repaint(1 + Math.floor(Math.random() * 999999));
    });
    el.download.addEventListener('click', download);
    el.keep.addEventListener('click', keep);
    el.share.addEventListener('click', copyShare);
    el.exportGallery.addEventListener('click', exportGallery);
    el.importGallery.addEventListener('click', function () { el.importFile.click(); });
    el.importFile.addEventListener('change', function () {
      importGallery(el.importFile.files && el.importFile.files[0]);
      el.importFile.value = '';
    });

    el.prompt.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter' && !ev.shiftKey) {
        ev.preventDefault();
        repaint(seed);
      }
    });
    el.style.addEventListener('change', function () { if (current) repaint(seed); });
    el.shape.addEventListener('change', function () { if (current) repaint(seed); });

    /* A shared link wins over whatever this browser was last doing. */
    var shared = readLink();
    if (shared) {
      el.prompt.value = shared.prompt;
      el.style.value = shared.style;
      el.shape.value = shared.shape;
      renderGallery();
      repaint(shared.seed);
      return;
    }

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
