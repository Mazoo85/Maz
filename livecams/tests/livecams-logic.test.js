/*
 * LIVE CAMS — logic tests.
 *
 *   node livecams/tests/livecams-logic.test.js
 *
 * No dependencies and no browser: the engine is a plain module. What these
 * prove is the promise the app makes — every common way of copying a YouTube
 * link lands on the wall, junk is turned away with a plain message, the same
 * stream can't be pinned twice, and a saved wall comes back exactly as it
 * went in.
 */
'use strict';

const path = require('path');
const L = require(path.join(__dirname, '..', 'js', 'livecams.js'));

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

const ID = 'dQw4w9WgXcQ';
const CH = 'UCxxxxxxxxxxxxxxxxxxxxxx';

test('reads every common shape of YouTube video link', () => {
  [
    'https://www.youtube.com/watch?v=' + ID,
    'https://youtube.com/watch?v=' + ID + '&t=30s',
    'https://m.youtube.com/watch?v=' + ID,
    'youtube.com/watch?v=' + ID,
    'https://youtu.be/' + ID,
    'https://youtu.be/' + ID + '?si=abc',
    'https://www.youtube.com/live/' + ID + '?feature=share',
    'https://www.youtube.com/embed/' + ID,
    'https://www.youtube.com/shorts/' + ID,
    '  ' + ID + '  '
  ].forEach((link) => {
    const p = L.parseLink(link);
    assert(p, 'not read: ' + link);
    eq(p.kind, 'video', link);
    eq(p.id, ID, link);
  });
});

test('reads channel links, which embed whatever the channel has live', () => {
  ['https://www.youtube.com/channel/' + CH,
    'https://www.youtube.com/channel/' + CH + '/live',
    'https://www.youtube.com/embed/live_stream?channel=' + CH,
    CH].forEach((link) => {
    const p = L.parseLink(link);
    assert(p, 'not read: ' + link);
    eq(p.kind, 'channel', link);
    eq(p.id, CH, link);
  });
});

test('other https pages are kept as links to open, not embedded', () => {
  const p = L.parseLink('https://www.skylinewebcams.com/en/webcam.html');
  eq(p.kind, 'web');
  eq(L.embedUrl(p), null);
  eq(L.watchUrl(p), 'https://www.skylinewebcams.com/en/webcam.html');
});

test('turns away junk and unsafe links', () => {
  ['', '   ', null, 'hello there', 'javascript:alert(1)', 'http://example.com/cam',
    'ftp://example.com', 'https://www.youtube.com/@somebody',
    'https://www.youtube.com/results?search_query=kyiv', 'https://youtu.be/short'
  ].forEach((link) => eq(L.parseLink(link), null, String(link)));
});

test('embeds start muted and play inline on phones', () => {
  const v = L.embedUrl({ kind: 'video', id: ID });
  assert(v.indexOf('https://www.youtube-nocookie.com/embed/' + ID) === 0, v);
  ['autoplay=1', 'mute=1', 'playsinline=1'].forEach((k) => assert(v.indexOf(k) > 0, k));
  const c = L.embedUrl({ kind: 'channel', id: CH });
  assert(c.indexOf('/embed/live_stream?') > 0 && c.indexOf('channel=' + CH) > 0, c);
});

test('live search links are filtered to what is live right now', () => {
  const u = L.searchUrl('Tel Aviv');
  eq(u, 'https://www.youtube.com/results?search_query=Tel+Aviv+live+cam&sp=EgJAAQ%3D%3D');
  L.PLACES.forEach((p) => {
    assert(p.cities.length > 0, p.area);
    assert(L.AREAS.indexOf(p.area) >= 0, p.area);
  });
});

test('makeStream names, files and explains', () => {
  const s = L.makeStream('https://youtu.be/' + ID, '  Kyiv   skyline ', 'Ukraine');
  eq(s.name, 'Kyiv skyline');
  eq(s.area, 'Ukraine');
  eq(L.makeStream(ID, '', 'Nowhere').area, 'Other');
  eq(L.makeStream(ID, '', '').name, 'Live stream');
  eq(L.makeStream('https://www.earthcam.com/x', '', '').name, 'earthcam.com');
  assert(L.makeStream('nope', 'x', 'Other').error, 'junk gives an error');
  eq(L.makeStream(ID, 'x'.repeat(200), '').name.length, 60);
});

test('the same stream cannot be pinned twice, however it was pasted', () => {
  let list = L.addStream([], L.makeStream('https://youtu.be/' + ID, 'a', 'Ukraine')).list;
  const again = L.addStream(list, L.makeStream('https://www.youtube.com/watch?v=' + ID, 'b', 'Israel'));
  assert(again.error, 'duplicate refused');
  eq(again.list.length, 1);
  list = L.addStream(list, L.makeStream(CH, 'c', 'Israel')).list;
  eq(list.length, 2);
  eq(list[0].name, 'c', 'newest first');
});

test('the wall has a ceiling', () => {
  let list = [];
  for (let i = 0; i < L.MAX_STREAMS; i++) {
    const id = ('0000000000' + i).slice(-10) + 'a';
    list = L.addStream(list, L.makeStream(id, '', '')).list;
  }
  eq(list.length, L.MAX_STREAMS);
  const over = L.addStream(list, L.makeStream(ID, '', ''));
  assert(over.error, 'full wall refuses');
  eq(over.list.length, L.MAX_STREAMS);
});

test('remove and filter', () => {
  let list = [];
  list = L.addStream(list, L.makeStream(ID, 'a', 'Ukraine')).list;
  list = L.addStream(list, L.makeStream(CH, 'b', 'Israel')).list;
  eq(L.filterByArea(list, 'Ukraine').length, 1);
  eq(L.filterByArea(list, 'All').length, 2);
  eq(L.filterByArea(list, '').length, 2);
  list = L.removeStream(list, L.keyOf(list[0]));
  eq(list.length, 1);
  eq(list[0].name, 'a');
});

test('a saved wall comes back exactly as it went in', () => {
  let list = [];
  list = L.addStream(list, L.makeStream(ID, 'Kyiv', 'Ukraine')).list;
  list = L.addStream(list, L.makeStream(CH, 'Jerusalem', 'Israel')).list;
  list = L.addStream(list, L.makeStream('https://www.earthcam.com/x', 'Earth', 'Other')).list;
  const back = L.deserialize(L.serialize(list));
  eq(JSON.stringify(back), JSON.stringify(list));
});

test('a broken or tampered save gives an empty or cleaned wall, never a crash', () => {
  ['', 'not json', 'null', '[]', '{"version":99,"streams":[]}', '{"version":1}'].forEach((t) => {
    eq(L.deserialize(t).length, 0, t);
  });
  const t = JSON.stringify({ version: 1, streams: [
    { kind: 'video', id: ID, name: 'ok', area: 'Ukraine' },
    { kind: 'video', id: ID, name: 'dupe', area: 'Ukraine' },
    { kind: 'video', id: '"><script>', name: 'bad' },
    { kind: 'web', url: 'javascript:alert(1)' },
    null, 7
  ] });
  const back = L.deserialize(t);
  eq(back.length, 1);
  eq(back[0].name, 'ok');
});

console.log('\n' + passed + ' passed, ' + failures.length + ' failed');
if (failures.length) process.exit(1);
