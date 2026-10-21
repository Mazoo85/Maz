# ✺ MANDALA MAKER

**Draw a mandala.** Drag a line anywhere on the circle and it is repeated all
the way round the centre — so a single scribble turns into a symmetrical
pattern. Or press **✨ Surprise me** and get a whole mandala in one go, then
draw on top of it.

### ▶ [Open it](https://mazoo85.github.io/Maz/mandala/)

Open `index.html`. No build step, no server, no dependencies, nothing leaves
your browser. Works with a finger on a phone or a mouse on a computer.

---

## What it does

| | |
|---|---|
| ✍️ **Draw** | Drag to draw. Every line repeats round the centre. |
| 🔁 **Repeats** | 2 to 32 copies round the circle. |
| 🪞 **Mirror** | Each copy is also flipped, like a kaleidoscope. |
| 🎨 **Colour** | Six colour sets (Neon, Lotus, Ocean, Earth, Ink, Gold), rainbow ink, or any colour you pick. Each set brings its own background. |
| 💡 **Glow** | Soft neon glow on the lines. |
| 📐 **Guide lines** | Faint spokes and rings to draw along. They are never in the saved picture. |
| 🌀 **Spin** | The mandala slowly turns — you can keep drawing while it does. |
| ✨ **Surprise me** | A random mandala: petals, arches, zigzags, dots and loops in rings from the centre out, using your repeats, colour set and mirror setting. |
| ↩ **Undo** / 🗑 **Clear** | Take back the last line (or Ctrl+Z), or start again. |
| ⬇ **Save picture** | Downloads a 2048 × 2048 PNG. |

Your drawing is kept in the browser (localStorage), so it is still there when
you come back.

## Files

| File | What it is |
|---|---|
| `js/mandala.js` | The engine — symmetry, the Surprise-me generator, the save format. No DOM, so it is tested in plain Node. |
| `js/app.js` | The page — canvas, touch/mouse drawing, buttons, saving. |
| `tests/mandala-logic.test.js` | `node mandala/tests/mandala-logic.test.js` — runs in CI. |
