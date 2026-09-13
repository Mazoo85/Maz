# Films in three dimensions

SCRIPT FORGE writes a film and cuts it. There are now **two renderers that can photograph the
result**, and they read the same document:

| | what it is | where it runs |
|---|---|---|
| **flat** (`apps/filmreel`) | three painted planes at three parallax rates, with silhouettes drawn between them | anywhere |
| **3D** (`apps/film3d`) | rooms with floors, bodies with proportions, a camera with a focal length | anywhere |

Neither needs a GPU, a window or a display. Both read the same `.reel.json`, make the same cuts at
the same moments, and draw the same captions — because a film rendered two ways has to be one film.

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

It is deliberately small. No textures, no shadows, no transparency sorting. Everything it does not
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
* `tests/film/stage.cpp` — a framing is a promise, so the subject is put through the lens and the
  test looks at where they land on the film; and the shadows on a real body are checked against a
  raycast, which is a different mechanism arriving at the same answer.
