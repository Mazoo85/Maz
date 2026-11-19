/*
 * EARTH EXPLORER — live and history layers, from Terra Desk (Cody's desktop globe).
 *
 *   Aurora       NOAA SWPC OVATION forecast (next hour) and the planetary Kp index
 *   Volcanoes    Smithsonian GVP Holocene list (bundled), coloured by USGS alert levels
 *   Battles      8,740 dated battles from Wikidata (CC0, bundled), picked by a span of years
 *   Launches     upcoming rocket launches from The Space Devs Launch Library 2
 *   Old maps     georeferenced historical maps from Allmaps, laid over the place they show
 *
 * Every source answers browsers directly (CORS) and needs no key. Live aircraft, ships and
 * lightning need the desktop program, because their services don't answer browsers.
 *
 * The top half is plain JavaScript with no map and no browser (window.EarthData, and Node for
 * the tests); attach() wires it to the map and the Layers panel.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.EarthData = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // ---- Logic (no browser) ---------------------------------------------

  const key = (s) => String(s || '').toLowerCase().replace(/[^a-z]/g, '');
  const yearText = (y) => (y < 0 ? `${-y} BCE` : String(y));

  // OVATION grid [lon 0..359, lat -90..90, chance %] to 1° squares, skipping chances under `min`.
  function auroraGeoJSON(coords, min = 4) {
    const features = [];
    for (const c of coords || []) {
      const lon = +c[0], lat = +c[1], v = +c[2];
      if (!(v >= min) || Math.abs(lat) > 84) continue;
      const w = (lon > 180 ? lon - 360 : lon) - 0.5, s = lat - 0.5;
      features.push({ type: 'Feature', properties: { v },
        geometry: { type: 'Polygon', coordinates: [[[w, s], [w + 1, s], [w + 1, s + 1], [w, s + 1], [w, s]]] } });
    }
    return { type: 'FeatureCollection', features };
  }

  // The latest planetary Kp from NOAA's table (an array of rows with a header row, or of objects).
  function latestKp(rows) {
    if (!Array.isArray(rows) || !rows.length) return null;
    const last = rows[rows.length - 1];
    const v = Array.isArray(last) ? +last[1] : +(last.Kp ?? last.kp_index ?? last.kp);
    return Number.isFinite(v) ? v : null;
  }

  function kpWords(kp) {
    if (kp == null) return '';
    return kp >= 7 ? 'strong storm: aurora far from the poles' : kp >= 5 ? 'storm: aurora at mid latitudes' : kp >= 4 ? 'active' : 'quiet';
  }

  // GVP volcanoes, with the USGS alert level where the US Geological Survey has raised one.
  const ALERT_ORDER = { WARNING: 3, WATCH: 2, ADVISORY: 1, NORMAL: 0 };
  function volcanoGeoJSON(list, elevated) {
    const usgs = new Map();
    for (const v of elevated || []) usgs.set(key(v.volcano_name), v);
    const features = (list || []).map((d) => {
      const u = usgs.get(key(d.name));
      const alert = u ? String(u.alert_level || 'ADVISORY').toUpperCase() : '';
      return { type: 'Feature', geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
        properties: { name: d.name, country: d.country || '', type: d.type || '', last: d.last == null ? '' : String(d.last),
          elev: d.elev == null ? '' : String(d.elev), alert, rank: alert ? ALERT_ORDER[alert] ?? 1 : -1, color: u ? String(u.color_code || '') : '' } };
    });
    return { type: 'FeatureCollection', features };
  }

  function battleGeoJSON(list) {
    return { type: 'FeatureCollection', features: (list || []).map((d) => ({
      type: 'Feature', geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
      properties: { id: d.id, name: d.name, year: d.year, date: d.date, war: (d.war || []).join(', '), wiki: d.wiki || '',
        s: (d.name + ' ' + (d.war || []).join(' ')).toLowerCase() } })) };
  }

  // The map filter for a span of years and a search, and the same test in JavaScript (for the count).
  function battleFilter(from, to, q) {
    const f = ['all', ['>=', ['get', 'year'], from], ['<=', ['get', 'year'], to]];
    const t = String(q || '').trim().toLowerCase();
    if (t) f.push(['in', t, ['get', 's']]);
    return f;
  }
  function countBattles(list, from, to, q) {
    const t = String(q || '').trim().toLowerCase();
    let n = 0;
    for (const d of list || []) {
      if (d.year < from || d.year > to) continue;
      if (t && !(d.name + ' ' + (d.war || []).join(' ')).toLowerCase().includes(t)) continue;
      n++;
    }
    return n;
  }

  const ERAS = [['Ancient', -1500, 499], ['Medieval', 500, 1499], ['1500s–1700s', 1500, 1799], ['1800s', 1800, 1899],
    ['World War I', 1914, 1918], ['World War II', 1939, 1945], ['Since 1946', 1946, 2026]];

  // "T−3d 04:05" before launch, "T+00:12" after.
  function countdown(when, now = Date.now()) {
    const s = Math.round((new Date(when) - now) / 1000);
    if (!Number.isFinite(s)) return '';
    const a = Math.abs(s), d = Math.floor(a / 86400), h = Math.floor(a / 3600) % 24, m = Math.floor(a / 60) % 60;
    return (s >= 0 ? 'T−' : 'T+') + (d ? d + 'd ' : '') + String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
  }

  function nextLaunch(results, now = Date.now()) {
    return (results || []).filter((l) => Date.parse(l.net) > now).sort((a, b) => Date.parse(a.net) - Date.parse(b.net))[0] || null;
  }

  function launchGeoJSON(results, now = Date.now()) {
    const features = [];
    for (const l of results || []) {
      const p = l.pad || {}, lat = +p.latitude, lon = +p.longitude;
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      features.push({ type: 'Feature', geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: { name: l.name || '', when: l.net || '', label: countdown(l.net, now),
          status: (l.status && (l.status.abbrev || l.status.name)) || '',
          provider: (l.launch_service_provider && l.launch_service_provider.name) || '',
          pad: [p.name, p.location && p.location.name].filter(Boolean).join(', '),
          mission: (l.mission && l.mission.description) || '' } });
    }
    return { type: 'FeatureCollection', features };
  }

  // Allmaps: a readable name, the picture, the size and the tile address of a georeferenced map.
  const pickLabel = (o) => o && (typeof o === 'string' ? o : (o.en || o.nl || o.none || Object.values(o)[0] || [])[0]);
  function mapHolder(m) {
    const pv = m.resource && m.resource.provider && m.resource.provider[0];
    if (pv && pickLabel(pv.label)) return pickLabel(pv.label);
    try { return new URL(m.resource.id).hostname; } catch (e) { return 'a library'; }
  }
  // A title worth showing has some words in it ("[1]" or "0042" is a page number, not a name).
  const wordy = (s) => s && /\p{L}{3}/u.test(String(s));
  function mapLabel(m) {
    for (const x of (m.resource && m.resource.partOf) || []) {
      const l = [pickLabel(x.label), ...(x.partOf || []).map((y) => pickLabel(y.label))].find(wordy);
      if (l) return String(l);
    }
    return `Map from ${mapHolder(m)}`;
  }
  function mapAcross(m) {
    const a = m._allmaps && m._allmaps.area;
    if (!a) return '';
    const km = Math.sqrt(a) / 1000;
    return km < 1 ? `about ${Math.round(km * 1000)} m across` : `about ${km < 10 ? km.toFixed(1) : Math.round(km).toLocaleString('en')} km across`;
  }
  const mapId = (m) => String(m.id || '').split('/').pop();
  const mapThumb = (m) => `${String(m.resource && m.resource.id || '').replace(/\/$/, '')}/full/96,/0/default.jpg`;
  const mapTiles = (id) => `https://allmaps.xyz/maps/${id}/{z}/{x}/{y}.png`;

  const logic = { auroraGeoJSON, latestKp, kpWords, volcanoGeoJSON, battleGeoJSON, battleFilter, countBattles, ERAS,
    countdown, nextLaunch, launchGeoJSON, mapLabel, mapHolder, mapAcross, mapId, mapThumb, mapTiles, yearText };

  // ---- The map and the panel (browser only) ---------------------------

  function attach(map, ui) {
    const $ = (id) => document.getElementById(id);
    const STORE = 'earth-explorer.data.v1';
    let st = { on: {}, from: 1939, to: 1945, q: '', opacity: 0.85 };
    try { st = Object.assign(st, JSON.parse(localStorage.getItem(STORE) || '{}')); } catch (e) { /* storage off */ }
    const keep = () => { try { localStorage.setItem(STORE, JSON.stringify(st)); } catch (e) { /* storage off */ } };
    const firstLabel = () => (map.getStyle().layers.find((l) => l.type === 'symbol') || {}).id;
    const getJSON = async (url) => { const r = await fetch(url); if (!r.ok) throw new Error(`${new URL(url, location.href).host} answered ${r.status}`); return r.json(); };
    const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
    const note = (id, text) => { const n = $(id); if (n) n.textContent = text; };
    const popup = (lngLat, html) => new maplibregl.Popup({ offset: 10, closeButton: true, maxWidth: '300px' }).setLngLat(lngLat).setHTML(html).addTo(map);
    const timers = {};
    const cache = {};

    function ensure(id, source, layers, beforeLabels) {
      if (!map.getSource(id)) map.addSource(id, source);
      for (const l of layers) if (!map.getLayer(l.id)) map.addLayer(l, beforeLabels ? firstLabel() : undefined);
    }
    function remove(id, layerIds) {
      for (const l of layerIds) if (map.getLayer(l)) map.removeLayer(l);
      if (map.getSource(id)) map.removeSource(id);
    }

    const LAYERS = {
      aurora: {
        async on() {
          const [g, k] = await Promise.all([getJSON('https://services.swpc.noaa.gov/json/ovation_aurora_latest.json'),
            getJSON('https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json').catch(() => null)]);
          const data = auroraGeoJSON(g.coordinates), kp = latestKp(k);
          if (map.getSource('d-aurora')) map.getSource('d-aurora').setData(data);
          ensure('d-aurora', { type: 'geojson', data }, [{ id: 'd-aurora', type: 'fill', source: 'd-aurora',
            paint: { 'fill-color': ['step', ['get', 'v'], '#3cff64', 25, '#e6ff50', 50, '#ff5a50'],
              'fill-opacity': ['interpolate', ['linear'], ['get', 'v'], 4, 0.22, 40, 0.7], 'fill-antialias': false } }], true);
          const peak = g.coordinates.reduce((m, c) => Math.max(m, c[2]), 0);
          note('d-aurora-note', `Kp ${kp == null ? '?' : kp.toFixed(1)} (${kpWords(kp)}), highest chance ${peak}%. Forecast for ${new Date(g['Forecast Time']).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}.`);
          clearInterval(timers.aurora); timers.aurora = setInterval(() => LAYERS.aurora.on().catch(() => {}), 10 * 60 * 1000);
        },
        off() { clearInterval(timers.aurora); remove('d-aurora', ['d-aurora']); note('d-aurora-note', ''); },
      },
      volcanoes: {
        async on() {
          cache.volc = cache.volc || await getJSON('data/volcanoes.json');
          const hans = await getJSON('https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes').catch(() => []);
          const data = volcanoGeoJSON(cache.volc, hans);
          if (map.getSource('d-volc')) map.getSource('d-volc').setData(data);
          ensure('d-volc', { type: 'geojson', data }, [{ id: 'd-volc', type: 'circle', source: 'd-volc',
            layout: { 'circle-sort-key': ['get', 'rank'] },
            paint: { 'circle-radius': ['case', ['>=', ['get', 'rank'], 0], 7, 3.5],
              'circle-color': ['match', ['get', 'alert'], 'WARNING', '#ff3b3b', 'WATCH', '#ff8a5c', 'ADVISORY', '#ffd166', 'NORMAL', '#66d19e', '#c9b8a6'],
              'circle-stroke-color': '#1a0f08', 'circle-stroke-width': 1 } }]);
          const raised = data.features.filter((f) => f.properties.rank > 0).length;
          note('d-volcanoes-note', `${cache.volc.length.toLocaleString('en')} volcanoes; ${raised} with a raised US alert level now.`);
        },
        off() { remove('d-volc', ['d-volc']); note('d-volcanoes-note', ''); },
        click(f) {
          const p = f.properties;
          return `<b>${esc(p.name)}</b><br>${esc([p.type, p.country].filter(Boolean).join(' · '))}` +
            `${p.elev ? `<br>Height ${esc(p.elev)} m` : ''}${p.last ? `<br>Last eruption ${esc(yearText(+p.last))}` : ''}` +
            `${p.alert ? `<br><b>USGS alert: ${esc(p.alert)}</b>${p.color ? ` (aviation ${esc(p.color)})` : ''}` : ''}`;
        },
      },
      battles: {
        async on() {
          cache.battles = cache.battles || await getJSON('data/battles.json');
          ensure('d-battles', { type: 'geojson', data: battleGeoJSON(cache.battles) }, [{ id: 'd-battles', type: 'circle', source: 'd-battles',
            paint: { 'circle-radius': 4.5, 'circle-stroke-color': '#000', 'circle-stroke-width': 0.8,
              'circle-color': ['step', ['get', 'year'], '#c792ea', 500, '#82aaff', 1500, '#66d19e', 1800, '#ffd166', 1900, '#ff8a5c', 1946, '#ff5c7a'] } }]);
          $('d-battles-box').hidden = false; applyBattles();
        },
        off() { remove('d-battles', ['d-battles']); $('d-battles-box').hidden = true; },
        click(f) {
          const p = f.properties;
          return `<b>${esc(p.name)}</b><br>${esc(+p.year < 0 ? yearText(+p.year) : p.date)}${p.war ? `<br>${esc(p.war)}` : ''}` +
            `<br>${p.wiki ? `<a href="${esc(p.wiki)}" target="_blank" rel="noopener">Wikipedia</a> · ` : ''}<a href="https://www.wikidata.org/wiki/${esc(p.id)}" target="_blank" rel="noopener">Wikidata</a>`;
        },
      },
      launches: {
        async on() {
          cache.launch = await getJSON('https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=15&mode=normal');
          const draw = () => {
            const data = launchGeoJSON(cache.launch.results);
            if (map.getSource('d-launch')) map.getSource('d-launch').setData(data);
            ensure('d-launch', { type: 'geojson', data }, [
              { id: 'd-launch', type: 'circle', source: 'd-launch', paint: { 'circle-radius': 6, 'circle-color': '#ffbf47', 'circle-stroke-color': '#1d1203', 'circle-stroke-width': 1.5 } },
              { id: 'd-launch-label', type: 'symbol', source: 'd-launch',
                layout: { 'text-field': ['get', 'label'], 'text-font': ['Noto Sans Regular'], 'text-size': 12, 'text-offset': [0, 1.2], 'text-anchor': 'top', 'text-allow-overlap': false },
                paint: { 'text-color': '#ffd98a', 'text-halo-color': 'rgba(0,0,0,0.85)', 'text-halo-width': 1.4 } }]);
          };
          draw();
          // Launch Library keeps launches in "upcoming" for a few hours after lift-off; name the next one still to come.
          const next = nextLaunch(cache.launch.results);
          note('d-launches-note', next ? `Next: ${next.name}, ${countdown(next.net)}.` : 'No launches listed.');
          clearInterval(timers.launch); timers.launch = setInterval(draw, 30 * 1000);   // countdowns; data refresh on re-open (15 calls/hour limit)
        },
        off() { clearInterval(timers.launch); remove('d-launch', ['d-launch-label', 'd-launch']); note('d-launches-note', ''); },
        click(f) {
          const p = f.properties;
          return `<b>${esc(p.name)}</b><br>${esc(countdown(p.when))} · ${esc(new Date(p.when).toLocaleString())}` +
            `<br>${esc(p.provider)}${p.status ? ` · ${esc(p.status)}` : ''}<br>${esc(p.pad)}${p.mission ? `<br><small>${esc(p.mission.slice(0, 220))}</small>` : ''}`;
        },
      },
      oldmaps: {
        async on() { $('d-oldmaps-box').hidden = false; },
        off() {
          for (const id of Object.keys(shownMaps)) hideMap(id);
          $('d-oldmaps-box').hidden = true;
        },
      },
    };
    const CLICK = { 'd-volc': 'volcanoes', 'd-battles': 'battles', 'd-launch': 'launches' };

    // Battles: span of years, eras and search.
    function applyBattles() {
      if (!map.getLayer('d-battles')) return;
      map.setFilter('d-battles', battleFilter(st.from, st.to, st.q));
      note('d-battles-count', `${countBattles(cache.battles, st.from, st.to, st.q).toLocaleString('en')} battles, ${yearText(st.from)} to ${yearText(st.to)}`);
      $('d-from').value = st.from; $('d-to').value = st.to; $('d-q').value = st.q;
      keep();
    }
    const eraBox = $('d-eras');
    ERAS.forEach(([n, a, b]) => {
      const btn = document.createElement('button'); btn.type = 'button'; btn.className = 'chip'; btn.textContent = n;
      btn.addEventListener('click', () => { st.from = a; st.to = b; applyBattles(); });
      eraBox.appendChild(btn);
    });
    const readSpan = () => { let a = Math.round(+$('d-from').value), b = Math.round(+$('d-to').value); if (b < a) [a, b] = [b, a]; st.from = a; st.to = b; applyBattles(); };
    $('d-from').addEventListener('change', readSpan); $('d-to').addEventListener('change', readSpan);
    let qt = 0; $('d-q').addEventListener('input', (e) => { clearTimeout(qt); qt = setTimeout(() => { st.q = e.target.value; applyBattles(); }, 200); });

    // Old maps: find for the view, then lay chosen ones over the globe.
    const shownMaps = {};
    function hideMap(id) { remove('om-' + id, ['om-' + id]); delete shownMaps[id]; renderMaps(); }
    function showMap(m) {
      const id = mapId(m);
      ensure('om-' + id, { type: 'raster', tiles: [mapTiles(id)], tileSize: 256, attribution: 'Old map: ' + esc(mapHolder(m)) + ' via Allmaps' },
        [{ id: 'om-' + id, type: 'raster', source: 'om-' + id, paint: { 'raster-opacity': st.opacity } }], true);
      shownMaps[id] = true; renderMaps();
    }
    let found = [];
    function renderMaps() {
      const box = $('d-maplist'); box.textContent = '';
      if (!found.length) { box.appendChild(Object.assign(document.createElement('p'), { className: 'fine', textContent: 'No search yet.' })); return; }
      for (const m of found) {
        const id = mapId(m), b = document.createElement('button');
        b.type = 'button'; b.className = 'map-item'; b.setAttribute('aria-pressed', String(!!shownMaps[id]));
        const img = document.createElement('img'); img.src = mapThumb(m); img.alt = ''; img.loading = 'lazy';
        const t = document.createElement('span'); t.className = 'map-name'; t.textContent = mapLabel(m).slice(0, 90);
        const s = document.createElement('span'); s.className = 'map-size'; s.textContent = (shownMaps[id] ? 'On the globe · ' : '') + mapAcross(m);
        b.append(img, t, s);
        b.addEventListener('click', () => (shownMaps[id] ? hideMap(id) : showMap(m)));
        box.appendChild(b);
      }
    }
    $('d-findmaps').addEventListener('click', async () => {
      const b = map.getBounds(), w = b.getWest(), s = b.getSouth(), e = b.getEast(), n = b.getNorth();
      if ((e - w) * (n - s) > 400) { ui.toast('Zoom in to a city or region first. Old maps are found for the area in view.'); return; }
      note('d-oldmaps-note', 'Looking…');
      try {
        found = await getJSON(`https://api.allmaps.org/maps?limit=60&intersects=${w.toFixed(4)},${s.toFixed(4)},${e.toFixed(4)},${n.toFixed(4)}`);
        found.sort((x, y) => ((x._allmaps && x._allmaps.area) || 1e15) - ((y._allmaps && y._allmaps.area) || 1e15));
        note('d-oldmaps-note', found.length ? `${found.length} old maps cover this view. Smallest areas first; tap one to lay it over the globe.` : 'No old maps cover this view yet.');
      } catch (err) { note('d-oldmaps-note', 'Allmaps did not answer: ' + err.message); }
      renderMaps();
    });
    $('d-opacity').value = String(st.opacity);
    $('d-opacity').addEventListener('input', (e) => {
      st.opacity = +e.target.value; keep();
      for (const id of Object.keys(shownMaps)) if (map.getLayer('om-' + id)) map.setPaintProperty('om-' + id, 'raster-opacity', st.opacity);
    });
    renderMaps();

    // Switches.
    async function set(name, on) {
      const sw = $('d-' + name);
      st.on[name] = on; keep(); if (sw) sw.checked = on;
      try { if (on) await LAYERS[name].on(); else LAYERS[name].off(); }
      catch (err) { st.on[name] = false; keep(); if (sw) sw.checked = false; LAYERS[name].off(); ui.toast(`${sw ? sw.nextElementSibling.textContent.trim() : name} could not load: ${err.message}`); }
    }
    for (const name of Object.keys(LAYERS)) {
      const sw = $('d-' + name); if (!sw) continue;
      sw.addEventListener('change', () => set(name, sw.checked));
    }
    map.on('click', (e) => {
      const ids = Object.keys(CLICK).filter((id) => map.getLayer(id));
      if (!ids.length) return;
      const f = map.queryRenderedFeatures(e.point, { layers: ids })[0];
      if (f) popup(e.lngLat, LAYERS[CLICK[f.layer.id]].click(f));
    });
    for (const id of Object.keys(CLICK)) {
      map.on('mouseenter', id, () => { map.getCanvas().style.cursor = 'pointer'; });
      map.on('mouseleave', id, () => { map.getCanvas().style.cursor = ''; });
    }
    for (const name of Object.keys(LAYERS)) if (st.on[name]) set(name, true);
    return { set, state: () => st, layers: () => Object.keys(LAYERS).filter((n) => st.on[n]) };
  }

  return Object.assign({ attach }, logic);
});
