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
   'chipSeed', 'tabScript', 'tabShots', 'tabFilm', 'viewScript', 'viewShots', 'viewFilm',
   'copy', 'dlFountain', 'dlFdx', 'dlText', 'dlShots', 'print', 'save', 'status',
   'libraryList', 'libCount', 'libEmpty', 'clearLib', 'filmCanvas', 'bigPlay', 'playFilm',
   'stopFilm', 'recordFilm', 'filmSize', 'speakAloud', 'scrubBar', 'scrubFill', 'filmClock',
   'filmNote'].forEach(function (id) {
    el[id] = document.getElementById(id);
  });

  var Reel = window.FilmReel;
  var PlayerLib = window.FilmPlayer;
  var ScoreLib = window.FilmScore;

  var current = null;  // the script on screen
  var reel = null;     // that script, cut into shots
  var player = null;   // the thing playing it
  var score = null;    // the thing scoring it
  var recording = false;

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
    buildFilm(script);
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
    var tabs = { script: el.tabScript, shots: el.tabShots, film: el.tabFilm };
    var views = { script: el.viewScript, shots: el.viewShots, film: el.viewFilm };
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
    sizeCanvas();
    // A poster frame, so the tab is never a black rectangle.
    PlayerLib.drawFrame(el.filmCanvas.getContext('2d'),
      el.filmCanvas.width, el.filmCanvas.height, reel, 1.2);
    el.bigPlay.classList.remove('hidden');
    updateScrub(0, reel.duration);

    describeRecording();
  }

  /* Say what the download will be *before* anyone sits through a recording.
   * Which format you get is the browser's choice, not ours, and it decides
   * whether the film will play on an iPhone. */
  function describeRecording() {
    var format = PlayerLib.bestFormat();

    if (!format || !PlayerLib.canRecord(el.filmCanvas)) {
      el.recordFilm.disabled = true;
      el.recordFilm.textContent = '⬇ Make the video file';
      el.filmNote.className = 'film-note warn';
      el.filmNote.textContent =
        'This browser can play the film but cannot save it to a video file. ' +
        'Chrome, Edge and Firefox on a computer can — Safari and most phones cannot.';
      return;
    }

    el.recordFilm.disabled = false;
    el.recordFilm.textContent = '⬇ Make the video file (' + format.extension + ')';

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
        score = new ScoreLib.Score(reel);
      } catch (e) {
        score = null; // a film with no sound still plays
      }
    }
    if (score) {
      var scored = score.startScore(reel);
      // The panel carries what it has: which kind of score, and what the
      // music is doing. The film note and the tests both read it.
      el.viewFilm.dataset.score = scored ? 'real' : 'fallback';
      el.viewFilm.dataset.sections = scored && score.player && score.player.song
        ? String(score.player.song.sections.length) : '0';
      if (!scored) say('Could not compose a score in this browser — using simple music.');
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
    if (!resumeAt) say('Playing. ' + Reel.clock(reel.duration) + ' of film.');
  }

  function stopFilm() {
    if (player) {
      if (player.playing) player.stop(false);
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
  el.reroll.addEventListener('click', function () { generate({}); });

  el.idea.addEventListener('keydown', function (e) {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      generate({ fresh: true });
    }
  });

  el.tabScript.addEventListener('click', function () { showTab('script'); });
  el.tabShots.addEventListener('click', function () { showTab('shots'); });
  el.tabFilm.addEventListener('click', function () { showTab('film'); });

  el.playFilm.addEventListener('click', playFilm);
  el.bigPlay.addEventListener('click', playFilm);
  el.stopFilm.addEventListener('click', function () {
    if (recording && player) player.stop(true); // keep the take shot so far
    else stopFilm();
  });
  el.recordFilm.addEventListener('click', recordFilm);

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
})();
