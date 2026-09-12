// tests/math/resamplepolyline.cpp — verifies even arc-length polyline resampling
// (math::resamplePolyline / resamplePolylineBySpacing). Ground truths: a straight line resampled to N points
// yields exactly evenly spaced points with endpoints preserved; an L-shaped path resamples across the corner
// at the correct arc positions; by-spacing emits a point every `spacing` units from the start; count<2 and
// degenerate/empty inputs are handled; and the 3D overload works. Distances cross-checked by hand. Pure CPU.
#include "maz/math/ResamplePolyline.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec2;
using maz::math::vec3;
using maz::math::resamplePolyline;
using maz::math::resamplePolylineBySpacing;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // --- 1. Straight line resampled to 6 points -> x = 0,2,4,6,8,10, endpoints preserved. ---
    {
        std::vector<vec2> line{vec2(0, 0), vec2(10, 0)};
        const auto r = resamplePolyline(line, 6);
        CHECK(r.size() == 6, "count honoured");
        bool ok = true;
        for (int i = 0; i < 6; ++i) ok = ok && near(r[static_cast<std::size_t>(i)].x, 2.0f * static_cast<float>(i)) &&
                                          near(r[static_cast<std::size_t>(i)].y, 0.0f);
        CHECK(ok, "line evenly spaced at x=0,2,4,6,8,10");
        CHECK(near(r.front().x, 0.0f) && near(r.back().x, 10.0f), "endpoints preserved");
    }

    // --- 2. L-shape (0,0)->(0,4)->(3,4), total 7, resampled to 8 points (spacing 1). ---
    {
        std::vector<vec2> L{vec2(0, 0), vec2(0, 4), vec2(3, 4)};
        const auto r = resamplePolyline(L, 8); // arc 0..7 step 1
        CHECK(r.size() == 8, "L count honoured");
        CHECK(near(r[4].x, 0.0f) && near(r[4].y, 4.0f), "arc 4 = corner (0,4)");
        CHECK(near(r[5].x, 1.0f) && near(r[5].y, 4.0f), "arc 5 = (1,4)");
        CHECK(near(r[7].x, 3.0f) && near(r[7].y, 4.0f), "arc 7 = end (3,4)");
    }

    // --- 3. count == 2 returns just the endpoints; count < 1 empty; count 1 -> first. ---
    {
        std::vector<vec2> line{vec2(1, 1), vec2(2, 2), vec2(5, 9)};
        const auto two = resamplePolyline(line, 2);
        CHECK(two.size() == 2 && near(two[0].x, 1.0f) && near(two[1].x, 5.0f), "count 2 -> endpoints");
        CHECK(resamplePolyline(line, 0).empty(), "count 0 -> empty");
        const auto one = resamplePolyline(line, 1);
        CHECK(one.size() == 1 && near(one[0].x, 1.0f), "count 1 -> first point");
    }

    // --- 4. Degenerate inputs. ---
    {
        CHECK(resamplePolyline(std::vector<vec2>{}, 5).empty(), "empty input -> empty");
        std::vector<vec2> coincident{vec2(3, 3), vec2(3, 3), vec2(3, 3)};
        const auto r = resamplePolyline(coincident, 4);
        CHECK(r.size() == 4 && near(r[0].x, 3.0f) && near(r[3].x, 3.0f), "zero-length path -> copies of first");
    }

    // --- 5. By-spacing: line length 10, spacing 2.5 -> arc 0,2.5,5,7.5,10 (5 points). ---
    {
        std::vector<vec2> line{vec2(0, 0), vec2(10, 0)};
        const auto r = resamplePolylineBySpacing(line, 2.5f);
        CHECK(r.size() == 5, "by-spacing count = 5");
        bool ok = true;
        const float xs[5] = {0.0f, 2.5f, 5.0f, 7.5f, 10.0f};
        for (int i = 0; i < 5; ++i) ok = ok && near(r[static_cast<std::size_t>(i)].x, xs[i]);
        CHECK(ok, "by-spacing at x=0,2.5,5,7.5,10");
        CHECK(resamplePolylineBySpacing(line, 0.0f).empty(), "spacing 0 -> empty");
    }

    // --- 6. 3D overload: diagonal line resampled evenly. ---
    {
        std::vector<vec3> line{vec3(0, 0, 0), vec3(0, 0, 8)};
        const auto r = resamplePolyline(line, 5); // z = 0,2,4,6,8
        CHECK(r.size() == 5 && near(r[2].z, 4.0f) && near(r[4].z, 8.0f), "3D even resample");
    }

    if (g_fail == 0) {
        std::printf("resample polyline: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
