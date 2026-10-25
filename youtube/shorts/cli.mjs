#!/usr/bin/env node
/*
 * Archive shorts: public-domain and openly licensed footage -> YouTube Shorts.
 *
 *   node youtube/shorts/cli.mjs search "<topic>" [--rows 20]
 *       Searches the Internet Archive and Wikimedia Commons, keeps only
 *       videos whose license allows editing and re-uploading, and prints
 *       each with its license, length and direct file address.
 *
 *   node youtube/shorts/cli.mjs make <plan.json> [--out <folder>]
 *       Cuts the scenes in the plan into a vertical short (<folder>/short.mp4)
 *       and writes the upload details with every credit (<folder>/upload.json).
 *
 * Needs Node 18+ and ffmpeg. See README.md beside this file.
 */
import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join, resolve, dirname } from 'node:path';
import {
  archiveSearchUrl, archiveCandidates, archiveMetadataUrl, archiveVideoFile,
  commonsSearchUrl, commonsCandidates
} from './sources.mjs';
import { checkPlan, cardArgs, sceneArgs, uploadDetails, wrap, sceneTotal, PlanError } from './compose.mjs';

const FONTS = ['/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 'C:/Windows/Fonts/arialbd.ttf', '/System/Library/Fonts/Supplemental/Arial Bold.ttf', '/Library/Fonts/Arial Bold.ttf'];
const USER_AGENT = 'maz-archive-shorts/1.0 (+https://github.com/Mazoo85/Maz)';

function fail(message) {
  console.error('Stopped: ' + message);
  process.exit(1);
}

/* JSON from a site, waiting and retrying when it says "too many requests". */
async function getJson(url) {
  for (let attempt = 1; ; attempt++) {
    const res = await fetch(url, { headers: { 'User-Agent': USER_AGENT, Accept: 'application/json' } });
    if (res.status === 429 && attempt < 4) {
      const wait = Math.min(30, Number(res.headers.get('retry-after')) || attempt * 5);
      await new Promise((r) => setTimeout(r, wait * 1000));
      continue;
    }
    if (!res.ok) throw new Error(new URL(url).host + ' answered ' + res.status);
    return res.json();
  }
}

function option(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

async function search(topic, rows) {
  const results = [];
  try {
    const found = archiveCandidates(await getJson(archiveSearchUrl(topic, rows))).filter((c) => c.ok).slice(0, rows);
    for (const c of found) {
      try {
        c.file = archiveVideoFile(await getJson(archiveMetadataUrl(c.id)));
      } catch (e) {
        c.file = null;
      }
      if (c.file) results.push(c);
    }
  } catch (e) {
    console.error('Internet Archive: ' + e.message);
  }
  try {
    results.push(...commonsCandidates(await getJson(commonsSearchUrl(topic, rows))).filter((c) => c.ok && c.file));
  } catch (e) {
    console.error('Wikimedia Commons: ' + e.message);
  }
  return results;
}

function ffmpeg(args) {
  const r = spawnSync('ffmpeg', args, { encoding: 'utf8' });
  if (r.error) fail('ffmpeg is not installed (' + r.error.message + ').');
  if (r.status !== 0) fail('ffmpeg could not make a piece of the video:\n' + (r.stderr || '').slice(-1500));
}

function make(planPath, outDir) {
  let plan;
  try {
    plan = checkPlan(JSON.parse(readFileSync(planPath, 'utf8')));
  } catch (e) {
    fail(e instanceof PlanError ? e.message : 'could not read the plan: ' + e.message);
  }
  const font = FONTS.find((f) => existsSync(f));
  if (!font) fail('no font found for captions.');
  mkdirSync(join(outDir, 'parts'), { recursive: true });
  const base = dirname(resolve(planPath));
  const parts = [];
  const text = (name, words, width) => {
    const p = join(outDir, 'parts', name + '.txt');
    writeFileSync(p, wrap(words, width));
    return p;
  };
  if (plan.hook) {
    const out = join(outDir, 'parts', 'hook.mp4');
    ffmpeg(cardArgs({ text: plan.hook, seconds: 2.5, font, textFile: text('hook', plan.hook, 16), out }));
    parts.push(out);
  }
  plan.scenes.forEach((sc, i) => {
    const src = plan.sources[sc.source];
    const file = /^https?:\/\//.test(src.file) ? src.file : resolve(base, src.file);
    const out = join(outDir, 'parts', 'scene' + (i + 1) + '.mp4');
    console.log('Cutting scene ' + (i + 1) + ' of ' + plan.scenes.length + ' from "' + (src.title || sc.source) + '"');
    ffmpeg(sceneArgs({ file, start: Number(sc.start), length: Number(sc.length), font, textFile: sc.caption ? text('caption' + (i + 1), sc.caption, 28) : null, out }));
    parts.push(out);
  });
  if (plan.end) {
    const out = join(outDir, 'parts', 'end.mp4');
    ffmpeg(cardArgs({ text: plan.end, seconds: 2, font, textFile: text('end', plan.end, 16), out }));
    parts.push(out);
  }
  const list = join(outDir, 'parts', 'list.txt');
  writeFileSync(list, parts.map((p) => "file '" + resolve(p).replace(/'/g, "'\\''") + "'").join('\n') + '\n');
  const short = join(outDir, 'short.mp4');
  ffmpeg(['-nostdin', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', list, '-c', 'copy', '-movflags', '+faststart', short]);
  const details = uploadDetails(plan);
  writeFileSync(join(outDir, 'upload.json'), JSON.stringify(details, null, 2) + '\n');
  console.log('\nMade ' + short + ' (' + sceneTotal(plan) + ' s) and ' + join(outDir, 'upload.json'));
}

const [cmd, ...args] = process.argv.slice(2);
if (cmd === 'search' && args[0]) {
  const rows = Number(option(args, '--rows', 20));
  const found = await search(args[0], rows);
  if (!found.length) console.log('Nothing usable found for "' + args[0] + '".');
  for (const c of found) {
    const len = c.file && c.file.seconds ? Math.round(c.file.seconds) + ' s' : 'length unknown';
    console.log('\n' + c.title + (c.year ? ' (' + c.year + ')' : '') + '  [' + c.source + ', ' + c.license + ', ' + len + (c.popularity ? ', ' + c.popularity.toLocaleString() + ' downloads' : '') + ']');
    console.log('  page: ' + c.page);
    console.log('  file: ' + c.file.url);
    if (c.description) console.log('  ' + c.description.slice(0, 160));
  }
} else if (cmd === 'make' && args[0]) {
  make(args[0], option(args, '--out', join(dirname(resolve(args[0])), 'out')));
} else {
  console.log('Usage:\n  node youtube/shorts/cli.mjs search "<topic>" [--rows 20]\n  node youtube/shorts/cli.mjs make <plan.json> [--out <folder>]');
  process.exit(cmd ? 1 : 0);
}
