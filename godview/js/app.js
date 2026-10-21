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

  /* Which layers start switched on. Weather is the heaviest download, so it
   * waits to be asked for. */
  const DEFAULT_ON = { quakes: true, sats: true };
  const GLOBE_COLOR = '#03140c';

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

  /* An image's pixels, for layers that work on pictures (weather). The
   * request is anonymous-CORS, so a source that does not allow web pages
   * fails here, cleanly, instead of tainting the canvas. */
  function loadPixels(url) {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      const timer = setTimeout(() => reject(new Error('timed out: ' + url)), 30000);
      img.onload = () => {
        clearTimeout(timer);
        try {
          const c = document.createElement('canvas');
          c.width = img.naturalWidth;
          c.height = img.naturalHeight;
          const ctx = c.getContext('2d');
          ctx.drawImage(img, 0, 0);
          const d = ctx.getImageData(0, 0, c.width, c.height);
          resolve({ width: c.width, height: c.height, data: d.data });
        } catch (e) {
          reject(e);
        }
      };
      img.onerror = () => {
        clearTimeout(timer);
        reject(new Error('image failed: ' + url));
      };
      img.src = url;
    });
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
    material.color.set(GLOBE_COLOR);
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
    const it = d.item;
    el.setAttribute('aria-label', it.name || it.callsign || it.reg || it.hex || 'marker');
    el.dataset.layer = d.layer;
    if (typeof d.rotation === 'number') {
      /* A plane: a small drawn silhouette, nose up, turned to its heading. */
      el.classList.add('marker--plane');
      el.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 2c.8 0 1.3.9 1.3 2v5.2l7.7 4.6v2.1l-7.7-2.3v4.6l2.2 1.7V21L12 20l-3.5 1v-1.1l2.2-1.7v-4.6L3 15.9v-2.1l7.7-4.6V4c0-1.1.5-2 1.3-2z"/></svg>';
    }
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
    const html = on.flatMap((l) => l.html || []);
    for (const d of html) {
      const el = markerEls.get(d);
      if (el && typeof d.rotation === 'number') el.style.setProperty('--rot', d.rotation + 'deg');
    }
    state.globe.htmlElementsData(html);
    paintSurface(on.map((l) => l.texture).find(Boolean) || null);
    paintCredits(on);
  }

  /* A layer's texture becomes the globe's surface; with none, the plain dark
   * globe comes back. The globe library loads an image a moment after it is
   * set and does not clear it when unset, so the surface is cleared here, and
   * once more shortly after in case a load was still landing. */
  let shownTexture = null;
  function clearSurface() {
    const m = state.globe.globeMaterial();
    if (m.map) {
      m.map.dispose();
      m.map = null;
    }
    if (m.color) m.color.set(GLOBE_COLOR);
    m.needsUpdate = true;
  }
  function paintSurface(texture) {
    if (texture === shownTexture) return;
    shownTexture = texture;
    if (texture) {
      const c = document.createElement('canvas');
      c.width = texture.width;
      c.height = texture.height;
      c.getContext('2d').putImageData(new ImageData(texture.data, texture.width, texture.height), 0, 0);
      state.globe.globeImageUrl(c.toDataURL('image/jpeg', 0.9));
    } else {
      state.globe.globeImageUrl(null);
      clearSurface();
      setTimeout(() => { if (!shownTexture) clearSurface(); }, 1500);
    }
  }

  function paintCredits(on) {
    const text = on.map((l) => l.credit).filter(Boolean).join(' · ');
    const el = $('credits');
    el.textContent = text;
    el.hidden = !text;
  }

  /* Layers that only fetch around the view (planes) are asked again as soon
   * as a drag or zoom ends somewhere new, instead of waiting for their timer. */
  function followView() {
    let last = state.globe.pointOfView();
    state.globe.controls().addEventListener('end', () => {
      const v = state.globe.pointOfView();
      if (window.GV_PLANES.distanceKm(last, v) < 220) return;
      last = v;
      for (const l of state.layers) {
        if (l.followsView && state.enabled[l.id]) l.refresh();
      }
    });
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
    else if (layer.summary) val = layer.summary;
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
    addLayer(window.GV_WEATHER_LAYER.create({ fetchJson: fetchJson, loadPixels: loadPixels }));
    addLayer(window.GV_PLANES_LAYER.create({ fetchJson: fetchJson, getView: () => state.globe.pointOfView() }));
    followView();
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
