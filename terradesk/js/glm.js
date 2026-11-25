// Reads NOAA GOES lightning (GLM) files on the phone. The desktop app did this in its main process with h5wasm;
// here the same library runs in the page. Used by the app (td-web.js) and its tests.
export function hourPrefix(d) {
  const doy = String(Math.floor((Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate()) - Date.UTC(d.getUTCFullYear(), 0, 0)) / 864e5)).padStart(3, '0');
  return `GLM-L2-LCFA/${d.getUTCFullYear()}/${doy}/${String(d.getUTCHours()).padStart(2, '0')}/`;
}
export const listKeys = xml => [...String(xml).matchAll(/<Key>([^<]+)<\/Key>/g)].map(m => m[1]).sort();

// bytes: Uint8Array of one GLM file. Returns [{lat, lon, e, sat}].
export async function readFlashes(h5, bytes, sat) {
  const M = await h5.ready;
  const p = '/glm-' + Math.random().toString(36).slice(2) + '.nc';
  M.FS.writeFile(p, bytes);
  const f = new h5.File(p, 'r');
  const out = [];
  try {
    const lat = f.get('flash_lat').value, lon = f.get('flash_lon').value, en = f.get('flash_energy');
    // flash_energy is packed: unsigned 16-bit counts times scale_factor plus add_offset gives joules (the desktop
    // build read the raw counts; fixed here).
    const e = en ? en.value : null, at = en ? en.attrs : {}, num = k => (at[k] ? +at[k].value[0] : null);
    const scale = num('scale_factor') ?? 1, add = num('add_offset') ?? 0, uns = !!(at._Unsigned && String(at._Unsigned.value) === 'true');
    const joules = v => ((uns && v < 0 ? v + 65536 : v) * scale + add);
    for (let i = 0; i < lat.length; i++) out.push({ lat: lat[i], lon: lon[i], e: e ? joules(e[i]) : 0, sat });
  } finally { f.close(); try { M.FS.unlink(p); } catch { /* gone */ } }
  return out;
}

// The whole read: newest hour folder per satellite, the last 15 files (about 5 minutes of flashes).
// io.list(url) -> text, io.bytes(url) -> Uint8Array or null.
export async function lightning(h5, io, now = new Date()) {
  const out = [];
  for (const bucket of ['noaa-goes19', 'noaa-goes18']) {
    let files = [];
    for (let back = 0; back < 2 && !files.length; back++) {
      const d = new Date(now.getTime() - back * 3600 * 1000);
      files = listKeys(await io.list(`https://${bucket}.s3.amazonaws.com/?list-type=2&prefix=${hourPrefix(d)}`));
    }
    for (const key of files.slice(-15)) {
      const b = await io.bytes(`https://${bucket}.s3.amazonaws.com/${key}`);
      if (!b) continue;
      out.push(...await readFlashes(h5, b, bucket.slice(-2)));
    }
  }
  return { flashes: out, at: Date.now() };
}
