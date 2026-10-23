/*
 * EARTH EXPLORER — the page: the globe, search, saved places, measuring and
 * layers. The maths and the places file live in geo.js (window.EarthGeo).
 *
 * Every map source is free and needs no account or key:
 *   satellite photos  Esri World Imagery
 *   mountains         AWS Terrain Tiles (Terrarium height images)
 *   names, buildings  OpenStreetMap, served as vector tiles by OpenFreeMap
 *   search            OpenStreetMap Nominatim (only when you press search)
 */
(function () {
  'use strict';

  const G = window.EarthGeo;
  const $ = (id) => document.getElementById(id);

  const PLACES_KEY = 'earth-explorer.places.v1';
  const PREFS_KEY = 'earth-explorer.prefs.v1';
  const NOMINATIM = 'https://nominatim.openstreetmap.org';
  const PHONE = window.matchMedia('(max-width: 640px)');
  const REDUCED_MOTION = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ---- Storage (never trusted to exist) ---------------------------------

  function load(key, fallback) {
    try {
      const raw = window.localStorage.getItem(key);
      return raw == null ? fallback : JSON.parse(raw);
    } catch (e) {
      return fallback;
    }
  }

  function save(key, value) {
    try {
      window.localStorage.setItem(key, JSON.stringify(value));
      return true;
    } catch (e) {
      return false;
    }
  }

  const prefs = Object.assign(
    { labels: true, terrain: true, exaggeration: 1.25, buildings: true, spin: true, units: 'metric' },
    load(PREFS_KEY, {})
  );
  if (prefs.units !== 'imperial') prefs.units = 'metric';
  prefs.exaggeration = Number(prefs.exaggeration);
  if (!(prefs.exaggeration >= 1 && prefs.exaggeration <= 3)) prefs.exaggeration = 1.25;

  // First visit: three examples that show off what the map can do. They are
  // marked as examples and can be deleted like any other place.
  const EXAMPLES = [
    { name: 'Mount Everest', lat: 27.9881, lng: 86.925, zoom: 12.5,
      note: 'The highest mountain on Earth, 8,849 m. Tilt the view to see it in 3D.' },
    { name: 'Grand Canyon', lat: 36.0544, lng: -112.1401, zoom: 12.5,
      note: 'About 1.8 km deep. Try measuring across it.' },
    { name: 'Empire State Building', lat: 40.7484, lng: -73.9857, zoom: 16,
      note: 'Zoom into any big city to see its buildings in 3D.' },
  ].map((p) => G.normalizePlace(Object.assign({ example: true }, p)));

  let places = (function () {
    const stored = load(PLACES_KEY, null);
    if (!Array.isArray(stored)) return EXAMPLES.slice();
    return stored.map(G.normalizePlace).filter(Boolean);
  })();

  function storePlaces() {
    if (!save(PLACES_KEY, places)) {
      toast('This browser is not letting the page save. Use “Save to a file” to keep your places.');
    }
  }

  // ---- Small helpers -----------------------------------------------------

  let toastTimer = 0;
  function toast(text) {
    const el = $('toast');
    el.textContent = text;
    el.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { el.hidden = true; }, 3600);
  }

  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text != null) node.textContent = text;
    return node;
  }

  function pinElement(draft) {
    const wrap = el('div', 'pin' + (draft ? ' draft' : ''));
    wrap.innerHTML =
      '<svg viewBox="0 0 30 40" aria-hidden="true"><path d="M15 39C15 39 3 24.5 3 14.5a12 12 0 0 1 24 0C27 24.5 15 39 15 39z"/>' +
      '<circle cx="15" cy="14.5" r="4.2" fill="#2b1d00" stroke="none"/></svg>';
    return wrap;
  }

  // ---- The map ----------------------------------------------------------

  if (!window.maplibregl) {
    document.body.appendChild(el('div', 'noscript',
      'The map could not load. Check your internet connection and reload the page.'));
    return;
  }

  const NAME = ['coalesce', ['get', 'name:en'], ['get', 'name_en'], ['get', 'name']];
  const LABEL_PAINT = {
    'text-color': '#ffffff',
    'text-halo-color': 'rgba(0, 0, 0, 0.8)',
    'text-halo-width': 1.4,
    'text-halo-blur': 0.5,
  };
  const LABEL_LAYERS = ['country-lines', 'road-names', 'water-names', 'place-minor', 'place-city', 'place-state', 'place-country'];

  const style = {
    version: 8,
    projection: { type: 'globe' },
    glyphs: 'https://tiles.openfreemap.org/fonts/{fontstack}/{range}.pbf',
    sky: {
      'sky-color': '#0d2a55',
      'horizon-color': '#9fd2ff',
      'fog-color': '#cfe6ff',
      'sky-horizon-blend': 0.6,
      'horizon-fog-blend': 0.6,
      'fog-ground-blend': 0.9,
      'atmosphere-blend': ['interpolate', ['linear'], ['zoom'], 0, 1, 5, 1, 8, 0],
    },
    sources: {
      satellite: {
        type: 'raster',
        tiles: ['https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}'],
        tileSize: 256,
        maxzoom: 19,
        attribution: 'Imagery © <a href="https://www.esri.com/" target="_blank" rel="noopener">Esri</a>, Maxar, Earthstar Geographics',
      },
      dem: {
        type: 'raster-dem',
        tiles: ['https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png'],
        tileSize: 256,
        maxzoom: 15,
        encoding: 'terrarium',
        attribution: 'Elevation: <a href="https://registry.opendata.aws/terrain-tiles/" target="_blank" rel="noopener">AWS Terrain Tiles</a>',
      },
      osm: {
        type: 'vector',
        url: 'https://tiles.openfreemap.org/planet',
        attribution: '<a href="https://openfreemap.org" target="_blank" rel="noopener">OpenFreeMap</a> © <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a>',
      },
    },
    layers: [
      { id: 'ocean', type: 'background', paint: { 'background-color': '#0b1f3d' } },
      { id: 'satellite', type: 'raster', source: 'satellite', paint: { 'raster-fade-duration': 150 } },
      {
        id: 'buildings', type: 'fill-extrusion', source: 'osm', 'source-layer': 'building', minzoom: 14,
        paint: {
          'fill-extrusion-color': '#e4ddd2',
          'fill-extrusion-height': ['coalesce', ['get', 'render_height'], 8],
          'fill-extrusion-base': ['coalesce', ['get', 'render_min_height'], 0],
          'fill-extrusion-opacity': ['interpolate', ['linear'], ['zoom'], 14, 0, 15, 0.88],
        },
      },
      {
        id: 'country-lines', type: 'line', source: 'osm', 'source-layer': 'boundary',
        filter: ['all', ['==', ['get', 'admin_level'], 2], ['!=', ['get', 'maritime'], 1]],
        paint: {
          'line-color': 'rgba(255, 255, 255, 0.55)',
          'line-width': ['interpolate', ['linear'], ['zoom'], 1, 0.6, 8, 1.6],
          'line-dasharray': [3, 2],
        },
      },
      {
        id: 'road-names', type: 'symbol', source: 'osm', 'source-layer': 'transportation_name', minzoom: 13,
        layout: {
          'symbol-placement': 'line', 'text-field': NAME, 'text-font': ['Noto Sans Regular'],
          'text-size': 12, 'text-max-angle': 30,
        },
        paint: LABEL_PAINT,
      },
      {
        id: 'water-names', type: 'symbol', source: 'osm', 'source-layer': 'water_name',
        filter: ['==', ['geometry-type'], 'Point'],
        layout: { 'text-field': NAME, 'text-font': ['Noto Sans Italic'], 'text-size': 13, 'text-letter-spacing': 0.1, 'text-max-width': 6 },
        paint: { 'text-color': '#bfe3ff', 'text-halo-color': 'rgba(0, 20, 50, 0.7)', 'text-halo-width': 1.2 },
      },
      {
        id: 'place-minor', type: 'symbol', source: 'osm', 'source-layer': 'place', minzoom: 10,
        filter: ['in', ['get', 'class'], ['literal', ['village', 'suburb', 'neighbourhood', 'hamlet', 'quarter']]],
        layout: { 'text-field': NAME, 'text-font': ['Noto Sans Regular'], 'text-size': 12, 'text-max-width': 8 },
        paint: LABEL_PAINT,
      },
      {
        id: 'place-city', type: 'symbol', source: 'osm', 'source-layer': 'place', minzoom: 3,
        filter: ['in', ['get', 'class'], ['literal', ['city', 'town']]],
        layout: {
          'text-field': NAME, 'text-font': ['Noto Sans Bold'], 'text-max-width': 8,
          'text-size': ['interpolate', ['linear'], ['zoom'], 4, 11, 10, 16],
          'symbol-sort-key': ['get', 'rank'],
        },
        paint: LABEL_PAINT,
      },
      {
        id: 'place-state', type: 'symbol', source: 'osm', 'source-layer': 'place', minzoom: 4, maxzoom: 8,
        filter: ['==', ['get', 'class'], 'state'],
        layout: { 'text-field': NAME, 'text-font': ['Noto Sans Regular'], 'text-size': 11, 'text-transform': 'uppercase', 'text-letter-spacing': 0.12 },
        paint: Object.assign({}, LABEL_PAINT, { 'text-color': 'rgba(255, 255, 255, 0.8)' }),
      },
      {
        id: 'place-country', type: 'symbol', source: 'osm', 'source-layer': 'place', maxzoom: 7,
        filter: ['==', ['get', 'class'], 'country'],
        layout: {
          'text-field': NAME, 'text-font': ['Noto Sans Bold'], 'text-max-width': 7,
          'text-size': ['interpolate', ['linear'], ['zoom'], 1, 11, 5, 16],
          'text-transform': 'uppercase', 'text-letter-spacing': 0.08,
        },
        paint: LABEL_PAINT,
      },
    ],
  };

  let map;
  try {
    map = new maplibregl.Map({
      container: 'map',
      style,
      center: [-30, 25],
      zoom: PHONE.matches ? 0.9 : 1.6,
      hash: true, // the view is kept in the address, so a copied link opens right here
      maxPitch: 80,
      // Phones like the iPhone draw at 3× the screen's size; 2× looks the same
      // and needs less than half the graphics memory.
      pixelRatio: Math.min(window.devicePixelRatio || 1, 2),
      attributionControl: { compact: true },
    });
  } catch (e) {
    document.body.appendChild(el('div', 'noscript',
      'This browser cannot draw the 3D map (it needs WebGL). Try an up-to-date Chrome, Edge, Firefox or Safari.'));
    return;
  }

  map.addControl(new maplibregl.NavigationControl({ visualizePitch: true }), 'bottom-right');
  const scale = new maplibregl.ScaleControl({ unit: prefs.units, maxWidth: 110 });
  map.addControl(scale, 'bottom-right');

  let ready = false;     // the style has loaded and the layers can be changed
  let warnedOffline = false;

  map.on('webglcontextlost', () => {
    toast('The map ran out of graphics memory. Reload the page to carry on where you were.');
  });

  map.on('error', (e) => {
    // A missing tile is normal at the edges of what a source covers; only say
    // something when a whole source cannot be reached.
    if (e && e.sourceId && e.error && /Failed to fetch|NetworkError/i.test(String(e.error.message))) {
      if (!warnedOffline) {
        warnedOffline = true;
        toast('Some of the map could not load. Check your internet connection.');
      }
    }
  });

  // ---- Spinning globe -------------------------------------------------

  // The globe is turned a little on every animation frame, never by chaining
  // easeTo() from moveend: when a pinch or a fly-to stops a running ease,
  // MapLibre fires moveend from inside stop(), and starting a new ease there
  // breaks its animation state and freezes the map for good.
  const SPIN_MAX_ZOOM = 3.5;
  const SPIN_DEG_PER_SEC = 4;
  const SPIN_PAUSE_MS = 3000;
  let interacting = false;  // a finger or mouse button is down on the map
  let resumeAt = 0;         // no spinning before this time
  let spinJump = false;     // the move happening now is our own spin
  let lastFrame = 0;
  let real3dOpen = false;   // the real 3D view covers the map

  function pauseSpin() { resumeAt = performance.now() + SPIN_PAUSE_MS; }

  function spinFrame(now) {
    window.requestAnimationFrame(spinFrame);
    const dt = lastFrame ? Math.min(100, now - lastFrame) : 0;
    lastFrame = now;
    if (!ready || !prefs.spin || REDUCED_MOTION || interacting || real3dOpen || now < resumeAt) return;
    if (map.isMoving() || map.getZoom() > SPIN_MAX_ZOOM) return;
    const c = map.getCenter();
    spinJump = true;
    map.jumpTo({ center: [c.lng - (SPIN_DEG_PER_SEC * dt) / 1000, c.lat] });
    spinJump = false;
  }

  ['mousedown', 'touchstart'].forEach((ev) => map.on(ev, () => { interacting = true; pauseSpin(); }));
  ['mouseup', 'touchend', 'touchcancel'].forEach((ev) => map.on(ev, () => { interacting = false; pauseSpin(); }));
  map.on('wheel', pauseSpin);
  map.on('movestart', () => { if (!spinJump) pauseSpin(); });
  map.on('moveend', () => { if (!spinJump) pauseSpin(); });

  // ---- Panels ---------------------------------------------------------

  let openPanel = null;

  function showPanel(name) {
    openPanel = name;
    $('panel').hidden = false;
    document.querySelectorAll('.panel-body').forEach((s) => { s.hidden = s.dataset.for !== name; });
    document.querySelectorAll('.tool[data-panel]').forEach((b) => {
      b.setAttribute('aria-pressed', String(b.dataset.panel === name));
    });
    if (name !== 'places') setPinMode(false);
    updateCursor();
  }

  function hidePanel() {
    openPanel = null;
    $('panel').hidden = true;
    document.querySelectorAll('.tool[data-panel]').forEach((b) => b.setAttribute('aria-pressed', 'false'));
    setPinMode(false);
    updateCursor();
  }

  document.querySelectorAll('.tool[data-panel]').forEach((b) => {
    b.addEventListener('click', () => {
      if (openPanel === b.dataset.panel) hidePanel();
      else showPanel(b.dataset.panel);
    });
  });
  document.querySelectorAll('[data-close]').forEach((b) => b.addEventListener('click', hidePanel));

  function updateCursor() {
    const placing = pinMode || openPanel === 'measure';
    map.getCanvas().style.cursor = placing ? 'crosshair' : '';
  }

  // ---- Search -----------------------------------------------------------

  let searchMarker = null;
  let searchAbort = null;

  function clearResults() {
    $('results').hidden = true;
    $('results').textContent = '';
  }

  function showSearchMarker(lng, lat, name) {
    if (searchMarker) searchMarker.remove();
    const pop = new maplibregl.Popup({ offset: 14, closeButton: true });
    const box = el('div');
    box.appendChild(el('p', 'pop-name', name));
    box.appendChild(el('p', 'pop-coord mono', G.formatCoord(lat, lng)));
    const saveBtn = el('button', 'btn primary', 'Save this place');
    saveBtn.type = 'button';
    saveBtn.style.marginTop = '8px';
    saveBtn.addEventListener('click', () => {
      pop.remove();
      showPanel('places');
      startDraft(lng, lat, name);
    });
    box.appendChild(saveBtn);
    pop.setDOMContent(box);
    const dot = el('div', 'me-dot');
    dot.style.background = 'var(--pin)';
    searchMarker = new maplibregl.Marker({ element: dot }).setLngLat([lng, lat]).setPopup(pop).addTo(map);
    searchMarker.togglePopup();
  }

  function goToResult(r) {
    clearResults();
    const lat = parseFloat(r.lat);
    const lng = parseFloat(r.lon);
    const parts = String(r.display_name || '').split(',');
    const name = r.name || parts[0] || 'Search result';
    const bb = r.boundingbox && r.boundingbox.map(parseFloat);
    if (bb && bb.every(Number.isFinite) && (bb[1] - bb[0] > 0.002 || bb[3] - bb[2] > 0.002)) {
      map.fitBounds([[bb[2], bb[0]], [bb[3], bb[1]]], { padding: 60, maxZoom: 16.5, duration: 3000 });
    } else {
      map.flyTo({ center: [lng, lat], zoom: 17, pitch: 55, duration: 3000, essential: true });
    }
    showSearchMarker(lng, lat, name);
  }

  function renderResults(list) {
    const ul = $('results');
    ul.textContent = '';
    if (!list.length) {
      ul.appendChild(el('li', 'r-none', 'Nothing found. Try adding a country or city.'));
    }
    for (const r of list) {
      const li = el('li');
      const b = el('button');
      b.type = 'button';
      b.setAttribute('role', 'option');
      const parts = String(r.display_name || '').split(',').map((s) => s.trim());
      const name = r.name || parts[0];
      b.appendChild(el('span', 'r-name', name));
      b.appendChild(el('span', 'r-where', parts.filter((p) => p !== name).join(', ')));
      b.addEventListener('click', () => goToResult(r));
      li.appendChild(b);
      ul.appendChild(li);
    }
    ul.hidden = false;
  }

  $('search').addEventListener('submit', async (e) => {
    e.preventDefault();
    const q = $('search-input').value.trim();
    if (!q) return;
    $('search-input').blur();
    const coords = G.parseCoordinates(q);
    if (coords) {
      clearResults();
      map.flyTo({ center: [coords.lng, coords.lat], zoom: 15, duration: 3000, essential: true });
      showSearchMarker(coords.lng, coords.lat, G.formatCoord(coords.lat, coords.lng));
      return;
    }
    if (searchAbort) searchAbort.abort();
    searchAbort = new AbortController();
    const ul = $('results');
    ul.textContent = '';
    ul.appendChild(el('li', 'r-none', 'Searching…'));
    ul.hidden = false;
    try {
      const url = NOMINATIM + '/search?format=jsonv2&limit=6&q=' + encodeURIComponent(q);
      const res = await fetch(url, {
        signal: searchAbort.signal,
        headers: { 'Accept-Language': navigator.language || 'en' },
      });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      renderResults(await res.json());
    } catch (err) {
      if (err.name === 'AbortError') return;
      ul.textContent = '';
      ul.appendChild(el('li', 'r-none', 'Search is not answering right now. Check your connection and try again.'));
    }
  });

  document.addEventListener('keydown', (e) => {
    if (e.key !== 'Escape') return;
    if (real3dOpen) closeReal3d();
    else if (!$('results').hidden) clearResults();
    else if (pinMode) setPinMode(false);
    else if (openPanel) hidePanel();
  });
  map.on('click', clearResults);

  // ---- My places --------------------------------------------------------

  let pinMode = false;
  let draft = null;        // { lng, lat, marker, editingId }
  let markers = [];
  let pendingDelete = null;

  function setPinMode(on) {
    pinMode = on;
    const b = $('drop-pin');
    b.setAttribute('aria-pressed', String(on));
    b.textContent = on ? 'Tap the map…' : 'Drop a pin';
    const hint = $('pin-hint');
    hint.classList.toggle('active', on);
    hint.innerHTML = on
      ? 'Tap the map where the pin should go.'
      : 'Tap <b>Drop a pin</b>, then tap the map.<span class="mouse-only"> Right-clicking the map drops one too.</span>';
    updateCursor();
  }

  function flyToPlace(p) {
    map.flyTo({ center: [p.lng, p.lat], zoom: p.zoom == null ? 15 : p.zoom, pitch: 60, duration: 3500, essential: true });
    if (PHONE.matches) hidePanel();
  }

  function popupFor(p) {
    const pop = new maplibregl.Popup({ offset: 32, closeButton: true });
    const box = el('div');
    box.appendChild(el('p', 'pop-name', p.name));
    if (p.note) box.appendChild(el('p', 'pop-note', p.note));
    box.appendChild(el('p', 'pop-coord mono', G.formatCoord(p.lat, p.lng)));
    const row = el('div', 'row');
    row.style.marginTop = '8px';
    const edit = el('button', 'btn', 'Edit');
    edit.type = 'button';
    edit.addEventListener('click', () => { pop.remove(); showPanel('places'); startEdit(p.id); });
    row.appendChild(edit);
    box.appendChild(row);
    pop.setDOMContent(box);
    return pop;
  }

  function renderPlaces() {
    markers.forEach((m) => m.remove());
    markers = places.map((p) =>
      new maplibregl.Marker({ element: pinElement(false), anchor: 'bottom' })
        .setLngLat([p.lng, p.lat]).setPopup(popupFor(p)).addTo(map));

    const ul = $('place-list');
    ul.textContent = '';
    $('place-empty').hidden = places.length > 0;
    for (const p of places) {
      const li = el('li', 'place');
      li.appendChild(el('span', 'place-dot'));
      const go = el('button', 'place-go');
      go.type = 'button';
      const name = el('span', 'place-name', p.name);
      if (p.example) name.appendChild(el('span', 'tag', 'Example'));
      go.appendChild(name);
      go.appendChild(el('span', 'place-meta', p.note ? p.note : G.formatCoord(p.lat, p.lng)));
      go.title = 'Fly to ' + p.name;
      go.addEventListener('click', () => flyToPlace(p));
      li.appendChild(go);

      const actions = el('span', 'place-actions');
      const edit = el('button', '', 'Edit');
      edit.type = 'button';
      edit.setAttribute('aria-label', 'Edit ' + p.name);
      edit.addEventListener('click', () => startEdit(p.id));
      const del = el('button', pendingDelete === p.id ? 'confirm' : '', pendingDelete === p.id ? 'Sure?' : 'Delete');
      del.type = 'button';
      del.setAttribute('aria-label', 'Delete ' + p.name);
      del.addEventListener('click', () => askDelete(p.id));
      actions.appendChild(edit);
      actions.appendChild(del);
      li.appendChild(actions);
      ul.appendChild(li);
    }
  }

  let deleteTimer = 0;
  function askDelete(id) {
    if (pendingDelete === id) {
      const gone = places.find((p) => p.id === id);
      places = places.filter((p) => p.id !== id);
      pendingDelete = null;
      storePlaces();
      renderPlaces();
      if (gone) toast('Deleted ' + gone.name);
      return;
    }
    pendingDelete = id;
    renderPlaces();
    clearTimeout(deleteTimer);
    deleteTimer = setTimeout(() => { pendingDelete = null; renderPlaces(); }, 3000);
  }

  function openForm(lng, lat, name, note) {
    $('place-form').hidden = false;
    $('place-form-coord').textContent = G.formatCoord(lat, lng);
    $('place-name').value = name || '';
    $('place-note').value = note || '';
    $('place-name').dataset.touched = name ? '1' : '';
    if (!PHONE.matches) $('place-name').focus();
  }

  function startDraft(lng, lat, name) {
    cancelDraft();
    setPinMode(false);
    const marker = new maplibregl.Marker({ element: pinElement(true), anchor: 'bottom', draggable: true })
      .setLngLat([lng, lat]).addTo(map);
    draft = { lng, lat, marker, editingId: null };
    marker.on('dragend', () => {
      const ll = marker.getLngLat();
      draft.lng = ll.lng;
      draft.lat = ll.lat;
      $('place-form-coord').textContent = G.formatCoord(ll.lat, ll.lng);
    });
    openForm(lng, lat, name, '');
    if (!name) suggestName(lng, lat);
  }

  function startEdit(id) {
    const p = places.find((q) => q.id === id);
    if (!p) return;
    cancelDraft();
    draft = { lng: p.lng, lat: p.lat, marker: null, editingId: id };
    openForm(p.lng, p.lat, p.name, p.note);
  }

  function cancelDraft() {
    if (draft && draft.marker) draft.marker.remove();
    draft = null;
    $('place-form').hidden = true;
  }

  // Asks OpenStreetMap what is at the pin, to suggest a name. The suggestion
  // never replaces anything the person has typed.
  async function suggestName(lng, lat) {
    const forDraft = draft;
    try {
      const url = NOMINATIM + '/reverse?format=jsonv2&zoom=17&lat=' + lat + '&lon=' + lng;
      const res = await fetch(url, { headers: { 'Accept-Language': navigator.language || 'en' } });
      if (!res.ok) return;
      const r = await res.json();
      const guess = r.name || (r.address && (r.address.road || r.address.village || r.address.town || r.address.city)) || '';
      if (guess && draft === forDraft && !$('place-name').dataset.touched && !$('place-name').value) {
        $('place-name').value = guess;
      }
    } catch (e) { /* no suggestion; the person types a name */ }
  }

  $('place-name').addEventListener('input', () => { $('place-name').dataset.touched = '1'; });

  $('place-form').addEventListener('submit', (e) => {
    e.preventDefault();
    if (!draft) return;
    const fields = { name: $('place-name').value, note: $('place-note').value, lat: draft.lat, lng: draft.lng };
    if (draft.editingId) {
      places = places.map((p) => (p.id === draft.editingId
        ? G.normalizePlace(Object.assign({}, p, fields, { example: false })) : p));
      toast('Saved changes');
    } else {
      places.push(G.normalizePlace(Object.assign(fields, { zoom: Math.max(14, Math.round(map.getZoom())) })));
      toast('Saved ' + places[places.length - 1].name);
    }
    cancelDraft();
    storePlaces();
    renderPlaces();
  });

  $('place-cancel').addEventListener('click', cancelDraft);
  $('drop-pin').addEventListener('click', () => setPinMode(!pinMode));
  $('pin-center').addEventListener('click', () => {
    const c = map.getCenter();
    startDraft(c.lng, c.lat);
  });

  map.on('contextmenu', (e) => {
    showPanel('places');
    startDraft(e.lngLat.lng, e.lngLat.lat);
  });

  $('export-places').addEventListener('click', () => {
    const blob = new Blob([G.serializePlaces(places)], { type: 'application/json' });
    const a = el('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'my-places.json';
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
    toast('Saved ' + places.length + (places.length === 1 ? ' place' : ' places') + ' to my-places.json');
  });

  $('import-places').addEventListener('change', (e) => {
    const file = e.target.files && e.target.files[0];
    e.target.value = '';
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const merged = G.mergePlaces(places, G.parsePlaces(String(reader.result)));
        places = merged.places;
        storePlaces();
        renderPlaces();
        toast(merged.added ? 'Added ' + merged.added + (merged.added === 1 ? ' place' : ' places')
          : 'Those places are already saved');
      } catch (err) {
        toast(err.message);
      }
    };
    reader.onerror = () => toast('That file could not be read.');
    reader.readAsText(file);
  });

  // ---- Measure ----------------------------------------------------------

  const measure = { mode: 'distance', points: [] };

  function measureData() {
    const pts = measure.points;
    const features = pts.map((p) => ({ type: 'Feature', properties: {}, geometry: { type: 'Point', coordinates: p } }));
    const ring = measure.mode === 'area' && pts.length >= 3 ? pts.concat([pts[0]]) : pts;
    if (ring.length >= 2) {
      features.push({ type: 'Feature', properties: {}, geometry: { type: 'LineString', coordinates: G.densify(ring) } });
    }
    if (measure.mode === 'area' && pts.length >= 3) {
      features.push({ type: 'Feature', properties: {}, geometry: { type: 'Polygon', coordinates: [G.densify(ring)] } });
    }
    return { type: 'FeatureCollection', features };
  }

  function renderMeasure() {
    const src = map.getSource('measure');
    if (src) src.setData(measureData());
    const pts = measure.points;
    const n = pts.length + (pts.length === 1 ? ' point' : ' points');
    if (measure.mode === 'area') {
      $('measure-label').textContent = 'Area';
      $('measure-value').textContent = G.formatArea(G.polygonArea(pts), prefs.units);
      const perimeter = pts.length >= 3 ? G.pathLength(pts.concat([pts[0]])) : G.pathLength(pts);
      $('measure-sub').textContent = 'Around the edge ' + G.formatDistance(perimeter, prefs.units) + ' · ' + n;
      $('measure-hint').textContent = 'Tap the map round the edge of the area. It closes itself.';
    } else {
      $('measure-label').textContent = 'Distance';
      $('measure-value').textContent = G.formatDistance(G.pathLength(pts), prefs.units);
      $('measure-sub').textContent = n;
      $('measure-hint').textContent = 'Tap the map to add points. Lines follow the curve of the Earth.';
    }
  }

  document.querySelectorAll('[data-mode]').forEach((b) => b.addEventListener('click', () => {
    measure.mode = b.dataset.mode;
    document.querySelectorAll('[data-mode]').forEach((o) => o.setAttribute('aria-checked', String(o === b)));
    renderMeasure();
  }));
  $('measure-undo').addEventListener('click', () => { measure.points.pop(); renderMeasure(); });
  $('measure-clear').addEventListener('click', () => { measure.points = []; renderMeasure(); });

  function addMeasureLayers() {
    map.addSource('measure', { type: 'geojson', data: measureData() });
    map.addLayer({
      id: 'measure-fill', type: 'fill', source: 'measure', filter: ['==', ['geometry-type'], 'Polygon'],
      paint: { 'fill-color': '#8fd0ff', 'fill-opacity': 0.2 },
    });
    map.addLayer({
      id: 'measure-line', type: 'line', source: 'measure', filter: ['==', ['geometry-type'], 'LineString'],
      layout: { 'line-cap': 'round', 'line-join': 'round' },
      paint: { 'line-color': '#8fd0ff', 'line-width': 3 },
    });
    map.addLayer({
      id: 'measure-points', type: 'circle', source: 'measure', filter: ['==', ['geometry-type'], 'Point'],
      paint: { 'circle-radius': 5, 'circle-color': '#ffffff', 'circle-stroke-color': '#2a8fd6', 'circle-stroke-width': 2 },
    });
  }

  // ---- Map clicks -------------------------------------------------------

  map.on('click', (e) => {
    if (pinMode) {
      startDraft(e.lngLat.lng, e.lngLat.lat);
    } else if (openPanel === 'measure') {
      measure.points.push([e.lngLat.lng, e.lngLat.lat]);
      renderMeasure();
    }
  });

  // ---- Layers -----------------------------------------------------------

  function applyLayers() {
    if (!ready) return;
    const vis = (on) => (on ? 'visible' : 'none');
    LABEL_LAYERS.forEach((id) => map.getLayer(id) && map.setLayoutProperty(id, 'visibility', vis(prefs.labels)));
    if (map.getLayer('buildings')) map.setLayoutProperty('buildings', 'visibility', vis(prefs.buildings));
    map.setTerrain(prefs.terrain ? { source: 'dem', exaggeration: prefs.exaggeration } : null);
    $('terrain-x').disabled = !prefs.terrain;
    $('terrain-x-out').textContent = prefs.exaggeration + '×';
    if (scale) scale.setUnit(prefs.units);
    document.querySelectorAll('[data-units]').forEach((b) =>
      b.setAttribute('aria-checked', String(b.dataset.units === prefs.units)));
    renderMeasure();
    save(PREFS_KEY, prefs);
  }

  $('layer-labels').checked = prefs.labels;
  $('layer-terrain').checked = prefs.terrain;
  $('layer-buildings').checked = prefs.buildings;
  $('layer-spin').checked = prefs.spin;
  $('terrain-x').value = String(prefs.exaggeration);

  $('layer-labels').addEventListener('change', (e) => { prefs.labels = e.target.checked; applyLayers(); });
  $('layer-terrain').addEventListener('change', (e) => { prefs.terrain = e.target.checked; applyLayers(); });
  $('layer-buildings').addEventListener('change', (e) => { prefs.buildings = e.target.checked; applyLayers(); });
  $('layer-spin').addEventListener('change', (e) => { prefs.spin = e.target.checked; save(PREFS_KEY, prefs); });
  $('terrain-x').addEventListener('input', (e) => { prefs.exaggeration = Number(e.target.value); applyLayers(); });
  document.querySelectorAll('[data-units]').forEach((b) => b.addEventListener('click', () => {
    prefs.units = b.dataset.units;
    applyLayers();
  }));

  $('copy-link').addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(location.href);
      toast('Link copied. Anyone who opens it lands on this view.');
    } catch (e) {
      toast('Copy the address from your browser’s address bar to share this view.');
    }
  });

  // ---- Tilt and where am I ---------------------------------------------

  function updateTilt() {
    $('tilt-label').textContent = map.getPitch() > 10 ? '2D' : '3D';
    $('tilt').title = map.getPitch() > 10 ? 'Look straight down' : 'Tilt to see 3D';
  }
  $('tilt').addEventListener('click', () => {
    if (map.getPitch() > 10) map.easeTo({ pitch: 0, bearing: 0, duration: 1200 });
    else map.easeTo({ pitch: 65, zoom: Math.max(map.getZoom(), 3), duration: 1200 });
  });
  map.on('pitchend', updateTilt);

  let meMarker = null;
  $('locate').addEventListener('click', () => {
    if (!navigator.geolocation) {
      toast('This browser cannot tell the map where you are.');
      return;
    }
    toast('Finding you…');
    navigator.geolocation.getCurrentPosition((pos) => {
      const ll = [pos.coords.longitude, pos.coords.latitude];
      if (meMarker) meMarker.remove();
      meMarker = new maplibregl.Marker({ element: el('div', 'me-dot') }).setLngLat(ll).addTo(map);
      map.flyTo({ center: ll, zoom: 16, pitch: 50, duration: 3500, essential: true });
      $('toast').hidden = true;
    }, (err) => {
      toast(err.code === 1
        ? 'Location is blocked for this page. Allow it in your browser settings, then try again.'
        : 'Your location could not be found. Try again in a moment.');
    }, { enableHighAccuracy: true, timeout: 15000, maximumAge: 60000 });
  });

  // ---- Status bar -------------------------------------------------------

  let lastHover = 0;
  function showStatus(lngLat, prefix) {
    $('status-coord').textContent = (prefix || '') + G.formatCoord(lngLat.lat, G.wrapLng(lngLat.lng));
    let elev = '';
    if (prefs.terrain && map.getZoom() > 6) {
      const h = map.queryTerrainElevation(lngLat);
      if (h != null) elev = G.formatElevation(h / prefs.exaggeration, prefs.units) + ' high';
    }
    $('status-elev').textContent = elev;
  }
  map.on('mousemove', (e) => { lastHover = Date.now(); showStatus(e.lngLat); });
  map.on('moveend', () => { if (Date.now() - lastHover > 1500) showStatus(map.getCenter(), 'Middle: '); });

  // ---- Real 3D cities -------------------------------------------------
  //
  // Google's Photorealistic 3D Tiles, drawn by CesiumJS in a view of its own
  // laid over the map rather than mixed into it: Google's terms limit showing
  // its tiles together with other map data. CesiumJS is big, so
  // it is only downloaded the first time someone opens the view. The key is
  // the person's own, kept in this browser only.

  const CESIUM_BASE = 'https://cdn.jsdelivr.net/npm/cesium@1.145.0/Build/Cesium/';
  const KEY_STORE = 'earth-explorer.google-key.v1';
  const GOOGLE_KEY_SHAPE = /^[A-Za-z0-9_-]{30,60}$/;
  let googleKey = String(load(KEY_STORE, '') || '');
  let cesiumLoading = null;
  let viewer = null;
  let real3dSession = 0;    // tells a slow load that its view was closed
  let statusTimer = 0;

  function loadCesium() {
    if (window.Cesium) return Promise.resolve(window.Cesium);
    if (cesiumLoading) return cesiumLoading;
    window.CESIUM_BASE_URL = CESIUM_BASE;
    cesiumLoading = new Promise((resolve, reject) => {
      const css = el('link');
      css.rel = 'stylesheet';
      css.href = CESIUM_BASE + 'Widgets/widgets.css';
      document.head.appendChild(css);
      const script = el('script');
      script.src = CESIUM_BASE + 'Cesium.js';
      script.onload = () => (window.Cesium ? resolve(window.Cesium) : reject(new Error('Cesium missing')));
      script.onerror = () => {
        cesiumLoading = null;
        script.remove();
        css.remove();
        reject(new Error('Cesium did not load'));
      };
      document.head.appendChild(script);
    });
    return cesiumLoading;
  }

  function setReal3dStatus(text, linger) {
    clearTimeout(statusTimer);
    $('real3d-status').textContent = text || '';
    if (text && linger) statusTimer = setTimeout(() => { $('real3d-status').textContent = ''; }, linger);
  }

  function showChangeKey(show) {
    let b = $('real3d-change');
    if (!b && show) {
      b = el('button', 'btn', 'Change key');
      b.type = 'button';
      b.id = 'real3d-change';
      b.addEventListener('click', () => { closeReal3d(); openReal3dSetup(); });
      $('real3d-close').after(b);
    }
    if (b) b.hidden = !show;
  }

  function openReal3dSetup() {
    $('real3d-key').value = googleKey;
    $('real3d-forget').hidden = !googleKey;
    $('real3d-site').textContent = (location.host || 'mazoo85.github.io') + '/*';
    showPanel('real3d');
  }

  // What the map is looking at now, in a form the 3D camera can use.
  function mapView() {
    const c = map.getCenter();
    let ground = 0;
    if (prefs.terrain) {
      const h = map.queryTerrainElevation(c);
      if (h != null) ground = h / prefs.exaggeration;
    }
    return {
      lng: G.wrapLng(c.lng), lat: c.lat, zoom: map.getZoom(), pitch: map.getPitch(),
      bearing: map.getBearing(), ground, height: map.getCanvas().clientHeight || window.innerHeight,
    };
  }

  function aimCamera(Cesium, v) {
    const target = Cesium.Cartesian3.fromDegrees(v.lng, v.lat, v.ground);
    const range = G.cameraRangeForZoom(v.lat, v.zoom, v.height);
    viewer.camera.lookAt(target, new Cesium.HeadingPitchRange(
      Cesium.Math.toRadians(v.bearing), Cesium.Math.toRadians(G.viewPitchFromMap(v.pitch)), range));
    viewer.camera.lookAtTransform(Cesium.Matrix4.IDENTITY); // free the camera again
  }

  // The reverse: where the 3D camera is looking, as a map view.
  function viewFromCamera(Cesium) {
    const cam = viewer.camera;
    const canvas = viewer.scene.canvas;
    const ray = cam.getPickRay(new Cesium.Cartesian2(canvas.clientWidth / 2, canvas.clientHeight / 2));
    const hit = ray && Cesium.IntersectionTests.rayEllipsoid(ray, Cesium.Ellipsoid.WGS84);
    const from = Cesium.Cartographic.fromCartesian(cam.positionWC);
    let lat, lng, range, pitch;
    if (hit) {
      const point = Cesium.Ray.getPoint(ray, hit.start);
      const at = Cesium.Cartographic.fromCartesian(point);
      lat = Cesium.Math.toDegrees(at.latitude);
      lng = Cesium.Math.toDegrees(at.longitude);
      range = Cesium.Cartesian3.distance(cam.positionWC, point);
      pitch = G.mapPitchFromView(Cesium.Math.toDegrees(cam.pitch));
    } else {
      // Looking at the sky: put the map under the camera instead.
      lat = Cesium.Math.toDegrees(from.latitude);
      lng = Cesium.Math.toDegrees(from.longitude);
      range = Math.max(50, from.height);
      pitch = 0;
    }
    return {
      center: [lng, lat],
      zoom: G.zoomForCameraRange(lat, range, map.getCanvas().clientHeight || window.innerHeight),
      pitch,
      bearing: G.bearingFromHeading(Cesium.Math.toDegrees(cam.heading)),
    };
  }

  function keyProblem(err) {
    const code = err && (err.statusCode || (err.error && err.error.statusCode));
    if (code === 400 || code === 401 || code === 403) {
      return 'Google turned the key down. Check that the Map Tiles API is enabled, billing is set up, and the key allows this website.';
    }
    if (code === 429) return 'Google says the key has used up its limit for today. Try again tomorrow, or raise the limit.';
    return 'The 3D city models could not load. Check your internet connection and try again.';
  }

  async function openReal3d() {
    if (!googleKey) {
      openReal3dSetup();
      return;
    }
    hidePanel();
    clearResults();
    const start = mapView();
    const session = ++real3dSession;
    real3dOpen = true;
    $('real3d').hidden = false;
    showChangeKey(false);
    setReal3dStatus('Loading the 3D view…');

    let Cesium;
    try {
      Cesium = await loadCesium();
    } catch (e) {
      if (session === real3dSession) setReal3dStatus('The 3D view could not load. Check your internet connection and try again.');
      return;
    }
    if (session !== real3dSession) return;

    try {
      viewer = new Cesium.Viewer('real3d-view', {
        globe: false,            // Google's tiles are the whole Earth, ground included
        baseLayer: false,
        baseLayerPicker: false, geocoder: false, homeButton: false, sceneModePicker: false,
        navigationHelpButton: false, animation: false, timeline: false, fullscreenButton: false,
        infoBox: false, selectionIndicator: false,
        requestRenderMode: true, // only draw when something changes: easier on phone batteries
        useBrowserRecommendedResolution: PHONE.matches,
      });
    } catch (e) {
      setReal3dStatus('This browser cannot draw the 3D view (it needs WebGL).');
      return;
    }
    // With no globe, the sky would glow white below the horizon until the
    // city arrives, so it stays off until there is ground to stand on.
    viewer.scene.skyAtmosphere.show = false;
    viewer.scene.backgroundColor = Cesium.Color.fromCssColorString('#070b12');
    aimCamera(Cesium, start);

    try {
      const tileset = await Cesium.createGooglePhotorealistic3DTileset(
        // There is no search inside this view, so no other geocoder is used with it.
        { key: googleKey, onlyUsingWithGoogleGeocoder: true },
        {
          maximumScreenSpaceError: PHONE.matches ? 24 : 16,
          cacheBytes: (PHONE.matches ? 256 : 512) * 1024 * 1024,
        }
      );
      if (session !== real3dSession || !viewer) {
        tileset.destroy();
        return;
      }
      viewer.scene.primitives.add(tileset);
      viewer.scene.skyAtmosphere.show = true;
      viewer.scene.requestRender();
      setReal3dStatus(window.matchMedia('(hover: none)').matches
        ? 'Drag to move, pinch to zoom, drag with two fingers to tilt and turn.'
        : 'Drag to move, scroll to zoom, right-drag or Ctrl-drag to tilt and turn.', 6000);
    } catch (err) {
      if (session !== real3dSession) return;
      setReal3dStatus(keyProblem(err));
      showChangeKey(true);
    }
  }

  function closeReal3d() {
    real3dSession++;
    if (viewer && window.Cesium) {
      try {
        map.jumpTo(viewFromCamera(window.Cesium));
      } catch (e) { /* keep the map where it was */ }
    }
    if (viewer) {
      viewer.destroy();
      viewer = null;
    }
    real3dOpen = false;
    $('real3d').hidden = true;
    setReal3dStatus('');
    showChangeKey(false);
    updateTilt();
  }

  $('real3d-open').addEventListener('click', openReal3d);
  $('real3d-close').addEventListener('click', closeReal3d);
  $('real3d-settings').addEventListener('click', openReal3dSetup);
  $('real3d-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const key = $('real3d-key').value.trim();
    if (!GOOGLE_KEY_SHAPE.test(key)) {
      toast('That doesn’t look like a Google key. It is a long code that starts with AIza.');
      return;
    }
    googleKey = key;
    if (!save(KEY_STORE, key)) toast('This browser won’t keep the key, so you’ll need to paste it again next time.');
    openReal3d();
  });
  $('real3d-forget').addEventListener('click', () => {
    googleKey = '';
    try { window.localStorage.removeItem(KEY_STORE); } catch (e) { /* nothing kept */ }
    $('real3d-key').value = '';
    $('real3d-forget').hidden = true;
    toast('Removed the key from this browser');
  });

  // ---- Install as an app ------------------------------------------------
  // Chrome and Edge offer their own install prompt; this button shows only
  // when they do. On an iPhone it is Safari's Share → Add to Home Screen.

  let installPrompt = null;
  window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault();
    installPrompt = e;
    $('install-app').hidden = false;
  });
  $('install-app').addEventListener('click', async () => {
    if (!installPrompt) return;
    installPrompt.prompt();
    try { await installPrompt.userChoice; } catch (e) { /* closed */ }
    installPrompt = null;
    $('install-app').hidden = true;
  });
  window.addEventListener('appinstalled', () => toast('Earth Explorer is installed. Open it from your apps.'));

  // ---- Start ----------------------------------------------------------

  // 'style.load' rather than 'load': 'load' waits for every first tile, which
  // on a slow phone connection can hold the pins and the spin back for ages.
  map.once('style.load', () => {
    ready = true;
    addMeasureLayers();
    applyLayers();
    renderPlaces();
    updateTilt();
    window.requestAnimationFrame(spinFrame);
  });
  renderMeasure();
})();
