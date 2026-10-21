/*
 * GOD'S EYE VIEW — public road cameras.
 *
 * Three open-data camera networks, each with its own list format: London
 * (TfL JamCams), Finland (Fintraffic weather cameras) and California
 * (Caltrans). Every list becomes the same plain Camera. Only the lists need a
 * source to allow web pages; the pictures are ordinary <img> tags. Cameras
 * marked out of service, without a place, or without an https picture are
 * dropped.
 */
(function (global) {
  'use strict';

  const CALTRANS_DISTRICTS = [4, 7, 11, 3]; // SF Bay, Los Angeles, San Diego, Sacramento

  const SOURCES = [
    {
      id: 'tfl',
      label: 'TfL JamCams (London)',
      credit: 'Cameras: TfL Open Data',
      urls: ['https://api.tfl.gov.uk/Place/Type/JamCam']
    },
    {
      id: 'digitraffic',
      label: 'Fintraffic (Finland)',
      credit: 'Cameras: Fintraffic / digitraffic.fi, CC BY 4.0',
      urls: ['https://tie.digitraffic.fi/api/weathercam/v1/stations']
    },
    {
      id: 'caltrans',
      label: 'Caltrans (California)',
      credit: 'Cameras: Caltrans',
      urls: CALTRANS_DISTRICTS.map((d) =>
        'https://cwwp2.dot.ca.gov/data/d' + d + '/cctv/cctvStatusD' + String(d).padStart(2, '0') + '.json')
    }
  ];

  function num(v) {
    return v === null || v === undefined || v === '' ? NaN : Number(v);
  }

  function camera(id, name, lat, lng, image, source) {
    if (!isFinite(lat) || !isFinite(lng) || Math.abs(lat) > 90 || Math.abs(lng) > 180) return null;
    if (typeof image !== 'string' || !/^https:\/\//.test(image)) return null;
    return { id: id, name: String(name || 'Camera').trim(), lat: lat, lng: lng, image: image, source: source };
  }

  function parseTfl(json) {
    if (!Array.isArray(json)) throw new Error('not a TfL camera list');
    const out = [];
    for (const p of json) {
      if (!p || !Array.isArray(p.additionalProperties)) continue;
      const prop = (key) => {
        const hit = p.additionalProperties.find((a) => a && a.key === key);
        return hit ? hit.value : undefined;
      };
      if (prop('available') !== 'true') continue;
      const c = camera('tfl:' + String(p.id).replace(/^JamCams_/, ''), p.commonName,
        num(p.lat), num(p.lon), prop('imageUrl'), 'tfl');
      if (c) out.push(c);
    }
    return out;
  }

  function parseDigitraffic(json) {
    if (!json || !Array.isArray(json.features)) throw new Error('not a Digitraffic station list');
    const out = [];
    for (const f of json.features) {
      const p = f && f.properties;
      const xy = f && f.geometry && f.geometry.coordinates;
      if (!p || !Array.isArray(xy) || p.collectionStatus !== 'GATHERING') continue;
      const preset = (p.presets || []).find((x) => x && x.inCollection && /^[A-Z0-9]+$/.test(x.id));
      if (!preset) continue;
      const c = camera('fi:' + p.id, String(p.name || '').replace(/_/g, ' '), num(xy[1]), num(xy[0]),
        'https://weathercam.digitraffic.fi/' + preset.id + '.jpg', 'digitraffic');
      if (c) out.push(c);
    }
    return out;
  }

  function parseCaltrans(json) {
    if (!json || !Array.isArray(json.data)) throw new Error('not a Caltrans camera list');
    const out = [];
    for (const row of json.data) {
      const c = row && row.cctv;
      if (!c || c.inService !== 'true' || !c.location) continue;
      const still = c.imageData && c.imageData.static;
      const name = String(c.location.locationName || '').replace(/^TV\d+\s*--\s*/, '');
      const cam = camera('ca:' + c.location.district + ':' + c.index, name,
        num(c.location.latitude), num(c.location.longitude), still && still.currentImageURL, 'caltrans');
      if (cam) out.push(cam);
    }
    return out;
  }

  const PARSERS = { tfl: parseTfl, digitraffic: parseDigitraffic, caltrans: parseCaltrans };

  const API = {
    SOURCES: SOURCES,
    PARSERS: PARSERS,
    parseTfl: parseTfl,
    parseDigitraffic: parseDigitraffic,
    parseCaltrans: parseCaltrans
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.GV_CAMERAS = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
