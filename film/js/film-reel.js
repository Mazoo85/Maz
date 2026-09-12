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

  /* How long the film lets a shot sit, by beat. A film that cuts at one rate
   * from the title to the end card has no build in it: the crisis plays at the
   * speed of the opening, which is why the opening feels hurried and the crisis
   * feels calm -- exactly backwards.
   *
   * Under 1 is quicker than natural, over 1 is a hold. The shape is a squeeze:
   * the film tightens through the middle, snaps at the crisis, and then the
   * choice is the longest shot in the picture, because the only way to make an
   * audience feel a decision is to make them sit in it.
   *
   * These scale a shot's duration but never below its floor. A caption still has
   * to be readable at the crisis, and an unreadable line is not tension, it is a
   * mistake. */
  var PACE = { open: 1.12, spark: 1.0, push: 0.9, turn: 0.84, crisis: 0.76, choice: 1.3, after: 1.15 };

  /* Reading speed. A caption has to be on screen long enough to actually read
   * it — comfortable is about three words a second, and a caption that shares
   * the frame with a picture wants to be slower than that, not faster. */
  var ACTION_SECONDS_PER_WORD = 0.38;
  var LINE_SECONDS_PER_WORD = 0.40;
  var MIN_ACTION = 2.4;
  /* The fastest a caption may ever go by, whatever the pace says. An unreadable
   * line is not tension, it is a mistake -- so acceleration is BOUNDED: the
   * crisis cuts from 2.6 words a second up to 3.1, and stops there. What makes
   * the crisis feel faster than that is the cutaways, which carry no words and
   * so have no floor to hit. */
  var READING_FLOOR = 0.32;
  var MIN_LINE = 1.9;
  var TITLE_SECONDS = 3.6;
  var ESTABLISH_SECONDS = 2.8;
  var END_SECONDS = 4.2;

  function words(text) {
    return String(text).trim().split(/\s+/).filter(Boolean).length;
  }

  /* A paced duration, never below what it takes to read the caption on it. */
  function readable(duration, text) {
    var w = words(text || '');
    return w ? Math.max(duration, w * READING_FLOOR) : duration;
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

    /* How a conversation is cut.
     *
     * This used to draw a framing at random per line, with a rule against the
     * same one three times running. That gives VARIETY, which is not the same
     * thing as GRAMMAR, and the difference is the whole reason a two-hander read
     * as two people talking to camera rather than to each other: close, two,
     * over-the-shoulder, close, in no relation to who was speaking.
     *
     * Shot/reverse-shot is the oldest rule in film and it is a rule about
     * MATCHING. When the speaker changes, the camera goes to the answering
     * angle: the reverse of a close is a close, the reverse of an over-the-
     * shoulder is an over-the-shoulder, from the other side. The sides take care
     * of themselves -- each shot carries the speaker's own side of the frame --
     * so matching the framing is the part that has to be deliberate.
     *
     * A run of lines is bracketed by two-shots: one to establish who is standing
     * where before the cutting starts, and another every few lines to put them
     * back in the same room. Without those the audience loses the geometry, and
     * a conversation with no geometry is just alternating portraits.
     */
    var convo = { lastSpeaker: null, lastFraming: null, lines: 0 };
    /* What was actually put on screen, across the whole scene. An exchange ends
     * whenever a line of action interrupts it, so `convo` resets often -- but the
     * AUDIENCE does not reset, and three identical framings running still reads
     * as a stuck camera even if the film considers them three separate
     * conversations. This is the memory that outlives the exchange. */
    var spoken = [];

    function resetConversation() {
      convo.lastSpeaker = null;
      convo.lastFraming = null;
      convo.lines = 0;
    }

    /* Off the speaker and onto the thing they are talking around.
     *
     * This is the cheapest shot in film -- no new art, no new line, one second
     * long -- and at the crisis it is the ONLY way to raise the cut rate. A shot
     * with a caption on it cannot go below reading speed however tense the scene
     * is, so squeezing the pace alone left the crisis cutting slower than the
     * beat before it: exactly backwards, and measured, not guessed. A shot that
     * carries no words has no floor to hit.
     */
    function cutAway(scene, set, light, mood, pace) {
      push({
        kind: 'action',
        duration: 1.1 * pace,
        set: set,
        time: light,
        framing: 'insert',
        camera: 'push-slow',
        caption: '',
        speaker: null,
        characters: [],
        mood: mood,
        scene: scene.number,
        beat: scene.beat.id,
        cutaway: true
      });
    }

    function nextLineFraming(speaker, both) {
      var framing;
      if (!both) {
        // Alone in the frame: nobody to cut against, so this is about size, and
        // it steps through the sizes across the scene rather than restarting at
        // 'mid' every time an action line breaks the run.
        var sizes = ['mid', 'close', 'low'];
        framing = sizes[spoken.length % sizes.length];
      } else if (convo.lines === 0) {
        framing = 'two';                       // establish the geography first
      } else if (convo.lines % 5 === 0) {
        framing = 'two';                       // and re-establish it now and then
      } else if (speaker !== convo.lastSpeaker) {
        // The reverse. Match the previous framing; the side flips with the
        // speaker on its own.
        framing = convo.lastFraming === 'two'
          ? (rng() < 0.55 ? 'ots' : 'close')   // out of the two-shot, pick the pair's register
          : convo.lastFraming;
      } else {
        // Same person, still talking: stay on them, but come off a two-shot.
        framing = convo.lastFraming === 'two' ? 'close' : convo.lastFraming;
      }
      // Last guard, and it is about the audience rather than the grammar: three
      // identical framings running is a stuck camera however well-motivated each
      // one was on its own.
      var n = spoken.length;
      if (n >= 2 && spoken[n - 1] === framing && spoken[n - 2] === framing) {
        var escape = both
          ? (framing === 'two' ? 'ots' : 'two')
          : (framing === 'close' ? 'mid' : 'close');
        framing = escape;
      }

      convo.lastFraming = framing;
      convo.lastSpeaker = speaker;
      convo.lines++;
      spoken.push(framing);
      return framing;
    }

    /* -------------------------------------------------------------- scenes */
    script.scenes.forEach(function (scene) {
      var set = setFor(scene.heading.place);
      var light = lightFor(scene.heading.time);
      var mood = MOOD[scene.beat.id] == null ? 0.4 : MOOD[scene.beat.id];
      var pace = PACE[scene.beat.id] == null ? 1 : PACE[scene.beat.id];

      /* Who has the thing, this scene.
       *
       * The whole story turns on an object and until now no character ever
       * touched one -- the insert shot drew it floating on its own, which is
       * an odd way to film a story about somebody holding something. The
       * writer marks each scene with the object's state (see object-arc.js);
       * three of those states mean it is in the lead's hand. */
      var objectState = null;
      scene.elements.forEach(function (el) { if (el.objectBeat) objectState = el.objectBeat; });
      var inHand = objectState === 'noticed' || objectState === 'carried' ||
                   objectState === 'reclaimed';
      var holder = inHand && script.characters.length
        ? { by: script.characters[0].name, what: script.premise ? script.premise.object : '' }
        : null;
      var onScreen = [];
      var shotsThisScene = 0;
      resetConversation();

      // Establishing shot: the slug line, held, so the audience knows where
      // they are before anyone speaks.
      push({
        kind: 'establish',
        duration: ESTABLISH_SECONDS * pace,
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
          // An action line that names the object earns an insert of it -- and a
          // line the writer MARKED as the object's beat always gets one, because
          // the two arcs whose crisis is about the object being gone do not name
          // it ("The hole is open. There is nothing in it."), and an insert on an
          // empty hole is the whole point of that shot.
          var isInsert = !!element.objectBeat || (script.premise && element.text.toLowerCase()
            .indexOf(String(script.premise.object).toLowerCase()) !== -1 && rng() < 0.7);
          var framing = isInsert ? 'insert' : (rng() < 0.45 ? 'wide' : 'mid');
          // Whoever the line names is in frame for it.
          var present = script.characters
            .filter(function (c) { return element.text.indexOf(c.name) !== -1; })
            .map(function (c) { return c.name; });
          present.forEach(function (n) { if (onScreen.indexOf(n) === -1) onScreen.push(n); });

          // The choice should sit a beat longer than is comfortable.
          var hold = scene.beat.id === 'choice' ? 1.35 : 1;
          // A line of action between two lines of dialogue ends the exchange:
          // whatever is said next starts a new one, and has to re-establish
          // where everybody is standing.
          resetConversation();
          push({
            kind: 'action',
            duration: readable(
              Math.max(MIN_ACTION, words(element.text) * ACTION_SECONDS_PER_WORD) * hold * pace,
              element.text),
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
            holding: framing === 'insert' ? null : holder,
            mood: mood,
            scene: scene.number,
            beat: scene.beat.id
          });
          shotsThisScene++;
          // A crisis is cut, not described. Hold on a long line of action there
          // and the worst moment of the film plays at the speed of the opening.
          if (scene.beat.id === 'crisis' && words(element.text) >= 9 && rng() < 0.7) {
            cutAway(scene, set, light, mood, pace);
            shotsThisScene++;
          }
        } else if (element.type === 'character') {
          if (onScreen.indexOf(element.text) === -1) onScreen.push(element.text);
          shots._pendingSpeaker = element.text;
          shots._pendingParen = null;
        } else if (element.type === 'parenthetical') {
          shots._pendingParen = element.text;
        } else if (element.type === 'dialogue') {
          var speaker = shots._pendingSpeaker || script.characters[0].name;
          var both = onScreen.length > 1;

          // A cutaway, mid-exchange, on the beats that can carry one. Cutting
          // off the speaker and onto the thing they are talking around is how an
          // edit says "look at what this is really about", and it is the single
          // cheapest shot in film: no new art, no new line, one second long.
          var every = scene.beat.id === 'crisis' ? 2 : 3;
          var chance = scene.beat.id === 'crisis' ? 0.85 : 0.6;
          if (both && convo.lines >= 2 && convo.lines % every === 0 &&
              (scene.beat.id === 'turn' || scene.beat.id === 'crisis') && rng() < chance) {
            cutAway(scene, set, light, mood, pace);
            shotsThisScene++;
          }

          var lineFraming = nextLineFraming(speaker, both);
          var pair = lineFraming === 'two' || lineFraming === 'ots';
          push({
            kind: 'line',
            // Pace scales the breath after the line, never the reading time: a
            // caption nobody can read is not tension, it is a mistake.
            duration: Math.max(MIN_LINE, words(element.text) * LINE_SECONDS_PER_WORD) + 0.25 * pace,
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
            holding: holder,
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

  /* ------------------------------------------------------------- the file
   * The reel has been kept free of the DOM from the start so that something
   * other than a browser could draw it. This is how it leaves: an explicit,
   * versioned document rather than a dump of whatever the builder happened to
   * put on the object, because a native renderer reading this needs a contract
   * that does not quietly change when the builder gains a field.
   *
   * Every field a renderer needs to place a shot in time, pick its set, light
   * it, frame it, and know who is in it and who is talking. Nothing else.
   */
  var REEL_FORMAT = 'maz-film-reel';
  var REEL_VERSION = 1;

  function exportShot(shot) {
    return {
      index: shot.index,
      start: shot.start,
      duration: shot.duration,
      kind: shot.kind,
      set: shot.set,
      time: shot.time,
      framing: shot.framing,
      camera: shot.camera,
      caption: shot.caption == null ? '' : String(shot.caption),
      subcaption: shot.subcaption == null ? '' : String(shot.subcaption),
      parenthetical: shot.parenthetical == null ? '' : String(shot.parenthetical),
      speaker: shot.speaker == null ? '' : String(shot.speaker),
      side: shot.side == null ? 0 : shot.side,
      characters: (shot.characters || []).slice(),
      mood: shot.mood,
      scene: shot.scene,
      beat: shot.beat
    };
  }

  function toDocument(reel) {
    var voices = {};
    Object.keys(reel.voices).forEach(function (name) {
      var v = reel.voices[name];
      voices[name] = { name: v.name, pitch: v.pitch, rate: v.rate, hue: v.hue, side: v.side };
    });
    return {
      format: REEL_FORMAT,
      version: REEL_VERSION,
      title: reel.title,
      genre: reel.genre,
      genreLabel: reel.genreLabel,
      seed: reel.seed,
      duration: reel.duration,
      object: reel.object,
      characters: reel.characters.map(function (c) {
        return { name: c.name, role: c.role, part: c.part };
      }),
      voices: voices,
      shots: reel.shots.map(exportShot)
    };
  }

  /* JSON.stringify escapes quotes, backslashes and control characters itself,
   * so a caption carrying the user's own punctuation cannot break the file. */
  function toJson(reel, indent) {
    return JSON.stringify(toDocument(reel), null, indent === undefined ? 2 : indent);
  }

  var API = { build: build, setFor: setFor, clock: clock, shotAt: shotAt, SET_BY_PLACE: SET_BY_PLACE, MOOD: MOOD,
    toJson: toJson, toDocument: toDocument, REEL_FORMAT: REEL_FORMAT, REEL_VERSION: REEL_VERSION };
  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmReel = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
