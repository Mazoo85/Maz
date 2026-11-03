// Udio Rescue — end-to-end test. Loads the real extension into a real
// Chromium, points it at a pretend Udio site, presses the buttons, and checks
// the files that land on disk.
//
//   node udio-rescue/tests/e2e.cjs
//
// Needs Chromium (set CHROME=/path/to/chrome, or have Playwright's installed)
// and openssl. No npm packages: Chrome is driven over its own DevTools
// protocol, because test drivers such as Playwright redirect and rename
// downloads, which would hide the very thing being tested.
//
// The pretend site covers what the real one may do: a library that loads more
// songs as you scroll, songs that arrive without an audio link (filled in from
// the song API, or from the song's own page), a streamed (HLS) song, a
// protected stream that must be skipped, and a file Chrome's downloader cannot
// reach (saved through the page instead).
"use strict";
const assert = require("assert");
const { spawn, execFileSync } = require("child_process");
const fs = require("fs");
const http = require("http");
const https = require("https");
const os = require("os");
const path = require("path");

const EXT = path.join(__dirname, "..", "extension");
const BIG = 3 * 1024 * 1024;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function findChrome() {
  if (process.env.CHROME) return process.env.CHROME;
  try { return require("playwright").chromium.executablePath(); } catch (_) { /* not installed */ }
  for (const c of ["/usr/bin/chromium", "/usr/bin/chromium-browser", "/usr/bin/google-chrome"]) if (fs.existsSync(c)) return c;
  throw new Error("No Chromium found: set CHROME=/path/to/chrome");
}

function makeSongs(audioBase) {
  const s = (n, extra) => Object.assign({ id: "song" + String(n).padStart(4, "0") + "x9Q7rT", title: "Track " + n, lyrics: "la la " + n, tags: ["test"] }, extra);
  return {
    page0: [
      s(1, { song_path: audioBase + "/audio/1.mp3", image_path: audioBase + "/img/1.jpg" }),
      s(2, { title: "Twin", song_path: audioBase + "/audio/2.mp3" }),
      s(3, { title: "Twin", song_path: audioBase + "/audio/3.mp3" }),
      // Chrome's downloader is refused here; the page itself is allowed.
      s(4, { title: "Unreachable", song_path: "https://cdn.mock.test/direct/4.mp3" }),
    ],
    page1: [ // no audio links: the tool must ask /api/songs?songIds=
      { id: s(5).id, title: "Track 5" }, { id: s(6).id, title: "Track 6" }, { id: s(7).id, title: "Stream" },
    ],
    details: {
      [s(5).id]: s(5, { song_path: audioBase + "/audio/5.mp3" }),
      [s(6).id]: s(6, { song_path: audioBase + "/audio/6.wav" }),
      [s(7).id]: s(7, { title: "Stream", song_path: "https://cdn.mock.test/hls/master.m3u8" }),
    },
    page2: [ // only reachable through each song's own page
      { id: s(8).id, title: "Only on its page" }, { id: s(9).id, title: "Locked" },
    ],
    pages: {
      [s(8).id]: '<html><head><title>Only on its page | Udio</title><meta property="og:audio" content="' + audioBase + '/audio/8.mp3"></head></html>',
      [s(9).id]: '<html><head><title>Locked | Udio</title><meta property="og:audio" content="https://cdn.mock.test/hls/locked.m3u8"></head></html>',
    },
  };
}

const LIBRARY_HTML = `<!doctype html><html><head><title>Library | Udio</title></head>
<body style="margin:0"><div id="feed" style="height:300px;overflow:auto"><div id="list"></div></div>
<a href="/songs/favorites">Favorites</a>
<script>
  let page = 0, loading = false, done = false;
  async function more() {
    if (loading || done) return;
    loading = true;
    const r = await fetch('/api/songs/me?pageParam=' + page + '&pageSize=30');
    const j = await r.json();
    if (!j.data.length) done = true;
    for (const s of j.data) {
      const a = document.createElement('a');
      a.href = '/songs/' + s.id; a.textContent = s.title + '\\n0:42';
      a.style.cssText = 'display:block;height:120px';
      document.getElementById('list').append(a);
    }
    page++; loading = false;
  }
  document.getElementById('feed').addEventListener('scroll', (e) => {
    const f = e.target; if (f.scrollTop + f.clientHeight >= f.scrollHeight - 20) more();
  });
  more();
</script></body></html>`;

