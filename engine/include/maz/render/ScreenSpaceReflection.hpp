#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render screen-space reflections (SSR) — the CPU ray-march that drives Godot's SDFGI/SSR-style mirror
// reflections on glossy floors, wet streets, and metal. SSR reflects what's already on screen: for a
// reflective fragment it bounces the view vector about the surface normal, then MARCHES that reflection ray
// forward, projecting each step back into the depth buffer and checking whether the ray has passed *behind*
// a visible surface. The first crossing (refined by a short binary search) is the reflected pixel; its UV is
// where the shader reads the color to mirror. The exact same march runs in a fragment/compute shader on the
// GPU — but because it is pure projection + depth comparison, it is fully unit-testable here against a
// hand-built depth buffer (a ray aimed at a wall must land on that wall's UV; a ray into empty screen or off
// the edge must miss), which is the part SSR most often gets subtly wrong.
//
// Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the TRACE + PROJECTION math below is CPU-verified here and is
// the arithmetic the SSR shader runs. Producing the actual reflected image (sampling the color buffer at the
// returned UV, roughness blur, temporal accumulation) is the GPU pass, verified on the owner's machine.
namespace maz::render {

// Pinhole camera used for SSR's view<->screen projection. Camera sits at the origin looking down +Z, with
// +X right and +Y up (right-handed view space). A view-space point p (p.z > 0, in front of the camera)
// projects to UV in [0,1] (origin top-left):
//   u = 0.5 + focalX * (p.x / p.z)
//   v = 0.5 - focalY * (p.y / p.z)
struct SsrCamera {
    float focalX = 1.0f;  // 0.5 / tan(fovX/2)
    float focalY = 1.0f;  // 0.5 / tan(fovY/2)
    float nearZ = 0.05f;  // nothing closer than this projects

    // Build from a vertical field of view (radians) and width/height aspect ratio.
    static SsrCamera fromFovY(float fovYRadians, float aspect, float nearZ = 0.05f) {
        const float t = std::tan(fovYRadians * 0.5f);
        SsrCamera c;
        c.focalY = 0.5f / t;
        c.focalX = 0.5f / (t * aspect);
        c.nearZ = nearZ;
        return c;
    }
};

// A linear view-space depth buffer: z[y*width + x] is the +Z distance from the camera to the nearest surface
// seen through that pixel. A value <= 0 means "no surface" (sky / infinitely far).
struct DepthBuffer {
    int width = 0;
    int height = 0;
    std::vector<float> z;

    DepthBuffer() = default;
    DepthBuffer(int w, int h, float fill = -1.0f)
        : width(w), height(h), z(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill) {}

    // Nearest-texel fetch by UV. Returns -1 (sky) for out-of-range UV.
    float sampleUV(float u, float v) const {
        if (width <= 0 || height <= 0) return -1.0f;
        const int x = static_cast<int>(u * static_cast<float>(width));
        const int y = static_cast<int>(v * static_cast<float>(height));
        if (x < 0 || y < 0 || x >= width || y >= height) return -1.0f;
        return z[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }
};

struct SsrParams {
    float maxDistance = 30.0f; // how far along the reflection ray to march (view-space units)
    int maxSteps = 128;        // linear march resolution
    float thickness = 0.6f;    // a surface counts as hit while the ray is at most this far behind it
    int refineSteps = 8;       // binary-search iterations that sharpen the hit
};

struct SsrHit {
    bool hit = false;
    float u = 0.0f, v = 0.0f;   // screen UV of the reflected surface (where to sample the color buffer)
    float distance = 0.0f;      // ray travel distance to the hit
    float confidence = 0.0f;    // 0..1, fades toward the screen edge (where SSR data runs out)
};

// Project a view-space point to screen UV. Returns false if it is at/behind the near plane.
inline bool ssrProject(const SsrCamera& cam, float x, float y, float z, float& u, float& v) {
    if (z <= cam.nearZ) return false;
    u = 0.5f + cam.focalX * (x / z);
    v = 0.5f - cam.focalY * (y / z);
    return true;
}

// March a reflection ray (origin o + normalized-internally direction d, view space) against the depth
// buffer and return the first surface it passes behind within `thickness`. A ray that leaves the screen or
// only ever sees sky returns { hit = false }.
inline SsrHit ssrTrace(const SsrCamera& cam, const DepthBuffer& depth,
                       float ox, float oy, float oz,
                       float dx, float dy, float dz,
                       const SsrParams& params = {}) {
    SsrHit out;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-8f || params.maxSteps < 1) return out;
    dx /= len; dy /= len; dz /= len;

    const float stepLen = params.maxDistance / static_cast<float>(params.maxSteps);

    for (int i = 1; i <= params.maxSteps; ++i) {
        const float t = stepLen * static_cast<float>(i);
        const float x = ox + dx * t;
        const float y = oy + dy * t;
        const float z = oz + dz * t;

        float u, v;
        if (!ssrProject(cam, x, y, z, u, v)) break;   // went behind the camera
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) break; // left the screen: SSR can't reflect it

        const float sceneZ = depth.sampleUV(u, v);
        if (sceneZ <= 0.0f) continue;                 // sky at this pixel: keep marching

        const float diff = z - sceneZ;                // > 0 means the ray is behind the visible surface
        if (diff > 0.0f && diff < params.thickness) {
            // Binary-refine between the previous (in-front) step and this (behind) step.
            float t0 = t - stepLen;
            float t1 = t;
            float hu = u, hv = v;
            for (int r = 0; r < params.refineSteps; ++r) {
                const float tm = 0.5f * (t0 + t1);
                const float xm = ox + dx * tm, ym = oy + dy * tm, zm = oz + dz * tm;
                float um, vm;
                if (!ssrProject(cam, xm, ym, zm, um, vm)) { t1 = tm; continue; }
                const float sz = depth.sampleUV(um, vm);
                if (sz > 0.0f && zm - sz > 0.0f) { t1 = tm; hu = um; hv = vm; } // still behind -> pull closer
                else { t0 = tm; }
            }
            out.hit = true;
            out.u = hu;
            out.v = hv;
            out.distance = t1;
            const float edge = std::min(std::min(hu, 1.0f - hu), std::min(hv, 1.0f - hv)) * 2.0f;
            out.confidence = std::clamp(edge, 0.0f, 1.0f);
            return out;
        }
    }
    return out;
}

// Reflect the incident view direction `d` about a unit surface normal `n`: r = d - 2*(d·n)*n. Handy for
// turning a fragment's view vector + normal into the reflection ray fed to ssrTrace.
inline void reflect(float dx, float dy, float dz, float nx, float ny, float nz,
                    float& rx, float& ry, float& rz) {
    const float dot = dx * nx + dy * ny + dz * nz;
    rx = dx - 2.0f * dot * nx;
    ry = dy - 2.0f * dot * ny;
    rz = dz - 2.0f * dot * nz;
}

} // namespace maz::render
