// tests/render/meshprincipalaxes.cpp — verifies the principal-inertia-axes report (render::computePrincipalAxes).
// Ground truths: for a solid box long along X the principal axes align with the world axes, the smallest moment is
// about the long (X) axis, the two cross moments are equal, the moments match the closed-form solid-cuboid values,
// mass = volume, centre = origin, and the axes are orthonormal. An open mesh is invalid. Pure CPU, headless.
#include "maz/render/MeshPrincipalAxes.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static bool neard(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// A closed axis-aligned box of the given half-extents, centred at the origin, outward winding.
static shapes::MeshData box(float hx, float hy, float hz) {
    shapes::MeshData m;
    auto quad = [&](maz::math::vec3 a, maz::math::vec3 b, maz::math::vec3 c, maz::math::vec3 d) {
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        auto v = [](maz::math::vec3 p) { MeshVertex x{}; x.px = p.x; x.py = p.y; x.pz = p.z; x.r = x.g = x.b = 1; return x; };
        m.vertices.push_back(v(a)); m.vertices.push_back(v(b));
        m.vertices.push_back(v(c)); m.vertices.push_back(v(d));
        m.indices.push_back(base + 0); m.indices.push_back(base + 1); m.indices.push_back(base + 2);
        m.indices.push_back(base + 0); m.indices.push_back(base + 2); m.indices.push_back(base + 3);
    };
    using V = maz::math::vec3;
    quad(V(-hx,-hy, hz), V( hx,-hy, hz), V( hx, hy, hz), V(-hx, hy, hz)); // +Z
    quad(V( hx,-hy,-hz), V(-hx,-hy,-hz), V(-hx, hy,-hz), V( hx, hy,-hz)); // -Z
    quad(V( hx,-hy, hz), V( hx,-hy,-hz), V( hx, hy,-hz), V( hx, hy, hz)); // +X
    quad(V(-hx,-hy,-hz), V(-hx,-hy, hz), V(-hx, hy, hz), V(-hx, hy,-hz)); // -X
    quad(V(-hx, hy, hz), V( hx, hy, hz), V( hx, hy,-hz), V(-hx, hy,-hz)); // +Y
    quad(V(-hx,-hy,-hz), V( hx,-hy,-hz), V( hx,-hy, hz), V(-hx,-hy, hz)); // -Y
    return m;
}

int main() {
    // A box long along X: full dims 4 x 1 x 1 (half-extents 2, 0.5, 0.5). Volume = 4.
    const PrincipalAxes p = computePrincipalAxes(box(2.0f, 0.5f, 0.5f));
    CHECK(p.valid, "a closed solid box yields a valid report");

    // --- 1. Mass = volume = 4; centre of mass at the origin. ---
    {
        CHECK(neard(p.mass, 4.0, 1e-3), "mass == volume == 4");
        CHECK(near(p.centre.x, 0.0f, 1e-4f) && near(p.centre.y, 0.0f, 1e-4f) && near(p.centre.z, 0.0f, 1e-4f),
              "centre of mass is at the origin");
    }

    // --- 2. Smallest moment is about the long (X) axis; the two cross moments are equal. ---
    {
        CHECK(p.moment[0] < p.moment[1] && p.moment[0] < p.moment[2], "the long axis has the smallest moment");
        CHECK(std::fabs(p.axis[0].x) > 0.99f && near(p.axis[0].y, 0.0f, 1e-3f) && near(p.axis[0].z, 0.0f, 1e-3f),
              "the smallest-moment axis aligns with X");
        CHECK(neard(p.moment[1], p.moment[2], 1e-3), "the two cross moments (Y,Z) are equal");
    }

    // --- 3. Moments match the closed-form solid cuboid: Ix = m(h^2+d^2)/12, Iy = m(w^2+d^2)/12. ---
    {
        // m=4, w=4,h=1,d=1: Ix = 4(1+1)/12 = 0.6667; Iy = 4(16+1)/12 = 5.6667.
        CHECK(neard(p.moment[0], 4.0 * (1.0 + 1.0) / 12.0, 1e-2), "Ix matches the cuboid formula (~0.667)");
        CHECK(neard(p.moment[1], 4.0 * (16.0 + 1.0) / 12.0, 1e-2), "Iy matches the cuboid formula (~5.667)");
    }

    // --- 4. The three principal axes are orthonormal. ---
    {
        auto dot = [](maz::math::vec3 a, maz::math::vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
        CHECK(near(dot(p.axis[0], p.axis[0]), 1.0f, 1e-3f), "axis 0 is unit length");
        CHECK(near(dot(p.axis[0], p.axis[1]), 0.0f, 1e-3f), "axes 0 and 1 are perpendicular");
        CHECK(near(dot(p.axis[1], p.axis[2]), 0.0f, 1e-3f), "axes 1 and 2 are perpendicular");
    }

    // --- 5. An open mesh (single triangle, no volume) is invalid. ---
    {
        shapes::MeshData open;
        MeshVertex a{}, b{}, c{};
        a.px = 0; b.px = 1; c.py = 1;
        open.vertices = {a, b, c};
        open.indices = {0, 1, 2};
        CHECK(!computePrincipalAxes(open).valid, "an open (zero-volume) mesh is invalid");
    }

    if (g_fail == 0) {
        std::printf("meshprincipalaxes: OK — long axis = smallest moment, cuboid moments match, axes orthonormal.\n");
        return 0;
    }
    std::printf("meshprincipalaxes: %d failure(s).\n", g_fail);
    return 1;
}
