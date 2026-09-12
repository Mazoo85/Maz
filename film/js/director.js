/*
 * SCRIPT FORGE — directing it.
 * ----------------------------
 * Until now a film was a slot machine: press the button, get a film, press it
 * again, get a different one. Every good film you got was a lucky roll you could
 * not refine, and the only control over any of it was the seed.
 *
 * The film is already pure data — a script of elements, which the reel is built
 * from — so all of this is reachable. These are the edits:
 *
 *   editLine          change what somebody says, and the shot re-times to it
 *   renameCharacter   a name, everywhere it appears
 *   setHour           what time of day a scene plays at
 *   deleteScene       cut a scene
 *   moveScene         and move one
 *   reroll            write a new film, keeping the scenes you liked
 *
 * Two rules make the whole thing work.
 *
 * SCENES ARE THE TRUTH. A script carries its scenes AND a flat list of elements
 * for the page, and they are separate objects holding the same text — the reel
 * reads the scenes, the screenplay view reads the flat list. Edit one and the
 * other still holds the old words, so an edit that looked right on the page
 * would not be in the film. So every edit here goes through the scenes and
 * `rebuild` regenerates the flat list from them. There is one source of truth
 * and it is not the one that gets rendered.
 *
 * NOTHING IS EDITED IN PLACE. Every function returns a new script and leaves the
 * one it was given alone, which is what makes undo a variable rather than a
 * feature.
 *
 * Exposed as window.FilmDirector (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var WRITER = root.FilmWriter || (typeof require !== 'undefined' ? require('./screenplay.js') : {});

  function cloneElement(el) {
    var copy = {};
    Object.keys(el).forEach(function (k) { copy[k] = el[k]; });
    return copy;
  }

  function cloneScene(scene) {
    return {
      number: scene.number,
      beat: scene.beat,
      heading: {
        int: scene.heading.int,
        place: scene.heading.place,
        time: scene.heading.time,
        continuous: scene.heading.continuous,
        text: scene.heading.text
      },
      elements: scene.elements.map(cloneElement),
      shots: scene.shots ? scene.shots.slice() : []
    };
  }

  function cloneScript(script) {
    var copy = {};
    Object.keys(script).forEach(function (k) { copy[k] = script[k]; });
    copy.scenes = script.scenes.map(cloneScene);
    copy.characters = script.characters.map(function (c) {
      return { name: c.name, role: c.role, part: c.part };
    });
    return copy;
  }

  /* Put the flat element list, the scene numbers and the page count back in
   * agreement with the scenes. Every edit ends here. */
  function rebuild(script) {
    var elements = [{ type: 'transition', text: 'FADE IN:' }];
    script.scenes.forEach(function (scene, i) {
      scene.number = i + 1;
      // The heading element is the scene's own first element; keep its text in
      // step with the heading it came from, since setHour and friends change it.
      if (scene.elements.length && scene.elements[0].type === 'scene_heading') {
        scene.elements[0].text = scene.heading.text;
      }
      // Transitions are NORMALISED rather than preserved. The writer hangs
      // "FADE OUT." off the end of its last scene, which is fine until somebody
      // moves that scene to the middle and the film fades out halfway through.
      // A film fades in at its start and out at its end, whatever survived the
      // cutting, so the scenes keep the words and this decides the bookends.
      scene.elements = scene.elements.filter(function (el) { return el.type !== 'transition'; });
      scene.elements.forEach(function (el) { elements.push(el); });
    });
    elements.push({ type: 'transition', text: 'FADE OUT.' });
    script.elements = elements;
    if (WRITER.paginate) {
      var paged = WRITER.paginate(elements);
      script.pages = paged.pages;
      script.runtime = paged.runtime;
    }
    return script;
  }

  /* Which element of which scene: the flat index the page renders is not a
   * stable address, because deleting a scene moves every index after it. */
  function locate(script, sceneNumber, elementIndex) {
    var scene = script.scenes[sceneNumber - 1];
    if (!scene) return null;
    var el = scene.elements[elementIndex];
    return el ? { scene: scene, element: el } : null;
  }

  /* Change what somebody says, or what an action line describes. The reel takes
   * a shot's length from its word count, so the film re-times to the new line
   * on its own. */
  function editLine(script, sceneNumber, elementIndex, text) {
    var next = cloneScript(script);
    var at = locate(next, sceneNumber, elementIndex);
    if (!at) return next;
    if (at.element.type === 'scene_heading' || at.element.type === 'character') return next;
    var clean = String(text).replace(/\s+/g, ' ').trim();
    if (!clean) return next;
    at.element.text = clean;
    // A line somebody wrote is no longer the writer's to overwrite on a reroll.
    at.element.edited = true;
    return rebuild(next);
  }

  /* A name, everywhere. Whole words only: renaming SAM must not turn SAMPLE into
   * <new>PLE, and the cue, the dialogue and the action all have to move together
   * or a character answers to two names in the same scene. */
  function renameCharacter(script, from, to) {
    var next = cloneScript(script);
    var oldName = String(from);
    var newName = String(to).replace(/\s+/g, ' ').trim().toUpperCase();
    if (!newName || newName === oldName) return next;
    var pattern = new RegExp('\\b' + oldName.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', 'g');

    next.scenes.forEach(function (scene) {
      scene.elements.forEach(function (el) {
        el.text = String(el.text).replace(pattern, newName);
      });
    });
    next.characters.forEach(function (c) {
      if (c.name === oldName) c.name = newName;
    });
    if (next.premise) {
      var premise = {};
      Object.keys(next.premise).forEach(function (k) { premise[k] = next.premise[k]; });
      if (premise.hero && premise.hero.name === oldName) {
        premise.hero = { name: newName, role: premise.hero.role };
      }
      if (premise.other && premise.other.name === oldName) {
        premise.other = { name: newName, role: premise.other.role };
      }
      next.premise = premise;
    }
    next.title = String(next.title).replace(pattern, newName);
    next.logline = String(next.logline).replace(pattern, newName);
    return rebuild(next);
  }

  var HOURS = ['NIGHT', 'DAWN', 'DAY', 'DUSK'];

  /* What time of day a scene plays at. The artist lights the set from this, so
   * it is the single biggest thing you can change about how a scene looks
   * without touching a word of it. */
  function setHour(script, sceneNumber, hour) {
    var next = cloneScript(script);
    var scene = next.scenes[sceneNumber - 1];
    if (!scene || HOURS.indexOf(hour) === -1) return next;
    scene.heading.time = hour;
    scene.heading.continuous = false;
    scene.heading.text = scene.heading.int + ' ' + scene.heading.place.slug + ' — ' + hour;
    return rebuild(next);
  }

  function deleteScene(script, sceneNumber) {
    var next = cloneScript(script);
    if (next.scenes.length <= 1) return next;      // a film needs a scene
    if (sceneNumber < 1 || sceneNumber > next.scenes.length) return next;
    next.scenes.splice(sceneNumber - 1, 1);
    return rebuild(next);
  }

  function moveScene(script, sceneNumber, delta) {
    var next = cloneScript(script);
    var from = sceneNumber - 1;
    var to = from + delta;
    if (from < 0 || from >= next.scenes.length) return next;
    if (to < 0 || to >= next.scenes.length) return next;
    var scene = next.scenes.splice(from, 1)[0];
    next.scenes.splice(to, 0, scene);
    return rebuild(next);
  }

  /* Write a new film and keep the scenes that were locked.
   *
   * `locked` is a list of scene NUMBERS in the script you are reading from. The
   * new film may be a different shape — a different spine is drawn per seed —
   * so a locked scene is kept at its own position where the new film is long
   * enough to have one, and appended where it is not. Losing a scene somebody
   * deliberately kept would be the worst possible outcome of a button called
   * "keep this".
   */
  function reroll(script, opts, locked) {
    opts = opts || {};
    if (!WRITER.write || !script.premise) return cloneScript(script);
    var keep = (locked || []).slice().sort(function (a, b) { return a - b; });
    var fresh = WRITER.write(script.premise, {
      length: opts.length || script.length,
      seed: opts.seed
    });
    if (!keep.length) return fresh;

    var next = cloneScript(fresh);
    keep.forEach(function (number) {
      var original = script.scenes[number - 1];
      if (!original) return;
      var at = number - 1;
      if (at < next.scenes.length) next.scenes[at] = cloneScene(original);
      else next.scenes.push(cloneScene(original));
    });
    return rebuild(next);
  }

  var API = {
    HOURS: HOURS,
    rebuild: rebuild,
    cloneScript: cloneScript,
    editLine: editLine,
    renameCharacter: renameCharacter,
    setHour: setHour,
    deleteScene: deleteScene,
    moveScene: moveScene,
    reroll: reroll
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmDirector = API;
})(typeof window !== 'undefined' ? window : globalThis);
