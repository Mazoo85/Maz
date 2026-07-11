#pragma once

#include <cstdint>
#include <memory>

namespace maz::platform {
class Window;
}

namespace maz::render {

struct RendererConfig {
    bool vsync = true;
    bool enableValidation = false;   // Vulkan validation layers (debug builds)
    // When true the renderer may run without a presentable surface / GPU and simply no-ops.
    // Used for headless CI so the rest of the engine can still be exercised.
    bool allowHeadless = false;
};

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

// Opaque texture handle. 0 is the invalid/"no texture" sentinel.
using TextureHandle = uint32_t;
constexpr TextureHandle kInvalidTexture = 0;

// One sprite draw. Position/size are in the active 2D camera's units (pixels by default),
// origin at the sprite's top-left. `rotation` is radians about the sprite center. `uvMin`/`uvMax`
// select a sub-rectangle of the texture (0..1); defaults cover the whole texture. `color` tints.
struct SpriteDesc {
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;
    float rotation = 0.0f;
    float uvMinX = 0.0f, uvMinY = 0.0f;
    float uvMaxX = 1.0f, uvMaxY = 1.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
};

// 2D camera. Defaults to pixel-space matching the framebuffer (origin top-left, y-down).
struct Camera2D {
    float centerX = 0.0f, centerY = 0.0f; // world point at the viewport center (0 = use default)
    float zoom = 1.0f;
    bool usePixelSpace = true;            // when true, ignore center/zoom and map 1 unit = 1 pixel
};

// A vertex for 3D meshes: position, normal, RGB color, and texture coordinate (object-space).
struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
    float u, v;
};

// Opaque handle to an uploaded mesh. 0 is invalid.
using MeshHandle = uint32_t;
constexpr MeshHandle kInvalidMesh = 0;

// Per-frame draw counts for profiling/debug overlays (the previous completed frame).
struct RenderStats {
    uint32_t meshDraws = 0; // meshes drawn after frustum culling
    uint32_t culled = 0;    // meshes skipped by frustum culling
    uint32_t particles = 0;
    uint32_t sprites = 0;
};

// Scene lighting for the 3D mesh path: ambient + one shadow-mapped directional "sun", plus up to
// kMaxPointLights positional point lights (lamps, window glow). Defaults reproduce the engine's
// standard daytime look, so apps that never set lighting are unaffected.
struct SceneLighting {
    static constexpr uint32_t kMaxPointLights = 8;
    struct Point {
        float pos[3] = {0, 0, 0};
        float range = 8.0f;         // distance at which the light fades to zero
        float color[3] = {1, 1, 1}; // light color
        float intensity = 1.0f;     // scales color
        // Optional spotlight cone. spotOuterDeg > 0 turns this into a spotlight aimed along
        // spotDir, full-bright within spotInnerDeg and fading to dark by spotOuterDeg (half-angles,
        // degrees). Left at 0 (default) the light is an omnidirectional point light.
        float spotDir[3] = {0, -1, 0};
        float spotInnerDeg = 0.0f;
        float spotOuterDeg = 0.0f;
    };
    float ambient[3] = {0.30f, 0.30f, 0.30f};
    float sunDir[3] = {0.4f, 0.8f, 0.6f};    // direction toward the sun (need not be normalized)
    float sunColor[3] = {0.85f, 0.85f, 0.85f};
    Point points[kMaxPointLights];
    uint32_t pointCount = 0;
    // Exponential distance fog blending meshes toward fogColor; fogDensity 0 disables it (default),
    // so apps that don't opt in are unaffected. Pair fogColor with the sky horizon to fade cleanly.
    float fogColor[3] = {0.72f, 0.82f, 0.95f};
    float fogDensity = 0.0f;
    // Sky gradient colors used by the sky pass (defaults reproduce the standard daytime sky). The
    // sky's sun glow follows sunDir/sunColor above, so animating the sun moves the glow too.
    float skyZenith[3] = {0.24f, 0.44f, 0.82f};
    float skyHorizon[3] = {0.72f, 0.82f, 0.95f};
    float skyGround[3] = {0.42f, 0.45f, 0.50f};
};

