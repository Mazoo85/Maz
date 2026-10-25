/*
 * Archive shorts - tested without a network.
 *
 *   node youtube/shorts/tests/shorts.test.mjs
 *
 * The license rules, both sites' answers (from saved examples of their API
 * shape), the plan checks and the upload details are checked directly. If
 * ffmpeg is installed, a real short is also made end to end from a generated
 * test clip, and its size and length are checked.
 */
import { spawnSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, readFileSync, existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  classifyLicense, parseLength, archiveSearchUrl, archiveCandidates, archiveVideoFile, yearOf,
  commonsSearchUrl, commonsCandidates, creditLine
} from '../sources.mjs';
import { checkPlan, uploadDetails, wrap, sceneTotal, sceneArgs, PlanError } from '../compose.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const tests = [];
const test = (name, fn) => tests.push([name, fn]);
function eq(actual, expected, message) {
  const a = JSON.stringify(actual);
  const e = JSON.stringify(expected);
  if (a !== e) throw new Error((message || 'values differ') + ': got ' + a + ', expected ' + e);
}
function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}
function throwsPlan(fn, pattern) {
  try {
    fn();
  } catch (e) {
    assert(e instanceof PlanError, 'a PlanError, got ' + e);
    assert(pattern.test(e.message), 'message ' + JSON.stringify(e.message) + ' should match ' + pattern);
    return;
  }
  throw new Error('expected a PlanError matching ' + pattern);
}

/* ------------------------------------------------------------- licenses */

test('public domain, CC0 and the Public Domain Mark are accepted as public domain', () => {
  for (const s of ['http://creativecommons.org/publicdomain/mark/1.0/', 'https://creativecommons.org/publicdomain/zero/1.0/',
    'Public domain', 'CC0', 'PD-USGov-NASA', 'PD-old']) {
    eq(classifyLicense(s).kind, 'public domain', s);
  }
});

test('CC BY and CC BY-SA are accepted', () => {
  eq(classifyLicense('http://creativecommons.org/licenses/by/3.0/').kind, 'CC BY');
  eq(classifyLicense('CC BY 4.0').kind, 'CC BY');
  eq(classifyLicense('https://creativecommons.org/licenses/by-sa/4.0/').kind, 'CC BY-SA');
  eq(classifyLicense('CC BY-SA 3.0').kind, 'CC BY-SA');
});

test('NonCommercial, NoDerivatives, unknown and missing licenses are refused with a reason', () => {
  assert(/NonCommercial/.test(classifyLicense('http://creativecommons.org/licenses/by-nc/3.0/').reason));
  assert(/NonCommercial/.test(classifyLicense('CC BY-NC-SA 4.0').reason));
  assert(/NoDerivatives/.test(classifyLicense('http://creativecommons.org/licenses/by-nd/4.0/').reason));
  assert(/no license/.test(classifyLicense('').reason));
  assert(/not recognised/.test(classifyLicense('All rights reserved').reason));
  eq(classifyLicense('Fair use').ok, false);
});

test('lengths are read in every form the sites use', () => {
  eq(parseLength('754.3'), 754.3);
  eq(parseLength('12:34'), 754);
  eq(parseLength('1:02:03'), 3723);
  eq(parseLength(''), null);
  eq(parseLength('soon'), null);
});

/* ------------------------------------------------------- Internet Archive */

test('the Archive search asks for licensed videos without wildcards (they break its search), most downloaded first', () => {
  const u = new URL(archiveSearchUrl('eiffel tower', 5));
  const q = u.searchParams.get('q');
  assert(/mediatype:\(movies\)/.test(q) && /licenseurl:\[\* TO \*\]/.test(q), q);
  assert(!/\*publicdomain\*/.test(q), 'no wildcard license filter');
  eq(u.searchParams.get('rows'), '25', 'asks for extra, since some licenses get refused');
  eq(u.searchParams.getAll('sort[]'), ['downloads desc']);
});

test('Archive results are licence-checked again, whatever the search returned', () => {
  const json = { response: { docs: [
    { identifier: 'eiffel1889', title: 'Paris Exposition <b>1889</b>', licenseurl: 'http://creativecommons.org/publicdomain/mark/1.0/', year: 1889, downloads: 5000, description: '<p>The tower &amp; the fair</p>' },
    { identifier: 'nc-film', title: 'NC film', licenseurl: 'http://creativecommons.org/licenses/by-nc/3.0/', downloads: 9999 },
    { identifier: 'no-licence', title: 'Mystery reel', downloads: 1 }
  ] } };
  const c = archiveCandidates(json);
  eq(c.map((x) => x.ok), [true, false, false]);
  eq(c[0].title, 'Paris Exposition 1889');
  eq(c[0].description, 'The tower & the fair');
  eq(c[0].page, 'https://archive.org/details/eiffel1889');
  eq([c[0].license, c[0].year, c[0].popularity], ['public domain', '1889', 5000]);
});

