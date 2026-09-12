/*
 * SCRIPT FORGE — what they are not saying.
 * ----------------------------------------
 * The dialogue bank is direct. Characters ask what they want to know and answer
 * what they were asked, which is how nobody has ever spoken to anybody about
 * anything that mattered. It is also the difference a reader feels instantly
 * and cannot name: dialogue that states its business reads as assembled, and
 * dialogue that dodges reads as written.
 *
 * The fix is NOT to rewrite the existing lines. A mechanical "make this evasive"
 * pass over hand-written dialogue loses the specificity that made the line worth
 * having and leaves a worse line behind. Subtext is not a way of phrasing a
 * line, it is a choice about what gets said INSTEAD — so these are whole
 * exchanges written with the dodge already in them, drawn alongside the direct
 * bank rather than replacing it.
 *
 * Every one comes with a TELL: an action line, placed straight after, that shows
 * what the words just denied. That pairing is the entire technique —
 *
 *   HERO:  It doesn't matter to me.
 *   OTHER: Right.
 *   > It is the third time {HERO} has said so tonight.
 *
 * — and it is why these live together in one file instead of the dialogue going
 * in one bank and the action in another. Split them up and the pairing is the
 * first thing to rot.
 *
 * A tell has to be photographable. "{HERO} already knows what they would do" is
 * a novel; "{HERO} answers too quickly" is a shot. The tests enforce that.
 *
 * Exposed as window.FilmSubtext (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* Beats where a dodge belongs. Not the opening — a film that starts evasive
   * has nothing to become — and not the closing scene, where the whole point is
   * that somebody finally says it. */
  var BEATS = ['spark', 'push', 'turn', 'crisis', 'choice'];

  var EXCHANGES = {
    spark: [
      {
        lines: [
          { who: 'other', line: "You're not going to ask me about it." },
          { who: 'hero', line: 'No.' },
          { who: 'other', line: 'Okay.' }
        ],
        tell: '{HERO} waits until {OTHER} is gone before looking at the {OBJ}.'
      },
      {
        lines: [
          { who: 'hero', line: "It's fine." },
          { who: 'other', line: "I didn't ask." },
          { who: 'hero', line: 'I know.' }
        ],
        tell: 'Neither of them moves.'
      },
      {
        lines: [
          { who: 'other', line: 'Is this about last time?' },
          { who: 'hero', line: "There's nothing for it to be about." }
        ],
        tell: '{HERO} says it to the middle distance.'
      },
      {
        lines: [
          { who: 'other', line: 'You slept.' },
          { who: 'hero', line: 'Some.' },
          { who: 'other', paren: '(not a question)', line: 'Some.' }
        ],
        tell: 'The {PLACE} is exactly as {OTHER} left it last night.'
      }
    ],
    push: [
      {
        lines: [
          { who: 'hero', line: "I'll be an hour." },
          { who: 'other', line: 'You said that yesterday.' },
          { who: 'hero', line: 'And I was.' }
        ],
        tell: '{HERO} picks up a coat and does not put it on.'
      },
      {
        lines: [
          { who: 'other', line: 'You could just leave it.' },
          { who: 'hero', line: 'I could.' }
        ],
        tell: '{HERO} does not leave it.'
      },
      {
        lines: [
          { who: 'hero', line: "It doesn't matter to me." },
          { who: 'other', line: 'Right.' }
        ],
        tell: 'It is the third time {HERO} has said so {TONIGHT}.'
      },
      {
        lines: [
          { who: 'other', line: 'Where are you going?' },
          { who: 'hero', line: 'Out.' },
          { who: 'other', line: 'Out.' }
        ],
        tell: 'Both of them look at the door. Neither goes through it yet.'
      }
    ],
    turn: [
      {
        lines: [
          { who: 'other', line: "Tell me you don't want it." },
          { who: 'hero', line: "I don't want it." },
          { who: 'other', line: 'Again.' }
        ],
        tell: '{HERO} does not say it again.'
      },
      {
        lines: [
          { who: 'hero', line: "We're fine." },
          { who: 'other', paren: '(flat)', line: 'Are we.' }
        ],
        tell: 'Nobody answers that.'
      },
      {
        lines: [
          { who: 'other', line: 'What would you do, if it were yours?' },
          { who: 'hero', line: "It isn't mine." }
        ],
        tell: '{HERO} answers too quickly.'
      },
      {
        lines: [
          { who: 'hero', line: 'Say what you came to say.' },
          { who: 'other', line: "I did." }
        ],
        tell: '{OTHER} has not sat down, and does not.'
      }
    ],
    crisis: [
      {
        lines: [
          { who: 'hero', line: "I'm not angry." },
          { who: 'other', line: "I didn't say you were." }
        ],
        tell: '{HERO}’s hands are shaking, and both of them can see it.'
      },
      {
        lines: [
          { who: 'other', line: 'You can stop any time you want.' },
          { who: 'hero', line: 'I know.' }
        ],
        tell: '{HERO} does not stop.'
      },
      {
        lines: [
          { who: 'other', line: 'What happened?' },
          { who: 'hero', line: 'Nothing happened.' },
          { who: 'other', line: 'Look at me and say that.' }
        ],
        tell: '{HERO} looks at the floor.'
      }
    ],
    choice: [
      {
        lines: [
          { who: 'other', line: 'Last chance to walk away.' },
          { who: 'hero', line: 'I heard you the first time.' }
        ],
        tell: '{HERO} does not walk away.'
      },
      {
        lines: [
          { who: 'hero', line: "It's not a decision. It's just what happens next." },
          { who: 'other', line: 'Sure.' }
        ],
        tell: '{HERO} takes a long time over something that takes no time at all.'
      },
      {
        lines: [
          { who: 'other', line: 'You already know what you are going to do.' },
          { who: 'hero', line: 'Then why are we talking.' }
        ],
        tell: 'The {OBJ} sits between them, and stays there.'
      }
    ]
  };

  function hasBeat(beatId) {
    return Object.prototype.hasOwnProperty.call(EXCHANGES, beatId) && EXCHANGES[beatId].length > 0;
  }

  /* One dodge for this beat, or null if the beat does not take one.
   *
   * `rng` is the writer's own stream, so a film that draws a subtext exchange
   * here draws one fewer neutral exchange there — the two banks share a budget
   * rather than the film getting longer.
   */
  function pick(beatId, rng) {
    if (!hasBeat(beatId)) return null;
    var list = EXCHANGES[beatId];
    return list[Math.floor(rng() * list.length) % list.length];
  }

  /* Every exchange in the bank, flattened — for the tests, and for anything that
   * wants to count how much the program can say. */
  function all() {
    var out = [];
    BEATS.forEach(function (beat) {
      (EXCHANGES[beat] || []).forEach(function (ex) { out.push({ beat: beat, exchange: ex }); });
    });
    return out;
  }

  var API = {
    BEATS: BEATS,
    EXCHANGES: EXCHANGES,
    hasBeat: hasBeat,
    pick: pick,
    all: all
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  root.FilmSubtext = API;
})(typeof window !== 'undefined' ? window : globalThis);
