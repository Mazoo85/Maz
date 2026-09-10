/*
 * SCRIPT FORGE — the dialogue bank.
 * ---------------------------------
 * Dialogue is written as whole *exchanges*, not single lines, so what the
 * characters say to each other actually follows on. Each exchange belongs to
 * a story beat, which is what keeps a line from the ending turning up in the
 * opening.
 *
 *   { who: 'hero' | 'other', line: '...', paren: '(optional)' }
 *
 * Slots are the same ones the lexicon uses: {HERO} {OTHER} {OBJ} {PLACE} {WANT}
 *
 * Exposed as window.FILM_DIALOGUE (and module.exports for the tests).
 */
(function (root) {
  'use strict';

  /* Exchanges that work in any genre. Every beat has several so two runs of
   * the same idea do not sound alike. */
  var SHARED = {
    open: [
      [
        { who: 'other', line: "You're here early." },
        { who: 'hero', line: "I'm always here early." },
        { who: 'other', line: "That's not the brag you think it is." }
      ],
      [
        { who: 'hero', line: 'Same as yesterday?' },
        { who: 'other', line: 'Same as yesterday.' },
        { who: 'hero', paren: '(flat)', line: 'Great.' }
      ],
      [
        { who: 'other', line: 'You ever think about doing something else?' },
        { who: 'hero', paren: '(beat)', line: 'No.' },
        { who: 'other', line: 'Liar.' }
      ],
      [
        { who: 'other', line: 'You look tired.' },
        { who: 'hero', line: 'I look like this.' }
      ],
      [
        { who: 'other', line: 'You going to sit down or just stand there being a mood?' },
        { who: 'hero', line: "I'm working up to sitting down." }
      ]
    ],
    spark: [
      [
        { who: 'other', line: 'Where did you get that?' },
        { who: 'hero', line: 'It was just... there.' },
        { who: 'other', line: "Things aren't just there, {HERO}." }
      ],
      [
        { who: 'hero', line: 'Is this yours?' },
        { who: 'other', line: "Never seen it before in my life." },
        { who: 'hero', line: 'Then why did you go quiet?' }
      ],
      [
        { who: 'other', line: 'Put it back.' },
        { who: 'hero', line: 'Why?' },
        { who: 'other', line: "Because I'm asking you nicely, and I only do that once." }
      ],
      [
        { who: 'hero', paren: '(to the {OBJ})', line: 'Okay. What are you.' },
        { who: 'other', paren: '(off)', line: 'Who are you talking to?' },
        { who: 'hero', line: 'Nobody.' }
      ]
    ],
    push: [
      [
        { who: 'other', line: "Tell me you didn't." },
        { who: 'hero', line: "I didn't." },
        { who: 'other', line: '{HERO}.' },
        { who: 'hero', line: 'I did.' }
      ],
      [
        { who: 'hero', line: 'Ten minutes. In, out, done.' },
        { who: 'other', line: 'You said that last time.' },
        { who: 'hero', line: 'And I was right last time.' }
      ],
      [
        { who: 'other', line: 'This is the part where we stop.' },
        { who: 'hero', paren: '(already moving)', line: 'This is the part where you stop.' }
      ],
      [
        { who: 'hero', line: 'If it goes bad, you were never here.' },
        { who: 'other', line: "That's not how it works and you know it." }
      ]
    ],
    turn: [
      [
        { who: 'other', line: 'How long have you known?' },
        { who: 'hero', line: 'Long enough not to tell you.' }
      ],
      [
        { who: 'other', line: 'You were never going to ask me. Were you.' },
        { who: 'hero', line: "I'm asking now." },
        { who: 'other', line: 'Now is late.' }
      ],
      [
        { who: 'hero', line: 'What do you want?' },
        { who: 'other', line: "The same thing you want. That's the problem." }
      ],
      [
        { who: 'other', line: "I've been standing here for a while, {HERO}." },
        { who: 'hero', line: 'How long is a while?' },
        { who: 'other', line: 'Long enough to hear all of it.' }
      ]
    ],
    crisis: [
      [
        { who: 'hero', line: 'I can fix it.' },
        { who: 'other', line: "You keep saying that like it's a plan." }
      ],
      [
        { who: 'other', line: 'Say it.' },
        { who: 'hero', line: "I can't." },
        { who: 'other', line: "Then we're done here." }
      ],
      [
        { who: 'hero', line: "I didn't mean for it to go like this." },
        { who: 'other', line: 'Nobody ever does. That is why it keeps going like this.' }
      ],
      [
        { who: 'hero', paren: '(quietly)', line: "I don't know what I'm doing." },
        { who: 'other', line: 'Finally. Something true.' }
      ]
    ],
    choice: [
      [
        { who: 'hero', line: "I'm going to say something, and I need you to let me finish." },
        { who: 'other', line: 'Okay.' },
        { who: 'hero', line: 'I want to {WANT}.' }
      ],
      [
        { who: 'other', line: "You don't have to do this." },
        { who: 'hero', line: "I do. That's the whole of it." }
      ],
      [
        { who: 'hero', line: 'Okay.' },
        { who: 'other', line: 'Okay?' },
        { who: 'hero', line: 'Okay.' }
      ],
      [
        { who: 'other', line: 'And if it goes wrong?' },
        { who: 'hero', line: 'Then it goes wrong with me telling the truth in it.' }
      ]
    ],
    after: [
      [
        { who: 'other', line: 'You alright?' },
        { who: 'hero', line: 'Ask me tomorrow.' },
        { who: 'other', line: "I'm asking now." },
        { who: 'hero', paren: '(beat)', line: 'Yeah.' }
      ],
      [
        { who: 'hero', line: "It's still here." },
        { who: 'other', line: 'So are you.' }
      ],
      [
        { who: 'other', line: 'Same as yesterday?' },
        { who: 'hero', line: 'No.' }
      ]
    ]
  };

  /* Genre tints. These are added to the shared pool when the genre matches, so
   * a horror spark can sound like horror and a comedy spark cannot. */
  var BY_GENRE = {
    thriller: {
      spark: [[
        { who: 'hero', line: 'Whose is this?' },
        { who: 'other', line: "Don't. Whatever you're about to do — don't." }
      ]],
      turn: [[
        { who: 'other', line: 'They know where you sleep, {HERO}.' },
        { who: 'hero', line: 'Then I stop sleeping.' }
      ]],
      choice: [[
        { who: 'hero', line: 'Give me forty seconds and then run.' },
        { who: 'other', line: 'And you?' },
        { who: 'hero', line: "I'm the forty seconds." }
      ]]
    },
    horror: {
      spark: [[
        { who: 'other', line: 'Did you hear that?' },
        { who: 'hero', line: 'No.' },
        { who: 'other', line: "You did. You've gone the colour of the wall." }
      ]],
      crisis: [[
        { who: 'hero', line: "It's not in the house." },
        { who: 'other', line: 'Then where—' },
        { who: 'hero', line: "It's in the walls of the house." }
      ]],
      choice: [[
        { who: 'hero', line: "I'm going to open the door." },
        { who: 'other', line: 'Please do not open the door.' },
        { who: 'hero', line: 'It stops when someone does.' }
      ]]
    },
    comedy: {
      open: [[
        { who: 'other', line: 'On a scale of one to a small fire, how are we?' },
        { who: 'hero', line: 'Small fire.' },
        { who: 'other', line: 'Cool. Cool cool cool.' }
      ]],
      spark: [[
        { who: 'hero', line: 'Okay, before you say anything—' },
        { who: 'other', line: "I haven't said anything." },
        { who: 'hero', line: "You're saying it with your entire face." }
      ]],
      choice: [[
        { who: 'hero', line: "I'm going to do the humiliating thing." },
        { who: 'other', line: 'Thank god. I brought a camera.' }
      ]]
    },
    romance: {
      open: [[
        { who: 'other', line: 'You always sit here.' },
        { who: 'hero', line: "It's a good chair." },
        { who: 'other', line: "It's a terrible chair." }
      ]],
      turn: [[
        { who: 'other', line: 'Say the thing you keep not saying.' },
        { who: 'hero', line: "I don't know how it ends." },
        { who: 'other', line: 'Nobody is asking you for the end.' }
      ]],
      choice: [[
        { who: 'hero', line: 'I would like to be somewhere you are. Regularly.' },
        { who: 'other', paren: '(trying it out loud)', line: 'Regularly.' }
      ]]
    },
    scifi: {
      spark: [[
        { who: 'hero', line: 'Say that again.' },
        { who: 'other', line: 'It said your name. Before you got here.' }
      ]],
      turn: [[
        { who: 'hero', line: 'How much of this is real?' },
        { who: 'other', line: 'Define real and I will tell you.' }
      ]],
      choice: [[
        { who: 'hero', line: 'Shut it down.' },
        { who: 'other', line: 'It will not come back.' },
        { who: 'hero', line: 'I know.' }
      ]]
    },
    mystery: {
      spark: [[
        { who: 'hero', line: 'This date is wrong.' },
        { who: 'other', line: "It's a receipt, {HERO}, not a confession." },
        { who: 'hero', line: 'Everything is a confession if you hold it right.' }
      ]],
      turn: [[
        { who: 'other', line: 'You should stop asking.' },
        { who: 'hero', line: "That's the third time you've told me that." }
      ]],
      crisis: [[
        { who: 'hero', line: 'I had it. I had all of it.' },
        { who: 'other', line: 'You had the story you wanted. Not the same thing.' }
      ]]
    },
    heist: {
      push: [[
        { who: 'other', line: 'Say the plan back to me.' },
        { who: 'hero', line: 'In at nine. Out at nine-oh-two.' },
        { who: 'other', line: 'And if it is nine-oh-three?' },
        { who: 'hero', line: "Then you didn't know me." }
      ]],
      crisis: [[
        { who: 'hero', line: 'Somebody talked.' },
        { who: 'other', line: 'Somebody always talks. Question is who to.' }
      ]]
    },
    drama: {
      turn: [[
        { who: 'other', line: 'You were not there.' },
        { who: 'hero', line: 'I know.' },
        { who: 'other', line: 'That is the whole sentence. There is no rest of it.' }
      ]],
      choice: [[
        { who: 'hero', line: "I'm not asking you to forgive me." },
        { who: 'other', line: 'Then what are you asking?' },
        { who: 'hero', line: 'To stay in the room another minute.' }
      ]]
    },
    fantasy: {
      spark: [[
        { who: 'other', line: 'That is not a thing you should be holding.' },
        { who: 'hero', line: 'It was holding me first.' }
      ]],
      choice: [[
        { who: 'hero', line: 'Then it takes what it takes.' },
        { who: 'other', line: 'It takes the part of you that remembers me.' }
      ]]
    },
    western: {
      turn: [[
        { who: 'other', line: 'Road out of here goes one direction.' },
        { who: 'hero', line: 'Then I will walk it one direction.' }
      ]],
      choice: [[
        { who: 'hero', line: 'I am done running at things sideways.' },
        { who: 'other', line: 'Front-on gets a man shot.' },
        { who: 'hero', line: 'Sideways got me here.' }
      ]]
    }
  };

  var DLG = { SHARED: SHARED, BY_GENRE: BY_GENRE };

  if (typeof module === 'object' && module.exports) module.exports = DLG;
  root.FILM_DIALOGUE = DLG;
})(typeof globalThis !== 'undefined' ? globalThis : this);
