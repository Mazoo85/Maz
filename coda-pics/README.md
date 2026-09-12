<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# CODA PICS

**Type what you want to see. Get a picture back.**

[▶ Open it](https://mazoo85.github.io/Maz/coda-pics/) · [source](index.html)

Type *"a red dragon over snowy mountains at sunset, neon"* and CODA PICS reads
that sentence, builds the scene it describes, and paints it — sky, light,
weather, mountains, the dragon, and a neon finish over the top.

It runs **entirely in your browser**. No account, no API key, no uploads, no
cost, no queue, and it keeps working with the wifi off. Every picture is drawn
from scratch with maths: there are no photographs in this repository and no
model weights to download.

```
cd coda-pics
python3 -m http.server      # then visit http://localhost:8000
# ...or just open index.html directly in a browser
```

## What it can draw

| | |
|---|---|
| **60 subjects** | dragons, whales, wolves, foxes, stags, owls, astronauts, robots, wizards, knights, ghosts, castles, lighthouses, temples, torii gates, windmills, tall ships, rockets, hot air balloons, trains, portals, crystals, great trees, waterfalls, ringed planets… |
| **20 settings** | mountains, forest, jungle, open sea, shore, lake, desert, city, deep space, snow, canyon, swamp, meadow, plains, volcano, ruins, caves, tropical island, an empty road, above the clouds |
| **Every hour** | dawn, daylight, sunset, night — each with its own sky, its own light, and its own colour in the shadows |
| **Weather** | clear, cloudy, rain, falling snow, fog, storms with lightning, the northern lights |
| **14 art styles** | Neon · Pixel art · 16-bit arcade · Watercolour · Film noir · Comic book · Blueprint · Storybook · Woodblock · Poster · Low poly · Stained glass · Oil painting · Soft realism |

Colour words (*a **red** dragon*), size words (*a **giant** wolf*), counts
(*three wolves*, *wolves*) and mood words (*lonely*, *epic*, *peaceful*) all
change what comes out. Two subjects in one sentence — *"a wolf and a castle"* —
puts both in the picture.

**It shows its working.** Under every picture is a row of tags saying what each
of your words was understood as, and which parts it chose for you. If you typed
something it did not know, you can see that immediately instead of guessing.

## Buttons

- **Paint it** — paint the words in the box (or just press Enter).
- **Another take** — same words, different roll of the dice.
- **Surprise me** — writes you a prompt in its own vocabulary.
- **Save the picture** — downloads a full-size PNG.
- **Keep in my gallery** — saves it to this device. The gallery stores the
  *words and the seed*, never the pixels, so a hundred kept pictures cost a few
  kilobytes and each one repaints exactly as it was.

Four shapes: square, wide, tall, and a phone wallpaper.

## How it works

Four steps, four files, each one able to be understood on its own:

```
you type  →  js/lexicon.js   every word the app knows, and nothing else
          →  js/prompt.js    words → a scene: subject, setting, hour, weather, style
          →  js/paint.js     the scene → sky, light, distance, ground, weather
          →  js/subjects.js  the thing itself: 42 routines, shared between 60 subjects
          →  js/finish.js    the style treatment, done on the raw pixels
          →  a picture
```

Two rules hold it together:

1. **Reading is total.** There is no "I didn't understand that". Anything your
   words leave unsaid is decided from a hash of the prompt itself, so even
   nonsense paints something — and paints the same thing every time.
2. **Same words, same picture.** Every random choice comes from the prompt and
   the seed, never from `Math.random`, on any machine. The seed is printed
   under each picture; that is the whole promise the gallery relies on.

The finishing passes (blur, bloom, dithering, edge detection, halftone,
mosaics) are written out by hand on the pixel array rather than handed to
canvas filters, so they behave identically in every browser — and so the tests
can run the real pipeline with no browser at all.

## Checking it

```
node coda-pics/tests/coda-logic.test.js
```

No browser, no dependencies, no network. It checks that every word in the
lexicon leads to a routine that exists, that any text at all produces a
complete drawable scene, that the same prompt paints identically twice, and
then actually paints **every subject, every setting, every style and every
hour/weather pair** against a recording canvas — so a typo in a rarely-chosen
branch fails here rather than in front of somebody typing *"a crab"*.

It runs on every push as part of
[Site CI](../.github/workflows/site-ci.yml), which also drives the live page in
a real Chromium.

## Project layout

```
coda-pics/
  index.html            entry point
  css/style.css         phone-first styling, MAZ ARCADE palette
  js/lexicon.js         the vocabulary          -> window.CodaLexicon
  js/prompt.js          words → a scene         -> window.CodaPrompt
  js/subjects.js        42 drawing routines     -> window.CodaSubjects
  js/paint.js           the scene painter       -> window.CodaPaint
  js/finish.js          14 style passes         -> window.CodaFinish
  js/app.js             the page wiring
  tests/                the headless test suite
```

## What it is not

CODA PICS is not DALL·E and does not pretend to be: there is no neural network
here, so it will not give you a photograph of something that never existed. It
gives you an **illustration** — bold, graphic, poster-like — of what you asked
for, instantly, for free, forever, with nothing sent anywhere. Those turn out
to be different and quite good things to have.