test('an Archive search error is reported, not mistaken for "nothing found"', () => {
  let message = '';
  try { archiveCandidates({ error: '[BACKEND_ERROR] Invalid or no response from Elasticsearch' }); } catch (e) { message = e.message; }
  assert(/search failed: \[BACKEND_ERROR\]/.test(message), message);
  eq(classifyLicense('http://creativecommons.org/licenses/publicdomain/').kind, 'public domain', "the Archive's old public-domain address");
});

test('the film year comes from the title before any upload date', () => {
  eq(yearOf('Titanic_Disaster_-_Genuine_Footage_(1911-1912).webm'), '1911-1912');
  eq(yearOf('La hantise (1912)'), '1912');
  eq(yearOf('2014-03-02 12:00'), '2014');
  eq(yearOf('no year'), null);
});

test('the largest h.264 file of an Archive item is chosen to cut from', () => {
  const json = { metadata: { identifier: 'eiffel1889' }, files: [
    { name: 'eiffel1889.ogv', format: 'Ogg Video', size: '9000000' },
    { name: 'eiffel1889_512kb.mp4', format: '512Kb MPEG4', size: '2000000', length: '03:20' },
    { name: 'eiffel 1889.mp4', format: 'h.264', size: '5000000', length: '200.5', width: '640', height: '480' }
  ] };
  const f = archiveVideoFile(json);
  eq(f.url, 'https://archive.org/download/eiffel1889/eiffel%201889.mp4');
  eq([f.seconds, f.width, f.height], [200.5, 640, 480]);
  eq(archiveVideoFile({ metadata: { identifier: 'x' }, files: [{ name: 'x.ogv', format: 'Ogg Video' }] }), null);
});

/* ------------------------------------------------------ Wikimedia Commons */

test('the Commons search asks for video files with their license', () => {
  const u = new URL(commonsSearchUrl('hurricane', 10));
  eq(u.searchParams.get('gsrsearch'), 'filetype:video hurricane');
  eq(u.searchParams.get('gsrnamespace'), '6');
  assert(/extmetadata/.test(u.searchParams.get('iiprop')));
});

test('Commons results carry the direct file, license and author', () => {
  const page = (title, lic, extra) => Object.assign({ title: title, index: 1, imageinfo: [{
    url: 'https://upload.wikimedia.org/x/' + title.slice(5), descriptionurl: 'https://commons.wikimedia.org/wiki/' + title,
    mediatype: 'VIDEO', duration: 42.5, width: 1920, height: 1080,
    extmetadata: { LicenseShortName: { value: lic }, Artist: { value: '<a href="#">NASA</a>' }, DateTimeOriginal: { value: '2005-08-28' } }
  }] }, extra || {});
  const json = { query: { pages: {
    1: page('File:Hurricane_Katrina.webm', 'Public domain', { index: 1 }),
    2: page('File:Storm_chaser.webm', 'CC BY-NC 4.0', { index: 2 }),
    3: page('File:Storm.jpg', 'CC BY 4.0', { index: 3 })
  } } };
  json.query.pages[3].imageinfo[0].mediatype = 'BITMAP';
  const c = commonsCandidates(json);
  eq(c.map((x) => x.ok), [true, false, false]);
  eq(c[0].title, 'Hurricane Katrina');
  eq([c[0].license, c[0].creator, c[0].year], ['public domain', 'NASA', '2005']);
  eq(c[0].file.seconds, 42.5);
  assert(/NonCommercial/.test(c[1].reason));
  eq(c[2].reason, 'not a video file');
});

test('credit lines name the work, year, author, site and license', () => {
  eq(creditLine({ title: 'Paris 1889', year: '1889', source: 'Internet Archive', license: 'public domain', page: 'https://archive.org/details/p' }),
    '"Paris 1889" (1889), Internet Archive, public domain: https://archive.org/details/p');
  eq(creditLine({ title: 'Storm', creator: 'Ann', source: 'Wikimedia Commons', license: 'CC BY-SA', license_url: 'https://creativecommons.org/licenses/by-sa/4.0/', page: 'https://c/p' }),
    '"Storm" by Ann, Wikimedia Commons, licensed CC BY-SA (https://creativecommons.org/licenses/by-sa/4.0/): https://c/p');
});

/* --------------------------------------------------------------- plans */

function plan(over) {
  return Object.assign({
    hook: 'THE TOWER PARIS HATED',
    end: 'WOULD YOU HAVE BUILT IT?',
    sources: { a: { file: 'clip.mp4', title: 'Paris 1889', license: 'http://creativecommons.org/publicdomain/mark/1.0/', page: 'https://archive.org/details/p', source: 'Internet Archive', year: '1889' } },
    scenes: [{ source: 'a', start: 1, length: 3, caption: 'In 1887, 300 workers began' }, { source: 'a', start: 5, length: 3, caption: 'Artists called it a disgrace' }],
    upload: { title: 'The Tower Paris Hated #shorts', description: 'How the Eiffel Tower went from hated to loved.', tags: ['eiffel tower', 'history'], category: 'Education' }
  }, over || {});
}

test('a good plan passes, and its length counts the cards', () => {
  const p = checkPlan(plan());
  eq(p.sources.a.license, 'public domain');
  eq(sceneTotal(p), 10.5);
});

