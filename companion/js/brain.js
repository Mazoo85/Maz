/*
 * COMPANION — the brain
 * ---------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test.
 *
 * The brain hears one line of chat at a time and answers with what to say and
 * what to do:
 *
 *   brain.hear('go get the apple', world)
 *     -> { say: 'On it!', actions: [{ type: 'fetch', target: 'apple' }],
 *          understood: true, mood: 'happy' }
 *
 * It is trainable. Out of the box it knows the everyday commands (follow,
 * stay, come here, fetch, go to, jump, dance, sit, explore, sleep) and a bit
 * of small talk. Everything else you teach it:
 *
 *   "when I say heel, follow me"   a new phrase for a command
 *   "when I say hi, say howdy!"    a new reply
 *   "my name is Cody"              facts it remembers
 *   "remember that I like apples"
 *   (it doesn't understand) -> "it means jump"   teaches the phrase it missed
 *   "good job" / "no, wrong"       after a guess: learn it, or never guess it again
 *
 * Phrases it was taught are matched loosely too, so "heel boy" still works
 * after you taught it "heel".
 *
 * `world` tells it what exists right now, so "get the gem" only works if there
 * is a gem: { items: ['apple', 'gem'], places: ['tree', 'pond'] }.
 *
 * Everything it learns is in brain.toJSON(), which the page keeps in
 * localStorage.
 *
 * Exposed as: window.CompanionBrain (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var SAVE_VERSION = 1;
  var MAX_FACTS = 50;
  var FUZZY_THRESHOLD = 0.6;

  // Every action the body in the game knows how to perform.
  var ACTION_TYPES = ['follow', 'stay', 'come', 'goto', 'fetch', 'jump', 'dance', 'sit', 'wander', 'sleep'];

  var DIRECTIONS = ['left', 'right', 'up', 'down'];

  // Words that carry no meaning for a command: "could you please jump" is "jump".
  var FILLER = /\b(please|pls|can you|could you|would you|will you|i want you to|i need you to|go ahead and|hey|ok|okay|now|buddy|pal|for me)\b/g;

  // Built-in commands, tried in order: the first that matches wins, so the
  // longer phrases ("come with me") sit above the shorter ones ("come").
  var COMMANDS = [
    { type: 'stay', re: /\b(stop following|stay(?! close| with)|wait|stop|halt|hold on|hold up|freeze|dont move|stand still)\b/ },
    { type: 'follow', re: /\b(follow|come with me|come along|lets go|tag along|stick with me|stay close|with me)\b/ },
    { type: 'fetch', re: /\b(get|fetch|grab|bring|pick up|collect|find)\b/, needsTarget: 'item' },
    { type: 'come', re: /\b(come here|come back|over here|to me|come)\b/ },
    { type: 'goto', re: /\b(go|walk|run|move|head|wander over)\b/, needsTarget: 'place' },
    { type: 'jump', re: /\b(jump|hop|leap)\b/ },
    { type: 'dance', re: /\b(dance|boogie|spin|twirl)\b/ },
    { type: 'sit', re: /\b(sit|sit down|take a seat)\b/ },
    { type: 'wander', re: /\b(explore|wander|look around|roam|go play|have fun)\b/ },
    { type: 'sleep', re: /\b(sleep|nap|rest|take a break|go to bed)\b/ }
  ];

  var ACK = {
    follow: ['Right behind you!', 'Lead the way!', 'Following!'],
    stay: ['Staying put.', 'I\'ll wait right here.', 'Okay, not moving.'],
    come: ['Coming!', 'On my way!', 'Be right there!'],
    goto: ['Heading there now.', 'Off I go!', 'On my way.'],
    fetch: ['On it!', 'I\'ll get it!', 'Leave it to me!'],
    jump: ['Hup!', 'Wheee!', 'Boing!'],
    dance: ['Watch these moves!', 'Dance party!', 'Ta-da!'],
    sit: ['Sitting.', 'Taking a seat.', 'Ahh, comfy.'],
    wander: ['I\'ll have a look around!', 'Exploring!', 'Adventure time!'],
    sleep: ['Zzz... wake me if you need me.', 'Just a little nap...', 'Yawn... goodnight.']
  };

  // ---- text helpers --------------------------------------------------------

  function normalize(text) {
    return String(text || '')
      .toLowerCase()
      .replace(/[’']/g, '')
      .replace(/[^a-z0-9\s]/g, ' ')
      .replace(/\s+/g, ' ')
      .trim();
  }

  // The meaning-bearing part of a line: filler and the companion's own name gone.
  function core(text, name) {
    var t = normalize(text).replace(FILLER, ' ');
    if (name) t = t.replace(new RegExp('\\b' + escapeRe(normalize(name)) + '\\b', 'g'), ' ');
    return t.replace(/\s+/g, ' ').trim();
  }

  function escapeRe(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  }

  function words(text) {
    return normalize(text).split(' ').filter(Boolean);
  }

  // 0..1: how alike two phrases are, by the words they share, with credit for
  // near-miss spellings so "folow" still counts as "follow".
  function similarity(a, b) {
    var wa = words(a), wb = words(b);
    if (!wa.length || !wb.length) return 0;
    var shared = 0;
    var used = {};
    for (var i = 0; i < wa.length; i++) {
      for (var j = 0; j < wb.length; j++) {
        if (used[j]) continue;
        if (wa[i] === wb[j] || (wa[i].length > 3 && editDistance(wa[i], wb[j]) <= 1)) {
          shared++;
          used[j] = true;
          break;
        }
      }
    }
    var jaccard = shared / (wa.length + wb.length - shared);
    // "heel boy" against a taught "heel": every word of the shorter phrase is
    // there, and the line isn't much longer than it. Close enough to guess.
    var shorter = Math.min(wa.length, wb.length), longer = Math.max(wa.length, wb.length);
    var contained = shared === shorter && shorter / longer >= 0.34 ? 0.8 : 0;
    return Math.max(jaccard, contained);
  }

  function editDistance(a, b) {
    var prev = [], cur, i, j;
    for (j = 0; j <= b.length; j++) prev[j] = j;
    for (i = 1; i <= a.length; i++) {
      cur = [i];
      for (j = 1; j <= b.length; j++) {
        cur[j] = Math.min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
      }
      prev = cur;
    }
    return prev[b.length];
  }

  // "apples" -> "apple", "berries" -> "berry", so plurals find the item.
  function singular(w) {
    if (/ies$/.test(w)) return w.slice(0, -3) + 'y';
    if (/(ch|sh|x|s)es$/.test(w)) return w.slice(0, -2);
    if (/s$/.test(w) && !/ss$/.test(w)) return w.slice(0, -1);
    return w;
  }

  function findIn(text, names) {
    var ws = words(text).map(singular);
    for (var i = 0; i < names.length; i++) {
      var n = normalize(names[i]);
      var nw = n.split(' ').map(singular).join(' ');
      if ((' ' + ws.join(' ') + ' ').indexOf(' ' + nw + ' ') !== -1) return names[i];
    }
    return null;
  }

  function capitalize(s) {
    return s.replace(/\b[a-z]/g, function (c) { return c.toUpperCase(); });
  }

  // ---- commands ------------------------------------------------------------

  // Turn a line into a list of actions, or null if it has no command in it.
  // "jump and then dance" is two actions, done in order.
  function parseCommands(text, world, name) {
    var parts = core(text, name).split(/\b(?:and then|then|and|after that)\b/);
    var actions = [];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i].trim();
      if (!p) continue;
      var a = parseOne(p, world || {});
      if (a) actions.push(a);
    }
    return actions.length ? actions : null;
  }

  function parseOne(text, world) {
    var items = world.items || [];
    var places = world.places || [];
    for (var i = 0; i < COMMANDS.length; i++) {
      var c = COMMANDS[i];
      if (!c.re.test(text)) continue;
      if (c.needsTarget === 'item') {
        var item = findIn(text, items);
        if (item) return { type: 'fetch', target: item };
        // "get over here" is not a fetch; fall through to the other commands.
        continue;
      }
      if (c.needsTarget === 'place') {
        var dir = findIn(text, DIRECTIONS);
        if (dir) return { type: 'goto', target: dir };
        var place = findIn(text, places);
        if (place) return { type: 'goto', target: place };
        var thing = findIn(text, items);
        if (thing) return { type: 'fetch', target: thing };
        continue;
      }
      return { type: c.type };
    }
    return null;
  }

  // ---- the brain -----------------------------------------------------------

  function Brain(saved, opts) {
    opts = opts || {};
    this.rand = opts.rand || Math.random;
    this.name = 'Pip';
    this.playerName = '';
    this.phrases = {};   // normalized phrase -> { phrase, actions?, reply? }
    this.facts = [];     // things the player asked it to remember
    this.pending = null; // the last phrase it didn't understand, waiting to be taught
    this.lastGuess = null; // a loosely-matched phrase, for "good job" / "wrong"
    this.rejected = {};  // heard phrase -> taught phrases it must not be guessed as
    if (saved) this.load(saved);
  }

  Brain.prototype.pick = function (list) {
    return list[Math.floor(this.rand() * list.length) % list.length];
  };

  Brain.prototype.result = function (say, actions, extra) {
    var r = { say: say, actions: actions || [], understood: true, mood: 'happy' };
    if (extra) for (var k in extra) r[k] = extra[k];
    return r;
  };

  Brain.prototype.ackFor = function (actions) {
    var a = actions[0];
    var say = this.pick(ACK[a.type] || ['Okay!']);
    if (a.type === 'fetch') say = this.pick(['On it! One ' + a.target + ', coming up.', 'I\'ll get the ' + a.target + '!', 'Going for the ' + a.target + '!']);
    if (a.type === 'goto') say = DIRECTIONS.indexOf(a.target) !== -1 ? 'Going ' + a.target + '!' : 'Heading to the ' + a.target + '.';
    return say;
  };

  // Hear one line from the player. world: { items: [...], places: [...] }.
  Brain.prototype.hear = function (text, world) {
    world = world || {};
    var raw = String(text || '').trim();
    var n = normalize(raw);
    var guess = this.lastGuess;
    this.lastGuess = null;
    if (!n) return this.result('...?', [], { mood: 'neutral' });
    // The same line without the companion's name: "hi Pip" is a plain "hi".
    var nn = normalize(n.replace(new RegExp('\\b' + escapeRe(normalize(this.name)) + '\\b', 'g'), ' '));

    // Tried in order; the first that answers wins. `kind` tells the page who
    // answered, so it can hand plain chat to the smarter AI when there is one.
    var self = this;
    var steps = [
      ['teach', function () { return self.teaching(raw, n, world); }],
      ['feedback', function () { return self.feedback(nn, guess); }],
      ['memory', function () { return self.memory(raw, n); }],
      ['taught', function () { return self.taught(n, world, false); }],
      ['command', function () { return self.command(raw, world); }],
      ['chat', function () { return self.smallTalk(nn); }],
      ['taught', function () { return self.taught(n, world, true); }]
    ];
    for (var i = 0; i < steps.length; i++) {
      var r = steps[i][1]();
      if (!r) continue;
      if (!r.keepPending) this.pending = null;
      delete r.keepPending;
      r.kind = steps[i][0];
      return r;
    }

    if (/\b(get|fetch|grab|bring|pick up|collect)\b/.test(n)) {
      var items = uniq(world.items || []);
      return this.result('I don\'t see one of those around here.' + (items.length ? ' I can see: ' + items.join(', ') + '.' : ''), [], { mood: 'neutral', kind: 'missing' });
    }

    this.pending = raw;
    return {
      say: 'Hmm, I don\'t know what "' + raw + '" means yet. Teach me! Say "it means follow me", or "say hello back".',
      actions: [],
      understood: false,
      mood: 'confused',
      kind: 'unknown'
    };
  };

  // "when I say X, do Y", "X means Y", and "it means Y" right after it got lost.
  // Works on the original line (capitals kept, so a taught reply reads the way
  // it was typed); a comma is the clearest way to end the phrase being taught.
  Brain.prototype.teaching = function (raw, n, world) {
    var line = raw.replace(/[\u201c\u201d"]/g, '').trim();
    var m, phrase = null, meaning = null;

    if ((m = line.match(/^(?:when|if|whenever)\s+i\s+say\s+(.+?)\s*[,:;]\s*(.+)$/i)) ||
        (m = line.match(/^(?:when|if|whenever)\s+i\s+say\s+(\S+)\s+(.+)$/i))) {
      phrase = m[1];
      meaning = m[2].replace(/^(?:you should|you|then|that means|it means|i mean|means)\s+/i, '');
    } else if ((m = line.match(/^(.+?)\s+(?:means|is the same as|is short for)\s+(.+)$/i)) && !/^(it|that|this)$/i.test(m[1].trim())) {
      phrase = m[1];
      meaning = m[2];
    } else if (this.pending && (m = line.match(/^(?:(?:it|that|this)\s+)?(?:means|is short for)\s+(.+)$/i))) {
      phrase = this.pending;
      meaning = m[1];
    } else if (this.pending && /^(say|reply|answer|respond)\b/i.test(line)) {
      phrase = this.pending;
      meaning = line;
    }
    if (!phrase) return null;

    var sayIt = meaning.match(/^(?:to\s+)?(?:say|reply|answer|respond)(?:\s+with)?\s*[,:]?\s+(.+)$/i);
    if (sayIt) return this.learnReply(phrase, sayIt[1]);

    var actions = parseCommands(meaning, world, this.name) || this.taughtActions(meaning);
    if (!actions) {
      return this.result('I don\'t know how to "' + meaning + '" yet. I can follow, stay, come, fetch things, go places, jump, dance, sit, explore and sleep.',
        [], { mood: 'confused', keepPending: true });
    }
    var key = core(phrase, this.name);
    if (!key) return null;
    this.phrases[key] = { phrase: phrase, actions: copyActions(actions) };
    return this.result('Got it! When you say "' + phrase + '", I\'ll ' + describe(actions) + '.', [], { learned: key });
  };

  Brain.prototype.learnReply = function (phrase, reply) {
    var key = core(phrase, this.name);
    reply = reply.replace(/^["“]|["”]$/g, '').trim();
    if (!key || !reply) return null;
    this.phrases[key] = { phrase: phrase, reply: reply };
    return this.result('Got it! When you say "' + phrase + '", I\'ll say "' + reply + '".', [], { learned: key });
  };

  // A phrase already taught as a command can be used to teach another one:
  // "when I say heel, do the hop" after teaching "do the hop".
  Brain.prototype.taughtActions = function (meaning) {
    var t = this.phrases[core(meaning, this.name)];
    return t && t.actions ? t.actions : null;
  };

  // "good job" keeps the last loose guess; "no, wrong" makes it forget it.
  Brain.prototype.feedback = function (n, guess) {
    var praise = /^(good|great|nice|well done|yes|yay|perfect|awesome|good job|good boy|good girl|attaboy|thats right|correct)( job| one| work| boy| girl)?$/.test(n);
    var scold = /^(no|nope|wrong|bad|not that|thats wrong|thats not it|thats not what i meant)( (thats )?(wrong|not it|not that|not what i meant|boy|girl))?$/.test(n);
    if (!praise && !scold) return null;
    if (scold && guess) {
      var heard = core(guess.heard, this.name);
      (this.rejected[heard] = this.rejected[heard] || []).push(guess.key);
      return this.result('Oops, sorry! "' + guess.heard + '" isn\'t "' + guess.key + '" then. Teach me what it means if you like.', [{ type: 'stay' }], { mood: 'sad' });
    }
    if (praise && guess && this.phrases[guess.key]) {
      // Make the guess official: next time it's an exact match.
      var key = core(guess.heard, this.name);
      var t = this.phrases[guess.key];
      if (!this.phrases[key]) this.phrases[key] = t.reply ? { phrase: guess.heard, reply: t.reply } : { phrase: guess.heard, actions: copyActions(t.actions) };
      return this.result('Yay! I\'ll remember that "' + guess.heard + '" means the same thing.', [{ type: 'jump' }], { learned: key });
    }
    if (praise) return this.result(this.pick(['Thanks!', 'Hehe, thank you!', 'I try my best!']), [{ type: 'jump' }]);
    return this.result(this.pick(['Sorry...', 'Oh no, sorry!', 'I\'ll do better.']), [{ type: 'stay' }], { mood: 'sad' });
  };

  Brain.prototype.memory = function (raw, n) {
    var m;
    if ((m = n.match(/^(?:my name is|call me|im called|you can call me) (.+)$/))) {
      this.playerName = capitalize(afterPhrase(raw, m[1]));
      return this.result('Nice to meet you, ' + this.playerName + '! I\'ll remember that.', [{ type: 'jump' }]);
    }
    if ((m = n.match(/^(?:your name is|ill call you|i will call you|call yourself|youre called|your new name is) (.+)$/))) {
      this.name = capitalize(afterPhrase(raw, m[1]));
      return this.result('I love it! From now on I\'m ' + this.name + '.', [{ type: 'dance' }]);
    }
    if ((m = n.match(/^(?:please )?(?:remember|dont forget|never forget)(?: that)? (.+)$/))) {
      var fact = afterPhrase(raw, m[1]);
      this.facts.push(fact);
      if (this.facts.length > MAX_FACTS) this.facts.shift();
      return this.result('I\'ll remember that ' + fact + '.', []);
    }
    if (/^(what do you (remember|know)( about me)?|what have you learned|what did i teach you|what do you know)$/.test(n)) {
      var lines = [];
      if (this.playerName) lines.push('your name is ' + this.playerName);
      for (var i = 0; i < this.facts.length; i++) lines.push(this.facts[i]);
      var keys = Object.keys(this.phrases);
      if (keys.length) lines.push('I know ' + keys.length + ' phrase' + (keys.length === 1 ? '' : 's') + ' you taught me: ' + keys.map(function (k) { return '"' + k + '"'; }).join(', '));
      return this.result(lines.length ? 'I remember that ' + lines.join('; ') + '.' : 'Nothing yet! Teach me something.', []);
    }
    if ((m = n.match(/^forget (?:about |the phrase |what )?(.+?)(?: means)?$/))) {
      if (m[1] === 'everything') {
        this.phrases = {};
        this.rejected = {};
        this.facts = [];
        this.playerName = '';
        return this.result('Okay... my mind is a blank slate. Who are you again?', [{ type: 'sit' }], { mood: 'neutral' });
      }
      var k = core(m[1], this.name);
      if (this.phrases[k]) {
        delete this.phrases[k];
        return this.result('Forgotten! "' + k + '" doesn\'t mean anything to me now.', [], { forgot: k });
      }
      return this.result('I didn\'t know "' + m[1] + '" anyway!', [], { mood: 'neutral' });
    }
    if (/^(whats my name|who am i|do you know my name|do you remember me)$/.test(n)) {
      return this.result(this.playerName ? 'You\'re ' + this.playerName + ', my best friend!' : 'I don\'t know yet! Tell me: "my name is ..."', []);
    }
    return null;
  };

  // A phrase the player taught. Exact match first; loose (fuzzy) matching
  // runs last, after the built-ins, so it never steals a real command.
  Brain.prototype.taught = function (n, world, fuzzy) {
    var key = core(n, this.name);
    var t = this.phrases[key];
    var matched = key;
    if (!t && fuzzy) {
      var best = 0;
      var refused = this.rejected[key] || [];
      for (var k in this.phrases) {
        if (refused.indexOf(k) !== -1) continue;
        var s = similarity(key, k);
        if (s > best) { best = s; matched = k; }
      }
      if (best >= FUZZY_THRESHOLD) t = this.phrases[matched];
      if (t) this.lastGuess = { key: matched, heard: key };
    }
    if (!t) return null;
    var guessed = fuzzy && matched !== key;
    if (t.reply) {
      return this.result(this.fill(t.reply), [], { guessed: guessed });
    }
    var actions = t.actions.filter(function (a) { return available(a, world); });
    if (!actions.length) return this.result('I would, but I can\'t see one around here.', [], { mood: 'neutral', guessed: guessed });
    var say = this.ackFor(actions);
    if (guessed) say += ' (I think that\'s what "' + matched + '" means? Say "good job" or "wrong".)';
    return this.result(say, actions, { guessed: guessed });
  };

  Brain.prototype.command = function (raw, world) {
    var actions = parseCommands(raw, world, this.name);
    if (!actions) return null;
    return this.result(this.ackFor(actions), actions);
  };

  Brain.prototype.smallTalk = function (n) {
    var you = this.playerName ? ', ' + this.playerName : '';
    if (/^(hi|hello|hey|heya|howdy|yo|hiya|sup|whats up|good (morning|afternoon|evening))( there| friend)?$/.test(n)) {
      return this.result(this.pick(['Hi' + you + '!', 'Hello' + you + '!', 'Hey hey' + you + '!']), [{ type: 'jump' }]);
    }
    if (/\bhow are you\b|\bhow do you feel\b|\bhows it going\b|\bhow are things\b/.test(n)) {
      return this.result(this.pick(['I\'m great, thanks for asking!', 'Happy as can be!', 'Ready for an adventure!']), []);
    }
    if (/\b(whats|what is) your name\b|\bwho are you\b/.test(n)) {
      return this.result('I\'m ' + this.name + ', your companion!', []);
    }
    if (/\b(thanks|thank you|thx|ty)\b/.test(n)) {
      return this.result(this.pick(['You\'re welcome!', 'Any time!', 'Happy to help!']), []);
    }
    if (/\b(joke|make me laugh|something funny)\b/.test(n)) {
      return this.result(this.pick([
        'Why did the scarecrow win an award? He was outstanding in his field!',
        'What do you call a sleeping dinosaur? A dino-snore!',
        'Why don\'t skeletons fight? They don\'t have the guts.'
      ]), [{ type: 'dance' }]);
    }
    if (/\b(i love you|love you|youre (the best|great|awesome|cute))\b/.test(n)) {
      return this.result(this.pick(['Aww! You\'re the best too!', 'Best friends forever!']), [{ type: 'dance' }]);
    }
    if (/^(bye|goodbye|see you|see ya|later|good ?night)\b/.test(n)) {
      return this.result(this.pick(['Bye! I\'ll be right here.', 'See you soon!']), [{ type: 'sit' }]);
    }
    if (/^(help|what can you do|commands|what do you know how to do)$/.test(n)) {
      return this.result('I can follow you, stay, come here, fetch things ("get the apple"), go places ("go to the pond", "go left"), jump, dance, sit, explore and sleep. Teach me new words with "when I say heel, follow me", and tell me things to remember!', []);
    }
    return null;
  };

  // "{name}" in a taught reply becomes the player's name.
  Brain.prototype.fill = function (reply) {
    return reply.replace(/\{name\}/gi, this.playerName || 'friend');
  };

  Brain.prototype.toJSON = function () {
    return {
      version: SAVE_VERSION,
      name: this.name,
      playerName: this.playerName,
      phrases: this.phrases,
      rejected: this.rejected,
      facts: this.facts.slice()
    };
  };

  Brain.prototype.load = function (data) {
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch (e) { return false; }
    }
    if (!data || data.version !== SAVE_VERSION) return false;
    if (typeof data.name === 'string' && data.name) this.name = data.name;
    if (typeof data.playerName === 'string') this.playerName = data.playerName;
    if (data.phrases && typeof data.phrases === 'object') {
      this.phrases = {};
      for (var k in data.phrases) {
        var p = data.phrases[k];
        if (p && (typeof p.reply === 'string' || (Array.isArray(p.actions) && p.actions.every(validAction)))) this.phrases[k] = p;
      }
    }
    if (data.rejected && typeof data.rejected === 'object') {
      this.rejected = {};
      for (var r in data.rejected) if (Array.isArray(data.rejected[r])) this.rejected[r] = data.rejected[r].filter(function (x) { return typeof x === 'string'; });
    }
    if (Array.isArray(data.facts)) this.facts = data.facts.filter(function (f) { return typeof f === 'string'; }).slice(-MAX_FACTS);
    return true;
  };

  // ---- small helpers -------------------------------------------------------

  // The original-case text matching a normalized tail: keeps "Cody" capitalized.
  function afterPhrase(raw, normTail) {
    var w = normTail.split(' ').length;
    var rawWords = raw.replace(/[.!?]+$/, '').trim().split(/\s+/);
    return rawWords.slice(-w).join(' ');
  }

  function available(action, world) {
    if (action.type === 'fetch') return (world.items || []).indexOf(action.target) !== -1;
    if (action.type === 'goto' && DIRECTIONS.indexOf(action.target) === -1) return (world.places || []).indexOf(action.target) !== -1;
    return true;
  }

  function uniq(list) {
    return list.filter(function (x, i) { return list.indexOf(x) === i; });
  }

  function copyActions(actions) {
    return actions.map(function (a) { return a.target ? { type: a.type, target: a.target } : { type: a.type }; });
  }

  function validAction(a) {
    return a && ACTION_TYPES.indexOf(a.type) !== -1 && (a.target === undefined || typeof a.target === 'string');
  }

  var VERB = {
    follow: 'follow you', stay: 'stay put', come: 'come to you', jump: 'jump', dance: 'dance',
    sit: 'sit down', wander: 'go exploring', sleep: 'take a nap'
  };

  function describe(actions) {
    return actions.map(function (a) {
      if (a.type === 'fetch') return 'fetch the ' + a.target;
      if (a.type === 'goto') return DIRECTIONS.indexOf(a.target) !== -1 ? 'go ' + a.target : 'go to the ' + a.target;
      return VERB[a.type];
    }).join(', then ');
  }

  var api = {
    Brain: Brain,
    create: function (saved, opts) { return new Brain(saved, opts); },
    ACTION_TYPES: ACTION_TYPES,
    DIRECTIONS: DIRECTIONS,
    normalize: normalize,
    similarity: similarity,
    parseCommands: parseCommands,
    validAction: validAction,
    describe: describe
  };

  root.CompanionBrain = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
