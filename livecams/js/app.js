/*
 * LIVE CAMS — UI layer
 * --------------------
 * Wires the DOM to window.LiveCams. Every pinned stream is a muted, autoplaying
 * YouTube embed in a grid; tapping Enlarge opens it full size, with sound.
 *
 * The only state that outlives a visit is the wall itself (and the chosen
 * size and filter), in localStorage — nothing leaves the browser. Names come
 * from the viewer, so they only ever reach the page as textContent.
 */
(function () {
  'use strict';

  var L = window.LiveCams;
  var SAVE_KEY = 'livecams.wall.v1';
  var PREFS_KEY = 'livecams.prefs.v1';

  var el = {
    places: document.getElementById('places'),
    form: document.getElementById('addForm'),
    link: document.getElementById('link'),
    name: document.getElementById('name'),
    area: document.getElementById('area'),
    message: document.getElementById('message'),
    filter: document.getElementById('filter'),
    cols: document.getElementById('cols'),
    wall: document.getElementById('wall'),
    empty: document.getElementById('empty'),
    big: document.getElementById('big'),
    bigName: document.getElementById('bigName'),
    bigFrame: document.getElementById('bigFrame'),
    bigClose: document.getElementById('bigClose')
  };

  function load(key) {
    try { return window.localStorage.getItem(key); } catch (e) { return null; }
  }

  function store(key, value) {
    try { window.localStorage.setItem(key, value); } catch (e) { /* private mode: the wall lasts this visit */ }
  }

  var wall = L.deserialize(load(SAVE_KEY) || '');
  var prefs = {};
  try { prefs = JSON.parse(load(PREFS_KEY) || '{}') || {}; } catch (e) { prefs = {}; }

  function savePrefs() {
    store(PREFS_KEY, JSON.stringify({ filter: el.filter.value, cols: el.cols.value }));
  }

  function make(tag, cls, text) {
    var n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text != null) n.textContent = text;
    return n;
  }

  function option(select, value, label) {
    var o = make('option', null, label || value);
    o.value = value;
    select.appendChild(o);
  }

  function say(text, bad) {
    el.message.textContent = text;
    el.message.classList.toggle('bad', !!bad);
  }

  function frameFor(stream, withSound) {
    var src = L.embedUrl(stream);
    if (!src) return null;
    var f = document.createElement('iframe');
    f.src = withSound ? src.replace('mute=1', 'mute=0') : src;
    f.title = stream.name;
    f.loading = 'lazy';
    f.allow = 'autoplay; encrypted-media; picture-in-picture; fullscreen';
    f.allowFullscreen = true;
    f.referrerPolicy = 'strict-origin-when-cross-origin';
    return f;
  }

  function renderPlaces() {
    L.PLACES.forEach(function (p) {
      var group = make('div', 'place');
      group.appendChild(make('h3', null, p.area));
      var row = make('div', 'chips');
      p.cities.forEach(function (city) {
        var a = make('a', 'chip', city);
        a.href = L.searchUrl(city);
        a.target = '_blank';
        a.rel = 'noopener';
        row.appendChild(a);
      });
      group.appendChild(row);
      el.places.appendChild(group);
    });
  }

  function renderWall() {
    el.wall.textContent = '';
    var shown = L.filterByArea(wall, el.filter.value);
    el.empty.hidden = shown.length > 0;
    if (!shown.length && wall.length) {
      el.empty.textContent = 'Nothing pinned in ' + el.filter.value + ' yet.';
    } else {
      el.empty.textContent = 'Nothing pinned yet. Find a live stream in step 1, copy its link, and paste it in step 2.';
    }

    shown.forEach(function (s) {
      var tile = make('article', 'tile');
      var frame = make('div', 'frame');
      var f = frameFor(s, false);
      if (f) {
        frame.appendChild(f);
      } else {
        var open = make('a', 'webcard', 'Open ' + s.name + ' ↗');
        open.href = s.url;
        open.target = '_blank';
        open.rel = 'noopener';
        frame.appendChild(open);
      }
      tile.appendChild(frame);

      var bar = make('div', 'tilebar');
      var label = make('div', 'label');
      label.appendChild(make('span', 'live', 'LIVE'));
      label.appendChild(make('span', 'tname', s.name));
      label.appendChild(make('span', 'tarea', s.area));
      bar.appendChild(label);

      var actions = make('div', 'actions');
      if (f) {
        var bigBtn = make('button', 'ghost', 'Enlarge');
        bigBtn.type = 'button';
        bigBtn.addEventListener('click', function () { enlarge(s); });
        actions.appendChild(bigBtn);
      }
      var yt = make('a', 'ghost', f ? 'YouTube' : 'Open');
      yt.href = L.watchUrl(s);
      yt.target = '_blank';
      yt.rel = 'noopener';
      actions.appendChild(yt);
      var rm = make('button', 'ghost danger', 'Remove');
      rm.type = 'button';
      rm.setAttribute('aria-label', 'Remove ' + s.name);
      rm.addEventListener('click', function () {
        wall = L.removeStream(wall, L.keyOf(s));
        store(SAVE_KEY, L.serialize(wall));
        say('Removed ' + s.name + '.');
        renderWall();
      });
      actions.appendChild(rm);
      bar.appendChild(actions);

      tile.appendChild(bar);
      el.wall.appendChild(tile);
    });
  }

  function enlarge(s) {
    el.bigName.textContent = s.name + ' · ' + s.area;
    el.bigFrame.textContent = '';
    el.bigFrame.appendChild(frameFor(s, true));
    if (el.big.showModal) el.big.showModal(); else el.big.setAttribute('open', '');
  }

  function closeBig() {
    el.bigFrame.textContent = '';  // stops the stream and its sound
    if (el.big.close) el.big.close(); else el.big.removeAttribute('open');
  }

  function setCols() {
    el.wall.className = 'wall cols-' + el.cols.value;
  }

  // ---- set up ----
  L.AREAS.forEach(function (a) { option(el.area, a); });
  option(el.filter, 'All', 'All areas');
  L.AREAS.forEach(function (a) { option(el.filter, a); });
  if (L.AREAS.indexOf(prefs.filter) >= 0) el.filter.value = prefs.filter;
  if (['1', '2', '3', '4'].indexOf(prefs.cols) >= 0) el.cols.value = prefs.cols;

  renderPlaces();
  setCols();
  renderWall();

  el.form.addEventListener('submit', function (ev) {
    ev.preventDefault();
    var s = L.makeStream(el.link.value, el.name.value, el.area.value);
    if (s.error) { say(s.error, true); el.link.focus(); return; }
    var r = L.addStream(wall, s);
    if (r.error) { say(r.error, true); return; }
    wall = r.list;
    store(SAVE_KEY, L.serialize(wall));
    if (el.filter.value !== 'All' && el.filter.value !== s.area) {
      el.filter.value = 'All';
      savePrefs();
    }
    el.link.value = '';
    el.name.value = '';
    say('Pinned ' + s.name + '.' + (s.kind === 'web' ? " That site can't play inside the wall, so it opens in a new tab." : ''));
    renderWall();
  });

  el.filter.addEventListener('change', function () { savePrefs(); renderWall(); });
  el.cols.addEventListener('change', function () { savePrefs(); setCols(); });
  el.bigClose.addEventListener('click', closeBig);
  el.big.addEventListener('close', function () { el.bigFrame.textContent = ''; });
  el.big.addEventListener('click', function (ev) { if (ev.target === el.big) closeBig(); });
})();
