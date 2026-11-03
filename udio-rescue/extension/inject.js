/*
 * Udio Rescue — the listener. Runs inside the Udio page itself.
 *
 * It reads (never changes) the replies Udio's own site receives while you use
 * it and passes them to content.js, which picks out anything shaped like a
 * song. It is deliberately small and self-contained: Chrome loads each file
 * only once per page, so this one cannot share lib.js with content.js.
 * It also notes the audio address whenever a song starts playing.
 *
 * It reads only replies from Udio's own servers, and it never reads or sends
 * cookies or login tokens.
 */
(function () {
  "use strict";
  if (window.__udioRescueListening) return;
  window.__udioRescueListening = true;

  const MEDIA = /\.(mp3|m4a|aac|wav|flac|ogg|opus|webm|mp4|m3u8)(?:[?#]|$)/i;
  const TAG = "udio-rescue";
  const buffer = [];
  const MAX_BUFFER = 400;
  const MAX_BODY = 8 * 1024 * 1024;

  function post(msg) {
    msg.source = TAG;
    if (buffer.length < MAX_BUFFER) buffer.push(msg);
    try { window.postMessage(msg, location.origin); } catch (_) { /* page closing */ }
  }

  // content.js loads a little later; when it says hello, replay what it missed.
  window.addEventListener("message", (e) => {
    if (e.source !== window || !e.data || e.data.source !== TAG + "-panel") return;
    if (e.data.type === "hello") {
      for (const m of buffer) {
        try { window.postMessage(m, location.origin); } catch (_) { /* ignore */ }
      }
    }
  });

  function isUdioUrl(url) {
    try {
      const h = new URL(url, location.href).hostname;
      return h === "udio.com" || h.endsWith(".udio.com");
    } catch (_) {
      return false;
    }
  }

  function absolute(url) {
    try { return new URL(url, location.href).href; } catch (_) { return String(url); }
  }

  function noteMedia(url) {
    if (!url || typeof url !== "string" || url.startsWith("blob:") || url.startsWith("data:")) return;
    const abs = absolute(url);
    if (/^https?:/i.test(abs) && MEDIA.test(abs.split(/[?#]/)[0])) post({ type: "media", url: abs });
  }

  // Hand the reply's text to content.js, which picks the songs out of it.
  function examine(method, url, status, contentType, text) {
    if (!text || text.length > MAX_BODY) return;
    const ct = (contentType || "").toLowerCase();
    if (!/json|x-component|text\/plain|javascript/.test(ct) && !/^\s*[[{]/.test(text.slice(0, 64))) return;
    post({ type: "reply", method, url: absolute(url), status, text });
  }

  // --- fetch -------------------------------------------------------------
  const realFetch = window.fetch;
  if (typeof realFetch === "function") {
    window.fetch = function (input, init) {
      const url = typeof input === "string" ? input : (input && input.url) || String(input);
      const method = (init && init.method) || (input && input.method) || "GET";
      const p = realFetch.apply(this, arguments);
      try {
        noteMedia(url);
        if (isUdioUrl(url)) {
          p.then((res) => {
            try {
              const ct = res.headers.get("content-type") || "";
              if (/audio|video|image|octet-stream|font|css/.test(ct)) return;
              res.clone().text().then((t) => examine(method, res.url || url, res.status, ct, t)).catch(() => {});
            } catch (_) { /* ignore */ }
          }).catch(() => {});
        }
      } catch (_) { /* never break the page */ }
      return p;
    };
  }

  // --- XMLHttpRequest ----------------------------------------------------
  const XHR = window.XMLHttpRequest;
  if (XHR && XHR.prototype) {
    const open = XHR.prototype.open;
    const send = XHR.prototype.send;
    XHR.prototype.open = function (method, url) {
      try { this.__udioRescue = { method, url: String(url) }; } catch (_) { /* ignore */ }
      return open.apply(this, arguments);
    };
    XHR.prototype.send = function () {
      try {
        const info = this.__udioRescue;
        if (info) {
          noteMedia(info.url);
          if (isUdioUrl(info.url)) {
            this.addEventListener("load", () => {
              try {
                if (this.responseType && this.responseType !== "text" && this.responseType !== "json") return;
                const text = this.responseType === "json" ? JSON.stringify(this.response) : this.responseText;
                examine(info.method, this.responseURL || info.url, this.status, this.getResponseHeader("content-type"), text);
              } catch (_) { /* ignore */ }
            });
          }
        }
      } catch (_) { /* ignore */ }
      return send.apply(this, arguments);
    };
  }

  // --- audio that starts playing ------------------------------------------
  document.addEventListener("play", (e) => {
    const el = e.target;
    if (el && (el.tagName === "AUDIO" || el.tagName === "VIDEO")) noteMedia(el.currentSrc || el.src);
  }, true);
  document.addEventListener("loadedmetadata", (e) => {
    const el = e.target;
    if (el && (el.tagName === "AUDIO" || el.tagName === "VIDEO")) noteMedia(el.currentSrc || el.src);
  }, true);
})();
