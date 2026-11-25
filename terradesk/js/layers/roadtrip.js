// Road Trip 3D: plan a route with stops (search or click), route it with OSRM (the public demo server: one request
// a second, personal use), then fly along it with Cesium's own clock and camera tracking. Export as GPX.
let stops = [], route = null, routeEnt = null, stopEnts = [], car = null, profile = 'driving', flying = false;

function gpx(name, pts) {
  return `<?xml version="1.0" encoding="UTF-8"?>\n<gpx version="1.1" creator="Terra Desk" xmlns="http://www.topografix.com/GPX/1/1"><trk><name>${name}</name><trkseg>\n` +
    pts.map(([lon, lat]) => `<trkpt lat="${lat.toFixed(6)}" lon="${lon.toFixed(6)}"></trkpt>`).join('\n') + '\n</trkseg></trk></gpx>\n';
}
function drawStops(T) {
  const { C, viewer } = T;
  for (const e of stopEnts.splice(0)) viewer.entities.remove(e);
  stops.forEach((s, i) => stopEnts.push(viewer.entities.add({ position: C.Cartesian3.fromDegrees(s.lon, s.lat), point: { pixelSize: 10, color: C.Color.fromCssColorString(i === 0 ? '#66d19e' : i === stops.length - 1 ? '#ff8a5c' : '#ffbf47'), outlineColor: C.Color.BLACK, outlineWidth: 1, heightReference: C.HeightReference.CLAMP_TO_GROUND, disableDepthTestDistance: Number.POSITIVE_INFINITY }, label: { text: `${i + 1}. ${s.name}`, font: '12px Segoe UI', pixelOffset: new C.Cartesian2(0, -15), showBackground: true, backgroundColor: C.Color.fromCssColorString('rgba(5,8,13,.75)'), disableDepthTestDistance: Number.POSITIVE_INFINITY } })));
}
async function plan(T, L) {
  const { C, viewer } = T;
  if (routeEnt) { viewer.entities.remove(routeEnt); routeEnt = null; } route = null;
  if (stops.length < 2) { T.panel(L); return; }
  const coords = stops.map(s => `${s.lon.toFixed(5)},${s.lat.toFixed(5)}`).join(';');
  const j = await T.json(`https://router.project-osrm.org/route/v1/${profile}/${coords}?overview=full&geometries=geojson`);
  if (!j.routes || !j.routes.length) throw new Error(j.message || 'no route between those stops');
  route = j.routes[0];
  routeEnt = viewer.entities.add({ polyline: { positions: C.Cartesian3.fromDegreesArray(route.geometry.coordinates.flat()), width: 5, material: new C.PolylineOutlineMaterialProperty({ color: C.Color.fromCssColorString('#8fd0ff'), outlineColor: C.Color.fromCssColorString('#05080d'), outlineWidth: 2 }), clampToGround: true } });
  viewer.flyTo(routeEnt, { duration: 2 });
  T.status(L, T.G.formatDistance(route.distance, T.settings().units), '');
  T.panel(L);
}
function fly(T, L) {
  const { C, viewer } = T;
  if (flying) { stopFly(T); T.panel(L); return; }
  const pts = route.geometry.coordinates;
  // Cesium's clock moves a marker along the route; the camera follows it (built-in tracking, no hand-made animation)
  const start = C.JulianDate.now(), total = 90, prop = new C.SampledPositionProperty();
  const cum = [0]; for (let i = 1; i < pts.length; i++) cum.push(cum[i - 1] + T.G.distance(pts[i - 1], pts[i]));
  const step = Math.max(1, Math.floor(pts.length / 1500));
  for (let i = 0; i < pts.length; i += step) prop.addSample(C.JulianDate.addSeconds(start, total * cum[i] / cum[cum.length - 1], new C.JulianDate()), C.Cartesian3.fromDegrees(pts[i][0], pts[i][1], 400));
  prop.setInterpolationOptions({ interpolationDegree: 2, interpolationAlgorithm: C.LagrangePolynomialApproximation });
  car = viewer.entities.add({ position: prop, orientation: new C.VelocityOrientationProperty(prop), point: { pixelSize: 12, color: C.Color.fromCssColorString('#ffbf47'), outlineColor: C.Color.BLACK, outlineWidth: 2 }, viewFrom: new C.Cartesian3(-2500, -2500, 1800) });
  viewer.clock.startTime = start.clone(); viewer.clock.stopTime = C.JulianDate.addSeconds(start, total, new C.JulianDate()); viewer.clock.currentTime = start.clone();
  viewer.clock.clockRange = C.ClockRange.CLAMPED; viewer.clock.multiplier = 1; viewer.clock.shouldAnimate = true;
  viewer.trackedEntity = car; flying = true;
  const end = viewer.clock.onStop.addEventListener(() => { end(); stopFly(T); T.panel(L); });
  T.panel(L);
}
function stopFly(T) { const v = T.viewer; v.trackedEntity = undefined; v.clock.shouldAnimate = false; if (car) v.entities.remove(car); car = null; flying = false; }

