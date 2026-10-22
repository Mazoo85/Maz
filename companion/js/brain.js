/*
 * COMPANION — the brain
 * ---------------------
 * Pure logic (no DOM) so it runs in the browser AND in a headless test, and so
 * any game can use it: the game describes its character (name, personality,
 * and the actions its body can do) and the brain does the understanding.
 *
 * The brain hears one line of chat at a time and answers with what to say and
 * what to do:
 *
 *   brain.hear('go get the apple', world)
 *     -> { say: 'On it!', actions: [{ type: 'fetch', target: 'apple' }],
 *          understood: true, mood: 'happy', kind: 'command' }
 *
 * It is trainable. Out of the box it knows the character's commands and a bit
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
 * `world` tells it what exists right now, one list per kind of target the
 * character's actions use: { items: ['apple', 'gem'], places: ['tree'] }.
 * So "get the gem" only works while there is a gem.
 *
 * Everything it learns is in brain.toJSON(), for the game to save.
 *
 * Using it in another game: see "Use the brain in another game" in
 * companion/README.md. Without a character, it is Pip from the Companion game.
 *
 * Exposed as: window.CompanionBrain (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var SAVE_VERSION = 1;
  var MAX_FACTS = 50;
  var FUZZY_THRESHOLD = 0.6;

  var DIRECTIONS = ['left', 'right', 'up', 'down'];

  // The character from the Companion game, and the example for any other.
  //
  //   type     what the game's code receives, e.g. { type: 'fetch', target: 'apple' }
  //   words    what the player might say. When several actions' words are in a
  //            line, the longest match wins: "stop following" is stay, not follow.
  //   target   (optional) which list in `world` the target comes from
  //   also     (optional) targets that always exist, like directions
  //   replies  what it says when it starts; {target} is filled in
  //   does     how it describes the action: "I'll fetch the apple"
  //   doesAlso (optional) the same, for a target from `also`: "I'll go left"
  //   ability  (optional) how "help" describes it
  //   help     how Claude is told to use it (only used with smart chat)
  //
  //   reactions: which action (if any) it does when praised, scolded, greeted...
  var PIP = {
    name: 'Pip',
    personality: 'warm, playful, a little goofy, and loyal',
    setting: 'a small cozy adventure game set in a sunny meadow',
    actions: [
      {
        type: 'follow',
        words: ['follow', 'come with me', 'come along', 'lets go', 'tag along', 'stick with me', 'stay close', 'with me'],
        replies: ['Right behind you!', 'Lead the way!', 'Following!'],
        does: 'follow you',
        help: 'walk with the player'
      },
      {
        type: 'stay',
        words: ['stay', 'wait', 'stop', 'halt', 'hold on', 'hold up', 'freeze', 'dont move', 'stand still', 'stop following'],
        replies: ['Staying put.', 'I\'ll wait right here.', 'Okay, not moving.'],
        does: 'stay put',
        help: 'stop and wait where you are'
      },
      {
        type: 'come',
        words: ['come', 'come here', 'come back', 'over here', 'to me'],
        replies: ['Coming!', 'On my way!', 'Be right there!'],
        does: 'come to you',
        help: 'walk to the player'
      },
      {
        type: 'goto',
        words: ['go', 'walk', 'run', 'move', 'head'],
        target: 'places',
        also: DIRECTIONS,
        replies: ['Heading there now.', 'Off I go!', 'On my way.'],
        does: 'go to the {target}',
        doesAlso: 'go {target}',
        ability: 'go places ("go to the pond", "go left")',
        help: 'walk to a place, or left/right/up/down'
      },
      {
        type: 'fetch',
        words: ['get', 'fetch', 'grab', 'bring', 'pick up', 'collect', 'find'],
        target: 'items',
        replies: ['On it! One {target}, coming up.', 'I\'ll get the {target}!', 'Going for the {target}!'],
        does: 'fetch the {target}',
        ability: 'fetch things ("get the apple")',
        help: 'go pick up an item and bring it to the player'
      },
      { type: 'jump', words: ['jump', 'hop', 'leap'], replies: ['Hup!', 'Wheee!', 'Boing!'], does: 'jump', help: 'jump for joy' },
      { type: 'dance', words: ['dance', 'boogie', 'spin', 'twirl'], replies: ['Watch these moves!', 'Dance party!', 'Ta-da!'], does: 'dance', help: 'do a little dance' },
      { type: 'sit', words: ['sit', 'sit down', 'take a seat'], replies: ['Sitting.', 'Taking a seat.', 'Ahh, comfy.'], does: 'sit down', help: 'sit down' },
      {
        type: 'wander',
        words: ['explore', 'wander', 'look around', 'roam', 'go play', 'have fun'],
        replies: ['I\'ll have a look around!', 'Exploring!', 'Adventure time!'],
        does: 'go exploring',
        ability: 'explore',
        help: 'explore on your own'
      },
      {
        type: 'sleep',
        words: ['sleep', 'nap', 'rest', 'take a break', 'go to bed'],
        replies: ['Zzz... wake me if you need me.', 'Just a little nap...', 'Yawn... goodnight.'],
        does: 'take a nap',
        ability: 'sleep',
        help: 'take a nap'
      }
    ],
    reactions: { praised: 'jump', scolded: 'stay', greeted: 'jump', loved: 'dance', bye: 'sit', named: 'jump', renamed: 'dance', wiped: 'sit' }
  };

  // Words that carry no meaning for a command: "could you please jump" is "jump".
  var FILLER = /\b(please|pls|can you|could you|would you|will you|i want you to|i need you to|go ahead and|hey|ok|okay|now|buddy|pal|for me)\b/g;

  // ---- characters ----------------------------------------------------------

  // Fill in what a game left out, and check what it gave. Throws on a mistake,
  // so a typo in a game's character shows up at once rather than as a
  // companion that silently ignores a command.
  function character(spec) {
    spec = spec || PIP;
    if (!Array.isArray(spec.actions) || !spec.actions.length) throw new Error('character needs a list of actions');
    var seen = {};
    var actions = spec.actions.map(function (a) {
      if (!a || typeof a.type !== 'string' || !a.type) throw new Error('every action needs a type');
      if (seen[a.type]) throw new Error('two actions are both called "' + a.type + '"');
      seen[a.type] = true;
      if (!Array.isArray(a.words) || !a.words.length) throw new Error('action "' + a.type + '" needs some words');
      var words = a.words.map(normalize).filter(Boolean);
      return {
        type: a.type,
        words: words,
        patterns: words.map(function (w) { return { len: w.length, re: new RegExp('\\b' + escapeRe(w) + '\\b') }; }),
        target: a.target || null,
        also: Array.isArray(a.also) ? a.also.slice() : [],
        replies: Array.isArray(a.replies) && a.replies.length ? a.replies.slice() : ['Okay!'],
        does: a.does || a.type,
        doesAlso: a.doesAlso || a.does || a.type,
        ability: a.ability || (a.target ? (a.does || a.type).replace(/\s*(the )?\{target\}/, ' things') : (a.does || a.type)),
        help: a.help || a.does || a.type
      };
    });
    return {
      name: spec.name || 'Pip',
      personality: spec.personality || 'friendly and helpful',
      setting: spec.setting || 'a video game',
      actions: actions,
      reactions: spec.reactions || {}
    };
  }

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
    var line = ' ' + words(text).map(singular).join(' ') + ' ';
    for (var i = 0; i < names.length; i++) {
      var nw = words(names[i]).map(singular).join(' ');
      if (nw && line.indexOf(' ' + nw + ' ') !== -1) return names[i];
    }
    return null;
  }

  function capitalize(s) {
    return s.replace(/\b[a-z]/g, function (c) { return c.toUpperCase(); });
  }

  function uniq(list) {
    return list.filter(function (x, i) { return list.indexOf(x) === i; });
  }

  // ---- commands ------------------------------------------------------------

  // Every action whose words are in the line, longest match first.
  function matches(text, ch) {
    var found = [];
    ch.actions.forEach(function (a, order) {
      var len = 0;
      a.patterns.forEach(function (p) { if (p.len > len && p.re.test(text)) len = p.len; });
      if (len) found.push({ action: a, len: len, order: order });
    });
    return found.sort(function (x, y) { return y.len - x.len || x.order - y.order; });
  }

  // One command in a line, or null. An action that needs a target and has none
  // ("get over here" has no item) steps aside for the next match.
  function parseOne(text, world, ch) {
    var found = matches(text, ch);
    for (var i = 0; i < found.length; i++) {
      var a = found[i].action;
      if (!a.target) return { type: a.type };
      var t = findIn(text, a.also) || findIn(text, world[a.target] || []);
      if (t) return { type: a.type, target: t };
    }
    return null;
  }

  // Turn a line into a list of actions, or null if it has no command in it.
  // "jump and then dance" is two actions, done in order.
  function parseCommands(text, world, ch, name) {
    var parts = core(text, name).split(/\b(?:and then|then|and|after that)\b/);
    var actions = [];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i].trim();
      if (!p) continue;
      var a = parseOne(p, world || {}, ch);
      if (a) actions.push(a);
    }
    return actions.length ? actions : null;
  }

  // ---- the brain -----------------------------------------------------------

  // saved: what toJSON() gave (object or JSON text), or nothing for a fresh brain.
  // opts.character: the game's character (see PIP); opts.rand: for tests.
  function Brain(saved, opts) {
    opts = opts || {};
    this.character = character(opts.character);
    this.rand = opts.rand || Math.random;
    this.name = this.character.name;
    this.playerName = '';
    this.phrases = {};   // normalized phrase -> { phrase, actions?, reply? }
    this.facts = [];     // things the player asked it to remember
    this.pending = null; // the last phrase it didn't understand, waiting to be taught
    this.lastGuess = null; // a loosely-matched phrase, for "good job" / "wrong"
    this.rejected = {};  // heard phrase -> taught phrases it must not be guessed as
    if (saved) this.load(saved);
  }

  Brain.prototype.action = function (type) {
    var list = this.character.actions;
    for (var i = 0; i < list.length; i++) if (list[i].type === type) return list[i];
    return null;
  };

  Brain.prototype.pick = function (list) {
    return list[Math.floor(this.rand() * list.length) % list.length];
  };

  // `react` names a moment ('praised', 'greeted'...); the character decides
  // which of its actions, if any, goes with it.
  Brain.prototype.result = function (say, react, extra) {
    var type = react && this.character.reactions[react];
    var r = { say: say, actions: type && this.action(type) ? [{ type: type }] : [], understood: true, mood: 'happy' };
    if (extra) for (var k in extra) r[k] = extra[k];
    return r;
  };

  Brain.prototype.ackFor = function (actions) {
    var a = actions[0];
    var spec = this.action(a.type);
    return this.pick(spec.replies).replace(/\{target\}/g, a.target || '');
  };

  // "follow you, then fetch the apple": what a list of actions will do.
  Brain.prototype.describe = function (actions) {
    var self = this;
    return actions.map(function (a) {
      var spec = self.action(a.type);
      if (!spec) return a.type;
      var text = a.target && spec.also.indexOf(a.target) !== -1 ? spec.doesAlso : spec.does;
      return text.replace(/\{target\}/g, a.target || '');
    }).join(', then ');
  };

  // True if the game can do this action right now: a known type, and a
  // target that exists (or none, for actions that take none).
  Brain.prototype.canDo = function (a, world) {
    var spec = a && this.action(a.type);
    if (!spec) return false;
    if (!spec.target) return true;
    return typeof a.target === 'string' &&
      (spec.also.indexOf(a.target) !== -1 || ((world || {})[spec.target] || []).indexOf(a.target) !== -1);
  };

  Brain.prototype.abilities = function () {
    var list = this.character.actions.map(function (a) { return a.ability; });
    return list.length > 1 ? list.slice(0, -1).join(', ') + ' and ' + list[list.length - 1] : list[0];
  };

  // Hear one line from the player. world: the game's lists of targets.
  Brain.prototype.hear = function (text, world) {
    world = world || {};
    var raw = String(text || '').trim();
    var n = normalize(raw);
    var guess = this.lastGuess;
    this.lastGuess = null;
    if (!n) return this.result('...?', null, { mood: 'neutral', kind: 'chat' });
    // The same line without the companion's name: "hi Pip" is a plain "hi".
    var nn = normalize(n.replace(new RegExp('\\b' + escapeRe(normalize(this.name)) + '\\b', 'g'), ' '));

    // Tried in order; the first that answers wins. `kind` tells the game who
    // answered, so it can hand plain chat to the smarter AI when there is one.
    var self = this;
    var steps = [
      ['teach', function () { return self.teaching(raw, world); }],
      ['feedback', function () { return self.feedback(nn, guess); }],
      ['memory', function () { return self.memory(raw, n); }],
      ['taught', function () { return self.taught(n, world, false); }],
      ['command', function () { return self.command(raw, world); }],
      ['chat', function () { return self.smallTalk(nn); }],
      ['taught', function () { return self.taught(n, world, true); }],
      ['missing', function () { return self.missing(raw, world); }]
    ];
    for (var i = 0; i < steps.length; i++) {
      var r = steps[i][1]();
      if (!r) continue;
      if (!r.keepPending) this.pending = null;
      delete r.keepPending;
      r.kind = steps[i][0];
      return r;
    }

    this.pending = raw;
    var example = this.character.actions[0].words[0];
    return {
      say: 'Hmm, I don\'t know what "' + raw + '" means yet. Teach me! Say "it means ' + example + '", or "say hello back".',
      actions: [],
      understood: false,
      mood: 'confused',
      kind: 'unknown'
    };
  };

  // "when I say X, do Y", "X means Y", and "it means Y" right after it got lost.
  // Works on the original line (capitals kept, so a taught reply reads the way
  // it was typed); a comma is the clearest way to end the phrase being taught.
  Brain.prototype.teaching = function (raw, world) {
    var line = raw.replace(/[“”"]/g, '').trim();
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

    var actions = parseCommands(meaning, world, this.character, this.name) || this.taughtActions(meaning);
    if (!actions) {
      return this.result('I don\'t know how to "' + meaning + '" yet. I can ' + this.abilities() + '.',
        null, { mood: 'confused', keepPending: true });
    }
    var key = core(phrase, this.name);
    if (!key) return null;
    this.phrases[key] = { phrase: phrase, actions: copyActions(actions) };
    return this.result('Got it! When you say "' + phrase + '", I\'ll ' + this.describe(actions) + '.', null, { learned: key });
  };

  Brain.prototype.learnReply = function (phrase, reply) {
    var key = core(phrase, this.name);
    reply = reply.replace(/^["“]|["”]$/g, '').trim();
    if (!key || !reply) return null;
    this.phrases[key] = { phrase: phrase, reply: reply };
    return this.result('Got it! When you say "' + phrase + '", I\'ll say "' + reply + '".', null, { learned: key });
  };

  // A phrase already taught as a command can be used to teach another one:
  // "when I say heel, do the hop" after teaching "do the hop".
  Brain.prototype.taughtActions = function (meaning) {
    var t = this.phrases[core(meaning, this.name)];
    return t && t.actions ? t.actions : null;
  };

  // "good job" makes the last loose guess official; "no, wrong" stops it.
  Brain.prototype.feedback = function (n, guess) {
    var praise = /^(good|great|nice|well done|yes|yay|perfect|awesome|good job|good boy|good girl|attaboy|thats right|correct)( job| one| work| boy| girl)?$/.test(n);
    var scold = /^(no|nope|wrong|bad|not that|thats wrong|thats not it|thats not what i meant)( (thats )?(wrong|not it|not that|not what i meant|boy|girl))?$/.test(n);
    if (!praise && !scold) return null;
    if (scold && guess) {
      (this.rejected[guess.heard] = this.rejected[guess.heard] || []).push(guess.key);
      return this.result('Oops, sorry! "' + guess.heard + '" isn\'t "' + guess.key + '" then. Teach me what it means if you like.', 'scolded', { mood: 'sad' });
    }
    if (praise && guess && this.phrases[guess.key]) {
      // Make the guess official: next time it's an exact match.
      var t = this.phrases[guess.key];
      if (!this.phrases[guess.heard]) this.phrases[guess.heard] = t.reply ? { phrase: guess.heard, reply: t.reply } : { phrase: guess.heard, actions: copyActions(t.actions) };
      return this.result('Yay! I\'ll remember that "' + guess.heard + '" means the same thing.', 'praised', { learned: guess.heard });
    }
    if (praise) return this.result(this.pick(['Thanks!', 'Hehe, thank you!', 'I try my best!']), 'praised');
    return this.result(this.pick(['Sorry...', 'Oh no, sorry!', 'I\'ll do better.']), 'scolded', { mood: 'sad' });
  };

  Brain.prototype.memory = function (raw, n) {
    var m;
    if ((m = n.match(/^(?:my name is|call me|im called|you can call me) (.+)$/))) {
      this.playerName = capitalize(afterPhrase(raw, m[1]));
      return this.result('Nice to meet you, ' + this.playerName + '! I\'ll remember that.', 'named');
    }
    if ((m = n.match(/^(?:your name is|ill call you|i will call you|call yourself|youre called|your new name is) (.+)$/))) {
      this.name = capitalize(afterPhrase(raw, m[1]));
      return this.result('I love it! From now on I\'m ' + this.name + '.', 'renamed');
    }
    if ((m = n.match(/^(?:please )?(?:remember|dont forget|never forget)(?: that)? (.+)$/))) {
      var fact = afterPhrase(raw, m[1]);
      this.facts.push(fact);
      if (this.facts.length > MAX_FACTS) this.facts.shift();
      return this.result('I\'ll remember that ' + fact + '.');
    }
    if (/^(what do you (remember|know)( about me)?|what have you learned|what did i teach you|what do you know)$/.test(n)) {
      var lines = [];
      if (this.playerName) lines.push('your name is ' + this.playerName);
      for (var i = 0; i < this.facts.length; i++) lines.push(this.facts[i]);
      var keys = Object.keys(this.phrases);
      if (keys.length) lines.push('I know ' + keys.length + ' phrase' + (keys.length === 1 ? '' : 's') + ' you taught me: ' + keys.map(function (k) { return '"' + k + '"'; }).join(', '));
      return this.result(lines.length ? 'I remember that ' + lines.join('; ') + '.' : 'Nothing yet! Teach me something.');
    }
    if ((m = n.match(/^forget (?:about |the phrase |what )?(.+?)(?: means)?$/))) {
      if (m[1] === 'everything') {
        this.phrases = {};
        this.rejected = {};
        this.facts = [];
        this.playerName = '';
        return this.result('Okay... my mind is a blank slate. Who are you again?', 'wiped', { mood: 'neutral' });
      }
      var k = core(m[1], this.name);
      if (this.phrases[k]) {
        delete this.phrases[k];
        return this.result('Forgotten! "' + k + '" doesn\'t mean anything to me now.', null, { forgot: k });
      }
      return this.result('I didn\'t know "' + m[1] + '" anyway!', null, { mood: 'neutral' });
    }
    if (/^(whats my name|who am i|do you know my name|do you remember me)$/.test(n)) {
      return this.result(this.playerName ? 'You\'re ' + this.playerName + ', my best friend!' : 'I don\'t know yet! Tell me: "my name is ..."');
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
      return this.result(this.fill(t.reply), null, { guessed: guessed });
    }
    var self = this;
    var actions = t.actions.filter(function (a) { return self.canDo(a, world); });
    if (!actions.length) return this.result('I would, but I can\'t see one around here.', null, { mood: 'neutral', guessed: guessed });
    var say = this.ackFor(actions);
    if (guessed) say += ' (I think that\'s what "' + matched + '" means? Say "good job" or "wrong".)';
    return this.result(say, null, { actions: actions, guessed: guessed });
  };

  Brain.prototype.command = function (raw, world) {
    var actions = parseCommands(raw, world, this.character, this.name);
    if (!actions) return null;
    return this.result(this.ackFor(actions), null, { actions: actions });
  };

  // A command whose target isn't there ("get the banana"): say what is.
  Brain.prototype.missing = function (raw, world) {
    var found = matches(core(raw, this.name), this.character).filter(function (f) { return f.action.target; });
    if (!found.length) return null;
    var there = uniq(world[found[0].action.target] || []);
    return this.result('I can\'t find that around here.' + (there.length ? ' I can see: ' + there.join(', ') + '.' : ''), null, { mood: 'neutral' });
  };

  Brain.prototype.smallTalk = function (n) {
    var you = this.playerName ? ', ' + this.playerName : '';
    if (/^(hi|hello|hey|heya|howdy|yo|hiya|sup|whats up|good (morning|afternoon|evening))( there| friend)?$/.test(n)) {
      return this.result(this.pick(['Hi' + you + '!', 'Hello' + you + '!', 'Hey hey' + you + '!']), 'greeted');
    }
    if (/\bhow are you\b|\bhow do you feel\b|\bhows it going\b|\bhow are things\b/.test(n)) {
      return this.result(this.pick(['I\'m great, thanks for asking!', 'Happy as can be!', 'Ready for an adventure!']));
    }
    if (/\b(whats|what is) your name\b|\bwho are you\b/.test(n)) {
      return this.result('I\'m ' + this.name + ', your companion!');
    }
    if (/\b(thanks|thank you|thx|ty)\b/.test(n)) {
      return this.result(this.pick(['You\'re welcome!', 'Any time!', 'Happy to help!']));
    }
    if (/\b(joke|make me laugh|something funny)\b/.test(n)) {
      return this.result(this.pick([
        'Why did the scarecrow win an award? He was outstanding in his field!',
        'What do you call a sleeping dinosaur? A dino-snore!',
        'Why don\'t skeletons fight? They don\'t have the guts.'
      ]), 'loved');
    }
    if (/\b(i love you|love you|youre (the best|great|awesome|cute))\b/.test(n)) {
      return this.result(this.pick(['Aww! You\'re the best too!', 'Best friends forever!']), 'loved');
    }
    if (/^(bye|goodbye|see you|see ya|later|good ?night)\b/.test(n)) {
      return this.result(this.pick(['Bye! I\'ll be right here.', 'See you soon!']), 'bye');
    }
    if (/^(help|what can you do|commands|what do you know how to do)$/.test(n)) {
      var first = this.character.actions[0];
      return this.result('I can ' + this.abilities() + '. Teach me new words with "when I say heel, ' + first.words[0] + '", and tell me things to remember!');
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

  // Lessons for actions this character doesn't have (a brain saved by another
  // game, say) are left out rather than loaded broken.
  Brain.prototype.load = function (data) {
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch (e) { return false; }
    }
    if (!data || data.version !== SAVE_VERSION) return false;
    var self = this;
    function known(a) {
      return a && self.action(a.type) && (a.target === undefined || typeof a.target === 'string');
    }
    if (typeof data.name === 'string' && data.name) this.name = data.name;
    if (typeof data.playerName === 'string') this.playerName = data.playerName;
    if (data.phrases && typeof data.phrases === 'object') {
      this.phrases = {};
      for (var k in data.phrases) {
        var p = data.phrases[k];
        if (p && (typeof p.reply === 'string' || (Array.isArray(p.actions) && p.actions.length && p.actions.every(known)))) this.phrases[k] = p;
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

  function copyActions(actions) {
    return actions.map(function (a) { return a.target ? { type: a.type, target: a.target } : { type: a.type }; });
  }

  var api = {
    Brain: Brain,
    create: function (saved, opts) { return new Brain(saved, opts); },
    PIP: PIP,
    DIRECTIONS: DIRECTIONS,
    character: character,
    normalize: normalize,
    similarity: similarity
  };

  root.CompanionBrain = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
