/*
 * SCRIPT FORGE — why it did that.
 * -------------------------------
 * Every choice in a film here is made on purpose and none of them are ever
 * stated. The program knows exactly why it picked horror, why the corridor, why
 * 107 beats a minute, why that character walks in the third scene — and it has
 * never once said so, which leaves a black box that happens to produce films.
 *
 * This says. It is not a second brain making up explanations after the fact:
 * every line here reads a decision the program actually made, from the module
 * that made it. Where a module can describe its own choice it is asked to —
 * voice.js, object-arc.js, set-dress.js and world-sound.js each grew a
 * `describe` for exactly this — and where one cannot, the value is read
 * straight out of the reel.
 *
 * That distinction is the whole design. An explanation generated separately
 * from the decision drifts from it, and a WRONG explanation is worse than none,
 * because it teaches somebody something untrue about their own film.
 *
 *   FilmWhy.explain(script, reel)  ->  [{ title, lines: [...] }, ...]
 *
 * Pure: no DOM, so the whole panel can be checked in Node.
 *
 * Exposed as window.FilmWhy (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  function need(name, path) {
    return root[name] || (typeof require !== 'undefined' ? require(path) : null);
  }

  var VOICE = need('FilmVoice', './voice.js');
  var ARC = need('FilmObjectArc', './object-arc.js');
  var DRESS = need('FilmSetDress', './set-dress.js');
  var WORLD = need('FilmWorldSound', './world-sound.js');
  var WEATHER = need('FilmWeather', './film-weather.js');
  var CONDUCTOR = need('FilmConductor', './film-score.js');
  var SETS = need('FilmSets', './film-sets.js');

  function sceneShots(reel, number) {
    return reel.shots.filter(function (s) { return s.scene === number; });
  }

  function explain(script, reel) {
    var out = [];
    var premise = script.premise || {};

    /* ------------------------------------------------------------- the story */
    var story = [];
    story.push('You typed: “' + (script.idea || premise.raw || '') + '”');
    story.push(premise.genreAuto
      ? 'Nobody chose a genre, so the words in that sentence did: ' + script.genreLabel +
        (premise.genreConfidence ? ' (' + Math.round(premise.genreConfidence * 100) + '% sure)' : '') + '.'
      : 'You chose ' + script.genreLabel + '.');
    story.push('The seed is ' + script.seed + '. Every choice below comes from it, which is why the ' +
      'same sentence always gives you the same film.');
    story.push('The story turns on the ' + premise.object + ', and what ' +
      (premise.hero ? premise.hero.name : 'the lead') + ' wants is to ' + premise.want + '.');
    out.push({ title: 'The story', lines: story });

    /* ------------------------------------------------------------- the cast */
    var cast = [];
    (script.characters || []).forEach(function (c) {
      var voice = VOICE ? VOICE.voiceFor(c.name, c.role, script.seed) : null;
      cast.push(c.name + ' — ' + c.role + (voice ? '. Talks like this: ' + VOICE.describe(voice) + '.' : '.'));
    });
    cast.push('Those habits come from the name and the job, not the dice, which is why a ' +
      'surgeon speaks in full words and a teenager does not.');
    out.push({ title: 'Who they are', lines: cast });

    /* -------------------------------------------------------------- the shape */
    var shape = [];
    shape.push('The spine is ' + script.scenes.map(function (s) { return s.beat.name; }).join(' → ') + '.');
    if (ARC) {
      var arc = ARC.arcFor(script.seed);
      shape.push('The ' + premise.object + ' runs the “' + arc.id + '” arc: ' +
        ARC.statesForSpine(script.scenes.map(function (s) { return s.beat.id; })).join(' → ') + '.');
      shape.push('The first and last of those were written as a pair, which is why the ending ' +
        'lands on the thing the opening set up.');
    }
    out.push({ title: 'The shape of it', lines: shape });

    /* -------------------------------------------------------------- the rooms */
    var rooms = [];
    script.scenes.forEach(function (scene) {
      var shots = sceneShots(reel, scene.number);
      var setKey = shots.length ? shots[0].set : 'room';
      var bits = ['Scene ' + scene.number + ' — ' + scene.heading.text + ' (the ' + setKey + ' set)'];
      if (DRESS) {
        var dressing = DRESS.dressingFor(setKey, reel.genre, reel.seed);
        bits.push(DRESS.describe(dressing, setKey));
      }
      if (WEATHER && WEATHER.forShot) {
        var air = WEATHER.forShot(reel.genre, scene.heading.time, setKey);
        if (air && air !== 'none') {
          bits.push('There is ' + air + ' in the air — ' + reel.genreLabel.toLowerCase() +
            ' at ' + scene.heading.time.toLowerCase() + (DRESS && DRESS.OUTDOORS[setKey] ? ', outdoors' : ', indoors') + '.');
        }
      }
      if (SETS && SETS.LIGHT && SETS.LIGHT[setKey] && SETS.LIGHT[setKey] !== 'none') {
        bits.push('Its light ' + ({
          sweep: 'sweeps, like a beam going round',
          passing: 'comes and goes, like headlights',
          flicker: 'flickers, and flickers harder the tenser it gets',
          cloud: 'dims and lifts, like cloud crossing the sun'
        }[SETS.LIGHT[setKey]] || 'moves') + '.');
      }
      rooms.push(bits.join(' '));
    });
    out.push({ title: 'Where it happens', lines: rooms });

    /* ------------------------------------------------------------ the cutting */
    var cutting = [];
    var byBeat = {};
    reel.shots.forEach(function (shot) {
      if (!byBeat[shot.beat]) byBeat[shot.beat] = { n: 0, t: 0 };
      byBeat[shot.beat].n++;
      byBeat[shot.beat].t += shot.duration;
    });
    var slowest = null;
    var fastest = null;
    Object.keys(byBeat).forEach(function (beat) {
      var b = byBeat[beat];
      var rate = 60 * b.n / b.t;
      cutting.push(beat + ': ' + b.n + ' shot' + (b.n === 1 ? '' : 's') + ', ' +
        rate.toFixed(1) + ' cuts a minute.');
      if (beat === 'title' || beat === 'end') return;
      if (!slowest || rate < slowest.rate) slowest = { beat: beat, rate: rate };
      if (!fastest || rate > fastest.rate) fastest = { beat: beat, rate: rate };
    });
    // Named from this film's own numbers rather than from the rule. Not every
    // spine has a 'choice' in it, and a panel that explains a beat the film does
    // not contain is worse than one that explains nothing.
    if (fastest && slowest && fastest.beat !== slowest.beat) {
      cutting.push('Fastest at the ' + fastest.beat + ', slowest at the ' + slowest.beat +
        ' — the film tightens towards the worst moment and then sits in what follows it.');
    }
    var cutaways = reel.shots.filter(function (s) { return s.cutaway; }).length;
    if (cutaways) {
      cutaways = cutaways + ' cutaway' + (cutaways === 1 ? '' : 's');
      cutting.push(cutaways + ' away from the speaker and onto the thing they are talking around.');
    }
    out.push({ title: 'How it is cut', lines: cutting });

    /* -------------------------------------------------------------- the sound */
    var sound = [];
    if (CONDUCTOR && CONDUCTOR.request) {
      var request = CONDUCTOR.request(reel);
      sound.push('SONG FORGE writes the music: ' + request.bpm + ' beats a minute, ' +
        request.genre + ', ' + request.mood + '.');
      sound.push('That tempo is picked so the bars land on the cuts rather than across them.');
      sound.push('A section per scene — ' + request.sections.map(function (s) { return s.type; }).join(', ') + '.');
    }
    if (WORLD) {
      var seen = {};
      script.scenes.forEach(function (scene) {
        var shots = sceneShots(reel, scene.number);
        var setKey = shots.length ? shots[0].set : 'room';
        if (seen[setKey]) return;
        seen[setKey] = true;
        var air = WEATHER && WEATHER.forShot
          ? WEATHER.forShot(reel.genre, scene.heading.time, setKey) : 'none';
        sound.push(WORLD.describe(setKey, air));
      });
    }
    out.push({ title: 'What you hear', lines: sound });

    return out;
  }

  /* The whole thing as plain text, for copying out. */
  function toText(script, reel) {
    return explain(script, reel).map(function (section) {
      return section.title.toUpperCase() + '\n' +
        section.lines.map(function (l) { return '  ' + l; }).join('\n');
    }).join('\n\n');
  }

  var API = { explain: explain, toText: toText };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmWhy = API;
})(typeof window !== 'undefined' ? window : globalThis);
