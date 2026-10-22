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

  let interacting = false;
  const SPIN_MAX_ZOOM = 3.5;

  function spin() {
    if (!prefs.spin || REDUCED_MOTION || interacting || map.getZoom() > SPIN_MAX_ZOOM) return;
    const c = map.getCenter();
    c.lng -= 4; // degrees per second
    map.easeTo({ center: c, duration: 1000, easing: (t) => t });
  }
  ['mousedown', 'touchstart', 'wheel'].forEach((ev) => map.on(ev, () => { interacting = true; }));
  ['mouseup', 'touchend', 'dragend', 'zoomend', 'pitchend', 'rotateend'].forEach((ev) => map.on(ev, () => {
    interacting = false;
    setTimeout(spin, 2500);
  }));
  map.on('moveend', (e) => { if (!e.originalEvent) spin(); });

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
    if (!$('results').hidden) clearResults();
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
  $('layer-spin').addEventListener('change', (e) => { prefs.spin = e.target.checked; save(PREFS_KEY, prefs); spin(); });
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

  // ---- Start ----------------------------------------------------------

  map.on('load', () => {
    ready = true;
    addMeasureLayers();
    applyLayers();
    renderPlaces();
    updateTilt();
    spin();
  });
  renderMeasure();
})();