// Rendering interface. Gameplay talks to this, never to Vulkan directly, so a future backend
// (a 3D path, a null/software path, WebGPU) can be swapped in without touching game code.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(platform::Window& window, const RendererConfig& cfg) = 0;
    virtual void shutdown() = 0;

    // Rebuild swapchain-sized resources after a window resize.
    virtual void onResize(uint32_t width, uint32_t height) = 0;

    // Acquire the next frame. Returns false if there's nothing to draw into this frame
    // (minimized, swapchain out-of-date, or headless/no-GPU) — callers should skip endFrame.
    virtual bool beginFrame() = 0;
    virtual void setClearColor(const Color& color) = 0;
    virtual void endFrame() = 0;

    // --- 2D sprites (Phase 3) ---
    // Load an RGBA image from disk. Returns kInvalidTexture on failure or when inactive.
    virtual TextureHandle loadTexture(const char* path) = 0;
    // Create a texture from tightly-packed RGBA8 pixels (rowlen = width*4).
    virtual TextureHandle createTexture(uint32_t width, uint32_t height, const void* rgbaPixels) = 0;
    // Set the 2D camera used for subsequent drawSprite calls this frame.
    virtual void setCamera2D(const Camera2D& camera) = 0;
    // Queue a sprite for drawing between beginFrame/endFrame. No-op when inactive.
    virtual void drawSprite(TextureHandle texture, const SpriteDesc& sprite) = 0;

    // --- 3D meshes (Phase 3) ---
    // Upload an indexed mesh. Returns kInvalidMesh on failure or when inactive.
    virtual MeshHandle createMesh(const MeshVertex* vertices, uint32_t vertexCount,
                                  const uint32_t* indices, uint32_t indexCount) = 0;
    // Set the combined view*projection matrix (column-major, 16 floats) for 3D draws this frame.
    virtual void setViewProjection3D(const float* viewProj16) = 0;
    // Set the world-space camera position for this frame (3 floats). Only needed for distance fog;
    // harmless otherwise. No-op when inactive.
    virtual void setCameraPosition(const float* pos3) = 0;
    // Set the scene lighting (ambient + sun + point lights + fog) for the 3D mesh path. Persists
    // until changed; defaults to the standard daytime look. No-op when inactive.
    virtual void setLighting(const SceneLighting& lighting) = 0;
    // Enable/adjust post-process bloom. strength 0 (default) is a faithful passthrough, so 2D and
    // 3D apps look identical; > 0 adds a soft glow to areas brighter than `threshold` (0..1).
    virtual void setBloom(float strength, float threshold) = 0;
    // Draw meshes as wireframe (line polygons) instead of filled — a debug-draw aid. Requires the
    // GPU's fillModeNonSolid feature; silently stays filled if unsupported. No-op when inactive.
    virtual void setWireframe(bool enabled) = 0;
    // Set the camera's world-space right/up axes (3 floats each) used to orient billboard
    // particles. Call once per frame before drawParticle3D; harmless otherwise.
    virtual void setCameraBasis(const float right3[3], const float up3[3]) = 0;
    // Queue a camera-facing billboard particle at a world position, with a size and RGBA color.
    // additive=true glows (embers/sparks); false is alpha-blended (smoke/dust). Depth-tested
    // against the 3D scene. No-op when inactive.
    virtual void drawParticle3D(const float pos3[3], float size, const float color4[4],
                                bool additive) = 0;
    // Convenience: additive particle.
    void drawParticle3D(const float pos3[3], float size, const float color4[4]) {
        drawParticle3D(pos3, size, color4, true);
    }
    // Queue a mesh draw with the given model matrix (column-major, 16 floats), an albedo texture
    // (use a white texture for flat/vertex-colored meshes), and an optional tangent-space normal
    // map (kInvalidTexture -> flat, no bump). Depth-tested, drawn beneath the 2D layer. No-op when
    // inactive.
    virtual void drawMesh(MeshHandle mesh, const float* model16, TextureHandle albedo,
                          TextureHandle normal) = 0;
    // Convenience: draw with no normal map.
    void drawMesh(MeshHandle mesh, const float* model16, TextureHandle albedo) {
        drawMesh(mesh, model16, albedo, kInvalidTexture);
    }

    // Draw counts from the previous completed frame (all zero when inactive).
    virtual RenderStats renderStats() const = 0;

    // True when a real GPU + presentable surface are backing this renderer.
    virtual bool isActive() const = 0;
};

// Factory for the Vulkan backend.
std::unique_ptr<Renderer> createVulkanRenderer();

} // namespace maz::render
