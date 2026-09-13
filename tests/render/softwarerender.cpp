// tests/render/softwarerender.cpp — verifies the CPU mesh rasterizer: a front triangle fills the
// centre and leaves the background, the z-buffer lets a nearer triangle occlude a farther one
// regardless of submission order, and triangles behind the near plane are culled. Headless, pure CPU.
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

int main() {
    const int W = 32, H = 32;
    const render::Color bg{0.0f, 0.0f, 0.0f, 1.0f};
    const math::vec3 light(0.0f, 0.0f, -1.0f); // -L = +Z, so +Z-facing tris are fully lit
    const math::mat4 identity(1.0f);

    // --- front triangle fills the centre, leaves the corner as background ---
    {
        render::shapes::MeshData m;
        m.vertices = {vtx(-0.8f, -0.8f, 0.5f, 1, 1, 1), vtx(0.8f, -0.8f, 0.5f, 1, 1, 1),
                      vtx(0.0f, 0.8f, 0.5f, 1, 1, 1)};
        m.indices = {0, 1, 2};
        const render::Image img = render::renderMeshPreview(m, identity, light, W, H, bg);
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
        return render::renderMeshPreview(m, identity, light, W, H, bg).getPixel(W / 2, H / 2);
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
        // z = 10 is behind the eye (camera at z=3 looking toward -z) -> clip w <= 0 -> culled.
        m.vertices = {vtx(-1.0f, -1.0f, 10.0f, 1, 1, 1), vtx(1.0f, -1.0f, 10.0f, 1, 1, 1),
                      vtx(0.0f, 1.0f, 10.0f, 1, 1, 1)};
        m.indices = {0, 1, 2};
        const render::Image img = render::renderMeshPreview(m, vp, light, W, H, bg);
        const render::Color centre = img.getPixel(W / 2, H / 2);
        CHECK(centre.r < 0.05f && centre.g < 0.05f && centre.b < 0.05f, "behind-camera tri culled");
    }

    if (g_fail == 0) {
        std::printf("softwarerender: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
