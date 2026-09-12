/*
 * SCRIPT FORGE — what happens to the thing.
 * -----------------------------------------
 * Every story here turns on an object: a key, a letter, a watch. Until now that
 * object reached the page only where a template happened to hold an {OBJ} slot,
 * which meant it could be named in the opening, vanish for three scenes, and be
 * mentioned again at the end as if the audience had been thinking about it the
 * whole time. Nothing was planted, so nothing paid off, which is most of why
 * the endings landed soft.
 *
 * An ARC fixes that. Each arc is a complete physical story about one object,
 * written as a single piece — seven lines, one per beat:
 *
 *   unnoticed → noticed → carried → contested → lost → reclaimed → changed
 *
 * A film draws ONE arc and uses all of it. That is the whole trick: because the
 * first line and the last line of an arc were written together, the ending
 * calls back to the opening by construction rather than by luck. Pairing a
 * random plant with a random payoff gets you neither.
 *
 * The lines are shot descriptions, not narration: present tense, visible, no
 * interior state. film-reel.js already gives an insert shot to any action line
 * that names the object, so a film with an arc cuts to the object at exactly
 * the seven moments it matters.
 *
 * Exposed as window.FilmObjectArc (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* Which state of the object each story beat is about. The beat spine can be
   * reordered by length (see LEX.STRUCTURES), so this is keyed by beat rather
   * than by position: a film that skips 'turn' skips the contested state and
   * the rest of the arc still runs in order. */
  var BY_BEAT = {
    open: 'unnoticed',
    spark: 'noticed',
    push: 'carried',
    turn: 'contested',
    crisis: 'lost',
    choice: 'reclaimed',
    after: 'changed'
  };

  var STATES = ['unnoticed', 'noticed', 'carried', 'contested', 'lost', 'reclaimed', 'changed'];

  /* What each state is for, in one line — used by the "why" panel, and by the
   * test that prints something useful when an arc is missing a state. */
  var PURPOSE = {
    unnoticed: 'it is already there, and nobody is looking at it',
    noticed: 'somebody picks it up, and the film is about it now',
    carried: 'it goes where they go',
    contested: 'somebody else wants it',
    lost: 'the worst thing that can happen to it, happens',
    reclaimed: 'it comes back, and there is a decision to make',
    changed: 'the same object, meaning something else'
  };

  /* Six arcs. Each one is internally consistent — a thing that is buried gets
   * dug up, a thing that is broken gets mended — so the object does one story
   * across the film instead of seven unrelated things. */
  var ARCS = [
    {
      id: 'kept',
      lines: {
        unnoticed: 'The {OBJ} has been on that shelf so long it has stopped being an object.',
        noticed: '{HERO} takes the {OBJ} down. Turns it over. Puts it in a pocket instead of back.',
        carried: 'The {OBJ} makes a shape in {HERO}’s coat. {HERO} keeps checking it is still there.',
        contested: '{OTHER} looks at the pocket. Not at {HERO}. At the pocket.',
        lost: 'The {OBJ} is not in the pocket. {HERO} goes through it twice anyway.',
        reclaimed: '{HERO} finds the {OBJ}. Holds it like it might go again.',
        changed: 'The {OBJ} goes back on the shelf. It does not stop being an object this time.'
      }
    },
    {
      id: 'hidden',
      lines: {
        unnoticed: 'The {OBJ} is where {HERO} left it, which is somewhere nobody looks.',
        noticed: '{HERO} moves the {OBJ} to a better hiding place. There is not one.',
        carried: '{HERO} carries the {OBJ} the way you carry something you have not decided about.',
        contested: '{OTHER} asks for the {OBJ} without asking for it.',
        lost: 'The hiding place is open and the {OBJ} is gone.',
        reclaimed: 'The {OBJ} is in {OTHER}’s hand, held out, and {HERO} has to decide whether to take it.',
        changed: '{HERO} puts the {OBJ} somewhere anyone could find it.'
      }
    },
    {
      id: 'broken',
      lines: {
        unnoticed: 'The {OBJ} sits in the {PLACE}, whole, and nobody is looking at it.',
        noticed: '{HERO} picks up the {OBJ}. Something about it is already wrong.',
        carried: '{HERO} will not put the {OBJ} down, and will not look at it either.',
        contested: '{OTHER} reaches for the {OBJ}. {HERO} moves it out of reach without seeming to.',
        lost: 'The {OBJ} hits the floor. The sound is smaller than it should be.',
        reclaimed: '{HERO} gathers up the {OBJ}. Every piece. Even the ones that will not matter.',
        changed: 'The {OBJ} is mended badly, in the open, where the mend shows.'
      }
    },
    {
      id: 'given',
      lines: {
        unnoticed: 'The {OBJ} belongs to somebody who is not here.',
        noticed: '{HERO} is holding the {OBJ} before deciding to.',
        carried: 'The {OBJ} goes everywhere {HERO} goes {TONIGHT}.',
        contested: '{OTHER} says the {OBJ} is not {HERO}’s to keep. Neither of them says whose it is.',
        lost: '{HERO} lets go of the {OBJ}. Watches it go.',
        reclaimed: 'The {OBJ} comes back the way things do, in the wrong hands at the wrong hour.',
        changed: '{HERO} gives the {OBJ} away, on purpose, and it costs what it costs.'
      }
    },
    {
      id: 'buried',
      lines: {
        unnoticed: 'Nothing in the {PLACE} says the {OBJ} is here. It is.',
        noticed: '{HERO} digs up the {OBJ} without having to look for the spot.',
        carried: 'The {OBJ} is out in the light and {HERO} keeps it covered anyway.',
        contested: '{OTHER} knows where the {OBJ} came from, and says so with a look.',
        lost: 'The hole is open. There is nothing in it.',
        reclaimed: '{HERO} puts a hand on the {OBJ} and does not pick it up yet.',
        changed: '{HERO} buries the {OBJ} again, somewhere they mean to remember.'
      }
    },
    {
      id: 'evidence',
      lines: {
        unnoticed: 'The {OBJ} is in the {PLACE} with everything else that got left behind.',
        noticed: '{HERO} sees the {OBJ} and stops walking.',
        carried: '{HERO} holds the {OBJ} where their hands can be seen holding it.',
        contested: '{OTHER} wants the {OBJ} to mean what {OTHER} says it means.',
        lost: 'The {OBJ} is gone and the room is exactly as it was.',
        reclaimed: 'The {OBJ} turns up somewhere it could not have got to by itself.',
        changed: '{HERO} sets the {OBJ} down between them, and lets it say it.'
      }
    }
  ];

  /* Which arc this film runs. Keyed off the seed alone, so the same idea always
   * tells the same story about its object. */
  function arcFor(seed) {
    var rng = PARSE.makeRng((PARSE.hashText('object-arc') ^ ((seed || 0) >>> 0)) >>> 0);
    return ARCS[Math.floor(rng() * ARCS.length) % ARCS.length];
  }

  function stateFor(beatId, opts) {
    if (opts && opts.isLast) return 'changed';
    return BY_BEAT[beatId] || null;
  }

  /* The object's line for one beat, or null if this beat has no state — which
   * is not a failure, it is a title card or a beat outside the seven.
   *
   * `opts.isLast` forces the payoff. Not every spine ends on 'after': two of the
   * three five-scene spines end on 'choice', and one three-scene spine ends
   * there too (see LEX.STRUCTURES). Keying the payoff to a beat that half the
   * films never reach means half the films have a plant and no payoff, which is
   * worse than having neither. The payoff belongs to the LAST SCENE, whatever
   * beat that scene happens to be. */
  function lineFor(arc, beatId, opts) {
    var state = (opts && opts.isLast) ? 'changed' : stateFor(beatId);
    if (!state || !arc || !arc.lines) return null;
    return arc.lines[state] || null;
  }

  /* For the "why" panel: what this film is doing with its object, in a sentence
   * anybody can read. */
  function describe(arc, beatId, object) {
    var state = stateFor(beatId);
    if (!state) return null;
    return 'The ' + (object || 'object') + ': ' + PURPOSE[state] +
      ' (the “' + arc.id + '” arc).';
  }

  var API = {
    ARCS: ARCS,
    STATES: STATES,
    BY_BEAT: BY_BEAT,
    PURPOSE: PURPOSE,
    arcFor: arcFor,
    stateFor: stateFor,
    lineFor: lineFor,
    describe: describe
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmObjectArc = API;
})(typeof window !== 'undefined' ? window : globalThis);
