/*
 * Where footage may come from, and whether its license lets us re-cut and
 * re-upload it. Pure functions, no network: the CLI fetches, these decide.
 *
 * Accepted: public domain (including CC0 and the Public Domain Mark), CC BY
 * and CC BY-SA. Refused: NonCommercial (NC) because a channel may earn money,
 * NoDerivatives (ND) because a short is an edit, and anything with no license
 * stated, because "no license" means "all rights reserved".
 */

export const ACCEPTED = ['public domain', 'CC BY', 'CC BY-SA'];

/* One license string or URL -> { ok, kind, reason }. */
export function classifyLicense(text) {
  const s = String(text || '').trim().toLowerCase();
  if (!s) return { ok: false, kind: null, reason: 'no license stated (all rights reserved)' };
  const hasNc = /(^|[^a-z])nc([^a-z]|$)|by-nc|noncommercial|non-commercial/.test(s);
  const hasNd = /(^|[^a-z])nd([^a-z]|$)|by-nd|-nd\b|noderiv|no deriv/.test(s);
  if (hasNc) return { ok: false, kind: null, reason: 'NonCommercial license: not allowed on a channel that may earn money' };
  if (hasNd) return { ok: false, kind: null, reason: 'NoDerivatives license: it may not be edited into a short' };
  if (/publicdomain|public domain|^pd\b|^pd-|cc0|cc-zero|creative commons zero/.test(s)) {
    return { ok: true, kind: 'public domain', reason: null };
  }
  if (/by-sa|cc[- ]by[- ]sa|attribution-sharealike|attribution share ?alike/.test(s)) {
    return { ok: true, kind: 'CC BY-SA', reason: null };
  }
  if (/licenses\/by\/|cc[- ]by(\b|[- ]\d)|^attribution\b|creative commons attribution(?! ?-?(non|no|share))/.test(s)) {
    return { ok: true, kind: 'CC BY', reason: null };
  }
  return { ok: false, kind: null, reason: 'license not recognised: ' + String(text).slice(0, 80) };
}

/* "12:34", "1:02:03" or "754.3" -> seconds; null if unreadable. */
export function parseLength(value) {
  if (value === undefined || value === null || value === '') return null;
  const s = String(value).trim();
  if (/^\d+(\.\d+)?$/.test(s)) return Number(s);
  const parts = s.split(':').map(Number);
  if (parts.length < 2 || parts.length > 3 || parts.some((n) => !Number.isFinite(n))) return null;
  return parts.reduce((total, n) => total * 60 + n, 0);
}

