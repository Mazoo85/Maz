#!/usr/bin/env node
/*
 * CODA PICS — the Google Photos connector, tested offline.
 *
 *   node coda-pics/tests/gphotos.test.js
 *
 * No network, no Google account, no credentials. Every call the connector
 * makes goes through an injected fetch, so a fake Google can answer them —
 * including the failures that matter: a 403 from the retired Library scopes,
 * a sign-in that did not start here, a session nobody ever finishes.
 *
 * What this CANNOT prove is the part only real credentials can: that Google
 * accepts the client, and that a browser is allowed to fetch a picked photo's
 * baseUrl cross-origin. Those are called out in the connector's docs rather
 * than papered over here.
 */
'use strict';

var path = require('path');
var G = require(path.join(__dirname, '..', 'js', 'gphotos.js'));

var failures = 0;
var checks = 0;
function check(cond, msg) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL  ' + msg); }
}
function pass(msg) { console.log('  ok    ' + msg); }
function section(name) { console.log('\n' + name); }

/* A store that behaves like sessionStorage, and a fake Google. */
function makeStore() {
  var data = {};
  return {
    get: function (k) { return Object.prototype.hasOwnProperty.call(data, k) ? data[k] : null; },
    set: function (k, v) { data[k] = v; },
    remove: function (k) { delete data[k]; },
    all: data
  };
}

function ok(body) {
  return Promise.resolve({
    ok: true, status: 200,
    json: function () { return Promise.resolve(body); },
    blob: function () { return Promise.resolve({ fake: 'blob', size: 1234 }); }
  });
}
function fail(status) {
  return Promise.resolve({ ok: false, status: status, json: function () { return Promise.resolve({}); } });
}

/* ---------------------------------------------------------------- the flow */
section('Signing in');

