# Maz Editor — plain-language getting-started guide

This guide is for someone with **no coding experience**. It explains how to turn the Maz Engine
into a real program you can open — the **Editor**, a Godot-style tool where you place objects in a
3D scene, tweak them, and press **Play**.

---

## First, the honest part

Right now the engine is **source code** — the raw recipe. A program you can double-click is made
*from* that recipe by a step called **building** ("cooking" the recipe into a finished meal).

Two things are true, and they matter:

1. **Building has to happen on your own computer.** A program built somewhere else won't run on
   your machine, and it needs a **graphics card** to run — every normal laptop/desktop from the
   last ~10 years has one.
2. **This is a one-time setup, then it's easy.** You install a few free tools once, run **one
   script**, and you get the Editor program. After that, opening it is just a double-click.

You do **not** need to learn to code. You're following a checklist, not writing software.

---

## Step 1 — Install three free tools (one time)

| Tool | What it's for | Where to get it |
|------|---------------|-----------------|
| **A C++ compiler** | Turns the recipe into a program | **Windows:** install [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) (free) and check **"Desktop development with C++"** during setup.  **Mac:** open the Terminal app and run `xcode-select --install`.  **Linux (Ubuntu/Debian):** run `sudo apt install build-essential`. |
| **CMake** | Organizes the build | [cmake.org/download](https://cmake.org/download/) |
| **Vulkan SDK** | The graphics system the engine draws with | [vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home) |

> **On a Mac?** The Vulkan SDK automatically includes a piece called **MoltenVK** that lets Vulkan
> run on Apple hardware. Nothing extra to do — just install the SDK.

That's the whole shopping list. These are standard, widely-used, free developer tools.

---

## Step 2 — Get the project onto your computer

If you already have this project folder on your computer, skip this.

Otherwise, download it from GitHub: on the repository page, click the green **"Code"** button →
**"Download ZIP"**, then unzip it somewhere easy to find, like your Desktop.

---

## Step 3 — Run the one-command builder

Inside the project there's a `tools` folder with a script that does everything for you. It checks
your tools are installed, builds the Editor, and tells you where it ended up.

- **Windows:** open the `tools` folder and **double-click `build_editor.bat`**.
  (If Windows warns about running it, choose "More info" → "Run anyway" — it's your own file.)
- **Mac / Linux:** open the **Terminal**, then type `bash ` (with a space), drag the
  `build_editor.sh` file onto the window, and press **Enter**.

The first run takes a few minutes and downloads a couple of small helper libraries automatically —
that's normal. When it finishes it prints **"Done!"** and the exact location of your Editor program.

> If it stops early, it will tell you **which tool is missing and where to get it**. Install that,
> then run the script again.

---

## Step 4 — Open the Editor

- **Windows:** double-click `editor.exe` inside the `build` folder (usually
  `build\bin\Release\editor.exe`).
- **Mac / Linux:** in the Terminal, run `build/bin/editor`.

A window titled **"Maz Engine — Editor"** opens: a 3D viewport in the middle, a list of the scene's
objects on one side, and an inspector for the selected object on the other.

---

## Using the Editor — the controls

| You want to… | Do this |
|--------------|---------|
| **Select an object** | Click it in the 3D view, or click its row in the object list |
| **Move / Rotate / Scale it** | Press **1** (move), **2** (rotate), or **3** (scale), then drag the colored handles |
| **Nudge the selected object** | Arrow keys (and **Q** / **E** to turn it) |
| **Snap to a grid** | Press **Space** to toggle snapping on/off |
| **Undo / Redo** | **Ctrl+Z** / **Ctrl+Y** |
| **Duplicate** | **Ctrl+D** |
| **Delete** | **Delete** key |
| **Save your scene** | **Ctrl+S** (saved as a readable file in your user folder) |
| **Open a saved scene** | **Ctrl+O** |
| **Play (simulate physics)** | Click the **▶ PLAY** button — press **STOP** to return to editing |
| **Package the scene** | **Ctrl+B** (bundles it into a distributable pack file) |
| **Quit** | **Esc** |

Pressing **Play** runs the scene like a game — objects fall and collide — then **Stop** puts
everything back exactly as you left it, so you can experiment freely.

---

## If something goes wrong

- **The script says a tool is missing.** Install the one it names (Step 1), then run it again.
- **A window never appears** but the terminal shows text and exits. You're likely on a computer with
  no display/graphics card (like a cloud server). The Editor needs a real screen — try it on a
  normal laptop or desktop.
- **You just want to check it built correctly** without a screen: run
  `build/bin/editor --headless --frames 5`. If it prints "editor ready" and exits without an error,
  the program is good.

---

## What this proves

The Editor is the same kind of program Godot is: a visual tool for building scenes and pressing
play. Getting it running on your own computer is the moment the engine stops being "code in a
folder" and becomes **a program you use**. From here, everything else in the engine — the sample
games, the 3D demos — builds and runs the same way (each one is a program in that same `build/bin`
folder).
