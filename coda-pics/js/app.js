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
  var PHOTO = window.CodaPhoto;
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

  var PALETTE_KEY = 'codaPics.palettes.v1';
  var STORE_KEY = 'codaPics.gallery.v2';
  var LEGACY_KEY = 'codaPics.gallery.v1';
  var LAST_KEY = 'codaPics.last.v1';
  var MAX_KEPT = 24;

  var el = {};
  var current = null;             // the spec on screen right now
  var photo = null;               // { image, analysis } — never leaves this page
  var library = [];               // palettes kept from photos: numbers, not pixels
  var chosenPalette = 'latest';   // 'latest' | 'mixture' | an index into library
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

  function draw(canvas, spec, w, h, media) {
    canvas.width = w;
    canvas.height = h;
    var ctx = canvas.getContext('2d');
    if (!ctx) return false;
    var palette = PAINT.render(ctx, w, h, spec, media);
    FINISH.apply(ctx, w, h, spec, palette);
    return true;
  }

  /* The photo itself, for the passes that paint onto it. Kept out of the spec
   * because an image is not data a gallery entry or a worker message can
   * carry. */
  function mediaNow(spec) {
    if (!photo || !spec.photo || !spec.photo.use.backdrop) return null;
    return { backdrop: photo.image };
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
    var media = mediaNow(spec);
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
      if (draw(small, spec, pw, ph, media)) {
        ctx.imageSmoothingEnabled = true;
        ctx.drawImage(small, 0, 0, w, h);
      }
    } catch (e) { /* the full render below is what matters */ }

    /* A picture painted onto a photo stays on this thread: the photo is an
     * image, and shipping one into a worker costs more than the paint saves. */
    var w2 = media ? null : getWorker();
    if (!w2) {
      var ok = false;
      try { ok = draw(canvas, spec, w, h, media); } catch (e) { ok = false; }
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
        try { made = draw(canvas, spec, w, h, media); } catch (e) { made = false; }
        whenDone(made);
      }
    };
    try {
      w2.postMessage({ id: id, spec: spec, w: w, h: h });
    } catch (e) {
      delete workerJobs[id];
      var drawn = false;
      try { drawn = draw(canvas, spec, w, h, media); } catch (e2) { drawn = false; }
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

  /*
   * Read a photo the person chose. Every step happens in this page: the file is
   * read by the browser, drawn to a canvas, and measured. Nothing is uploaded,
   * and nothing is kept once the page is closed unless they save a picture.
   */
  /* Several photographs at once: each is measured, each leaves a palette
   * behind, and the mixture of all of them becomes a palette of its own. */
  function loadPhotos(files) {
    if (!files || !files.length) return;
    var list = Array.prototype.slice.call(files);
    var done = 0;
    setStatus('Reading ' + list.length + ' photo' + (list.length > 1 ? 's' : '') + '…');
    list.forEach(function (file) {
      loadPhoto(file, function () {
        done++;
        if (done === list.length) {
          chosenPalette = list.length > 1 ? 'mixture' : 'latest';
          renderPalettes();
          setStatus(list.length > 1
            ? 'Kept the colours of ' + list.length + ' photos. <b>Mixture</b> paints in all of them at once.'
            : 'Kept the colours of that photo.');
          if (el.prompt.value.trim()) repaint(seed);
        }
      });
    });
  }

  function loadPhoto(file, whenDone) {
    if (!file || !PHOTO) { if (whenDone) whenDone(); return; }
    var reader = new FileReader();
    reader.onerror = function () {
      setStatus('That file could not be read.');
      if (whenDone) whenDone();
    };
    reader.onload = function () {
      var img = new Image();
      img.onerror = function () {
        setStatus('That does not look like an image.');
        if (whenDone) whenDone();
      };
      img.onload = function () {
        /* Measured at a modest size: the analysis wants colours and a horizon,
         * not detail, and a 12-megapixel phone photo would be wasted work. */
        var aw = 320;
        var ah = Math.max(1, Math.round(aw * img.height / img.width));
        var work = document.createElement('canvas');
        work.width = aw; work.height = ah;
        var wctx = work.getContext('2d');
        if (!wctx) { if (whenDone) whenDone(); return; }
        wctx.drawImage(img, 0, 0, aw, ah);
        var data;
        try { data = wctx.getImageData(0, 0, aw, ah); } catch (e) {
          if (whenDone) whenDone();
          return;
        }

        var analysis = PHOTO.analyse(data, aw, ah);
        photo = { image: img, analysis: analysis };
        rememberPalette(file.name || 'a photo', analysis);
        showPhoto();
        if (whenDone) { whenDone(); return; }
        renderPalettes();
        if (current) repaint(seed);
      };
      img.src = String(reader.result);
    };
    reader.readAsDataURL(file);
  }

  /*
   * Keep what a photograph gave, and nothing else. A palette is about twenty
   * numbers; the photograph is megabytes and is not ours to store. This is why
   * the app can still paint in the colours of your summer next month without
   * ever having held a photo of it.
   */
  function rememberPalette(name, analysis) {
    library = library.filter(function (p) { return p.name !== name; });
    library.unshift({ name: String(name).slice(0, 40), palette: analysis.palette, at: Date.now() });
    if (library.length > 12) library.length = 12;
    save(PALETTE_KEY, library);
  }

  function paletteSwatch(palette, w, h) {
    var c = document.createElement('canvas');
    c.width = w; c.height = h;
    var ctx = c.getContext('2d');
    if (!ctx) return c;
    function css(hsl) {
      return 'hsl(' + hsl[0].toFixed(1) + ',' + hsl[1].toFixed(1) + '%,' + hsl[2].toFixed(1) + '%)';
    }
    var g = ctx.createLinearGradient(0, 0, 0, h);
    g.addColorStop(0, css(palette.sky.top));
    g.addColorStop(0.5, css(palette.sky.mid));
    g.addColorStop(1, css(palette.sky.low));
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = css(palette.scene.land);
    ctx.fillRect(0, h * 0.62, w, h * 0.38);
    ctx.fillStyle = css(palette.scene.ink);
    ctx.fillRect(0, h * 0.82, w, h * 0.18);
    return c;
  }

  function mixedPalette() {
    if (library.length < 2 || !PHOTO.mix) return null;
    return PHOTO.mix(library.map(function (p) { return { palette: p.palette }; }));
  }

  function activePalette() {
    if (chosenPalette === 'mixture') return mixedPalette();
    if (chosenPalette === 'latest') return photo ? photo.analysis.palette : (library[0] && library[0].palette);
    var entry = library[chosenPalette];
    return entry ? entry.palette : null;
  }

  function renderPalettes() {
    el.paletteBox.hidden = library.length === 0;
    el.paletteChips.innerHTML = '';
    if (!library.length) return;

    function chip(key, label, palette, isMix) {
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'swatch' + (isMix ? ' mixture' : '');
      b.setAttribute('aria-pressed', String(chosenPalette === key));
      b.appendChild(paletteSwatch(palette, 34, 16));
      var span = document.createElement('span');
      span.textContent = label;
      b.appendChild(span);
      b.addEventListener('click', function () {
        chosenPalette = key;
        el.usePhotoColours.checked = true;
        renderPalettes();
        if (current) repaint(seed);
      });
      el.paletteChips.appendChild(b);
    }

    var mixed = mixedPalette();
    if (mixed) chip('mixture', 'Mixture of ' + library.length, mixed, true);
    library.forEach(function (entry, i) {
      chip(i, entry.name.replace(/\.[a-z0-9]+$/i, ''), entry.palette, false);
    });
  }

  function forgetPalettes() {
    library = [];
    chosenPalette = 'latest';
    save(PALETTE_KEY, library);
    renderPalettes();
    setStatus('All of those colours are forgotten. The photos were never here to forget.');
    if (current) repaint(seed);
  }

  function showPhoto() {
    if (!photo) { el.photoInfo.hidden = true; return; }
    el.photoInfo.hidden = false;

    var tw = 132;
    var th = Math.max(1, Math.round(tw * photo.image.height / photo.image.width));
    el.photoThumb.width = tw;
    el.photoThumb.height = th;
    var tctx = el.photoThumb.getContext('2d');
    if (tctx) tctx.drawImage(photo.image, 0, 0, tw, th);

    var sky = photo.analysis.skyline;
    if (sky.confidence < 0.35) {
      el.usePhotoSkyline.checked = false;
      el.usePhotoSkyline.disabled = true;
      el.photoNote.textContent =
        'No clear horizon in this one, so it can lend its colours but not its skyline.';
    } else {
      el.usePhotoSkyline.disabled = false;
      el.photoNote.textContent = 'Horizon found ' +
        Math.round(sky.mean * 100) + '% down, and the light is coming from the ' +
        (photo.analysis.light.x < 0.4 ? 'left' : photo.analysis.light.x > 0.6 ? 'right' : 'middle') + '.';
    }
  }

  function forgetPhoto() {
    photo = null;
    el.photoInfo.hidden = true;
    el.photoFile.value = '';
    setStatus('Photo forgotten. It was never anywhere but this page.');
    if (current) repaint(seed);
  }

  function photoUse() {
    if (!photo) return null;
    return {
      colours: el.usePhotoColours.checked,
      skyline: el.usePhotoSkyline.checked && !el.usePhotoSkyline.disabled,
      backdrop: el.usePhotoBackdrop.checked
    };
  }

  /* What the painter is told about the photo: plain measured numbers, so it
   * travels into the render worker and into a kept gallery entry unchanged. */
  function photoSpec() {
    /* Colours can come from the library with no photo loaded at all — that is
     * the point of keeping them. The horizon and the backdrop need the actual
     * photograph, so they are only offered while one is in hand. */
    var pal = el.usePhotoColours && el.usePhotoColours.checked ? activePalette() : null;
    var use = photoUse() || { colours: !!pal, skyline: false, backdrop: false };
    if (!pal) use.colours = false;
    if (!use.colours && !use.skyline && !use.backdrop) return null;
    return {
      use: use,
      palette: pal || (photo && photo.analysis.palette),
      skyline: photo ? photo.analysis.skyline : null,
      light: photo ? photo.analysis.light : null
    };
  }

  function specFor(text, useSeed) {
    var locks = locksNow();
    var any = locks.subject || locks.sky || locks.land;
    var spec = PROMPT.parse(text, {
      seed: useSeed,
      style: el.style.value,
      locked: (any && current) ? PROMPT.holdLocks(current.seed, locks) : null
    });
    var ph = photoSpec();
    if (ph) spec.photo = ph;
    return spec;
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
    if (current.photo && current.photo.use && current.photo.use.backdrop) {
      setStatus('This one is painted onto your photo, and the gallery stores scenes ' +
        'rather than photographs — so it could not bring this back as it is. ' +
        'Use <b>Save the picture</b> instead.');
      return;
    }
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
        var espec = specOfEntry(entry);
        draw(thumb, espec, tw, th, mediaNow(espec));
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

  /*
   * The photograph, put through one of the app's own styles. Nothing is drawn
   * on top: the finishing passes work on whatever pixels they are given, and a
   * photograph is pixels. This is the shortest path from "a photo you took" to
   * "a woodblock print of a photo you took".
   */
  function stylePhotoNow() {
    if (!photo) return;
    var size = sizeOf();
    el.canvas.width = size.w;
    el.canvas.height = size.h;
    var ctx = el.canvas.getContext('2d');
    if (!ctx) return;

    var spec = specFor(el.prompt.value.trim() || 'a photograph', seed);
    PAINT.coverDraw(ctx, photo.image, size.w, size.h);
    try {
      FINISH.apply(ctx, size.w, size.h, spec, PAINT.makePalette(spec));
    } catch (e) {
      setStatus('That style could not be applied to this photo.');
      return;
    }

    current = spec;
    el.placeholder.hidden = true;
    el.outButtons.hidden = false;
    el.canvas.dataset.painted = String(++painted);
    el.canvas.setAttribute('aria-label', 'Your photograph, in the ' +
      styleName(spec) + ' style.');
    el.readout.innerHTML = '';
    el.unknown.hidden = true;
    setStatus('<b>Your photo, in ' + escapeHtml(styleName(spec)) + '</b> · ' +
      size.w + ' × ' + size.h + ' — change the art style and press this again.');
  }

  function styleName(spec) {
    var ids = spec.styles && spec.styles.length ? spec.styles : [spec.style];
    return ids.map(function (id) {
      for (var i = 0; i < LEX.STYLES.length; i++) {
        if (LEX.STYLES[i].id === id) return LEX.STYLES[i].label;
      }
      return id;
    }).join(' + ');
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
        try { draw(c, spec, tw, th, mediaNow(spec)); } catch (e) { /* skip this one */ }
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

  /* -------------------------------------------------------- google photos
   * The only part of CODA PICS that touches a network, and it is opt-in,
   * folded away, and works exactly like choosing a file once the photos land:
   * measured here, colours kept, pixels forgotten.
   *
   * There is no client ID baked into this repo on purpose. A client ID is tied
   * to one Google project and one set of allowed origins, so a shared one would
   * either not work for anybody else or hand strangers a project. The person
   * makes their own, once, and this page keeps it in their browser.
   */
  var GP_KEY = 'codaPics.gphotos.clientId';
  var gp = null;               // the live connector, once connected
  var gpPickerWindow = null;   // opened on the click, filled in when we know where

  function gpRedirectUri() {
    return window.location.origin + window.location.pathname;
  }

  /* Kept for the length of the redirect and no longer. sessionStorage, not
   * localStorage: the verifier is a secret for one sign-in, not a setting. */
  var gpStore = {
    get: function (k) { try { return window.sessionStorage.getItem(k); } catch (e) { return null; } },
    set: function (k, v) { try { window.sessionStorage.setItem(k, v); } catch (e) {} },
    remove: function (k) { try { window.sessionStorage.removeItem(k); } catch (e) {} }
  };

  function gpConnector(clientId) {
    if (!window.CodaGPhotos) return null;
    return new window.CodaGPhotos.Connector({
      /* Both of these are replaceable so the whole flow can be driven in a
       * test without Google, and without a real window ever moving. */
      fetch: window.CODA_GPHOTOS_TRANSPORT || function (url, opts) {
        return window.fetch(url, opts);
      },
      go: window.CODA_GPHOTOS_GO || function (url) { window.location.href = url; },
      store: gpStore,
      clientId: clientId,
      redirectUri: gpRedirectUri()
    });
  }

  function gpSay(msg) { if (el.gpNote) el.gpNote.textContent = msg; }

  function gpShowState() {
    var connected = !!(gp && gp.token);
    el.gpPick.hidden = !connected;
    el.gpForget.hidden = !connected;
    el.gpConnect.hidden = connected;
  }

  function gpConnect() {
    var id = String(el.gpClientId.value || '').trim();
    if (!id) {
      gpSay('Paste your Google client ID above first — the README shows where to get one.');
      el.gpClientId.focus();
      return;
    }
    if (!window.CodaGPhotos) { gpSay('The connector did not load. Reload the page.'); return; }
    save(GP_KEY, id);
    gp = gpConnector(id);
    gpSay('Sending you to Google to say yes…');
    gp.beginSignIn().catch(function (err) {
      gpSay(err && err.message ? err.message : 'That sign-in could not be started.');
    });
  }

  /* Back from Google. The code in the address bar is single-use and must not
   * survive in history, so it is swapped and then wiped from the URL. */
  function gpFinishSignIn(code, state) {
    var id = load(GP_KEY, '') || '';
    gp = gpConnector(id);
    if (!gp) return;
    gpSay('Finishing the sign-in…');
    gp.completeSignIn(code, state).then(function () {
      gpCleanUrl();
      gpShowState();
      gpSay('Connected. Choose photos whenever you like.');
      if (el.gphotos) el.gphotos.open = true;
    }).catch(function (err) {
      gpCleanUrl();
      gp = null;
      gpShowState();
      gpSay(err && err.message ? err.message : 'That sign-in did not finish.');
      if (el.gphotos) el.gphotos.open = true;
    });
  }

  function gpCleanUrl() {
    try {
      window.history.replaceState({}, '', gpRedirectUri() + window.location.hash);
    } catch (e) {}
  }

  function gpPick() {
    if (!gp || !gp.token) { gpSay('Connect first.'); return; }
    /* Opened on the click itself, before anything is awaited: a window opened
     * after a network round-trip is a popup, and gets blocked. */
    gpPickerWindow = null;
    if (!window.CODA_GPHOTOS_GO) {
      try { gpPickerWindow = window.open('', '_blank'); } catch (e) { gpPickerWindow = null; }
    }
    gpSay('Opening your gallery — choose the photos you want, then come back here.');
    el.gpPick.disabled = true;

    gp.pick({
      limit: 12,
      size: 640,
      onPicker: function (uri) {
        if (gpPickerWindow) { try { gpPickerWindow.location.href = uri; } catch (e) {} }
        else if (window.CODA_GPHOTOS_GO) window.CODA_GPHOTOS_GO(uri);
        else window.open(uri, '_blank');
      }
    }).then(function (picked) {
      el.gpPick.disabled = false;
      if (gpPickerWindow) { try { gpPickerWindow.close(); } catch (e) {} }
      gpSay('Brought in ' + picked.length + ' photo' + (picked.length > 1 ? 's' : '') + '.');
      /* From here they are ordinary photos: the same reader, the same
       * measurement, the same palettes. Nothing about the rest of the app
       * knows or cares that these came from Google. */
      loadPhotos(picked.map(function (item) {
        try { return new File([item.blob], item.name, { type: item.blob.type }); }
        catch (e) { item.blob.name = item.name; return item.blob; }
      }));
    }).catch(function (err) {
      el.gpPick.disabled = false;
      if (gpPickerWindow) { try { gpPickerWindow.close(); } catch (e) {} }
      gpSay(err && err.message ? err.message : 'Nothing came back.');
    });
  }

  function gpForget() {
    gp = null;
    gpStore.remove('gphotos.verifier');
    gpStore.remove('gphotos.state');
    gpShowState();
    gpSay('Disconnected. The colours you already kept are still here.');
  }

  function gpStart() {
    if (!el.gpConnect) return;
    el.gpRedirect.textContent = gpRedirectUri();
    el.gpClientId.value = load(GP_KEY, '') || '';
    el.gpConnect.addEventListener('click', gpConnect);
    el.gpPick.addEventListener('click', gpPick);
    el.gpForget.addEventListener('click', gpForget);
    gpShowState();

    var q = new URLSearchParams(window.location.search);
    if (q.get('error')) {
      gpCleanUrl();
      if (el.gphotos) el.gphotos.open = true;
      gpSay('Google said no: ' + q.get('error') + '.');
      return;
    }
    if (q.get('code') && q.get('state')) {
      if (el.gphotos) el.gphotos.open = true;
      gpFinishSignIn(q.get('code'), q.get('state'));
    }
  }

  function start() {
    ['prompt', 'style', 'shape', 'examples', 'paint', 'reroll', 'six', 'surprise',
      'canvas', 'placeholder', 'busy', 'status', 'readout', 'unknown', 'outButtons',
      'download', 'keep', 'share', 'sheet', 'sheetGrid', 'gallery', 'galleryWrap',
      'exportGallery', 'importGallery', 'importFile',
      'lockSubject', 'lockSky', 'lockLand',
      'photoFile', 'photoInfo', 'photoThumb', 'photoNote', 'stylePhoto', 'clearPhoto',
      'usePhotoColours', 'usePhotoSkyline', 'usePhotoBackdrop',
      'paletteBox', 'paletteChips', 'clearPalettes',
      'gphotos', 'gpClientId', 'gpRedirect', 'gpConnect', 'gpPick', 'gpForget',
      'gpNote'].forEach(function (id) {
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

    el.photoFile.addEventListener('change', function () {
      loadPhotos(el.photoFile.files);
    });
    el.clearPalettes.addEventListener('click', forgetPalettes);
    el.clearPhoto.addEventListener('click', forgetPhoto);
    el.stylePhoto.addEventListener('click', stylePhotoNow);
    [el.usePhotoColours, el.usePhotoSkyline, el.usePhotoBackdrop].forEach(function (box) {
      box.addEventListener('change', function () { if (current) repaint(seed); });
    });

    el.prompt.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter' && !ev.shiftKey) {
        ev.preventDefault();
        repaint(seed);
      }
    });
    el.style.addEventListener('change', function () { if (current) repaint(seed); });
    el.shape.addEventListener('change', function () { if (current) repaint(seed); });

    /* The colours kept from photos come back before anything is painted, on
     * every path through this function — a shared link included. */
    library = load(PALETTE_KEY, []) || [];
    renderPalettes();
    gpStart();

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
