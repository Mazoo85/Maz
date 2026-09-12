/*
 * The little message that slides up from the bottom of the page and goes away.
 *
 * MADLIBS and NAME FORGE had written this out identically — the same element,
 * the same class, the same 1600ms — which is how `tools/inventory/main.mjs`
 * found it. It is not in shared/maz-util.js because that file is for pure
 * helpers and this one touches the document; it sits beside shared/maz-nav.js
 * instead, with the rest of the shared page furniture.
 *
 * What is shared is the BEHAVIOUR, not the look. The element is created with
 * id="toast" and given the class "show" while it is up, and each project styles
 * `#toast` in its own CSS — MADLIBS' is a green-bordered rounded box, NAME
 * FORGE's an orange pill — so adopting this changes nothing on screen.
 *
 *   <script src="../shared/maz-toast.js"></script>
 *   MazToast('Copied');
 */
(function (root) {
  'use strict';

  var timer = null;

  /** Show `msg` for `ms` (default 1600), replacing whatever is up. */
  function toast(msg, ms) {
    var doc = root.document;
    if (!doc) return; // no page: a headless test importing this does nothing
    var t = doc.getElementById('toast');
    if (!t) {
      t = doc.createElement('div');
      t.id = 'toast';
      doc.body.appendChild(t);
    }
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(timer);
    timer = setTimeout(function () { t.classList.remove('show'); },
                       typeof ms === 'number' ? ms : 1600);
  }

  if (typeof module === 'object' && module.exports) module.exports = toast;
  root.MazToast = toast;
})(typeof globalThis !== 'undefined' ? globalThis : this);
