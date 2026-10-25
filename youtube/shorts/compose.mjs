/*
 * Turns a plan into a vertical YouTube Short with ffmpeg, and writes the
 * upload details beside it.
 *
 * A plan (JSON) names the sources, the scenes to cut and the words to show:
 *
 *   {
 *     "hook": "THE TOWER PARIS HATED",            // first 2.5 s, big text
 *     "end":  "WOULD YOU HAVE BUILT IT?",        // last 2 s
 *     "sources": { "eiffel": { "file": "https://...mp4", "title": "...", "license": "public domain",
 *                              "page": "https://...", "source": "Internet Archive", "year": "1889" } },
 *     "scenes": [ { "source": "eiffel", "start": 12.5, "length": 4, "caption": "1887: 300 workers begin" } ],
 *     "upload": { "title": "...", "description": "...", "tags": ["..."], "category": "Education" }
 *   }
 *
 * Every source must carry an accepted license (see sources.mjs). Credits for
 * each one are added to the description automatically, so they can't be lost.
 */
import { classifyLicense, creditLine } from './sources.mjs';

export const WIDTH = 1080;
export const HEIGHT = 1920;
export const MAX_SHORT_SECONDS = 60; // YouTube Shorts allow up to 3 minutes; 60 s is the sweet spot

const CATEGORY_IDS = { 'Film & Animation': '1', 'Autos & Vehicles': '2', Music: '10', 'Pets & Animals': '15', Sports: '17',
  'Travel & Events': '19', Gaming: '20', 'People & Blogs': '22', Comedy: '23', Entertainment: '24', 'News & Politics': '25',
  'Howto & Style': '26', Education: '27', 'Science & Technology': '28' };

export class PlanError extends Error {}

/* Check a plan and fill defaults. Throws PlanError with a plain sentence. */
export function checkPlan(plan) {
  if (!plan || typeof plan !== 'object') throw new PlanError('The plan is empty.');
  const sources = plan.sources || {};
  const names = Object.keys(sources);
  if (!names.length) throw new PlanError('The plan names no sources.');
  for (const name of names) {
    const s = sources[name];
    if (!s.file) throw new PlanError('Source "' + name + '" has no file.');
    if (!s.page) throw new PlanError('Source "' + name + '" has no page to credit.');
    const lic = classifyLicense(s.license_url || s.license);
    if (!lic.ok) throw new PlanError('Source "' + name + '" cannot be used: ' + lic.reason + '.');
    s.license = lic.kind;
  }
  const scenes = plan.scenes || [];
  if (!scenes.length) throw new PlanError('The plan has no scenes.');
  for (const [i, sc] of scenes.entries()) {
    if (!sources[sc.source]) throw new PlanError('Scene ' + (i + 1) + ' uses an unknown source "' + sc.source + '".');
    if (!(Number(sc.start) >= 0)) throw new PlanError('Scene ' + (i + 1) + ' needs a start time in seconds.');
    if (!(Number(sc.length) > 0 && Number(sc.length) <= 15)) throw new PlanError('Scene ' + (i + 1) + ' needs a length between 0 and 15 seconds.');
  }
  const total = sceneTotal(plan);
  if (total > MAX_SHORT_SECONDS) throw new PlanError('The short would be ' + total + ' s; keep it to ' + MAX_SHORT_SECONDS + ' s or less.');
  const up = plan.upload || {};
  if (!up.title || String(up.title).length > 100) throw new PlanError('The upload needs a title of 100 characters or fewer.');
  if (!CATEGORY_IDS[up.category || 'Education']) throw new PlanError('Unknown category "' + up.category + '".');
  return plan;
}

export function sceneTotal(plan) {
  const scenes = (plan.scenes || []).reduce((t, sc) => t + Number(sc.length || 0), 0);
  return Math.round((scenes + (plan.hook ? 2.5 : 0) + (plan.end ? 2 : 0)) * 10) / 10;
}

/* Text for ffmpeg's drawtext, kept in a file so no quoting can break it.
 * Long lines are wrapped so they fit the 1080 px width. */
export function wrap(text, maxChars) {
  const words = String(text || '').split(/\s+/).filter(Boolean);
  const lines = [];
  let line = '';
  for (const w of words) {
    if (line && (line + ' ' + w).length > maxChars) { lines.push(line); line = w; } else line = line ? line + ' ' + w : w;
  }
  if (line) lines.push(line);
  return lines.join('\n');
}

/* ffmpeg arguments for one card (hook or end): plain background, big text. */
export function cardArgs({ text, seconds, font, textFile, out }) {
  return ['-nostdin', '-loglevel', 'error', '-y',
    '-f', 'lavfi', '-i', `color=c=0x111111:s=${WIDTH}x${HEIGHT}:d=${seconds}:r=30`,
    '-f', 'lavfi', '-i', 'anullsrc=r=44100:cl=stereo', '-t', String(seconds),
    '-vf', `drawtext=fontfile=${font}:textfile=${textFile}:fontsize=84:fontcolor=white:line_spacing=24:x=(w-text_w)/2:y=(h-text_h)/2:borderw=6:bordercolor=black`,
    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-r', '30', '-c:a', 'aac', '-ar', '44100', '-ac', '2', '-shortest', out];
}

/* ffmpeg arguments for one scene: the clip centred on a blurred copy of
 * itself (so wide old film fills a tall screen), caption near the bottom.
 * The source's own sound is dropped: old film audio is often damaged and
 * the caption carries the story. */
export function sceneArgs({ file, start, length, font, textFile, out }) {
  const filter =
    `[0:v]split[a][b];` +
    `[a]scale=${WIDTH}:${HEIGHT}:force_original_aspect_ratio=increase,crop=${WIDTH}:${HEIGHT},boxblur=24:2,eq=brightness=-0.15[bg];` +
    `[b]scale=${WIDTH}:-2[fg];` +
    `[bg][fg]overlay=(W-w)/2:(H-h)/2,` +
    (textFile ? `drawtext=fontfile=${font}:textfile=${textFile}:fontsize=58:fontcolor=white:line_spacing=16:x=(w-text_w)/2:y=h-440:box=1:boxcolor=black@0.55:boxborderw=24,` : '') +
    `fps=30,format=yuv420p[v]`;
  return ['-nostdin', '-loglevel', 'error', '-y',
    '-ss', String(start), '-t', String(length), '-i', file,
    '-f', 'lavfi', '-i', 'anullsrc=r=44100:cl=stereo',
    '-filter_complex', filter, '-map', '[v]', '-map', '1:a', '-t', String(length),
    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-r', '30', '-c:a', 'aac', '-ar', '44100', '-ac', '2', out];
}

/* The upload details, with a credit for every source appended. */
export function uploadDetails(plan) {
  const up = plan.upload || {};
  const used = [...new Set(plan.scenes.map((sc) => sc.source))];
  const credits = used.map((n) => '- ' + creditLine(plan.sources[n]));
  const description = [String(up.description || '').trim(), '', 'Footage:', ...credits].join('\n').trim();
  const tags = (up.tags || []).map(String).filter(Boolean);
  if (!tags.includes('shorts')) tags.push('shorts');
  return {
    title: String(up.title),
    description: description.slice(0, 5000),
    tags: tags.slice(0, 30),
    category: up.category || 'Education',
    category_id: CATEGORY_IDS[up.category || 'Education'],
    default_language: up.language || 'en',
    made_for_kids: up.made_for_kids === true,
    privacy_status: up.privacy || 'private',
    license: 'youtube',
    embeddable: true,
    notify_subscribers: up.notify_subscribers !== false,
    public_stats_viewable: true
  };
}
