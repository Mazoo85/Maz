/*
 * SCRIPT FORGE — the writer.
 * --------------------------
 * Takes a premise (from parse.js) and builds an actual short film out of it:
 * scenes on a beat spine, slug lines, action, dialogue, a shot list per scene
 * and an honest runtime estimate.
 *
 * A script is a flat list of screenplay *elements*, which is all any of the
 * exporters need:
 *
 *   { type: 'scene_heading' | 'action' | 'character' | 'parenthetical'
 *           | 'dialogue' | 'transition', text: '...' }
 *
 * Exposed as window.FilmWriter (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var LEX = root.FILM_LEXICON || (typeof require !== 'undefined' ? require('./lexicon.js') : {});
  var DLG = root.FILM_DIALOGUE || (typeof require !== 'undefined' ? require('./dialogue.js') : {});
  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* Ages have to match the part. A script that introduces "ALEX (40s), a kid"
   * has told the reader nothing and lost them at the same time. */
  var AGES = ['late 20s', '30s', 'late 30s', '40s', '50s'];
  var YOUNG_AGES = ['9', '11', '12', '14', '16', '17'];
  var OLD_AGES = ['60s', 'late 60s', '70s', 'late 70s'];
  var YOUNG_ROLES = /\b(?:kid|boy|girl|teenager|student|babysitter|son|daughter|intern|twin)\b/;
  var OLD_ROLES = /\b(?:grandmother|grandfather|old man|old woman|widow|widower|veteran|retired)\b/;

  function ageFor(role, rng) {
    if (YOUNG_ROLES.test(role)) return pick(YOUNG_AGES, rng);
    if (OLD_ROLES.test(role)) return pick(OLD_AGES, rng);
    return pick(AGES, rng);
  }

  var INTROS = [
    'who has stopped expecting company',
    'dressed for a day that already went wrong',
    'wearing yesterday, and not hiding it',
    'awake longer than anyone should be',
    'better at this than the job deserves',
    'doing sums in their head and losing',
    'with one good coat, wearing it',
    'who answers questions a beat too late',
    'holding it together in a way you can see the seams of',
    'who has learned to take up very little room'
  ];

  /* A pool you can draw from without repeating until it runs dry. */
  function pool(items, rng) {
    var left = items.slice();
    var last = null;
    return function () {
      if (!left.length) {
        left = items.filter(function (x) { return x !== last; });
        if (!left.length) left = items.slice();
      }
      var i = Math.floor(rng() * left.length) % left.length;
      last = left.splice(i, 1)[0];
      return last;
    };
  }

  function capitalize(s) {
    return s.charAt(0).toUpperCase() + s.slice(1);
  }

  /* Time of day moves on for the closing scene, so the film reads as a night
   * that ended rather than a night that repeated. */
  var NEXT_TIME = { NIGHT: 'DAWN', DAWN: 'DAY', DAY: 'DUSK', DUSK: 'NIGHT' };

  /* The pool draws are functions, not values, because String.replace evaluates
   * its arguments whether or not the pattern matches: passing ctx.nextDetail()
   * directly spent a detail and two sounds on *every* line in the film, which
   * emptied banks that hold a dozen entries and put the same sound cue on
   * screen twice in a row. */
  function fill(text, ctx) {
    return String(text)
      .replace(/\{HERO\}/g, ctx.hero.name)
      .replace(/\{OTHER\}/g, ctx.other.name)
      .replace(/\{OBJ\}/g, ctx.object)
      .replace(/\{PLACE\}/g, ctx.placeWord)
      .replace(/\{WANT\}/g, ctx.want)
      .replace(/\{TONIGHT\}/g, ctx.tonight)
      .replace(/\{DETAIL_SHOT\}/g, 'the ' + ctx.object)
      .replace(/\{DETAIL\}/g, function () { return ctx.nextDetail(); })
      .replace(/\{SOUND_CUE\}/g, function () { return 'From somewhere close: ' + ctx.nextSound() + '.'; })
      .replace(/\{SOUND\}/g, function () { return ctx.nextSound(); });
  }

  /* Which of the premise's locations each beat plays in. The film opens and
   * closes in the same place — that is what makes an ending feel like one. */
  var BEAT_PLACE = { open: 0, spark: 0, push: 1, turn: 1, crisis: 1, choice: 0, after: 0 };

  function headingFor(premise, beatId, placeIndex, previous) {
    var place = premise.places[Math.min(placeIndex, premise.places.length - 1)];
    var time = beatId === 'after' ? (NEXT_TIME[premise.time] || premise.time) : premise.time;
    // Two scenes running in one room read as CONTINUOUS, then LATER.
    if (previous && previous.place.slug === place.slug && previous.time === time) {
      time = previous.continuous ? 'LATER' : 'CONTINUOUS';
    }
    return {
      int: place.int,
      place: place,
      time: time,
      continuous: time === 'CONTINUOUS' || time === 'LATER',
      text: place.int + ' ' + place.slug + ' — ' + time
    };
  }

  function exchangesFor(genreKey, beatId, rng) {
    var shared = (DLG.SHARED[beatId] || []).slice();
    var tinted = (DLG.BY_GENRE[genreKey] && DLG.BY_GENRE[genreKey][beatId]) || [];
    // A genre-specific exchange gets two tickets in the draw, so the film
    // sounds like its genre without ever sounding like only its genre.
    var all = shared.concat(tinted, tinted);
    if (!all.length) return null;
    return all[Math.floor(rng() * all.length) % all.length];
  }

  function write(premise, opts) {
    opts = opts || {};
    var lengthKey = LEX.STRUCTURES[opts.length] ? opts.length : 'short';
    var structure = LEX.STRUCTURES[lengthKey];
    var seed = typeof opts.seed === 'number' ? opts.seed >>> 0 : premise.seed;
    var rng = PARSE.makeRng((seed + 0x9e3779b9) >>> 0);
    var genre = LEX.GENRES[premise.genre] || LEX.GENRES.drama;

    var ctx = {
      hero: premise.hero,
      other: premise.other,
      object: premise.object,
      placeWord: premise.places[0].word.toLowerCase(),
      want: premise.want,
      tonight: premise.time === 'NIGHT' || premise.time === 'DUSK' ? 'tonight' : 'today',
      nextDetail: pool(genre.details, rng),
      nextSound: pool(genre.sounds, rng)
    };

    var beatById = {};
    LEX.BEATS.forEach(function (b) { beatById[b.id] = b; });

    var elements = [{ type: 'transition', text: 'FADE IN:' }];
    var scenes = [];
    var previousHeading = null;
    var introduced = {};

    structure.beats.forEach(function (beatId, index) {
      var beat = beatById[beatId];
      var heading = headingFor(premise, beatId, BEAT_PLACE[beatId] || 0, previousHeading);
      previousHeading = heading;
      // Action lines say "the kitchen" only when the scene is in the kitchen.
      ctx.placeWord = heading.place.word.toLowerCase();

      var sceneElements = [{ type: 'scene_heading', text: heading.text }];
      var actionPool = pool(beat.action, rng);

      // First time we meet someone, screenplay convention introduces them in
      // caps with one line of who they are.
      if (!introduced[premise.hero.name]) {
        introduced[premise.hero.name] = true;
        sceneElements.push({
          type: 'action',
          text: premise.hero.name + ' (' + ageFor(premise.hero.role, rng) + '), ' +
            PARSE.withArticle(premise.hero.role) + ' ' +
            pick(INTROS, rng) + ', is here and has been for a while.'
        });
      }

      sceneElements.push({ type: 'action', text: fill(actionPool(), ctx) });
      sceneElements.push({ type: 'action', text: fill(actionPool(), ctx) });
      if (lengthKey !== 'micro' && rng() < 0.5) {
        sceneElements.push({ type: 'action', text: fill(actionPool(), ctx) });
      }

      var exchange = exchangesFor(premise.genre, beatId, rng);
      if (exchange) {
        var otherSpeaks = exchange.some(function (l) { return l.who === 'other'; });
        if (otherSpeaks && !introduced[premise.other.name]) {
          introduced[premise.other.name] = true;
          sceneElements.push({
            type: 'action',
            text: premise.other.name + ' (' + ageFor(premise.other.role, rng) + '), ' +
              PARSE.withArticle(premise.other.role) +
              ', comes in without knocking. They have never had to.'
          });
        }
        stageExchange(exchange, sceneElements, premise, ctx);
      }

      // A second exchange where there is room for one: the middle of a longer
      // short is where a scene can afford to breathe.
      if (exchange && lengthKey === 'festival' && rng() < 0.45) {
        var encore = exchangesFor(premise.genre, beatId, rng);
        if (encore && encore !== exchange) {
          sceneElements.push({ type: 'action', text: capitalize(ctx.nextDetail()) });
          stageExchange(encore, sceneElements, premise, ctx);
        }
      }

      // Close on an image, never on a spoken line — and on a *sensory* one, so
      // it can never contradict the action it follows.
      sceneElements.push({ type: 'action', text: capitalize(ctx.nextDetail()) });

      var isLast = index === structure.beats.length - 1;
      if (isLast) sceneElements.push({ type: 'transition', text: 'FADE OUT.' });

      scenes.push({
        number: index + 1,
        beat: { id: beat.id, name: beat.name, purpose: beat.purpose },
        heading: heading,
        elements: sceneElements,
        shots: beat.shots.map(function (s) { return fill(s, ctx); })
      });

      sceneElements.forEach(function (el) { elements.push(el); });
    });

    var measure = paginate(elements);

    return {
      title: premise.title,
      logline: premise.logline,
      idea: premise.raw,
      genre: premise.genre,
      genreLabel: premise.genreLabel,
      length: lengthKey,
      lengthLabel: structure.label,
      seed: seed,
      premise: premise,
      characters: [
        { name: premise.hero.name, role: premise.hero.role, part: 'lead' },
        { name: premise.other.name, role: premise.other.role, part: 'supporting' }
      ],
      scenes: scenes,
      elements: elements,
      pages: measure.pages,
      runtime: measure.runtime
    };
  }

  function pick(list, rng) {
    return list[Math.floor(rng() * list.length) % list.length];
  }

  function stageExchange(exchange, out, premise, ctx) {
    exchange.forEach(function (line) {
      var who = line.who === 'hero' ? premise.hero : premise.other;
      out.push({ type: 'character', text: who.name });
      if (line.paren) out.push({ type: 'parenthetical', text: fill(line.paren, ctx) });
      out.push({ type: 'dialogue', text: fill(line.line, ctx) });
    });
  }

  /* Rough but honest: a screenplay page is ~55 lines, and a page of a short
   * plays at about a minute. Wrap widths match the printed layout below. */
  var WRAP = { action: 60, dialogue: 35, parenthetical: 25, character: 38, scene_heading: 60, transition: 60 };

  function paginate(elements) {
    var lines = 0;
    elements.forEach(function (el) {
      var width = WRAP[el.type] || 60;
      lines += Math.max(1, Math.ceil(el.text.length / width)) + 1; // + the blank line after
    });
    var pages = Math.max(1, Math.round((lines / 55) * 10) / 10);
    var minutes = Math.max(1, Math.round(pages));
    return { pages: pages, runtime: '≈ ' + minutes + ' min' };
  }

  var API = { write: write, paginate: paginate };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWriter = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
