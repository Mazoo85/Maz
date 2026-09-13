# Films in three dimensions

SCRIPT FORGE writes a film and cuts it. There are now **two renderers that can photograph the
result**, and they read the same document:

| | what it is | where it runs |
|---|---|---|
| **flat** (`apps/filmreel`, `film/js/film-player.js`) | three painted planes at three parallax rates, with silhouettes drawn between them | a command line, or the browser |
| **3D** (`apps/film3d`, `film/wasm/`) | rooms with floors, bodies with proportions, a camera with a focal length | a command line, or the browser |

Neither needs a GPU, a window or a display. Both read the same `.reel.json`, make the same cuts at
the same moments, and draw the same captions — because a film rendered two ways has to be one film.

**The 3D renderer runs in the browser too.** The same C++, compiled to WebAssembly: 147 KB, fetched
the first time somebody picks the 3D look in the film app and cached from then on, so it keeps
working with the wifi off. It draws a frame at 480 across in about 25 ms, against the 83 ms a film at
twelve frames a second has.

```sh
tools/build-wasm.sh                 # rebuild film/wasm/ — needs emcc on PATH
node film/tests/film-3d.test.js     # the browser must draw EXACTLY what the command line draws
```

The WebAssembly build's output is committed, because the film app is served as static files with no
build step between the repo and the page.

```sh
cmake --build build --target film3d film3d_demo

./build/bin/film3d my-film.reel.json --gif film.gif --width 640 --fps 12
./build/bin/film3d my-film.reel.json --contact sheet.qoi     # one still per shot
./build/bin/film3d_demo --gif demo.gif                       # the engine's own demo reel, in 3D
```

A reel comes out of SCRIPT FORGE: make a film in the browser and use **Download → reel (.json)**.

---

## The four pieces

### 1. `render/SoftRaster.hpp` — triangles into a picture, with no GPU in the room

The engine draws 3D through Vulkan, which wants a device, a display, a driver and a shader compiler.
That is right for a game, which is *played on a machine*, and wrong for a film, which is *rendered*: a
film should come out the same on a laptop, on a build server and in a headless container, frame by
frame into an image file, with no window ever opening.

So this is the other path — the same meshes, the same matrices, the same Vulkan clip conventions,
rasterised on the CPU into a `render::Image`. A depth buffer, near-plane clipping, backface culling,
perspective-correct interpolation, a key light with an ambient and a fill, and distance fog. The
projection matches `Camera3D::worldToScreen` exactly, so a point the camera says is at a pixel is the
pixel this fills.

It is deliberately small. No textures, no transparency sorting. Everything it does not
do, it does not do in a way you can see rather than in a way that corrupts the frame.

### 2. `film/Actor.hpp` — a person, built to the proportions of a person

Every measurement is a fraction of the character's **height**, from the figure-drawing canon:

```
     crown 1.000   an adult is 7.5 heads tall
      chin 0.867   the eyes sit halfway up the head, not near the top
  shoulder 0.815
     elbow 0.625   level with the navel
 hip joint 0.530   the femoral head — NOT the crotch, and NOT the halfway line
     wrist 0.485   level with the crotch; the fingertips reach mid-thigh
      knee 0.285   below halfway down the leg, not at it
     ankle 0.039
```

A woman is narrower across the shoulders and wider at the hip; a child is **six** heads tall rather
than seven and a half, because scaling an adult down produces a small adult, which is uncanny and
never a child.

The arms hang forward from the shoulders (angles at the shoulder and elbow, which is how an arm is
posed). The legs run backward from the feet — the foot is placed on the floor and the knee is solved
with `anim::solveTwoBoneIK` — because a walk with feet that slide is not a walk. A foot asked for
somewhere the leg cannot reach **falls short of it** rather than stretching the shin: a bone is a
fixed length, and that is what makes it a bone.

#### The head, and the face on it

A reaction shot is half of film grammar: the reason to cut to somebody is to watch them take
something in. So the head is worth more than its share of the work, and it gets it.

The head is **one sculpted surface**, lofted from a table of cross-sections — the brow, the eye
sockets, the cheekbones, the jaw and the chin are shaped into the same skin. It was a sphere with
those things stuck on it as separate lumps for a while, and it never worked: two nearly-parallel
surfaces meet along a curve that wanders by a whole facet at a time, so every lump showed its own rim
and a face came out as four pale eggs glued to a fifth. The same failure put a sawtooth on the
hairline, so the hair is lofted too, from a rim placed where a hairline actually is — high across the
front, falling away fast at the temples, low round the back.

