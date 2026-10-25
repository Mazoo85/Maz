/*
 * YouTube for Claude - tested without a network.
 *
 *   node youtube/tests/worker.test.mjs
 *
 * The worker (youtube/worker.js) runs as a Cloudflare Worker, which is the
 * standard fetch/Request/Response API that Node also has. YouTube is replaced
 * by a fake fetch, so every path is checked: the password, the MCP handshake,
 * each tool's happy path, every link form, the license filter, and each plain
 * error sentence.
 */
import worker, {
  parseVideoId, parseChannel, parsePlaylistId, parseDuration, formatDuration,
  shapeVideo, explainApiError, viewsPerDay, TOOLS
} from '../worker.js';

const ENV = { YOUTUBE_API_KEY: 'test-key', CONNECTOR_PASSWORD: 'open sesame' };
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

/* ------------------------------------------------------- a fake YouTube */

function video(id, extra) {
  return Object.assign({
    id: id,
    snippet: { title: 'Video ' + id, channelId: 'UCabcdefghijklmnopqrstuv', channelTitle: 'Maker', publishedAt: '2026-01-02T03:04:05Z', description: 'About ' + id, tags: ['a', 'b'] },
    contentDetails: { duration: 'PT1H2M3S' },
    statistics: { viewCount: '1200', likeCount: '34', commentCount: '5' },
    status: { license: 'creativeCommon' }
  }, extra || {});
}
const CHANNEL = {
  id: 'UCabcdefghijklmnopqrstuv',
  snippet: { title: 'Maker', customUrl: '@maker', country: 'US', publishedAt: '2020-01-01T00:00:00Z', description: 'Makes things' },
  statistics: { subscriberCount: '5000', videoCount: '12', viewCount: '99999' },
  contentDetails: { relatedPlaylists: { uploads: 'UUabcdefghijklmnopqrstuv' } }
};

/* answers: { videos: {...}, search: {...}, ... } by API path; each is
 * { status, body } or a function(url) returning one. */
function fakeNetwork(answers) {
  const asked = [];
  globalThis.fetch = async (url) => {
    const u = new URL(String(url));
    const path = u.pathname.split('/').pop();
    asked.push(u);
    let a = answers[path];
    if (typeof a === 'function') a = a(u);
    if (!a) throw new Error('network down');
    if (a === 'timeout') { const e = new Error('aborted'); e.name = 'AbortError'; throw e; }
    return new Response(JSON.stringify(a.body), { status: a.status || 200 });
  };
  return asked;
}

async function rpc(method, params, options) {
  const o = options || {};
  const headers = { 'Content-Type': 'application/json' };
  if (o.bearer) headers.Authorization = 'Bearer ' + o.bearer;
  const req = new Request('https://yt.example' + (o.path || '/mcp/open%20sesame'), {
    method: o.method || 'POST',
    headers: headers,
    body: o.raw !== undefined ? o.raw : JSON.stringify(Object.assign({ jsonrpc: '2.0', method: method, params: params }, o.noId ? {} : { id: 1 }))
  });
  const res = await worker.fetch(req, o.env === undefined ? ENV : o.env);
  const text = await res.text();
  return { status: res.status, body: text ? JSON.parse(text) : null, headers: res.headers };
}

async function call(tool, args, options) {
  const r = await rpc('tools/call', { name: tool, arguments: args }, options);
  eq(r.status, 200, 'tools/call status');
  const result = r.body.result;
  return { isError: !!result.isError, text: result.content[0].text, data: result.isError ? null : JSON.parse(result.content[0].text) };
}

/* --------------------------------------------------------- pure helpers */

