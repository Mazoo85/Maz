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

// How a 2D draw composites over what's already in the framebuffer.
//   Alpha    — standard "over" blend (src·a + dst·(1−a)); the default for sprites/UI/shapes.
//   Additive — src·a is ADDED to dst; overlapping draws get brighter, never darker. This is how
//              Godot's Light2D accumulates light, and the right mode for glows / fire / energy.
enum class BlendMode {
    Alpha,
    Additive,
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

// A 2D point in the active camera's space (world or pixel), used for polygon fills.
struct Point2 {
    float x = 0.0f, y = 0.0f;
};

// A 2D polygon vertex carrying its own color (for gradient fills / light pools).
struct PolyVertex {
    float x = 0.0f, y = 0.0f;
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
    // Fill a CONVEX polygon (points in world/pixel space per the active 2D camera, any winding),
    // triangulated as a fan and flat-shaded with `color` — the vector-shape primitive (analogous to
    // Godot's draw_colored_polygon / Polygon2D). `blend` selects alpha (default) or additive
    // compositing. No-op when inactive.
    virtual void drawConvexPolygon(const Point2* points, uint32_t count, Color color,
                                   BlendMode blend = BlendMode::Alpha) {
        (void)points;
        (void)count;
        (void)color;
        (void)blend;
    }
    // A triangle fan from verts[0] with PER-VERTEX color (interpolated) — for gradients: 2D light
    // pools (bright center, faded rim), soft fills. verts[0] must "see" every other vertex (the shape
    // is a fan / star around it). `blend` selects alpha (default) or additive — additive is how 2D
    // lights accumulate (overlaps brighten), matching Godot's Light2D. No-op when inactive.
    virtual void drawPolygonFan(const PolyVertex* verts, uint32_t count,
                                BlendMode blend = BlendMode::Alpha) {
        (void)verts;
        (void)count;
        (void)blend;
    }

    // --- 3D meshes (Phase 3) ---
    // Upload an indexed mesh. Returns kInvalidMesh on failure or when inactive.
    virtual MeshHandle createMesh(const MeshVertex* vertices, uint32_t vertexCount,
                                  const uint32_t* indices, uint32_t indexCount) = 0;
    // Upload an indexed mesh whose vertices can be re-streamed every frame via updateMesh (animated
    // geometry: water, cloth, morph targets). Indices are fixed. Returns kInvalidMesh if inactive.
    virtual MeshHandle createDynamicMesh(const MeshVertex* vertices, uint32_t vertexCount,
                                         const uint32_t* indices, uint32_t indexCount) = 0;
    // Replace a dynamic mesh's vertices for the current frame (vertexCount must not exceed the
    // count it was created with). No-op for a static mesh or when inactive.
    virtual void updateMesh(MeshHandle mesh, const MeshVertex* vertices, uint32_t vertexCount) = 0;
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
    // Enable ACES filmic tonemapping of the HDR scene with an exposure multiplier (1 = neutral).
    // The scene renders to a float target, so emissive/bloom can exceed 1 and the tonemap rolls the
    // highlights off smoothly. Off by default (a faithful passthrough); enable per app. No-op when
    // inactive.
    virtual void setTonemap(float exposure, bool enabled) = 0;
    // Enable a filmic color grade on the composited image: `vignette` darkens the corners (0..1),
    // `saturation` scales chroma (1 = neutral), `contrast` scales about mid-grey (1 = neutral). Off
    // by default (a faithful passthrough). No-op when inactive.
    virtual void setColorGrade(float vignette, float saturation, float contrast, bool enabled) = 0;
    // Radial chromatic aberration on the composited image: `strength` is the UV split at the screen
    // edge (e.g. 0.004). 0 (default) is off. No-op when inactive.
    virtual void setChromaticAberration(float strength) = 0;
    // Animated film grain on the composited image: `strength` is the noise amplitude (0 = off, the
    // default), `time` (e.g. the app clock) shifts the pattern each frame. No-op when inactive.
    virtual void setFilmGrain(float strength, float time) = 0;
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
    // Draw a mesh with an added emissive (self-illumination) color — applied after lighting, so the
    // object glows on its own and feeds bloom (coins, lamps, lava). emissive3 is linear RGB (values
    // above 1 glow harder). normal may be kInvalidTexture for no bump map. No-op when inactive.
    virtual void drawMeshEmissive(MeshHandle mesh, const float* model16, TextureHandle albedo,
                                  TextureHandle normal, const float emissive3[3]) = 0;

    // Surface material for a mesh draw. The defaults reproduce the plain matte look of drawMesh
    // (no emissive, no specular), so switching an existing draw to drawMeshMaterial changes nothing
    // until you raise `specular`. `roughness` 1 is a broad soft highlight, 0 a tight glossy one.
    struct Material {
        TextureHandle albedo = kInvalidTexture;
        TextureHandle normal = kInvalidTexture;
        float emissive[3] = {0.0f, 0.0f, 0.0f};
        float roughness = 1.0f;
        float specular = 0.0f; // specular strength (0 = matte)
    };
    // Draw a mesh with a full material (albedo + normal map + emissive + specular/roughness). The
    // specular highlight comes from the sun. No-op when inactive.
    virtual void drawMeshMaterial(MeshHandle mesh, const float* model16, const Material& mat) = 0;
    // Draw `count` instances of one mesh in a single instanced draw call — `models16` is `count`
    // contiguous column-major 4x4 matrices, one per instance; all instances share `mat`. The big win
    // over calling drawMesh in a loop is one draw call for the whole batch (foliage, debris, crowds).
    // Instanced meshes receive shadows but do not cast them. No-op when inactive.
    virtual void drawMeshInstanced(MeshHandle mesh, const float* models16, uint32_t count,
                                   const Material& mat) = 0;
    // Draw a translucent mesh with `opacity` in [0,1] (1 = fully opaque). Transparent draws are
    // collected, sorted back-to-front by camera distance, and blended over the opaque scene after
    // it is complete (glass, water panes, force fields, ghosts). They receive lighting/fog like
    // opaque meshes but do not write depth (so they don't occlude each other) or cast shadows.
    // No-op when inactive.
    virtual void drawMeshTransparent(MeshHandle mesh, const float* model16, const Material& mat,
                                     float opacity) = 0;

    // --- Debug draw (world-space lines) ---
    // Queue a world-space line segment (RGBA, alpha-blended, depth-tested so geometry occludes it).
    // Useful for visualizing colliders, light ranges, paths, and gizmos. No-op when inactive.
    virtual void drawLine(const float a3[3], const float b3[3], const float color4[4]) = 0;
    // Queue the 12 edges of an axis-aligned box between min and max corners. No-op when inactive.
    virtual void drawAabb(const float min3[3], const float max3[3], const float color4[4]) = 0;

    // Draw counts from the previous completed frame (all zero when inactive).
    virtual RenderStats renderStats() const = 0;

    // True when a real GPU + presentable surface are backing this renderer.
    virtual bool isActive() const = 0;
};

// Factory for the Vulkan backend.
std::unique_ptr<Renderer> createVulkanRenderer();

} // namespace maz::render
