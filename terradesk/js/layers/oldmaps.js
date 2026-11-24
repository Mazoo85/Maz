// Old Maps: historical maps georeferenced by the Allmaps community (IIIF Georeference Annotations), found for the
// area you are looking at and laid over the terrain through the Allmaps tile server. Each map keeps its own
// holder's license (many are public domain: Library of Congress, Boston Public Library, national libraries).
let found = [], shown = new Map(), opacity = 0.85;
const label = m => {
  const pick = o => o && (typeof o === 'string' ? o : (o.en || o.nl || o.none || Object.values(o)[0] || [])[0]);
  const p = m.resource && m.resource.partOf || [];
  for (const x of p) { const l = pick(x.label) || (x.partOf || []).map(y => pick(y.label)).find(Boolean); if (l) return l; }
  return `Map from ${holder(m)}`;      // many collections give no title in the annotation; the picture and holder identify it
};
const holder = m => { const pv = m.resource && m.resource.provider && m.resource.provider[0]; const pick = o => o && (o.en || o.none || Object.values(o)[0] || [])[0]; if (pv && pick(pv.label)) return pick(pv.label); try { return new URL(m.resource.id).hostname.replace(/^(www|iiif|images)\./, ''); } catch { return ''; } };
const across = m => { const a = m._allmaps && m._allmaps.area; if (!a) return ''; const km = Math.sqrt(a) / 1000; return km < 1 ? `about ${Math.round(km * 1000)} m across` : `about ${km < 10 ? km.toFixed(1) : Math.round(km).toLocaleString()} km across`; };
const thumb = m => `${m.resource.id.replace(/\/$/, '')}/full/${m.resource.type === 'ImageService3' ? '96,' : '96,'}/0/default.jpg`;
const mapId = m => m.id.split('/').pop();

async function search(T, L) {
  const v = T.view();
  const [w, s, e, n] = v.box;
  if ((e - w) * (n - s) > 400) { T.toast('Zoom in to a city or region first; old maps are listed for the area in view.'); return; }
  T.status(L, 'searching…');
  found = await T.json(`https://api.allmaps.org/maps?limit=80&intersects=${w.toFixed(4)},${s.toFixed(4)},${e.toFixed(4)},${n.toFixed(4)}`);
  found.sort((a, b) => ((a._allmaps && a._allmaps.area) || 1e15) - ((b._allmaps && b._allmaps.area) || 1e15));   // local maps first
  T.status(L, `${found.length} maps here`, '');
  T.panel(L);
}
async function showMap(T, m) {
  const id = mapId(m);
  if (shown.has(id)) { T.viewer.imageryLayers.remove(shown.get(id).layer, true); shown.delete(id); return false; }
  const tj = await T.json(`https://allmaps.xyz/maps/${id}/tiles.json`);
  const [w, s, e, n] = tj.bounds;
  const layer = T.viewer.imageryLayers.addImageryProvider(new T.C.UrlTemplateImageryProvider({ url: `https://allmaps.xyz/maps/${id}/{z}/{x}/{y}.png`, rectangle: T.C.Rectangle.fromDegrees(w, s, e, n), maximumLevel: 21, credit: `Old map: ${label(m)} (${holder(m)}) via Allmaps` }));
  layer.alpha = opacity;
  shown.set(id, { layer, m, bounds: [w, s, e, n] });
  T.viewer.camera.flyTo({ destination: T.C.Rectangle.fromDegrees(w, s, e, n), duration: 1.5 });
  return true;
}

export default {
  id: 'oldmaps', name: 'Old Maps', group: 'History and worlds', worldOnly: true, sub: 'Historical maps laid on the globe', credit: 'Old maps: Allmaps (each map keeps its own license)',
  info: 'Find historical maps of the area you are looking at, georeferenced by the Allmaps community, and lay them over the terrain.',
  async enable(T) { T.status(this, 'zoom in, then search'); },
  async disable(T) { for (const s of shown.values()) T.viewer.imageryLayers.remove(s.layer, true); shown.clear(); },
  onImagery(T) { const keep = [...shown.values()].map(s => s.m); shown.clear(); for (const m of keep) showMap(T, m); },
  panel(T) {
    return `<p class="note">Zoom to a city or region, then press Find. Tap a map to lay it over the globe; tap again to remove it.</p>
      <div class="row"><button id="omFind" class="primary">Find old maps here</button><label class="f" style="flex:1">Opacity <input id="omOp" type="range" min="0.2" max="1" step="0.05" value="${opacity}"></label></div>
      <div class="list" id="omList">${found.map((m, i) => `<div class="it" data-i="${i}" style="grid-template-columns:56px minmax(0,1fr) auto"><img src="${T.esc(thumb(m))}" alt="" loading="lazy" style="width:56px;height:42px;object-fit:cover;border-radius:4px;grid-row:span 2"><span>${T.esc(label(m).slice(0, 80))}</span><span class="r">${shown.has(mapId(m)) ? 'on' : ''}</span><span class="s" style="grid-column:2/4">${T.esc(across(m))}</span></div>`).join('') || '<div class="it muted">No search yet.</div>'}</div>
      <p class="note">Each map keeps its holder's license. Library of Congress and many national libraries are public domain; some collections (David Rumsey) are for non-commercial use.</p>`;
  },
  wire(T, el) {
    el.querySelector('#omFind').onclick = () => search(T, this).catch(e => T.toast('Old maps: ' + e.message, true));
    el.querySelector('#omOp').oninput = e => { opacity = +e.target.value; for (const s of shown.values()) s.layer.alpha = opacity; };
    el.querySelector('#omList').onclick = async e => { const it = e.target.closest('[data-i]'); if (!it) return; try { await showMap(T, found[+it.dataset.i]); T.panel(this); T.credits(); } catch (err) { T.toast('That map could not be loaded: ' + err.message, true); } };
  },
  async test(T) {
    T.viewer.camera.setView({ destination: T.C.Rectangle.fromDegrees(-74.05, 40.68, -73.93, 40.8) });   // Manhattan
    await search(T, this);
    let ok = false; if (found[0]) ok = await showMap(T, found[0]);
    return { found: found.length, overlaid: ok, first: found[0] && label(found[0]) };
  },
};
