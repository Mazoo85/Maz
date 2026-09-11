/*
 * SCRIPT FORGE — the director.
 * ----------------------------
 * Turns a finished script into a *reel*: an ordered list of timed shots, each
 * one saying what the camera sees, for how long, what is heard over it and how
 * tense the moment is. Everything downstream — the artist, the score and the
 * recorder — reads the reel and nothing else.
 *
 *   { start, duration, kind, set, framing, camera, caption, speaker, mood }
 *
 * No DOM and no audio here, so the whole edit of a film can be checked in Node.
 *
 * Exposed as window.FilmReel (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  var PARSE = root.FilmParse || (typeof require !== 'undefined' ? require('./parse.js') : {});

  /* ------------------------------------------------------------------ sets
   * Eighty-odd locations, fourteen buildable sets. A scene heading picks the
   * set; the artist knows how to draw each one.
   */
  var SET_BY_PLACE = {
    lighthouse: 'lighthouse',
    kitchen: 'kitchen',
    hallway: 'corridor', stairwell: 'corridor', elevator: 'corridor',
    subway: 'corridor', 'train station': 'corridor', airport: 'corridor',
    prison: 'corridor',
    woods: 'woods', forest: 'woods', park: 'woods', graveyard: 'woods',
    cemetery: 'woods', ruins: 'woods',
    street: 'street', city: 'street', town: 'street', alley: 'street',
    'parking lot': 'street', 'gas station': 'street', bridge: 'street',
    rooftop: 'street', roof: 'street', porch: 'street', yard: 'street',
    field: 'field', desert: 'field', mountain: 'field', farm: 'field',
    ranch: 'field', barn: 'field',
    car: 'vehicle', truck: 'vehicle', bus: 'vehicle', train: 'vehicle',
    warehouse: 'industrial', factory: 'industrial', garage: 'industrial',
    basement: 'industrial', 'parking garage': 'industrial', attic: 'industrial',
    office: 'office', bank: 'office', 'police station': 'office', lab: 'office',
    laboratory: 'office', 'server room': 'office', library: 'office',
    classroom: 'office', school: 'office', studio: 'office', theater: 'office',
    bar: 'bar', saloon: 'bar', diner: 'bar', restaurant: 'bar', cafe: 'bar',
    'coffee shop': 'bar', store: 'bar', shop: 'bar', laundromat: 'bar', gym: 'bar',
    spaceship: 'ship', station: 'ship', space: 'ship',
    dock: 'water', boat: 'water', beach: 'water', lake: 'water', river: 'water',
    island: 'water',
    hospital: 'ward', clinic: 'ward', 'waiting room': 'ward',
    church: 'chapel', cottage: 'room', cabin: 'room', house: 'room',
    apartment: 'room', bedroom: 'room', 'living room': 'room', bathroom: 'room',
    motel: 'room', 'motel room': 'room', hotel: 'room'
  };

  function setFor(place) {
    var direct = SET_BY_PLACE[place.key];
    if (direct) return direct;
    return place.int === 'EXT.' ? 'street' : 'room';
  }

  /* How wound-up each beat is. Drives the score, the light and the cutting. */
  var MOOD = { open: 0.15, spark: 0.38, push: 0.52, turn: 0.62, crisis: 0.88, choice: 0.5, after: 0.18 };

  /* Reading speed. A caption has to be on screen long enough to actually read
   * it — comfortable is about three words a second, and a caption that shares
   * the frame with a picture wants to be slower than that, not faster. */
  var ACTION_SECONDS_PER_WORD = 0.38;
  var LINE_SECONDS_PER_WORD = 0.40;
  var MIN_ACTION = 2.4;
  var MIN_LINE = 1.9;
  var TITLE_SECONDS = 3.6;
  var ESTABLISH_SECONDS = 2.8;
  var END_SECONDS = 4.2;

  function words(text) {
    return String(text).trim().split(/\s+/).filter(Boolean).length;
  }

  /* A voice and a silhouette colour per character, from the name, so the same
   * character sounds and looks the same every time the film is rebuilt. */
  function voiceFor(name, index) {
    var h = PARSE.hashText(String(name));
    // The lead speaks lower than the foil, so two voices are never confused,
    // and each name lands on its own shade within that range.
    // Wide enough apart that nobody has to wonder who is talking.
    var base = index === 0 ? 112 : 186;
    var spread = index === 0 ? 6 : 8;
    return {
      name: name,
      pitch: base + (h % 6) * spread,
      rate: 0.9 + ((h >>> 3) % 5) * 0.05,
      hue: (h >>> 6) % 360,
      side: index === 0 ? -1 : 1 // which side of the frame they stand on
    };
  }

  function build(script, opts) {
    opts = opts || {};
    var rng = PARSE.makeRng((script.seed + 0x5bf03635) >>> 0);
    var voices = {};
    script.characters.forEach(function (c, i) { voices[c.name] = voiceFor(c.name, i); });

    var shots = [];
    var t = 0;
    // "CONTINUOUS" and "LATER" tell a reader the clock, not the light. The
    // artist needs a real time of day, so carry the last concrete one.
    var lastTime = script.premise ? script.premise.time : 'NIGHT';
    function lightFor(headingTime) {
      if (headingTime === 'CONTINUOUS' || headingTime === 'LATER') return lastTime;
      lastTime = headingTime;
      return headingTime;
    }

    function push(shot) {
      shot.start = t;
      shot.index = shots.length;
      t += shot.duration;
      shots.push(shot);
      return shot;
    }

    /* ---------------------------------------------------------- title card */
    push({
      kind: 'title',
      duration: TITLE_SECONDS,
      set: setFor(script.scenes[0].heading.place),
      time: script.premise ? script.premise.time : 'NIGHT',
      framing: 'wide',
      camera: 'push',
      caption: script.title,
      subcaption: script.genreLabel + ' · ' + script.runtime,
      speaker: null,
      characters: [],
      mood: 0.2,
      scene: 0,
      beat: 'title'
    });

    /* A two-hander reads as a conversation when the camera changes sides. Never
     * the same framing three times running. `ots` needs two people in frame, so
     * it is only on the table when both of them are. */
    var spokenFramings = [];
    function nextLineFraming(both) {
      var options = both ? ['two', 'ots', 'close'] : ['close', 'low'];
      var last = spokenFramings[spokenFramings.length - 1];
      var prev = spokenFramings[spokenFramings.length - 2];
      var fresh = options.filter(function (f) { return !(last === f && prev === f); });
      if (!fresh.length) fresh = options;
      var pick = fresh[Math.floor(rng() * fresh.length) % fresh.length];
      spokenFramings.push(pick);
      return pick;
    }

    /* -------------------------------------------------------------- scenes */
    script.scenes.forEach(function (scene) {
      var set = setFor(scene.heading.place);
      var light = lightFor(scene.heading.time);
      var mood = MOOD[scene.beat.id] == null ? 0.4 : MOOD[scene.beat.id];
      var onScreen = [];
      var shotsThisScene = 0;

      // Establishing shot: the slug line, held, so the audience knows where
      // they are before anyone speaks.
      push({
        kind: 'establish',
        duration: ESTABLISH_SECONDS,
        set: set,
        time: light,
        framing: 'wide',
        camera: rng() < 0.5 ? 'pan-r' : 'pan-l',
        caption: scene.heading.text,
        speaker: null,
        characters: [],
        mood: mood,
        scene: scene.number,
        beat: scene.beat.id
      });
      shotsThisScene++;

      scene.elements.forEach(function (element) {
        if (element.type === 'action') {
          // An action line that names the object earns an insert of it.
          var isInsert = script.premise && element.text.toLowerCase()
            .indexOf(String(script.premise.object).toLowerCase()) !== -1 && rng() < 0.7;
          var framing = isInsert ? 'insert' : (rng() < 0.45 ? 'wide' : 'mid');
          // Whoever the line names is in frame for it.
          var present = script.characters
            .filter(function (c) { return element.text.indexOf(c.name) !== -1; })
            .map(function (c) { return c.name; });
          present.forEach(function (n) { if (onScreen.indexOf(n) === -1) onScreen.push(n); });

          // The choice should sit a beat longer than is comfortable.
          var hold = scene.beat.id === 'choice' ? 1.35 : 1;
          push({
            kind: 'action',
            duration: Math.max(MIN_ACTION, words(element.text) * ACTION_SECONDS_PER_WORD) * hold,
            set: set,
            time: light,
            framing: framing,
            // The whip pan is a transition *between* two shots, so it only
            // ever takes over a shot that is not the scene's first.
            camera: (shotsThisScene > 1 && rng() < 0.18) ? 'whip'
                  : scene.beat.id === 'crisis' ? 'handheld'
                  : scene.beat.id === 'push' ? (rng() < 0.5 ? 'track-l' : 'track-r')
                  : framing === 'insert' ? 'push-slow'
                  : (rng() < 0.5 ? 'push' : 'static'),
            caption: element.text,
            speaker: null,
            characters: framing === 'insert' ? [] : present.slice(),
            mood: mood,
            scene: scene.number,
            beat: scene.beat.id
          });
          shotsThisScene++;
        } else if (element.type === 'character') {
          if (onScreen.indexOf(element.text) === -1) onScreen.push(element.text);
          shots._pendingSpeaker = element.text;
          shots._pendingParen = null;
        } else if (element.type === 'parenthetical') {
          shots._pendingParen = element.text;
        } else if (element.type === 'dialogue') {
          var speaker = shots._pendingSpeaker || script.characters[0].name;
          var both = onScreen.length > 1 && rng() < 0.35;
          var lineFraming = nextLineFraming(both);
          var pair = lineFraming === 'two' || lineFraming === 'ots';
          push({
            kind: 'line',
            duration: Math.max(MIN_LINE, words(element.text) * LINE_SECONDS_PER_WORD) + 0.25,
            set: set,
            time: light,
            framing: lineFraming,
            // A tense conversation is unsteady too.
            camera: scene.beat.id === 'crisis' ? 'handheld'
                  : pair ? 'static' : 'push-slow',
            caption: element.text,
            parenthetical: shots._pendingParen || null,
            speaker: speaker,
            side: voices[speaker] ? voices[speaker].side : 0,
            characters: pair ? onScreen.slice(0, 2) : [speaker],
            mood: mood,
            scene: scene.number,
            beat: scene.beat.id
          });
          shotsThisScene++;
          shots._pendingParen = null;
        }
        // Transitions inside the script (FADE IN:/FADE OUT.) are the player's
        // job, not a shot of their own.
      });
    });

    /* ------------------------------------------------------------ end card */
    push({
      kind: 'end',
      duration: END_SECONDS,
      set: setFor(script.scenes[script.scenes.length - 1].heading.place),
      time: lastTime,
      framing: 'wide',
      camera: 'pull',
      caption: 'THE END',
      subcaption: 'written with SCRIPT FORGE',
      speaker: null,
      characters: [],
      mood: 0.15,
      scene: 0,
      beat: 'end'
    });

    delete shots._pendingSpeaker;
    delete shots._pendingParen;

    return {
      title: script.title,
      genre: script.genre,
      genreLabel: script.genreLabel,
      seed: script.seed,
      shots: shots,
      voices: voices,
      characters: script.characters,
      object: script.premise ? script.premise.object : '',
      duration: t
    };
  }

  function clock(seconds) {
    var s = Math.max(0, Math.round(seconds));
    var m = Math.floor(s / 60);
    return m + ':' + (s % 60 < 10 ? '0' : '') + (s % 60);
  }

  /* Which shot is on screen at a given moment. */
  function shotAt(reel, time) {
    for (var i = 0; i < reel.shots.length; i++) {
      var s = reel.shots[i];
      if (time >= s.start && time < s.start + s.duration) return s;
    }
    return reel.shots[reel.shots.length - 1];
  }

  var API = { build: build, setFor: setFor, clock: clock, shotAt: shotAt, SET_BY_PLACE: SET_BY_PLACE, MOOD: MOOD };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmReel = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