(async function () {
  var store = makeStore();
  var sent = null;
  var calls = [];
  var c = new G.Connector({
    clientId: 'test-client.apps.googleusercontent.com',
    redirectUri: 'https://example.test/coda-pics/',
    store: store,
    go: function (url) { sent = url; },
    fetch: function (url, opts) {
      calls.push({ url: url, opts: opts });
      return ok({ access_token: 'tok-123', expires_in: 3600 });
    }
  });

  var url = await c.beginSignIn();
  check(url.indexOf(G.endpoints.AUTH) === 0, 'it sends people to Google to sign in');
  check(url.indexOf('code_challenge=') > 0 && url.indexOf('code_challenge_method=S256') > 0,
    'with a PKCE challenge, since a page has no secret to keep');
  check(url.indexOf(encodeURIComponent(G.SCOPE)) > 0,
    'asking only for the picker scope');
  check(url.indexOf('response_type=code') > 0, 'using the code flow');
  check(sent === url, 'and actually goes there');
  check(!!store.get('gphotos.verifier') && !!store.get('gphotos.state'),
    'keeping the verifier and state for the trip back');
  pass('sign-in starts correctly');

  /* A code that came back with the wrong state is not ours. */
  var rejected = false;
  try { await c.completeSignIn('code-1', 'not-the-state'); } catch (e) { rejected = true; }
  check(rejected, 'a sign-in that did not start here is refused');

  var token = await c.completeSignIn('code-1', store.get('gphotos.state'));
  check(token === 'tok-123', 'a good code becomes a token');
  check(store.get('gphotos.verifier') === null, 'and the verifier is not left lying about');
  var tokenCall = calls[calls.length - 1];
  check(tokenCall.url === G.endpoints.TOKEN, 'the swap happens at the token endpoint');
  check(tokenCall.opts.body.indexOf('code_verifier=') >= 0, 'and proves it with the verifier');
  pass('the code is exchanged for a token, once, and only for us');
})().then(function () {

  /* ------------------------------------------------------------ picking */
  section('Picking photos');

  var polls = 0;
  var store = makeStore();
  var c = new G.Connector({
    clientId: 'x', redirectUri: 'https://example.test/', store: store,
    fetch: function (url, opts) {
      var auth = opts && opts.headers && opts.headers.Authorization;
      if (!auth) return fail(401);
      if (/\/sessions$/.test(url)) {
        return ok({ id: 'sess-1', pickerUri: 'https://photos.google.com/pick/abc',
          pollingConfig: { pollInterval: '0.01s' }, mediaItemsSet: false });
      }
      if (/\/sessions\/sess-1$/.test(url)) {
        polls++;
        return ok({ id: 'sess-1', mediaItemsSet: polls >= 3 });
      }
      if (/\/mediaItems\?/.test(url)) {
        return ok({ mediaItems: [
          { id: 'a', mediaFile: { baseUrl: 'https://lh3.test/a', mimeType: 'image/jpeg', filename: 'beach.jpg' } },
          { id: 'b', mediaFile: { baseUrl: 'https://lh3.test/b', mimeType: 'video/mp4', filename: 'clip.mp4' } },
          { id: 'c', mediaFile: { baseUrl: 'https://lh3.test/c', mimeType: 'image/png', filename: 'hill.png' } }
        ] });
      }
      if (/lh3\.test/.test(url)) {
        check(/=w\d+-h\d+$/.test(url), 'a photo is asked for at a sensible size (' + url + ')');
        return ok(null);
      }
      return fail(404);
    }
  });
  c.token = 'tok';

  var shownPicker = null;
  c.pick({
    onPicker: function (uri) { shownPicker = uri; },
    sleep: function () { return Promise.resolve(); },
    limit: 10
  }).then(function (photos) {
    check(shownPicker === 'https://photos.google.com/pick/abc',
      'the person is sent to Google\'s own picker');
    check(polls >= 3, 'it waits for them to finish choosing (' + polls + ' polls)');
    check(photos.length === 2, 'only the photographs come back, not the video (' + photos.length + ')');
    check(photos[0].name === 'beach.jpg' && photos[1].name === 'hill.png', 'each keeps its name');
    check(photos.every(function (p) { return p.blob; }), 'and each arrives as pixels to measure');
    pass('a full pick, from session to photographs');

    /* ------------------------------------------------------- the failures */
    section('When Google says no');

    var dead = new G.Connector({
      clientId: 'x', redirectUri: 'https://example.test/', store: makeStore(),
      fetch: function () { return fail(403); }
    });
    dead.token = 'tok';
    return dead.createSession().then(function () {
      check(false, 'a 403 should not look like success');
    }, function (err) {
      check(/403/.test(err.message) && /Picker API/.test(err.message),
        'a 403 explains that the old library scopes are gone (' + err.message + ')');
    });
  }).then(function () {
    var never = new G.Connector({
      clientId: 'x', redirectUri: 'https://example.test/', store: makeStore(),
      fetch: function (url) {
        if (/\/sessions$/.test(url)) {
          return ok({ id: 's', pickerUri: 'p', pollingConfig: { pollInterval: '0.01s' } });
        }
        return ok({ id: 's', mediaItemsSet: false });
      }
    });
    never.token = 'tok';
    var clock = 0;
    return never.pick({
      sleep: function () { clock += 1000; return Promise.resolve(); },
      now: function () { return clock; },
      timeoutMs: 3000
    }).then(function () {
      check(false, 'a pick nobody finishes should not resolve');
    }, function (err) {
      check(/in time/.test(err.message), 'a pick nobody finishes gives up (' + err.message + ')');
    });
  }).then(function () {
    check(G.pollInterval({ pollingConfig: { pollInterval: '5s' } }) === 5000,
      'it polls as often as Google asks');
    check(G.pollInterval({}) === 3000, 'and has a sane default');
    check(G.pollInterval({ pollingConfig: { pollInterval: '0.001s' } }) === 1000,
      'but never hammers the API');
    check(!G.isPhoto({ mediaFile: { mimeType: 'video/mp4', baseUrl: 'x' } }), 'videos are not photos');
    check(!G.isPhoto({}), 'and neither is nothing');
    pass('failures are reported as themselves');

    console.log('\n' + (failures ? 'FAILED ' + failures + ' of ' + checks + ' checks'
      : 'All ' + checks + ' checks passed'));
    process.exit(failures ? 1 : 0);
  });
}).catch(function (err) {
  console.log('  FAIL  the run itself threw: ' + err.message);
  process.exit(1);
});
