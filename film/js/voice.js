/*
 * SCRIPT FORGE — how a person talks.
 * ----------------------------------
 * The dialogue bank is written in one neutral voice, because an exchange has to
 * work for whoever ends up saying it. The cost of that is real: two characters
 * in the same film sound like the same person taking both sides, which is the
 * single loudest tell that nobody wrote the scene.
 *
 * This gives every character four dials, derived from their name and their role
 * so they are the same in every rebuild of the same film, and runs every line
 * they say through them:
 *
 *   formal   — contracts ("I'm") or expands ("I am")
 *   terse    — cuts the trailing clause off a long sentence
 *   hedging  — softens a flat statement, or strips a softener already there
 *   warmth   — uses the other person's name, and agrees out loud
 *
 * The rules are deliberately conservative. A transform that mangles English is
 * far worse than no transform at all, so each one is table-driven or refuses to
 * fire unless the sentence has the exact shape it needs. `film/tests` runs the
 * whole dialogue bank through every voice this can produce and demands the
 * result still be a well-formed line.
 *
 * Works on TEMPLATES, before parse.js's slots are filled: {HERO} and {OTHER}
 * are still in the text, which is what lets a warm character address the person
 * they are actually talking to.
 *
 * Exposed as window.FilmVoice (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* Roles that carry a register with them. A police officer and a fourteen year
   * old do not speak the same way whatever the dice say, so the role nudges the
   * dials rather than the seed deciding alone. */
  var FORMAL_ROLES = /\b(?:doctor|nurse|surgeon|officer|detective|inspector|lawyer|judge|teacher|professor|priest|minister|soldier|captain|sergeant|clerk|banker|accountant|undertaker|librarian)\b/;
  var INFORMAL_ROLES = /\b(?:kid|boy|girl|teenager|student|drifter|busker|bartender|roadie|dealer|thief|twin|apprentice|intern)\b/;
  var GUARDED_ROLES = /\b(?:stranger|witness|widow|widower|veteran|survivor|smuggler|spy|fugitive|hermit)\b/;

  /* Both directions of the same table, so a voice can be moved either way and a
   * line that arrives already contracted can still be made formal. Longest
   * first: "I'd have" has to match before "I'd". */
  var CONTRACTIONS = [
    ['I am', "I'm"], ['I will', "I'll"], ['I have', "I've"], ['I would', "I'd"],
    ['you are', "you're"], ['you will', "you'll"], ['you have', "you've"], ['you would', "you'd"],
    ['we are', "we're"], ['we will', "we'll"], ['we have', "we've"], ['we would', "we'd"],
    ['they are', "they're"], ['they will', "they'll"], ['they have', "they've"],
    ['he is', "he's"], ['she is', "she's"], ['it is', "it's"], ['that is', "that's"],
    ['there is', "there's"], ['who is', "who's"], ['what is', "what's"],
    ['do not', "don't"], ['does not', "doesn't"], ['did not', "didn't"],
    ['is not', "isn't"], ['are not', "aren't"], ['was not', "wasn't"], ['were not', "weren't"],
    ['can not', "can't"], ['cannot', "can't"], ['could not', "couldn't"],
    ['would not', "wouldn't"], ['should not', "shouldn't"], ['will not', "won't"],
    ['have not', "haven't"], ['has not', "hasn't"], ['had not', "hadn't"]
  ];

  /* Softeners a hedging character reaches for, and the same list read backwards
   * for a blunt one, who has them taken away. */
  var HEDGES = ['Maybe ', 'I think ', 'Look, ', 'I mean, ', 'Probably '];
  var HEDGE_PATTERN = /^(?:Maybe|I think|Look,|I mean,|Probably|Perhaps|Sort of|Kind of|I guess|I suppose)[, ]+/;

  /* What a warm character says instead of nothing. */
  var AGREEMENTS = ['Right.', 'Sure.', 'Okay.', 'Yeah.'];

  function clamp01(v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

  /* A character's voice, from their name and role. Deterministic: the same
   * person in the same film talks the same way every time it is rebuilt, which
   * is the whole point — a voice that re-rolled per scene would be worse than
   * no voice at all. */
  function voiceFor(name, role, seed) {
    var key = 'voice:' + String(name) + ':' + String(role || '');
    var rng = PARSE.makeRng((PARSE.hashText(key) ^ ((seed || 0) >>> 0)) >>> 0);
    var formal = rng();
    var terse = rng();
    var hedging = rng();
    var warmth = rng();

    var r = String(role || '').toLowerCase();
    if (FORMAL_ROLES.test(r)) { formal = clamp01(formal * 0.4 + 0.6); terse = clamp01(terse * 0.7 + 0.2); }
    if (INFORMAL_ROLES.test(r)) { formal = clamp01(formal * 0.4); warmth = clamp01(warmth * 0.5 + 0.4); }
    if (GUARDED_ROLES.test(r)) { terse = clamp01(terse * 0.35 + 0.65); warmth = clamp01(warmth * 0.4); }

    return {
      name: String(name),
      formal: formal,
      terse: terse,
      hedging: hedging,
      warmth: warmth
    };
  }

  /* A one-line description of a voice, for the "why" panel and for the tests to
   * print when they fail. */
  function describe(v) {
    var bits = [];
    bits.push(v.formal > 0.62 ? 'speaks in full words' : (v.formal < 0.34 ? 'clips their words' : 'speaks plainly'));
    if (v.terse > 0.66) bits.push('stops at the point');
    if (v.hedging > 0.62) bits.push('softens things');
    else if (v.hedging < 0.3) bits.push('says it flat');
    if (v.warmth > 0.62) bits.push('uses your name');

    // Two people can land on the same side of every threshold and get the same
    // sentence, which makes the description useless exactly where it matters:
    // in the panel that claims they sound different. So whatever else is true,
    // name the dial this person is furthest along.
    var dials = [
      ['formal', v.formal, 'careful with words', 'blunt'],
      ['terse', v.terse, 'brief', 'takes their time'],
      ['hedging', v.hedging, 'hesitant', 'certain'],
      ['warmth', v.warmth, 'warm', 'distant']
    ];
    var strongest = dials[0];
    var furthest = -1;
    dials.forEach(function (d) {
      var away = Math.abs(d[1] - 0.5);
      if (away > furthest) { furthest = away; strongest = d; }
    });
    return bits.join(', ') + '; above all, ' +
      (strongest[1] >= 0.5 ? strongest[2] : strongest[3]);
  }

  var DIALS = ['formal', 'terse', 'hedging', 'warmth'];

  /* Push a second voice away from the first.
   *
   * The dials come from the name and the role, and roles can collapse two people
   * onto the same register: a night nurse is a FORMAL_ROLE and a stranger is a
   * GUARDED_ROLE, so one film gave both its leads formal 0.95 / 0.93 and terse
   * 0.72 / 0.66 -- which is two people who sound the same, in the feature whose
   * whole purpose is that they do not.
   *
   * So the supporting character is moved. The dial they are already furthest
   * apart on is pushed further, which keeps whatever the role earned and only
   * sharpens it; nobody is turned into somebody else to satisfy a number.
   */
  var MIN_APART = 0.3;

  function contrast(lead, second) {
    var out = { name: second.name };
    DIALS.forEach(function (k) { out[k] = second[k]; });

    var widest = DIALS[0];
    var apart = -1;
    DIALS.forEach(function (k) {
      var gap = Math.abs(lead[k] - second[k]);
      if (gap > apart) { apart = gap; widest = k; }
    });
    if (apart >= MIN_APART) return out;

    // Away from the lead, and past the threshold. Clamped, so a lead already at
    // an extreme does not push the other off the end of the scale -- in that
    // case they land at the far end, which is as far apart as the dial goes.
    var target = lead[widest] >= 0.5
      ? Math.min(lead[widest], 1) - MIN_APART
      : Math.max(lead[widest], 0) + MIN_APART;
    out[widest] = clamp01(target);
    return out;
  }

  /* ------------------------------------------------------------- transforms */

  function swapAll(text, from, to) {
    var out = text;
    var i;
    for (i = 0; i < CONTRACTIONS.length; i++) {
      var a = CONTRACTIONS[i][from];
      var b = CONTRACTIONS[i][to];
      // Word boundaries on both sides, and the capitalised form too, so
      // "It is" at the head of a sentence moves with the rest.
      out = out.replace(new RegExp('\\b' + a.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', 'g'), b);
      var A = a.charAt(0).toUpperCase() + a.slice(1);
      var B = b.charAt(0).toUpperCase() + b.slice(1);
      out = out.replace(new RegExp('\\b' + A.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', 'g'), B);
    }
    return out;
  }

  /* Cut a trailing subordinate clause: "I'm always here early, same as ever."
   * becomes "I'm always here early." Only fires when the head is a sentence in
   * its own right (four words or more) and the tail is not a quoted name or a
   * slot, because "Take it, {HERO}." is an address and cutting it is rude
   * rather than terse. */
  function trimTail(line) {
    var m = /^(.{8,}?),\s+([^,]{3,})([.?!])$/.exec(line);
    if (!m) return line;
    var head = m[1];
    var tail = m[2];
    if (head.split(/\s+/).length < 4) return line;
    if (/\{[A-Z_]+\}/.test(tail)) return line;
    if (/^(?:and|or|but|then|so)\b/i.test(tail)) return line;
    return head + m[3];
  }

  function stripHedge(line) {
    var out = line.replace(HEDGE_PATTERN, '');
    if (!out) return line;
    return out.charAt(0).toUpperCase() + out.slice(1);
  }

  function addHedge(line, pick) {
    if (HEDGE_PATTERN.test(line)) return line;
    if (line.split(/\s+/).length < 3) return line;          // "No." stays "No."
    // One sentence only. A softener reaches the sentence it is attached to and
    // no further, so "Probably don't. Whatever you're about to do — don't."
    // softens the first half and leaves the second half contradicting it.
    if (/[.?!—…:;]/.test(line.slice(0, -1))) return line;
    var hedge = HEDGES[pick % HEDGES.length];
    var body = line.charAt(0).toLowerCase() + line.slice(1);
    // "I" must not be lowercased into "i".
    if (/^i\b/.test(body) && /^I\b/.test(line)) body = line;
    return hedge + body;
  }

  /* "You're here early." -> "You're here early, {OTHER}." Addressing someone by
   * name is the cheapest thing that makes two people sound like two people who
   * know each other. */
  function address(line, listenerSlot) {
    if (!listenerSlot) return line;
    if (line.indexOf(listenerSlot) !== -1) return line;
    var m = /^(.*[^\s])([.?!])$/.exec(line);
    if (!m) return line;
    var body = m[1];
    if (body.split(/\s+/).length < 3) return line;
    if (/,\s*$/.test(body)) return line;
    // One sentence only. "Don't. Whatever you're about to do — don't." ends on a
    // repeat for emphasis, and hanging a name off the end of it kills the beat
    // the repeat was there for.
    if (/[.?!—…:;]/.test(body)) return line;
    return body + ', ' + listenerSlot + m[2];
  }

  /* --------------------------------------------------------------- speaking */

  /* One line, in one character's mouth.
   *
   * `at` is the line's position in the exchange; it is what keeps a warm
   * character from putting your name on the end of every single sentence, which
   * reads less like warmth and more like a hostage video.
   */
  function speak(line, voice, opts) {
    opts = opts || {};
    var at = opts.at || 0;
    var text = String(line);
    if (!text) return text;
    if (!voice) return text;

    var salt = (PARSE.hashText(text) + at) >>> 0;

    // 1. Contractions. The middle of the range is left alone: most people are
    //    neither formal nor clipped, and a film where both characters are at an
    //    extreme sounds like a sketch.
    if (voice.formal > 0.62) text = swapAll(text, 1, 0);
    else if (voice.formal < 0.34) text = swapAll(text, 0, 1);

    // 2. Trailing clause, for someone who stops at the point.
    if (voice.terse > 0.66) text = trimTail(text);

    // 3. Softeners, in or out.
    var softened = false;
    if (voice.hedging > 0.62 && (salt % 3) !== 0 && !/\?$/.test(text)) {
      var hedged = addHedge(text, salt);
      softened = hedged !== text;
      text = hedged;
    } else if (voice.hedging < 0.3) {
      text = stripHedge(text);
    }

    // 4. The other person's name, now and then — but never on a line that was
    //    just softened. "I mean, that is not how it works, {OTHER}." is two
    //    mannerisms fighting over one sentence, and reads as neither.
    if (!softened && voice.warmth > 0.62 && (salt % 4) === 0) {
      text = address(text, opts.listenerSlot);
    }

    return text;
  }

  /* An agreement noise a warm character can be given instead of silence. Used
   * by the subtext pass; kept here because it is a property of the voice. */
  function agreement(voice, salt) {
    if (!voice || voice.warmth <= 0.5) return null;
    return AGREEMENTS[(salt >>> 0) % AGREEMENTS.length];
  }

  var API = {
    DIALS: DIALS,
    MIN_APART: MIN_APART,
    contrast: contrast,
    voiceFor: voiceFor,
    describe: describe,
    speak: speak,
    agreement: agreement,
    // exported for the tests, which check the pieces as well as the whole
    trimTail: trimTail,
    addHedge: addHedge,
    stripHedge: stripHedge,
    address: address,
    CONTRACTIONS: CONTRACTIONS,
    HEDGES: HEDGES
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmVoice = API;
})(typeof window !== 'undefined' ? window : globalThis);