The **bridge of the nose is part of that surface** as well, not a piece added to it; drawn separately
it is a pipe laid down the middle of a face, whichever shape the pipe is. Only the end of the nose,
the eyes, the eyebrows and the mouth are their own geometry, and each of them is laid onto the face
**at its own position on it** — a head is a ball, so at the eyes the surface is already a tenth of a
head further back than it is at the nose.

The heights are the canon again. Measuring from the crown of the hair down to the chin:

```
  hairline  a quarter of the way down
      brow  halfway — the eyebrows sit on it
      eyes  just under the brow
 nose base  halfway again, from the eyes to the chin
     mouth  a third of the way from the nose base to the chin
```

The first draft had the nose and the mouth each a third of a head too high, and nothing else about
the face mattered while that was true.

`film/Expression.hpp` decides what the face is **doing**, from the one thing the film already knows
about every shot: which beat of the story it is. A face built from the beat, the mood and who is
speaking is a face reacting to the scene it is in, in every shot of every film, for free — where
picking expressions at random gives a character who is astonished during small talk and blank at the
worst moment of their life. The mouth opens on the **syllable clock**, the same one the hands gesture
on, so the gesture, the mouth and the voice are one performance rather than three things happening at
once. Blinks run on a per-character clock so two people listening to the same line do not blink in
unison.

### 3. `film/Perform.hpp` — what the body is doing

* **Walking.** A real gait cycle, driven by *distance travelled* rather than by time, so it cannot
  desynchronise from the movement however the speed changes. The pelvis rises and falls twice per
  stride and swings side to side once; the hips turn with the swinging leg and the ribcage turns
  against them; the arms swing opposite the legs. The foot rolls heel-to-toe, and the ankle lifts by
  exactly how far the leg is short of where the gait wants the foot — which is both what really
  happens and what buys the leg its reach at full stride.
* **Standing.** Breathing, a slow shift of weight from one leg to the other, a drift of the head. Two
  people standing together do not do it in step.
* **Speaking.** One hand leads, on the syllable clock the film already counts. Two hands doing the
  same thing at once is semaphore.

#### What they are wearing, and what their hair does

Not decoration. At the distance a film watches people from, the **silhouette is who somebody is** —
you know which one is which across a car park, in the dark, long before you can read a face. Two
figures in the same painted T-shirt with the same helmet of hair are one figure drawn twice, whatever
colour they have been tinted, and that is what every character in this renderer was.

So a `Build` carries a coat hem, a skirt hem, a sleeve length, a hair length and a fringe — all of
them heights as a fraction of the body's own height, so they mean the same thing on a tall man, a
short woman and a child. A coat or a skirt is **lofted and flares**, because a hem is always wider
than the waist above it and a garment that does not flare is a tube of paint; it hangs off the pelvis
rather than off the legs, so the legs move inside it.

Hair is the cap plus, optionally, a **fall** — and the fall is its own closed tube rather than more
ribs on the cap. Bridged onto the cap, the surface has to get from a hairline that is high at the
forehead and low at the nape down to a level ring, and the only way across the front is a sheet
straight down the face. It looked exactly like somebody wearing their hair over their eyes.

`castFor` chooses from the **role** first, because the role is the one thing the film actually knows
about a character — a nurse is in scrubs with her hair up because that is what you have to do to work
a ward, and the stranger who turns up at midnight is in a coat — and off the name only where the role
says nothing.

#### Using the room

Standing on a mark and talking is a radio play with a picture over it. Three things put somebody *in*
a room, and they are three because everything an actor does on a set is one of them: they take the
weight off their feet, they put their weight on something, or they touch something.

* **Sitting** is not a pose so much as a place for two joints — the pelvis goes down onto the seat and
  the feet go out in front. Everything else follows, because the knees are already solved from the
  feet: the same leg IK that stops a walker skating bends a seated knee to ninety degrees with nothing
  added for it. It blends 0 to 1, so *sitting down* can be a beat rather than a cut to somebody
  already seated.
* **Leaning** is a weight shift, not a tilt. The shoulder goes to the wall and the hips swing out the
  other way, so the body makes a shallow S and the feet stay away from whatever is being leaned on.
  Tipping the whole figure instead gives somebody falling over slowly.
* **Reaching** solves the arm the way the legs have always been solved, to a point in the room — a
  table top, a door handle, the other person's hand. A hand asked for somewhere it cannot go falls
  short of it rather than stretching the forearm, which is the same rule and for the same reason.

`film/Business.hpp` decides which of them happens, from the reel. Two sources of truth, and they are
not equally good:

