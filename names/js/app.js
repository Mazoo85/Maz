/*
 * NAME FORGE — UI layer
 * ---------------------
 * Wires the DOM to window.NameForge. The only state that outlives a visit is
 * the saved-names list, which lives in localStorage — nothing leaves the
 * browser.
 */
(function () {
  'use strict';

  var F = window.NameForge;
  var LIB_KEY = 'names.library.v1';

  var el = {
    nameOut: document.getElementById('nameOut'),
    orderChip: document.getElementById('orderChip'),
    seedChip: document.getElementById('seedChip'),
    roll: document.getElementById('roll'),
    copy: document.getElementById('copy'),
    save: document.getElementById('save'),
    order: document.getElementById('order'),
    style: document.getElementById('style'),
    scaleStat: document.getElementById('scaleStat'),
    batchList: document.getElementById('batchList'),
    batchStatus: document.getElementById('batchStatus'),
    batchActions: document.getElementById('batchActions'),
    copyBatch: document.getElementById('copyBatch'),
    exportTxt: document.getElementById('exportTxt'),
    exportCsv: document.getElementById('exportCsv'),
    libraryList: document.getElementById('libraryList'),
    libCount: document.getElementById('libCount'),
    libEmpty: document.getElementById('libEmpty'),
    libActions: document.getElementById('libActions'),
    copyLib: document.getElementById('copyLib'),
    exportLib: document.getElementById('exportLib'),
    clearLib: document.getElementById('clearLib'),
    wordStat: document.getElementById('wordStat')
  };

  var current = null; // the name on the stage right now
  var batch = [];     // the most recent batch

  // ------------------------------------------------------------- utilities
  // The slide-up message is shared/maz-toast.js's; this page styles #toast itself.
  var toast = window.MazToast || function () {};

  function copyText(text, what) {
    function fallback() {
      var ta = document.createElement('textarea');
      ta.value = text;
      ta.setAttribute('readonly', '');
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      try { document.execCommand('copy'); } catch (e) { /* nothing else to try */ }
      document.body.removeChild(ta);
      toast(what + ' copied');
    }
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(function () {
        toast(what + ' copied');
      }, fallback);
    } else {
      fallback();
    }
  }

  function download(filename, text, type) {
    var blob = new Blob([text], { type: (type || 'text/plain') + ';charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  }

  function orderLabel(id) {
    return id === 'noun-adjective' ? 'noun first' : 'adjective first';
  }

  function opts() {
    return { order: el.order.value, style: el.style.value };
  }

  // ---------------------------------------------------------------- stage
  function show(name) {
    current = name;
    el.nameOut.textContent = name.text;
    el.nameOut.classList.remove('roll');
    void el.nameOut.offsetWidth; // restart the animation
    el.nameOut.classList.add('roll');

    // The two words it was built from, adjective first — and, on the rare
    // roll that isn't adjective-first, a note saying so.
    el.orderChip.textContent = name.adjective + ' + ' + name.noun +
      (name.order === 'adjective-noun' ? '' : ' · ' + orderLabel(name.order));
    el.orderChip.hidden = false;
    el.seedChip.textContent = 'seed ' + name.seed;
    el.seedChip.hidden = false;
  }

  function roll() {
    show(F.generate(opts()));
  }

  // -------------------------------------------------------------- library
  function loadLib() {
    try {
      var raw = localStorage.getItem(LIB_KEY);
      var list = raw ? JSON.parse(raw) : [];
      return Array.isArray(list) ? list : [];
    } catch (e) {
      return [];
    }
  }

  function saveLib(list) {
    try {
      localStorage.setItem(LIB_KEY, JSON.stringify(list));
    } catch (e) {
      toast('could not save — storage is full or blocked');
    }
  }

  function addToLib(name) {
    var list = loadLib();
    for (var i = 0; i < list.length; i++) {
      if (list[i].text.toLowerCase() === name.text.toLowerCase()) {
        toast('already saved');
        return;
      }
    }
    list.unshift({
      text: name.text,
      adjective: name.adjective,
      noun: name.noun,
      order: name.order,
      style: name.style,
      seed: name.seed
    });
    saveLib(list);
    renderLib();
    toast('saved ' + name.text);
  }

  function renderLib() {
    var list = loadLib();
    el.libCount.textContent = String(list.length);
    el.libraryList.innerHTML = '';
    el.libEmpty.hidden = list.length > 0;
    el.libActions.hidden = list.length === 0;

    list.forEach(function (item, index) {
      var li = document.createElement('li');

      var name = document.createElement('span');
      name.className = 'n';
      name.textContent = item.text;

      var actions = document.createElement('div');
      actions.className = 'row-actions';

      var copyBtn = document.createElement('button');
      copyBtn.textContent = '📋';
      copyBtn.title = 'Copy';
      copyBtn.addEventListener('click', function () { copyText(item.text, item.text); });

      var delBtn = document.createElement('button');
      delBtn.textContent = '✕';
      delBtn.title = 'Remove';
      delBtn.addEventListener('click', function () {
        var next = loadLib();
        next.splice(index, 1);
        saveLib(next);
        renderLib();
      });

      actions.appendChild(copyBtn);
      actions.appendChild(delBtn);
      li.appendChild(name);
      li.appendChild(actions);
      el.libraryList.appendChild(li);
    });
  }

  // ---------------------------------------------------------------- batch
  function rollBatch(n) {
    batch = F.generateMany(n, opts());
    el.batchList.innerHTML = '';
    el.batchActions.hidden = batch.length === 0;
    el.batchStatus.textContent = batch.length + ' names';

    batch.forEach(function (name) {
      var li = document.createElement('li');

      var text = document.createElement('span');
      text.className = 'n';
      text.textContent = name.text;

      // Only worth saying when the name did not come out adjective-first.
      var why = null;
      if (name.order !== 'adjective-noun') {
        why = document.createElement('span');
        why.className = 'why';
        why.textContent = orderLabel(name.order);
      }

      var actions = document.createElement('div');
      actions.className = 'row-actions';

      var saveBtn = document.createElement('button');
      saveBtn.textContent = '★';
      saveBtn.title = 'Save';
      saveBtn.addEventListener('click', function () { addToLib(name); });

      var copyBtn = document.createElement('button');
      copyBtn.textContent = '📋';
      copyBtn.title = 'Copy';
      copyBtn.addEventListener('click', function () { copyText(name.text, name.text); });

      actions.appendChild(saveBtn);
      actions.appendChild(copyBtn);
      li.appendChild(text);
      if (why) li.appendChild(why);
      li.appendChild(actions);
      el.batchList.appendChild(li);
    });
  }

  // ----------------------------------------------------------------- init
  function fillSelects() {
    var orderOptions = F.ORDERS.concat([
      { id: 'random', label: '🎲 Let the dice decide', example: 'either way round' }
    ]);
    orderOptions.forEach(function (o) {
      var opt = document.createElement('option');
      opt.value = o.id;
      opt.textContent = o.example ? o.label + ' — ' + o.example : o.label;
      el.order.appendChild(opt);
    });

    var styleOptions = F.STYLES.concat([{ id: 'random', label: '🎲 Surprise me', example: '' }]);
    styleOptions.forEach(function (s) {
      var opt = document.createElement('option');
      opt.value = s.id;
      opt.textContent = s.example ? s.label + ' — ' + s.example : s.label;
      el.style.appendChild(opt);
    });

    el.order.value = 'adjective-noun';
    el.style.value = 'title';
  }

  function renderScale() {
    var order = el.order.value;
    var total = F.combinations(order).toLocaleString();
    el.scaleStat.innerHTML = order === 'random'
      ? '<b>' + total + '</b> different names — every adjective against every noun, ' +
        'and each pair can land either way round.'
      : '<b>' + total + '</b> different names — every one of the ' +
        F.words.adjectives.length + ' adjectives against every one of the ' +
        F.words.nouns.length + ' nouns.';
    el.wordStat.textContent =
      F.words.adjectives.length + ' adjectives × ' + F.words.nouns.length + ' nouns';
  }

  el.roll.addEventListener('click', roll);
  el.copy.addEventListener('click', function () {
    if (current) copyText(current.text, current.text);
  });
  el.save.addEventListener('click', function () {
    if (current) addToLib(current);
  });
  el.order.addEventListener('change', function () {
    renderScale();
    roll();
  });
  el.style.addEventListener('change', roll);

  Array.prototype.forEach.call(document.querySelectorAll('.batch-btn'), function (btn) {
    btn.addEventListener('click', function () {
      rollBatch(parseInt(btn.getAttribute('data-n'), 10) || 10);
    });
  });

  el.copyBatch.addEventListener('click', function () {
    if (batch.length) copyText(F.toText(batch), batch.length + ' names');
  });
  el.exportTxt.addEventListener('click', function () {
    if (batch.length) download('names-' + batch.length + '.txt', F.toText(batch));
  });
  el.exportCsv.addEventListener('click', function () {
    if (batch.length) download('names-' + batch.length + '.csv', F.toCsv(batch), 'text/csv');
  });

  el.copyLib.addEventListener('click', function () {
    var list = loadLib();
    if (list.length) copyText(F.toText(list), list.length + ' saved names');
  });
  el.exportLib.addEventListener('click', function () {
    var list = loadLib();
    if (list.length) download('saved-names.txt', F.toText(list));
  });
  el.clearLib.addEventListener('click', function () {
    if (!loadLib().length) return;
    if (window.confirm('Remove every saved name?')) {
      saveLib([]);
      renderLib();
    }
  });

  // Space or Enter anywhere that isn't a control rolls again.
  document.addEventListener('keydown', function (e) {
    var tag = (e.target && e.target.tagName || '').toLowerCase();
    if (tag === 'input' || tag === 'select' || tag === 'textarea' || tag === 'button') return;
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      roll();
    }
  });

  fillSelects();
  renderScale();
  renderLib();
  roll();
})();
