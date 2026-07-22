// tests/math/gridsample.cpp — verifies fractional grid sampling (math::gridNearest / gridBilinear / gridBicubic).
// Ground truths: at integer coordinates every filter returns the exact grid value; nearest rounds to the
// closest cell; bilinear on a linear ramp reproduces the ramp exactly and a midpoint is the average of two
// cells; bicubic (Catmull-Rom) passes through the grid values and is also exact on a linear ramp; Clamp edge
// repeats the border while Wrap edge tiles; a vec-typed grid (a flow field) interpolates componentwise.
// Values checked against the interpolation definitions. Pure CPU, deterministic.
#include "maz/math/GridSample.hpp"
#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::math;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // A 4x4 grid whose value equals the x coordinate (a horizontal ramp, constant down each column-of-rows).
    const int W = 4, H = 4;
    std::vector<float> ramp(static_cast<std::size_t>(W * H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) ramp[static_cast<std::size_t>(y * W + x)] = static_cast<float>(x);

    // --- 1. Integer coordinates return exact grid values (all filters). ---
    {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float fx = static_cast<float>(x), fy = static_cast<float>(y);
                CHECK(near(gridNearest(ramp, W, H, fx, fy), static_cast<float>(x)), "nearest exact at integer");
                CHECK(near(gridBilinear(ramp, W, H, fx, fy), static_cast<float>(x)), "bilinear exact at integer");
                CHECK(near(gridBicubic(ramp, W, H, fx, fy), static_cast<float>(x)), "bicubic passes through grid");
            }
    }

    // --- 2. Nearest rounds to the closest cell. ---
    {
        CHECK(near(gridNearest(ramp, W, H, 1.4f, 0.0f), 1.0f), "nearest 1.4 -> cell 1");
        CHECK(near(gridNearest(ramp, W, H, 1.6f, 0.0f), 2.0f), "nearest 1.6 -> cell 2");
    }

    // --- 3. Bilinear on a linear ramp reproduces it exactly; midpoint is the average. ---
    {
        CHECK(near(gridBilinear(ramp, W, H, 1.5f, 0.0f), 1.5f), "bilinear midpoint == average of cells 1,2");
        CHECK(near(gridBilinear(ramp, W, H, 2.25f, 1.0f), 2.25f), "bilinear reproduces the linear ramp");
        // Interpolating vertically on a horizontally-constant column stays constant.
        CHECK(near(gridBilinear(ramp, W, H, 2.0f, 1.5f), 2.0f), "bilinear constant along a flat direction");
    }

    // --- 4. Bicubic is also exact on a linear ramp (Catmull-Rom reproduces linears). ---
    {
        CHECK(near(gridBicubic(ramp, W, H, 1.5f, 1.5f), 1.5f), "bicubic exact on a linear ramp");
        // x=1.3 keeps the full 4-wide stencil (cells 0..3) in-bounds so the ramp is truly linear there.
        CHECK(near(gridBicubic(ramp, W, H, 1.3f, 0.0f), 1.3f), "bicubic reproduces the ramp value (in-bounds stencil)");
    }

    // --- 5. Edge modes: Clamp repeats the border, Wrap tiles. ---
    {
        // x = -0.5 : Clamp reads cell 0 both sides -> 0; Wrap reads cell 3 (value 3) and cell 0 (value 0).
        CHECK(near(gridBilinear(ramp, W, H, -0.5f, 0.0f, GridEdge::Clamp), 0.0f), "clamp: below 0 -> border 0");
        CHECK(near(gridBilinear(ramp, W, H, -1.0f, 0.0f, GridEdge::Wrap), 3.0f), "wrap: -1 -> cell 3 (value 3)");
        // Past the right edge: Clamp holds value 3.
        CHECK(near(gridBilinear(ramp, W, H, 5.0f, 0.0f, GridEdge::Clamp), 3.0f), "clamp: beyond right -> 3");
        CHECK(near(gridNearest(ramp, W, H, 4.0f, 0.0f, GridEdge::Wrap), 0.0f), "wrap: x=4 -> cell 0");
    }

    // --- 6. A vec2 flow-field grid interpolates componentwise. ---
    {
        std::vector<vec2> flow(4); // 2x2: (0,0)=(0,0) (1,0)=(2,0) (0,1)=(0,4) (1,1)=(2,4)
        flow[0] = vec2(0, 0);
        flow[1] = vec2(2, 0);
        flow[2] = vec2(0, 4);
        flow[3] = vec2(2, 4);
        const vec2 mid = gridBilinear(flow, 2, 2, 0.5f, 0.5f);
        CHECK(near(mid.x, 1.0f) && near(mid.y, 2.0f), "vec2 grid: centre is the componentwise average (1,2)");
        const vec2 corner = gridBilinear(flow, 2, 2, 1.0f, 0.0f);
        CHECK(near(corner.x, 2.0f) && near(corner.y, 0.0f), "vec2 grid: exact at a corner");
    }

    if (g_fail == 0) {
        std::printf("gridsample: OK — integer-exact, nearest rounding, bilinear/bicubic ramp, edges, vec grid.\n");
        return 0;
    }
    std::printf("gridsample: %d failure(s).\n", g_fail);
    return 1;
}