* **The object is recorded data.** The reel says who is holding the thing the story turns on, shot by
  shot, because the screenplay tracked it while it was being written — so it can be carried in a hand,
  picked up in the shot where it arrives, and put down in the shot where it leaves. Before this it was
  welded to its plinth for the whole film, which made it scenery rather than the thing everybody in
  the film is arguing about.
* **Sitting and leaning are read out of the prose**, because the prose is the only place the film ever
  says anybody did them. That is string matching and there is no dressing it up. What makes it work at
  all is that the prose is not arbitrary: it comes from the same generator every time, out of a small
  and known vocabulary. A word it does not know costs nothing — the character stands, as they did
  before. The two ways it goes wrong are both tested: finding a word inside another word (`sat` lives
  inside `satisfied`), and attributing to one character what the other one did.

#### The weather, in the room

`film/Air.hpp` draws weather in **screen space**: streaks painted across the finished frame,
deliberately not moving with the camera. That is the right answer for a painted film and the wrong one
here, because in three dimensions the air is *somewhere*. Rain that falls between the camera and a
face is the shot; rain painted on top of it is a screen saver.

So `film/AirVolume.hpp` puts the same weather in the room — particles with positions, in front of some
things and behind others, lit by the same key, getting smaller as they go back. It fills a box in
front of the **lens** rather than a fixed volume of the set, because weather is everywhere and a shot
only ever sees the part of it the lens is pointed at.

It borrows `Air.hpp`'s vocabulary rather than inventing its own, and that is the point: `weatherFor()`
is the one chooser, both renderers ask it, and **a shot that rains in the flat look rains in the 3D
one**. Anything else and the Look control stops being a look and becomes a different film.

Fog, haze and shimmer draw no particles at all. They are the air *itself* rather than things in it, and
the renderer already fades every surface toward the sky colour with distance — faking them again on
top would be two depth cues disagreeing about how far away the back wall is.

### 4. `film/Stage.hpp` — a room, and a camera standing in it

The fifteen places built as rooms, **to scale in metres**: a door 2.05m, a table 0.74m, a counter
0.92m, a corridor 2.4m across, a chapel that is tall because the height *is* the room. An actor is
1.78m and every one of those numbers is read against them.

The camera carries a **focal length** as well as a distance, because that is the half of framing that
is not distance — a wide is about a 35mm, a mid a 50, a close-up an 85, and the camera steps back to
make up the difference, exactly as it would on a set.

---

## Two things that had to be added before it was watchable

Both were found by rendering a contact sheet of a whole film and looking at it.

**Exposure.** A night interior, shaded honestly, is very nearly black — because a dark surface under
a dim light *is* very nearly black. The fix is the one photography has always used: keep the ambient
low and open the aperture, printing through a filmic curve (the engine's own ACES, in
`render/Tonemap.hpp`). Lifting the ambient instead turns night into an overcast afternoon.

**Aerial perspective.** Everything distant mixes toward the colour of the air. It does three jobs at
once: gives the frame depth, separates a figure from the ground behind them, and hides the edge of
the world, so a ground plane no longer has to stop somewhere visible.

---

## Light, and what it falls on

Three rounds went into this and the first two were spent in the wrong place, which is worth writing
down because the mistake is an easy one to make twice.

The frames were flat: a wall was one value across its whole area, and the whole film was one hue. The
first fix was a **hemisphere ambient** — light comes down off the sky or the ceiling and comes back up
off the floor, weaker and carrying the floor's own colour, so a surface facing up and a surface facing
down are lit differently. The second was **one practical**: a lamp that is *in* the room, at a place,
with a distance falloff, as against the key, which comes from infinitely far away and therefore lights
a whole wall to exactly one value however big the wall is. Both were right and both helped, and the
pictures were still green.

The third fix was the actual problem. **The set was painted out of the palette**: the floor was
`pal.deep` at 0.95, the walls `pal.deep` at 1.55, the props `pal.ink` — one colour at four
brightnesses. A frame like that has no colour information in it at all, so nothing is warm relative to
anything else and it reads as *tinted* rather than as *lit*. And no amount of work on the lighting
fixes it, because **light multiplies the surface colour**: a green wall lit pink is a green wall.

So a set is built out of **materials** now — plaster, lino, wood, steel, cloth — each a real colour
leaned about a third of the way toward the film's palette. A horror room is still green enough to know
it is a horror room, and the wooden chair in it is browner than the plaster behind it.

