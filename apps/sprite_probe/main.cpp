// Maz sprite probe — automated proof that the 2D sprite renderer actually draws.
//
// Renders textured quads off-screen (no window/display) via OffscreenRenderer, reads the pixels
// back, and asserts the sprites landed where and in the colors expected:
//   * A 2x2 "quadrant" texture (red / green / blue / white) is drawn as one big sprite in the
//     middle of the image. Sampling the four quadrant centers must return the four colors — this
//     verifies texture upload, UV mapping, and nearest-filtered sampling.
//   * A separate 1x1 white texture is drawn small with a magenta tint — this verifies per-sprite
//     tinting and that batching across two textures issues both draws.
//   * The image corners must stay the clear color — proving we didn't just flood the frame.
// With a software Vulkan driver (lavapipe) this runs in CI. Exits non-zero on failure.

#include "maz/Engine.hpp"
#include "maz/render/OffscreenRenderer.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace maz;

namespace {

struct Pixel {
    uint8_t r, g, b, a;
};

Pixel at(const std::vector<uint8_t>& px, uint32_t w, uint32_t x, uint32_t y) {
    const size_t i = (static_cast<size_t>(y) * w + x) * 4;
    return {px[i], px[i + 1], px[i + 2], px[i + 3]};
}

} // namespace

int main() {
    const uint32_t size = 128;

    platform::Window window;
    platform::WindowConfig wc;
    wc.headless = true;
    wc.width = size;
    wc.height = size;
    if (!window.init(wc)) {
        std::printf("sprite probe: FAIL (window init)\n");
        return 1;
    }

    render::OffscreenRenderer offscreen;
    if (!offscreen.init(window, size, size)) {
        std::printf("sprite probe: FAIL (no Vulkan device — is a Vulkan ICD installed?)\n");
        window.shutdown();
        return 2;
    }

    // 2x2 texel texture, top row first: TL=red, TR=green, BL=blue, BR=white.
    const uint8_t quad[] = {
        255, 0,   0,   255, /* TL */ 0,   255, 0,   255, /* TR */
        0,   0,   255, 255, /* BL */ 255, 255, 255, 255, /* BR */
    };
    const int quadTex = offscreen.uploadTexture(quad, 2, 2);

    // 1x1 white texel, to be tinted.
    const uint8_t white[] = {255, 255, 255, 255};
    const int whiteTex = offscreen.uploadTexture(white, 1, 1);

    if (quadTex < 0 || whiteTex < 0) {
        std::printf("sprite probe: FAIL (texture upload: quad=%d white=%d)\n", quadTex, whiteTex);
        offscreen.shutdown();
        window.shutdown();
        return 1;
    }

    std::vector<render::OffscreenRenderer::SpriteItem> sprites;
    // Big quadrant sprite centered: dst (32,32)-(96,96), full UV.
    {
        render::OffscreenRenderer::SpriteItem it;
        it.texture = quadTex;
        it.sprite.x = 32.0f;
        it.sprite.y = 32.0f;
        it.sprite.w = 64.0f;
        it.sprite.h = 64.0f;
        sprites.push_back(it);
    }
    // Small magenta-tinted white sprite near the top-right: dst (100,16)-(116,32).
    {
        render::OffscreenRenderer::SpriteItem it;
        it.texture = whiteTex;
        it.sprite.x = 100.0f;
        it.sprite.y = 16.0f;
        it.sprite.w = 16.0f;
        it.sprite.h = 16.0f;
        it.sprite.tint = {1.0f, 0.0f, 1.0f, 1.0f}; // magenta
        sprites.push_back(it);
    }

    const render::Color clear{0.10f, 0.11f, 0.13f, 1.0f};
    std::vector<uint8_t> px;
    if (!offscreen.renderSpritesToPixels(clear, sprites, px)) {
        std::printf("sprite probe: FAIL (render — sprite pipeline unavailable?)\n");
        offscreen.shutdown();
        window.shutdown();
        return 1;
    }

    // Quadrant centers: sprite spans x/y in [32,96); quarter points are at 16px and 48px in.
    const Pixel tl = at(px, size, 48, 48);  // red
    const Pixel tr = at(px, size, 80, 48);  // green
    const Pixel bl = at(px, size, 48, 80);  // blue
    const Pixel br = at(px, size, 80, 80);  // white
    const Pixel tint = at(px, size, 107, 23); // magenta
    const Pixel corner = at(px, size, 3, 3);  // clear

    auto p = [](const char* n, Pixel v) {
        std::printf("  %s=(%u,%u,%u)\n", n, unsigned(v.r), unsigned(v.g), unsigned(v.b));
    };
    p("TL", tl); p("TR", tr); p("BL", bl); p("BR", br); p("tint", tint); p("corner", corner);

    auto nearClear = [](Pixel v) { return v.r < 60 && v.g < 60 && v.b < 70; };

    int failures = 0;
    auto expect = [&](bool cond, const char* what) {
        if (!cond) { std::printf("  MISS: %s\n", what); ++failures; }
    };

    expect(tl.r > 200 && tl.g < 60 && tl.b < 60, "top-left quadrant is red");
    expect(tr.g > 200 && tr.r < 60 && tr.b < 60, "top-right quadrant is green");
    expect(bl.b > 200 && bl.r < 60 && bl.g < 60, "bottom-left quadrant is blue");
    expect(br.r > 200 && br.g > 200 && br.b > 200, "bottom-right quadrant is white");
    expect(tint.r > 200 && tint.g < 60 && tint.b > 200, "tinted sprite is magenta");
    expect(nearClear(corner), "corner pixel is still the clear color");

    std::printf("sprite probe: %s\n", failures == 0 ? "PASS" : "FAIL");
    offscreen.shutdown();
    window.shutdown();
    return failures == 0 ? 0 : 1;
}
