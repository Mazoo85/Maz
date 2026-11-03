/*
 * Udio Rescue — the panel. Runs on udio.com pages, beside the site.
 *
 * Step 1, "Find my songs": scrolls your library so Udio loads every song,
 *   collecting each one the listener (inject.js) sees, then asks Udio for any
 *   missing audio links.
 * Step 2, "Save selected songs": hands each audio file, cover and lyrics
 *   sheet to background.js, which saves them into Downloads/Udio Rescue/.
 *
 * What it has found is remembered (in the extension's own storage), so a long
 * scan survives a page reload and songs already saved aren't saved twice.
 */
(function () {
  "use strict";
  if (window.__udioRescuePanel) return;
  window.__udioRescuePanel = true;

  const lib = globalThis.UdioRescueLib;
  const VERSION = chrome.runtime.getManifest().version;
  const TAG = "udio-rescue";

  const state = {
    songs: new Map(),      // every song seen, by id
    mine: new Set(),       // songs collected by a scan: what the list shows
    order: [],             // ids in the order they were collected
    savedIds: new Set(),
    selected: new Set(),
    onThisPage: new Set(), // songs seen since the address last changed
    endpoints: new Map(),
    fieldNames: new Set(),
    media: [],
    errors: [],
    scanning: false,
    saving: false,
    stop: false,
    extras: true,
    lastClick: null,
    path: location.pathname,
  };

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  // ---------------------------------------------------------------- storage
  async function load() {
    try {
      const d = await chrome.storage.local.get(["songs", "order", "savedIds", "extras"]);
      for (const s of d.songs || []) state.songs.set(s.id, s);
      for (const id of d.order || []) if (state.songs.has(id)) markMine(id);
      for (const id of d.savedIds || []) { state.savedIds.add(id); state.selected.delete(id); }
      if (typeof d.extras === "boolean") state.extras = d.extras;
    } catch (e) {
      noteError("could not read saved list: " + e.message);
    }
  }

  let persistTimer = null;
  function persist() {
    clearTimeout(persistTimer);
    persistTimer = setTimeout(() => {
      chrome.storage.local.set({
        songs: state.order.map((id) => state.songs.get(id)).filter(Boolean),
        order: state.order,
        savedIds: Array.from(state.savedIds),
        extras: state.extras,
      }).catch((e) => noteError("could not remember the list: " + e.message));
    }, 400);
  }

  function noteError(msg) {
    state.errors.push(String(msg).slice(0, 300));
    if (state.errors.length > 200) state.errors.shift();
  }

  // ------------------------------------------------------------------ songs
  function markMine(id) {
    if (state.mine.has(id)) return;
    state.mine.add(id);
    state.order.push(id);
    if (!state.savedIds.has(id)) state.selected.add(id);
  }

  function addSong(song, mine) {
    if (!song || !song.id) return;
    state.songs.set(song.id, lib.mergeSong(state.songs.get(song.id), song));
    state.onThisPage.add(song.id);
    if (mine) markMine(song.id);
    if (state.mine.has(song.id)) persist();
    scheduleRender();
  }

  function attachMedia(url) {
    for (const [id, s] of state.songs) {
      if (url.includes(id)) {
        if (!s.audioUrl) { s.audioUrl = url; persist(); scheduleRender(); }
        return;
      }
    }
    const lc = state.lastClick;
    if (lc && Date.now() - lc.at < 8000) {
      const s = state.songs.get(lc.id);
      if (s && !s.audioUrl) { s.audioUrl = url; persist(); scheduleRender(); return; }
    }
    if (!state.media.includes(url)) state.media.push(url);
    if (state.media.length > 50) state.media.shift();
  }

  // A reply Udio's site received: note the address, keep any songs in it.
  function readReply(m) {
    let songs = [];
    let sample = null;
    for (const v of lib.parseLooseJson(m.text)) {
      songs = songs.concat(lib.findSongs(v));
      if (!sample && songs.length) sample = firstSongObject(v);
    }
    const label = lib.endpointLabel(m.method, m.url);
    state.endpoints.set(label, { endpoint: label, status: m.status, songs: songs.length });
    if (sample) for (const k of Object.keys(sample)) state.fieldNames.add(k);
    for (const s of songs) addSong(s, state.scanning);
  }

  function firstSongObject(data) {
    const stack = [data];
    for (let n = 0; stack.length && n < 50000; n++) {
      const node = stack.pop();
      if (!node || typeof node !== "object") continue;
      if (lib.looksLikeSong(node)) return node;
      for (const k in node) if (node[k] && typeof node[k] === "object") stack.push(node[k]);
    }
    return null;
  }

  // Messages from the listener inside the page.
  window.addEventListener("message", (e) => {
    if (e.source !== window || !e.data || e.data.source !== TAG) return;
    const m = e.data;
    if (m.type === "songs" && Array.isArray(m.songs)) {
      for (const s of m.songs) addSong(s, state.scanning);
    } else if (m.type === "reply" && typeof m.text === "string") {
      readReply(m);
    } else if (m.type === "media" && typeof m.url === "string") {
      attachMedia(m.url);
    }
  });
  window.postMessage({ source: TAG + "-panel", type: "hello" }, location.origin);

  // Remember which song was last clicked, to match a song that starts playing.
  document.addEventListener("click", (e) => {
    let el = e.target;
    for (let i = 0; el && i < 4; i++, el = el.parentElement) {
      const a = el.matches && el.matches("a[href]") ? el : el.querySelector && el.querySelector("a[href*='/song']");
      const id = a && lib.songIdFromHref(a.getAttribute("href"));
      if (id) { state.lastClick = { id, at: Date.now() }; return; }
    }
  }, true);

  // A single-page site changes its address without reloading.
  setInterval(() => {
    if (location.pathname !== state.path) {
      state.path = location.pathname;
      state.onThisPage.clear();
    }
  }, 500);

  function readPageScripts(mine) {
    for (const sc of document.querySelectorAll("script:not([src])")) {
      try {
        for (const s of lib.songsFromScriptText(sc.textContent || "")) addSong(s, mine);
      } catch (_) { /* skip */ }
    }
  }

  function collectLinks() {
    for (const a of document.querySelectorAll("a[href]")) {
      const id = lib.songIdFromHref(a.getAttribute("href"));
      if (!id) continue;
      if (!state.songs.has(id)) {
        const text = (a.innerText || a.textContent || "").split("\n").map((t) => t.trim()).filter(Boolean)[0] || "";
        state.songs.set(id, {
          id, title: text.slice(0, 120), titleFromLink: !!text, audioUrl: "", imageUrl: "", lyrics: "",
          prompt: "", tags: [], artist: "", createdAt: "", duration: null,
        });
      }
      markMine(id);
    }
    persist();
    scheduleRender();
  }

  // Scroll everything scrollable to the bottom, so the site loads more songs.
  function scrollAll() {
    const boxes = [document.scrollingElement || document.documentElement];
    for (const el of document.querySelectorAll("body *")) {
      if (el.closest && el.closest("#udio-rescue-host")) continue;
      if (el.scrollHeight > el.clientHeight + 40) {
        const oy = getComputedStyle(el).overflowY;
        if (oy === "auto" || oy === "scroll") boxes.push(el);
      }
    }
    for (const el of boxes) el.scrollTop = el.scrollHeight;
    // Some lists load more with a button instead.
    for (const b of document.querySelectorAll("button")) {
      if (/^\s*(load|show|see)\s+more\s*$/i.test(b.textContent || "")) { try { b.click(); } catch (_) { /* ignore */ } }
    }
    return boxes.map((el) => el.scrollHeight).join(",");
  }

  async function findSongs() {
    if (state.scanning || state.saving) return;
    state.scanning = true;
    state.stop = false;
    render();
    try {
      for (const id of state.onThisPage) markMine(id);
      readPageScripts(true);
      setStatus("Scrolling your library so Udio loads every song… keep this tab open.");
      let idle = 0;
      let lastCount = -1;
      let lastHeights = "";
      for (let round = 0; !state.stop && idle < 6 && round < 2000; round++) {
        collectLinks();
        const heights = scrollAll();
        await sleep(1300);
        collectLinks();
        const count = state.mine.size;
        idle = count === lastCount && heights === lastHeights ? idle + 1 : 0;
        lastCount = count;
        lastHeights = heights;
        setStatus("Found " + count + " songs so far… keep this tab open.");
      }
      if (!state.stop) await fillIn();
      const s = summary();
      setStatus(state.stop
        ? "Stopped. Found " + s.total + " songs (" + s.ready + " ready to save)."
        : "Done looking. Found " + s.total + " songs: " + s.ready + " ready to save" +
          (s.missing ? ", " + s.missing + " still without an audio link (see the tip below)." : "."));
    } catch (e) {
      noteError("scan: " + e.message);
      setStatus("Something went wrong while looking: " + e.message);
    } finally {
      state.scanning = false;
      persist();
      render();
    }
  }

  // Ask Udio for songs we know only by their link.
  async function fillIn() {
    const needs = () => state.order.filter((id) => {
      const s = state.songs.get(id);
      return s && (!s.audioUrl || !s.title || s.titleFromLink);
    });

    let todo = needs();
    for (let i = 0; i < todo.length && !state.stop; i += 20) {
      const chunk = todo.slice(i, i + 20);
      setStatus("Asking Udio for song details… " + Math.min(i + 20, todo.length) + " of " + todo.length);
      try {
        const res = await fetch("/api/songs?songIds=" + chunk.map(encodeURIComponent).join(","), { credentials: "include" });
        if (!res.ok) { noteError("song details: HTTP " + res.status); break; }
        const text = await res.text();
        for (const v of lib.parseLooseJson(text)) {
          for (const s of lib.findSongs(v)) addSong(s, chunk.includes(s.id));
        }
      } catch (e) {
        noteError("song details: " + e.message);
        break;
      }
      await sleep(300);
    }

    todo = needs().filter((id) => !state.songs.get(id).audioUrl);
    for (let i = 0; i < todo.length && !state.stop; i++) {
      const id = todo[i];
      setStatus("Checking song pages for audio links… " + (i + 1) + " of " + todo.length);
      try {
        const res = await fetch("/songs/" + encodeURIComponent(id), { credentials: "include" });
        if (res.ok) {
          const s = lib.songFromHtml(await res.text(), id);
          if (s) addSong(s, false);
        } else {
          noteError("song page: HTTP " + res.status);
        }
      } catch (e) {
        noteError("song page: " + e.message);
      }
      await sleep(350);
    }
  }

  // ----------------------------------------------------------------- saving
  function bg(msg) {
    return chrome.runtime.sendMessage(Object.assign({ to: "udio-rescue-bg" }, msg))
      .then((r) => r || { ok: false, error: "no answer" })
      .catch((e) => ({ ok: false, error: e.message }));
  }

  function blobToDataUrl(blob) {
    return new Promise((resolve, reject) => {
      const r = new FileReader();
      r.onload = () => resolve(r.result);
      r.onerror = () => reject(r.error || new Error("could not read the file"));
      r.readAsDataURL(blob);
    });
  }

  async function saveBlob(blob, filename) {
    if (blob.size > 300 * 1024 * 1024) return { ok: false, error: "file is too large" };
    return bg({ type: "save", url: await blobToDataUrl(blob), filename });
  }

  // When Chrome can't fetch a link directly, fetch it from the page instead.
  async function saveViaPage(url, filename) {
    try {
      const res = await fetch(url, { credentials: "omit" });
      if (!res.ok) return { ok: false, error: "HTTP " + res.status };
      return saveBlob(await res.blob(), filename);
    } catch (e) {
      return { ok: false, error: e.message };
    }
  }

  async function fetchText(url) {
    const res = await fetch(url, { credentials: "omit" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    return res.text();
  }

  // A song played as a stream of small pieces (HLS): join the pieces into one file.
  async function saveStream(song, base) {
    try {
      let url = song.audioUrl;
      let p = lib.parseM3U8(await fetchText(url), url);
      if (p.encrypted) return { ok: false, error: "protected stream, skipped" };
      if (p.variants.length) {
        url = p.variants[0].url;
        p = lib.parseM3U8(await fetchText(url), url);
        if (p.encrypted) return { ok: false, error: "protected stream, skipped" };
      }
      if (!p.valid || !p.segments.length) return { ok: false, error: "stream had no audio pieces" };
      const parts = [];
      if (p.initUrl) parts.push(await (await fetch(p.initUrl, { credentials: "omit" })).arrayBuffer());
      for (const seg of p.segments) {
        if (state.stop) return { ok: false, error: "stopped" };
        const res = await fetch(seg, { credentials: "omit" });
        if (!res.ok) return { ok: false, error: "stream piece: HTTP " + res.status };
        parts.push(await res.arrayBuffer());
      }
      const ext = lib.extForSegments(p);
      const type = { m4a: "audio/mp4", aac: "audio/aac", mp3: "audio/mpeg" }[ext] || "video/mp2t";
      const filename = base + "." + ext;
      const r = await saveBlob(new Blob(parts, { type }), filename);
      if (r.ok) song.file = r.file || filename;
      return r;
    } catch (e) {
      return { ok: false, error: e.message };
    }
  }

  async function saveOne(song) {
    const base = lib.baseNameFor(song);
    let r;
    if (lib.isPlaylistUrl(song.audioUrl)) {
      r = await saveStream(song, base);
    } else {
      const filename = base + "." + lib.extFromUrl(song.audioUrl, "mp3");
      r = await bg({ type: "save", url: song.audioUrl, filename });
      if (!r.ok) r = await saveViaPage(song.audioUrl, filename);
      if (r.ok) song.file = r.file || filename;
    }
    if (r.ok && state.extras) {
      if (song.imageUrl) {
        const ext = lib.extFromUrl(song.imageUrl, "jpg");
        const img = ["jpg", "jpeg", "png", "webp", "gif", "avif"].includes(ext) ? ext : "jpg";
        const ir = await bg({ type: "save", url: song.imageUrl, filename: base + "." + img });
        if (!ir.ok) noteError("cover for " + song.title + ": " + ir.error);
      }
      const tr = await bg({ type: "save", url: lib.textDataUrl(lib.infoText(song)), filename: base + ".txt" });
      if (!tr.ok) noteError("lyrics sheet for " + song.title + ": " + tr.error);
    }
    return r;
  }

  async function saveSelected() {
    if (state.scanning || state.saving) return;
    const chosen = state.order.filter((id) => state.selected.has(id)).map((id) => state.songs.get(id)).filter(Boolean);
    const ready = chosen.filter((s) => s.audioUrl);
    if (!ready.length) {
      setStatus(chosen.length ? "None of the ticked songs has an audio link yet." : "Tick at least one song first.");
      return;
    }
    state.saving = true;
    state.stop = false;
    render();
    let ok = 0;
    let failed = 0;
    try {
      for (let i = 0; i < ready.length && !state.stop; i++) {
        const s = ready[i];
        setStatus("Saving " + (i + 1) + " of " + ready.length + ": " + (s.title || "Untitled"));
        const r = await saveOne(s);
        if (r.ok) {
          ok++;
          state.savedIds.add(s.id);
          state.selected.delete(s.id);
          s.saveError = "";
        } else {
          failed++;
          s.saveError = r.error;
          noteError((s.title || s.id) + ": " + r.error);
        }
        persist();
        scheduleRender();
      }
      const all = state.order.map((id) => state.songs.get(id)).filter(Boolean);
      const lr = await bg({ type: "save", url: lib.textDataUrl(lib.libraryJson(all), "application/json"), filename: "library.json", overwrite: true });
      if (!lr.ok) noteError("library.json: " + lr.error);
      setStatus((state.stop ? "Stopped. " : "Finished. ") + "Saved " + ok + " song" + (ok === 1 ? "" : "s") +
        (failed ? ", " + failed + " could not be saved (marked in red)." : ".") +
        " They are in your Downloads folder, under “Udio Rescue”.");
    } finally {
      state.saving = false;
      persist();
      render();
    }
  }

  function summary() {
    let ready = 0;
    let missing = 0;
    let saved = 0;
    for (const id of state.order) {
      const s = state.songs.get(id);
      if (!s) continue;
      if (state.savedIds.has(id)) saved++;
      if (s.audioUrl) ready++; else missing++;
    }
    return { total: state.order.length, ready, missing, saved, selected: state.selected.size };
  }

  function diagnosticsText() {
    return JSON.stringify(lib.diagnostics({
      version: VERSION,
      page: location.href,
      songs: state.order.map((id) => state.songs.get(id)).filter(Boolean),
      savedIds: Array.from(state.savedIds),
      endpoints: Array.from(state.endpoints.values()),
      sampleKeys: Array.from(state.fieldNames),
      media: state.media,
      errors: state.errors,
    }), null, 2);
  }

  // -------------------------------------------------------------------- UI
  let ui = null;
  let statusText = "Open your Udio library (the page listing the songs you made), then press “Find my songs”.";
  let expanded = false;

  function setStatus(t) {
    statusText = t;
    if (ui) ui.status.textContent = t;
  }

  const CSS = `
    :host { all: initial; }
    * { box-sizing: border-box; font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
    .pill { position: fixed; left: 16px; bottom: 16px; z-index: 2147483647; background: #ffb547; color: #1b1208;
      border: 0; border-radius: 999px; padding: 10px 16px; font-size: 14px; font-weight: 700; cursor: pointer;
      box-shadow: 0 4px 18px rgba(0,0,0,.4); }
    .panel { position: fixed; left: 16px; bottom: 16px; z-index: 2147483647; width: min(420px, calc(100vw - 32px));
      max-height: min(640px, calc(100vh - 32px)); display: flex; flex-direction: column; gap: 10px;
      background: #17141f; color: #f1edf7; border: 1px solid #3a3347; border-radius: 14px; padding: 14px;
      box-shadow: 0 10px 40px rgba(0,0,0,.55); font-size: 14px; line-height: 1.4; }
    .head { display: flex; align-items: center; justify-content: space-between; }
    .head b { font-size: 16px; color: #ffb547; }
    .x { background: none; border: 0; color: #b9b0c9; font-size: 20px; cursor: pointer; padding: 0 4px; }
    .status { background: #221d2d; border-radius: 8px; padding: 8px 10px; min-height: 38px; }
    .row { display: flex; gap: 8px; flex-wrap: wrap; }
    button.big { flex: 1; border: 0; border-radius: 9px; padding: 10px 12px; font-size: 14px; font-weight: 700; cursor: pointer; }
    .find { background: #ffb547; color: #1b1208; }
    .save { background: #5ee0a0; color: #06231a; }
    .stop { background: #ff6b6b; color: #2a0707; }
    button:disabled { opacity: .45; cursor: default; }
    .counts { color: #b9b0c9; font-size: 13px; }
    .list { overflow-y: auto; min-height: 60px; max-height: 260px; border: 1px solid #2f293b; border-radius: 8px; }
    .song { display: flex; align-items: center; gap: 8px; padding: 6px 8px; border-bottom: 1px solid #241f30; font-size: 13px; }
    .song:last-child { border-bottom: 0; }
    .song span.t { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .id { font-size: 11px; color: #7d7490; font-family: ui-monospace, monospace; }
    .tag { font-size: 11px; border-radius: 999px; padding: 2px 8px; white-space: nowrap; }
    .tag.ready { background: #1f3b2f; color: #8ff0bf; }
    .tag.saved { background: #26324a; color: #9cc1ff; }
    .tag.none { background: #3a3020; color: #ffd08a; }
    .tag.err { background: #4a2020; color: #ffaaaa; }
    .links { display: flex; gap: 12px; flex-wrap: wrap; font-size: 12px; }
    .links button { background: none; border: 0; color: #c9b8ff; text-decoration: underline; cursor: pointer; padding: 0; font-size: 12px; }
    label.opt { display: flex; gap: 6px; align-items: center; font-size: 13px; color: #d8d0e6; }
    .tip { font-size: 12px; color: #b9b0c9; }
    textarea { width: 100%; height: 120px; background: #0f0d14; color: #d8d0e6; border: 1px solid #3a3347; border-radius: 6px; font: 11px ui-monospace, monospace; }
    .empty { padding: 12px; color: #8f86a0; font-size: 13px; }
  `;

  function el(tag, props, kids) {
    const e = document.createElement(tag);
    Object.assign(e, props || {});
    for (const k of kids || []) e.append(k);
    return e;
  }

  function build() {
    const host = el("div", { id: "udio-rescue-host" });
    const root = host.attachShadow({ mode: "open" });
    root.append(el("style", { textContent: CSS }));

    const pill = el("button", { className: "pill", textContent: "Udio Rescue" });
    pill.onclick = () => { expanded = true; render(); };

    const status = el("div", { className: "status", textContent: statusText });
    const find = el("button", { className: "big find", textContent: "1. Find my songs" });
    const save = el("button", { className: "big save", textContent: "2. Save selected songs" });
    const stop = el("button", { className: "big stop", textContent: "Stop" });
    find.onclick = findSongs;
    save.onclick = saveSelected;
    stop.onclick = () => { state.stop = true; setStatus("Stopping…"); };

    const counts = el("div", { className: "counts" });
    const list = el("div", { className: "list" });
    list.addEventListener("change", (e) => {
      const id = e.target && e.target.dataset && e.target.dataset.id;
      if (!id) return;
      if (e.target.checked) state.selected.add(id); else state.selected.delete(id);
      renderCounts();
    });

    const extras = el("input", { type: "checkbox", checked: state.extras });
    extras.onchange = () => { state.extras = extras.checked; persist(); };
    const opt = el("label", { className: "opt" }, [extras, "Also save each song’s cover art and a lyrics/prompt sheet"]);

    const linkBtn = (text, fn) => { const b = el("button", { textContent: text }); b.onclick = fn; return b; };
    const diag = el("textarea", { readOnly: true });
    diag.style.display = "none";
    const links = el("div", { className: "links" }, [
      linkBtn("Tick all", () => { for (const id of state.order) state.selected.add(id); render(); }),
      linkBtn("Tick none", () => { state.selected.clear(); render(); }),
      linkBtn("Tick unsaved", () => { state.selected.clear(); for (const id of state.order) if (!state.savedIds.has(id)) state.selected.add(id); render(); }),
      linkBtn("Open Downloads folder", () => bg({ type: "open-folder" })),
      linkBtn("Copy troubleshooting report", async () => {
        const t = diagnosticsText();
        diag.value = t;
        diag.style.display = "block";
        try { await navigator.clipboard.writeText(t); setStatus("Troubleshooting report copied. Paste it to whoever is helping you."); }
        catch (_) { setStatus("Select the text in the box below and copy it."); diag.select(); }
      }),
      linkBtn("Clear list", () => {
        if (!confirm("Forget every song in this list? Files already saved on your computer are not touched.")) return;
        state.mine.clear(); state.order = []; state.selected.clear(); state.savedIds.clear();
        persist(); render(); setStatus("List cleared.");
      }),
    ]);
    const tip = el("div", { className: "tip", textContent:
      "Tip: if a song says “no audio yet”, press play on it in Udio for a second. The tool picks the audio link up as it plays." });

    const close = el("button", { className: "x", textContent: "×", title: "Hide" });
    close.onclick = () => { expanded = false; render(); };
    const panel = el("div", { className: "panel" }, [
      el("div", { className: "head" }, [el("b", { textContent: "Udio Rescue" }), close]),
      status, el("div", { className: "row" }, [find, save, stop]), counts, list, opt, links, tip, diag,
    ]);

    root.append(pill, panel);
    (document.body || document.documentElement).append(host);
    ui = { pill, panel, status, find, save, stop, counts, list, tip };
  }

  function renderCounts() {
    const s = summary();
    ui.counts.textContent = s.total + " songs found · " + s.ready + " ready · " + s.saved + " saved · " + s.selected + " ticked";
    ui.save.disabled = state.scanning || state.saving || s.selected === 0;
  }

  function render() {
    if (!ui) return;
    ui.pill.style.display = expanded ? "none" : "block";
    ui.panel.style.display = expanded ? "flex" : "none";
    ui.status.textContent = statusText;
    const busy = state.scanning || state.saving;
    ui.find.disabled = busy;
    ui.stop.style.display = busy ? "block" : "none";
    renderCounts();
    if (!expanded) return;

    const frag = document.createDocumentFragment();
    if (!state.order.length) frag.append(el("div", { className: "empty", textContent: "No songs yet." }));
    let missing = 0;
    for (const id of state.order) {
      const s = state.songs.get(id);
      if (!s) continue;
      const box = el("input", { type: "checkbox", checked: state.selected.has(id) });
      box.dataset.id = id;
      let tag;
      if (s.saveError) tag = el("span", { className: "tag err", textContent: "failed", title: s.saveError });
      else if (state.savedIds.has(id)) tag = el("span", { className: "tag saved", textContent: "saved" });
      else if (s.audioUrl) tag = el("span", { className: "tag ready", textContent: "ready" });
      else { tag = el("span", { className: "tag none", textContent: "no audio yet" }); missing++; }
      const title = el("span", { className: "t", textContent: s.title || "(no title yet)", title: s.title || id });
      // Udio makes two versions of each prompt with the same title; the id tells them apart.
      const sub = el("span", { className: "id", textContent: "#" + id.slice(0, 8) });
      frag.append(el("label", { className: "song" }, [box, title, sub, tag]));
    }
    ui.list.replaceChildren(frag);
    ui.tip.style.display = missing ? "block" : "none";
  }

  let renderQueued = false;
  function scheduleRender() {
    if (renderQueued) return;
    renderQueued = true;
    setTimeout(() => { renderQueued = false; render(); }, 250);
  }

  chrome.runtime.onMessage.addListener((msg) => {
    if (msg && msg.to === "udio-rescue-panel" && msg.type === "show") { expanded = true; render(); }
  });

  async function start() {
    await load();
    build();
    readPageScripts(false);
    if (state.order.length) {
      const s = summary();
      statusText = "Welcome back. " + s.total + " songs in your list, " + s.saved + " already saved.";
    }
    render();
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start);
  else start();
})();
