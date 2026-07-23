/*
 * MadLibs Story Forge — storyboard renderer
 * -----------------------------------------
 * A storyboard is a list of shot panels. Panels are authored as one pipe-
 * delimited line each, so they're easy to paste and edit inside the app:
 *
 *   shot | framing | camera | slug | action | dialogue | seconds
 *
 * e.g.  1 | WIDE | STATIC | INT. GRAND HALL - NIGHT | Moonlight through the
 *       dome; the Nightingale glows on its plinth. | | 5
 *
 * Blank lines and lines starting with # are ignored. Missing trailing fields are
 * fine. Renders to the neon panel grid used across the app.
 *
 * Exposed as window.Storyboard (also module.exports for headless testing).
 */
(function (root) {
  'use strict';

  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  // Parse pipe-delimited panel lines into an array of panel objects.
  function parse(text) {
    var panels = [];
    String(text == null ? '' : text).replace(/\r\n?/g, '\n').split('\n').forEach(function (raw) {
      var line = raw.trim();
      if (!line || line.charAt(0) === '#') return;
      var parts = line.split('|').map(function (p) { return p.trim(); });
      panels.push({
        shot: parts[0] || String(panels.length + 1),
        framing: parts[1] || '',
        camera: parts[2] || '',
        slug: parts[3] || '',
        action: parts[4] || '',
        line: parts[5] || '',
        dur: parts[6] || ''
      });
    });
    return panels;
  }

  function totalSeconds(panels) {
    return panels.reduce(function (sum, p) {
      var n = parseInt(p.dur, 10);
      return sum + (isNaN(n) ? 0 : n);
    }, 0);
  }

  function fmtRuntime(sec) {
    if (!sec) return '';
    var m = Math.floor(sec / 60), s = sec % 60;
    return m + ' min' + (s ? ' ' + s + ' s' : '');
  }

  function renderPanel(p) {
    var meta = '';
    if (p.framing) meta += '<span class="tag frame-tag">' + esc(p.framing) + '</span>';
    if (p.camera) meta += '<span class="tag cam-tag">' + esc(p.camera) + '</span>';
    var dia = '';
    if (p.line) {
      // Highlight a leading "NAME:" speaker label if present.
      var m = p.line.match(/^([^:]{1,32}):\s*(.*)$/);
      dia = m
        ? '<div class="sb-line"><b>' + esc(m[1]) + ':</b> ' + esc(m[2]) + '</div>'
        : '<div class="sb-line">' + esc(p.line) + '</div>';
    }
    return '<div class="sb-panel">' +
      '<div class="sb-frame"><span class="sb-num">' + esc(p.shot) + '</span>' +
      (p.dur ? '<span class="sb-dur">' + esc(p.dur) + 's</span>' : '') + '</div>' +
      '<div class="sb-body">' +
      (meta ? '<div class="sb-meta">' + meta + '</div>' : '') +
      (p.slug ? '<div class="sb-slug">' + esc(p.slug) + '</div>' : '') +
      (p.action ? '<div class="sb-desc">' + esc(p.action) + '</div>' : '') +
      dia +
      '</div></div>';
  }

  // Render an array of panels (or panel text) into the board grid HTML.
  function render(panelsOrText) {
    var panels = typeof panelsOrText === 'string' ? parse(panelsOrText) : (panelsOrText || []);
    return '<div class="sb-board">' + panels.map(renderPanel).join('') + '</div>';
  }

  function summary(panelsOrText) {
    var panels = typeof panelsOrText === 'string' ? parse(panelsOrText) : (panelsOrText || []);
    var rt = fmtRuntime(totalSeconds(panels));
    return panels.length + ' panel' + (panels.length === 1 ? '' : 's') +
      (rt ? ' · ~' + rt : '');
  }

  var API = {
    parse: parse, render: render, summary: summary,
    totalSeconds: totalSeconds, fmtRuntime: fmtRuntime
  };
  root.Storyboard = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
