/*
 * Udio Rescue — the pure logic.
 *
 * Nothing here touches the browser, the network or Chrome's APIs, so every
 * function can be tested in plain Node (see ../tests/udio-rescue.test.js).
 * The extension's other scripts load this file first and use it as
 * `UdioRescueLib`.
 *
 * Udio has no public API, and its site changes. So instead of depending on one
 * exact endpoint, this code recognises "a song" by its shape wherever it turns
 * up: in an API reply, in data embedded in a page, or in a link.
 */
(function (root) {
  "use strict";

  const AUDIO_EXT = /\.(mp3|m4a|aac|wav|flac|ogg|opus|webm|mp4)(?:[?#]|$)/i;
  const PLAYLIST_EXT = /\.m3u8(?:[?#]|$)/i;
  const IMAGE_EXT = /\.(jpe?g|png|webp|gif|avif)(?:[?#]|$)/i;

  // Field names that have held a song's audio link, in order of preference.
  const AUDIO_FIELDS = [
    "song_path", "songPath", "audio_url", "audioUrl", "audio_path", "audioPath",
    "mp3_url", "mp3Url", "wav_url", "wavUrl", "stream_url", "streamUrl",
    "media_url", "mediaUrl", "audio", "url",
  ];
  const IMAGE_FIELDS = [
    "image_path", "imagePath", "image_url", "imageUrl", "cover_url", "coverUrl",
    "artwork_url", "artworkUrl", "thumbnail", "image",
  ];
  const TITLE_FIELDS = ["title", "name", "song_title", "songTitle"];
  const LYRICS_FIELDS = ["lyrics", "lyric", "text_lyrics"];
  const PROMPT_FIELDS = ["prompt", "replaced_prompt", "description", "caption"];

  function isHttpUrl(u) {
    return typeof u === "string" && /^https?:\/\//i.test(u.trim());
  }
  function isAudioUrl(u) {
    return isHttpUrl(u) && AUDIO_EXT.test(urlPath(u));
  }
  function isPlaylistUrl(u) {
    return isHttpUrl(u) && PLAYLIST_EXT.test(urlPath(u));
  }
  function isImageUrl(u) {
    return isHttpUrl(u) && IMAGE_EXT.test(urlPath(u));
  }
  // The path part of a URL (so "?x=.mp3" in a query string doesn't fool us).
  function urlPath(u) {
    const s = String(u);
    const q = s.search(/[?#]/);
    return q === -1 ? s : s.slice(0, q);
  }

  function firstString(obj, fields) {
    for (const f of fields) {
      const v = obj[f];
      if (typeof v === "string" && v.trim()) return v.trim();
    }
    return "";
  }

  /** The audio link in a song-shaped object, or "" if it has none. */
  function audioUrlOf(o) {
    if (!o || typeof o !== "object") return "";
    for (const f of AUDIO_FIELDS) {
      const v = o[f];
      if (isAudioUrl(v) || isPlaylistUrl(v)) return v.trim();
      if (v && typeof v === "object" && (isAudioUrl(v.url) || isPlaylistUrl(v.url))) return v.url.trim();
    }
    // Any other string field that is plainly an audio file.
    for (const k of Object.keys(o)) {
      const v = o[k];
      if (isAudioUrl(v) || isPlaylistUrl(v)) return v.trim();
    }
    return "";
  }

  function imageUrlOf(o) {
    for (const f of IMAGE_FIELDS) {
      const v = o[f];
      if (isHttpUrl(v)) return v.trim();
      if (v && typeof v === "object" && isHttpUrl(v.url)) return v.url.trim();
    }
    return "";
  }

  function looksLikeId(v) {
    return typeof v === "string" && /^[A-Za-z0-9_-]{8,64}$/.test(v);
  }

  /**
   * Does this object look like a song? It needs an id, and either an audio
   * link or a title alongside something only songs have (lyrics, a prompt,
   * tags, a duration).
   */
  function looksLikeSong(o) {
    if (!o || typeof o !== "object" || Array.isArray(o)) return false;
    if (!looksLikeId(o.id) && !looksLikeId(o.song_id) && !looksLikeId(o.songId)) return false;
    if (audioUrlOf(o)) return true;
    const hasTitle = !!firstString(o, TITLE_FIELDS);
    const songish = LYRICS_FIELDS.concat(["prompt", "tags", "duration", "song_path", "songPath"])
      .some((f) => f in o);
    return hasTitle && songish;
  }

  function asTags(v) {
    if (Array.isArray(v)) return v.map((t) => (typeof t === "string" ? t : t && t.name)).filter(Boolean).map(String);
    if (typeof v === "string" && v.trim()) return v.split(",").map((t) => t.trim()).filter(Boolean);
    return [];
  }

  /** Turn any song-shaped object into the one shape the extension keeps. */
  function normalizeSong(o) {
    const id = String(o.id || o.song_id || o.songId);
    const artist = o.artist || o.artist_name || o.user_name || o.username ||
      (o.user && (o.user.display_name || o.user.username || o.user.name)) || "";
    return {
      id,
      title: firstString(o, TITLE_FIELDS),
      audioUrl: audioUrlOf(o),
      imageUrl: imageUrlOf(o),
      lyrics: firstString(o, LYRICS_FIELDS),
      prompt: firstString(o, PROMPT_FIELDS),
      tags: asTags(o.tags),
      artist: typeof artist === "string" ? artist : "",
      createdAt: typeof o.created_at === "string" ? o.created_at :
        typeof o.createdAt === "string" ? o.createdAt : "",
      duration: typeof o.duration === "number" && isFinite(o.duration) ? o.duration : null,
    };
  }

  /** Combine two records of the same song, keeping whatever each one knows. */
  function mergeSong(a, b) {
    if (!a) return Object.assign({}, b);
    if (!b) return Object.assign({}, a);
    const out = Object.assign({}, a);
    for (const k of Object.keys(b)) {
      if (k === "title" || k === "titleFromLink") continue;
      const v = b[k];
      const empty = v === "" || v === null || v === undefined || (Array.isArray(v) && v.length === 0);
      if (!empty) out[k] = v;
    }
    // A title read off a link is only a guess; a title from Udio's data wins.
    if (b.title && (!a.title || (a.titleFromLink && !b.titleFromLink))) {
      out.title = b.title;
      if (b.titleFromLink) out.titleFromLink = true;
      else delete out.titleFromLink;
    }
    return out;
  }

  /** Every song-shaped object anywhere inside `data`. */
  function findSongs(data, maxNodes) {
    const limit = maxNodes || 200000;
    const found = new Map();
    const stack = [data];
    const seen = new Set();
    let n = 0;
    while (stack.length && n < limit) {
      const node = stack.pop();
      n++;
      if (!node || typeof node !== "object" || seen.has(node)) continue;
      seen.add(node);
      if (looksLikeSong(node)) {
        const s = normalizeSong(node);
        found.set(s.id, mergeSong(found.get(s.id), s));
      }
      // Push children last-first, so songs come out in the order Udio sent them.
      const keys = Object.keys(node);
      for (let i = keys.length - 1; i >= 0; i--) {
        const v = node[keys[i]];
        if (v && typeof v === "object") stack.push(v);
      }
    }
    return Array.from(found.values());
  }

  /**
   * Parse a reply that may be JSON, or a stream of JSON pieces (Next.js sends
   * lines like `1a:{"..."}`). Returns every value that parsed.
   */
  function parseLooseJson(text) {
    if (typeof text !== "string" || !text) return [];
    const t = text.trim();
    try { return [JSON.parse(t)]; } catch (_) { /* fall through */ }
    const out = [];
    for (const line of t.split("\n")) {
      const m = line.match(/^[0-9a-zA-Z]{1,8}:(.*)$/);
      const body = (m ? m[1] : line).trim();
      if (!body || (body[0] !== "{" && body[0] !== "[")) continue;
      try { out.push(JSON.parse(body)); } catch (_) { /* not JSON */ }
    }
    return out;
  }

  /**
   * Songs inside the text of one of the page's own <script> tags. Next.js
   * pages carry their data either as plain JSON or as
   * `self.__next_f.push([1,"<escaped text>"])` calls.
   */
  function songsFromScriptText(text) {
    if (typeof text !== "string" || !text) return [];
    const values = [];
    const t = text.trim();
    if (t[0] === "{" || t[0] === "[") values.push(...parseLooseJson(t));
    const re = /__next_f\.push\(\[\s*\d+\s*,\s*("(?:[^"\\]|\\.)*")\s*\]\)/g;
    let m;
    while ((m = re.exec(text))) {
      let inner;
      try { inner = JSON.parse(m[1]); } catch (_) { continue; }
      values.push(...parseLooseJson(inner));
    }
    const found = new Map();
    for (const v of values) for (const s of findSongs(v)) found.set(s.id, mergeSong(found.get(s.id), s));
    return Array.from(found.values());
  }

  /** Song ids from page links like /songs/abc123. */
  function songIdFromHref(href) {
    if (typeof href !== "string") return "";
    const m = href.match(/\/songs?\/([A-Za-z0-9_-]{8,64})(?:[/?#]|$)/);
    if (!m) return "";
    // Real song ids are long or contain a digit; this skips words like /songs/favorites.
    const id = m[1];
    return id.length >= 16 || /\d/.test(id) ? id : "";
  }

  function decodeEntities(s) {
    return s.replace(/&amp;/g, "&").replace(/&quot;/g, '"').replace(/&#39;/g, "'")
      .replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&#x27;/g, "'");
  }

  function metaContent(html, prop) {
    const re1 = new RegExp('<meta[^>]+(?:property|name)=["\']' + prop.replace(/[:.]/g, "\\$&") +
      '["\'][^>]*content=["\']([^"\']*)["\']', "i");
    const re2 = new RegExp('<meta[^>]+content=["\']([^"\']*)["\'][^>]*(?:property|name)=["\']' +
      prop.replace(/[:.]/g, "\\$&") + '["\']', "i");
    const m = html.match(re1) || html.match(re2);
    return m ? decodeEntities(m[1]).trim() : "";
  }

  /**
   * What a song's own web page tells us about it: the share tags (og:audio,
   * og:title, og:image) and any song data embedded in the page's scripts.
   */
  function songFromHtml(html, id) {
    if (typeof html !== "string") return null;
    const song = { id, title: "", audioUrl: "", imageUrl: "", lyrics: "", prompt: "", tags: [], artist: "", createdAt: "", duration: null };

    // 1. Song data embedded in the page (Next.js puts it in scripts, often
    //    with its quotes escaped).
    const unescaped = html.replace(/\\\\/g, "\\").replace(/\\"/g, '"').replace(/\\\//g, "/").replace(/\\u0026/g, "&");
    const scriptRe = /<script\b[^>]*>([\s\S]*?)<\/script>/gi;
    let sm;
    while ((sm = scriptRe.exec(html))) {
      for (const s of songsFromScriptText(sm[1])) {
        if (s.id === id) Object.assign(song, mergeSong(song, s));
      }
    }
    if (!song.audioUrl) {
      // Pull any {"id":"<id>", ...} object out of the unescaped text.
      const at = unescaped.indexOf('"id":"' + id + '"');
      if (at !== -1) {
        const windowText = unescaped.slice(Math.max(0, at - 4000), at + 4000);
        const urls = windowText.match(/https?:\/\/[^"'\s<>\\]+/g) || [];
        const audio = urls.find((u) => isAudioUrl(u) || isPlaylistUrl(u));
        if (audio) song.audioUrl = audio;
      }
    }

    // 2. Share tags.
    const ogAudio = metaContent(html, "og:audio") || metaContent(html, "og:audio:url") ||
      metaContent(html, "og:audio:secure_url") || metaContent(html, "twitter:player:stream");
    if (!song.audioUrl && (isAudioUrl(ogAudio) || isPlaylistUrl(ogAudio))) song.audioUrl = ogAudio;
    if (!song.title) {
      const t = metaContent(html, "og:title") || ((html.match(/<title>([^<]*)<\/title>/i) || [])[1] || "");
      song.title = decodeEntities(t).replace(/\s*[|\-–]\s*Udio.*$/i, "").trim();
    }
    if (!song.imageUrl) {
      const img = metaContent(html, "og:image");
      if (isHttpUrl(img)) song.imageUrl = img;
    }

    // 3. Last resort: an audio link anywhere in the page that names this song.
    if (!song.audioUrl) {
      const urls = unescaped.match(/https?:\/\/[^"'\s<>\\]+/g) || [];
      const named = urls.find((u) => (isAudioUrl(u) || isPlaylistUrl(u)) && u.includes(id));
      if (named) song.audioUrl = named;
    }
    return song;
  }

  /** A name that is safe as a file name on Windows, Mac and Linux. */
  function safeFilename(s, maxLen) {
    const max = maxLen || 90;
    let out = String(s || "")
      .replace(/[\u0000-\u001f\u007f]/g, " ")
      .replace(/[<>:"/\\|?*]/g, " ")
      .replace(/\s+/g, " ")
      .trim()
      .replace(/^[.\s]+|[.\s]+$/g, "");
    if (/^(con|prn|aux|nul|com\d|lpt\d)$/i.test(out)) out = "_" + out;
    if (out.length > max) out = out.slice(0, max).trim().replace(/[.\s]+$/, "");
    return out || "Untitled";
  }

  /** The file extension a URL points at, e.g. "mp3". */
  function extFromUrl(u, fallback) {
    const m = urlPath(u || "").match(/\.([A-Za-z0-9]{2,5})$/);
    return m ? m[1].toLowerCase() : (fallback || "mp3");
  }

  /**
   * The base file name for a song: its title plus the start of its id, so the
   * two versions Udio makes of every prompt (same title) never overwrite each
   * other.
   */
  function baseNameFor(song) {
    return safeFilename(song.title || "Untitled") + " [" + String(song.id).slice(0, 8) + "]";
  }

  /** The text file saved beside each song. */
  function infoText(song) {
    const lines = [song.title || "Untitled", ""];
    if (song.artist) lines.push("Artist: " + song.artist);
    if (song.createdAt) lines.push("Created: " + song.createdAt);
    if (song.duration) lines.push("Length: " + formatDuration(song.duration));
    if (song.tags && song.tags.length) lines.push("Tags: " + song.tags.join(", "));
    lines.push("Udio link: https://www.udio.com/songs/" + song.id);
    if (song.prompt) lines.push("", "Prompt:", song.prompt);
    if (song.lyrics) lines.push("", "Lyrics:", song.lyrics);
    return lines.join("\n") + "\n";
  }

  function formatDuration(sec) {
    const s = Math.round(sec);
    return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0");
  }

  /** The library.json index saved with everything. */
  function libraryJson(songs, savedAt) {
    return JSON.stringify({
      savedBy: "Udio Rescue",
      savedAt: savedAt || new Date().toISOString(),
      count: songs.length,
      songs: songs.map((s) => ({
        id: s.id, title: s.title, file: s.file || "", artist: s.artist, createdAt: s.createdAt,
        duration: s.duration, tags: s.tags, prompt: s.prompt, lyrics: s.lyrics,
        audioUrl: s.audioUrl, imageUrl: s.imageUrl, link: "https://www.udio.com/songs/" + s.id,
      })),
    }, null, 2);
  }

  /** Text → a data: link Chrome can save as a file. */
  function textDataUrl(text, mime) {
    return "data:" + (mime || "text/plain") + ";charset=utf-8," + encodeURIComponent(text);
  }

  function resolveUrl(ref, base) {
    try { return new URL(ref, base).href; } catch (_) { return ref; }
  }

  /**
   * Read an HLS playlist (.m3u8). Returns the quality variants (for a master
   * playlist) or the segments (for a media playlist), and whether the stream
   * is encrypted. Encrypted streams are never downloaded.
   */
  function parseM3U8(text, baseUrl) {
    const result = { variants: [], segments: [], initUrl: "", encrypted: false, valid: false };
    if (typeof text !== "string") return result;
    const lines = text.split(/\r?\n/).map((l) => l.trim());
    if (lines[0] !== "#EXTM3U") return result;
    result.valid = true;
    let pendingBandwidth = null;
    for (const line of lines) {
      if (!line) continue;
      if (line.startsWith("#EXT-X-STREAM-INF")) {
        const bw = line.match(/BANDWIDTH=(\d+)/);
        pendingBandwidth = bw ? Number(bw[1]) : 0;
      } else if (line.startsWith("#EXT-X-KEY") || line.startsWith("#EXT-X-SESSION-KEY")) {
        const method = (line.match(/METHOD=([A-Z0-9-]+)/) || [])[1];
        if (method && method !== "NONE") result.encrypted = true;
      } else if (line.startsWith("#EXT-X-MAP")) {
        const uri = (line.match(/URI="([^"]+)"/) || [])[1];
        if (uri) result.initUrl = resolveUrl(uri, baseUrl);
      } else if (!line.startsWith("#")) {
        const url = resolveUrl(line, baseUrl);
        if (pendingBandwidth !== null) {
          result.variants.push({ bandwidth: pendingBandwidth, url });
          pendingBandwidth = null;
        } else {
          result.segments.push(url);
        }
      }
    }
    result.variants.sort((a, b) => b.bandwidth - a.bandwidth);
    return result;
  }

  /** The file type to give a stream rebuilt from its segments. */
  function extForSegments(parsed) {
    if (parsed.initUrl) return "m4a";
    const first = parsed.segments[0] || "";
    const ext = extFromUrl(first, "ts");
    if (ext === "m4s" || ext === "mp4") return "m4a";
    if (["aac", "mp3", "ts"].includes(ext)) return ext;
    return "ts";
  }

  /**
   * A summary of what the tool has seen, for troubleshooting. It records
   * which kinds of addresses Udio used and which fields its songs had — never
   * cookies, tokens, or song contents.
   */
  function diagnostics(state) {
    const songs = state.songs || [];
    const keyCounts = {};
    for (const k of state.sampleKeys || []) keyCounts[k] = (keyCounts[k] || 0) + 1;
    return {
      tool: "Udio Rescue",
      version: state.version || "",
      page: state.page ? urlPath(state.page) : "",
      songsFound: songs.length,
      songsWithAudio: songs.filter((s) => s.audioUrl).length,
      songsSaved: (state.savedIds || []).length,
      audioHosts: Array.from(new Set(songs.filter((s) => s.audioUrl).map((s) => hostOf(s.audioUrl)))),
      audioKinds: Array.from(new Set(songs.filter((s) => s.audioUrl).map((s) => (isPlaylistUrl(s.audioUrl) ? "m3u8" : extFromUrl(s.audioUrl))))),
      endpoints: (state.endpoints || []).slice(0, 60),
      songFieldNames: Object.keys(keyCounts).sort(),
      unmatchedMedia: (state.media || []).slice(0, 20).map((u) => hostOf(u) + " ." + (isPlaylistUrl(u) ? "m3u8" : extFromUrl(u, "?"))),
      errors: (state.errors || []).slice(-20),
    };
  }

  function hostOf(u) {
    try { return new URL(u).host; } catch (_) { return ""; }
  }

  /** "GET /api/songs?songIds=…" → "GET /api/songs?songIds" (values dropped). */
  function endpointLabel(method, url) {
    try {
      const u = new URL(url, "https://www.udio.com/");
      const keys = Array.from(u.searchParams.keys());
      const path = u.pathname.replace(/[A-Za-z0-9_-]{20,}/g, ":id");
      return (method || "GET").toUpperCase() + " " + u.host + path + (keys.length ? "?" + keys.join("&") : "");
    } catch (_) {
      return (method || "GET").toUpperCase() + " ?";
    }
  }

  const lib = {
    isAudioUrl, isPlaylistUrl, isImageUrl, audioUrlOf, imageUrlOf, looksLikeSong, normalizeSong,
    mergeSong, findSongs, parseLooseJson, songsFromScriptText, songIdFromHref, songFromHtml, safeFilename, extFromUrl,
    baseNameFor, infoText, libraryJson, textDataUrl, parseM3U8, extForSegments, diagnostics,
    endpointLabel, hostOf, formatDuration,
  };

  if (typeof module !== "undefined" && module.exports) module.exports = lib;
  else root.UdioRescueLib = lib;
})(typeof globalThis !== "undefined" ? globalThis : this);
