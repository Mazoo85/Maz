/*
 * SCRIPT FORGE — the file fixer.
 * ------------------------------
 * MediaRecorder writes a WebM with no duration in it: the recorder is still
 * shooting when it writes the header, so it does not yet know how long the
 * film is. Players cope by reporting "Infinity", which means no timeline, no
 * reliable seeking, and a length of "unknown" wherever you share it.
 *
 * This walks the container, inserts a Duration into the Info block and hands
 * back a fixed file. If anything about the file is unfamiliar it returns the
 * original untouched — a film with an odd header still plays, and losing the
 * take would be the worse outcome by far.
 *
 * Exposed as window.FilmWebm (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var ID_SEGMENT = 0x18538067;
  var ID_INFO = 0x1549a966;
  var ID_DURATION = 0x4489;
  var ID_TIMECODE_SCALE = 0x2ad7b1;

  /* ------------------------------------------------------------------ EBML
   * Element ids and sizes are variable-length integers: the first set bit says
   * how many bytes long the number is. Ids keep that marker bit, sizes drop it.
   */
  function readVint(bytes, at, keepMarker) {
    if (at >= bytes.length) return null;
    var first = bytes[at];
    if (first === 0) return null;             // 8+ byte vint: not something we write
    var length = 1;
    var mask = 0x80;
    while (!(first & mask)) {
      mask >>= 1;
      length++;
      if (length > 8) return null;
    }
    var value = keepMarker ? first : (first & (mask - 1));
    var allOnes = keepMarker ? false : (first & (mask - 1)) === mask - 1;
    for (var i = 1; i < length; i++) {
      if (at + i >= bytes.length) return null;
      value = value * 256 + bytes[at + i];
      if (bytes[at + i] !== 0xff) allOnes = false;
    }
    return { value: value, length: length, unknown: !keepMarker && allOnes };
  }

  /* Smallest vint that can hold `value` as an element size. */
  function writeVint(value) {
    for (var length = 1; length <= 8; length++) {
      var max = Math.pow(2, 7 * length) - 1;
      if (value < max) {
        var out = new Uint8Array(length);
        var v = value;
        for (var i = length - 1; i >= 0; i--) {
          out[i] = v & 0xff;
          v = Math.floor(v / 256);
        }
        out[0] |= 1 << (8 - length);
        return out;
      }
    }
    return null;
  }

  /* Walk one level of children, calling back with each element found. */
  function eachChild(bytes, start, end, visit) {
    var at = start;
    while (at < end && at < bytes.length) {
      var id = readVint(bytes, at, true);
      if (!id) return;
      var size = readVint(bytes, at + id.length, false);
      if (!size) return;
      var headerLength = id.length + size.length;
      var contentAt = at + headerLength;
      var contentEnd = size.unknown ? end : contentAt + size.value;

      var stop = visit({
        id: id.value,
        at: at,
        headerLength: headerLength,
        sizeAt: at + id.length,
        sizeLength: size.length,
        size: size.value,
        unknown: size.unknown,
        contentAt: contentAt,
        contentEnd: contentEnd
      });
      if (stop) return;
      if (size.unknown) return;               // cannot skip past an open element
      at = contentEnd;
    }
  }

  function findInfo(bytes) {
    var segment = null;
    eachChild(bytes, 0, bytes.length, function (el) {
      if (el.id === ID_SEGMENT) { segment = el; return true; }
      return false;
    });
    if (!segment) return null;

    var info = null;
    eachChild(bytes, segment.contentAt, Math.min(segment.contentEnd, bytes.length), function (el) {
      if (el.id === ID_INFO) { info = el; return true; }
      return false;
    });
    return info;
  }

  /* A Duration is a float, in units of TimecodeScale (nanoseconds, normally
   * a million — i.e. milliseconds). */
  function timecodeScale(bytes, info) {
    var scale = 1000000;
    eachChild(bytes, info.contentAt, info.contentEnd, function (el) {
      if (el.id !== ID_TIMECODE_SCALE) return false;
      var v = 0;
      for (var i = el.contentAt; i < el.contentAt + el.size; i++) v = v * 256 + bytes[i];
      if (v > 0) scale = v;
      return true;
    });
    return scale;
  }

  function durationElement(ms) {
    var body = new Uint8Array(8);
    new DataView(body.buffer).setFloat64(0, ms, false);
    var out = new Uint8Array(2 + 1 + 8);
    out[0] = 0x44; out[1] = 0x89;             // Duration id
    out[2] = 0x88;                            // 8-byte payload
    out.set(body, 3);
    return out;
  }

  /* Returns a new Uint8Array with the duration written in, or the original
   * array if this file is not one we recognise. */
  function patch(bytes, seconds) {
    if (!(bytes instanceof Uint8Array) || !(seconds > 0)) return bytes;
    var info = findInfo(bytes);
    if (!info || info.unknown) return bytes;

    var scaled = (seconds * 1e9) / timecodeScale(bytes, info);

    // Already has one? Overwrite it where it lies — no resizing needed.
    var existing = null;
    eachChild(bytes, info.contentAt, info.contentEnd, function (el) {
      if (el.id === ID_DURATION) { existing = el; return true; }
      return false;
    });
    if (existing) {
      if (existing.size !== 4 && existing.size !== 8) return bytes;
      var copy = bytes.slice();
      var view = new DataView(copy.buffer, copy.byteOffset, copy.byteLength);
      if (existing.size === 8) view.setFloat64(existing.contentAt, scaled, false);
      else view.setFloat32(existing.contentAt, scaled, false);
      return copy;
    }

    // Otherwise splice one in at the end of Info and grow Info's size field.
    var element = durationElement(scaled);
    var newSize = writeVint(info.size + element.length);
    if (!newSize) return bytes;

    var out = new Uint8Array(bytes.length + element.length + (newSize.length - info.sizeLength));
    var cursor = 0;
    out.set(bytes.subarray(0, info.sizeAt), cursor);          // everything up to Info's size
    cursor += info.sizeAt;
    out.set(newSize, cursor);                                  // Info's new size
    cursor += newSize.length;
    out.set(bytes.subarray(info.contentAt, info.contentEnd), cursor); // Info's old content
    cursor += info.contentEnd - info.contentAt;
    out.set(element, cursor);                                  // the Duration
    cursor += element.length;
    out.set(bytes.subarray(info.contentEnd), cursor);          // the rest of the file
    return out;
  }

  /* The browser-facing wrapper: Blob in, fixed Blob out. Never rejects — a
   * film that could not be patched is still a film. */
  function withDuration(blob, seconds) {
    if (!blob || !blob.arrayBuffer || String(blob.type).indexOf('webm') === -1) {
      return Promise.resolve(blob);
    }
    return blob.arrayBuffer().then(function (buffer) {
      try {
        var bytes = new Uint8Array(buffer);
        var patched = patch(bytes, seconds);
        return patched === bytes ? blob : new Blob([patched], { type: blob.type });
      } catch (e) {
        return blob;
      }
    }, function () {
      return blob;
    });
  }

  /* Builds the smallest possible valid WebM head — used by the tests, so the
   * patcher can be checked without a 20 MB file in the repository. */
  function fixture(withExistingDuration) {
    var info = [0x2a, 0xd7, 0xb1, 0x83, 0x0f, 0x42, 0x40];    // TimecodeScale = 1000000
    if (withExistingDuration) {
      var d = durationElement(1234);
      for (var i = 0; i < d.length; i++) info.push(d[i]);
    }
    var head = [0x1a, 0x45, 0xdf, 0xa3, 0x84, 0x42, 0x86, 0x81, 0x01]; // EBML header
    var segment = [0x18, 0x53, 0x80, 0x67, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    var infoHeader = [0x15, 0x49, 0xa9, 0x66];
    var sizeByte = writeVint(info.length);
    var cluster = [0x1f, 0x43, 0xb6, 0x75, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0x81, 0x00];
    var bytes = head.concat(segment, infoHeader, Array.prototype.slice.call(sizeByte), info, cluster);
    return new Uint8Array(bytes);
  }

  /* Reads a duration back out, so a test can prove the patch took. */
  function readDuration(bytes) {
    var info = findInfo(bytes);
    if (!info) return null;
    var seconds = null;
    var scale = timecodeScale(bytes, info);
    eachChild(bytes, info.contentAt, info.contentEnd, function (el) {
      if (el.id !== ID_DURATION) return false;
      var view = new DataView(bytes.buffer, bytes.byteOffset + el.contentAt, el.size);
      var scaled = el.size === 8 ? view.getFloat64(0, false) : view.getFloat32(0, false);
      seconds = (scaled * scale) / 1e9;
      return true;
    });
    return seconds;
  }

  var API = {
    withDuration: withDuration,
    patch: patch,
    readDuration: readDuration,
    fixture: fixture,
    readVint: readVint,
    writeVint: writeVint
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWebm = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