That change had a consequence worth stating: the palette is dark at night, so while the set was
painted out of it the darkness of the night was *in the paint*. Plaster reflects three quarters of
what falls on it at midnight as well as at noon, so the night had to move into the light, which is
where it belonged — the key now carries the hour, and the print was pulled down to match. For one
round in between, a night horror film looked like a hospital at lunchtime.

## Shadows

A depth map rendered from the light: everything the light can see is lit, everything hidden behind
something it can see is in shadow. Without it there is nothing to say where a body meets the floor,
and the eye reads "hovering" long before it reads "unlit".

Three decisions, all arrived at by measuring against a **raycast ground truth** — fire rays at a
standing body from where the light is, and whatever each one hits first is, by definition, a point
the light reaches, so it had better come back lit:

| | wrongly shadowed |
|---|---|
| back-face casting + slope-scaled bias | **1.5%** — what ships |
| the same bias, not slope-scaled | 2.0% |
| the textbook normal offset instead | 23.9% |
| no bias at all | 12.1% |
| the body built inside out | ~100% |

**Cast from back faces.** If the surfaces facing the light write the map, every lit surface's own
depth is the depth in the map and half of it shadows itself — shadow acne. Casting from the far side
of each object puts the object's whole thickness between the two. The price is that casting needs
closed geometry: a single-sided plane has no far side and casts nothing.

**Slope-scaled bias**, half a texel divided by how squarely the surface faces the light. Back-face
casting settles flat geometry completely; a body is not flat, and near the silhouette of an arm the
recorded far side and the lit near side are within a texel of each other.

**The room does not cast.** A key light is a conceit, not a lamp hanging above a sealed box, and the
first frame rendered with shadows on was a correctly pitch-dark room with a lid. The shell is lit and
receives; the furniture, the trees and the people cast.

### The bug this found

Every lofted piece of the body — both arms, both legs, the whole torso — had been built **inside
out** since the day it was written. The rings were wound the wrong way, so the renderer culled the
outside of each limb and drew the inside, with the normals pointing into the body. On a smooth tube
that is very nearly invisible, which is how it survived being looked at for weeks. It makes shadows
impossible.

`tests/film/actor.cpp` now checks the signed volume of the body — positive when wound outward, and it
collapses from 0.040 m³ to 0.002 m³ when wound inward.

---

## One renderer, compiled twice

`film/tests/film-3d.test.js` demands that the browser draw **exactly** the film the command line
draws — every pixel of nine frames, no tolerance at all. That standard was not where the test
started, and how it got there is worth keeping.

The first version allowed a step or two of difference, on the reasoning that two compilers round
arithmetic differently. It failed anyway: a tenth of the pixels in a street scene disagreed, by up to
42 steps out of 255. Blurring did not absorb it, so it was not sub-pixel. Turning the shadows off did
not change it, so it was not the lighting. Building the native renderer with the *same* compiler the
WebAssembly build uses reproduced it exactly, so it was not WebAssembly. Cropping the worst pixel and
looking at both showed the same buildings **standing in different places**.

The cause was two dice rolls inside one function call:

```cpp
stand(dice.range(3.0f, 7.0f), dice.range(4.0f, 9.0f), dice.range(3.0f, 6.0f), x, z, ink)
```

C++ does not specify which argument is evaluated first, and GCC and Clang genuinely choose
differently — so the random numbers were dealt out in a different order and the skyline came out
different, for the same film with the same seed, depending only on which compiler built the renderer.
One roll per statement, and the two builds agree on all 881,280 pixels of the fixture without a single
step between them.

The tolerance stays at zero, because anything else would have hidden it.

## A lens

Everything a rasteriser draws is in focus, because a rasteriser is a **pinhole camera**: every point in
the world lands on exactly one pixel however far away it is. Real glass focuses at one distance and
turns everything else into a small disc, and the size of that disc is most of what separates a
photograph from a diagram. It is also the cheapest way to say *look here*: a close-up with the room
sharp behind it is a snapshot, and the same close-up with the room fallen away is a close-up.

`render/DepthOfField.hpp` does it the practical way rather than the correct way. The correct way is to
gather a disc of samples per pixel with a radius that varies per pixel, which costs a hundred taps at
every pixel of every frame. Instead two blurred copies of the frame are made at fixed radii — each one
two separable box passes — and the result is mixed between sharp, soft and softer by how far out of
focus that pixel is.

Three things about it are not obvious and all three are load-bearing:

* **The depth buffer is not a distance.** It stores z/w after the projection, and more than half of its
  range goes on the first quarter of a metre. Blurring by that number directly gives a lens that
  treats everything past ten metres as the same distance.
