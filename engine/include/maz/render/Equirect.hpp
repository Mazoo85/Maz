#pragma once

#include "maz/math/Math.hpp"       // vec2, vec3
#include "maz/render/Image.hpp"    // Image, Color

#include <cmath>

// maz::render — equirectangular (lat-long) panorama mapping: convert a 3D view direction to a (u,v) texture
// coordinate on a 360x180 panorama image, and back. This is how a single wide photo or HDR sky panorama is
// wrapped around a scene as a skybox / environment map (Godot's PanoramaSkyMaterial), how reflection lookups
// read a lat-long environment, and how you sample "what does the world look like in this direction?". The
// horizontal axis is the compass angle (azimuth) around +Y, the vertical axis is the up/down angle
// (elevation) from the top pole to the bottom; matching the engine's SphericalCoords convention (+Z forward
// at u=0.5, +Y up at v=0). The engine has cube maps but no equirectangular sampling. Direction<->UV round-
// trips exactly (away from the poles, where the seam meridian is arbitrary). Header-only, deterministic.
namespace maz::render {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

// Unit (or any) direction -> equirectangular UV in [0,1]^2. u = azimuth (compass), v = elevation (0 top).
inline math::vec2 equirectUvFromDir(const math::vec3& dir) {
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    const math::vec3 d = len > 1e-20f ? math::vec3{dir.x / len, dir.y / len, dir.z / len} : math::vec3{0, 0, 1};
    const float azimuth = std::atan2(d.x, d.z);        // 0 at +Z, +pi/2 at +X
    const float elevation = std::asin(d.y < -1.0f ? -1.0f : (d.y > 1.0f ? 1.0f : d.y)); // -pi/2..pi/2
    float u = azimuth / kTwoPi + 0.5f;
    const float v = 0.5f - elevation / kPi; // 0 at +Y (top), 1 at -Y (bottom)
    if (u < 0.0f) u += 1.0f;
    if (u >= 1.0f) u -= 1.0f;
    return math::vec2{u, v};
}

// Equirectangular UV in [0,1]^2 -> unit direction. Exact inverse of equirectUvFromDir.
inline math::vec3 dirFromEquirectUv(const math::vec2& uv) {
    const float azimuth = (uv.x - 0.5f) * kTwoPi;
    const float elevation = (0.5f - uv.y) * kPi;
    const float cosE = std::cos(elevation);
    return math::vec3{cosE * std::sin(azimuth), std::sin(elevation), cosE * std::cos(azimuth)};
}

// Bilinearly sample an equirectangular panorama image in a direction. Wraps horizontally (seamless around
// the compass), clamps vertically (poles). Empty image -> transparent black.
inline Color sampleEquirect(const Image& img, const math::vec3& dir) {
    if (img.empty()) {
        return Color{0.0f, 0.0f, 0.0f, 0.0f};
    }
    const math::vec2 uv = equirectUvFromDir(dir);
    const int w = img.width(), h = img.height();
    const float fx = uv.x * static_cast<float>(w) - 0.5f;
    const float fy = uv.y * static_cast<float>(h) - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    auto wrapX = [w](int x) { int m = x % w; return m < 0 ? m + w : m; };
    auto clampY = [h](int y) { return y < 0 ? 0 : (y >= h ? h - 1 : y); };
    const Color c00 = img.getPixel(wrapX(x0), clampY(y0));
    const Color c10 = img.getPixel(wrapX(x0 + 1), clampY(y0));
    const Color c01 = img.getPixel(wrapX(x0), clampY(y0 + 1));
    const Color c11 = img.getPixel(wrapX(x0 + 1), clampY(y0 + 1));
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto mix = [&](float a, float b, float c, float d) {
        return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
    };
    return Color{mix(c00.r, c10.r, c01.r, c11.r), mix(c00.g, c10.g, c01.g, c11.g),
                 mix(c00.b, c10.b, c01.b, c11.b), mix(c00.a, c10.a, c01.a, c11.a)};
}

} // namespace maz::render
