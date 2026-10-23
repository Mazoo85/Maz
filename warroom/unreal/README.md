# War Room 1936 — the Unreal Engine game

This folder is the Unreal Engine 5 project (5.5 or later; the project adapts to your version). The game's rules live in `Source/WarRoomSim`,
plain C++ that is tested without Unreal; the globe, camera and interface in
`Source/WarRoom` are the Unreal side; the world data comes from `../data` and the 3D
pieces from `../models`, both made by the pipeline and Blender scripts in the folders next
to this one.

## Getting it running on your PC

You need Windows, a graphics card from the last few years, and about 100 GB of free
disk space. Any Unreal Engine 5 version works.

**1. Install Visual Studio 2022 (free).** Unreal needs it to build the game's code; without
it the project will not open. Get *Visual Studio Community 2022* from
visualstudio.microsoft.com. When the installer asks what you want, tick
**Game development with C++**. On the right, make sure **Unreal Engine installer** and a
**Windows 11 SDK** are ticked. Install, then restart the PC.

**2. Download the game.** Open this link in your browser and save the ZIP:
<https://github.com/Mazoo85/Maz/archive/refs/heads/claude/warroom-stage1.zip>
Right-click the ZIP, choose *Extract All*, and extract it to **`C:\Maz`** (a short
path matters: Windows and Unreal both choke on very long folder paths). You should end
up with `C:\Maz\Maz-claude-warroom-stage1\warroom\unreal\WarRoom.uproject`.

**3. Open the project.** Double-click `WarRoom.uproject`.
- If it asks *which engine version*, pick the one you installed. If it then offers to
  convert the project, choose **Convert in-place**.
- It will say the modules are missing or out of date and ask to rebuild them: click
  **Yes**. The first build takes 5 to 15 minutes with nothing much on screen. Then the
  editor opens.
- If it says the build failed, open
  `warroom\unreal\Saved\Logs\WarRoom.log` (or the window it shows) and send the lines
  with `error` in them back to Claude. A first build of code written without Unreal on
  hand may need a fix or two.

**4. Set up the game's content, once.** In the editor's top menu choose
**Tools → Execute Python Script…** and pick
`warroom\unreal\Scripts\setup_content.py`. It takes a minute or two. Open
**Window → Output Log** and look for `[warroom setup] done. Press Play.`

**5. Press Play** (the green triangle at the top). The world of 1936 appears as a globe.

| To | Do this |
|---|---|
| Turn the globe | Drag with the left or right mouse button |
| Zoom | Mouse wheel |
| Look at a place | Click it |
| Find a place | Type in the box at the top right |
| Pause / speed | Space bar, keys 1–5, or the buttons at the top |
| Change map mode | The buttons at the bottom left |
| Play a country | Click it, click its name in the panel, then *Play as* |
| Save or load | *Saves* at the top right |
| Stop | Esc closes a panel; Shift+F1 frees the mouse; the Stop button ends play |

## What is where

| | |
|---|---|
| `WarRoom.uproject` | The project. Plugins: ProceduralMeshComponent, Python editor scripting, Interchange (glTF import). |
| `Source/WarRoomSim/` | The simulation: clock, seeded random numbers, data loading, game state, commands, weather, saves. Standard C++20 only. |
| `Source/WarRoom/` | `WarRoomSubsystem` runs the simulation and the clock; `GlobeActor` builds the sphere, keeps the lookup textures, picks provinces and places the counters; `OrbitCameraPawn` is the camera; `WarRoomPlayerController` the mouse and keys; `SWarRoomHUD` the interface (Slate); `WarRoomGameMode` wires them. |
| `Shaders/Globe.ush` | The globe's shader: province ids → colours, borders, relief, day and night. Included by the `M_Globe` material. |
| `Scripts/setup_content.py` | Builds the assets that cannot be text (textures, materials, meshes, the level). |
| `Config/` | Engine, game and input settings. |
| `Tests/` | `make && ./sim_tests ../../data` builds and runs the simulation tests with any C++ compiler; CI runs them on every push. |

## Testing without Unreal

```sh
make -C warroom/unreal/Tests && warroom/unreal/Tests/sim_tests warroom/data
```

Ten test groups: random numbers, the clock, a new game, commands, weather, determinism,
saves, and the real 1936 data (loads, borders, a year of game time).

## Unreal automation tests

Inside the editor the same checks run as automation tests under *WarRoom.*
(Tools → Session Frontend → Automation), so they also run from the editor or from
`UnrealEditor-Cmd.exe WarRoom.uproject -ExecCmds="Automation RunTests WarRoom" -unattended -nopause -testexit="Automation Test Queue Empty"`.
