// Udio Rescue — logic tests. Plain Node, no dependencies:
//   node udio-rescue/tests/udio-rescue.test.js
"use strict";
const assert = require("assert");
const path = require("path");
const lib = require(path.join(__dirname, "..", "extension", "lib.js"));

let passed = 0;
function test(name, fn) {
  try { fn(); passed++; } catch (e) { console.error("FAIL " + name + "\n  " + (e && e.stack || e)); process.exitCode = 1; }
}

const SONG = {
  id: "a1b2c3d4-0000-4000-8000-123456789abc",
  title: "Neon Rain",
  song_path: "https://storage.googleapis.com/udio-artifacts/a1b2/neon-rain.mp3",
  image_path: "https://imagedelivery.example/cover/a1b2.jpg",
  lyrics: "[Verse]\nRain on the wire",
  prompt: "synthwave, rainy night",
  tags: ["synthwave", "dark"],
  created_at: "2025-06-01T10:00:00Z",
  duration: 131.6,
  artist: "cody",
};

test("audio links are recognised by their path, not their query string", () => {
  assert.ok(lib.isAudioUrl("https://x.com/a/song.mp3"));
  assert.ok(lib.isAudioUrl("https://x.com/a/song.M4A?sig=abc"));
  assert.ok(!lib.isAudioUrl("https://x.com/page?file=song.mp3"));
  assert.ok(!lib.isAudioUrl("/relative/song.mp3"));
  assert.ok(lib.isPlaylistUrl("https://x.com/s/index.m3u8?t=1"));
});

test("a Udio song object is recognised and normalised", () => {
  assert.ok(lib.looksLikeSong(SONG));
  const s = lib.normalizeSong(SONG);
  assert.strictEqual(s.title, "Neon Rain");
  assert.strictEqual(s.audioUrl, SONG.song_path);
  assert.strictEqual(s.imageUrl, SONG.image_path);
  assert.deepStrictEqual(s.tags, ["synthwave", "dark"]);
  assert.strictEqual(s.duration, 131.6);
  assert.strictEqual(s.artist, "cody");
});

test("objects that are not songs are ignored", () => {
  assert.ok(!lib.looksLikeSong({ id: "abc12345678", title: "A playlist" }));
  assert.ok(!lib.looksLikeSong({ title: "no id", song_path: SONG.song_path }));
  assert.ok(!lib.looksLikeSong([SONG]));
  assert.ok(lib.looksLikeSong({ id: "abc12345678", title: "T", lyrics: "" }), "title + lyrics field = song");
});

test("an audio link under an unexpected field name is still found", () => {
  const s = lib.normalizeSong({ id: "zzzz1111yyyy", weird_field: "https://cdn.x/q/zz.wav" });
  assert.strictEqual(s.audioUrl, "https://cdn.x/q/zz.wav");
});

test("findSongs digs songs out of nested replies and merges duplicates", () => {
  const reply = { data: { pages: [{ songs: [SONG] }, { items: [{ song: { id: SONG.id, title: "Neon Rain", lyrics: "x" } }] }] }, user: { id: "user00000001", name: "Cody" } };
  const found = lib.findSongs(reply);
  assert.strictEqual(found.length, 1);
  assert.strictEqual(found[0].audioUrl, SONG.song_path, "merge keeps the audio link");
});

test("findSongs keeps the order Udio sent", () => {
  const list = [1, 2, 3, 4].map((n) => ({ id: "order000000" + n, title: "S" + n, lyrics: "" }));
  assert.deepStrictEqual(lib.findSongs({ data: list }).map((s) => s.title), ["S1", "S2", "S3", "S4"]);
});

test("findSongs survives a reply that refers to itself", () => {
  const a = { id: "loop00000001", title: "L", lyrics: "" };
  a.self = a;
  assert.strictEqual(lib.findSongs({ a }).length, 1);
});

test("mergeSong prefers real titles over ones read off a link", () => {
  const stub = { id: "x1234567890", title: "Neon Rain 0:42", titleFromLink: true, audioUrl: "" };
  const real = lib.normalizeSong(SONG);
  const m = lib.mergeSong(stub, Object.assign({}, real, { id: stub.id }));
  assert.strictEqual(m.title, "Neon Rain");
  assert.strictEqual(m.titleFromLink, undefined);
  const m2 = lib.mergeSong(m, { id: stub.id, title: "Other link text", titleFromLink: true });
  assert.strictEqual(m2.title, "Neon Rain", "a link title never replaces a real one");
  const m3 = lib.mergeSong(m, { id: stub.id, audioUrl: "" });
  assert.strictEqual(m3.audioUrl, SONG.song_path, "empty values never wipe known ones");
});