export default {
  id: 'roadtrip', name: 'Road Trip 3D', group: 'Tools', tool: true, worldOnly: true, sub: 'Plan a route and fly it', credit: 'Routes: OSRM (OpenStreetMap)',
  info: 'Add stops by search or by tapping the globe, route them by car, bike or foot, then fly the whole route in 3D.',
  async enable(T) { this.on = true; },
  async disable(T) { stopFly(T); stops = []; drawStops(T); if (routeEnt) T.viewer.entities.remove(routeEnt); routeEnt = null; route = null; T.capture(null); },
  panel(T) {
    const u = T.settings().units;
    return `<div class="row"><input id="rtQ" placeholder="Add a stop: town, address, landmark" class="grow"><button id="rtAdd">Add</button><button id="rtClick" title="Click the globe">Pick</button></div>
      <div class="list" id="rtList">${stops.map((s, i) => `<div class="it" data-i="${i}"><span>${i + 1}. ${T.esc(s.name)}</span><span><button class="small" data-up="${i}">↑</button> <button class="small" data-del="${i}">✕</button></span></div>`).join('') || '<div class="it muted">No stops yet. Add at least two.</div>'}</div>
      <div class="row"><select id="rtProf"><option value="driving">By car</option><option value="cycling">By bike</option><option value="foot">On foot</option></select><button id="rtGo" class="primary" ${stops.length < 2 ? 'disabled' : ''}>Route it</button></div>
      ${route ? `<div class="kv"><div>Distance</div><div>${T.esc(T.G.formatDistance(route.distance, u))}</div><div>Time</div><div>${Math.floor(route.duration / 3600)} h ${Math.round(route.duration / 60) % 60} min</div><div>Stops</div><div>${stops.length}</div></div>
        <div class="row"><button id="rtFly" class="primary">${flying ? 'Stop flying' : 'Fly the route'}</button><button id="rtGpx">Save GPX</button></div>` : ''}
      <p class="note">Routing by the OSRM public server (personal, non-commercial use). Search by OpenStreetMap Nominatim.</p>`;
  },
  wire(T, el) {
    const L = this;
    el.querySelector('#rtProf').value = profile;
    el.querySelector('#rtProf').onchange = e => { profile = e.target.value; };
    const add = async () => { const q = el.querySelector('#rtQ').value.trim(); if (!q) return; const r = await T.json(`https://nominatim.openstreetmap.org/search?format=jsonv2&limit=1&q=${encodeURIComponent(q)}`); if (!r[0]) { T.toast('Not found: ' + q, true); return; } stops.push({ name: r[0].name || q, lon: +r[0].lon, lat: +r[0].lat }); drawStops(T); T.panel(L); };
    el.querySelector('#rtAdd').onclick = () => add().catch(e => T.toast(e.message, true));
    el.querySelector('#rtQ').onkeydown = e => { if (e.key === 'Enter') add().catch(er => T.toast(er.message, true)); };
    el.querySelector('#rtClick').onclick = () => { T.toast('Click the globe to add a stop.'); T.capture((lon, lat) => { T.capture(null); stops.push({ name: T.G.formatCoord(lat, lon), lon, lat }); drawStops(T); T.panel(L); }); };
    el.querySelector('#rtList').onclick = e => { const d = e.target.closest('[data-del]'), u = e.target.closest('[data-up]'); if (d) stops.splice(+d.dataset.del, 1); else if (u && +u.dataset.up > 0) { const i = +u.dataset.up; [stops[i - 1], stops[i]] = [stops[i], stops[i - 1]]; } else return; drawStops(T); T.panel(L); };
    el.querySelector('#rtGo').onclick = () => plan(T, L).catch(e => T.toast('Route: ' + e.message, true));
    const f = el.querySelector('#rtFly'); if (f) f.onclick = () => fly(T, L);
    const g = el.querySelector('#rtGpx'); if (g) g.onclick = async () => { const r = await T.td.saveFile({ name: 'road-trip.gpx', data: gpx('Road trip', route.geometry.coordinates), filters: [{ name: 'GPX', extensions: ['gpx'] }] }); if (r.file) T.toast('Saved ' + r.file); };
  },
  async test(T) {
    stops = [{ name: 'Denver', lon: -104.9903, lat: 39.7392 }, { name: 'Boulder', lon: -105.2705, lat: 40.015 }];
    await plan(T, this); const r = { km: route && Math.round(route.distance / 1000), points: route && route.geometry.coordinates.length };
    await this.disable(T); return r;
  },
};