// --- a tiny DevTools-protocol client -------------------------------------
async function devtools(wsUrl) {
  const ws = new WebSocket(wsUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let nextId = 1;
  const waiting = new Map();
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && waiting.has(m.id)) {
      const { resolve, reject } = waiting.get(m.id);
      waiting.delete(m.id);
      if (m.error) reject(new Error(m.error.message)); else resolve(m.result);
    }
  };
  const send = (method, params, sessionId) => new Promise((resolve, reject) => {
    const id = nextId++;
    waiting.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params: params || {}, sessionId }));
  });
  return { send, close: () => ws.close() };
}

async function main() {
  const work = fs.mkdtempSync(path.join(os.tmpdir(), "udio-rescue-e2e-"));
  const profile = path.join(work, "profile");
  const downloads = path.join(work, "downloads");
  fs.mkdirSync(path.join(profile, "Default"), { recursive: true });
  fs.mkdirSync(downloads);
  fs.writeFileSync(path.join(profile, "Default", "Preferences"), JSON.stringify({
    download: { default_directory: downloads, prompt_for_download: false },
  }));
  execFileSync("openssl", ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-subj", "/CN=www.udio.com",
    "-keyout", path.join(work, "key.pem"), "-out", path.join(work, "cert.pem")], { stdio: "ignore" });

  // Plain-HTTP "audio CDN", reachable by Chrome's own downloader.
  const audio = http.createServer((req, res) => {
    if (req.url.startsWith("/audio/") || req.url.startsWith("/img/")) {
      const type = req.url.startsWith("/img/") ? "image/jpeg" : req.url.endsWith(".wav") ? "audio/wav" : "audio/mpeg";
      res.writeHead(200, { "content-type": type });
      res.end("FAKEAUDIO:" + req.url);
    } else { res.writeHead(404); res.end(); }
  });
  await new Promise((r) => audio.listen(0, "127.0.0.1", r));
  const data = makeSongs("http://127.0.0.1:" + audio.address().port);

  // HTTPS server playing both www.udio.com and cdn.mock.test.
  const calls = [];
  const site = https.createServer({ key: fs.readFileSync(path.join(work, "key.pem")), cert: fs.readFileSync(path.join(work, "cert.pem")) }, (req, res) => {
    const u = new URL(req.url, "https://" + req.headers.host);
    const send = (status, type, body, headers) => { res.writeHead(status, Object.assign({ "content-type": type }, headers)); res.end(body); };
    if (u.hostname === "cdn.mock.test") {
      const cors = { "access-control-allow-origin": "*" };
      const p = u.pathname;
      const m3u = (b) => send(200, "application/vnd.apple.mpegurl", b, cors);
      if (p === "/hls/master.m3u8") return m3u("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=64000\nlow.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=256000\nhigh.m3u8\n");
      if (p === "/hls/high.m3u8") return m3u('#EXTM3U\n#EXT-X-MAP:URI="init.mp4"\n#EXTINF:6,\nseg0.m4s\n#EXTINF:6,\nseg1.m4s\n#EXT-X-ENDLIST\n');
      if (p === "/hls/locked.m3u8") return m3u('#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI="k"\nseg0.ts\n');
      if (p === "/hls/init.mp4") return send(200, "video/mp4", "INIT|", cors);
      // Song-sized pieces (~3 MB each), so the rebuilt file is a realistic size.
      if (p.startsWith("/hls/seg")) return send(200, "video/iso.segment", p.slice(5) + "|" + "x".repeat(BIG) + "|", cors);
      if (p === "/direct/4.mp3") {
        // Pages send an Origin header; Chrome's downloader does not.
        if (!req.headers.origin) return send(403, "text/plain", "no", cors);
        return send(200, "audio/mpeg", "FAKEAUDIO:via-page", cors);
      }
      return send(404, "text/plain", "", cors);
    }
    calls.push(u.pathname + u.search);
    const json = (o) => send(200, "application/json", JSON.stringify(o));
    if (u.pathname === "/library") return send(200, "text/html", LIBRARY_HTML);
    if (u.pathname === "/api/songs/me") return json({ data: [data.page0, data.page1, data.page2][Number(u.searchParams.get("pageParam"))] || [] });
    if (u.pathname === "/api/songs") {
      const ids = (u.searchParams.get("songIds") || "").split(",");
      return json({ songs: ids.map((id) => data.details[id]).filter(Boolean) });
    }
    const m = u.pathname.match(/^\/songs\/(.+)$/);
    if (m && data.pages[m[1]]) return send(200, "text/html", data.pages[m[1]]);
    return send(404, "text/plain", "not found");
  });
  await new Promise((r) => site.listen(0, "127.0.0.1", r));
  const sitePort = site.address().port;

  const chrome = spawn(findChrome(), [
    "--headless=new", "--no-sandbox", "--no-first-run", "--no-default-browser-check",
    "--remote-debugging-port=0", "--user-data-dir=" + profile,
    "--host-resolver-rules=MAP www.udio.com 127.0.0.1:" + sitePort + ", MAP cdn.mock.test 127.0.0.1:" + sitePort,
    "--ignore-certificate-errors", "--no-proxy-server",
    "--disable-extensions-except=" + EXT, "--load-extension=" + EXT, "about:blank",
  ], { stdio: ["ignore", "ignore", "pipe"] });
  let chromeErr = "";
  const wsUrl = await new Promise((resolve, reject) => {
    chrome.stderr.on("data", (d) => {
      chromeErr += d;
      const m = chromeErr.match(/DevTools listening on (ws:\/\/\S+)/);
      if (m) resolve(m[1]);
    });
    chrome.on("exit", (code) => reject(new Error("Chromium exited " + code + "\n" + chromeErr)));
  });
  const cdp = await devtools(wsUrl);

  let sessionId = null;
  const openPage = async (url) => {
    const { targetId } = await cdp.send("Target.createTarget", { url });
    ({ sessionId } = await cdp.send("Target.attachToTarget", { targetId, flatten: true }));
  };
  const evaluate = async (expr) => {
    const r = await cdp.send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true }, sessionId);
    if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text);
    return r.result.value;
  };
  const panel = (js) => evaluate("(() => { const h = document.querySelector('#udio-rescue-host'); if (!h) return null; const r = h.shadowRoot; " + js + " })()");
  const status = () => panel("return r.querySelector('.status').textContent;");
  const waitFor = async (what, test, ms) => {
    const end = Date.now() + (ms || 120000);
    let last = null;
    while (Date.now() < end) {
      try { last = await status(); } catch (_) { last = null; }
      if (last && test(last)) return last;
      await sleep(300);
    }
    throw new Error("timed out waiting for " + what + "; status was: " + last);
  };

  try {
    await openPage("https://www.udio.com/library");
    await waitFor("the panel", () => true, 30000);
    await panel("r.querySelector('.pill').click(); return 1;");

    // Step 1.
    await panel("r.querySelector('.find').click(); return 1;");
    const after = await waitFor("the scan", (t) => t.startsWith("Done looking"));
    console.log("after scan:", after);
    assert.ok(after.includes("Found 9 songs"), "all nine library songs found, not /songs/favorites");
    assert.ok(after.includes("9 ready"), "every song got an audio link");
    assert.ok(calls.some((c) => c.startsWith("/api/songs?songIds=")), "asked the song API for missing links");

    // SCREENSHOT=file.png saves a picture of the panel at this point.
    if (process.env.SCREENSHOT) {
      await cdp.send("Emulation.setDeviceMetricsOverride", { width: 900, height: 720, deviceScaleFactor: 2, mobile: false }, sessionId);
      await sleep(400);
      const shot = await cdp.send("Page.captureScreenshot", { format: "png" }, sessionId);
      fs.writeFileSync(process.env.SCREENSHOT, Buffer.from(shot.data, "base64"));
    }

    // Step 2.
    await panel("r.querySelector('.save').click(); return 1;");
    const final = await waitFor("the save", (t) => t.startsWith("Finished"));
    console.log("after save:", final);
    assert.ok(final.includes("Saved 8 songs"), "eight saved");
    assert.ok(final.includes("1 could not be saved"), "the protected stream was refused");

    // What actually landed on disk.
    const walk = (dir) => fs.readdirSync(dir, { withFileTypes: true })
      .flatMap((d) => (d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]));
    await sleep(500);
    const names = walk(downloads).map((f) => path.relative(downloads, f)).sort();
    console.log("saved files:\n  " + names.join("\n  "));
    const read = (name) => {
      const f = path.join(downloads, "Udio Rescue", name);
      assert.ok(fs.existsSync(f), "saved: " + name);
      return fs.readFileSync(f, "utf8");
    };
    assert.ok(names.every((n) => n.startsWith("Udio Rescue" + path.sep)), "everything is inside the Udio Rescue folder");
    assert.ok(!names.some((n) => n.endsWith(".crdownload")), "no half-finished downloads");
    assert.strictEqual(read("Track 1 [song0001].mp3"), "FAKEAUDIO:/audio/1.mp3");
    assert.strictEqual(read("Track 1 [song0001].jpg"), "FAKEAUDIO:/img/1.jpg");
    assert.ok(read("Track 1 [song0001].txt").includes("Lyrics:\nla la 1"));
    assert.strictEqual(read("Twin [song0002].mp3"), "FAKEAUDIO:/audio/2.mp3");
    assert.strictEqual(read("Twin [song0003].mp3"), "FAKEAUDIO:/audio/3.mp3", "twin titles never overwrite each other");
    assert.strictEqual(read("Unreachable [song0004].mp3"), "FAKEAUDIO:via-page", "fell back to fetching through the page");
    read("Track 5 [song0005].mp3");
    read("Track 6 [song0006].wav");
    const big = "x".repeat(BIG);
    assert.ok(read("Stream [song0007].m4a") === "INIT|seg0.m4s|" + big + "|seg1.m4s|" + big + "|", "stream rebuilt from its pieces, best quality, full size");
    assert.strictEqual(read("Only on its page [song0008].mp3"), "FAKEAUDIO:/audio/8.mp3");
    assert.ok(!names.some((n) => n.includes("Locked")), "protected stream never saved");
    const libj = JSON.parse(read("library.json"));
    assert.strictEqual(libj.count, 9);
    assert.strictEqual(libj.songs.find((s) => s.title === "Stream").file, "Stream [song0007].m4a");

    // Reload: the list is remembered and nothing is saved twice.
    await evaluate("location.reload(), 1");
    await sleep(1000);
    const welcome = await waitFor("the reload", (t) => t.startsWith("Welcome back"), 30000);
    console.log("after reload:", welcome);
    assert.ok(welcome.includes("9 songs") && welcome.includes("8 already saved"));
    const counts = await panel("return r.querySelector('.counts').textContent;");
    assert.ok(counts.includes("1 ticked"), "only the unsaved song stays ticked: " + counts);

    console.log("ok — Udio Rescue end-to-end test passed");
  } finally {
    cdp.close();
    const exited = new Promise((r) => chrome.once("exit", r));
    chrome.kill();
    await Promise.race([exited, sleep(5000)]);
    audio.close();
    site.close();
    fs.rmSync(work, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 });
  }
}

main().catch((e) => { console.error(e); process.exit(1); });
