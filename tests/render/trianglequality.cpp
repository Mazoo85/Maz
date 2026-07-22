// tests/render/trianglequality.cpp — verifies triangle shape-quality analysis
// (render::analyzeTriangleQuality). Ground truths of the mean-ratio metric q = 4*sqrt(3)*area/(sum of squared
// edges): an equilateral triangle scores 1 with a 60-degree min angle; a right isosceles scores sqrt(3)/2 ~
// 0.866 with a 45-degree min angle; a long thin sliver scores near 0 with a tiny min angle and is flagged;
// the worst-triangle index and sliver/degenerate counts are correct. Pure CPU, headless.
#include "maz/render/TriangleQuality.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::analyzeTriangleQuality;
using maz::render::TriangleQualityStats;
using maz::render::MeshVertex;
namespace shapes = maz::render::shapes;

static MeshVertex p(float x, float y) { MeshVertex v{}; v.px = x; v.py = y; v.r = v.g = v.b = 1.0f; return v; }
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Equilateral triangle: quality 1, min angle 60. ---
    {
        shapes::MeshData m;
        m.vertices = {p(0,0), p(1,0), p(0.5f, 0.8660254f)}; // side 1 equilateral
        m.indices = {0,1,2};
        const TriangleQualityStats s = analyzeTriangleQuality(m);
        CHECK(near(s.quality[0], 1.0f, 1e-4f), "equilateral quality is 1");
        CHECK(near(s.minAngleDeg[0], 60.0f, 1e-2f), "equilateral min angle is 60 degrees");
        CHECK(s.degenerateCount == 0 && s.sliverCount == 0, "equilateral is neither degenerate nor a sliver");
    }

    // --- 2. Right isosceles triangle: quality sqrt(3)/2, min angle 45. ---
    {
        shapes::MeshData m;
        m.vertices = {p(0,0), p(1,0), p(0,1)};
        m.indices = {0,1,2};
        const TriangleQualityStats s = analyzeTriangleQuality(m);
        CHECK(near(s.quality[0], std::sqrt(3.0f) / 2.0f, 1e-4f), "right isosceles quality is sqrt(3)/2");
        CHECK(near(s.minAngleDeg[0], 45.0f, 1e-2f), "right isosceles min angle is 45 degrees");
    }

    // --- 3. A long thin sliver: quality near 0, tiny min angle, flagged as a sliver. ---
    {
        shapes::MeshData m;
        m.vertices = {p(0,0), p(10,0), p(5, 0.05f)}; // very flat
        m.indices = {0,1,2};
        const TriangleQualityStats s = analyzeTriangleQuality(m);
        CHECK(s.quality[0] < 0.05f, "sliver quality is near zero");
        CHECK(s.minAngleDeg[0] < 5.0f, "sliver has a tiny min angle");
        CHECK(s.sliverCount == 1, "the sliver is flagged at the default threshold");
    }

    // --- 4. A mix: worst-triangle index points at the sliver, ordering by quality. ---
    {
        shapes::MeshData m;
        m.vertices = {
            p(0,0), p(1,0), p(0.5f, 0.8660254f),  // tri 0: equilateral (best)
            p(0,0), p(10,0), p(5, 0.05f),         // tri 1: sliver (worst)
        };
        m.indices = {0,1,2, 3,4,5};
        const TriangleQualityStats s = analyzeTriangleQuality(m);
        CHECK(s.worstTriangle == 1, "worst triangle is the sliver");
        CHECK(s.quality[0] > s.quality[1], "equilateral outranks the sliver");
        CHECK(s.avgQuality > s.minQuality, "average sits above the worst");
        CHECK(near(s.minAngleOverall, s.minAngleDeg[1], 1e-3f), "overall min angle comes from the sliver");
    }

    // --- 5. Degenerate (collinear) triangle counted and excluded. ---
    {
        shapes::MeshData m;
        m.vertices = {p(0,0), p(1,0), p(2,0)}; // collinear
        m.indices = {0,1,2};
        const TriangleQualityStats s = analyzeTriangleQuality(m);
        CHECK(s.degenerateCount == 1, "collinear triangle is degenerate");
        CHECK(near(s.quality[0], 0.0f, 1e-6f), "degenerate triangle has 0 quality recorded");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const TriangleQualityStats s = analyzeTriangleQuality(shapes::MeshData{});
        CHECK(s.triangleCount == 0 && s.quality.empty(), "empty mesh -> empty stats");
    }

    if (g_fail == 0) {
        std::printf("trianglequality: OK — equilateral=1/60deg, right=.866/45deg, sliver flagged, worst found.\n");
        return 0;
    }
    std::printf("trianglequality: %d failure(s).\n", g_fail);
    return 1;
}
