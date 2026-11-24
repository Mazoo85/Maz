// Aurora Watch: NOAA SWPC OVATION aurora forecast (30–90 minutes ahead, 1° grid) drawn over the globe, plus the
// planetary Kp index. Public domain, no key. Refreshes every 10 minutes.
let layer = null, timer = 0, info = null, url = null;

function paint(grid) {
  // 360 x 180 cells covering exactly -180..180, -90..90 (a rectangle past the poles is rejected by Cesium)
  const c = document.createElement('canvas'); c.width = 360; c.height = 180; const g = c.getContext('2d'); const img = g.createImageData(360, 180);
  for (const [lon, lat, v] of grid) {
    if (v < 4) continue;                       // under 4% is noise on NOAA's own map too
    const x = (Math.round(lon) + 180) % 360, y = Math.min(179, Math.max(0, 89 - Math.floor(lat))), i = (y * 360 + x) * 4;
    // green at low probability through yellow to red at high, like the NOAA map
    img.data[i] = v > 50 ? 255 : v > 25 ? 230 : 60; img.data[i + 1] = v > 50 ? 80 : 255; img.data[i + 2] = 80; img.data[i + 3] = Math.round(Math.min(230, 50 + v * 6));
  }
  g.putImageData(img, 0, 0);
  return c.toDataURL('image/png');
}
async function load(T, L) {
  const j = await T.json('https://services.swpc.noaa.gov/json/ovation_aurora_latest.json');
  let kp = null; try { const k = await T.json('https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json'); const last = Array.isArray(k[0]) ? k[k.length - 1] : k[k.length - 1]; kp = Array.isArray(last) ? +last[1] : +(last.Kp ?? last.kp_index); } catch { /* optional */ }
  const max = j.coordinates.reduce((m, c) => Math.max(m, c[2]), 0);
  info = { obs: j['Observation Time'], fc: j['Forecast Time'], kp, max };
  url = paint(j.coordinates);
  await place(T);
  T.status(L, `Kp ${kp != null ? kp.toFixed(1) : '?'} · peak ${max}%`, 'live');
}
async function place(T) {
  if (layer) T.viewer.imageryLayers.remove(layer, true);
  if (!url) return;
  layer = T.viewer.imageryLayers.addImageryProvider(await T.C.SingleTileImageryProvider.fromUrl(url, { rectangle: T.C.Rectangle.fromDegrees(-180, -90, 180, 90), credit: 'Aurora: NOAA SWPC OVATION' }));
  layer.alpha = 0.85; layer.dayAlpha = 0.85; layer.nightAlpha = 1;   // stronger on the night side when day and night is on
}

export default {
  id: 'aurora', name: 'Aurora Watch', group: 'Live now', worldOnly: true, sub: 'Aurora forecast and Kp index', credit: 'Aurora: NOAA SWPC',
  info: 'NOAA\'s OVATION aurora forecast for the next hour over the whole globe, with the Kp index. Green to red shows the chance of seeing aurora overhead.',
  async enable(T) { await load(T, this); timer = setInterval(() => load(T, this).catch(() => {}), 10 * 60 * 1000); },
  async disable(T) { clearInterval(timer); if (layer) T.viewer.imageryLayers.remove(layer, true); layer = null; },
  onImagery(T) { layer = null; place(T); },
  panel(T) {
    if (!info) return '<p class="note">Loading the forecast…</p>';
    const kp = info.kp, word = kp == null ? '' : kp >= 7 ? 'strong storm: aurora far from the poles' : kp >= 5 ? 'geomagnetic storm: aurora at mid latitudes' : kp >= 4 ? 'active' : 'quiet';
    return `<div class="big">Kp ${kp != null ? kp.toFixed(1) : '?'}</div><span class="muted small">${T.esc(word)}</span>
      <div class="kv"><div>Forecast for</div><div>${new Date(info.fc).toLocaleString()}</div><div>Observed</div><div>${new Date(info.obs).toLocaleString()}</div><div>Highest chance</div><div>${info.max}%</div></div>
      <p class="note">Best seen on a dark, clear night, looking toward the coloured band. Turn on Day and night in Settings to see where it is dark.</p>`;
  },
  async test(T) {
    T.viewer.camera.setView({ destination: T.C.Cartesian3.fromDegrees(-100, 75, 9000000), orientation: { heading: 0, pitch: T.C.Math.toRadians(-90), roll: 0 } });
    await new Promise(r => setTimeout(r, 5000));   // the overlay attaches to the globe tiles as they load
    const L = T.viewer.imageryLayers, i = layer ? L.indexOf(layer) : -1;
    return { ...info, layerIndex: i, layers: L.length, show: layer && layer.show, alpha: layer && layer.alpha, ready: !!(layer && layer.imageryProvider && layer.imageryProvider.tileWidth), lighting: T.viewer.scene.globe.enableLighting };
  },
};