test("parseLooseJson reads plain JSON and Next.js line streams", () => {
  assert.deepStrictEqual(lib.parseLooseJson('{"a":1}'), [{ a: 1 }]);
  const rsc = '0:["$","div",null,{}]\n1a:{"songs":[' + JSON.stringify(SONG) + ']}\n2:I["chunk.js"]\nnot json';
  const vals = lib.parseLooseJson(rsc);
  assert.strictEqual(vals.length, 2);
  assert.strictEqual(lib.findSongs(vals).length, 1);
  assert.deepStrictEqual(lib.parseLooseJson(""), []);
});

test("songsFromScriptText reads Next.js __next_f pushes", () => {
  const payload = '5:{"song":' + JSON.stringify(SONG) + "}";
  const script = "self.__next_f.push([1," + JSON.stringify(payload) + "])";
  const songs = lib.songsFromScriptText(script);
  assert.strictEqual(songs.length, 1);
  assert.strictEqual(songs[0].audioUrl, SONG.song_path);
});

test("song ids come from song links only", () => {
  assert.strictEqual(lib.songIdFromHref("/songs/eLsL9r3mbY7jA4Lr1n2Zxd"), "eLsL9r3mbY7jA4Lr1n2Zxd");
  assert.strictEqual(lib.songIdFromHref("https://www.udio.com/songs/" + SONG.id + "?x=1"), SONG.id);
  assert.strictEqual(lib.songIdFromHref("/songs/favorites"), "");
  assert.strictEqual(lib.songIdFromHref("/creators/abc12345678"), "");
  assert.strictEqual(lib.songIdFromHref(null), "");
});

test("songFromHtml reads share tags", () => {
  const html = '<html><head><title>Neon Rain | Udio</title>' +
    '<meta property="og:audio" content="https://cdn.x/a/neon.mp3?x=1&amp;y=2">' +
    '<meta content="https://cdn.x/c.jpg" property="og:image"></head></html>';
  const s = lib.songFromHtml(html, "abc123456789");
  assert.strictEqual(s.audioUrl, "https://cdn.x/a/neon.mp3?x=1&y=2");
  assert.strictEqual(s.title, "Neon Rain");
  assert.strictEqual(s.imageUrl, "https://cdn.x/c.jpg");
});

test("songFromHtml finds this song's data in escaped page scripts, not a neighbour's", () => {
  const other = { id: "other0000001", title: "Other", song_path: "https://cdn.x/other.mp3" };
  const payload = '7:{"related":[' + JSON.stringify(other) + '],"song":' + JSON.stringify(SONG) + "}";
  const html = "<script>self.__next_f.push([1," + JSON.stringify(payload) + "])</script>";
  const s = lib.songFromHtml(html, SONG.id);
  assert.strictEqual(s.audioUrl, SONG.song_path);
  assert.strictEqual(s.lyrics, SONG.lyrics);
});

test("file names are safe on every system and never collide for twin songs", () => {
  assert.strictEqual(lib.safeFilename('a/b\\c:d*e?"f<g>h|i'), "a b c d e f g h i");
  assert.strictEqual(lib.safeFilename("  ...  "), "Untitled");
  assert.strictEqual(lib.safeFilename("CON"), "_CON");
  assert.ok(lib.safeFilename("x".repeat(500)).length <= 90);
  const a = lib.baseNameFor({ id: "11111111aaaa", title: "Same" });
  const b = lib.baseNameFor({ id: "22222222bbbb", title: "Same" });
  assert.notStrictEqual(a, b);
  assert.strictEqual(a, "Same [11111111]");
});

test("extensions come from the link", () => {
  assert.strictEqual(lib.extFromUrl("https://x/y/z.WAV?sig=1"), "wav");
  assert.strictEqual(lib.extFromUrl("https://x/y/noext"), "mp3");
  assert.strictEqual(lib.extFromUrl("https://x/y/noext", "jpg"), "jpg");
});

