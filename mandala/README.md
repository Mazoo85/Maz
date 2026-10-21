# Mandala Maker

**Draw a mandala.** Drag a line anywhere on the circle and it is repeated all
the way round the centre, so a single scribble becomes a symmetrical pattern.
Or press **Surprise me** for a whole mandala, then draw on top of it.

Open `index.html` in any browser. No build step, no server, no dependencies,
and nothing leaves your browser. Works with a finger on a phone or a mouse on
a computer, in light or dark mode.

A standalone app: it is not part of MAZ ARCADE.

## What it does

| | |
|---|---|
| **Draw** | Drag to draw. Every line repeats round the centre. |
| **Repeats** | 2 to 32 copies round the circle. |
| **Mirror** | Each copy is also flipped, like a kaleidoscope. |
| **Colour** | Six colour sets (Earth, Jewel, Lotus, Ocean, Ink, Gold), rainbow ink, or any colour you pick. Each set brings its own background. |
| **Glow** | A soft glow on the lines. |
| **Guide lines** | Faint spokes and rings to draw along. Never in a saved picture or video. |
| **Motion** | Still; Spin (the whole mandala turns); Swirl (rings turn opposite ways, outer ones faster); Breathe (a slow, calm breath: about 3 seconds drawing in and 3 opening out, starting at the heart and travelling out); Flow (swirl and breathe together); Vortex (the default: the mandala spirals into itself forever: copies of it nest inside each other, each smaller and turned a little further, and all of them sink towards the centre while a new one opens from the edge). Keep drawing while it moves: a line lands under your finger and then travels with its ring. Devices set to reduce motion start on Still. |
| **Breath guide** | Shows *Breathe in* while the mandala opens and *Breathe out* while it draws in, so you can breathe along with it. |
| **Speed** | 0× to 3×. Changing it never makes the picture jump, and 0× pauses it where it is. |
| **Surprise me** | A full, layered mandala laid out the way a hand-drawn one is: a rosette at the heart; bold feature rings (petals, lotus petals, leaves, teardrops, diamond lattices), each with a smaller echo inside and often dots at the tips; fine detail bands of beads, scallops, zigzags and rings between them, some at twice the repeats; and a scalloped border at the edge. Colours step through the colour set ring by ring. Uses your repeats, colour set and mirror setting. |
| **Undo / Clear** | Take back the last line (or Ctrl+Z), or start again. |
| **Save picture** | Downloads a 2048 × 2048 PNG of the mandala at rest. |
| **Save video** | Records exactly one breath (or one turn of the vortex) of the moving mandala (3–12 seconds, depending on speed), so the clip loops. MP4 where the browser can make it, otherwise WebM. Guide lines are left out. |

Your drawing is kept in the browser (localStorage), so it is still there when
you come back.

## Files

| File | What it is |
|---|---|
| `js/mandala.js` | The engine: symmetry, ring motion, the Surprise-me generator, the save format. No DOM, so it is tested in plain Node. |
| `js/app.js` | The page: canvas, touch and mouse drawing, buttons, saving. |
| `css/style.css` | The look: warm paper in light mode, ink-dark in dark mode. |
| `tests/mandala-logic.test.js` | `node mandala/tests/mandala-logic.test.js`. Runs in CI via `.github/workflows/mandala.yml`. |
