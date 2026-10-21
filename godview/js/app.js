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
  const DEFAULT_ON = { quakes: true };

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
      .ringRepeatPeriod(1400);

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

  function redraw() {
    const on = state.layers.filter((l) => state.enabled[l.id]);
    state.globe.pointsData(on.flatMap((l) => l.points));
    state.globe.ringsData(on.flatMap((l) => l.rings));
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
