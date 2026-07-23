/*
 * MadLibs Story Forge — UI layer
 * ------------------------------
 * A small multi-screen app (no framework, no build):
 *   Forge        — generate story ideas
 *   Productions  — every idea sent to production
 *   Project      — one production: its Idea, Script, and Storyboard, all in-app
 *
 * State lives in localStorage; scripts/storyboards are stored and rendered
 * inside the app (via window.Screenplay / window.Storyboard). Nothing leaves the
 * browser. Views are switched with hash routing (#forge, #productions,
 * #project/<signature>).
 */
(function () {
  'use strict';

  var G = window.MadlibsGenerator;
  var SP = window.Screenplay;
  var SB = window.Storyboard;
  var SAMPLE = window.MADLIBS_SAMPLE;

  var LIB_KEY = 'madlibs.library.v1';
  var PROD_KEY = 'madlibs.productions.v1';
  var SEED_FLAG = 'madlibs.seeded.v1';

  var STAGES = [
    { value: 'idea', label: 'Idea' },
    { value: 'scripting', label: 'Scripting' },
    { value: 'chosen', label: 'Script chosen' },
    { value: 'storyboarded', label: 'Storyboarded' }
  ];

  var el = {
    // nav / views
    navlinks: document.querySelectorAll('.navlink'),
    navProdCount: document.getElementById('navProdCount'),
    viewForge: document.getElementById('view-forge'),
    viewProductions: document.getElementById('view-productions'),
    viewProject: document.getElementById('view-project'),
    // forge
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
    script: document.getElementById('script'),
    batchStatus: document.getElementById('batchStatus'),
    libraryList: document.getElementById('libraryList'),
    libCount: document.getElementById('libCount'),
    libEmpty: document.getElementById('libEmpty'),
    clearLib: document.getElementById('clearLib'),
    templateCount: document.getElementById('templateCount'),
    install: document.getElementById('install'),
    // productions
    prodGrid: document.getElementById('prodGrid'),
    prodEmpty: document.getElementById('prodEmpty'),
    clearProd: document.getElementById('clearProd'),
    // project
    backToProd: document.getElementById('backToProd'),
    projTitle: document.getElementById('projTitle'),
    projGenre: document.getElementById('projGenre'),
    projStatus: document.getElementById('projStatus'),
    tabs: document.querySelectorAll('.tab'),
    tabIdea: document.getElementById('tab-idea'),
    tabScript: document.getElementById('tab-script'),
    tabStoryboard: document.getElementById('tab-storyboard')
  };

  var current = null;      // story shown in Forge
  var currentProjSig = null;
  var currentTab = 'idea';

  // ------------------------------------------------------------- utilities
  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
  function toast(msg) {
    var t = document.getElementById('toast');
    if (!t) { t = document.createElement('div'); t.id = 'toast'; document.body.appendChild(t); }
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.classList.remove('show'); }, 1700);
  }
  function download(filename, text, mime) {
    var blob = new Blob([text], { type: (mime || 'text/markdown') + ';charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url; a.download = filename;
    document.body.appendChild(a); a.click(); document.body.removeChild(a);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }
  function copyText(text, okMsg) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () { toast(okMsg || 'Copied'); },
        function () { toast('Copy failed'); }
      );
    } else { toast('Clipboard unavailable'); }
  }
  function genreLabel(g) {
    return String(g || '').replace(/-/g, ' ').replace(/\b\w/g, function (c) { return c.toUpperCase(); });
  }
  function stageLabel(v) {
    for (var i = 0; i < STAGES.length; i++) if (STAGES[i].value === v) return STAGES[i].label;
    return v;
  }

  // --------------------------------------------------------------- storage
  function loadLibrary() {
    try { return JSON.parse(localStorage.getItem(LIB_KEY)) || []; } catch (e) { return []; }
  }
  function saveLibrary(lib) {
    try { localStorage.setItem(LIB_KEY, JSON.stringify(lib)); } catch (e) {}
  }
  function loadProductions() {
    try { return JSON.parse(localStorage.getItem(PROD_KEY)) || []; } catch (e) { return []; }
  }
  function saveProductions(list) {
    try { localStorage.setItem(PROD_KEY, JSON.stringify(list)); } catch (e) {}
    el.navProdCount.textContent = String(list.length);
  }
  function getProduction(sig) {
    return loadProductions().filter(function (p) { return p.signature === sig; })[0] || null;
  }
  function updateProduction(sig, patch) {
    var list = loadProductions();
    for (var i = 0; i < list.length; i++) {
      if (list[i].signature === sig) {
        for (var k in patch) if (patch.hasOwnProperty(k)) list[i][k] = patch[k];
        break;
      }
    }
    saveProductions(list);
  }
  function removeProduction(sig) {
    saveProductions(loadProductions().filter(function (p) { return p.signature !== sig; }));
  }
  // Add a story to productions (idempotent by signature). Returns the signature.
  function upsertProduction(story) {
    var list = loadProductions();
    if (!list.some(function (p) { return p.signature === story.signature; })) {
      list.unshift({
        signature: story.signature,
        slug: story.id + '-' + story.seed,
        title: story.title, genre: story.genre, seed: story.seed,
        story: story, status: 'idea',
        script: null, storyboard: null,
        scriptUrl: '', storyboardUrl: ''
      });
      saveProductions(list);
    }
    return story.signature;
  }

  // ---------------------------------------------------------- forge: story
  function renderStory(story) {
    current = story;
    el.storyTitle.textContent = story.title;
    el.storyGenre.textContent = genreLabel(story.genre);
    el.storySeed.textContent = 'seed ' + story.seed;
    el.beats.innerHTML = '';
    story.beats.forEach(function (b) {
      var wrap = document.createElement('div'); wrap.className = 'beat';
      var label = document.createElement('div'); label.className = 'label'; label.textContent = b.label;
      var text = document.createElement('div'); text.className = 'text'; text.textContent = b.text;
      wrap.appendChild(label); wrap.appendChild(text);
      el.beats.appendChild(wrap);
    });
    el.story.classList.remove('hidden');
    el.placeholder.classList.add('hidden');
  }
  function currentGenre() { return el.genre.value || 'all'; }
  function doGenerate() { renderStory(G.generate({ genre: currentGenre() })); }
  function doReroll() {
    if (!current) return doGenerate();
    var template = G.templates.filter(function (t) { return t.id === current.id; })[0];
    var story = G.fillTemplate(template);
    story.genre = template.genre;
    renderStory(story);
  }
  function doSave() {
    if (!current) return;
    var lib = loadLibrary();
    if (lib.some(function (s) { return s.signature === current.signature; })) return toast('Already in your library');
    lib.unshift(current); saveLibrary(lib); renderLibrary();
    toast('Saved to library ★');
  }
  // Send current idea to Productions and jump into its project.
  function doScript() {
    if (!current) return;
    var sig = upsertProduction(current);
    copyText(G.toBrief(current), 'Sent to Productions — brief copied');
    navigate('project/' + encodeURIComponent(sig));
  }

  // -------------------------------------------------------- forge: library
  function renderLibrary() {
    var lib = loadLibrary();
    el.libCount.textContent = String(lib.length);
    el.libraryList.innerHTML = '';
    el.libEmpty.style.display = lib.length ? 'none' : 'block';
    lib.forEach(function (item, i) {
      var li = document.createElement('li');
      var main = document.createElement('div'); main.className = 'li-main';
      var title = document.createElement('div'); title.className = 'li-title'; title.textContent = item.title;
      var sub = document.createElement('div'); sub.className = 'li-sub';
      sub.textContent = genreLabel(item.genre) + ' · ' + (item.beats[0] ? item.beats[0].text : '');
      main.appendChild(title); main.appendChild(sub);
      main.title = 'Click to reopen this idea';
      main.addEventListener('click', function () {
        renderStory(item); navigate('forge');
        window.scrollTo({ top: 0, behavior: 'smooth' });
      });

      var film = document.createElement('button'); film.className = 'li-film';
      film.textContent = '🎬'; film.title = 'Send to Productions & start its script';
      film.addEventListener('click', function (e) { e.stopPropagation(); renderStory(item); doScript(); });

      var del = document.createElement('button'); del.className = 'li-del'; del.textContent = '×'; del.title = 'Remove';
      del.addEventListener('click', function (e) {
        e.stopPropagation();
        var l = loadLibrary(); l.splice(i, 1); saveLibrary(l); renderLibrary();
      });

      li.appendChild(main); li.appendChild(film); li.appendChild(del);
      el.libraryList.appendChild(li);
    });
  }

  // ----------------------------------------------------------------- batch
  function doBatch(n, btn) {
    btn.classList.add('busy');
    el.batchStatus.textContent = 'Forging ' + n + ' unique ideas…';
    setTimeout(function () {
      var stories = G.generateMany(n, { genre: currentGenre() });
      var parts = stories.map(function (s, i) { return '<!-- #' + (i + 1) + ' -->\n' + G.toMarkdown(s); });
      var header = '# MadLibs Story Forge — ' + stories.length + ' story ideas\n' +
        '_Genre: ' + genreLabel(currentGenre()) + '_\n\n---\n\n';
      download('madlibs-' + stories.length + '.md', header + parts.join('\n---\n\n'));
      el.batchStatus.textContent = 'Exported ' + stories.length + ' ideas.';
      btn.classList.remove('busy');
    }, 30);
  }

  // ----------------------------------------------------- productions (grid)
  function badge(ok, on, off) {
    return '<span class="mini-badge ' + (ok ? 'on' : 'off') + '">' + (ok ? on : off) + '</span>';
  }
  function renderProductionsGrid() {
    var list = loadProductions();
    el.navProdCount.textContent = String(list.length);
    el.prodGrid.innerHTML = '';
    el.prodEmpty.style.display = list.length ? 'none' : 'block';
    list.forEach(function (p) {
      var hasScript = !!(p.script && p.script.text);
      var hasBoard = !!(p.storyboard && p.storyboard.text);
      var card = document.createElement('button');
      card.className = 'prod-card stage-' + p.status;
      card.innerHTML =
        '<div class="pc-top"><span class="pc-title">' + esc(p.title) + '</span>' +
        (p.example ? '<span class="pc-example">example</span>' : '') + '</div>' +
        '<div class="pc-chips"><span class="chip">' + esc(genreLabel(p.genre)) + '</span>' +
        '<span class="chip ghost">' + esc(stageLabel(p.status)) + '</span></div>' +
        '<div class="pc-badges">' +
        badge(hasScript, '📝 script', '📝 no script') + ' ' +
        badge(hasBoard, '🎬 storyboard', '🎬 no board') + '</div>';
      card.addEventListener('click', function () { navigate('project/' + encodeURIComponent(p.signature)); });
      el.prodGrid.appendChild(card);
    });
  }

  // ---------------------------------------------------------- project view
  function openProject(sig) {
    currentProjSig = sig;
    var p = getProduction(sig);
    if (!p) { navigate('productions'); return; }

    el.projTitle.textContent = p.title;
    el.projGenre.textContent = genreLabel(p.genre);

    // status dropdown
    el.projStatus.innerHTML = STAGES.map(function (s) {
      return '<option value="' + s.value + '"' + (s.value === p.status ? ' selected' : '') + '>' + s.label + '</option>';
    }).join('');

    renderTabs(p);
    showTab(currentTab);
  }
  function refreshProject() { if (currentProjSig) openProject(currentProjSig); }

  function renderTabs(p) {
    // IDEA
    var beats = p.story.beats.map(function (b) {
      return '<div class="beat"><div class="label">' + esc(b.label) + '</div>' +
        '<div class="text">' + esc(b.text) + '</div></div>';
    }).join('');
    el.tabIdea.innerHTML =
      '<div class="pane-tools">' +
        '<button class="mini" data-act="copy-brief">📋 Copy brief for Claude</button>' +
        '<button class="mini" data-act="copy-idea">⬇ Copy idea</button>' +
      '</div><div class="beats">' + beats + '</div>';

    // SCRIPT
    el.tabScript.innerHTML = scriptPaneHTML(p);
    // STORYBOARD
    el.tabStoryboard.innerHTML = storyboardPaneHTML(p);

    wireProjectPane(p);
  }

  function scriptPaneHTML(p) {
    var has = p.script && p.script.text;
    if (!has) {
      return '' +
        '<div class="empty-pane">' +
          '<p>No script yet.</p>' +
          '<ol class="how-steps">' +
            '<li><button class="mini" data-act="copy-brief">📋 Copy the brief</button> and paste it to Claude.</li>' +
            '<li>Ask Claude to write the 30-minute script.</li>' +
            '<li><button class="mini accent" data-act="edit-script">＋ Paste the script here</button></li>' +
          '</ol>' +
        '</div>' +
        '<div class="editor hidden" data-editor="script">' + scriptEditorHTML('') + '</div>';
    }
    var mins = SP.estimateMinutes(p.script.text);
    return '' +
      '<div class="pane-tools">' +
        '<span class="pane-stat">~' + mins + ' min read</span>' +
        '<button class="mini" data-act="edit-script">✎ Edit</button>' +
        '<button class="mini" data-act="export-script">⬇ .fountain</button>' +
        '<button class="mini" data-act="copy-brief">📋 Copy brief</button>' +
      '</div>' +
      '<div class="script-page">' + SP.render(p.script.text) + '</div>' +
      '<div class="editor hidden" data-editor="script">' + scriptEditorHTML(p.script.text) + '</div>';
  }
  function scriptEditorHTML(text) {
    return '<p class="muted">Paste or edit the screenplay. Scene headings (INT./EXT.), ' +
      'ALL-CAPS character names, and (parentheticals) format automatically.</p>' +
      '<textarea class="edit-area" data-field="script" spellcheck="false">' + esc(text) + '</textarea>' +
      '<div class="editor-actions"><button class="mini accent" data-act="save-script">Save script</button>' +
      '<button class="mini" data-act="cancel-script">Cancel</button></div>';
  }

  function storyboardPaneHTML(p) {
    var has = p.storyboard && p.storyboard.text;
    if (!has) {
      return '' +
        '<div class="empty-pane">' +
          '<p>No storyboard yet.</p>' +
          '<ol class="how-steps">' +
            '<li>Pick a script first (write it in the Script tab).</li>' +
            '<li>Ask Claude to board it, or add panels yourself.</li>' +
            '<li><button class="mini accent" data-act="edit-board">＋ Add the storyboard</button></li>' +
          '</ol>' +
        '</div>' +
        '<div class="editor hidden" data-editor="board">' + boardEditorHTML('') + '</div>';
    }
    return '' +
      '<div class="pane-tools">' +
        '<span class="pane-stat">' + SB.summary(p.storyboard.text) + '</span>' +
        '<button class="mini" data-act="edit-board">✎ Edit</button>' +
      '</div>' +
      SB.render(p.storyboard.text) +
      '<div class="editor hidden" data-editor="board">' + boardEditorHTML(p.storyboard.text) + '</div>';
  }
  function boardEditorHTML(text) {
    return '<p class="muted">One panel per line, fields separated by <code>|</code>:<br>' +
      '<code>shot | framing | camera | slug | action | dialogue | seconds</code></p>' +
      '<textarea class="edit-area" data-field="board" spellcheck="false">' + esc(text) + '</textarea>' +
      '<div class="editor-actions"><button class="mini accent" data-act="save-board">Save storyboard</button>' +
      '<button class="mini" data-act="cancel-board">Cancel</button></div>';
  }

  // Wire the buttons inside the project panes (they're re-created each render).
  function wireProjectPane(p) {
    function on(act, fn) {
      Array.prototype.forEach.call(el.viewProject.querySelectorAll('[data-act="' + act + '"]'), function (b) {
        b.addEventListener('click', fn);
      });
    }
    function toggleEditor(kind, show) {
      var ed = el.viewProject.querySelector('[data-editor="' + kind + '"]');
      if (ed) ed.classList.toggle('hidden', !show);
    }
    on('copy-brief', function () { copyText(G.toBrief(p.story), 'Brief copied'); });
    on('copy-idea', function () { copyText(G.toMarkdown(p.story), 'Idea copied'); });

    on('edit-script', function () { toggleEditor('script', true); });
    on('cancel-script', function () { refreshProject(); });
    on('save-script', function () {
      var ta = el.viewProject.querySelector('textarea[data-field="script"]');
      var text = ta ? ta.value.trim() : '';
      var patch = { script: text ? { text: text, updatedAt: Date.now() } : null };
      if (text && (p.status === 'idea' || p.status === 'scripting')) patch.status = 'chosen';
      updateProduction(p.signature, patch);
      toast(text ? 'Script saved' : 'Script cleared');
      refreshProject();
    });
    on('export-script', function () {
      download((p.slug || 'script') + '.fountain', p.script.text, 'text/plain');
    });

    on('edit-board', function () { toggleEditor('board', true); });
    on('cancel-board', function () { refreshProject(); });
    on('save-board', function () {
      var ta = el.viewProject.querySelector('textarea[data-field="board"]');
      var text = ta ? ta.value.trim() : '';
      var patch = { storyboard: text ? { text: text, updatedAt: Date.now() } : null };
      if (text) patch.status = 'storyboarded';
      updateProduction(p.signature, patch);
      toast(text ? 'Storyboard saved' : 'Storyboard cleared');
      refreshProject();
    });
  }

  function showTab(tab) {
    currentTab = tab;
    Array.prototype.forEach.call(el.tabs, function (t) {
      t.classList.toggle('active', t.getAttribute('data-tab') === tab);
    });
    el.tabIdea.classList.toggle('hidden', tab !== 'idea');
    el.tabScript.classList.toggle('hidden', tab !== 'script');
    el.tabStoryboard.classList.toggle('hidden', tab !== 'storyboard');
  }

  // ------------------------------------------------------------- routing
  function setView(name) {
    el.viewForge.classList.toggle('hidden', name !== 'forge');
    el.viewProductions.classList.toggle('hidden', name !== 'productions');
    el.viewProject.classList.toggle('hidden', name !== 'project');
    Array.prototype.forEach.call(el.navlinks, function (n) {
      var v = n.getAttribute('data-view');
      n.classList.toggle('active', v === name || (name === 'project' && v === 'productions'));
    });
  }
  function navigate(hash) { location.hash = '#' + hash; }
  function route() {
    var h = (location.hash || '#forge').replace(/^#/, '');
    if (h.indexOf('project/') === 0) {
      var sig = decodeURIComponent(h.slice('project/'.length));
      if (getProduction(sig)) { setView('project'); openProject(sig); window.scrollTo(0, 0); return; }
      navigate('productions'); return;
    }
    if (h === 'productions') { setView('productions'); renderProductionsGrid(); window.scrollTo(0, 0); return; }
    setView('forge');
  }

  // ------------------------------------------------------------- install
  function initInstall() {
    var deferred = null;
    if (!el.install) return;
    window.addEventListener('beforeinstallprompt', function (e) {
      e.preventDefault(); deferred = e; el.install.classList.remove('hidden');
    });
    el.install.addEventListener('click', function () {
      if (!deferred) return;
      deferred.prompt();
      deferred.userChoice.then(function (c) {
        if (c && c.outcome === 'accepted') toast('Installing Story Forge…');
        deferred = null; el.install.classList.add('hidden');
      });
    });
    window.addEventListener('appinstalled', function () {
      deferred = null; el.install.classList.add('hidden'); toast('Installed ★');
    });
  }

  // ---------------------------------------------------------------- init
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
    el.scaleStat.innerHTML = 'Can produce <b>' + est.pretty + '</b> distinct story ideas';
    el.templateCount.textContent = String(G.templates.length);
  }
  // Seed the built-in example once, so the app opens already populated.
  function seedSample() {
    if (!SAMPLE) return;
    if (localStorage.getItem(SEED_FLAG)) return;
    var list = loadProductions();
    if (!list.some(function (p) { return p.signature === SAMPLE.signature; })) {
      list.push(SAMPLE);
      saveProductions(list);
    }
    try { localStorage.setItem(SEED_FLAG, '1'); } catch (e) {}
  }

  function bind() {
    // nav
    Array.prototype.forEach.call(el.navlinks, function (n) {
      n.addEventListener('click', function () { navigate(n.getAttribute('data-view')); });
    });
    el.backToProd.addEventListener('click', function () { navigate('productions'); });
    Array.prototype.forEach.call(el.tabs, function (t) {
      t.addEventListener('click', function () { showTab(t.getAttribute('data-tab')); });
    });
    el.projStatus.addEventListener('change', function () {
      if (currentProjSig) { updateProduction(currentProjSig, { status: el.projStatus.value }); }
    });

    // forge
    el.generate.addEventListener('click', doGenerate);
    el.reroll.addEventListener('click', doReroll);
    el.copy.addEventListener('click', function () { if (current) copyText(G.toMarkdown(current), 'Copied to clipboard'); });
    el.exportOne.addEventListener('click', function () {
      if (current) download('madlibs-' + current.id + '-' + current.seed + '.md', G.toMarkdown(current));
    });
    el.save.addEventListener('click', doSave);
    el.script.addEventListener('click', doScript);
    el.clearLib.addEventListener('click', function () {
      if (!loadLibrary().length) return;
      saveLibrary([]); renderLibrary(); toast('Library cleared');
    });
    Array.prototype.forEach.call(document.querySelectorAll('.batch-btn'), function (btn) {
      btn.addEventListener('click', function () { doBatch(parseInt(btn.getAttribute('data-n'), 10), btn); });
    });

    // productions
    el.clearProd.addEventListener('click', function () {
      if (!loadProductions().length) return;
      if (!window.confirm('Remove all productions (including the example)?')) return;
      saveProductions([]); renderProductionsGrid(); toast('Productions cleared');
    });

    window.addEventListener('hashchange', route);
  }

  initGenres();
  initScaleStat();
  seedSample();
  el.navProdCount.textContent = String(loadProductions().length);
  renderLibrary();
  renderProductionsGrid();
  initInstall();
  bind();
  doGenerate();  // one idea ready on the Forge screen
  route();       // honor the opening hash
})();
