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
  /*
   * Where an entry's own vocabulary appears, and how prominently.
   *
   * The lexicon gives every subject, scene and weather the list of words that
   * mean it, so this asks the lexicon rather than keeping a second copy of its
   * vocabulary here. Multi-word entries ("hot air balloon") are matched against
   * the whole line; single words against the token list, so that "ratepayer"
   * does not count as "rat".
   */
  function hits(entry, text) {
    if (!entry || !entry.words || !text) return [];
    var line = text.line, said = text.said, found = [];
    entry.words.forEach(function (w) {
      if (w.indexOf(' ') >= 0) {
        var at = line.indexOf(w);
        if (at >= 0) found.push({ word: w, at: at / Math.max(line.length, 1), phrase: true });
      } else {
        for (var i = 0; i < said.length; i++) {
          if (said[i] === w) found.push({ word: w, at: i / Math.max(said.length, 1), phrase: false });
        }
      }
    });
    return found;
  }

  /*
   * Which of the things an article mentions it is actually about.
   *
   * Taking the first entry that matched meant taking whichever came first in
   * the lexicon, which has nothing to do with the story: "Cathedral
   * restoration uncovers medieval cat carving" was illustrated with a cat,
   * because cats are listed well above temples. A picture beside a story has
   * to be about the story, so what matters is prominence — in the headline far
   * above in the body, early above late, often above once.
   */
  function rank(list, head, body) {
    var best = null;
    list.forEach(function (entry) {
      var inHead = hits(entry, head), inBody = hits(entry, body);
      if (!inHead.length && !inBody.length) return;
      var score = 0;
      inHead.forEach(function (h) {
        /* A headline is the story's own summary of itself, and it front-loads
         * what the story is about. */
        score += 10 + (1 - h.at) * 6 + (h.phrase ? 3 : 0);
      });
      inBody.forEach(function (h) {
        score += 1 + (1 - h.at) * 0.5 + (h.phrase ? 0.5 : 0);
      });
      if (!best || score > best.score) {
        best = {
          entry: entry, score: score,
          word: (inHead[0] || inBody[0]).word,
          inHeadline: inHead.length > 0,
          times: inHead.length + inBody.length
        };
      }
    });
    return best;
  }

  /*
   * What came in. A bare string is a headline; an object may carry the body
   * too, under whichever of the usual names a feed happens to use. The body is
   * worth reading because it is where a story says what it is about at length,
   * and because a headline alone often names nothing that can be drawn.
   */
  function piecesOf(text) {
    return { line: String(text || '').toLowerCase(), said: words(text) };
  }
  function readArticle(input) {
    if (!input) return null;
    var head, body, whole;
    if (typeof input === 'string') { head = input; body = ''; }
    else {
      head = input.headline || input.title || '';
      body = [input.standfirst, input.summary, input.description, input.body, input.text]
        .filter(function (v) { return typeof v === 'string'; }).join(' ');
    }
    if (!String(head).trim() && !String(body).trim()) return null;
    /* The seed is taken from headline and body together, so correcting a typo
     * in the copy gives the story a new picture — which is right, because it
     * is a new version of the story. */
    whole = String(head) + ' \u0000 ' + String(body);
    return {
      headline: whole, head: piecesOf(head), body: piecesOf(body),
      shown: String(head)
    };
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

  function aOrAn(word) {
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
    var piece = readArticle(headline);
    if (!piece) return null;
    var head = piece.head, body = piece.body;
    var whole = piece.headline;

    var subject = rank(LEX.SUBJECTS, head, body);
    var scene = rank(LEX.SCENES, head, body);
    var weather = rank(LEX.WEATHER, head, body);
    var time = rank(LEX.TIMES, head, body);

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
          : aOrAn(plain(subject)) + ' ' + plain(subject));
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

    var seed = seedOf(whole);
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
    /*
     * The painter fills in a subject of its own when it cannot find one. That
     * is a good default for somebody typing a prompt and the wrong one beside
     * a news story, because a picture next to an article is a claim about the
     * article — so a picture containing anything the article never mentioned
     * is not returned at all unless the caller asks for it.
     */
    var invented = !!(back.subject && !subject);
    if (invented && !(opts && opts.allowInvented)) return null;

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
      /* Whether the story said it in its own headline or only down in the
       * copy. A desk that wants pictures only for what a story leads with can
       * require this; one filling a page can take either. */
      fromHeadline: !!((subject && subject.inHeadline) || (scene && scene.inHeadline)),
      from: [subject, scene, weather, time]
        .filter(Boolean).map(function (m) { return m.word; })
    };
  }

  /* Every headline in a list that can be illustrated, in order, with the ones
   * that cannot left out — which is usually most of them. */
  function pickable(articles, opts) {
    var out = [];
    (articles || []).forEach(function (a) {
      var got = pictureFor(a, opts);
      if (got) { got.article = a; out.push(got); }
    });
    return out;
  }

  var API = { pictureFor: pictureFor, pickable: pickable, words: words, NEUTRAL: NEUTRAL };
  root.CodaHeadline = API;
  if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
