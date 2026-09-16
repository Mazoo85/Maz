// tests/render/softwarerender.cpp — verifies the CPU mesh rasterizer: a front triangle fills the
// centre and leaves the background, the z-buffer occludes correctly, near-plane culling, smooth normal
// interpolation, additive multi-light accumulation, and supersample anti-aliasing. Headless, pure CPU.
#include "maz/math/Projection.hpp"
#include "maz/render/SoftwareRender.hpp"

#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp> // glm::lookAt

using namespace maz;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

static render::MeshVertex vtx(float x, float y, float z, float r, float g, float b) {
    return render::MeshVertex{x, y, z, 0.0f, 0.0f, 1.0f, r, g, b, 0.0f, 0.0f}; // normal +Z
}
// A single white directional light + grey ambient — reproduces the simple lit setup the checks assume.
static render::PreviewLighting lit(const math::vec3& dir, float ambient = 0.2f) {
    render::PreviewLighting L;
    L.lights.push_back({dir, math::vec3(1, 1, 1), 1.0f});
    L.ambient = math::vec3(ambient, ambient, ambient);
    return L;
}

int main() {
    const int W = 32, H = 32;
    const render::Color bg{0.0f, 0.0f, 0.0f, 1.0f};
    const render::PreviewLighting frontLight = lit(math::vec3(0, 0, -1)); // -dir = +Z, so +Z faces are lit
    const math::mat4 identity(1.0f);

    // --- front triangle fills the centre, leaves the corner as background ---
    {
        render::shapes::MeshData m;
        m.vertices = {vtx(-0.8f, -0.8f, 0.5f, 1, 1, 1), vtx(0.8f, -0.8f, 0.5f, 1, 1, 1),
                      vtx(0.0f, 0.8f, 0.5f, 1, 1, 1)};
        m.indices = {0, 1, 2};
        const render::Image img = render::renderMeshPreview(m, identity, math::vec3(0, 0, 5), frontLight, W, H, bg);
        const render::Color centre = img.getPixel(W / 2, H / 2);
        const render::Color corner = img.getPixel(0, 0);
        CHECK(centre.r > 0.8f && centre.g > 0.8f && centre.b > 0.8f, "centre filled & lit (white)");
        CHECK(corner.r < 0.05f && corner.g < 0.05f && corner.b < 0.05f, "corner stays background");
    }

    // --- z-buffer: a nearer red triangle occludes a farther blue one, either submission order ---
    auto centreColorFor = [&](bool blueFirst) {
        render::shapes::MeshData m;
        const render::MeshVertex blue[3] = {vtx(-0.8f, -0.8f, 0.8f, 0, 0, 1),
                                            vtx(0.8f, -0.8f, 0.8f, 0, 0, 1),
                                            vtx(0.0f, 0.8f, 0.8f, 0, 0, 1)};
        const render::MeshVertex red[3] = {vtx(-0.8f, -0.8f, 0.2f, 1, 0, 0),
                                           vtx(0.8f, -0.8f, 0.2f, 1, 0, 0),
                                           vtx(0.0f, 0.8f, 0.2f, 1, 0, 0)};
        if (blueFirst) {
            m.vertices = {blue[0], blue[1], blue[2], red[0], red[1], red[2]};
        } else {
            m.vertices = {red[0], red[1], red[2], blue[0], blue[1], blue[2]};
        }
        m.indices = {0, 1, 2, 3, 4, 5};
        // Eye off-axis so the specular highlight doesn't wash the sampled centre — base colour dominates.
        return render::renderMeshPreview(m, identity, math::vec3(3, 3, 5), frontLight, W, H, bg).getPixel(W / 2, H / 2);
    };
    {
        const render::Color cBlueFirst = centreColorFor(true);
        const render::Color cRedFirst = centreColorFor(false);
        CHECK(cBlueFirst.r > 0.8f && cBlueFirst.b < 0.2f, "near red wins when blue submitted first");
        CHECK(cRedFirst.r > 0.8f && cRedFirst.b < 0.2f, "near red wins when red submitted first");
    }

    // --- near-plane cull: a triangle behind the camera produces no pixels ---
    {
        const math::mat4 proj =
            math::Projection::perspective(glm::radians(60.0f),
                                          static_cast<float>(W) / static_cast<float>(H), 0.1f, 100.0f)
                .m;
        const math::mat4 view = glm::lookAt(math::vec3(0, 0, 3), math::vec3(0, 0, 0), math::vec3(0, 1, 0));
        const math::mat4 vp = proj * view;
        render::shapes::MeshData m;
        m.vertices = {vtx(-1.0f, -1.0f, 10.0f, 1, 1, 1), vtx(1.0f, -1.0f, 10.0f, 1, 1, 1),
                      vtx(0.0f, 1.0f, 10.0f, 1, 1, 1)};
        m.indices = {0, 1, 2};
        const render::Image img = render::renderMeshPreview(m, vp, math::vec3(0, 0, 3), frontLight, W, H, bg);
        const render::Color centre = img.getPixel(W / 2, H / 2);
        CHECK(centre.r < 0.05f && centre.g < 0.05f && centre.b < 0.05f, "behind-camera tri culled");
    }

    // --- smooth shading: per-vertex normals interpolate across the triangle (flat couldn't) ---
    {
        render::shapes::MeshData m;
        m.vertices = {
            render::MeshVertex{-0.8f, -0.8f, 0.5f, 0, 0, 1, 1, 1, 1, 0, 0},   // base verts face +Z -> lit
            render::MeshVertex{0.8f, -0.8f, 0.5f, 0, 0, 1, 1, 1, 1, 0, 0},    // lit
            render::MeshVertex{0.0f, 0.9f, 0.5f, 0, 0, -1, 1, 1, 1, 0, 0},    // apex faces away -> dark
        };
        m.indices = {0, 1, 2};
        const render::Image img = render::renderMeshPreview(m, identity, math::vec3(0, 0, 5), frontLight, W, H, bg);
        const render::Color lo = img.getPixel(W / 2, static_cast<int>(H * 0.75f)); // near the lit base
        const render::Color hi = img.getPixel(W / 2, static_cast<int>(H * 0.2f));  // near the dark apex
        CHECK(lo.r > 0.7f && hi.r < 0.5f && lo.r > hi.r + 0.3f,
              "interpolated normals shade a gradient across one triangle");
    }

    // --- multiple lights accumulate additively ---
    {
        render::shapes::MeshData m;
        m.vertices = {vtx(-0.9f, -0.9f, 0.5f, 1, 1, 1), vtx(0.9f, -0.9f, 0.5f, 1, 1, 1),
                      vtx(0.0f, 0.9f, 0.5f, 1, 1, 1)};
        m.indices = {0, 1, 2};
        const render::DirLight tilt{math::vec3(0.6f, 0.0f, -0.8f), math::vec3(1, 1, 1), 0.5f}; // ndl=0.8 on +Z
        render::PreviewLighting one;
        one.lights = {tilt};
        one.ambient = math::vec3(0, 0, 0);
        render::PreviewLighting two;
        two.lights = {tilt, tilt};
        two.ambient = math::vec3(0, 0, 0);
        const float r1 = render::renderMeshPreview(m, identity, math::vec3(0, 0, 5), one, W, H, bg).getPixel(W / 2, H / 2).r;
        const float r2 = render::renderMeshPreview(m, identity, math::vec3(0, 0, 5), two, W, H, bg).getPixel(W / 2, H / 2).r;
        CHECK(r1 > 0.2f && r2 > r1 + 0.2f, "two lights accumulate brighter than one");
    }

    // --- supersampling anti-aliases edges: more partial-coverage pixels at ssaa > 1 ---
    {
        render::shapes::MeshData m;
        m.vertices = {vtx(-0.7f, -0.6f, 0.5f, 1, 1, 1), vtx(0.6f, -0.8f, 0.5f, 1, 1, 1),
                      vtx(0.2f, 0.7f, 0.5f, 1, 1, 1)}; // slanted edges cross pixels at an angle
        m.indices = {0, 1, 2};
        auto partialCount = [&](int ssaa) {
            const render::Image im =
                render::renderMeshPreview(m, identity, math::vec3(0, 0, 5), frontLight, 40, 40, bg, ssaa);
            int n = 0;
            for (int y = 0; y < im.height(); ++y) {
                for (int x = 0; x < im.width(); ++x) {
                    const float r = im.getPixel(x, y).r;
                    if (r > 0.05f && r < 0.85f) {
                        ++n; // a partially-covered (anti-aliased) edge pixel
                    }
                }
            }
            return n;
        };
        CHECK(partialCount(3) > partialCount(1), "supersampling produces anti-aliased edge pixels");
    }

    if (g_fail == 0) {
        std::printf("softwarerender: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