test('video ids are read from every link form', () => {
  eq(parseVideoId('dQw4w9WgXcQ'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=10'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://youtu.be/dQw4w9WgXcQ?si=x'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://youtube.com/shorts/dQw4w9WgXcQ'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://m.youtube.com/live/dQw4w9WgXcQ'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ'), 'dQw4w9WgXcQ');
  eq(parseVideoId('https://vimeo.com/123'), null);
  eq(parseVideoId('not a video'), null);
  eq(parseVideoId(''), null);
});

test('channels are read as id, @handle or username', () => {
  eq(parseChannel('UCabcdefghijklmnopqrstuv'), { id: 'UCabcdefghijklmnopqrstuv' });
  eq(parseChannel('https://www.youtube.com/channel/UCabcdefghijklmnopqrstuv/videos'), { id: 'UCabcdefghijklmnopqrstuv' });
  eq(parseChannel('@maker'), { forHandle: '@maker' });
  eq(parseChannel('https://youtube.com/@maker'), { forHandle: '@maker' });
  eq(parseChannel('maker'), { forHandle: '@maker' });
  eq(parseChannel('https://www.youtube.com/c/maker'), { forHandle: '@maker' });
  eq(parseChannel('https://www.youtube.com/user/oldname'), { forUsername: 'oldname' });
  eq(parseChannel('https://example.com/@maker'), null);
  eq(parseChannel(''), null);
});

test('playlist ids are read from ids and links', () => {
  eq(parsePlaylistId('PLabcdefghijklmnop'), 'PLabcdefghijklmnop');
  eq(parsePlaylistId('https://www.youtube.com/playlist?list=PLabcdefghijklmnop'), 'PLabcdefghijklmnop');
  eq(parsePlaylistId('https://www.youtube.com/watch?v=dQw4w9WgXcQ&list=PLabcdefghijklmnop'), 'PLabcdefghijklmnop');
  eq(parsePlaylistId('https://example.com/?list=PLabcdefghijklmnop'), null);
  eq(parsePlaylistId('x'), null);
});

test('durations are read and shown', () => {
  eq(parseDuration('PT1H2M3S'), 3723);
  eq(parseDuration('PT45S'), 45);
  eq(parseDuration('PT3M'), 180);
  eq(parseDuration('P1DT1S'), 86401);
  eq(parseDuration('nope'), null);
  eq(formatDuration(3723), '1:02:03');
  eq(formatDuration(45), '0:45');
  eq(formatDuration(180), '3:00');
  eq(formatDuration(null), null);
});

test('a video is shaped the same way everywhere, with a credit when reusable', () => {
  const v = shapeVideo(video('abcdefghijk'));
  eq(v.url, 'https://www.youtube.com/watch?v=abcdefghijk');
  eq(v.channel, { id: 'UCabcdefghijklmnopqrstuv', title: 'Maker', url: 'https://www.youtube.com/channel/UCabcdefghijklmnopqrstuv' });
  eq(v.duration, { seconds: 3723, text: '1:02:03' });
  eq([v.views, v.likes, v.comments], [1200, 34, 5]);
  eq([v.license, v.reusable], ['creativeCommon', true]);
  eq(v.credit, 'Video by Maker (https://www.youtube.com/watch?v=abcdefghijk), licensed CC BY');
  const plain = shapeVideo(video('abcdefghijk', { status: { license: 'youtube' }, statistics: {} }));
  eq([plain.license, plain.reusable, plain.credit, plain.views], ['youtube', false, undefined, null]);
});

test("Google's error reasons become one plain sentence each", () => {
  const reason = (r, msg) => ({ error: { message: msg || '', errors: [{ reason: r }] } });
  assert(/daily allowance/.test(explainApiError(403, reason('quotaExceeded'))), 'quota');
  assert(/API key is wrong/.test(explainApiError(400, reason('badRequest', 'API key not valid. Please pass a valid API key.'))), 'bad key');
  assert(/not enabled/.test(explainApiError(403, reason('accessNotConfigured'))), 'not enabled');
  assert(/restricted/.test(explainApiError(403, reason('ipRefererBlocked'))), 'referer');
  assert(/turned off/.test(explainApiError(403, reason('commentsDisabled'))), 'comments');
  assert(/no public playlist/.test(explainApiError(404, reason('playlistNotFound'))), 'playlist');
  eq(explainApiError(500, null), 'YouTube answered 500.');
  eq(explainApiError(418, { error: { message: 'teapot' } }), 'YouTube answered 418: teapot.');
});

/* ------------------------------------------------------------- the door */

test('the wrong password, or none, is turned away', async () => {
  fakeNetwork({});
  const bad = await rpc('ping', {}, { path: '/mcp/wrong' });
  eq(bad.status, 401);
  assert(/password/.test(bad.body.error), 'says why');
  eq((await rpc('ping', {}, { path: '/mcp' })).status, 401);
  eq((await rpc('ping', {}, { path: '/mcp', bearer: 'nope' })).status, 401);
  eq((await rpc('ping', {}, { env: { YOUTUBE_API_KEY: 'k' } })).status, 401, 'no password set at all');
  eq((await rpc('ping', {}, { path: '/mcp/%E0%A4%A' })).status, 401, 'a broken address is turned away, not crashed on');
});

test('the password works in the path and as a bearer token', async () => {
  fakeNetwork({});
  eq((await rpc('ping', {})).body, { jsonrpc: '2.0', id: 1, result: {} });
  eq((await rpc('ping', {}, { path: '/mcp', bearer: 'open sesame' })).status, 200);
  eq((await rpc('ping', {}, { path: '/mcp/open%20sesame/' })).status, 200, 'trailing slash');
});

test('a stray newline or space around a stored secret is ignored', async () => {
  const asked = fakeNetwork({ videos: { body: { items: [video('aaaaaaaaaaa')] } } });
  const env = { YOUTUBE_API_KEY: 'test-key\r\n', CONNECTOR_PASSWORD: ' open sesame\n' };
  const r = await call('get_video', { video: 'aaaaaaaaaaa' }, { env: env });
  eq(r.isError, false, r.text);
  eq(asked[0].searchParams.get('key'), 'test-key');
  eq((await rpc('ping', {}, { env: { CONNECTOR_PASSWORD: '\n' } })).status, 401, 'a blank password never matches');
});

test('other paths and methods get a hint', async () => {
  fakeNetwork({});
  const r404 = await worker.fetch(new Request('https://yt.example/'), ENV);
  eq(r404.status, 404);
  eq((await worker.fetch(new Request('https://yt.example/mcp/open%20sesame'), ENV)).status, 405, 'GET offers no event stream');
  eq((await worker.fetch(new Request('https://yt.example/mcp/open%20sesame', { method: 'DELETE' }), ENV)).status, 204);
  eq((await worker.fetch(new Request('https://yt.example/mcp/open%20sesame', { method: 'PUT' }), ENV)).status, 405);
});

/* ---------------------------------------------------------- the handshake */

test('initialize answers with a version it supports and the tool capability', async () => {
  fakeNetwork({});
  const r = await rpc('initialize', { protocolVersion: '2025-03-26', capabilities: {}, clientInfo: { name: 'claude', version: '1' } });
  eq(r.status, 200);
  eq(r.body.result.protocolVersion, '2025-03-26');
  eq(r.body.result.serverInfo.name, 'youtube-for-claude');
  assert(r.body.result.capabilities.tools, 'offers tools');
  assert(/never downloads/.test(r.body.result.instructions), 'instructions say what it will not do');
  const newer = await rpc('initialize', { protocolVersion: '2099-01-01' });
  eq(newer.body.result.protocolVersion, '2025-06-18', 'falls back to the newest it knows');
});

test('notifications get 202 and no body', async () => {
  fakeNetwork({});
  const r = await rpc('notifications/initialized', {}, { noId: true });
  eq(r.status, 202);
  eq(r.body, null);
});

test('tools/list names the eight tools with schemas', async () => {
  fakeNetwork({});
  const r = await rpc('tools/list', {});
  const names = r.body.result.tools.map((t) => t.name);
  eq(names, ['search_videos', 'trending_videos', 'rising_reusable_videos', 'get_video', 'get_channel', 'list_channel_videos', 'list_playlist_videos', 'get_comments']);
  for (const t of r.body.result.tools) {
    assert(t.description && t.inputSchema && t.inputSchema.type === 'object', t.name + ' has a description and schema');
  }
  eq(TOOLS.length, 8);
});

test('bad JSON, batches, unknown methods and unknown tools are refused clearly', async () => {
  fakeNetwork({});
  const parse = await rpc(null, null, { raw: '{not json' });
  eq(parse.status, 400);
  eq(parse.body.error.code, -32700);
  const batch = await rpc(null, null, { raw: '[]' });
  eq(batch.body.error.code, -32600);
  const notRpc = await rpc(null, null, { raw: '{"hello":1}' });
  eq(notRpc.body.error.code, -32600);
  const unknown = await rpc('resources/list', {});
  eq(unknown.body.error.code, -32601);
  const tool = await call('download_video', { video: 'x' });
  eq(tool.isError, true);
  assert(/no tool called "download_video"/.test(tool.text), tool.text);
});

/* --------------------------------------------------------------- tools */

test('search_videos asks for Creative Commons by default, then fetches details', async () => {
  const asked = fakeNetwork({
    search: { body: { pageInfo: { totalResults: 2 }, items: [{ id: { videoId: 'aaaaaaaaaaa' } }, { id: { videoId: 'bbbbbbbbbbb' } }] } },
    videos: { body: { items: [video('bbbbbbbbbbb'), video('aaaaaaaaaaa')] } }
  });
  const r = await call('search_videos', { query: 'nature timelapse', max_results: 5, order: 'views', duration: 'short', published_after: '2026-01-01', region_code: 'gb' });
  eq(r.isError, false, r.text);
  const search = asked[0].searchParams;
  eq(search.get('q'), 'nature timelapse');
  eq(search.get('videoLicense'), 'creativeCommon');
  eq(search.get('type'), 'video');
  eq(search.get('order'), 'viewCount');
  eq(search.get('videoDuration'), 'short');
  eq(search.get('publishedAfter'), '2026-01-01T00:00:00.000Z');
  eq(search.get('regionCode'), 'GB');
  eq(search.get('maxResults'), '5');
  eq(search.get('key'), 'test-key');
  eq(asked[1].searchParams.get('id'), 'aaaaaaaaaaa,bbbbbbbbbbb');
  eq(r.data.videos.map((v) => v.id), ['aaaaaaaaaaa', 'bbbbbbbbbbb'], 'search order is kept');
  eq(r.data.license, 'creative_commons');
  eq(r.data.total_estimate, 2);
  eq(r.data.quota_used, 101);
  assert(r.data.videos.every((v) => v.reusable && v.credit), 'all reusable with credit');
});

test('search_videos with license any sends no license filter and keeps every video', async () => {
  const asked = fakeNetwork({
    search: { body: { items: [{ id: { videoId: 'aaaaaaaaaaa' } }] } },
    videos: { body: { items: [video('aaaaaaaaaaa', { status: { license: 'youtube' } })] } }
  });
  const r = await call('search_videos', { query: 'x', license: 'any' });
  eq(asked[0].searchParams.has('videoLicense'), false);
  eq(asked[0].searchParams.get('maxResults'), '10', 'default count');
  eq(r.data.videos.length, 1);
  eq(r.data.videos[0].reusable, false);
});

test('search_videos checks its inputs before spending quota', async () => {
  const asked = fakeNetwork({});
  assert(/"query"/.test((await call('search_videos', {})).text), 'missing query');
  assert(/license/.test((await call('search_videos', { query: 'x', license: 'free' })).text), 'bad license');
  assert(/order/.test((await call('search_videos', { query: 'x', order: 'best' })).text), 'bad order');
  assert(/published_after/.test((await call('search_videos', { query: 'x', published_after: 'yesterday' })).text), 'bad date');
  eq(asked.length, 0, 'nothing asked of YouTube');
});

test('trending_videos reads the chart and can keep only reusable videos', async () => {
  const asked = fakeNetwork({
    videos: { body: { items: [video('aaaaaaaaaaa', { status: { license: 'youtube' } }), video('bbbbbbbbbbb')] } }
  });
  const all = await call('trending_videos', { region_code: 'ca', category_id: '10', max_results: 2 });
  eq(asked[0].searchParams.get('chart'), 'mostPopular');
  eq(asked[0].searchParams.get('regionCode'), 'CA');
  eq(asked[0].searchParams.get('videoCategoryId'), '10');
  eq(all.data.videos.length, 2);
  eq(all.data.quota_used, 1);
  const some = await call('trending_videos', { reusable_only: true });
  eq(some.data.videos.map((v) => v.id), ['bbbbbbbbbbb']);
  eq(asked[1].searchParams.get('regionCode'), 'US', 'default country');
  assert(/region_code/.test((await call('trending_videos', { region_code: 'USA' })).text), 'bad country');
});

test('views per day ranks fast-rising videos, counting under a day as one day', () => {
  const now = Date.parse('2026-10-25T00:00:00Z');
  eq(viewsPerDay(1000, '2026-10-15T00:00:00Z', now), 100);
  eq(viewsPerDay(500, '2026-10-24T18:00:00Z', now), 500, 'six hours old counts as one day');
  eq(viewsPerDay(null, '2026-10-15T00:00:00Z', now), null);
  eq(viewsPerDay(10, 'not a date', now), null);
});

test('rising_reusable_videos asks for recent Creative Commons videos and ranks by views per day', async () => {
  const day = 86400000;
  const ago = (d) => new Date(Date.now() - d * day).toISOString();
  const asked = fakeNetwork({
    search: { body: { items: ['aaaaaaaaaaa', 'bbbbbbbbbbb', 'ccccccccccc', 'ddddddddddd'].map((id) => ({ id: { videoId: id } })) } },
    videos: { body: { items: [
      video('aaaaaaaaaaa', { snippet: { title: 'old and big', channelTitle: 'A', publishedAt: ago(6) }, statistics: { viewCount: '600000' } }),
      video('bbbbbbbbbbb', { snippet: { title: 'new and rising', channelTitle: 'B', publishedAt: ago(1) }, statistics: { viewCount: '300000' } }),
      video('ccccccccccc', { snippet: { title: 'not reusable', channelTitle: 'C', publishedAt: ago(1) }, statistics: { viewCount: '9000000' }, status: { license: 'youtube' } }),
      video('ddddddddddd', { snippet: { title: 'slow', channelTitle: 'D', publishedAt: ago(3) }, statistics: { viewCount: '3000' } })
    ] } }
  });
  const r = await call('rising_reusable_videos', { query: 'funny animals', days: 3, region_code: 'us', max_results: 2 });
  eq(r.isError, false, r.text);
  const q = asked[0].searchParams;
  eq([q.get('videoLicense'), q.get('order'), q.get('q'), q.get('regionCode'), q.get('maxResults')], ['creativeCommon', 'viewCount', 'funny animals', 'US', '50']);
  const since = Date.parse(q.get('publishedAfter'));
  assert(Math.abs(Date.now() - 3 * day - since) < 60000, 'looks back three days');
  eq(r.data.videos.map((v) => v.title), ['new and rising', 'old and big'], 'fastest first, non-reusable dropped, capped');
  eq(r.data.videos[0].views_per_day, 300000);
  assert(r.data.videos.every((v) => v.reusable && v.credit), 'all reusable with credit');
  eq(r.data.quota_used, 101);
  const any = await call('rising_reusable_videos', {});
  eq(asked[2].searchParams.has('q'), false, 'no topic means every topic');
  eq(any.data.days, 7, 'default window');
  assert(/region_code/.test((await call('rising_reusable_videos', { region_code: 'USA' })).text), 'bad country');
});

test('get_video returns one shaped video or says there is none', async () => {
  fakeNetwork({ videos: (u) => ({ body: { items: u.searchParams.get('id') === 'aaaaaaaaaaa' ? [video('aaaaaaaaaaa')] : [] } }) });
  const ok = await call('get_video', { video: 'https://youtu.be/aaaaaaaaaaa' });
  eq(ok.data.id, 'aaaaaaaaaaa');
  eq(ok.data.quota_used, 1);
  const missing = await call('get_video', { video: 'zzzzzzzzzzz' });
  eq(missing.text, 'There is no video with that id.');
  assert(/not a YouTube video/.test((await call('get_video', { video: 'https://vimeo.com/1' })).text), 'not youtube');
});

test('get_channel resolves ids, handles and links', async () => {
  const asked = fakeNetwork({ channels: (u) => ({ body: { items: u.searchParams.get('forHandle') === '@nobody' ? [] : [CHANNEL] } }) });
  const byHandle = await call('get_channel', { channel: '@maker' });
  eq(asked[0].searchParams.get('forHandle'), '@maker');
  eq(byHandle.data.subscribers, 5000);
  eq(byHandle.data.uploads_playlist, 'UUabcdefghijklmnopqrstuv');
  eq(byHandle.data.url, 'https://www.youtube.com/channel/UCabcdefghijklmnopqrstuv');
  await call('get_channel', { channel: 'UCabcdefghijklmnopqrstuv' });
  eq(asked[1].searchParams.get('id'), 'UCabcdefghijklmnopqrstuv');
  await call('get_channel', { channel: 'https://www.youtube.com/user/oldname' });
  eq(asked[2].searchParams.get('forUsername'), 'oldname');
  eq((await call('get_channel', { channel: '@nobody' })).text, 'There is no channel with that id or handle.');
  assert(/not a channel/.test((await call('get_channel', { channel: 'https://example.com/x' })).text), 'not a channel');
});

test('list_channel_videos walks channel -> uploads -> details and filters by license', async () => {
  const asked = fakeNetwork({
    channels: { body: { items: [CHANNEL] } },
    playlistItems: { body: { items: [{ contentDetails: { videoId: 'aaaaaaaaaaa' } }, { contentDetails: { videoId: 'bbbbbbbbbbb' } }, { contentDetails: { videoId: 'ccccccccccc' } }] } },
    videos: { body: { items: [video('aaaaaaaaaaa', { status: { license: 'youtube' } }), video('bbbbbbbbbbb'), video('ccccccccccc')] } }
  });
  const cc = await call('list_channel_videos', { channel: '@maker', max_results: 1 });
  eq(asked[1].searchParams.get('playlistId'), 'UUabcdefghijklmnopqrstuv');
  eq(asked[1].searchParams.get('maxResults'), '50', 'looks through the newest 50 when filtering');
  eq(cc.data.videos.map((v) => v.id), ['bbbbbbbbbbb']);
  eq(cc.data.channel.title, 'Maker');
  eq(cc.data.quota_used, 3);
  const any = await call('list_channel_videos', { channel: '@maker', license: 'any', max_results: 2 });
  eq(asked[4].searchParams.get('maxResults'), '2');
  eq(any.data.videos.map((v) => v.id), ['aaaaaaaaaaa', 'bbbbbbbbbbb'], 'no license filter, capped at max_results');
});

test('list_playlist_videos reads a playlist link and reports a missing playlist', async () => {
  fakeNetwork({
    playlistItems: (u) => u.searchParams.get('playlistId') === 'PLgone12345'
      ? { status: 404, body: { error: { message: 'x', errors: [{ reason: 'playlistNotFound' }] } } }
      : { body: { items: [{ contentDetails: { videoId: 'aaaaaaaaaaa' } }] } },
    videos: { body: { items: [video('aaaaaaaaaaa')] } }
  });
  const ok = await call('list_playlist_videos', { playlist: 'https://www.youtube.com/playlist?list=PLabcdefghijklmnop' });
  eq(ok.data.playlist, 'PLabcdefghijklmnop');
  eq(ok.data.videos.length, 1);
  eq(ok.data.quota_used, 2);
  eq((await call('list_playlist_videos', { playlist: 'PLgone12345' })).text, 'There is no public playlist with that id.');
  assert(/not a playlist/.test((await call('list_playlist_videos', { playlist: '?' })).text), 'bad playlist');
});

test('get_comments shapes threads and explains comments being off', async () => {
  const asked = fakeNetwork({
    commentThreads: (u) => u.searchParams.get('videoId') === 'bbbbbbbbbbb'
      ? { status: 403, body: { error: { errors: [{ reason: 'commentsDisabled' }] } } }
      : { body: { items: [{ id: 'c1', snippet: { totalReplyCount: 2, topLevelComment: { snippet: { authorDisplayName: 'Ann', textOriginal: 'Nice', likeCount: '7', publishedAt: '2026-02-02T00:00:00Z' } } } }] } }
  });
  const ok = await call('get_comments', { video: 'aaaaaaaaaaa', order: 'time', max_results: 3 });
  eq(asked[0].searchParams.get('order'), 'time');
  eq(asked[0].searchParams.get('maxResults'), '3');
  eq(ok.data.comments, [{ id: 'c1', author: 'Ann', text: 'Nice', likes: 7, published: '2026-02-02T00:00:00Z', replies: 2 }]);
  eq((await call('get_comments', { video: 'bbbbbbbbbbb' })).text, 'Comments are turned off on that video.');
  assert(/order/.test((await call('get_comments', { video: 'aaaaaaaaaaa', order: 'best' })).text), 'bad order');
});

test('a used-up quota, a bad key, a timeout and a missing key each give one sentence', async () => {
  fakeNetwork({ videos: { status: 403, body: { error: { errors: [{ reason: 'quotaExceeded' }] } } } });
  assert(/daily allowance/.test((await call('get_video', { video: 'aaaaaaaaaaa' })).text), 'quota');
  fakeNetwork({ videos: { status: 400, body: { error: { message: 'API key not valid. Please pass a valid API key.', errors: [{ reason: 'badRequest' }] } } } });
  assert(/API key is wrong/.test((await call('get_video', { video: 'aaaaaaaaaaa' })).text), 'key');
  fakeNetwork({ videos: 'timeout' });
  eq((await call('get_video', { video: 'aaaaaaaaaaa' })).text, 'YouTube took too long to answer.');
  fakeNetwork({});
  assert(/Could not reach YouTube/.test((await call('get_video', { video: 'aaaaaaaaaaa' })).text), 'down');
  const noKey = await call('get_video', { video: 'aaaaaaaaaaa' }, { env: { CONNECTOR_PASSWORD: 'open sesame' } });
  assert(/No YouTube API key is set/.test(noKey.text), 'no key');
});

/* -------------------------------------------------------------- /check */

test('/check reports setup state and whether YouTube answers, without secrets', async () => {
  fakeNetwork({ videos: { body: { items: [{ id: 'x' }] } } });
  const ok = await (await worker.fetch(new Request('https://yt.example/check'), ENV)).json();
  eq([ok.api_key_set, ok.password_set, ok.youtube.ok], [true, true, true]);
  eq(ok.tools.length, 8);
  assert(!JSON.stringify(ok).includes('test-key') && !JSON.stringify(ok).includes('sesame'), 'no secrets');
  fakeNetwork({ videos: { status: 403, body: { error: { errors: [{ reason: 'accessNotConfigured' }] } } } });
  const bad = await (await worker.fetch(new Request('https://yt.example/check'), { YOUTUBE_API_KEY: 'k' })).json();
  eq([bad.api_key_set, bad.password_set, bad.youtube.ok], [true, false, false]);
  assert(/not enabled/.test(bad.youtube.error), bad.youtube.error);
  const none = await (await worker.fetch(new Request('https://yt.example/check'), {})).json();
  eq(none.youtube.ok, false);
  assert(/No YouTube API key/.test(none.youtube.error), none.youtube.error);
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
