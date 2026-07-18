# Maz 2D Sprites

The **sprite renderer** is the engine's 2D drawing path — textured, alpha-blended quads in screen
space. It's roadmap **Phase 3 (2D)** and the first big step toward the engine's forcing-function
target, the native *ZOMBOID: ANCHORAGE* port. This is also where Maz grows its first **descriptor
sets** and its first **staging upload** path.

## What it does

- Draws **textured quads** (`Renderer::drawSprite`) from **RGBA8 textures** you upload once
  (`Renderer::uploadTexture`).
- **Pixel-space coordinates, top-left origin.** A sprite at `(0,0)` sits in the top-left corner; the
  projection is `maz::math::ortho2D(width, height)`, matched to the drawable size each frame.
- **Per-sprite tint, rotation, and UV sub-rect.** The tint multiplies the texel (so one white
  texture can be recolored into many); the UV rect `(u0,v0)-(u1,v1)` selects an **atlas cell** out of
  a larger texture; `rotation` spins the quad about its own centre.
- **Alpha blending, painter order.** Depth test/write are off — sprites composite in the order you
  submit them, over any 3D meshes drawn the same frame. Natural for HUDs, tilemaps, and 2D scenes.
- **Batched.** Consecutive sprites that share a texture collapse into a single `vkCmdDrawIndexed`, so
  a tilemap that reuses one atlas is one draw call, not one per tile.
- **Nearest-filtered** sampling — crisp pixel art, the SEGA-90s / neon look the flagship game wants.

## Using it

```cpp
// Upload a texture once (tightly packed RGBA8, top row first). Returns a handle, or -1 headless.
std::vector<uint8_t> pixels = /* w*h*4 bytes */;
const int tex = renderer->uploadTexture(pixels.data(), w, h);

// Each frame, between beginFrame() and endFrame():
render::Sprite s;
s.x = 100.0f; s.y = 60.0f;   // top-left, pixels
s.w = 64.0f;  s.h = 64.0f;   // size, pixels
s.tint = {1.0f, 0.1f, 0.6f, 1.0f};   // neon pink
s.rotation = 0.0f;           // radians
// s.u0/v0/u1/v1 default to the whole texture; set a sub-rect for an atlas cell.
renderer->drawSprite(tex, s);
```

Sprites are queued by `drawSprite()` and recorded as one batched pass in `endFrame()` — after the 3D
meshes, before the ImGui overlay.

## Try it

A live demo bounces a handful of neon-tinted, spinning tiles over the animated 3D clear:

```
cmake -S . -B build -G Ninja && cmake --build build
./build/bin/sandbox --sprite-demo          # needs a GPU + display
./build/bin/sandbox --headless --sprite-demo   # CI-safe: uploads no-op, exits clean
```

## How it's verified

- **`ctest sprite_probe`** rasterizes sprites **off-screen** (via lavapipe in CI) and checks the
  actual pixels: a 2×2 red/green/blue/white texture drawn as one big sprite must sample its four
  quadrant colors at the right screen positions (texture upload + UV mapping), a separately-tinted
  1×1 white texture must come out magenta (tinting + a second batch), and the image corners must
  stay the clear color. This proves the sprite path **draws correctly**, not merely that it compiles.
- **`ctest sprite_demo_headless`** runs the sandbox's `--sprite-demo` path headless, exercising the
  `uploadTexture` / `drawSprite` wiring end to end with no GPU.

## Design notes

- **Textures** are device-local `R8G8B8A8_UNORM` images. Pixels are copied through a host-visible
  **staging buffer** with a one-time `UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY` transition. UNORM
  (not sRGB) keeps a sampled texel equal to its source byte, which is what the pixel test asserts on.
- **Descriptor sets:** one combined image-sampler set per texture, from a fixed-size pool
  (`kMaxTextures`). One shared nearest/clamp sampler.
- **Geometry** is a per-frame-in-flight, host-visible dynamic vertex+index buffer, grown on demand.
  A frame's buffer is only rewritten after its in-flight fence has been waited on, so growing it in
  place is safe.
- The projection push-constant is a single `mat4`; positions arrive already in pixels.

### Deferred (later phases)

Image-file loading (stb_image / KTX2) so textures come from PNGs instead of code; a pannable /
zoomable `Camera2D`; a chunked tilemap renderer on top of this batch; line/shape debug draw; and
mipmaps for scaled-down sprites.
