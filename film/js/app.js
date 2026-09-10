/*
 * SCRIPT FORGE — the interface.
 * -----------------------------
 * Wires the box you type in to the reader (parse.js), the writer
 * (screenplay.js) and the exporters (format.js), and keeps a small library of
 * scripts in this browser's local storage.
 *
 * Saved scripts store the *idea and its settings*, not the pages, so opening
 * one rebuilds it exactly — the seed is what makes that possible.
 */
(function () {
  'use strict';

  var LEX = window.FILM_LEXICON;
  var Parse = window.FilmParse;
  var Writer = window.FilmWriter;
  var Format = window.FilmFormat;

  var LIB_KEY = 'scriptforge.library.v1';

  var EXAMPLES = [
    "A lonely lighthouse keeper finds a radio that plays tomorrow's news.",
    'Two sisters rob their own family diner at midnight.',
    'A kid hears his dead brother on a walkie-talkie in the attic.',
    'A woman named Ada visits her father in the hospital and cannot say goodbye.',
    'A courier discovers the package is addressed to her, from herself.',
    'The last human on the station teaches the ship to lie.'
  ];

  var el = {};
  ['idea', 'examples', 'length', 'genre', 'titleInput', 'write', 'reroll', 'placeholder',
   'result', 'scriptTitle', 'scriptLogline', 'chipGenre', 'chipScenes', 'chipRuntime',
   'chipSeed', 'tabScript', 'tabShots', 'viewScript', 'viewShots', 'copy', 'dlFountain',
   'dlFdx', 'dlText', 'dlShots', 'print', 'save', 'status', 'libraryList', 'libCount',
   'libEmpty', 'clearLib'].forEach(function (id) {
    el[id] = document.getElementById(id);
  });

  var current = null; // the script on screen

  /* ------------------------------------------------------------------ setup */
  function buildGenreOptions() {
    Object.keys(LEX.GENRES).forEach(function (key) {
      var opt = document.createElement('option');
      opt.value = key;
      opt.textContent = LEX.GENRES[key].label;
      el.genre.appendChild(opt);
    });
  }

  function buildExamples() {
    EXAMPLES.forEach(function (text) {
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'example';
      b.textContent = text.length > 46 ? text.slice(0, 44) + '…' : text;
      b.title = text;
      b.addEventListener('click', function () {
        el.idea.value = text;
        el.idea.focus();
        generate({ fresh: true });
      });
      el.examples.appendChild(b);
    });
  }

  function say(message) {
    el.status.textContent = message || '';
    if (message) {
      window.clearTimeout(say._t);
      say._t = window.setTimeout(function () { el.status.textContent = ''; }, 4000);
    }
  }

  /* --------------------------------------------------------------- generate */
  function generate(opts) {
    opts = opts || {};
    var idea = el.idea.value.trim();
    if (!idea) {
      say('Type what your film is about first — one sentence is plenty.');
      el.idea.focus();
      return;
    }

    var seed = opts.seed;
    if (typeof seed !== 'number') {
      // A fresh idea is seeded from its own words, so the same sentence always
      // gives the same film. "Another take" rolls a new one on purpose.
      seed = opts.fresh ? Parse.hashText(idea) : (Math.random() * 4294967296) >>> 0;
    }

    var premise = Parse.parse(idea, {
      genre: el.genre.value,
      seed: seed,
      title: el.titleInput.value.trim() || null
    });
    current = Writer.write(premise, { length: el.length.value, seed: seed });
    render(current);
  }

  /* ----------------------------------------------------------------- render */
  function render(script) {
    el.placeholder.classList.add('hidden');
    el.result.classList.remove('hidden');

    el.scriptTitle.textContent = script.title;
    el.scriptLogline.textContent = script.logline;
    el.chipGenre.textContent = script.genreLabel + (script.premise.genreAuto ? ' (auto)' : '');
    el.chipScenes.textContent = script.scenes.length + ' scenes';
    el.chipRuntime.textContent = script.runtime + ' · ' + script.pages + ' pages';
    el.chipSeed.textContent = 'seed ' + script.seed;

    renderPage(script);
    renderShots(script);
    el.result.scrollIntoView({ behavior: 'smooth', block: 'start' });
  }

  function renderPage(script) {
    var page = el.viewScript;
    page.textContent = '';
    var sceneNo = 0;

    script.elements.forEach(function (element) {
      var node = document.createElement('p');
      node.className = 'el ' + element.type;
      if (element.type === 'scene_heading') {
        sceneNo++;
        var num = document.createElement('span');
        num.className = 'scene-num';
        num.textContent = String(sceneNo);
        node.appendChild(num);
      }
      if (element.type === 'parenthetical') {
        node.appendChild(document.createTextNode('(' + element.text.replace(/^\(|\)$/g, '') + ')'));
      } else {
        node.appendChild(document.createTextNode(element.text));
      }
      page.appendChild(node);
    });

    var end = document.createElement('p');
    end.className = 'el the-end';
    end.textContent = 'THE END';
    page.appendChild(end);
  }

  function renderShots(script) {
    var host = el.viewShots;
    host.textContent = '';

    var meta = document.createElement('p');
    meta.className = 'meta';
    meta.textContent = script.characters.map(function (c) { return c.name + ' — ' + c.role; }).join(' · ') +
      ' | ' + Format.uniqueLocations(script).join(' · ');
    host.appendChild(meta);

    script.scenes.forEach(function (scene) {
      var h = document.createElement('h3');
      h.textContent = scene.number + '. ' + scene.heading.text;
      host.appendChild(h);

      var purpose = document.createElement('p');
      purpose.className = 'purpose';
      purpose.textContent = scene.beat.name + ' — ' + scene.beat.purpose;
      host.appendChild(purpose);

      var list = document.createElement('ul');
      scene.shots.forEach(function (shot, i) {
        var li = document.createElement('li');
        li.textContent = scene.number + String.fromCharCode(65 + i) + '  ' + shot;
        list.appendChild(li);
      });
      host.appendChild(list);
    });
  }

  function showTab(which) {
    var onScript = which === 'script';
    el.tabScript.classList.toggle('is-on', onScript);
    el.tabShots.classList.toggle('is-on', !onScript);
    el.tabScript.setAttribute('aria-selected', String(onScript));
    el.tabShots.setAttribute('aria-selected', String(!onScript));
    el.viewScript.classList.toggle('hidden', !onScript);
    el.viewShots.classList.toggle('hidden', onScript);
  }

  /* ---------------------------------------------------------------- exports */
  function download(filename, text, mime) {
    var blob = new Blob([text], { type: (mime || 'text/plain') + ';charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    window.setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
    say('Saved ' + filename);
  }

  function copyText(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () { say('Script copied to the clipboard.'); },
        function () { fallbackCopy(text); }
      );
    } else {
      fallbackCopy(text);
    }
  }

  function fallbackCopy(text) {
    var ta = document.createElement('textarea');
    ta.value = text;
    ta.setAttribute('readonly', '');
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    try {
      document.execCommand('copy');
      say('Script copied to the clipboard.');
    } catch (e) {
      say('Could not copy — use one of the download buttons instead.');
    }
    document.body.removeChild(ta);
  }

  /* ---------------------------------------------------------------- library */
  function readLibrary() {
    try {
      var raw = window.localStorage.getItem(LIB_KEY);
      var list = raw ? JSON.parse(raw) : [];
      return Object.prototype.toString.call(list) === '[object Array]' ? list : [];
    } catch (e) {
      return [];
    }
  }

  function writeLibrary(list) {
    try {
      window.localStorage.setItem(LIB_KEY, JSON.stringify(list));
    } catch (e) {
      say('This browser will not let the app save locally.');
    }
  }

  function saveCurrent() {
    if (!current) return;
    var list = readLibrary();
    list.unshift({
      title: current.title,
      idea: current.idea,
      genre: current.premise.genreAuto ? 'auto' : current.genre,
      length: current.length,
      seed: current.seed,
      runtime: current.runtime,
      savedAt: Date.now()
    });
    writeLibrary(list.slice(0, 50));
    renderLibrary();
    say('Saved to your library on this device.');
  }

  function renderLibrary() {
    var list = readLibrary();
    el.libCount.textContent = String(list.length);
    el.libEmpty.classList.toggle('hidden', list.length > 0);
    el.libraryList.textContent = '';

    list.forEach(function (entry, index) {
      var li = document.createElement('li');

      var main = document.createElement('div');
      var t = document.createElement('div');
      t.className = 'lib-title';
      t.textContent = entry.title;
      var sub = document.createElement('div');
      sub.className = 'lib-sub';
      sub.textContent = (entry.runtime || '') + ' · ' + (entry.idea || '').slice(0, 70);
      main.appendChild(t);
      main.appendChild(sub);

      var actions = document.createElement('div');
      actions.className = 'lib-actions';

      var open = document.createElement('button');
      open.type = 'button';
      open.textContent = 'Open';
      open.addEventListener('click', function () {
        el.idea.value = entry.idea;
        el.length.value = entry.length;
        el.genre.value = entry.genre;
        el.titleInput.value = entry.title;
        generate({ seed: entry.seed });
      });

      var remove = document.createElement('button');
      remove.type = 'button';
      remove.textContent = 'Delete';
      remove.addEventListener('click', function () {
        var next = readLibrary();
        next.splice(index, 1);
        writeLibrary(next);
        renderLibrary();
      });

      actions.appendChild(open);
      actions.appendChild(remove);
      li.appendChild(main);
      li.appendChild(actions);
      el.libraryList.appendChild(li);
    });
  }

  /* ------------------------------------------------------------------ wiring */
  el.write.addEventListener('click', function () { generate({ fresh: true }); });
  el.reroll.addEventListener('click', function () { generate({}); });

  el.idea.addEventListener('keydown', function (e) {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      generate({ fresh: true });
    }
  });

  el.tabScript.addEventListener('click', function () { showTab('script'); });
  el.tabShots.addEventListener('click', function () { showTab('shots'); });

  el.copy.addEventListener('click', function () {
    if (current) copyText(Format.toText(current));
  });
  el.dlFountain.addEventListener('click', function () {
    if (current) download(Format.slugify(current.title) + '.fountain', Format.toFountain(current));
  });
  el.dlFdx.addEventListener('click', function () {
    if (current) download(Format.slugify(current.title) + '.fdx', Format.toFdx(current), 'application/xml');
  });
  el.dlText.addEventListener('click', function () {
    if (current) download(Format.slugify(current.title) + '.txt', Format.toText(current));
  });
  el.dlShots.addEventListener('click', function () {
    if (current) download(Format.slugify(current.title) + '-shot-list.md', Format.toShotList(current), 'text/markdown');
  });
  el.print.addEventListener('click', function () {
    if (current) window.print();
  });
  el.save.addEventListener('click', saveCurrent);
  el.clearLib.addEventListener('click', function () {
    writeLibrary([]);
    renderLibrary();
    say('Library cleared.');
  });

  buildGenreOptions();
  buildExamples();
  renderLibrary();
})();
