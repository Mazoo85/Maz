# War Room 1936 — the Unreal Engine game

This folder is the Unreal Engine 5.5 project. The game's rules live in `Source/WarRoomSim`,
plain C++ that is tested without Unreal; the globe, camera and interface in
`Source/WarRoom` are the Unreal side; the world data comes from `../data` and the 3D
pieces from `../models`, both made by the pipeline and Blender scripts in the folders next
to this one.

## Getting it running on your PC (first time, about an hour of installs)

You need a Windows PC with a graphics card made in the last few years and about 80 GB
of free disk space.

1. **Install the Epic Games Launcher** from epicgames.com, sign in (a free account), open
   the *Unreal Engine* tab, *Library*, and install **Unreal Engine 5.5**.
2. **Install Visual Studio 2022 Community** from visualstudio.microsoft.com. In the
   installer tick **Game development with C++** and, inside it, make sure
   *Unreal Engine installer* and *Windows 10/11 SDK* are ticked.
3. **Get this repository** onto the PC: in the Epic/GitHub world the easiest is
   *GitHub Desktop* → *Clone repository* → `Mazoo85/Maz`. (The whole repository; the game
   reads `warroom/data` and `warroom/models`, which sit next to this folder.)
4. **Open the project.** In the `warroom/unreal` folder, right-click `WarRoom.uproject`
   and choose **Generate Visual Studio project files**, then double-click
   `WarRoom.uproject`. Unreal will say the modules need to be built: answer **Yes** and
   wait (the first build takes several minutes). The editor opens.
5. **Set up the content, once.** In the editor: **Tools → Execute Python Script…** and
   pick `warroom/unreal/Scripts/setup_content.py`. It imports the province map and
   relief, the Blender pieces, builds the two materials and creates the level. Watch the
   *Output Log* for `[warroom setup] done`.
6. **Press Play** (the green arrow). The globe of 1936 appears. Drag with the left mouse
   button to turn it, scroll to zoom, click a province. Space pauses, 1–5 set the speed,
   the box at the top finds places, *Saves* saves and loads.

If step 4 reports a compile error, copy the red lines from the *Output* window and send
them back; this project was written without an Unreal install to compile against, so a
first build may need a fix or two.

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
