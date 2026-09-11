/*
 * SCRIPT FORGE — stories borrowed from MADLIBS.
 * --------------------------------------------
 * MADLIBS, next door in this repo, knows 45 story skeletons across genres far
 * more numerous than SCRIPT FORGE's own ten, and fills them with words that
 * stay consistent across the beats (a hero's name, a place, a key object).
 * This module is the two lookup tables Task 5 needs to turn one of those
 * filled-in stories into something SCRIPT FORGE can shoot: a genre it
 * recognizes, and beats it knows how to place.
 *
 * MADLIBS is not modified: it is used as a library, one way.
 *
 * Exposed as window.FilmStorySeed (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var MAD = root.MadlibsGenerator ||
    (typeof require !== 'undefined' ? require('../../madlibs/js/generator.js') : {});

  /* MADLIBS has 34 genres across its 45 templates; SCRIPT FORGE only has ten
   * (drama thriller horror comedy romance scifi mystery fantasy heist
   * western). Every MADLIBS genre is mapped to whichever of those ten its
   * own loglines (madlibs/js/templates.js) actually shoot closest to — never
   * to a made-up default, and never left out so a test has to be loosened.
   *
   * The nine with an obvious match go straight across ('sci-fi' carries an
   * extra 'scifi' alias in case a caller normalizes the hyphen away). The
   * rest were each decided by reading their loglines:
   *
   *   adventure         -> fantasy   map-of-the-dead & sky-pirates: a magic
   *                                  relic, a fortune, a ship's crew — reads
   *                                  as a fantasy quest, not a modern caper.
   *   superhero         -> scifi     "gains the power to ... protect the
   *                                  city" is contemporary-urban, not a
   *                                  realm/kingdom story.
   *   noir               -> mystery  "takes one case ... falls into a city
   *                                  of secrets" — case/secret are mystery's
   *                                  own keywords.
   *   fairy-tale         -> fantasy  the genre by definition.
   *   post-apocalyptic   -> scifi    a ruined future world.
   *   mythic             -> fantasy  gods and a relic.
   *   spy                -> thriller "spy" is literally in SCRIPT FORGE's
   *                                  thriller keyword list.
   *   coming-of-age      -> drama    a restless-summer, growing-up story.
   *   disaster           -> thriller a race against rising water; "survive"
   *                                  is a thriller keyword.
   *   political          -> thriller uncovering a ruler's corruption plays
   *                                  as an exposure/conspiracy thriller.
   *   steampunk          -> scifi    the plot turns on an invention/machine.
   *   survival           -> thriller stranded and must "survive" — again a
   *                                  thriller keyword verbatim.
   *   crime              -> thriller mob loyalty with a ledger that "could
   *                                  bury them all" — danger, not domestic
   *                                  drama.
   *   sports             -> drama    the underdog-training arc.
   *   historical          -> drama   an epic, personal rise-to-power story.
   *   cyberpunk          -> scifi    "signal" and a neon-city conspiracy —
   *                                  straight out of SCRIPT FORGE's own
   *                                  scifi keyword list.
   *   war                -> drama    strangers led home across enemy ground;
   *                                  about people, not a chase.
   *   paranormal         -> horror   a séance summoning something old.
   *   buddy-cop          -> thriller "cop" is a thriller keyword; taking
   *                                  down a gang before a deadline.
   *   time-travel        -> scifi    "time" and "travel" are both literally
   *                                  scifi keywords.
   *   music-drama        -> drama    it says so in its own name.
   *   courtroom          -> drama    legal drama, not a whodunit.
   *   pirate             -> fantasy  hunting a relic that "rules the
   *                                  storms" — a magic item, not a caper.
   *   monster            -> horror   a creature hunting a town.
   *   road-trip          -> drama    a journey about the relationship, not
   *                                  a deadline chase.
   */
  var GENRE_FOR = {
    'fantasy': 'fantasy',
    'sci-fi': 'scifi',
    'scifi': 'scifi',
    'horror': 'horror',
    'mystery': 'mystery',
    'romance': 'romance',
    'comedy': 'comedy',
    'thriller': 'thriller',
    'heist': 'heist',
    'western': 'western',
    'drama': 'drama',

    'adventure': 'fantasy',
    'superhero': 'scifi',
    'noir': 'mystery',
    'fairy-tale': 'fantasy',
    'post-apocalyptic': 'scifi',
    'mythic': 'fantasy',
    'spy': 'thriller',
    'coming-of-age': 'drama',
    'disaster': 'thriller',
    'political': 'thriller',
    'steampunk': 'scifi',
    'survival': 'thriller',
    'crime': 'thriller',
    'sports': 'drama',
    'historical': 'drama',
    'cyberpunk': 'scifi',
    'war': 'drama',
    'paranormal': 'horror',
    'buddy-cop': 'thriller',
    'time-travel': 'scifi',
    'music-drama': 'drama',
    'courtroom': 'drama',
    'pirate': 'fantasy',
    'monster': 'horror',
    'road-trip': 'drama'
  };

  /* MADLIBS's six labels against the seven beats a film is cut from. The
   * logline is the premise rather than a scene, so it maps to nothing. */
  var BEAT_FOR = {
    'Logline': null,
    'Setup': 'open',
    'Inciting Incident': 'spark',
    'Conflict': 'push',
    'Climax': 'crisis',
    'Resolution': 'after'
  };

  var API = { GENRE_FOR: GENRE_FOR, BEAT_FOR: BEAT_FOR };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmStorySeed = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
