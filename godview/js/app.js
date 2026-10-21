/*
 * GOD'S EYE VIEW — the page.
 *
 * The only file that touches the DOM or the globe. It boots the globe, draws
 * every enabled layer's points and rings onto it, keeps the layer bar, the
 * clock and the LIVE light up to date, and opens the details sheet on a tap.
 * Layers know nothing about the page: they follow the layer contract in
 * docs/superpowers/plans/2026-10-21-godview-step1.md.
 */
(function () {
  'use strict';

  const F = window.GV_FORMAT;
  const $ = (id) => document.getElementById(id);

  /* Which layers start switched on. */
  const DEFAULT_ON = { quakes: true, sats: true };

  function fail(message) {
    $('fallback-msg').textContent = message;
    $('fallback').hidden = false;
  }

  function hasWebGL() {
    try {
      const c = document.createElement('canvas');
      return !!(c.getContext('webgl2') || c.getContext('webgl'));
    } catch (e) {
      return false;
    }
  }

  async function fetchJson(url) {
    const res = await fetch(url, { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return res.json();
  }

  async function fetchText(url) {
    const res = await fetch(url, { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return res.text();
  }

  /* ------------------------------------------------------------ clock */
  function startClock() {
    const tick = () => { $('clock').textContent = F.utcClock(new Date()); };
    tick();
    setInterval(tick, 1000);
  }

  /* ------------------------------------------------------------ globe */
  function makeGlobe(el) {
    const globe = new Globe(el, { animateIn: false })
      .width(window.innerWidth)
      .height(window.innerHeight)
      .backgroundColor('#000000')
      .showAtmosphere(true)
      .atmosphereColor('#1aff8c')
      .atmosphereAltitude(0.16)
      .showGraticules(true)
      .polygonsData([])
      .polygonCapColor(() => 'rgba(0,0,0,0)')
      .polygonSideColor(() => 'rgba(0,0,0,0)')
      .polygonStrokeColor(() => '#1aff8c')
      .polygonAltitude(0.004)
      .pointsData([])
      .pointLat('lat')
      .pointLng('lng')
      .pointColor('color')
      .pointRadius('radius')
      .pointAltitude('altitude')
      .pointsMerge(false)
      .ringsData([])
      .ringLat('lat')
      .ringLng('lng')
      .ringColor((r) => (t) => F.rgba(r.color, 1 - t))
      .ringMaxRadius('maxRadius')
      .ringPropagationSpeed(2)
      .ringRepeatPeriod(1400)
      .htmlElementsData([])
      .htmlLat('lat')
      .htmlLng('lng')
      .htmlAltitude('altitude')
      .htmlElement(markerFor)
      .htmlTransitionDuration(1000)
      .htmlElementVisibilityModifier((el, visible) => {
        el.classList.toggle('is-hidden', !visible);
      });

    const material = globe.globeMaterial();
    material.color.set('#03140c');
    if (material.emissive) material.emissive.set('#010a05');

    globe.renderer().setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
    globe.pointOfView({ lat: 20, lng: -40, altitude: 2.4 });

    const controls = globe.controls();
    controls.autoRotate = true;
    controls.autoRotateSpeed = 0.35;
    controls.addEventListener('start', () => { controls.autoRotate = false; });

    window.addEventListener('resize', () => {
      globe.width(window.innerWidth).height(window.innerHeight);
    });
    return globe;
  }

  /* ------------------------------------------------------------ markers */
  /* One DOM element per floating marker, made once and reused: the globe
   * moves it, and a tap on it opens the sheet. */
  const markerEls = new WeakMap();
  function markerFor(d) {
    let el = markerEls.get(d);
    if (el) return el;
    el = document.createElement('button');
    el.type = 'button';
    el.className = 'marker';
    el.style.setProperty('--c', d.color);
    el.setAttribute('aria-label', d.item.name || 'marker');
    el.dataset.layer = d.layer;
    if (d.label) {
      const tag = document.createElement('span');
      tag.className = 'marker-label';
      tag.textContent = d.label;
      el.append(tag);
    }
    el.addEventListener('click', (e) => {
      e.stopPropagation();
      openSheet(d);
    });
    markerEls.set(d, el);
    return el;
  }

  /* ------------------------------------------------------------ sheet */
  function openSheet(point) {
    const layer = state.layers.find((l) => l.id === point.layer);
    if (!layer) return;
    const d = layer.describe(point.item);
    $('sheet-title').textContent = d.title;
    const rows = $('sheet-rows');
    rows.textContent = '';
    for (const [k, v] of d.rows) {
      const dt = document.createElement('dt');
      dt.textContent = k;
      const dd = document.createElement('dd');
      dd.textContent = v;
      rows.append(dt, dd);
    }
    const link = $('sheet-link');
    if (d.link) {
      link.href = d.link;
      link.hidden = false;
    } else {
      link.removeAttribute('href');
      link.hidden = true;
    }
    $('sheet').hidden = false;
  }

  function closeSheet() {
    $('sheet').hidden = true;
  }

  /* ------------------------------------------------------------ layers */
  const state = { globe: null, layers: [], enabled: {}, buttons: {} };

  /* Points and rings are only re-set when what is in them changed, so a
   * satellite tick does not restart the quake rings; markers are re-set on
   * every change so the globe glides them to their new positions. */
  function sameItems(a, b) {
    return a.length === b.length && a.every((x, i) => x === b[i]);
  }

  function redraw() {
    const on = state.layers.filter((l) => state.enabled[l.id]);
    const points = on.flatMap((l) => l.points);
    const rings = on.flatMap((l) => l.rings);
    if (!sameItems(points, state.globe.pointsData())) state.globe.pointsData(points);
    if (!sameItems(rings, state.globe.ringsData())) state.globe.ringsData(rings);
    state.globe.htmlElementsData(on.flatMap((l) => l.html || []));
  }

  /* One clock for everything that moves between fetches. Paused while the
   * page is hidden, to save the phone's battery. */
  function startTicker() {
    setInterval(() => {
      if (document.hidden) return;
      for (const l of state.layers) {
        if (state.enabled[l.id] && typeof l.tick === 'function') l.tick();
      }
    }, 1000);
  }

  function paintButton(layer) {
    const btn = state.buttons[layer.id];
    const enabled = !!state.enabled[layer.id];
    btn.setAttribute('aria-pressed', String(enabled));
    btn.classList.toggle('is-offline', enabled && layer.status === 'offline');
    btn.classList.toggle('is-loading', enabled && layer.status === 'loading');
    let val;
    if (!enabled) val = 'OFF';
    else if (layer.status === 'offline') val = 'OFFLINE';
    else if (layer.status === 'loading') val = '…';
    else val = F.count(layer.count);
    btn.querySelector('.val').textContent = val;
  }

  let blinkTimer = null;
  function paintLive(blink) {
    const anyLive = state.layers.some((l) => state.enabled[l.id] && l.status === 'live');
    const live = $('live');
    live.classList.toggle('is-on', anyLive);
    if (blink && anyLive) {
      live.classList.add('is-blink');
      clearTimeout(blinkTimer);
      blinkTimer = setTimeout(() => live.classList.remove('is-blink'), 600);
    }
  }

  function addLayer(layer) {
    state.layers.push(layer);
    state.enabled[layer.id] = !!DEFAULT_ON[layer.id];

    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'layer-btn';
    btn.dataset.layer = layer.id;
    const icon = document.createElement('span');
    icon.className = 'icon';
    icon.textContent = layer.icon;
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = layer.label;
    const val = document.createElement('span');
    val.className = 'val';
    btn.append(icon, name, val);
    btn.addEventListener('click', () => {
      const on = !state.enabled[layer.id];
      state.enabled[layer.id] = on;
      if (on) layer.start();
      else layer.stop();
      paintButton(layer);
      paintLive(false);
      redraw();
    });
    $('layerbar').append(btn);
    state.buttons[layer.id] = btn;

    layer.onChange(() => {
      paintButton(layer);
      paintLive(layer.status === 'live');
      redraw();
    });
    paintButton(layer);
    if (state.enabled[layer.id]) layer.start();
  }

  /* ------------------------------------------------------------ boot */
  function boot() {
    startClock();
    $('sheet-close').addEventListener('click', closeSheet);

    if (typeof window.Globe !== 'function') {
      fail('The globe could not be downloaded. Check your internet connection and reload the page.');
      return;
    }
    if (!hasWebGL()) {
      fail('This browser cannot draw 3D graphics (WebGL is switched off or not supported).');
      return;
    }
    try {
      state.globe = makeGlobe($('globe'));
    } catch (e) {
      fail('The globe could not start: ' + e.message);
      return;
    }
    state.globe.onPointClick(openSheet).onGlobeClick(closeSheet);

    fetchJson('data/countries.geojson')
      .then((geo) => state.globe.polygonsData(geo.features))
      .catch(() => { /* outlines are decoration; the live layers still work */ });

    addLayer(window.GV_QUAKES_LAYER.create({ fetchJson: fetchJson }));
    addLayer(window.GV_SATS_LAYER.create({ fetchText: fetchText }));
    startTicker();

    window.GODVIEW = {
      globe: state.globe,
      layers: state.layers,
      enabled: state.enabled,
      openSheet: openSheet,
      closeSheet: closeSheet
    };
  }

  boot();
})();
