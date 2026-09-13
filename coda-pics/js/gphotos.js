/*
 * CODA PICS — the Google Photos connector.
 * ----------------------------------------
 * Picks photographs out of a Google Photos library and hands them to the app,
 * which measures them exactly as it measures a photo chosen off the device.
 *
 * WHY IT IS SHAPED LIKE THIS. Google removed the read scopes that let an app
 * browse somebody's library — photoslibrary.readonly and its siblings stopped
 * working on 31 March 2025, and now return 403. The only sanctioned way in is
 * the Picker API: the app opens Google's own picker, the person chooses, and
 * the app is granted those photographs and nothing else.
 *
 * That is a deliberate design, and it is the reason this lives in the app
 * rather than in a Claude connector: the flow *requires a human to pick*, so
 * something that ran without one could never use it.
 *
 * The flow:
 *   1. sessions.create            -> a pickerUri to send the person to
 *   2. (they pick, in Google's UI, in another tab)
 *   3. sessions.get, polled       -> mediaItemsSet turns true
 *   4. mediaItems.list            -> the chosen items and their baseUrls
 *   5. each baseUrl fetched with the token, read into a canvas, measured
 *
 * Nothing is uploaded. Photographs come *in*, are measured in the page, and
 * only the resulting palette is ever kept — the same as a photo off the disk.
 * The one thing that changes is that this feature talks to Google, so it is
 * the only part of CODA PICS that needs a network.
 *
 * Every network call goes through an injected `fetch`, so the whole flow is
 * tested offline against a fake transport.
 *
 * Exposed as window.CodaGPhotos (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var AUTH = 'https://accounts.google.com/o/oauth2/v2/auth';
  var TOKEN = 'https://oauth2.googleapis.com/token';
  var PICKER = 'https://photospicker.googleapis.com/v1';
  var SCOPE = 'https://www.googleapis.com/auth/photospicker.mediaitems.readonly';

  /* ------------------------------------------------------------------ PKCE
   * A page with no server of its own cannot keep a client secret, so it proves
   * it started the exchange by keeping a random verifier and sending only its
   * hash up front. This is the flow Google asks browser apps to use.
   */
  function randomString(len) {
    var bytes = new Uint8Array(len);
    (root.crypto || root.msCrypto).getRandomValues(bytes);
    var out = '';
    for (var i = 0; i < bytes.length; i++) {
      out += 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~'
        .charAt(bytes[i] % 66);
    }
    return out;
  }

  function base64url(buffer) {
    var bytes = new Uint8Array(buffer), str = '';
    for (var i = 0; i < bytes.length; i++) str += String.fromCharCode(bytes[i]);
    return root.btoa(str).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
  }

  function challengeFor(verifier) {
    return root.crypto.subtle
      .digest('SHA-256', new TextEncoder().encode(verifier))
      .then(base64url);
  }

  /* -------------------------------------------------------------- the flow */

  /*
   * `deps` is everything touching the outside world: fetch, where to send the
   * person, and where to keep the verifier between the redirect out and back.
   * The tests supply all three.
   */
  function Connector(deps) {
    this.fetch = deps.fetch;
    this.store = deps.store;                       // { get, set, remove }
    this.go = deps.go || function () {};           // send the browser somewhere
    this.clientId = deps.clientId;
    this.redirectUri = deps.redirectUri;
    this.token = null;
  }

  /* Step 0: send them to Google to say yes. */
  Connector.prototype.beginSignIn = function () {
    var self = this;
    var verifier = randomString(64);
    var state = randomString(24);
    return challengeFor(verifier).then(function (challenge) {
      self.store.set('gphotos.verifier', verifier);
      self.store.set('gphotos.state', state);
      var url = AUTH +
        '?client_id=' + encodeURIComponent(self.clientId) +
        '&redirect_uri=' + encodeURIComponent(self.redirectUri) +
        '&response_type=code' +
        '&scope=' + encodeURIComponent(SCOPE) +
        '&code_challenge=' + encodeURIComponent(challenge) +
        '&code_challenge_method=S256' +
        '&state=' + encodeURIComponent(state) +
        '&access_type=online' +
        '&prompt=consent';
      self.go(url);
      return url;
    });
  };

  /* Step 0b: back from Google with a code. Swap it for a token. */
  Connector.prototype.completeSignIn = function (code, state) {
    var self = this;
    var expected = this.store.get('gphotos.state');
    if (!expected || state !== expected) {
      return Promise.reject(new Error('That sign-in did not come from this page.'));
    }
    var verifier = this.store.get('gphotos.verifier');
    if (!verifier) return Promise.reject(new Error('This page did not start that sign-in.'));

    var body = 'client_id=' + encodeURIComponent(this.clientId) +
      '&code=' + encodeURIComponent(code) +
      '&code_verifier=' + encodeURIComponent(verifier) +
      '&grant_type=authorization_code' +
      '&redirect_uri=' + encodeURIComponent(this.redirectUri);

    return this.fetch(TOKEN, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    }).then(readJson).then(function (data) {
      self.store.remove('gphotos.verifier');
      self.store.remove('gphotos.state');
      if (!data || !data.access_token) throw new Error('Google did not return a token.');
      self.token = data.access_token;
      return self.token;
    });
  };

  function readJson(res) {
    if (!res || !res.ok) {
      var status = res ? res.status : 0;
      /* 403 here almost always means the old library scopes, which no longer
       * work for anyone. Saying so beats "request failed". */
      if (status === 403) {
        throw new Error('Google refused that (403). The Picker API must be enabled for ' +
          'this client, and the old Photos Library scopes no longer work.');
      }
      throw new Error('Google replied ' + status + '.');
    }
    return res.json();
  }

  Connector.prototype.authHeaders = function () {
    return { Authorization: 'Bearer ' + this.token };
  };

  /* Step 1: a session, which is what the picker is opened against. */
  Connector.prototype.createSession = function () {
    return this.fetch(PICKER + '/sessions', {
      method: 'POST',
      headers: this.authHeaders()
    }).then(readJson);
  };

  /* Step 2 and 3: wait for them to finish choosing. Google tells us how often
   * to ask and when to give up; both are honoured rather than guessed. */
  Connector.prototype.waitForPicks = function (session, opts) {
    var self = this;
    opts = opts || {};
    var sleep = opts.sleep || function (ms) {
      return new Promise(function (r) { setTimeout(r, ms); });
    };
    var everyMs = pollInterval(session);
    var deadline = (opts.now ? opts.now() : Date.now()) + (opts.timeoutMs || 5 * 60 * 1000);

    function ask() {
      return self.fetch(PICKER + '/sessions/' + encodeURIComponent(session.id), {
        headers: self.authHeaders()
      }).then(readJson).then(function (state) {
        if (state && state.mediaItemsSet) return state;
        var now = opts.now ? opts.now() : Date.now();
        if (now >= deadline) throw new Error('Nothing was picked in time.');
        return sleep(everyMs).then(ask);
      });
    }
    return ask();
  };

  /* pollingConfig.pollInterval arrives as a duration like "3s". */
  function pollInterval(session) {
    var raw = session && session.pollingConfig && session.pollingConfig.pollInterval;
    var seconds = parseFloat(String(raw || '3s'));
    if (!isFinite(seconds) || seconds <= 0) seconds = 3;
    return Math.max(1000, Math.min(30000, seconds * 1000));
  }

  /* Step 4: what they chose. Paged, because somebody will pick sixty. */
  Connector.prototype.listPicked = function (session, limit) {
    var self = this;
    var out = [];
    var cap = limit || 24;

    function page(token) {
      var url = PICKER + '/mediaItems?sessionId=' + encodeURIComponent(session.id) +
        '&pageSize=' + Math.min(cap, 100) + (token ? '&pageToken=' + encodeURIComponent(token) : '');
      return self.fetch(url, { headers: self.authHeaders() })
        .then(readJson)
        .then(function (data) {
          (data.mediaItems || []).forEach(function (item) { out.push(item); });
          if (data.nextPageToken && out.length < cap) return page(data.nextPageToken);
          return out.slice(0, cap);
        });
    }
    return page(null);
  };

  /* Only photographs, and only ones with something to fetch. */
  function isPhoto(item) {
    var file = item && item.mediaFile;
    if (!file || !file.baseUrl) return false;
    var type = String(file.mimeType || '');
    return type.indexOf('image/') === 0;
  }

  /*
   * Step 5: the pixels. A baseUrl needs a size suffix and the token — it is not
   * a public link, which is the point. Fetched as a blob so it can be drawn to
   * a canvas and measured without ever going anywhere else.
   */
  Connector.prototype.fetchPhoto = function (item, size) {
    var px = size || 640;
    var url = item.mediaFile.baseUrl + '=w' + px + '-h' + px;
    return this.fetch(url, { headers: this.authHeaders() }).then(function (res) {
      if (!res || !res.ok) throw new Error('That photo could not be fetched.');
      return res.blob();
    });
  };

  /* The whole thing, for the app: sign in if needed, open the picker, wait,
   * and come back with blobs. */
  Connector.prototype.pick = function (opts) {
    var self = this;
    opts = opts || {};
    return this.createSession().then(function (session) {
      if (opts.onPicker) opts.onPicker(session.pickerUri, session);
      return self.waitForPicks(session, opts).then(function () {
        return self.listPicked(session, opts.limit);
      });
    }).then(function (items) {
      var photos = items.filter(isPhoto);
      if (!photos.length) throw new Error('Nothing that could be painted with was picked.');
      return Promise.all(photos.map(function (item) {
        return self.fetchPhoto(item, opts.size).then(function (blob) {
          return { id: item.id, name: (item.mediaFile && item.mediaFile.filename) || 'photo', blob: blob };
        });
      }));
    });
  };

  var API = {
    Connector: Connector,
    SCOPE: SCOPE,
    endpoints: { AUTH: AUTH, TOKEN: TOKEN, PICKER: PICKER },
    isPhoto: isPhoto,
    pollInterval: pollInterval,
    randomString: randomString
  };

  root.CodaGPhotos = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