test('a plan is refused, with a plain reason, for each kind of mistake', () => {
  throwsPlan(() => checkPlan(plan({ sources: {} })), /no sources/);
  throwsPlan(() => checkPlan(plan({ sources: { a: { file: 'x', page: 'p', license: 'CC BY-NC 4.0' } } })), /NonCommercial/);
  throwsPlan(() => checkPlan(plan({ sources: { a: { file: 'x', license: 'CC0' } } })), /no page/);
  throwsPlan(() => checkPlan(plan({ scenes: [] })), /no scenes/);
  throwsPlan(() => checkPlan(plan({ scenes: [{ source: 'b', start: 0, length: 3 }] })), /unknown source/);
  throwsPlan(() => checkPlan(plan({ scenes: [{ source: 'a', start: 0, length: 20 }] })), /between 0 and 15/);
  throwsPlan(() => checkPlan(plan({ scenes: Array.from({ length: 5 }, () => ({ source: 'a', start: 0, length: 12 })) })), /keep it to 60/);
  throwsPlan(() => checkPlan(plan({ upload: { title: 'x'.repeat(101) } })), /100 characters/);
  throwsPlan(() => checkPlan(plan({ upload: { title: 't', category: 'Cooking' } })), /Unknown category/);
});

test('upload details fill every field and append a credit for each source used', () => {
  const d = uploadDetails(checkPlan(plan()));
  eq(d.title, 'The Tower Paris Hated #shorts');
  assert(d.description.startsWith('How the Eiffel Tower went from hated to loved.\n\nFootage:\n- "Paris 1889" (1889)'), d.description);
  eq(d.tags, ['eiffel tower', 'history', 'shorts']);
  eq([d.category, d.category_id, d.default_language], ['Education', '27', 'en']);
  eq([d.made_for_kids, d.privacy_status, d.license, d.notify_subscribers], [false, 'private', 'youtube', true]);
});

test('captions are wrapped to fit the screen', () => {
  eq(wrap('In 1887, three hundred workers began building the tower', 20), 'In 1887, three\nhundred workers\nbegan building the\ntower');
});

test('each scene drops the film sound and puts the clip on a blurred, tall background', () => {
  const args = sceneArgs({ file: 'in.mp4', start: 2, length: 3, font: 'f.ttf', textFile: 'c.txt', out: 'o.mp4' }).join(' ');
  assert(/-ss 2 -t 3 -i in\.mp4/.test(args), 'cuts only the needed part');
  assert(/boxblur/.test(args) && /1080:1920/.test(args), 'blurred tall background');
  assert(/-map 1:a/.test(args), 'silent track, not the film sound');
});

/* ------------------------------------------------ end to end, with ffmpeg */

const hasFfmpeg = spawnSync('ffmpeg', ['-version']).status === 0;
test('a real short is made from a test clip: 1080x1920, the planned length, with upload details' + (hasFfmpeg ? '' : ' (skipped: no ffmpeg)'), () => {
  if (!hasFfmpeg) return;
  const dir = mkdtempSync(join(tmpdir(), 'shorts-'));
  const clip = join(dir, 'clip.mp4');
  const made = spawnSync('ffmpeg', ['-nostdin', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', 'testsrc=size=640x480:rate=25:duration=10',
    '-f', 'lavfi', '-i', 'sine=frequency=440:duration=10', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-shortest', clip]);
  eq(made.status, 0, 'test clip made');
  writeFileSync(join(dir, 'plan.json'), JSON.stringify(plan()));
  const run = spawnSync(process.execPath, [join(HERE, '..', 'cli.mjs'), 'make', join(dir, 'plan.json'), '--out', join(dir, 'out')], { encoding: 'utf8' });
  eq(run.status, 0, 'make finished: ' + run.stderr);
  const short = join(dir, 'out', 'short.mp4');
  assert(existsSync(short), 'short.mp4 exists');
  const probe = spawnSync('ffprobe', ['-v', 'error', '-show_entries', 'stream=codec_type,width,height:format=duration', '-of', 'json', short], { encoding: 'utf8' });
  const info = JSON.parse(probe.stdout);
  const video = info.streams.find((s) => s.codec_type === 'video');
  eq([video.width, video.height], [1080, 1920]);
  assert(info.streams.some((s) => s.codec_type === 'audio'), 'has a sound track (silent)');
  const seconds = Number(info.format.duration);
  assert(Math.abs(seconds - 10.5) < 0.4, 'about 10.5 s long, got ' + seconds);
  const up = JSON.parse(readFileSync(join(dir, 'out', 'upload.json'), 'utf8'));
  assert(/Footage:\n- "Paris 1889"/.test(up.description), 'credit in description');
});

/* ---------------------------------------------------------------- run */

let failed = 0;
for (const [name, fn] of tests) {
  try {
    await fn();
    console.log('ok   ' + name);
  } catch (e) {
    failed++;
    console.log('FAIL ' + name + '\n     ' + (e && e.stack || e));
  }
}
console.log(failed ? failed + ' of ' + tests.length + ' failed' : 'all ' + tests.length + ' passed');
process.exit(failed ? 1 : 0);
