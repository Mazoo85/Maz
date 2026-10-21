/*
 * CODA PICS — a picture for a headline.
 * -------------------------------------
 * The painter will make a picture out of any sentence at all, and that is
 * exactly wrong for a newsroom. Handed "Teachers vote to accept pay offer" it
 * recognises nothing, rolls the dice, and returns a bear in a canyon. Handed
 * "Shares fall as central bank holds rates" it returns a mountain peak in
 * space. A picture beside a story is a claim about that story, and an invented
 * one is a false claim — so the first job here is knowing when to say no.
 *
 * What this adds on top of CodaPrompt:
 *
 *   It checks that whatever the painter chose is actually *in* the headline,
 *   by asking the lexicon which words stand for the thing it picked and
 *   looking for one of them. Anything ungrounded is dropped rather than drawn.
 *
 *   It builds a clean prompt from only the grounded pieces, so the picture
 *   contains what the story mentioned and nothing else.
 *
 *   It keeps the hour and the weather neutral unless the headline names them.
 *   This one is an editorial decision rather than a technical one: a storm
 *   over a story about interest rates, or midnight over a school closure, is
 *   the picture editorialising, and it would do it by accident on a dice roll.
 *
 *   It seeds from the headline itself, so one story always gets one picture —
 *   the same on every reload, on every device, for every reader.
 *
 * No drawing happens here and nothing is fetched. Data in, a prompt out.
 *
 * Exposed as window.CodaHeadline (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var LEX = root.CodaLexicon ||
    (typeof require !== 'undefined' ? require('./lexicon.js') : null);
  var PROMPT = root.CodaPrompt ||
    (typeof require !== 'undefined' ? require('./prompt.js') : null);

  /* The hour a picture gets when the story does not name one. Daylight,
   * deliberately: every other choice says something about the story. */
  var NEUTRAL = 'in daylight';

  /* Places with no sky over them, where an hour is not a thing that exists.
   * "deep space in daylight" is the adapter talking rather than the story. */
  var NO_SKY = { space: true, cave: true };

  function words(text) {
    return String(text || '')
      .toLowerCase()
      .replace(/[‘’]/g, "'")
      .replace(/[^a-z0-9'\- ]+/g, ' ')
      .split(/\s+/)
      .filter(Boolean);
  }

  /*
   * Is this entry's own vocabulary present in the headline?
   *
   * The lexicon gives every subject, scene and weather a list of the words
   * that mean it, so this asks the lexicon rather than keeping a second copy
   * of its vocabulary here. Multi-word entries ("hot air balloon") are checked
   * against the whole line; single words against the token list, so that
   * "ratepayer" does not count as "rat".
   */
  function grounded(entry, said, line) {
    if (!entry || !entry.words) return null;
    for (var i = 0; i < entry.words.length; i++) {
      var w = entry.words[i];
      if (w.indexOf(' ') >= 0) {
        if (line.indexOf(w) >= 0) return w;
      } else if (said.indexOf(w) >= 0) {
        return w;
      }
    }
    return null;
  }

  function seedOf(headline) {
    return PROMPT.hash(String(headline)) % 1000000;
  }

  /*
   * The lexicon's own first word for a thing, rather than the one the headline
   * happened to use. The headline's word is whatever grammar the sentence
   * needed — "Wolves return" gave "a wolves" — and the first word in the list
   * is the plain singular the painter draws. The headline's wording is kept in
   * `from` for anyone who wants to see what matched.
   */
  function plain(match) {
    var list = match.entry.words || [];
    return list.length ? list[0] : match.word;
  }

  function article(word) {
    return /^[aeiou]/i.test(word) ? 'an' : 'a';
  }

  function find(list, said, line) {
    for (var i = 0; i < list.length; i++) {
      var hit = grounded(list[i], said, line);
      if (hit) return { entry: list[i], word: hit };
    }
    return null;
  }

  /*
   * What to draw for a headline, or nothing.
   *
   * Returns null when the story has nothing depictable in it, which is the
   * common case for politics, money and most of the front page. A news app
   * should show no picture at all rather than a wrong one.
   */
  function pictureFor(headline, opts) {
    if (!LEX || !PROMPT) return null;
    var line = String(headline || '').toLowerCase();
    var said = words(headline);
    if (!said.length) return null;

    var subject = find(LEX.SUBJECTS, said, line);
    var scene = find(LEX.SCENES, said, line);
    var weather = find(LEX.WEATHER, said, line);
    var time = find(LEX.TIMES, said, line);

    /* Nothing in the headline the painter can draw. */
    if (!subject && !scene) return null;

    /*
     * Built from the words the headline used, not from the lexicon's labels.
     *
     * The labels read better and they round-trip badly. "deep space" is the
     * label for space and contains "deep", which is one of the ocean's own
     * words — so the prompt came back parsed as an ocean, and a story about a
     * telescope was illustrated with the sea. The word the headline actually
     * said cannot do that, because it is the word that matched.
     */
    /*
     * Two goes at the wording, and the second only if the first fails.
     *
     * The lexicon's labels read best — "a lighthouse", "the mountains", "deep
     * space" — but a label is prose and prose can collide: "deep space"
     * contains "deep", which is one of the ocean's own words, so that prompt
     * came back parsed as the sea and a story about a telescope was
     * illustrated with water. The plain first word of the entry cannot do that
     * because it is the word that matched in the first place, but it reads
     * worse and sometimes gets the number wrong.
     *
     * So: write it well, read it back the way the painter will, and only drop
     * to the blunt wording if the good one came back meaning something else.
     */
    function build(useLabel) {
      var bits = [];
      if (subject) {
        bits.push(useLabel && subject.entry.label
          ? subject.entry.label
          : article(plain(subject)) + ' ' + plain(subject));
      }
      if (scene) {
        var where = useLabel && scene.entry.label ? scene.entry.label : 'the ' + plain(scene);
        bits.push((scene.entry.prep || 'in') + ' ' + where);
      }
      var roofed = scene && NO_SKY[scene.entry.id];
      if (time) bits.push(time.entry.label);
      else if (!roofed) bits.push(NEUTRAL);
      /*
       * The weather labels already carry their own preposition — "in the
       * rain", "under an aurora" — so putting one in front gives "in in a
       * storm". The two that do not ("clear", "clouded") are adjectives, and
       * the only one worth saying is the one the painter does by default
       * anyway, so they are left out rather than bent into a phrase.
       */
      if (weather && !roofed && /^(in|under|at) /.test(weather.entry.label || '')) {
        bits.push(weather.entry.label);
      }
      return bits.join(' ').replace(/\s+/g, ' ').trim();
    }

    var seed = seedOf(headline);
    function reads(text) {
      var back = PROMPT.parse(text, { seed: seed, style: 'auto' });
      return {
        back: back,
        faithful:
          (!subject || (back.subject && back.subject.id === subject.entry.id)) &&
          (!scene || (back.scene && back.scene.id === scene.entry.id))
      };
    }

    var prompt = build(true);
    var got = reads(prompt);
    if (!got.faithful) {
      var blunt = build(false);
      var second = reads(blunt);
      if (second.faithful) { prompt = blunt; got = second; }
    }
    var back = got.back;
    var faithful = got.faithful;
    /* The painter fills in a subject of its own when it cannot find one, which
     * is how a storm warning for the coast acquired a swan. Not a failure —
     * the picture is still worth having — but a caller that wants only what
     * the story paid for can read this and show nothing otherwise. */
    var invented = !!(back.subject && !subject);

    return {
      prompt: prompt,
      faithful: faithful,
      invented: invented,
      paints: {
        subject: back.subject ? back.subject.id : null,
        scene: back.scene ? back.scene.id : null,
        weather: back.weather
      },
      /* One story, one picture: the same on every reload and every device. */
      seed: seed,
      subject: subject ? subject.entry.id : null,
      scene: scene ? scene.entry.id : null,
      /* How much of the picture the headline actually paid for. A caller may
       * want to show only the sure ones. */
      standing: subject && scene ? 'both' : subject ? 'subject' : 'scene',
      from: [subject, scene, weather, time]
        .filter(Boolean).map(function (m) { return m.word; })
    };
  }

  /* Every headline in a list that can be illustrated, in order, with the ones
   * that cannot left out — which is usually most of them. */
  function pickable(headlines) {
    var out = [];
    (headlines || []).forEach(function (h) {
      var got = pictureFor(h);
      if (got) { got.headline = h; out.push(got); }
    });
    return out;
  }

  var API = { pictureFor: pictureFor, pickable: pickable, words: words, NEUTRAL: NEUTRAL };
  root.CodaHeadline = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
