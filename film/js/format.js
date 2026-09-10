/*
 * SCRIPT FORGE — the exporters.
 * -----------------------------
 * One script, four ways out of the app:
 *
 *   .fountain  plain-text screenplay markup that Final Draft, Highland,
 *              Slugline, Fade In, WriterDuet and Celtx all import
 *   .txt       the same script laid out in the standard Courier columns,
 *              ready to print
 *   .fdx       Final Draft's own XML format
 *   .md        a shot list — every scene, its job, and the shots to cover it
 *
 * Exposed as window.FilmFormat (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* Column layout of a real screenplay page, in characters at 12pt Courier. */
  var COLS = {
    action: { indent: 0, width: 60 },
    scene_heading: { indent: 0, width: 60 },
    character: { indent: 22, width: 38 },
    parenthetical: { indent: 16, width: 26 },
    dialogue: { indent: 10, width: 35 },
    transition: { indent: 45, width: 15 }
  };

  function wrap(text, width) {
    var words = String(text).split(/\s+/);
    var lines = [];
    var line = '';
    words.forEach(function (w) {
      if (!line.length) line = w;
      else if ((line + ' ' + w).length <= width) line += ' ' + w;
      else { lines.push(line); line = w; }
    });
    if (line.length) lines.push(line);
    return lines.length ? lines : [''];
  }

  function pad(n) {
    return new Array(n + 1).join(' ');
  }

  function today() {
    var d = new Date();
    function two(n) { return (n < 10 ? '0' : '') + n; }
    return d.getFullYear() + '-' + two(d.getMonth() + 1) + '-' + two(d.getDate());
  }

  /* --------------------------------------------------------------- fountain */
  function toFountain(script) {
    var out = [];
    out.push('Title: **' + script.title + '**');
    out.push('Credit: written with');
    out.push('Author: SCRIPT FORGE');
    out.push('Draft date: ' + today());
    out.push('');
    out.push('= ' + script.logline);
    out.push('');
    if (script.idea) {
      out.push('[[ from the idea: ' + script.idea + ' ]]');
      out.push('');
    }

    script.elements.forEach(function (el) {
      switch (el.type) {
        case 'scene_heading':
          out.push('', el.text.toUpperCase());
          break;
        case 'action':
          out.push('', el.text);
          break;
        case 'character':
          out.push('', '@' + el.text.toUpperCase()); // @ forces a character cue
          break;
        case 'parenthetical':
          out.push(el.text);
          break;
        case 'dialogue':
          out.push(el.text);
          break;
        case 'transition':
          out.push('', '> ' + el.text);
          break;
        default:
          out.push('', el.text);
      }
    });
    out.push('', '> THE END <');
    return out.join('\n').replace(/\n{3,}/g, '\n\n') + '\n';
  }

  /* ------------------------------------------------------------ plain text */
  function toText(script) {
    var out = [];
    out.push(pad(Math.max(0, Math.floor((60 - script.title.length) / 2))) + script.title.toUpperCase());
    out.push('');
    out.push(pad(Math.max(0, Math.floor((60 - 18) / 2))) + 'written with');
    out.push(pad(Math.max(0, Math.floor((60 - 12) / 2))) + 'SCRIPT FORGE');
    out.push('', '', wrap(script.logline, 60).join('\n'), '');
    out.push(script.genreLabel + ' · ' + script.lengthLabel + ' · ' + script.runtime +
      ' · draft ' + today());
    out.push('', pad(24) + '* * *', '');

    script.elements.forEach(function (el) {
      var col = COLS[el.type] || COLS.action;
      var text = el.type === 'scene_heading' || el.type === 'character' || el.type === 'transition'
        ? el.text.toUpperCase()
        : el.text;
      if (el.type === 'parenthetical' && text.charAt(0) !== '(') text = '(' + text + ')';
      wrap(text, col.width).forEach(function (line) { out.push(pad(col.indent) + line); });
      // Dialogue blocks run tight; everything else gets air around it.
      if (el.type !== 'character' && el.type !== 'parenthetical') out.push('');
    });
    out.push('', pad(27) + 'THE END', '');
    return out.join('\n');
  }

  /* ---------------------------------------------------------- final draft */
  function xmlEscape(s) {
    return String(s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  var FDX_TYPE = {
    scene_heading: 'Scene Heading',
    action: 'Action',
    character: 'Character',
    parenthetical: 'Parenthetical',
    dialogue: 'Dialogue',
    transition: 'Transition'
  };

  function toFdx(script) {
    var out = [];
    out.push('<?xml version="1.0" encoding="UTF-8" standalone="no"?>');
    out.push('<FinalDraft DocumentType="Script" Template="No" Version="1">');
    out.push('  <Content>');
    script.elements.forEach(function (el) {
      var type = FDX_TYPE[el.type] || 'Action';
      var text = el.type === 'parenthetical' && el.text.charAt(0) !== '(' ? '(' + el.text + ')' : el.text;
      if (el.type === 'scene_heading' || el.type === 'character' || el.type === 'transition') {
        text = text.toUpperCase();
      }
      out.push('    <Paragraph Type="' + type + '">');
      out.push('      <Text>' + xmlEscape(text) + '</Text>');
      out.push('    </Paragraph>');
    });
    out.push('  </Content>');
    out.push('</FinalDraft>');
    return out.join('\n') + '\n';
  }

  /* ------------------------------------------------------------ shot list */
  function toShotList(script) {
    var out = [];
    out.push('# ' + script.title + ' — shot list');
    out.push('');
    out.push('*' + script.logline + '*');
    out.push('');
    out.push('| | |');
    out.push('|---|---|');
    out.push('| Genre | ' + script.genreLabel + ' |');
    out.push('| Length | ' + script.lengthLabel + ' (' + script.runtime + ', ' + script.pages + ' pages) |');
    out.push('| Scenes | ' + script.scenes.length + ' |');
    out.push('| Cast | ' + script.characters.map(function (c) {
      return c.name + ' (' + c.role + ')';
    }).join(', ') + ' |');
    out.push('| Locations | ' + uniqueLocations(script).join(', ') + ' |');
    out.push('');
    out.push('## Shooting order is up to you — story order is below');
    out.push('');

    script.scenes.forEach(function (scene) {
      out.push('### ' + scene.number + '. ' + scene.heading.text);
      out.push('');
      out.push('**' + scene.beat.name + '** — ' + scene.beat.purpose);
      out.push('');
      scene.shots.forEach(function (shot, i) {
        out.push('- [ ] **' + scene.number + String.fromCharCode(65 + i) + '** — ' + shot);
      });
      out.push('');
    });

    out.push('---');
    out.push('');
    out.push('Generated by SCRIPT FORGE from the idea: *' + (script.idea || '(none given)') + '*');
    out.push('');
    return out.join('\n');
  }

  function uniqueLocations(script) {
    var seen = {};
    var out = [];
    script.scenes.forEach(function (s) {
      var key = s.heading.int + ' ' + s.heading.place.slug;
      if (!seen[key]) { seen[key] = true; out.push(key); }
    });
    return out;
  }

  /* Filename-safe slug for downloads: "THE RADIO" -> "the-radio". */
  function slugify(title) {
    return String(title).toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'script';
  }

  var API = {
    toFountain: toFountain,
    toText: toText,
    toFdx: toFdx,
    toShotList: toShotList,
    uniqueLocations: uniqueLocations,
    slugify: slugify,
    wrap: wrap
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmFormat = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