* **The circle of confusion is the difference of the reciprocals**, not a distance. That is the
  asymmetry that makes a face a metre in front of a wall separate from it while two trees fifty metres
  away stay stuck together.
* **A wide shot gets no lens at all.** A wide is about the room; throwing the room out of focus in it
  is throwing away the shot. It also happens to pay for the close-ups — at play quality the lens costs
  about six milliseconds, and most shots are wides.

It focuses on whatever the camera is aimed at, which is not so much a choice as the definition:
`lensFor` already decided what the shot is *of*, so the focus distance falls out of it and cannot
disagree with the framing.

## Where the time goes

Every pixel is worked out on the processor, so the frame budget is what everything else has to be paid
for out of. It is worth writing down what was actually measured rather than what seemed likely, because
two rounds of optimising the wrong thing came before this:

```
a 480 x 204 frame, supersample 1, hard shadows — 24 ms against a budget of 83

  rasterising            11.0 ms   of which shadows are 3.8
  captions and grain      4.9 ms
  tonemap                 2.0 ms
  building the bodies     2.0 ms
  copying down            1.8 ms
  the shadow map          1.3 ms
  staging the scene       0.15 ms
```

The two guesses that were wrong: it is not the geometry (a level of detail that cut the bodies from
10,500 triangles to a third of that saved 2 ms), and it is not the edge setup (stepping the edge
functions instead of recomputing them per pixel saved 1.4 ms). Only about a hundred thousand pixels
are shaded in a whole frame — there is not much overdraw to remove.

There **is** a level of detail, and it is worth having anyway: a head is three thousand triangles at
full detail and fifteen pixels tall in a wide shot. It is chosen from the shot's framing rather than
from the camera's position at this instant, deliberately — a push that halves the distance over five
seconds would otherwise walk the figure up through the detail levels while it played, and every step
of that is a visible change of shape.

## What the 3D renderer does not do

* **No sound.** The score is SONG FORGE's Web Audio program and the voices are the browser's speech
  synthesis; both live in the browser, by design. The native renderers make silent picture.
* **No textures.** Colour is per-vertex. A wall is a colour, not a wallpaper.
* **No transparency sorting.** Glass, smoke and rain are the flat renderer's for now.

---

## Testing

Everything here is pure logic against `render::Image`, so all of it runs headless:

```sh
ctest --test-dir build -R "software_rasteriser|film_actor|film_perform|film_stage" --output-on-failure
```

* `tests/render/softraster.cpp` — the six ways a triangle rasteriser goes wrong, each run as a
  deliberate break first to prove the check catches it.
* `tests/film/actor.cpp` — the canon, plus: a joint moves only what hangs off it, and the leg solve
  never stretches a bone.
* `tests/film/perform.cpp` — chiefly one thing: **while a foot is on the ground, its position on the
  floor does not change.** Everybody gets that wrong, and it is measurable.
* `tests/film/air.cpp` — the weather is in the room and stays there: it falls (and embers rise), it
  never runs out four minutes into a film, none of it is behind the camera or pressed against the
  lens, and the same moment of the same film has the same air in it to the last decimal — which it
  has to, because the browser and the command line are compared pixel for pixel.
* `tests/film/business.cpp` — what a character is doing with the room, and chiefly the two ways
  reading prose goes wrong: a word found inside another word, and one character's stage direction
  applied to the whole cast. (It did apply to the whole cast: one line saying somebody sat down put
  everybody in the film on the floor.)
* `tests/film/face.cpp` — the head is built like a head, and what it is doing comes from the story.
  Three of its checks each exist because of a bug that cost an afternoon of rendering pictures and
  staring at them:
  * **The head was lofted inside out.** Every triangle faced inward, so the renderer culled the front
    of the face and drew the inside of the back of the skull instead — which is smooth, has no chin,
    and still looks enough like a head to survive being stared at, enlarged, for hours. The signed
    volume of a closed surface comes out negative when that happens, and positive when it does not:
    one line of arithmetic that cannot be fooled by a head that happens to look right.
  * **Features were laid onto the depth of the face measured down the middle**, so the eyes stood out
    like golf balls and the cheekbones like two flying saucers. Nothing on the face may stand more
    than an eighth of a head-depth off it.
  * **The neck was aimed at the middle of the head.** `limb()` finishes with a rounded cap standing a
    whole radius past the point it is given — five centimetres on a neck — so it arrived as a dome
    behind the mouth and the figure grew a muzzle.
* `tests/film/stage.cpp` — a framing is a promise, so the subject is put through the lens and the
  test looks at where they land on the film; and the shadows on a real body are checked against a
  raycast, which is a different mechanism arriving at the same answer.
