#pragma once

// A render::Renderer that draws nothing and remembers everything.
//
// The engine talks to the GPU only through render::Renderer, which is what makes the UI layer
// testable without one: substitute this, call the code under test, and read back the exact draw
// calls it would have issued. Tests assert on *numbers* — a glyph's sprite rectangle, its UVs,
// how many sprites a string produced — not on pixels, so they run in CI under no display and
// fail with a readable diff instead of an image comparison.
//
// Everything here is deliberately inert. createTexture hands out sequential handles so callers
// can tell one atlas from another; nothing is allocated, nothing is uploaded, and no method has
// a side effect beyond appending to a log.

#include "maz/render/Renderer.hpp"

#include <cstdint>
#include <vector>

namespace maz::testing {

class RecordingRenderer final : public render::Renderer {
public:
    // One recorded sprite draw, with the texture it was drawn from.
    struct SpriteCall {
        render::TextureHandle texture = render::kInvalidTexture;
        render::SpriteDesc desc{};
    };

    // One recorded texture upload. `pixels` is a copy, so a test can inspect the atlas after the
    // caller's staging buffer has gone out of scope.
    struct TextureUpload {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> pixels;
    };

    std::vector<SpriteCall> sprites;
    std::vector<TextureUpload> textures;

    // Reported by renderStats(); set it to whatever a test wants the overlay to read back.
    render::RenderStats stats{};

    // Make createTexture fail, to exercise the caller's failure path.
    bool textureCreationFails = false;

    void clear() {
        sprites.clear();
        textures.clear();
    }

    // --- the parts tests actually use ------------------------------------------------------

    render::TextureHandle createTexture(std::uint32_t width, std::uint32_t height,
                                        const void* rgbaPixels) override {
        if (textureCreationFails) {
            return render::kInvalidTexture;
        }
        TextureUpload up;
        up.width = width;
        up.height = height;
        if (rgbaPixels != nullptr) {
            const auto* bytes = static_cast<const std::uint8_t*>(rgbaPixels);
            up.pixels.assign(bytes, bytes + static_cast<std::size_t>(width) * height * 4);
        }
        textures.push_back(std::move(up));
        // Handle 0 is the invalid sentinel, so the first real texture is 1.
        return static_cast<render::TextureHandle>(textures.size());
    }

    void drawSprite(render::TextureHandle texture, const render::SpriteDesc& sprite) override {
        sprites.push_back(SpriteCall{texture, sprite});
    }

    render::RenderStats renderStats() const override { return stats; }

    // --- everything else: inert ------------------------------------------------------------

    bool init(platform::Window&, const render::RendererConfig&) override { return true; }
    void shutdown() override {}
    void onResize(std::uint32_t, std::uint32_t) override {}
    bool beginFrame() override { return true; }
    void setClearColor(const render::Color&) override {}
    void endFrame() override {}
    render::TextureHandle loadTexture(const char*) override { return render::kInvalidTexture; }
    void setCamera2D(const render::Camera2D&) override {}
    render::MeshHandle createMesh(const render::MeshVertex*, std::uint32_t, const std::uint32_t*,
                                  std::uint32_t) override {
        return render::kInvalidMesh;
    }
    render::MeshHandle createDynamicMesh(const render::MeshVertex*, std::uint32_t,
                                         const std::uint32_t*, std::uint32_t) override {
        return render::kInvalidMesh;
    }
    void updateMesh(render::MeshHandle, const render::MeshVertex*, std::uint32_t) override {}
    void setViewProjection3D(const float*) override {}
    void setCameraPosition(const float*) override {}
    void setLighting(const render::SceneLighting&) override {}
    void setBloom(float, float) override {}
    void setSsao(bool, float, float) override {}
    void setTonemap(float, bool, TonemapOp) override {}
    void setColorGrade(float, float, float, bool) override {}
    void setChromaticAberration(float) override {}
    void setFilmGrain(float, float) override {}
    void setWireframe(bool) override {}
    void setCameraBasis(const float[3], const float[3]) override {}
    // The base class overloads these; `using` keeps its convenience overloads visible.
    using render::Renderer::drawMesh;
    using render::Renderer::drawParticle3D;
    void drawParticle3D(const float[3], float, const float[4], bool) override {}
    void drawMesh(render::MeshHandle, const float*, render::TextureHandle,
                  render::TextureHandle) override {}
    void drawMeshEmissive(render::MeshHandle, const float*, render::TextureHandle,
                          render::TextureHandle, const float[3]) override {}
    void drawMeshMaterial(render::MeshHandle, const float*, const Material&) override {}
    void drawMeshInstanced(render::MeshHandle, const float*, std::uint32_t,
                           const Material&) override {}
    void drawMeshTransparent(render::MeshHandle, const float*, const Material&, float) override {}
    void drawLine(const float[3], const float[3], const float[4]) override {}
    void drawAabb(const float[3], const float[3], const float[4]) override {}
    bool isActive() const override { return true; }
};

} // namespace maz::testing
