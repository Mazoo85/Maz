// Lightning Live: NOAA GOES-19 and GOES-18 Geostationary Lightning Mapper flashes from the last ~5 minutes
// (public data on AWS, read in the main process). Coverage: the Americas and nearby oceans only. Blitzortung's
// worldwide network does not allow other apps to use its data, so it is not used.
let pts = null, timer = 0, last = null;

async function load(T, L) {
  const r = await T.td.lightning();
  if (r.error) { T.status(L, 'error', 'err'); return; }
  last = r;
  if (!pts) return;
  pts.removeAll();
  for (const f of r.flashes) pts.add({ position: T.C.Cartesian3.fromDegrees(f.lon, f.lat, 12000), pixelSize: 3 + Math.min(4, Math.log10(1 + (f.e || 0) * 1e15)), color: T.C.Color.fromCssColorString('#fff36b').withAlpha(0.9), id: { td: { layer: 'lightning', data: f } } });
  T.status(L, `${r.flashes.length.toLocaleString()} flashes`, 'live');
}

export default {
  id: 'lightning', name: 'Lightning Live', group: 'Live now', worldOnly: true, sub: 'Flashes, last 5 minutes (Americas)', credit: 'Lightning: NOAA GOES GLM',
  info: 'Lightning flashes seen by NOAA\'s GOES satellites in the last five minutes. Covers North and South America and the nearby oceans.',
  async enable(T) { pts = T.viewer.scene.primitives.add(new T.C.PointPrimitiveCollection()); T.status(this, 'loading…'); await load(T, this); timer = setInterval(() => load(T, this).catch(() => {}), 60000); },
  async disable(T) { clearInterval(timer); if (pts) T.viewer.scene.primitives.remove(pts); pts = null; },
  panel() {
    return `<div class="big">${last ? last.flashes.length.toLocaleString() : '…'}</div><span class="muted small">flashes in the last five minutes</span>
      <p class="note">From NOAA's GOES-East and GOES-West lightning mappers, updated every minute. They watch the Americas and nearby oceans; the rest of the world has no free live lightning feed that allows apps to use it.</p>`;
  },
  onPick(T, f) { T.detail('Lightning flash', `<div class="kv"><div>Where</div><div>${T.esc(T.G.formatCoord(f.lat, f.lon))}</div><div>Seen by</div><div>GOES-${f.sat}</div><div>Energy</div><div>${f.e ? f.e.toExponential(2) + ' J' : '—'}</div></div>`); },
  async test() { return { flashes: last ? last.flashes.length : 0 }; },
};
