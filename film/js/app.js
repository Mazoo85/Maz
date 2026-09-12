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
  var Seed = window.FilmStorySeed;

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
  ['idea', 'examples', 'length', 'genre', 'titleInput', 'write', 'reroll', 'surprise', 'placeholder',
   'result', 'scriptTitle', 'scriptLogline', 'chipGenre', 'chipScenes', 'chipRuntime',
   'chipSeed', 'tabScript', 'tabShots', 'tabFilm', 'tabWhy', 'viewScript', 'viewShots', 'viewFilm', 'viewWhy',
   'editToggle', 'rerollFree', 'undoEdit', 'directStatus', 'directBar',
   'copy', 'dlFountain', 'dlFdx', 'dlText', 'dlShots', 'print', 'save', 'status',
   'libraryList', 'libCount', 'libEmpty', 'clearLib', 'exportLib', 'importLib', 'importFile', 'shareFilm', 'filmCanvas', 'bigPlay', 'playFilm',
   'stopFilm', 'recordFilm', 'dlReel', 'filmSize', 'speakAloud', 'scrubBar', 'scrubFill', 'filmClock',
   'filmNote'].forEach(function (id) {
    el[id] = document.getElementById(id);
  });

  var Reel = window.FilmReel;
  var Director = window.FilmDirector;
  var Why = window.FilmWhy;
  var Library = window.FilmLibrary;
  var PlayerLib = window.FilmPlayer;
  var ScoreLib = window.FilmScore;

  var current = null;  // the script on screen

  /* ------------------------------------------------------------- directing
   * A film used to be a slot machine: press the button, get a film, press it
   * again, get a different one, and every good one was a roll you could not
   * refine. These three hold what it takes to refine one — whether the page is
   * editable, which scenes are being kept, and one step of undo, because an
   * edit you cannot take back is one most people will not risk making. */
  var editing = false;
  var lockedScenes = [];
  var undoStack = [];
  var reel = null;     // that script, cut into shots
  var player = null;   // the thing playing it
  var score = null;    // the thing scoring it
  var recording = false;

  // Set by "Surprise me" right before it clicks Write, and consumed by the
  // very next generate() call — never carried past it, so a borrowed title
  // cannot leak into a later, unrelated write.
  var pendingBorrow = null;

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

    // "Surprise me" hands the next call its own borrowed idea/genre/title;
    // consumed the moment it's read, so it never outlives this one write.
    var borrow = pendingBorrow;
    pendingBorrow = null;

    var idea = el.idea.value.trim();
    var thin = idea.split(/\s+/).filter(Boolean).length < 3;

    var seed = opts.seed;
    if (typeof seed !== 'number') {
      seed = borrow ? borrow.seed :
        // A fresh, real idea is seeded from its own words, so the same
        // sentence always gives the same film. A thin or empty box has no
        // words worth seeding from — hashing 'blank' every time would draw
        // the exact same borrowed story forever, so it rolls like "Another
        // take" does instead, and a second press on the same empty box
        // lands somewhere new.
        (opts.fresh && !thin ? Parse.hashText(idea) : (Math.random() * 4294967296) >>> 0);
    }

    // An idea too thin to work with — empty, or under three words — borrows
    // one from MADLIBS instead, seeded the same way this film will be, so
    // "Write the script" and "Another take" never dead-end on a bare box.
    // "Surprise me" already writes its borrowed text into the box on
    // purpose (below); a thin idea has to be just as honest about it, or
    // the box keeps showing words that are no longer what the film is
    // about.
    if (!borrow && thin) {
      borrow = Seed.idea(seed);
      el.idea.value = borrow.text;
      say('Too thin to write from — borrowed a story from MADLIBS instead.');
    }
    if (borrow) idea = borrow.text;

    var premise = Parse.parse(idea, {
      // A genre the user actually chose always wins; MADLIBS's own genre for
      // a borrowed story only fills in while the dropdown is left on auto.
      genre: (el.genre.value === 'auto' && borrow) ? borrow.genre : el.genre.value,
      seed: seed,
      // A title the user typed always wins; otherwise a borrowed story keeps
      // its own hand-written title (uppercased to match every generated
      // title's ALL CAPS card) instead of a generated one, and that title
      // never persists past this one script — the next call starts fresh.
      title: el.titleInput.value.trim() || (borrow ? borrow.title.toUpperCase() : null)
    });
    current = Writer.write(premise, { length: el.length.value, seed: seed });
    // Locks and undo belong to the film they were made on. Carrying them over
    // would keep a scene from a film that no longer exists.
    lockedScenes = [];
    undoStack = [];
    editing = false;
    render(current);
    updateDirectBar();
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
    buildFilm(script);
    renderWhy(script);
    el.result.scrollIntoView({ behavior: 'smooth', block: 'start' });
  }

  /* Apply an edit: keep the old script for undo, show the new one, and rebuild
   * the film from it. Everything the director does goes through here, so there
   * is exactly one place that knows an edit has to reach the picture. */
  function applyEdit(next, note) {
    if (!next) return;
    undoStack.push(current);
    if (undoStack.length > 20) undoStack.shift();
    current = next;
    render(current);
    say(note || 'Changed.');
    updateDirectBar();
  }

  function updateDirectBar() {
    if (!el.editToggle) return;
    el.editToggle.setAttribute('aria-pressed', editing ? 'true' : 'false');
    el.editToggle.textContent = editing ? '✓ Done editing' : '✎ Edit lines';
    el.undoEdit.disabled = undoStack.length === 0;
    el.directStatus.textContent = lockedScenes.length
      ? lockedScenes.length + ' scene' + (lockedScenes.length === 1 ? '' : 's') + ' locked'
      : (editing ? 'Click a line to rewrite it' : '');
    el.viewScript.classList.toggle('is-editing', editing);
  }

  function toggleLock(number) {
    var at = lockedScenes.indexOf(number);
    if (at === -1) lockedScenes.push(number);
    else lockedScenes.splice(at, 1);
    renderPage(current);
    updateDirectBar();
  }

  function renderPage(script) {
    var page = el.viewScript;
    page.textContent = '';
    var sceneNo = 0;

    // Where each element sits in its own scene. The flat index the page renders
    // is not a stable address for an edit -- deleting a scene moves every index
    // after it -- so each node carries the scene it belongs to and its position
    // inside that scene, which survive everything the director can do.
    var withinScene = -1;

    script.elements.forEach(function (element) {
      var node = document.createElement('p');
      node.className = 'el ' + element.type;
      if (element.type === 'scene_heading') {
        sceneNo++;
        withinScene = 0;
        var num = document.createElement('span');
        num.className = 'scene-num';
        num.textContent = String(sceneNo);
        node.appendChild(num);
        if (lockedScenes.indexOf(sceneNo) !== -1) node.classList.add('scene-locked');
      } else if (element.type !== 'transition') {
        withinScene++;
      }

      if (element.type === 'parenthetical') {
        node.appendChild(document.createTextNode('(' + element.text.replace(/^\(|\)$/g, '') + ')'));
      } else {
        node.appendChild(document.createTextNode(element.text));
      }

      if (element.type === 'scene_heading') {
        node.appendChild(sceneTools(sceneNo, script));
      } else if (editing && (element.type === 'dialogue' || element.type === 'action')) {
        makeEditable(node, sceneNo, withinScene);
      }
      page.appendChild(node);
    });

    var end = document.createElement('p');
    end.className = 'el the-end';
    end.textContent = 'THE END';
    page.appendChild(end);
  }

  /* One scene's controls, on its heading. Shown only while editing, because a
   * row of buttons on every heading is not a screenplay any more. */
  function sceneTools(number, script) {
    var wrap = document.createElement('span');
    wrap.className = 'scene-tools no-print';
    if (!editing) return wrap;

    var lock = document.createElement('button');
    var isLocked = lockedScenes.indexOf(number) !== -1;
    lock.textContent = isLocked ? '🔒 kept' : '🔓 keep';
    lock.title = 'Keep this scene when you reroll the rest';
    if (isLocked) lock.className = 'locked';
    lock.addEventListener('click', function () { toggleLock(number); });
    wrap.appendChild(lock);

    var hour = document.createElement('select');
    hour.title = 'What time of day this scene plays at';
    Director.HOURS.forEach(function (h) {
      var opt = document.createElement('option');
      opt.value = h; opt.textContent = h;
      if (script.scenes[number - 1] && script.scenes[number - 1].heading.time === h) opt.selected = true;
      hour.appendChild(opt);
    });
    hour.addEventListener('change', function () {
      applyEdit(Director.setHour(current, number, hour.value), 'Scene ' + number + ' now plays at ' + hour.value + '.');
    });
    wrap.appendChild(hour);

    [['↑', -1, 'Move this scene earlier'], ['↓', 1, 'Move this scene later']].forEach(function (spec) {
      var move = document.createElement('button');
      move.textContent = spec[0];
      move.title = spec[2];
      move.addEventListener('click', function () {
        applyEdit(Director.moveScene(current, number, spec[1]), 'Moved scene ' + number + '.');
      });
      wrap.appendChild(move);
    });

    var cut = document.createElement('button');
    cut.textContent = '✕';
    cut.title = 'Cut this scene';
    cut.addEventListener('click', function () {
      applyEdit(Director.deleteScene(current, number), 'Cut scene ' + number + '.');
    });
    wrap.appendChild(cut);
    return wrap;
  }

  /* A line you can type into. Enter commits, Escape abandons, and leaving the
   * line commits too -- anything else and people lose work to a stray click. */
  function makeEditable(node, sceneNumber, elementIndex) {
    node.contentEditable = 'true';
    node.spellcheck = true;
    var before = node.textContent;
    node.addEventListener('keydown', function (e) {
      if (e.key === 'Enter') { e.preventDefault(); node.blur(); }
      if (e.key === 'Escape') { node.textContent = before; node.blur(); }
    });
    node.addEventListener('blur', function () {
      var text = node.textContent.replace(/\s+/g, ' ').trim();
      if (!text || text === before) { node.textContent = before; return; }
      applyEdit(Director.editLine(current, sceneNumber, elementIndex, text), 'Rewritten.');
    });
  }

  /* Every choice the program made, in the order somebody would want to read
   * them. Rendered after the film is built, because half of what it explains --
   * the cutting, the tempo, the room tone -- is a property of the reel rather
   * than of the script. */
  function renderWhy(script) {
    if (!Why || !el.viewWhy) return;
    el.viewWhy.textContent = '';
    var sections;
    try {
      sections = Why.explain(script, reel);
    } catch (e) {
      // A panel that explains the film must never be the thing that breaks it.
      el.viewWhy.textContent = 'Could not work out why: ' + e.message;
      return;
    }
    sections.forEach(function (section) {
      var box = document.createElement('section');
      box.className = 'why-section';
      var heading = document.createElement('h3');
      heading.textContent = section.title;
      box.appendChild(heading);
      var list = document.createElement('ul');
      section.lines.forEach(function (line) {
        var item = document.createElement('li');
        item.textContent = line;
        list.appendChild(item);
      });
      box.appendChild(list);
      el.viewWhy.appendChild(box);
    });
    var foot = document.createElement('p');
    foot.className = 'why-foot';
    foot.textContent = 'Every line above is read from the decision itself, not written about it ' +
      'afterwards — so it cannot drift out of step with the film you are watching.';
    el.viewWhy.appendChild(foot);
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
    var tabs = { script: el.tabScript, shots: el.tabShots, film: el.tabFilm, why: el.tabWhy };
    var views = { script: el.viewScript, shots: el.viewShots, film: el.viewFilm, why: el.viewWhy };
    Object.keys(tabs).forEach(function (key) {
      var on = key === which;
      tabs[key].classList.toggle('is-on', on);
      tabs[key].setAttribute('aria-selected', String(on));
      views[key].classList.toggle('hidden', !on);
    });
    // Leaving the film tab stops the film; nobody wants a soundtrack from a
    // tab they are not looking at.
    if (which !== 'film' && player && player.playing && !recording) stopFilm();
  }

  /* ------------------------------------------------------------------- film */
  function buildFilm(script) {
    stopFilm();
    reel = Reel.build(script);
    // A window onto what is actually on screen, for the browser tests. Every
    // director edit has to survive the trip from a click to the picture, and
    // the trip is exactly where these things break -- a test that calls the
    // module instead of clicking the button proves the module, not the app.
    window.__filmState = { script: function () { return current; }, reel: function () { return reel; } };
    sizeCanvas();
    // A poster frame, so the tab is never a black rectangle.
    PlayerLib.drawFrame(el.filmCanvas.getContext('2d'),
      el.filmCanvas.width, el.filmCanvas.height, reel, 1.2);
    el.bigPlay.classList.remove('hidden');
    updateScrub(0, reel.duration);

    describeRecording();
  }

  /* What the film tab says when SONG FORGE could not compose. The film still
   * plays — it just has no music under it. */
  var NO_SCORE_NOTE = 'This browser could not compose a score, so the film plays with ' +
    'its cut hits, character voices and a pulse — but no music.';
  var scoreNote = '';

  /* Say what the download will be *before* anyone sits through a recording.
   * Which format you get is the browser's choice, not ours, and it decides
   * whether the film will play on an iPhone. The missing-score notice rides
   * along on the same element, because both are things to know before you
   * press play. */
  function describeRecording() {
    var format = PlayerLib.bestFormat();

    if (!format || !PlayerLib.canRecord(el.filmCanvas)) {
      if (!recording) {
        el.recordFilm.disabled = true;
        el.recordFilm.textContent = '⬇ Make the video file';
      }
      el.filmNote.className = 'film-note warn';
      el.filmNote.textContent =
        'This browser can play the film but cannot save it to a video file. ' +
        'Chrome, Edge and Firefox on a computer can — Safari and most phones cannot.';
      addScoreNote();
      return;
    }

    if (!recording) {
      el.recordFilm.disabled = false;
      el.recordFilm.textContent = '⬇ Make the video file (' + format.extension + ')';
    }

    var realTime = 'Recording plays the film once, in real time — a two-minute film takes ' +
      'two minutes. Leave this tab open while it records.';

    if (format.playsOnApple) {
      el.filmNote.className = 'film-note';
      el.filmNote.textContent = 'You will get an .mp4, which plays on anything — phone, ' +
        'computer, TV. ' + realTime;
    } else {
      // An honest warning beats a file that fails silently on someone's phone.
      el.filmNote.className = 'film-note warn';
      el.filmNote.textContent = 'This browser saves .webm, which plays on computers ' +
        '(Chrome, Edge, Firefox, VLC) and Android — but not on an iPhone, iPad or in ' +
        'QuickTime. To get a film onto an Apple device, upload the .webm somewhere that ' +
        're-encodes it, such as YouTube or Google Photos, or open it in a free converter ' +
        'like HandBrake. ' + realTime;
    }
    addScoreNote();
  }

  function addScoreNote() {
    if (!scoreNote) return;
    el.filmNote.className = 'film-note warn';
    el.filmNote.textContent = scoreNote + ' ' + el.filmNote.textContent;
  }

  function sizeCanvas() {
    var size = (el.filmSize.value || '1280x720').split('x');
    el.filmCanvas.width = parseInt(size[0], 10);
    el.filmCanvas.height = parseInt(size[1], 10);
  }

  function updateScrub(time, duration) {
    var pct = duration ? Math.min(100, (time / duration) * 100) : 0;
    el.scrubFill.style.width = pct + '%';
    el.filmClock.textContent = Reel.clock(time) + ' / ' + Reel.clock(duration || 0);
    el.scrubBar.setAttribute('aria-valuenow', String(Math.round(time)));
    el.scrubBar.setAttribute('aria-valuemax', String(Math.round(duration || 0)));
  }

  /* Clicking the bar jumps there. Every frame is drawn from the reel on
   * demand, so this works even on a part of the film nobody has watched yet. */
  function scrubTo(fraction) {
    if (!reel || recording) return;
    if (!player) makePlayer();
    player.seek(Math.max(0, Math.min(1, fraction)) * reel.duration);
    el.bigPlay.classList.toggle('hidden', player.playing);
  }

  function makePlayer() {
    if (score) score.close();
    score = null;
    if (ScoreLib.supported()) {
      try {
        // The stride rate comes from the player so the footsteps land on the
        // same beat the legs do. One number, one owner.
        score = new ScoreLib.Score(reel, { walkRate: PlayerLib.WALK_RATE });
      } catch (e) {
        score = null; // a film with no sound still plays
      }
    }
    // Every rebuild says where it stands, including the browsers that gave us
    // no Score at all — otherwise the panel keeps the last film's answer and
    // the audience is told nothing.
    var scored = score ? score.startScore(reel) : false;
    // The panel carries what it has: which kind of score, and what the
    // music is doing. The film note and the tests both read it.
    el.viewFilm.dataset.score = scored ? 'real' : 'fallback';
    el.viewFilm.dataset.sections = scored && score.player && score.player.song
      ? String(score.player.song.sections.length) : '0';
    // A missing score is a fact about the film you are about to watch, so it
    // belongs on the film tab beside the recording note — not in the status
    // line, which scrolls away a moment later.
    var note = scored ? '' : NO_SCORE_NOTE;
    if (note !== scoreNote) {
      scoreNote = note;
      describeRecording();
    }
    player = new PlayerLib.Player(el.filmCanvas, reel, {
      score: score,
      onFrame: function (time, duration) { updateScrub(time, duration); },
      onShot: function (shot) { if (el.speakAloud.checked) speakAloud(shot); },
      onPlay: function () { el.viewFilm.dataset.music = 'playing'; },
      onStop: function (ended) {
        el.bigPlay.classList.remove('hidden');
        el.playFilm.textContent = '▶ Play the film';
        if (window.speechSynthesis) window.speechSynthesis.cancel();
        if (ended) updateScrub(reel.duration, reel.duration);
        el.viewFilm.dataset.music = 'stopped';
      },
      onPause: function () {
        el.bigPlay.classList.remove('hidden');
        el.playFilm.textContent = '▶ Resume';
        if (window.speechSynthesis) window.speechSynthesis.cancel();
        el.viewFilm.dataset.music = 'paused';
      }
    });
    return player;
  }

  function playFilm() {
    if (!reel) return;
    if (player && player.playing) {
      player.pause();
      return;
    }
    showTab('film');
    // Resuming keeps the canvas as it is; starting over resizes it first.
    var resumeAt = player && !player.playing && player.time > 0 && player.time < reel.duration
      ? player.time
      : 0;
    if (!resumeAt) sizeCanvas();
    var p = resumeAt && player ? player : makePlayer();
    p.play(resumeAt);
    el.bigPlay.classList.add('hidden');
    el.playFilm.textContent = '⏸ Pause';
    // Starting over announces the runtime. Nothing to guard against now: a
    // missing score is written on the film tab, not here.
    if (!resumeAt) say('Playing. ' + Reel.clock(reel.duration) + ' of film.');
  }

  function stopFilm() {
    if (player) {
      // Stop unconditionally: the film can be stopped from a pause too, and
      // that still has to flip the music state to 'stopped', not leave it
      // reading 'paused'.
      player.stop(false);
      player.time = 0;
    }
    if (window.speechSynthesis) window.speechSynthesis.cancel();
    if (reel) {
      PlayerLib.drawFrame(el.filmCanvas.getContext('2d'),
        el.filmCanvas.width, el.filmCanvas.height, reel, 1.2);
      updateScrub(0, reel.duration);
    }
    el.bigPlay.classList.remove('hidden');
    el.playFilm.textContent = '▶ Play the film';
  }

  /* The browser's own voice, for anyone who would rather hear words than
   * character blips. It cannot be recorded into the file — the browser keeps
   * that audio to itself — so it is offered as a live extra, clearly labelled. */
  function speakAloud(shot) {
    if (!window.speechSynthesis || shot.kind !== 'line') return;
    var utter = new window.SpeechSynthesisUtterance(shot.caption);
    var voice = reel.voices[shot.speaker];
    if (voice) {
      utter.pitch = Math.max(0.4, Math.min(1.8, voice.pitch / 150));
      utter.rate = voice.rate;
    }
    utter.volume = 0.9;
    window.speechSynthesis.speak(utter);
  }

  function recordFilm() {
    if (!reel || recording) return;
    if (!PlayerLib.canRecord(el.filmCanvas)) {
      say('This browser cannot save video. Try Chrome, Edge or Firefox on a computer.');
      return;
    }
    showTab('film');
    sizeCanvas();
    recording = true;
    el.recordFilm.disabled = true;
    el.playFilm.disabled = true;
    el.filmSize.disabled = true;
    el.bigPlay.classList.add('hidden');
    el.recordFilm.textContent = '● Recording…';

    var p = makePlayer();

    // Drawn art is flat colour and hard edges, so it needs far less bitrate
    // than camera footage at the same size. These are set where the picture
    // stops improving, which keeps the files small enough to actually send.
    var height = el.filmCanvas.height;
    var bitrate = height >= 1080 ? 6000000 : height >= 720 ? 2200000 : 1100000;

    PlayerLib.record(p, { fps: 30, videoBitrate: bitrate })
      .then(function (result) {
        // The recorder cannot know the length while it is still recording, so
        // write it into the file afterwards — otherwise players show no
        // timeline and the length reads as unknown wherever you share it.
        var played = player ? Math.max(1, player.time) : reel.duration;
        return window.FilmWebm.withDuration(result.blob, played).then(function (blob) {
          var name = Format.slugify(current.title) + result.format.extension;
          download(name, blob, result.mime);
          say('Your film is saved as ' + name + ' — ' + Reel.clock(played) + ' long.');
        });
      })
      .catch(function (e) {
        say('Recording failed: ' + (e && e.message ? e.message : 'unknown error'));
      })
      .then(function () {
        recording = false;
        el.playFilm.disabled = false;
        el.filmSize.disabled = false;
        describeRecording();
      });
  }

  /* ---------------------------------------------------------------- exports */
  function download(filename, content, mime) {
    var blob = content instanceof Blob
      ? content
      : new Blob([content], { type: (mime || 'text/plain') + ';charset=utf-8' });
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

  /* ------------------------------------------------------------- directing */
  el.editToggle.addEventListener('click', function () {
    editing = !editing;
    renderPage(current);
    updateDirectBar();
    if (editing) say('Click any line to rewrite it. Lock the scenes you want to keep.');
  });

  el.rerollFree.addEventListener('click', function () {
    if (!current) return;
    // A new seed for the scenes nobody kept. The locked ones come through
    // untouched, which is the whole point of the button.
    var seed = (Math.random() * 4294967296) >>> 0;
    applyEdit(Director.reroll(current, { seed: seed, length: current.length }, lockedScenes),
      lockedScenes.length
        ? 'Rewrote everything except ' + lockedScenes.length + ' locked scene' +
          (lockedScenes.length === 1 ? '' : 's') + '.'
        : 'Rewrote the whole film.');
  });

  el.undoEdit.addEventListener('click', function () {
    if (!undoStack.length) return;
    current = undoStack.pop();
    render(current);
    say('Undone.');
    updateDirectBar();
  });
  el.reroll.addEventListener('click', function () { generate({}); });
  el.surprise.addEventListener('click', function () {
    // The one legitimate random number: the user asked to be surprised.
    // Everything downstream of this seed is deterministic.
    var seed = (Date.now() ^ Math.floor(Math.random() * 0xffffffff)) >>> 0;
    var story = Seed.idea(seed);
    el.idea.value = story.text;
    pendingBorrow = { genre: story.genre, title: story.title, seed: seed };
    el.write.click();          // the same path the Write button takes
  });

  el.idea.addEventListener('keydown', function (e) {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      generate({ fresh: true });
    }
  });

  el.tabScript.addEventListener('click', function () { showTab('script'); });
  el.tabShots.addEventListener('click', function () { showTab('shots'); });
  el.tabFilm.addEventListener('click', function () { showTab('film'); });
  el.tabWhy.addEventListener('click', function () { showTab('why'); });

  /* ------------------------------------------------- getting films off here */

  el.shareFilm.addEventListener('click', function () {
    if (!current) return;
    // A film is its seed -- but only until somebody edits it. Rebuild from the
    // card and compare: if the link would open a DIFFERENT film, say so rather
    // than handing over a link that quietly lies.
    var rebuilt = null;
    try {
      rebuilt = Writer.write(
        Parse.parse(current.idea, { genre: current.premise.genreAuto ? undefined : current.genre }),
        { length: current.length, seed: current.seed });
    } catch (e) { rebuilt = null; }

    if (!Library.shareableBySeed(current, rebuilt)) {
      say('This film has been edited, so a link would rebuild the original. ' +
          'Use ⬇ .reel.json to share exactly what you have.');
      return;
    }
    var url = Library.toShareUrl(current, window.location.href);
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(url).then(function () {
        say('Link copied. It rebuilds this exact film on any device.');
      }, function () {
        window.prompt('Copy this link:', url);
      });
    } else {
      window.prompt('Copy this link:', url);
    }
  });

  el.exportLib.addEventListener('click', function () {
    var list = readLibrary();
    if (!list.length) { say('Nothing saved yet.'); return; }
    download('script-forge-library.json', Library.exportLibrary(list), 'application/json');
    say('Exported ' + list.length + ' film' + (list.length === 1 ? '' : 's') + '.');
  });

  el.importLib.addEventListener('click', function () { el.importFile.click(); });

  el.importFile.addEventListener('change', function () {
    var file = el.importFile.files && el.importFile.files[0];
    if (!file) return;
    var reader = new FileReader();
    reader.onload = function () {
      // MERGE, never replace: somebody importing their laptop's library onto
      // their phone has films on the phone too.
      var result = Library.importLibrary(reader.result, readLibrary());
      if (result.error) { say(result.error); return; }
      writeLibrary(result.films.slice(0, 50));
      renderLibrary();
      say('Added ' + result.added + ' film' + (result.added === 1 ? '' : 's') +
          (result.skipped ? ', skipped ' + result.skipped + ' already here' : '') + '.');
    };
    reader.readAsText(file);
    el.importFile.value = '';
  });

  el.playFilm.addEventListener('click', playFilm);
  el.bigPlay.addEventListener('click', playFilm);
  el.stopFilm.addEventListener('click', function () {
    if (recording && player) player.stop(true); // keep the take shot so far
    else stopFilm();
  });
  el.recordFilm.addEventListener('click', recordFilm);
  /* The reel is the film as plain data: every shot, when it starts, how long it
   * holds, which set, which framing, who is in it and what they say. It is what
   * the picture is drawn *from*, so anything that can read it can draw the film
   * — the Maz engine included, which is the point of keeping it browser-free. */
  el.dlReel.addEventListener('click', function () {
    if (!reel) return;
    download(Format.slugify(current.title) + '.reel.json', Reel.toJson(reel), 'application/json');
    say('Saved the reel — ' + reel.shots.length + ' shots, ' + Reel.clock(reel.duration) + ' of film.');
  });

  el.scrubBar.addEventListener('click', function (e) {
    var box = el.scrubBar.getBoundingClientRect();
    scrubTo((e.clientX - box.left) / box.width);
  });
  el.scrubBar.addEventListener('keydown', function (e) {
    if (!reel || !player) return;
    var step = e.key === 'ArrowLeft' ? -5 : e.key === 'ArrowRight' ? 5 : 0;
    if (!step) return;
    e.preventDefault();
    scrubTo((player.time + step) / reel.duration);
  });
  el.filmSize.addEventListener('change', function () {
    if (reel) buildFilm(current);
  });

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

  /* A shared link, opened. Fill the box in as well as building the film, so the
   * person who arrived here can see what it was made from and change it -- a
   * link that produces a film you cannot edit is a video, and there are better
   * ways to send somebody a video. */
  (function openSharedLink() {
    var shared = Library.fromHash(window.location.hash);
    if (!shared) return;
    el.idea.value = shared.idea;
    if (shared.genre && shared.genre !== 'auto') el.genre.value = shared.genre;
    if (shared.length) el.length.value = shared.length;
    if (shared.title) el.titleInput.value = shared.title;
    generate({ seed: shared.seed });
    say('Opened a shared film. Change the sentence to make it yours.');
  }());
})();
