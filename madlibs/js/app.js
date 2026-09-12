/*
 * MadLibs Story Forge — UI layer
 * ------------------------------
 * Wires the DOM to window.MadlibsGenerator. All state (the saved library) lives
 * in localStorage, so nothing leaves the browser.
 */
(function () {
  'use strict';

  var G = window.MadlibsGenerator;
  var LIB_KEY = 'madlibs.library.v1';

  var el = {
    genre: document.getElementById('genre'),
    generate: document.getElementById('generate'),
    reroll: document.getElementById('reroll'),
    scaleStat: document.getElementById('scaleStat'),
    story: document.getElementById('story'),
    placeholder: document.getElementById('placeholder'),
    storyTitle: document.getElementById('storyTitle'),
    storyGenre: document.getElementById('storyGenre'),
    storySeed: document.getElementById('storySeed'),
    beats: document.getElementById('beats'),
    copy: document.getElementById('copy'),
    exportOne: document.getElementById('exportOne'),
    save: document.getElementById('save'),
    batchStatus: document.getElementById('batchStatus'),
    libraryList: document.getElementById('libraryList'),
    libCount: document.getElementById('libCount'),
    libEmpty: document.getElementById('libEmpty'),
    clearLib: document.getElementById('clearLib'),
    templateCount: document.getElementById('templateCount')
  };

  var current = null; // the story shown right now

  // -------------------------------------------------------------- utilities
  // The slide-up message is shared/maz-toast.js's; this page styles #toast itself.
  var toast = window.MazToast || function () {};

  function download(filename, text) {
    var blob = new Blob([text], { type: 'text/markdown;charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }

  function genreLabel(g) {
    return g.replace(/-/g, ' ').replace(/\b\w/g, function (c) { return c.toUpperCase(); });
  }

  function loadLibrary() {
    try { return JSON.parse(localStorage.getItem(LIB_KEY)) || []; }
    catch (e) { return []; }
  }
  function saveLibrary(lib) {
    try { localStorage.setItem(LIB_KEY, JSON.stringify(lib)); }
    catch (e) { /* storage full / disabled — non-fatal */ }
  }

  // -------------------------------------------------------------- rendering
  function renderStory(story) {
    current = story;
    el.storyTitle.textContent = story.title;
    el.storyGenre.textContent = genreLabel(story.genre);
    el.storySeed.textContent = 'seed ' + story.seed;
    el.beats.innerHTML = '';
    story.beats.forEach(function (b) {
      var wrap = document.createElement('div');
      wrap.className = 'beat';
      var label = document.createElement('div');
      label.className = 'label';
      label.textContent = b.label;
      var text = document.createElement('div');
      text.className = 'text';
      text.textContent = b.text;
      wrap.appendChild(label);
      wrap.appendChild(text);
      el.beats.appendChild(wrap);
    });
    el.story.classList.remove('hidden');
    el.placeholder.classList.add('hidden');
  }

  function currentGenre() { return el.genre.value || 'all'; }

  function doGenerate() {
    renderStory(G.generate({ genre: currentGenre() }));
  }

  function doReroll() {
    if (!current) return doGenerate();
    // Same template, brand-new words.
    var template = G.templates.filter(function (t) { return t.id === current.id; })[0];
    var story = G.fillTemplate(template);
    story.genre = template.genre;
    renderStory(story);
  }

  // -------------------------------------------------------------- library UI
  function renderLibrary() {
    var lib = loadLibrary();
    el.libCount.textContent = String(lib.length);
    el.libraryList.innerHTML = '';
    el.libEmpty.style.display = lib.length ? 'none' : 'block';
    lib.forEach(function (item, i) {
      var li = document.createElement('li');

      var main = document.createElement('div');
      main.className = 'li-main';
      var title = document.createElement('div');
      title.className = 'li-title';
      title.textContent = item.title;
      var sub = document.createElement('div');
      sub.className = 'li-sub';
      sub.textContent = genreLabel(item.genre) + ' · ' + (item.beats[0] ? item.beats[0].text : '');
      main.appendChild(title);
      main.appendChild(sub);
      main.title = 'Click to reopen this idea';
      main.addEventListener('click', function () {
        renderStory(item);
        window.scrollTo({ top: 0, behavior: 'smooth' });
      });

      var del = document.createElement('button');
      del.className = 'li-del';
      del.textContent = '×';
      del.title = 'Remove';
      del.addEventListener('click', function (e) {
        e.stopPropagation();
        var l = loadLibrary();
        l.splice(i, 1);
        saveLibrary(l);
        renderLibrary();
      });

      li.appendChild(main);
      li.appendChild(del);
      el.libraryList.appendChild(li);
    });
  }

  function doSave() {
    if (!current) return;
    var lib = loadLibrary();
    if (lib.some(function (s) { return s.signature === current.signature; })) {
      return toast('Already in your library');
    }
    lib.unshift(current);
    saveLibrary(lib);
    renderLibrary();
    toast('Saved to library ★');
  }

  // -------------------------------------------------------------- batch
  function doBatch(n, btn) {
    btn.classList.add('busy');
    el.batchStatus.textContent = 'Forging ' + n + ' unique ideas…';
    // Defer so the UI can paint the "busy" state before the heavy loop.
    setTimeout(function () {
      var stories = G.generateMany(n, { genre: currentGenre() });
      var parts = stories.map(function (s, i) {
        return '<!-- #' + (i + 1) + ' -->\n' + G.toMarkdown(s);
      });
      var header = '# MadLibs Story Forge — ' + stories.length + ' story ideas\n' +
        '_Genre: ' + genreLabel(currentGenre()) + '_\n\n---\n\n';
      download('madlibs-' + stories.length + '.md', header + parts.join('\n---\n\n'));
      el.batchStatus.textContent = 'Exported ' + stories.length + ' ideas.';
      btn.classList.remove('busy');
    }, 30);
  }

  // -------------------------------------------------------------- init
  function initGenres() {
    var genres = {};
    G.templates.forEach(function (t) { genres[t.genre] = true; });
    var opts = ['<option value="all">All genres</option>'];
    Object.keys(genres).sort().forEach(function (g) {
      opts.push('<option value="' + g + '">' + genreLabel(g) + '</option>');
    });
    el.genre.innerHTML = opts.join('');
  }

  function initScaleStat() {
    var est = G.estimateCombinations();
    el.scaleStat.innerHTML = 'Can produce <b>' + est.pretty +
      '</b> distinct story ideas';
    el.templateCount.textContent = String(G.templates.length);
  }

  function bind() {
    el.generate.addEventListener('click', doGenerate);
    el.reroll.addEventListener('click', doReroll);
    el.copy.addEventListener('click', function () {
      if (!current) return;
      var md = G.toMarkdown(current);
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(md).then(
          function () { toast('Copied to clipboard'); },
          function () { toast('Copy failed'); }
        );
      } else {
        toast('Clipboard unavailable');
      }
    });
    el.exportOne.addEventListener('click', function () {
      if (!current) return;
      download('madlibs-' + current.id + '-' + current.seed + '.md', G.toMarkdown(current));
    });
    el.save.addEventListener('click', doSave);
    el.clearLib.addEventListener('click', function () {
      if (!loadLibrary().length) return;
      saveLibrary([]);
      renderLibrary();
      toast('Library cleared');
    });
    Array.prototype.forEach.call(document.querySelectorAll('.batch-btn'), function (btn) {
      btn.addEventListener('click', function () { doBatch(parseInt(btn.getAttribute('data-n'), 10), btn); });
    });
  }

  initGenres();
  initScaleStat();
  renderLibrary();
  bind();
  doGenerate(); // start with one ready to go
})();
