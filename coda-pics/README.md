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
| **75 subjects** | dragons, whales, wolves, foxes, stags, owls, astronauts, robots, wizards, knights, ghosts, castles, lighthouses, temples, torii gates, windmills, tall ships, rockets, hot air balloons, trains, portals, crystals, great trees, waterfalls, ringed planets… |
| **20 settings** | mountains, forest, jungle, open sea, shore, lake, desert, city, deep space, snow, canyon, swamp, meadow, plains, volcano, ruins, caves, tropical island, an empty road, above the clouds |
| **Every hour** | dawn, daylight, sunset, night — each with its own sky, its own light, and its own colour in the shadows |
| **Weather** | clear, cloudy, rain, falling snow, fog, storms with lightning, the northern lights |
| **14 art styles** | Neon · Pixel art · 16-bit arcade · Watercolour · Film noir · Comic book · Blueprint · Storybook · Woodblock · Poster · Low poly · Stained glass · Oil painting · Soft realism |

Colour words (*a **red** dragon*), size words (*a **giant** wolf*), counts
(*three wolves*, *wolves*) and mood words (*lonely*, *epic*, *peaceful*) all
change what comes out. A colour lands on the **subject**, not the whole frame:
a red dragon is a red dragon, not a red world.

Two subjects in one sentence put both in the picture, and **where** you say
they are is where they go — *"a cat **under** a tree"*, *"a castle **behind**
the mountains"*, *"a balloon **above** a castle"*, *"a wolf **beside** a
campfire"*.

Two style words mean both: *"watercolour pixel art"* is a wash, then blocked
out, and looks like neither on its own. And a typo is forgiven — *"a dragn"*
still gets you a dragon, and the readout says it read it that way.

**It shows its working, including its ignorance.** Under every picture is a row
of tags saying what each word was understood as — and a line naming any word it
did not know, so *"a griffin in a forest"* tells you why there is no griffin.

## Using a photo you already have

Four things it can do with one of your own photographs, all of them offline.
**The photo is read by your browser, measured in the page, and never uploaded** —
the same terms as everything else here.

| | |
|---|---|
| **Paint in its colours** | The sky, land, haze, shadow and highlight are taken from the photo, so a dragon gets painted in the colours of an evening you actually stood in. |
| **Use its horizon** | The skyline is read out of the photo — a ridge of hills, a city, the edge of the sea — and becomes the terrain the painter draws. |
| **Paint onto it** | The photo is the picture, and the subject is drawn into it, lit from wherever the photo's own light is coming from. |
| **Style the photo itself** | The photo through any of the 14 styles, with nothing drawn on top: your photograph as a woodblock print, a blueprint, stained glass, pixel art. |

It says what it found — *"Horizon found 64% down, and the light is coming from
the right"* — and when there is no horizon to find (a close-up, a flat wall) it
says that too and switches that option off, rather than inventing a ridge out
of nothing.

None of this is a model. Nothing is learned or recognised; pixels are measured.
It cannot put your face in a picture or make a photograph of something that
never happened, and it does not pretend to.

One honest limit: a picture painted **onto** a photo cannot be kept in the
gallery, because the gallery stores scenes and not photographs. The app says so
and points you at **Save the picture** instead of bringing it back wrong later.

## Buttons

- **Paint it** — paint the words in the box (or just press Enter).
- **Another take** — same words, different roll of the dice.
- **Show me six** — six takes at once; tap one to paint it full size.
- **Surprise me** — writes you a prompt in its own vocabulary.
- **Save the picture** — downloads a full-size PNG.
- **Keep in my gallery** — saves it to this device, and it can be saved to a
  file and loaded on another one.
- **Copy a link to it** — a link that paints this exact picture for whoever
  opens it. The link *is* the picture: a prompt, a seed, a style and a shape,
  four short values, so sharing one uploads nothing.

**Keep when you take another** holds part of a picture still while the rest
re-rolls — keep the subject and change the weather, or keep the land and change
everything standing on it.

Seven shapes, from a square to a 2560 × 1440 desktop wallpaper.

**Installable.** Add it to your home screen and it opens without browser
chrome and keeps working with no network — there was never anything to fetch.

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
   under each picture; that is the whole promise the gallery and every shared
   link rely on.

   The gallery stores the *finished scene*, not the words that made it. The
   scene a prompt resolves to depends on the vocabulary the app had at the
   time, so a picture kept today and repainted after a new subject is added
   would otherwise come back as a different picture. Keeping the resolved
   scene is what makes "kept" mean kept.

The finishing passes (blur, bloom, dithering, edge detection, halftone,
mosaics) are written out by hand on the pixel array rather than handed to
canvas filters, so they behave identically in every browser — and so the tests
can run the real pipeline with no browser at all.

## Checking it

```
node coda-pics/tests/coda-logic.test.js     # the engine, no browser needed
node coda-pics/tests/coda-browser.test.js   # the real page in Chromium
node coda-pics/tests/visual.test.js         # the pictures have not changed
```

No browser, no dependencies, no network. It checks that every word in the
lexicon leads to a routine that exists, that any text at all produces a
complete drawable scene, that the same prompt paints identically twice, and
then actually paints **every subject, every setting, every style and every
hour/weather pair** against a recording canvas — so a typo in a rarely-chosen
branch fails here rather than in front of somebody typing *"a crab"*.

The third one is the one that guards the promise above. It reduces twelve fixed
prompts to a 64-bit average hash and fails if any of them moved — because a
quiet change in the painter silently repaints pictures people have already kept
and already shared. When a change is deliberate, re-record it and the diff
shows exactly which pictures moved:

```
node coda-pics/tests/visual.test.js --update
```

All three run on every push as part of
[Site CI](../.github/workflows/site-ci.yml), which also drives the live page in
a real Chromium.

## Project layout

```
coda-pics/
  index.html            entry point
  manifest.webmanifest  what makes it installable
  sw.js                 the offline worker
  icons/                home-screen icons
  css/style.css         phone-first styling, MAZ ARCADE palette
  js/lexicon.js         the vocabulary          -> window.CodaLexicon
  js/photo.js           reading a photograph    -> window.CodaPhoto
  js/prompt.js          words → a scene         -> window.CodaPrompt
  js/subjects.js        42 drawing routines     -> window.CodaSubjects
  js/paint.js           the scene painter       -> window.CodaPaint
  js/finish.js          14 style passes         -> window.CodaFinish
  js/render-worker.js   the same engine, off the main thread
  js/app.js             the page wiring
  tests/                the test suites
```

Big pictures are painted in a worker so the page never freezes, and a quarter
size preview appears first so there is something to look at while it works.
Both paths run the identical engine; if a browser has no worker, the page
paints it itself and the result is the same picture.

## What it is not

CODA PICS is not DALL·E and does not pretend to be: there is no neural network
here, so it will not give you a photograph of something that never existed. It
gives you an **illustration** — bold, graphic, poster-like — of what you asked
for, instantly, for free, forever, with nothing sent anywhere. Those turn out
to be different and quite good things to have.
