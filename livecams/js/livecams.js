/*
 * LIVE CAMS — Engine
 * ------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test.
 *
 * The wall is a list of streams the viewer pinned. Each one is a public
 * livestream someone chose to broadcast — a city webcam or a person's own
 * stream — given as a link. This module turns a pasted link into something
 * the page can embed, keeps the list tidy (no duplicates, no junk), and
 * reads and writes the save format.
 *
 * Streams change address often, so the app ships no fixed list of video ids
 * that would quietly go dead. PLACES instead gives, for each area, the
 * cities to look in; searchUrl() builds a YouTube search filtered to what is
 * live right now, so it always finds what is actually on air.
 *
 * Exposed as: window.LiveCams (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var SAVE_VERSION = 1;
  var MAX_STREAMS = 48;
  var MAX_NAME = 60;

  // YouTube's "Live" search filter (the sp= value its own Filters menu sets).
  var LIVE_FILTER = 'EgJAAQ%3D%3D';

  var PLACES = [
    { area: 'Ukraine', cities: ['Kyiv', 'Lviv', 'Odesa', 'Kharkiv', 'Dnipro', 'Zaporizhzhia', 'Mykolaiv'] },
    { area: 'Russia', cities: ['Moscow', 'Belgorod', 'Kursk', 'Sevastopol'] },
    { area: 'Israel', cities: ['Jerusalem', 'Tel Aviv', 'Haifa', 'Ashkelon'] },
    { area: 'Palestine', cities: ['Gaza', 'Ramallah', 'Bethlehem'] },
    { area: 'Lebanon', cities: ['Beirut', 'Tyre'] },
    { area: 'Syria', cities: ['Damascus', 'Aleppo'] },
    { area: 'Iran', cities: ['Tehran', 'Isfahan'] }
  ];

  var AREAS = PLACES.map(function (p) { return p.area; }).concat(['Other']);

  var VIDEO_ID = /^[A-Za-z0-9_-]{11}$/;
  var CHANNEL_ID = /^UC[A-Za-z0-9_-]{22}$/;

  function searchUrl(city) {
    var q = encodeURIComponent(String(city) + ' live cam').replace(/%20/g, '+');
    return 'https://www.youtube.com/results?search_query=' + q + '&sp=' + LIVE_FILTER;
  }

  function toUrl(text) {
    var s = String(text == null ? '' : text).trim();
    if (!s) return null;
    if (!/^[a-z][a-z0-9+.-]*:\/\//i.test(s)) s = 'https://' + s;
    try { return new URL(s); } catch (e) { return null; }
  }

  /*
   * Reads a pasted link. Returns one of
   *   { kind: 'video',   id }   a YouTube video or live stream
   *   { kind: 'channel', id }   a YouTube channel id — embeds whatever it has live
   *   { kind: 'web',     url }  any other https page: opened, not embedded
   *   null                      not a usable link
   * A bare 11-character video id or UC… channel id is accepted too.
   */
  function parseLink(text) {
    var raw = String(text == null ? '' : text).trim();
    if (VIDEO_ID.test(raw)) return { kind: 'video', id: raw };
    if (CHANNEL_ID.test(raw)) return { kind: 'channel', id: raw };

    var u = toUrl(raw);
    if (!u || (u.protocol !== 'https:' && u.protocol !== 'http:')) return null;
    if (u.hostname.indexOf('.') < 0) return null;

    var host = u.hostname.toLowerCase().replace(/^(www|m|music)\./, '');
    var parts = u.pathname.split('/').filter(Boolean);

    if (host === 'youtu.be') {
      return parts[0] && VIDEO_ID.test(parts[0]) ? { kind: 'video', id: parts[0] } : null;
    }
    if (host === 'youtube.com' || host === 'youtube-nocookie.com') {
      var v = u.searchParams.get('v');
      if (parts[0] === 'watch' && v && VIDEO_ID.test(v)) return { kind: 'video', id: v };
      // "live_stream" is itself 11 characters, so it is checked before ids.
      if (parts[0] === 'embed' && parts[1] === 'live_stream') {
        var ch = u.searchParams.get('channel');
        return ch && CHANNEL_ID.test(ch) ? { kind: 'channel', id: ch } : null;
      }
      if (['live', 'embed', 'shorts', 'v'].indexOf(parts[0]) >= 0 && parts[1] && VIDEO_ID.test(parts[1])) {
        return { kind: 'video', id: parts[1] };
      }
      if (parts[0] === 'channel' && parts[1] && CHANNEL_ID.test(parts[1])) {
        return { kind: 'channel', id: parts[1] };
      }
      // @handles and searches cannot be embedded without YouTube's API.
      return null;
    }
    if (u.protocol !== 'https:') return null;
    return { kind: 'web', url: u.href };
  }

  function embedUrl(stream) {
    var q = '?autoplay=1&mute=1&playsinline=1&rel=0';
    if (stream.kind === 'video') return 'https://www.youtube-nocookie.com/embed/' + stream.id + q;
    if (stream.kind === 'channel') {
      return 'https://www.youtube-nocookie.com/embed/live_stream' + q + '&channel=' + stream.id;
    }
    return null;
  }

  function watchUrl(stream) {
    if (stream.kind === 'video') return 'https://www.youtube.com/watch?v=' + stream.id;
    if (stream.kind === 'channel') return 'https://www.youtube.com/channel/' + stream.id + '/live';
    return stream.url;
  }

  function keyOf(stream) {
    return stream.kind === 'web' ? 'web:' + stream.url : stream.kind + ':' + stream.id;
  }

  function cleanName(name) {
    return String(name == null ? '' : name).replace(/\s+/g, ' ').trim().slice(0, MAX_NAME);
  }

  function cleanArea(area) {
    return AREAS.indexOf(area) >= 0 ? area : 'Other';
  }

  // Builds a stream from what the viewer typed, or returns { error }.
  function makeStream(link, name, area) {
    var parsed = parseLink(link);
    if (!parsed) {
      return { error: "That doesn't look like a link to a livestream. Paste a YouTube video, live or channel link." };
    }
    parsed.name = cleanName(name) || (parsed.kind === 'web' ? hostOf(parsed.url) : 'Live stream');
    parsed.area = cleanArea(area);
    return parsed;
  }

  function hostOf(url) {
    try { return new URL(url).hostname.replace(/^www\./, ''); } catch (e) { return 'Web page'; }
  }

  // Adds a stream to the front of the list. Returns { list, error? }.
  function addStream(list, stream) {
    var key = keyOf(stream);
    for (var i = 0; i < list.length; i++) {
      if (keyOf(list[i]) === key) return { list: list, error: "That stream is already on your wall." };
    }
    if (list.length >= MAX_STREAMS) {
      return { list: list, error: 'Your wall is full (' + MAX_STREAMS + ' streams). Remove one first.' };
    }
    return { list: [stream].concat(list) };
  }

  function removeStream(list, key) {
    return list.filter(function (s) { return keyOf(s) !== key; });
  }

  function filterByArea(list, area) {
    if (!area || area === 'All') return list.slice();
    return list.filter(function (s) { return s.area === area; });
  }

  function serialize(list) {
    return JSON.stringify({
      version: SAVE_VERSION,
      streams: list.map(function (s) {
        var o = { kind: s.kind, name: s.name, area: s.area };
        if (s.kind === 'web') o.url = s.url; else o.id = s.id;
        return o;
      })
    });
  }

  // Reads a save back. Anything unreadable or tampered with is dropped, so a
  // broken save gives an empty wall rather than a broken page.
  function deserialize(text) {
    var data;
    try { data = JSON.parse(text); } catch (e) { return []; }
    if (!data || data.version !== SAVE_VERSION || !Array.isArray(data.streams)) return [];
    var out = [];
    data.streams.forEach(function (s) {
      if (!s || typeof s !== 'object') return;
      var parsed = null;
      if (s.kind === 'video' && VIDEO_ID.test(s.id)) parsed = { kind: 'video', id: s.id };
      else if (s.kind === 'channel' && CHANNEL_ID.test(s.id)) parsed = { kind: 'channel', id: s.id };
      else if (s.kind === 'web') {
        var p = parseLink(s.url);
        if (p && p.kind === 'web') parsed = p;
      }
      if (!parsed) return;
      parsed.name = cleanName(s.name) || 'Live stream';
      parsed.area = cleanArea(s.area);
      out = addStream(out, parsed).list;
    });
    return out.reverse();
  }

  var api = {
    PLACES: PLACES,
    AREAS: AREAS,
    MAX_STREAMS: MAX_STREAMS,
    searchUrl: searchUrl,
    parseLink: parseLink,
    embedUrl: embedUrl,
    watchUrl: watchUrl,
    keyOf: keyOf,
    makeStream: makeStream,
    addStream: addStream,
    removeStream: removeStream,
    filterByArea: filterByArea,
    serialize: serialize,
    deserialize: deserialize
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.LiveCams = api;
})(typeof window !== 'undefined' ? window : this);
