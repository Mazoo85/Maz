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
  var PROD_KEY = 'madlibs.productions.v1';

  // Production stages, in order. value -> label shown in the dropdown.
  var STAGES = [
    { value: 'idea', label: 'Idea' },
    { value: 'scripting', label: 'Scripting' },
    { value: 'chosen', label: 'Script chosen' },
    { value: 'storyboarded', label: 'Storyboarded' }
  ];

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
    templateCount: document.getElementById('templateCount'),
    install: document.getElementById('install'),
    script: document.getElementById('script'),
    prodList: document.getElementById('prodList'),
    prodCount: document.getElementById('prodCount'),
    prodEmpty: document.getElementById('prodEmpty'),
    clearProd: document.getElementById('clearProd')
  };

  var current = null; // the story shown right now

  // -------------------------------------------------------------- utilities
  function toast(msg) {
    var t = document.getElementById('toast');
    if (!t) {
      t = document.createElement('div');
      t.id = 'toast';
      document.body.appendChild(t);
    }
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.classList.remove('show'); }, 1600);
  }

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

  // Copy text to the clipboard with a toast; falls back gracefully.
  function copyText(text, okMsg) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () { toast(okMsg || 'Copied'); },
        function () { toast('Copy failed'); }
      );
    } else {
      toast('Clipboard unavailable');
    }
  }

  function slugFor(story) { return story.id + '-' + story.seed; }

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

      var film = document.createElement('button');
      film.className = 'li-film';
      film.textContent = '🎬';
      film.title = 'Send to Productions & copy the brief';
      film.addEventListener('click', function (e) {
        e.stopPropagation();
        renderStory(item);   // make it current so doScript uses it
        doScript();
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
      li.appendChild(film);
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

  // -------------------------------------------------------------- productions
  function loadProductions() {
    try { return JSON.parse(localStorage.getItem(PROD_KEY)) || []; }
    catch (e) { return []; }
  }
  function saveProductions(list) {
    try { localStorage.setItem(PROD_KEY, JSON.stringify(list)); }
    catch (e) { /* non-fatal */ }
  }

  // Send the current story to production and copy its brief for the writer.
  function doScript() {
    if (!current) return;
    var list = loadProductions();
    var existing = list.filter(function (p) { return p.signature === current.signature; })[0];
    if (!existing) {
      list.unshift({
        signature: current.signature,
        slug: slugFor(current),
        title: current.title,
        genre: current.genre,
        seed: current.seed,
        story: current,
        status: 'idea',
        scriptUrl: '',
        storyboardUrl: ''
      });
      saveProductions(list);
      renderProductions();
    }
    copyText(G.toBrief(current),
      existing ? 'Already in productions — brief copied' : 'Sent to Productions — brief copied');
  }

  function updateProduction(signature, patch) {
    var list = loadProductions();
    for (var i = 0; i < list.length; i++) {
      if (list[i].signature === signature) {
        for (var k in patch) if (patch.hasOwnProperty(k)) list[i][k] = patch[k];
        break;
      }
    }
    saveProductions(list);
  }

  function removeProduction(signature) {
    saveProductions(loadProductions().filter(function (p) { return p.signature !== signature; }));
    renderProductions();
  }

  // Build one labelled URL field (script or storyboard link).
  function linkField(prod, key, label) {
    var wrap = document.createElement('label');
    wrap.className = 'link-field';
    var span = document.createElement('span');
    span.textContent = label;
    var input = document.createElement('input');
    input.type = 'url';
    input.placeholder = 'paste ' + label.toLowerCase() + '…';
    input.value = prod[key] || '';
    input.addEventListener('change', function () {
      updateProduction(prod.signature, defineOne(key, input.value.trim()));
      renderProductions();
    });
    wrap.appendChild(span);
    wrap.appendChild(input);
    if (prod[key]) {
      var open = document.createElement('a');
      open.href = prod[key];
      open.target = '_blank';
      open.rel = 'noopener';
      open.className = 'open-link';
      open.textContent = 'open ↗';
      wrap.appendChild(open);
    }
    return wrap;
  }
  function defineOne(k, v) { var o = {}; o[k] = v; return o; }

  function renderProductions() {
    var list = loadProductions();
    el.prodCount.textContent = String(list.length);
    el.prodList.innerHTML = '';
    el.prodEmpty.style.display = list.length ? 'none' : 'block';

    list.forEach(function (prod) {
      var li = document.createElement('li');
      li.className = 'prod-item stage-' + prod.status;

      // Header row: title + stage dropdown + delete
      var head = document.createElement('div');
      head.className = 'prod-head';

      var titleWrap = document.createElement('div');
      titleWrap.className = 'prod-title-wrap';
      var title = document.createElement('span');
      title.className = 'prod-title';
      title.textContent = prod.title;
      var chip = document.createElement('span');
      chip.className = 'chip';
      chip.textContent = genreLabel(prod.genre);
      titleWrap.appendChild(title);
      titleWrap.appendChild(chip);

      var stage = document.createElement('select');
      stage.className = 'stage-select';
      stage.title = 'Production stage';
      STAGES.forEach(function (s) {
        var o = document.createElement('option');
        o.value = s.value; o.textContent = s.label;
        if (s.value === prod.status) o.selected = true;
        stage.appendChild(o);
      });
      stage.addEventListener('change', function () {
        updateProduction(prod.signature, { status: stage.value });
        renderProductions();
      });

      var del = document.createElement('button');
      del.className = 'li-del';
      del.textContent = '×';
      del.title = 'Remove from productions';
      del.addEventListener('click', function () { removeProduction(prod.signature); });

      head.appendChild(titleWrap);
      head.appendChild(stage);
      head.appendChild(del);

      // Actions: copy brief again
      var actions = document.createElement('div');
      actions.className = 'prod-actions';
      var briefBtn = document.createElement('button');
      briefBtn.className = 'mini';
      briefBtn.textContent = '📋 Copy brief';
      briefBtn.addEventListener('click', function () {
        copyText(G.toBrief(prod.story), 'Brief copied');
      });
      actions.appendChild(briefBtn);

      // Link fields
      var links = document.createElement('div');
      links.className = 'prod-links';
      links.appendChild(linkField(prod, 'scriptUrl', 'Script link'));
      links.appendChild(linkField(prod, 'storyboardUrl', 'Storyboard link'));

      li.appendChild(head);
      li.appendChild(actions);
      li.appendChild(links);
      el.prodList.appendChild(li);
    });
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

  // -------------------------------------------------------------- install (PWA)
  // Chromium fires beforeinstallprompt when the app is installable; we stash the
  // event and surface our own button, then trigger the native prompt on click.
  function initInstall() {
    var deferred = null;
    if (!el.install) return;

    window.addEventListener('beforeinstallprompt', function (e) {
      e.preventDefault();
      deferred = e;
      el.install.classList.remove('hidden');
    });

    el.install.addEventListener('click', function () {
      if (!deferred) return;
      deferred.prompt();
      deferred.userChoice.then(function (choice) {
        if (choice && choice.outcome === 'accepted') toast('Installing Story Forge…');
        deferred = null;
        el.install.classList.add('hidden');
      });
    });

    window.addEventListener('appinstalled', function () {
      deferred = null;
      el.install.classList.add('hidden');
      toast('Installed ★');
    });
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
      copyText(G.toMarkdown(current), 'Copied to clipboard');
    });
    el.exportOne.addEventListener('click', function () {
      if (!current) return;
      download('madlibs-' + current.id + '-' + current.seed + '.md', G.toMarkdown(current));
    });
    el.save.addEventListener('click', doSave);
    el.script.addEventListener('click', doScript);
    el.clearLib.addEventListener('click', function () {
      if (!loadLibrary().length) return;
      saveLibrary([]);
      renderLibrary();
      toast('Library cleared');
    });
    el.clearProd.addEventListener('click', function () {
      if (!loadProductions().length) return;
      saveProductions([]);
      renderProductions();
      toast('Productions cleared');
    });
    Array.prototype.forEach.call(document.querySelectorAll('.batch-btn'), function (btn) {
      btn.addEventListener('click', function () { doBatch(parseInt(btn.getAttribute('data-n'), 10), btn); });
    });
  }

  initGenres();
  initScaleStat();
  initInstall();
  renderLibrary();
  renderProductions();
  bind();
  doGenerate(); // start with one ready to go
})();