test("the info sheet carries title, prompt, lyrics and the Udio link", () => {
  const t = lib.infoText(lib.normalizeSong(SONG));
  assert.ok(t.startsWith("Neon Rain\n"));
  assert.ok(t.includes("Tags: synthwave, dark"));
  assert.ok(t.includes("Length: 2:12"));
  assert.ok(t.includes("Prompt:\nsynthwave, rainy night"));
  assert.ok(t.includes("Lyrics:\n[Verse]\nRain on the wire"));
  assert.ok(t.includes("https://www.udio.com/songs/" + SONG.id));
});

test("library.json lists every song", () => {
  const j = JSON.parse(lib.libraryJson([lib.normalizeSong(SONG)], "2026-01-01T00:00:00Z"));
  assert.strictEqual(j.count, 1);
  assert.strictEqual(j.songs[0].title, "Neon Rain");
  assert.strictEqual(j.savedAt, "2026-01-01T00:00:00Z");
});

test("text data links round-trip, including emoji and newlines", () => {
  const u = lib.textDataUrl("héllo 🎵\nline 2");
  assert.ok(u.startsWith("data:text/plain;charset=utf-8,"));
  assert.strictEqual(decodeURIComponent(u.split(",")[1]), "héllo 🎵\nline 2");
});

test("HLS master playlists pick the best quality", () => {
  const master = "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=64000\nlow/index.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=320000\nhigh/index.m3u8\n";
  const p = lib.parseM3U8(master, "https://cdn.x/s/master.m3u8");
  assert.ok(p.valid);
  assert.strictEqual(p.variants[0].url, "https://cdn.x/s/high/index.m3u8");
  assert.strictEqual(p.segments.length, 0);
});

test("HLS media playlists list segments and the init piece", () => {
  const media = '#EXTM3U\n#EXT-X-MAP:URI="init.mp4"\n#EXTINF:6,\nseg0.m4s\n#EXTINF:6,\nhttps://other.cdn/seg1.m4s\n#EXT-X-ENDLIST\n';
  const p = lib.parseM3U8(media, "https://cdn.x/s/high/index.m3u8");
  assert.deepStrictEqual(p.segments, ["https://cdn.x/s/high/seg0.m4s", "https://other.cdn/seg1.m4s"]);
  assert.strictEqual(p.initUrl, "https://cdn.x/s/high/init.mp4");
  assert.strictEqual(lib.extForSegments(p), "m4a");
  assert.ok(!p.encrypted);
  assert.strictEqual(lib.extForSegments(lib.parseM3U8("#EXTM3U\nseg0.ts\n", "https://c/x.m3u8")), "ts");
});

test("encrypted streams are flagged so they are never downloaded", () => {
  const enc = '#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI="key"\nseg0.ts\n';
  assert.ok(lib.parseM3U8(enc, "https://c/x.m3u8").encrypted);
  const none = "#EXTM3U\n#EXT-X-KEY:METHOD=NONE\nseg0.ts\n";
  assert.ok(!lib.parseM3U8(none, "https://c/x.m3u8").encrypted);
  assert.ok(!lib.parseM3U8("not a playlist", "https://c/x.m3u8").valid);
});

test("endpoint labels drop query values and long ids", () => {
  assert.strictEqual(lib.endpointLabel("get", "/api/songs?songIds=abc,def&x=1"), "GET www.udio.com/api/songs?songIds&x");
  assert.strictEqual(lib.endpointLabel("POST", "https://www.udio.com/api/song/" + "A".repeat(22)), "POST www.udio.com/api/song/:id");
});

test("the troubleshooting report holds no song contents or secrets", () => {
  const songs = [lib.normalizeSong(SONG)];
  const d = lib.diagnostics({ version: "1.0.0", page: "https://www.udio.com/library?token=SECRET", songs, savedIds: [], endpoints: [], sampleKeys: ["id", "title"], media: [], errors: [] });
  const text = JSON.stringify(d);
  assert.ok(!text.includes("SECRET"));
  assert.ok(!text.includes("Rain on the wire"));
  assert.ok(!text.includes(SONG.song_path));
  assert.deepStrictEqual(d.audioHosts, ["storage.googleapis.com"]);
  assert.strictEqual(d.songsWithAudio, 1);
});

console.log((process.exitCode ? "FAILED" : "ok") + " — " + passed + " Udio Rescue logic tests passed");
