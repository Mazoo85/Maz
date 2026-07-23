/*
 * MadLibs Story Forge — screenplay renderer
 * -----------------------------------------
 * Parses a lightweight, Fountain-like screenplay text into formatted HTML so a
 * script can be READ inside the app as a real teleplay. Rules (forgiving):
 *
 *   Scene heading   line starts with INT./EXT./EST./INT./EXT or a leading "."
 *   Act / section   COLD OPEN, ACT ONE/TWO/THREE, TAG, TEASER, END OF …  (centered)
 *                   or a Fountain "> CENTERED <" line
 *   Transition      an UPPERCASE line ending in "TO:" / "OUT." / "IN:" or "> …"
 *   Character cue    a short UPPERCASE line with a non-blank line under it
 *   Parenthetical   a line wrapped in ( … )
 *   Dialogue        the lines under a character cue, until a blank line
 *   Action          anything else
 *
 * Exposed as window.Screenplay (also module.exports for headless testing).
 */
(function (root) {
  'use strict';

  function esc(s) {
    return String(s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  var SCENE = /^(INT|EXT|EST|INT\.?\/EXT|I\/E)[\.\s]/i;
  var ACT = /^(COLD OPEN|TEASER|ACT\s+(ONE|TWO|THREE|FOUR|FIVE|\d+)|TAG|END OF (EPISODE|ACT|SHOW|TEASER)|MAIN TITLES?|SMASH TO TITLE.*)\b/i;

  function isScene(l) { return SCENE.test(l) || /^\.[A-Za-z]/.test(l); }
  function isAct(l) {
    return ACT.test(l) || (/^>.*<$/.test(l));
  }
  function isTransition(l) {
    if (/^>\s?\S/.test(l) && !/<$/.test(l)) return true;
    return l === l.toUpperCase() && l.length <= 40 && /(TO:|OUT\.|IN:|OUT:)$/.test(l);
  }
  function isParen(l) { return /^\(.*\)$/.test(l); }

  // A character cue: short, uppercase, not another element, with dialogue under it.
  function isCharacter(l, lines, idx) {
    if (!l || l.length > 42) return false;
    if (isScene(l) || isAct(l) || isTransition(l)) return false;
    if (/[.!?,]$/.test(l)) return false;               // sentences end in punctuation
    // allow letters, digits, spaces, and cue suffixes like (V.O.) (O.S.) (CONT'D)
    if (!/^[A-Z0-9][A-Z0-9 .'#\-\/&()]*$/.test(l)) return false;
    if (l !== l.toUpperCase()) return false;
    if (!/[A-Z]/.test(l)) return false;
    // must be followed by a non-blank line (the dialogue)
    var next = lines[idx + 1];
    return next != null && next.trim() !== '';
  }

  function render(text) {
    var lines = String(text == null ? '' : text).replace(/\r\n?/g, '\n').split('\n');
    var out = [];
    var i = 0;
    while (i < lines.length) {
      var line = lines[i].trim();
      if (!line) { i++; continue; }

      if (isAct(line)) {
        out.push('<p class="sp-act">' + esc(line.replace(/^>\s?/, '').replace(/\s?<$/, '')) + '</p>');
        i++; continue;
      }
      if (isScene(line)) {
        out.push('<p class="sp-scene">' + esc(line.replace(/^\./, '')) + '</p>');
        i++; continue;
      }
      if (isTransition(line)) {
        out.push('<p class="sp-trans">' + esc(line.replace(/^>\s?/, '')) + '</p>');
        i++; continue;
      }
      if (isCharacter(line, lines, i)) {
        out.push('<div class="sp-dual">');
        out.push('<p class="sp-char">' + esc(line) + '</p>');
        i++;
        while (i < lines.length && lines[i].trim()) {
          var d = lines[i].trim();
          out.push('<p class="' + (isParen(d) ? 'sp-paren' : 'sp-dia') + '">' + esc(d) + '</p>');
          i++;
        }
        out.push('</div>');
        continue;
      }
      // action: gather consecutive lines until a blank or a new element
      var buf = [];
      while (i < lines.length) {
        var a = lines[i].trim();
        if (!a || isScene(a) || isAct(a) || isTransition(a) || isCharacter(a, lines, i)) break;
        buf.push(esc(a));
        i++;
      }
      if (buf.length) out.push('<p class="sp-action">' + buf.join('<br>') + '</p>');
    }
    return out.join('\n');
  }

  // Rough page/time estimate: ~1 screenplay minute per "page"; ~55 lines/page.
  function estimateMinutes(text) {
    var lines = String(text == null ? '' : text).split('\n').filter(function (l) {
      return l.trim();
    }).length;
    return Math.max(1, Math.round(lines / 22));
  }

  var API = { render: render, estimateMinutes: estimateMinutes };
  root.Screenplay = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
