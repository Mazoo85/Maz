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

  const GM_DRUM = {
    kick: 36, snare: 38, clap: 39, hh: 42, oh: 46,
    tom: 45, crash: 49, perc: 76, shaker: 82, rim: 37
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
   * Download helper
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

  global.Exporter = {
    encodeWav: encodeWav,
    buildMidi: buildMidi,
    download: download,
    safeName: safeName
  };
})(window);
