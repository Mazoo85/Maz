/*
 * GOD'S EYE VIEW — planes.
 *
 * Live aircraft within 250 nautical miles of the point under the camera,
 * every 15 seconds: through the planes relay when one is configured (every
 * free flight service refuses web pages); otherwise adsb.lol first, OpenSky
 * only when adsb.lol fails (its anonymous allowance is small). Between fetches every airborne plane is
 * flown along its track at its speed from where it was last seen, so planes
 * glide instead of jumping. At most 300, nearest the centre, for phone speed.
 * Follows the layer contract (step 1 plan) with the rotation / followsView
 * additions in docs/superpowers/plans/2026-10-21-godview-step4-planes.md.
 */
(function (global) {
  'use strict';

  const EARTH_RADIUS_KM = 6371;
  const CAP = 300;
  const COLOR = '#ffb300';
  const GROUND_COLOR = '#8a7a40';
  const COMPASS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];

  function create(opts) {
    const PL = global.GV_PLANES;
    const P = global.GV_POLLER;
    const F = global.GV_FORMAT;
    const now = opts.now || Date.now;
    const listeners = [];
    let markers = [];
    let fetchedAt = 0;

    const layer = {
      id: 'planes',
      label: 'PLANES',
      icon: '✈',
      status: 'idle',
      count: 0,
      points: [],
      rings: [],
      html: [],
      credit: '',
      followsView: true,
      start() { poller.start(); },
      stop() {
        poller.stop();
        markers = [];
        layer.credit = '';
        place();
      },
      refresh() { poller.refresh(); },
      tick() { place(); },
      onChange(fn) { listeners.push(fn); },
      describe: describe
    };

    function emit() {
      for (const fn of listeners) fn(layer);
    }

    /* Fly every marker on from where its plane was last seen. */
    function place() {
      const seconds = Math.max(0, (now() - fetchedAt) / 1000);
      for (const m of markers) {
        const at = PL.advance(m.item, seconds);
        m.lat = at.lat;
        m.lng = at.lng;
      }
      layer.html = markers;
      layer.count = markers.length;
      emit();
    }

    function show(result) {
      fetchedAt = now();
      const centre = opts.getView();
      markers = PL.nearest(result.planes, centre, CAP).map((p) => ({
        layer: 'planes',
        item: p,
        lat: p.lat,
        lng: p.lng,
        altitude: (p.altFt || 0) * 0.3048 / 1000 / EARTH_RADIUS_KM,
        color: p.onGround ? GROUND_COLOR : COLOR,
        label: null,
        rotation: p.track,
        glyph: '✈'
      }));
      layer.credit = result.credit;
      place();
    }

    /* Every free flight service refuses web pages, so with a relay configured
     * (godview/relay/) only the relay is asked; asking the services directly
     * as well would only add refused requests. */
    const relay = opts.relayUrl ? String(opts.relayUrl).replace(/\/+$/, '') : '';

    async function fetchAround() {
      const v = opts.getView();
      if (relay) {
        const json = await opts.fetchJson(relay + '/planes?lat=' + v.lat.toFixed(2) + '&lng=' + v.lng.toFixed(2));
        return { planes: PL.parseAdsb(json), credit: 'Planes: ' + (json.source || 'relay') + ' (via relay)' };
      }
      try {
        return { planes: PL.parseAdsb(await opts.fetchJson(PL.adsbUrl(v.lat, v.lng))), credit: 'Planes: adsb.lol (ODbL)' };
      } catch (e) {
        return { planes: PL.parseOpensky(await opts.fetchJson(PL.openskyUrl(v.lat, v.lng))), credit: 'Planes: OpenSky Network' };
      }
    }

    function describe(p) {
      const heading = Math.round(p.track) % 360;
      return {
        title: p.callsign || p.reg || p.hex.toUpperCase(),
        rows: [
          ['ALTITUDE', p.onGround ? 'ON THE GROUND' : p.altFt === null ? 'unknown' :
            F.count(p.altFt) + ' ft / ' + F.count(p.altFt * 0.3048) + ' m'],
          ['SPEED', F.count(p.speedKt) + ' kt / ' + F.count(p.speedKt * 1.150779) + ' mph'],
          ['HEADING', String(heading).padStart(3, '0') + '° ' + COMPASS[Math.round(heading / 45) % 8]],
          ['AIRCRAFT', [p.type, p.reg].filter(Boolean).join(' · ') || 'unknown']
        ],
        link: 'https://globe.adsb.lol/?icao=' + encodeURIComponent(p.hex)
      };
    }

    const poller = P.createPoller({
      intervalMs: 15000,
      /* The flight services, even through the relay, refuse now and then
       * ("too many requests" on Cloudflare's shared addresses), so a refusal
       * is retried within seconds, the planes stay on screen meanwhile, and
       * only three in a row show OFFLINE. */
      retryMs: 4000,
      offlineAfter: 3,
      maxBackoffMs: 120000,
      setTimer: opts.setTimer,
      clearTimer: opts.clearTimer,
      fetchOnce: fetchAround,
      onData: show,
      onStatus: (s) => { layer.status = s; emit(); }
    });

    return layer;
  }

  const API = { create: create };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_PLANES_LAYER = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
