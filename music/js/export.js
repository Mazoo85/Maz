/*
 * export.js — turn a rendered AudioBuffer into a .wav file, and a composed
 * song into a standard .mid file that opens in any DAW (GarageBand, Ableton,
 * FL Studio, Logic, Reaper, MuseScore ...).
 */
(function (global) {
  'use strict';

  /* ------------------------------------------------------------------ *
   * WAV
   * ------------------------------------------------------------------ */

  function encodeWav(buffer) {
    const channels = buffer.numberOfChannels;
    const frames = buffer.length;
    const rate = buffer.sampleRate;
    const bytesPerSample = 2;
    const blockAlign = channels * bytesPerSample;
    const dataSize = frames * blockAlign;
    const out = new ArrayBuffer(44 + dataSize);
    const view = new DataView(out);

    function str(offset, s) {
      for (let i = 0; i < s.length; i++) view.setUint8(offset + i, s.charCodeAt(i));
    }

    str(0, 'RIFF');
    view.setUint32(4, 36 + dataSize, true);
    str(8, 'WAVE');
    str(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);              // PCM
    view.setUint16(22, channels, true);
    view.setUint32(24, rate, true);
    view.setUint32(28, rate * blockAlign, true);
    view.setUint16(32, blockAlign, true);
    view.setUint16(34, 8 * bytesPerSample, true);
    str(36, 'data');
    view.setUint32(40, dataSize, true);

    const data = [];
    for (let c = 0; c < channels; c++) data.push(buffer.getChannelData(c));

    let offset = 44;
    for (let i = 0; i < frames; i++) {
      for (let c = 0; c < channels; c++) {
        let s = data[c][i];
        if (s > 1) s = 1; else if (s < -1) s = -1;
        view.setInt16(offset, s < 0 ? s * 0x8000 : s * 0x7fff, true);
        offset += 2;
      }
    }
    return new Blob([out], { type: 'audio/wav' });
  }

  /* ------------------------------------------------------------------ *
   * MIDI
   * ------------------------------------------------------------------ */

  const PPQ = 480;

  /* General MIDI has no riser or impact; the nearest cymbals keep the shape of
     the arrangement legible when the file is opened elsewhere. */
  const GM_DRUM = {
    kick: 36, snare: 38, clap: 39, hh: 42, oh: 46,
    tom: 45, crash: 49, perc: 76, shaker: 82, rim: 37,
    riser: 52, impact: 55, ride: 51, tamb: 54, cowbell: 56, conga: 64
  };

  /* General MIDI program numbers (0-based) chosen to resemble each part. */
  const PROGRAMS = {
    lofi:      { bass: 33, chords: 4,  arp: 11, lead: 4,  pad: 89 },
    synthwave: { bass: 38, chords: 90, arp: 81, lead: 81, pad: 51 },
    house:     { bass: 38, chords: 4,  arp: 11, lead: 80, pad: 89 },
    ambient:   { bass: 32, chords: 89, arp: 98, lead: 98, pad: 91 },
    cinematic: { bass: 43, chords: 48, arp: 0,  lead: 0,  pad: 51 },
    chiptune:  { bass: 80, chords: 80, arp: 80, lead: 80, pad: 81 },
    dnb:       { bass: 39, chords: 89, arp: 81, lead: 98, pad: 91 },
    trap:      { bass: 38, chords: 89, arp: 9,  lead: 9,  pad: 91 }
  };

  function vlq(n) {
    const bytes = [n & 0x7f];
    n >>= 7;
    while (n > 0) {
      bytes.unshift((n & 0x7f) | 0x80);
      n >>= 7;
    }
    return bytes;
  }

  function pushStr(arr, s) {
    for (let i = 0; i < s.length; i++) arr.push(s.charCodeAt(i) & 0xff);
  }

  function chunk(id, bytes) {
    const out = [];
    pushStr(out, id);
    out.push((bytes.length >> 24) & 0xff, (bytes.length >> 16) & 0xff,
             (bytes.length >> 8) & 0xff, bytes.length & 0xff);
    return out.concat(bytes);
  }

  function trackChunk(events, name) {
    // events: [{tick, data:[...]}], already the raw MIDI messages
    const bytes = [];
    if (name) {
      bytes.push(0x00, 0xff, 0x03, name.length);
      pushStr(bytes, name);
    }
    events.sort(function (a, b) {
      if (a.tick !== b.tick) return a.tick - b.tick;
      return a.order - b.order;
    });
    let last = 0;
    for (let i = 0; i < events.length; i++) {
      const e = events[i];
      const delta = Math.max(0, Math.round(e.tick - last));
      last = e.tick;
      Array.prototype.push.apply(bytes, vlq(delta));
      Array.prototype.push.apply(bytes, e.data);
    }
    bytes.push(0x00, 0xff, 0x2f, 0x00);       // end of track
    return chunk('MTrk', bytes);
  }

  function buildMidi(song) {
    const progs = PROGRAMS[song.genreId] || PROGRAMS.synthwave;
    const tracks = [];

    // Track 0: tempo map
    const meta = [];
    const usPerQuarter = Math.round(60000000 / song.bpm);
    meta.push({ tick: 0, order: 0, data: [0xff, 0x51, 0x03,
      (usPerQuarter >> 16) & 0xff, (usPerQuarter >> 8) & 0xff, usPerQuarter & 0xff] });
    meta.push({ tick: 0, order: 1, data: [0xff, 0x58, 0x04, 4, 2, 24, 8] });
    tracks.push(trackChunk(meta, song.title));

    const melodic = ['bass', 'chords', 'arp', 'lead', 'pad'];
    let channel = 0;
    melodic.forEach(function (name) {
      const evs = song.tracks[name] || [];
      if (channel === 9) channel = 10;
      const ch = channel % 16;
      const list = [{ tick: 0, order: 0, data: [0xc0 | ch, progs[name] || 0] }];
      for (let i = 0; i < evs.length; i++) {
        const e = evs[i];
        const on = Math.round(e.t * PPQ);
        const off = Math.max(on + 10, Math.round((e.t + e.d) * PPQ));
        const vel = Math.max(1, Math.min(127, Math.round(e.v * 110)));
        const pitch = Math.max(0, Math.min(127, Math.round(e.p)));
        list.push({ tick: on, order: 2, data: [0x90 | ch, pitch, vel] });
        list.push({ tick: off, order: 1, data: [0x80 | ch, pitch, 64] });
      }
      tracks.push(trackChunk(list, name.charAt(0).toUpperCase() + name.slice(1)));
      channel++;
    });

    // Drums on channel 10 (index 9)
    const drumEvents = song.tracks.drums || [];
    const dlist = [];
    for (let i = 0; i < drumEvents.length; i++) {
      const e = drumEvents[i];
      const pitch = GM_DRUM[e.inst] || 39;
      const on = Math.round(e.t * PPQ);
      const vel = Math.max(1, Math.min(127, Math.round(e.v * 115)));
      dlist.push({ tick: on, order: 2, data: [0x99, pitch, vel] });
      dlist.push({ tick: on + Math.round(PPQ / 8), order: 1, data: [0x89, pitch, 64] });
    }
    tracks.push(trackChunk(dlist, 'Drums'));

    const header = chunk('MThd', [0, 1, (tracks.length >> 8) & 0xff, tracks.length & 0xff,
      (PPQ >> 8) & 0xff, PPQ & 0xff]);

    let all = header;
    for (let i = 0; i < tracks.length; i++) all = all.concat(tracks[i]);
    return new Blob([new Uint8Array(all)], { type: 'audio/midi' });
  }

  /* ------------------------------------------------------------------ *
   * ZIP (store only — no compression)
   *
   * Audio and MIDI are not on the hosted viewer's download allowlist, but zip
   * is, so when the page cannot hand over a file directly the track travels in
   * a zip instead. Stored, not deflated: a WAV barely compresses anyway, and
   * this keeps the writer to a page.
   * ------------------------------------------------------------------ */

  let CRC_TABLE = null;
  function crcTable() {
    if (CRC_TABLE) return CRC_TABLE;
    CRC_TABLE = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      CRC_TABLE[n] = c >>> 0;
    }
    return CRC_TABLE;
  }

  function crc32(bytes) {
    const t = crcTable();
    let c = 0xffffffff;
    for (let i = 0; i < bytes.length; i++) c = t[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }

  /** entries: [{ name, bytes: Uint8Array }] */
  function makeZip(entries) {
    const enc = new TextEncoder();
    const parts = [];
    const central = [];
    let offset = 0;

    // A fixed timestamp keeps the same song byte-identical between exports.
    const dosTime = 0x6000;              // 12:00:00
    const dosDate = ((2026 - 1980) << 9) | (1 << 5) | 1;

    entries.forEach(function (e) {
      const name = enc.encode(e.name);
      const crc = crc32(e.bytes);
      const local = new DataView(new ArrayBuffer(30));
      local.setUint32(0, 0x04034b50, true);
      local.setUint16(4, 20, true);
      local.setUint16(6, 0, true);
      local.setUint16(8, 0, true);        // stored
      local.setUint16(10, dosTime, true);
      local.setUint16(12, dosDate, true);
      local.setUint32(14, crc, true);
      local.setUint32(18, e.bytes.length, true);
      local.setUint32(22, e.bytes.length, true);
      local.setUint16(26, name.length, true);
      local.setUint16(28, 0, true);
      parts.push(new Uint8Array(local.buffer), name, e.bytes);

      const cd = new DataView(new ArrayBuffer(46));
      cd.setUint32(0, 0x02014b50, true);
      cd.setUint16(4, 20, true);
      cd.setUint16(6, 20, true);
      cd.setUint16(8, 0, true);
      cd.setUint16(10, 0, true);
      cd.setUint16(12, dosTime, true);
      cd.setUint16(14, dosDate, true);
      cd.setUint32(16, crc, true);
      cd.setUint32(20, e.bytes.length, true);
      cd.setUint32(24, e.bytes.length, true);
      cd.setUint16(28, name.length, true);
      cd.setUint32(42, offset, true);
      central.push(new Uint8Array(cd.buffer), name);

      offset += 30 + name.length + e.bytes.length;
    });

    let cdSize = 0;
    central.forEach(function (p) { cdSize += p.length; });

    const end = new DataView(new ArrayBuffer(22));
    end.setUint32(0, 0x06054b50, true);
    end.setUint16(8, entries.length, true);
    end.setUint16(10, entries.length, true);
    end.setUint32(12, cdSize, true);
    end.setUint32(16, offset, true);

    return new Blob(parts.concat(central, [new Uint8Array(end.buffer)]), { type: 'application/zip' });
  }

  /* ------------------------------------------------------------------ *
   * Handing a file to the person using the app
   * ------------------------------------------------------------------ */

  function safeName(s) {
    return String(s).replace(/[^a-z0-9\- ]/gi, '').replace(/\s+/g, '-').toLowerCase() || 'song';
  }

  function download(blob, filename) {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    setTimeout(function () {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 1000);
  }

  /** Resolves the hosted viewer's download channel, or null when running as a plain page. */
  let saverPromise = null;
  function saver() {
    if (saverPromise) return saverPromise;
    saverPromise = (global.claude && typeof global.claude.use === 'function')
      ? global.claude.use('downloads').catch(function () { return null; })
      : Promise.resolve(null);
    return saverPromise;
  }

  /**
   * Give the viewer a file. Opened as an ordinary page this is a normal
   * download; inside the hosted viewer the page may not start one itself, so it
   * offers the file through the host and the viewer confirms it.
   * Resolves 'saved', 'declined' or 'unavailable'.
   */
  function deliver(blob, filename) {
    return saver().then(function (downloads) {
      if (!downloads) { download(blob, filename); return 'saved'; }
      return blob.arrayBuffer().then(function (buf) {
        const zip = makeZip([{ name: filename, bytes: new Uint8Array(buf) }]);
        return downloads.save({
          filename: filename.replace(/\.[^.]+$/, '') + '.zip',
          data: zip
        }).then(function () { return 'saved'; });
      }).catch(function (err) {
        const code = err && err.code;
        if (code === 'declined') return 'declined';
        if (code === 'rate_limited') return 'busy';
        return 'unavailable';
      });
    });
  }

  /**
   * Hand over several files at once, always as a zip — a folder of stems is a
   * folder either way, and one prompt beats six.
   */
  function deliverMany(files, zipName) {
    const reads = files.map(function (f) {
      return f.blob.arrayBuffer().then(function (buf) {
        return { name: f.name, bytes: new Uint8Array(buf) };
      });
    });
    return Promise.all(reads).then(function (entries) {
      const zip = makeZip(entries);
      return saver().then(function (downloads) {
        if (!downloads) { download(zip, zipName); return 'saved'; }
        return downloads.save({ filename: zipName, data: zip })
          .then(function () { return 'saved'; })
          .catch(function (err) {
            const code = err && err.code;
            if (code === 'declined') return 'declined';
            if (code === 'rate_limited') return 'busy';
            return 'unavailable';
          });
      });
    });
  }

  /** True once we know the page must route downloads through the host. */
  function hostedSave() {
    return saver().then(function (d) { return !!d; });
  }

  global.Exporter = {
    encodeWav: encodeWav,
    buildMidi: buildMidi,
    makeZip: makeZip,
    download: download,
    deliver: deliver,
    deliverMany: deliverMany,
    hostedSave: hostedSave,
    safeName: safeName
  };
})(window);
