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
  var VOICE = root.FilmVoice || (typeof require !== 'undefined' ? require('./voice.js') : {});

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
    // The swap happens on the template, before {HERO}/{OBJ} go in, because the
    // map is keyed by the raw strings that live in the lexicon.
    return String(ctx.exterior ? LEX.outdoors(text) : text)
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

  /* Which of the premise's locations each beat plays in.
   *
   * This used to be three independent per-beat rules (open/choice/after home,
   * crisis at the far end, spark/push/turn spread through the middle) and
   * that is exactly the bug: 'crisis at the far end' and 'spread through the
   * middle, including the far end' were fighting over the same index, and the
   * spread rule usually won — measured, the crisis landed on a place a middle
   * beat had already used in the *majority* of short and festival films (see
   * placesForSpine below). A per-beat rule can't see the rest of the spine,
   * so it can't avoid that. This one does: it is handed the whole spine and
   * decides every beat's place at once.
   *
   * The legacy per-beat function (`placeForBeat`, still below) is kept only
   * because `film/tests/film-logic.test.js:1462` pins its behaviour
   * (open/choice/after all hardcode place 0) as a documented historical
   * property; `write()` no longer calls it, so there is exactly one notion of
   * "which place" governing an actual film. */
  function placeForBeat(beatId, placeCount, seed) {
    if (placeCount <= 1) return 0;
    if (beatId === 'open' || beatId === 'choice' || beatId === 'after') return 0;
    if (beatId === 'crisis') return placeCount - 1;
    var span = Math.max(1, placeCount - 1);
    var rng = PARSE.makeRng((PARSE.hashText('spread') ^ (seed >>> 0)) >>> 0);
    var base = Math.floor(rng() * span) % span;
    var step = { spark: 0, push: 1, turn: 2 }[beatId] || 0;
    return 1 + (base + step) % span;
  }

  /* The real, spine-aware assignment `write()` actually uses.
   *
   * Three rules, in priority order — each below gives way only when the
   * place count can't satisfy it *and* the ones above it at once:
   *
   *   1. the first beat and the last beat share place 0 (unconditional —
   *      this is what makes an ending feel like one; pinned for every spine
   *      by the 'ends where it began' test)
   *   2. the crisis lands on a place no *earlier* beat in this same spine
   *      used — a room the film has not needed yet, because the worst
   *      moment of the night is supposed to be somewhere unfamiliar
   *   3. the other middle beats spread across whatever places are left, as
   *      widely as that allows
   *
   * Rule 2 outranks rule 3: the middle beats between rule 2's neighbors
   * reserve the crisis's spot for it rather than the other way around, so at
   * 3 places rule 2 still holds and the middle beats simply share their one
   * remaining spot with each other (rule 3 narrows) rather than the crisis
   * falling back into a used room. Only when there is no spare place at all
   * (fewer than 3 places total — never the case for a generated premise,
   * which always offers 3-5) does rule 2 itself give way; the return value's
   * `degraded` field says which rule gave way, or null if none did.
   *
   * Returns { places: [placeIndex per beat, same order as `spine`], degraded }
   * where degraded is null, 'crisis-unused' (rule 2 gave way — only possible
   * with fewer than 3 places), or 'spread-narrowed' (rule 3 gave way: the
   * non-crisis middle beats had to repeat a place among themselves so the
   * crisis could have one to itself). */
  function placesForSpine(spine, placeCount, seed) {
    var n = spine.length;
    var places = new Array(n);
    var ci = spine.indexOf('crisis');

    if (placeCount <= 1 || n <= 1) {
      for (var z = 0; z < n; z++) places[z] = 0;
      return { places: places, degraded: (ci !== -1 && n > 1) ? 'crisis-unused' : null };
    }

    places[0] = 0;
    places[n - 1] = 0;

    var span = placeCount - 1;                    // nonzero places: 1 .. placeCount-1
    var rng = PARSE.makeRng((PARSE.hashText('spread:' + spine.join('>')) ^ (seed >>> 0)) >>> 0);

    var crisisPlace = ci !== -1 ? 1 + (Math.floor(rng() * span) % span) : null;

    // Everyone else draws from every nonzero place except the crisis's own —
    // reserving it is what lets rule 2 hold — unless there is only the one
    // nonzero place to go around, in which case there is nothing to reserve
    // and the crisis's own rule (2) is what gives way, not this one.
    var spreadValues = [];
    for (var v = 1; v < placeCount; v++) {
      if (crisisPlace === null || v !== crisisPlace || span === 1) spreadValues.push(v);
    }
    var draw = pool(spreadValues, rng);

    var middleNonCrisis = 0;
    for (var i = 1; i < n - 1; i++) {
      if (i === ci) continue;
      middleNonCrisis++;
      places[i] = draw();
    }
    if (ci !== -1) places[ci] = crisisPlace;

    var degraded = null;
    if (ci !== -1 && span === 1) degraded = 'crisis-unused';
    else if (spreadValues.length < middleNonCrisis) degraded = 'spread-narrowed';

    return { places: places, degraded: degraded };
  }

  /* Two films of the same length should not be the same shape.
   *
   * The design spec says spines are "chosen by seed and genre"; this only
   * uses the seed. Recorded as a deviation rather than honoured, because
   * honouring it means inventing a reason a given genre prefers a given beat
   * order (does horror really want 'push' before 'spark' more than drama
   * does?) with nothing to base that association on — see the spec's own
   * note at docs/superpowers/specs/2026-09-11-better-stories-design.md. */
  function spineFor(lengthKey, seed) {
    var structure = LEX.STRUCTURES[lengthKey] || LEX.STRUCTURES.short;
    var spines = structure.spines || [structure.beats];
    var rng = PARSE.makeRng((PARSE.hashText('spine:' + lengthKey) ^ (seed >>> 0)) >>> 0);
    return spines[Math.floor(rng() * spines.length) % spines.length];
  }

  function headingFor(premise, beatId, placeIndex, previous, advanceTime) {
    var place = premise.places[Math.min(placeIndex, premise.places.length - 1)];
    // Time advances once, for 'after' and everything at or after it in the
    // spine — not for the 'after' beat id alone. That rule was written when
    // 'after' was always the last beat; once a shape puts 'choice' after it
    // (festival's third shape does), the beat id test alone let the closing
    // scene revert to the premise's original time, running the clock
    // backwards on the last two cards of ~30% of festival films.
    var time = advanceTime ? (NEXT_TIME[premise.time] || premise.time) : premise.time;
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

    var ctx;
    ctx = {
      hero: premise.hero,
      other: premise.other,
      object: premise.object,
      placeWord: premise.places[0].word.toLowerCase(),
      want: premise.want,
      tonight: premise.time === 'NIGHT' || premise.time === 'DUSK' ? 'tonight' : 'today',
      exterior: false,
      nextDetail: (function (next) {
        // Details reach the page two ways: substituted into a template by
        // fill(), and pushed straight through at the end of a scene. Swapping
        // here covers both, and never double-swaps (an outdoor twin is not
        // itself a key).
        return function () { return ctx.exterior ? LEX.outdoors(next()) : next(); };
      }(pool(genre.details, rng))),
      nextSound: (function (next) {
        return function () { return ctx.exterior ? LEX.outdoors(next()) : next(); };
      }(pool(genre.sounds, rng)))
    };

    // How each of them talks. Derived from the name and the role rather than
    // drawn from `rng`, so adding a scene above this line cannot change the way
    // a character speaks in the scene below it.
    ctx.voices = {
      hero: VOICE.voiceFor(premise.hero.name, premise.hero.role, seed),
      other: VOICE.voiceFor(premise.other.name, premise.other.role, seed)
    };

    var beatById = {};
    LEX.BEATS.forEach(function (b) { beatById[b.id] = b; });

    var elements = [{ type: 'transition', text: 'FADE IN:' }];
    var scenes = [];
    var previousHeading = null;
    var introduced = {};

    var spine = spineFor(lengthKey, seed);
    var placement = placesForSpine(spine, premise.places.length, seed);
    var afterIndex = spine.indexOf('after');
    spine.forEach(function (beatId, index) {
      var beat = beatById[beatId];
      var advanceTime = afterIndex !== -1 && index >= afterIndex;
      var heading = headingFor(premise, beatId,
        placement.places[index], previousHeading, advanceTime);
      previousHeading = heading;
      // Action lines say "the kitchen" only when the scene is in the kitchen.
      ctx.placeWord = heading.place.word.toLowerCase();
      // ...and only talk about sills and ceilings when there are some.
      ctx.exterior = heading.int === 'EXT.';

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

      var isLast = index === spine.length - 1;
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
    exchange.forEach(function (line, at) {
      var isHero = line.who === 'hero';
      var who = isHero ? premise.hero : premise.other;
      out.push({ type: 'character', text: who.name });
      if (line.paren) out.push({ type: 'parenthetical', text: fill(line.paren, ctx) });
      // The bank is written in one neutral voice because an exchange has to work
      // for whoever ends up saying it; this is where it stops being neutral. The
      // voice runs on the TEMPLATE, before fill(), so a character who uses your
      // name can be handed the slot for whoever they are talking to.
      var spoken = line.line;
      if (ctx.voices) {
        spoken = VOICE.speak(spoken, isHero ? ctx.voices.hero : ctx.voices.other, {
          at: at,
          listenerSlot: isHero ? '{OTHER}' : '{HERO}'
        });
      }
      out.push({ type: 'dialogue', text: fill(spoken, ctx) });
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

  var API = {
    write: write, paginate: paginate,
    placeForBeat: placeForBeat, placesForSpine: placesForSpine, spineFor: spineFor
  };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWriter = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
