# Your First Game in 10 Minutes

This tutorial takes you from an empty folder to a **running, playable game** — a tiny
"catch the falling block" game — using the Maz engine. No prior game-engine experience is
assumed. Every line of code here uses real Maz APIs (the same ones the bundled `pong` and
`catcher` samples use), so what you build actually compiles and runs.

By the end you will have: a window, a game loop, keyboard input, moving shapes, collision,
a score, and on-screen text.

---

## 0. Before you start (2 min)

You need the Maz repository built once. From the repo root:

```
cmake -S . -B build
cmake --build build -j
```

That produces the engine library plus the sample games under `build/bin/`. If those samples
run, your machine is ready. (A GPU + up-to-date drivers are needed to see the window; the
engine also has a `--headless` mode for automated testing.)

---

## 1. Make a project folder (1 min)

Create `apps/firstgame/` with two files: `main.cpp` (your game) and `CMakeLists.txt` (how to
build it). Then add one line to the repo's root `CMakeLists.txt` next to the other apps:

```
add_subdirectory(apps/firstgame)
```

Your `apps/firstgame/CMakeLists.txt` is two lines — it mirrors every other sample app. (The
font and shaders are staged into `build/bin/` automatically by the engine target, so you
don't list assets here.)

```
add_executable(firstgame main.cpp)
target_link_libraries(firstgame PRIVATE maz::maz maz_warnings)
```

---

## 2. Open a window and start the loop (2 min)

Every Maz game is the same skeleton: create a **window**, create the **renderer**, read
**input**, and run a **fixed-timestep loop**. Put this in `main.cpp`:

```cpp
#include "maz/Engine.hpp"
#include <SDL3/SDL_scancode.h>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);   // handles --headless, --frames N, size

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "My First Game";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) return 1;

    render::RendererConfig rc;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) return 1;

    platform::Input input;
    core::Clock clock(1.0 / 60.0);                        // 60 updates per second

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) window.requestClose();

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // --- game logic goes here (step 3) ---
        }

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;                     // 1 unit = 1 pixel, origin top-left
            renderer->setCamera2D(cam);
            // --- drawing goes here (step 4) ---
            renderer->endFrame();
        }
    }

    renderer->shutdown();
    window.shutdown();
    return 0;
}
```

Build and run it now — you should see a dark blue window you can close with `Esc`:

```
cmake --build build --target firstgame -j
./build/bin/firstgame
```

---

## 3. Add the game: a basket and a falling block (2 min)

Declare a few variables *before* the loop — the player's basket at the bottom, a block that
falls from the top, and a score:

```cpp
const float W = static_cast<float>(cfg.width);
const float H = static_cast<float>(cfg.height);
float basketX = W * 0.5f;                 // basket center x
const float basketY = H - 60.0f, basketW = 120.0f, basketH = 20.0f;
float blockX = W * 0.5f, blockY = 0.0f;   // falling block position
const float blockSize = 28.0f;
int score = 0;
core::Random rng(1234);                   // deterministic random for the next drop x
```

Now fill the fixed-step block from step 2 with the logic — move the basket with the arrow
keys, drop the block, and check for a catch:

```cpp
if (input.keyDown(SDL_SCANCODE_LEFT))  basketX -= 8.0f;
if (input.keyDown(SDL_SCANCODE_RIGHT)) basketX += 8.0f;
if (basketX < basketW * 0.5f)     basketX = basketW * 0.5f;
if (basketX > W - basketW * 0.5f) basketX = W - basketW * 0.5f;

blockY += 4.0f;                            // gravity

// Caught? (simple box overlap at basket height)
const bool atBasket = blockY + blockSize * 0.5f >= basketY;
const bool overlapX = std::abs(blockX - basketX) < (basketW + blockSize) * 0.5f;
if (atBasket && overlapX) {
    score += 1;
    blockY = 0.0f;
    blockX = rng.range(blockSize, W - blockSize);   // next drop
} else if (blockY > H) {
    blockY = 0.0f;                                   // missed — just respawn (add "lives" later!)
    blockX = rng.range(blockSize, W - blockSize);
}
```

`core::Random` is the engine's deterministic RNG, so the game plays the same every run —
handy for testing.

---

## 4. Draw it (1 min)

Maz draws 2D shapes as filled polygons. A tiny helper keeps the drawing readable — put it
above `main`:

```cpp
static void box(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}
```

Then inside the `beginFrame()` block, after `setCamera2D`, draw the basket and block:

```cpp
box(*renderer, basketX - basketW * 0.5f, basketY, basketW, basketH,
    render::Color{0.4f, 0.8f, 1.0f, 1.0f});
box(*renderer, blockX - blockSize * 0.5f, blockY - blockSize * 0.5f, blockSize, blockSize,
    render::Color{1.0f, 0.8f, 0.3f, 1.0f});
```

---

## 5. Show the score with text (1 min)

Load a font once, before the loop:

```cpp
#include <SDL3/SDL_filesystem.h>
#include <string>
// ...
ui::Font font;
{
    const char* base = SDL_GetBasePath();
    const std::string path = (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
    font.load(*renderer, path.c_str(), 48.0f);
}
```

Then draw it each frame (inside the `beginFrame()` block):

```cpp
char buf[32];
std::snprintf(buf, sizeof(buf), "Score: %d", score);
font.drawText(*renderer, 16.0f, 16.0f, buf, render::Color{1, 1, 1, 1}, 0.6f);
```

Rebuild and run — you now have a complete little game: move the basket with the arrow keys,
catch the falling blocks, watch the score climb, quit with `Esc`.

```
cmake --build build --target firstgame -j
./build/bin/firstgame
```

---

## Where to go next

You just used five of the engine's core systems: **window**, **renderer** (2D polygons +
camera), **input**, the **fixed-timestep clock**, **deterministic RNG**, and **font text**.
From here:

- **Sound** — `maz::audio` plays WAV/procedural SFX; see the `orbs` sample.
- **Sprites & images** — load a PNG and draw `render::SpriteDesc` instead of plain boxes.
- **Physics** — `game::PhysicsWorld2D` gives real bouncing/stacking bodies.
- **Bigger worlds** — `game::Tilemap`, `game::NavGrid` pathfinding, the `maz::ecs` entity system.
- **Read the code** — every folder under `apps/` is a small, self-contained example of one
  feature. `pong`, `catcher`, and `platformer` are complete games worth reading end-to-end.
- **API reference** — see [API.md](API.md); the architecture overview is in
  [ARCHITECTURE.md](ARCHITECTURE.md).

Happy building.
