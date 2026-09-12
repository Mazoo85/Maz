/*
 * SCRIPT FORGE — dressing the set.
 * --------------------------------
 * Fifteen sets, drawn identically every time. The premise changed the colour
 * and the weather; it never changed the room. A hospital ward was the same ward
 * in every film anybody had ever made with this, which is the one thing you
 * cannot un-notice once you have seen two.
 *
 * Rewriting fifteen hand-built sets to be variable is a lot of work and a lot of
 * risk for one payoff. This is the other way in: a layer of DRESSING, seeded
 * from the story, drawn over whatever the set painter drew. One layer varies all
 * fifteen at once, and a set that has not been touched still gets it.
 *
 * What actually makes a room look like a different room is not where the
 * furniture is, it is HOW THE ROOM HAS BEEN TREATED — so the dial is condition:
 *
 *   kept       somebody looks after this place. A lamp on, almost nothing out.
 *   worn       lived in. Things left where they were put down, a mark or two.
 *   abandoned  nobody has been here in a while. Clutter, damp, no lamp lit.
 *
 * Genre leans on it (horror does not get a tidy room; comedy rarely gets a
 * derelict one) and the seed settles it, so the same idea always gets the same
 * room.
 *
 * WHERE IT MAY DRAW is the whole safety story. Set painters put their furniture
 * in the middle and figures stand between x=250 and x=660 of a 1000-unit stage
 * (see figureLayout in film-player.js), so dressing keeps to the outer bands and
 * the floor line, where every set has room and nobody is standing. That is why
 * this can be switched on for all fifteen without looking at all fifteen.
 *
 * Exposed as window.FilmSetDress (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  var STAGE_W = 1000;
  var STAGE_H = 420;

  /* The bands dressing is allowed into. Everything outside them belongs to the
   * set painter or to the actors. */
  var LEFT_BAND = [10, 200];
  var RIGHT_BAND = [770, 975];
  var FLOOR_Y = 352;
  var WALL_BAND = [60, 240];    // y, for marks on the back wall

  var CONDITIONS = ['kept', 'worn', 'abandoned'];

  /* How each genre treats a room. The numbers are weights over the three
   * conditions, not probabilities of anything else. */
  var BY_GENRE = {
    horror:   [0, 1, 4],
    thriller: [1, 3, 3],
    mystery:  [1, 3, 2],
    scifi:    [3, 3, 2],
    western:  [1, 3, 3],
    heist:    [2, 4, 1],
    drama:    [2, 4, 1],
    fantasy:  [2, 3, 2],
    romance:  [4, 3, 1],
    comedy:   [4, 3, 1]
  };

  /* Which sets are outdoors. A crate in a field is a mistake, not a mood. */
  var OUTDOORS = { woods: 1, street: 1, field: 1, water: 1 };

  function conditionFor(genre, seed) {
    var weights = BY_GENRE[genre] || BY_GENRE.drama;
    var total = weights[0] + weights[1] + weights[2];
    var rng = PARSE.makeRng((PARSE.hashText('dress:condition') ^ ((seed || 0) >>> 0)) >>> 0);
    var roll = rng() * total;
    var acc = 0;
    for (var i = 0; i < weights.length; i++) {
      acc += weights[i];
      if (roll < acc) return CONDITIONS[i];
    }
    return 'worn';
  }

  /* How much of it, per condition. */
  var AMOUNT = {
    kept:      { props: [1, 2], marks: [0, 0], lamp: 0.8 },
    worn:      { props: [2, 4], marks: [1, 2], lamp: 0.45 },
    abandoned: { props: [3, 5], marks: [2, 3], lamp: 0.05 }
  };

  var INDOOR_PROPS = ['crate', 'stack', 'plank', 'hanging'];
  var OUTDOOR_PROPS = ['post', 'rock', 'scrub', 'plank'];

  function between(rng, lo, hi) {
    return lo + rng() * (hi - lo);
  }

  function intBetween(rng, range) {
    return Math.floor(range[0] + rng() * (range[1] - range[0] + 1));
  }

  /* Everything this film puts in this set. Pure data — no canvas, so the
   * decisions can be checked without drawing any of them. */
  function dressingFor(setKey, genre, seed) {
    var condition = conditionFor(genre, seed);
    var amount = AMOUNT[condition];
    var outdoors = !!OUTDOORS[setKey];
    var pool = outdoors ? OUTDOOR_PROPS : INDOOR_PROPS;
    // Keyed on the set as well as the film, so the two rooms of one film are
    // dressed differently from each other.
    var rng = PARSE.makeRng((PARSE.hashText('dress:' + setKey) ^ ((seed || 0) >>> 0)) >>> 0);

    var props = [];
    var count = intBetween(rng, amount.props);
    for (var i = 0; i < count; i++) {
      var kind = pool[Math.floor(rng() * pool.length) % pool.length];
      // Alternate sides so a room is not all piled into one corner.
      var band = (i % 2 === 0) ? LEFT_BAND : RIGHT_BAND;
      var w, h;
      if (kind === 'hanging') {
        w = between(rng, 10, 26);
        h = between(rng, 90, 230);
      } else {
        w = between(rng, 26, 64);
        h = between(rng, 18, 52);
      }
      // The box is the promise. Drawing a prop WIDER than the position it was
      // given is how a crate that was placed inside the left band ends up
      // growing out of somebody's chest at x=231 -- so x leaves room for w, and
      // every shape in drawProp stays inside {x, y, w, h}.
      var x = between(rng, band[0], Math.max(band[0], band[1] - w));
      props.push({ kind: kind, x: x, y: kind === 'hanging' ? 0 : FLOOR_Y - h, w: w, h: h });
    }

    var marks = [];
    var markCount = outdoors ? 0 : intBetween(rng, amount.marks);
    for (var m = 0; m < markCount; m++) {
      var mb = (m % 2 === 0) ? LEFT_BAND : RIGHT_BAND;
      var mw = between(rng, 30, 90);
      var mh = between(rng, 40, 120);
      marks.push({
        x: between(rng, mb[0], Math.max(mb[0], mb[1] - mw)),
        y: between(rng, WALL_BAND[0], WALL_BAND[1]),
        w: mw,
        h: mh,
        alpha: between(rng, 0.1, 0.26)
      });
    }

    var lamp = null;
    if (!outdoors && rng() < amount.lamp) {
      var lb = rng() < 0.5 ? LEFT_BAND : RIGHT_BAND;
      lamp = { x: between(rng, lb[0] + 30, lb[1] - 30), y: between(rng, 150, 250), r: between(rng, 40, 90) };
    }

    return { condition: condition, outdoors: outdoors, props: props, marks: marks, lamp: lamp };
  }

  /* In a sentence, for the "why" panel. */
  function describe(dressing, setKey) {
    var how = {
      kept: 'looked after — almost nothing out of place',
      worn: 'lived in — things left where they were put down',
      abandoned: 'nobody has been here in a while'
    }[dressing.condition];
    return 'The ' + setKey + ' is ' + how + '.';
  }

  /* ------------------------------------------------------------- drawing */

  function drawProp(ctx, p, prop, rgb) {
    if (prop.kind === 'hanging') {
      ctx.fillStyle = rgb(p.ink, 0.85);
      ctx.fillRect(prop.x, prop.y, prop.w, prop.h);
      return;
    }
    if (prop.kind === 'rock' || prop.kind === 'scrub') {
      ctx.fillStyle = rgb(p.ink, prop.kind === 'scrub' ? 0.7 : 0.9);
      ctx.beginPath();
      ctx.ellipse(prop.x + prop.w / 2, prop.y + prop.h, prop.w * 0.5, prop.h * 0.62, 0, Math.PI, 0);
      ctx.fill();
      return;
    }
    if (prop.kind === 'post') {
      // Tall and thin, rising out of the top of its box -- height is free,
      // width is not.
      ctx.fillStyle = rgb(p.ink, 0.9);
      ctx.fillRect(prop.x + prop.w * 0.3, prop.y - prop.h * 1.4, prop.w * 0.3, prop.h * 2.4);
      return;
    }
    if (prop.kind === 'plank') {
      // Leaning, which is what tells you nobody put it away. Drawn as a quad
      // rather than a rotation, so the lean is bounded by the box's own width
      // instead of by however far a rotation happened to throw it.
      var bx = prop.x, by = prop.y + prop.h;
      var topX = prop.x + prop.w * 0.62, topY = prop.y - prop.h * 1.1;
      var t = prop.w * 0.3;
      ctx.fillStyle = rgb(p.ink, 0.88);
      ctx.beginPath();
      ctx.moveTo(bx, by);
      ctx.lineTo(bx + t, by);
      ctx.lineTo(topX + t, topY);
      ctx.lineTo(topX, topY);
      ctx.closePath();
      ctx.fill();
      return;
    }
    // crate, and stack: the same box, once or twice.
    ctx.fillStyle = rgb(p.ink, 0.92);
    ctx.fillRect(prop.x, prop.y, prop.w, prop.h);
    if (prop.kind === 'stack') {
      ctx.fillStyle = rgb(p.ink, 0.8);
      ctx.fillRect(prop.x + prop.w * 0.15, prop.y - prop.h * 0.72, prop.w * 0.7, prop.h * 0.72);
    }
  }

  /* Draw the dressing onto the mid plane, after the set painter and before the
   * actors. `rgb` is the artist's own colour helper, passed in rather than
   * imported so this file stays free of the art module. */
  function draw(ctx, p, dressing, rgb) {
    if (!dressing) return;

    // Damp, soot, the shape something used to hang in: the cheapest signal that
    // a room has a history.
    dressing.marks.forEach(function (mark) {
      ctx.fillStyle = rgb(p.shadow, mark.alpha);
      ctx.beginPath();
      ctx.ellipse(mark.x + mark.w * 0.5, mark.y, mark.w * 0.5, mark.h * 0.5, 0, 0, Math.PI * 2);
      ctx.fill();
    });

    if (dressing.lamp) {
      var g = ctx.createRadialGradient(dressing.lamp.x, dressing.lamp.y, 2,
                                       dressing.lamp.x, dressing.lamp.y, dressing.lamp.r);
      g.addColorStop(0, rgb(p.key, 0.4));
      g.addColorStop(1, rgb(p.key, 0));
      ctx.fillStyle = g;
      ctx.fillRect(dressing.lamp.x - dressing.lamp.r, dressing.lamp.y - dressing.lamp.r,
                   dressing.lamp.r * 2, dressing.lamp.r * 2);
      ctx.fillStyle = rgb(p.key, 0.75);
      ctx.fillRect(dressing.lamp.x - 7, dressing.lamp.y - 7, 14, 14);
    }

    dressing.props.forEach(function (prop) { drawProp(ctx, p, prop, rgb); });
  }

  var API = {
    STAGE_W: STAGE_W,
    STAGE_H: STAGE_H,
    LEFT_BAND: LEFT_BAND,
    RIGHT_BAND: RIGHT_BAND,
    FLOOR_Y: FLOOR_Y,
    CONDITIONS: CONDITIONS,
    OUTDOORS: OUTDOORS,
    AMOUNT: AMOUNT,
    conditionFor: conditionFor,
    dressingFor: dressingFor,
    describe: describe,
    draw: draw
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmSetDress = API;
})(typeof window !== 'undefined' ? window : globalThis);