function stripHtml(text) {
  return String(text || '').replace(/<[^>]*>/g, ' ').replace(/&amp;/g, '&').replace(/&quot;/g, '"')
    .replace(/&#0?39;|&apos;/g, "'").replace(/&nbsp;/g, ' ').replace(/\s+/g, ' ').trim();
}

/* ------------------------------------------------------- Internet Archive */

export function archiveSearchUrl(topic, rows) {
  // No license filter in the query: the Archive's search fails on wildcard
  // license filters ("Invalid or no response from Elasticsearch"), so every
  // result's license is checked here instead (archiveCandidates).
  const q = '(' + String(topic).replace(/[()]/g, ' ').trim() + ') AND mediatype:(movies) AND licenseurl:[* TO *]';
  const u = new URL('https://archive.org/advancedsearch.php');
  u.searchParams.set('q', q);
  for (const f of ['identifier', 'title', 'licenseurl', 'year', 'downloads', 'description', 'creator']) u.searchParams.append('fl[]', f);
  u.searchParams.append('sort[]', 'downloads desc');
  u.searchParams.set('rows', String(Math.min(200, (rows || 20) * 5)));
  u.searchParams.set('output', 'json');
  return u.toString();
}

/* advancedsearch JSON -> candidates, licence-checked (the search filter is a
 * first pass only; every item is classified again here). */
export function archiveCandidates(json) {
  if (json && json.error) throw new Error('the Internet Archive search failed: ' + String(json.error).slice(0, 160));
  const docs = (json && json.response && json.response.docs) || [];
  return docs.map((d) => {
    const lic = classifyLicense(d.licenseurl);
    return {
      source: 'Internet Archive',
      id: d.identifier,
      title: stripHtml(Array.isArray(d.title) ? d.title[0] : d.title) || d.identifier,
      year: d.year ? String(d.year) : null,
      creator: d.creator ? stripHtml(Array.isArray(d.creator) ? d.creator.join(', ') : d.creator) : null,
      page: 'https://archive.org/details/' + d.identifier,
      license: lic.kind,
      license_url: d.licenseurl || null,
      ok: lic.ok,
      reason: lic.reason,
      popularity: Number(d.downloads) || 0,
      description: stripHtml(Array.isArray(d.description) ? d.description[0] : d.description).slice(0, 300)
    };
  });
}

export function archiveMetadataUrl(id) {
  return 'https://archive.org/metadata/' + encodeURIComponent(id);
}

/* /metadata JSON -> the best video file to cut from: an h.264/MPEG4 .mp4,
 * preferring the largest (best picture). Null if the item has none. */
export function archiveVideoFile(json) {
  const id = json && json.metadata && json.metadata.identifier;
  const files = (json && json.files) || [];
  const videos = files.filter((f) => /\.(mp4|m4v)$/i.test(f.name || '') && /h\.264|mpeg4|mp4/i.test(f.format || 'mp4'));
  if (!id || !videos.length) return null;
  videos.sort((a, b) => (Number(b.size) || 0) - (Number(a.size) || 0));
  const f = videos[0];
  return {
    url: 'https://archive.org/download/' + encodeURIComponent(id) + '/' + f.name.split('/').map(encodeURIComponent).join('/'),
    name: f.name,
    seconds: parseLength(f.length),
    width: Number(f.width) || null,
    height: Number(f.height) || null
  };
}

/* ------------------------------------------------------ Wikimedia Commons */

/* "Footage (1911-1912)" -> "1911-1912"; "2005-08-28" -> "2005"; else null.
 * The title's year is the film's; a date field is often just the upload. */
export function yearOf(text) {
  const s = String(text || '').replace(/_/g, ' ');
  const range = s.match(/\b(1[89]\d\d|20\d\d)\s*[-–]\s*(1[89]\d\d|20\d\d)\b/);
  if (range) return range[1] + '-' + range[2];
  const one = s.match(/\b(1[89]\d\d|20\d\d)\b/);
  return one ? one[1] : null;
}

export function commonsSearchUrl(topic, rows) {
  const u = new URL('https://commons.wikimedia.org/w/api.php');
  u.searchParams.set('action', 'query');
  u.searchParams.set('format', 'json');
  u.searchParams.set('generator', 'search');
  u.searchParams.set('gsrsearch', 'filetype:video ' + String(topic).trim());
  u.searchParams.set('gsrnamespace', '6');
  u.searchParams.set('gsrlimit', String(rows || 20));
  u.searchParams.set('prop', 'imageinfo');
  u.searchParams.set('iiprop', 'url|size|mediatype|extmetadata');
  u.searchParams.set('iiextmetadatafilter', 'LicenseShortName|LicenseUrl|Artist|ImageDescription|DateTimeOriginal|ObjectName');
  return u.toString();
}

/* Commons API JSON -> candidates with a direct file URL and licence. */
export function commonsCandidates(json) {
  const pages = Object.values((json && json.query && json.query.pages) || {});
  pages.sort((a, b) => (a.index || 0) - (b.index || 0));
  return pages.map((p) => {
    const info = (p.imageinfo && p.imageinfo[0]) || {};
    const meta = info.extmetadata || {};
    const val = (k) => (meta[k] && meta[k].value) || '';
    const licText = val('LicenseShortName') || val('LicenseUrl');
    const lic = classifyLicense(licText);
    const name = String(p.title || '').replace(/^File:/, '');
    return {
      source: 'Wikimedia Commons',
      id: name,
      title: stripHtml(val('ObjectName')) || name.replace(/\.[a-z0-9]+$/i, '').replace(/_/g, ' '),
      year: yearOf(name) || yearOf(stripHtml(val('ObjectName'))) || yearOf(stripHtml(val('DateTimeOriginal'))),
      creator: stripHtml(val('Artist')) || null,
      page: info.descriptionurl || 'https://commons.wikimedia.org/wiki/' + encodeURIComponent(p.title || ''),
      license: lic.kind,
      license_url: val('LicenseUrl') || null,
      ok: lic.ok && info.mediatype === 'VIDEO',
      reason: lic.ok ? (info.mediatype === 'VIDEO' ? null : 'not a video file') : lic.reason,
      popularity: 0,
      description: stripHtml(val('ImageDescription')).slice(0, 300),
      file: info.url ? { url: info.url, name: name, seconds: info.duration ? Number(info.duration) : null, width: info.width || null, height: info.height || null } : null
    };
  });
}

/* ---------------------------------------------------------------- credits */

/* The credit line for one source, right for its license. Public domain needs
 * no credit, but naming the source is honest and helps viewers find it. */
export function creditLine(src) {
  const who = src.creator ? ' by ' + src.creator : '';
  const when = src.year ? ' (' + src.year + ')' : '';
  const where = src.source ? ', ' + src.source : '';
  if (src.license === 'public domain') return '"' + src.title + '"' + when + who + where + ', public domain: ' + src.page;
  const lic = src.license + (src.license_url ? ' (' + src.license_url + ')' : '');
  return '"' + src.title + '"' + when + who + where + ', licensed ' + lic + ': ' + src.page;
}
