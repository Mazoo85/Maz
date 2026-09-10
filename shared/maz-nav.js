/*
 * MAZ ARCADE — shared in-app navigation.
 *
 * Drop this one line into any page in the repo and it grows a link back to the
 * hub plus a jump menu to every other project:
 *
 *   <script src="../shared/maz-nav.js" defer></script>
 *
 * It loads shared/projects.js itself, so the project list lives in exactly one
 * file. Pages that are not one level below the repo root override the path with
 * a data-root attribute, e.g. data-root="../../".
 *
 * Two modes, because the hosts are not alike:
 *
 *   data-mode="inline" (the default) — a slim sticky strip inserted at the top
 *     of the document. It takes part in layout, so it pushes the page down and
 *     can never cover a heading or a control. Right for ordinary scrolling apps.
 *
 *   data-mode="overlay" — a small floating pill in the corner, for full-screen
 *     canvas games where there is no document flow to sit in. It covers as
 *     little canvas as possible and dims itself once play starts.
 *
 * In both modes pointer events are confined to the nav itself and, while open,
 * the backdrop; key events are swallowed only while the menu is open, so game
 * input is untouched the rest of the time.
 */
(function () {
  'use strict';

  if (window.__mazNavLoaded) return;
  window.__mazNavLoaded = true;

  var script =
    document.currentScript ||
    (function () {
      var all = document.getElementsByTagName('script');
      for (var i = all.length - 1; i >= 0; i--) {
        if ((all[i].src || '').indexOf('maz-nav.js') !== -1) return all[i];
      }
      return null;
    })();

  var root = (script && script.getAttribute('data-root')) || '../';
  if (root && root.slice(-1) !== '/') root += '/';

  // The page announces which project it is so the menu can mark it current.
  var currentId = (script && script.getAttribute('data-current')) || '';

  // Default to inline: it is the mode that cannot obscure anything.
  var mode = (script && script.getAttribute('data-mode')) === 'overlay' ? 'overlay' : 'inline';

  function loadProjects(done) {
    if (window.MAZ_PROJECTS) return done(window.MAZ_PROJECTS);
    var s = document.createElement('script');
    s.src = root + 'shared/projects.js';
    s.onload = function () {
      done(window.MAZ_PROJECTS || []);
    };
    s.onerror = function () {
      done([]); // still render the "back to hub" pill even if the list is missing
    };
    document.head.appendChild(s);
  }

  function injectStyles() {
    var css = [
      '.mazNav{z-index:2147483000;font-family:"Trebuchet MS","Segoe UI",system-ui,sans-serif;}',
      '.mazNav *{box-sizing:border-box;}',
      // Overlay: floats above a full-screen canvas, click-through except the pill.
      '.mazNav--overlay{position:fixed;top:0;left:0;pointer-events:none;}',
      // Inline: a real strip in the document, so it displaces content instead of hiding it.
      '.mazNav--inline{position:sticky;top:0;display:block;padding:5px 6px;',
      'background:rgba(11,7,20,.94);border-bottom:1px solid rgba(255,255,255,.09);',
      'backdrop-filter:blur(6px);-webkit-backdrop-filter:blur(6px);}',
      '.mazNav--inline .mazNav-pill{margin:0;}',
      // min-height keeps it a real touch target (>=28px) without making it bulky.
      '.mazNav-pill{pointer-events:auto;display:inline-flex;align-items:center;gap:6px;margin:8px;',
      'padding:5px 11px;min-height:30px;',
      'border:1px solid rgba(255,45,149,.55);border-radius:999px;background:rgba(10,6,20,.72);color:#ffd9ef;',
      'font-size:11px;font-weight:700;letter-spacing:1.5px;text-transform:uppercase;cursor:pointer;',
      'backdrop-filter:blur(4px);-webkit-backdrop-filter:blur(4px);box-shadow:0 0 12px rgba(255,45,149,.35);',
      'opacity:1;transition:opacity .4s ease,transform .15s ease;}',
      '.mazNav-pill:hover,.mazNav-pill:focus-visible{opacity:1!important;transform:translateY(1px);outline:none;',
      'border-color:#ff2d95;box-shadow:0 0 18px rgba(255,45,149,.6);}',
      '.mazNav.is-dim .mazNav-pill{opacity:.22;}',
      '.mazNav-mark{color:#39ff14;font-size:10px;line-height:1;}',
      '.mazNav-backdrop{pointer-events:auto;position:fixed;inset:0;background:rgba(4,2,10,.82);',
      'backdrop-filter:blur(3px);-webkit-backdrop-filter:blur(3px);opacity:0;transition:opacity .18s ease;}',
      '.mazNav-panel{pointer-events:auto;position:fixed;top:0;left:0;right:0;max-height:100%;overflow:auto;',
      '-webkit-overflow-scrolling:touch;padding:14px 14px 18px;background:linear-gradient(180deg,#150a26,#0b0714);',
      'border-bottom:2px solid #ff2d95;box-shadow:0 10px 40px rgba(0,0,0,.7);',
      'transform:translateY(-104%);transition:transform .22s ease;}',
      '.mazNav.is-open .mazNav-backdrop{opacity:1;}',
      '.mazNav.is-open .mazNav-panel{transform:translateY(0);}',
      '.mazNav-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px;}',
      '.mazNav-logo{font-size:15px;font-weight:900;letter-spacing:4px;color:#fff;text-shadow:0 0 12px rgba(255,45,149,.8);}',
      '.mazNav-logo b{color:#00e5ff;font-weight:900;}',
      '.mazNav-close{pointer-events:auto;border:1px solid rgba(255,255,255,.25);background:transparent;color:#fff;',
      'width:30px;height:30px;border-radius:8px;font-size:16px;line-height:1;cursor:pointer;}',
      '.mazNav-close:hover{border-color:#ff2d95;color:#ff2d95;}',
      '.mazNav-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:8px;}',
      // Keep the menu readable on a wide monitor instead of stretching edge to edge.
      '.mazNav-panel>*{max-width:940px;margin-left:auto;margin-right:auto;}',
      '.mazNav-item{display:block;padding:9px 11px;border:1px solid rgba(255,255,255,.14);border-radius:10px;',
      'background:rgba(255,255,255,.03);color:#e9e2f7;text-decoration:none;transition:border-color .15s,background .15s;}',
      '.mazNav-item:hover{background:rgba(255,255,255,.08);}',
      '.mazNav-item[aria-current="page"]{opacity:.55;pointer-events:none;}',
      '.mazNav-name{display:block;font-size:12px;font-weight:800;letter-spacing:1px;}',
      '.mazNav-tag{display:block;margin-top:2px;font-size:10.5px;opacity:.62;letter-spacing:.3px;}',
      '.mazNav-sep{margin:14px 0 8px;font-size:10px;letter-spacing:3px;text-transform:uppercase;opacity:.45;color:#fff;}',
      '@media (max-width:420px){.mazNav-grid{grid-template-columns:1fr 1fr;}.mazNav-tag{display:none;}}',
      '@media (prefers-reduced-motion:reduce){.mazNav-pill,.mazNav-panel,.mazNav-backdrop{transition:none;}}'
    ].join('');
    var tag = document.createElement('style');
    tag.setAttribute('data-maz-nav', '');
    tag.textContent = css;
    document.head.appendChild(tag);
  }

  function build(projects) {
    injectStyles();

    var wrap = document.createElement('div');
    wrap.className = 'mazNav mazNav--' + mode;

    var pill = document.createElement('button');
    pill.type = 'button';
    pill.className = 'mazNav-pill';
    pill.setAttribute('aria-expanded', 'false');
    pill.setAttribute('aria-label', 'Open the MAZ ARCADE menu');
    pill.innerHTML = '<span class="mazNav-mark">▲</span><span>MAZ</span>';

    var backdrop = document.createElement('div');
    backdrop.className = 'mazNav-backdrop';
    backdrop.hidden = true;

    var panel = document.createElement('div');
    panel.className = 'mazNav-panel';
    panel.setAttribute('role', 'dialog');
    panel.setAttribute('aria-label', 'MAZ ARCADE menu');
    panel.hidden = true;

    var head = document.createElement('div');
    head.className = 'mazNav-head';
    head.innerHTML =
      '<a class="mazNav-logo" href="' +
      root +
      'index.html" style="text-decoration:none">MAZ <b>ARCADE</b></a>';
    var close = document.createElement('button');
    close.type = 'button';
    close.className = 'mazNav-close';
    close.setAttribute('aria-label', 'Close menu');
    close.textContent = '✕';
    head.appendChild(close);
    panel.appendChild(head);

    var groups = [
      { key: 'play', label: 'Games' },
      { key: 'app', label: 'Apps' },
      { key: 'code', label: 'Code & docs' }
    ];

    groups.forEach(function (group) {
      var members = projects.filter(function (p) {
        return p.kind === group.key;
      });
      if (!members.length) return;

      var sep = document.createElement('div');
      sep.className = 'mazNav-sep';
      sep.textContent = group.label;
      panel.appendChild(sep);

      var grid = document.createElement('div');
      grid.className = 'mazNav-grid';
      members.forEach(function (p) {
        var a = document.createElement('a');
        a.className = 'mazNav-item';
        a.href = root + p.path;
        a.style.borderLeft = '3px solid ' + p.accent;
        if (p.id === currentId) a.setAttribute('aria-current', 'page');
        var name = document.createElement('span');
        name.className = 'mazNav-name';
        name.textContent = p.name;
        var tag = document.createElement('span');
        tag.className = 'mazNav-tag';
        tag.textContent = p.id === currentId ? "you're here" : p.tag;
        a.appendChild(name);
        a.appendChild(tag);
        grid.appendChild(a);
      });
      panel.appendChild(grid);
    });

    wrap.appendChild(pill);
    wrap.appendChild(backdrop);
    wrap.appendChild(panel);

    if (mode === 'inline') {
      // First in the document, so the strip pushes the page down rather than
      // sitting on top of whatever the app draws in its own top-left corner.
      document.body.insertBefore(wrap, document.body.firstChild);
    } else {
      document.body.appendChild(wrap);
    }

    // --- open / close -----------------------------------------------------
    var open = false;

    function setOpen(next) {
      open = next;
      wrap.classList.toggle('is-open', open);
      pill.setAttribute('aria-expanded', open ? 'true' : 'false');
      if (open) {
        backdrop.hidden = false;
        panel.hidden = false;
        wrap.classList.remove('is-dim');
        // Let the browser paint the hidden->shown state before transitioning.
        void panel.offsetHeight;
        close.focus();
      } else {
        window.setTimeout(function () {
          if (!open) {
            backdrop.hidden = true;
            panel.hidden = true;
          }
        }, 220);
        if (mode === 'overlay') scheduleDim();
      }
    }

    pill.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      setOpen(!open);
    });
    close.addEventListener('click', function () {
      setOpen(false);
    });
    backdrop.addEventListener('click', function () {
      setOpen(false);
    });

    // While the menu is open the host page must not also act on the keystroke —
    // otherwise Escape (and WASD) would drive the game behind the menu.
    window.addEventListener(
      'keydown',
      function (e) {
        if (!open) return;
        if (e.key === 'Escape') {
          setOpen(false);
          pill.focus();
        }
        e.stopPropagation();
      },
      true
    );

    // --- get out of the way once play starts ------------------------------
    var dimTimer = null;
    function scheduleDim() {
      window.clearTimeout(dimTimer);
      dimTimer = window.setTimeout(function () {
        if (!open) wrap.classList.add('is-dim');
      }, 5000);
    }
    // Only the overlay covers anything, so only the overlay needs to fade.
    //
    // Deliberately *not* reset by activity: aiming in ZOMBOID is a constant
    // stream of pointermove, so an activity-reset timer would keep the pill at
    // full brightness for the whole session — the opposite of getting out of
    // the way. It fades on a fixed delay; :hover brings it straight back.
    if (mode === 'overlay') scheduleDim();
  }

  function start() {
    loadProjects(build);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', start);
  } else {
    start();
  }
})();
