// tests/render/gi.cpp — verifies the one-bounce GI hemisphere gather (render::gatherIrradiance /
// bakeIndirect). Pure CPU path-traced irradiance, so every claim is provable headlessly: an open surfel
// under a uniform sky returns exactly the sky color; a surfel facing a bright patch gets more than one
// facing away; a surfel occluded by a dark ceiling is darker than an open one; results stay bounded by the
// input radiances (energy conservation).
#include "maz/render/GlobalIllumination.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;
namespace math = maz::math;

// A large horizontal quad (two triangles) at height h spanning [-s,s] in x and z, carrying `radiance`.
static void addCeiling(std::vector<GiPatch>& patches, float h, float s, math::vec3 radiance) {
    patches.push_back(GiPatch{{-s, h, -s}, {s, h, -s}, {s, h, s}, radiance});
    patches.push_back(GiPatch{{-s, h, -s}, {s, h, s}, {-s, h, s}, radiance});
}

int main() {
    // --- 1. Open surfel under a uniform sky returns exactly the sky color (cosine estimator is exact). ---
    {
        Surfel s{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        GiBakeOptions o;
        o.skyColor = {0.5f, 0.7f, 1.0f};
        o.samples = 256;
        const math::vec3 e = gatherIrradiance(s, {}, o);
        CHECK(std::fabs(e.x - 0.5f) < 1e-3f && std::fabs(e.y - 0.7f) < 1e-3f && std::fabs(e.z - 1.0f) < 1e-3f,
              "open sky returns exactly skyColor");
    }

    // --- 2. Facing a bright patch collects more than facing away from it. ---
    {
        std::vector<GiPatch> patches;
        addCeiling(patches, 2.0f, 20.0f, math::vec3{10.0f, 10.0f, 10.0f}); // bright ceiling above
        GiBakeOptions o;
        o.skyColor = {0.0f, 0.0f, 0.0f}; // black sky, so only the ceiling contributes
        o.samples = 256;

        const Surfel up{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};   // faces the ceiling
        const Surfel down{{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}; // faces the floor (away)
        const math::vec3 eUp = gatherIrradiance(up, patches, o);
        const math::vec3 eDown = gatherIrradiance(down, patches, o);
        CHECK(eUp.x > 1.0f, "up-facing surfel receives strong bounce from the bright ceiling");
        CHECK(eDown.x < 0.05f, "down-facing surfel (away) receives ~nothing");
        CHECK(eUp.x > eDown.x + 1.0f, "facing the light beats facing away");
    }

    // --- 3. Occlusion: a surfel under a dark ceiling is darker than the same surfel out in the open. ---
    {
        std::vector<GiPatch> occluder;
        addCeiling(occluder, 2.0f, 20.0f, math::vec3{0.0f, 0.0f, 0.0f}); // black ceiling blocks the sky
        GiBakeOptions o;
        o.skyColor = {1.0f, 1.0f, 1.0f};
        o.samples = 256;

        const Surfel s{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        const math::vec3 open = gatherIrradiance(s, {}, o);        // no occluder -> full sky
        const math::vec3 shut = gatherIrradiance(s, occluder, o);  // ceiling eats most of the sky
        CHECK(std::fabs(open.x - 1.0f) < 1e-3f, "open surfel sees full sky = 1");
        CHECK(shut.x < 0.4f, "ceiling-occluded surfel is much darker");
        CHECK(shut.x < open.x - 0.5f, "occlusion measurably darkens the gather");
    }

    // --- 4. Energy stays bounded by the inputs (a convex combination of sky + patch radiances). ---
    {
        std::vector<GiPatch> patches;
        addCeiling(patches, 3.0f, 5.0f, math::vec3{5.0f, 5.0f, 5.0f});
        GiBakeOptions o;
        o.skyColor = {1.0f, 1.0f, 1.0f};
        o.samples = 128;
        const Surfel s{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        const math::vec3 e = gatherIrradiance(s, patches, o);
        CHECK(e.x >= 0.0f && e.x <= 5.0f + 1e-3f, "gather bounded below by 0 and above by max input radiance");
        CHECK(e.x > 1.0f, "mix of bright patch + sky exceeds sky alone");
    }

    // --- 5. bakeIndirect runs the gather per surfel and returns one result each. ---
    {
        std::vector<Surfel> surfels = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{2.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        };
        GiBakeOptions o;
        o.skyColor = {0.3f, 0.3f, 0.3f};
        o.samples = 64;
        const std::vector<math::vec3> baked = bakeIndirect(surfels, {}, o);
        CHECK(baked.size() == 3, "one baked value per surfel");
        CHECK(std::fabs(baked[0].x - 0.3f) < 1e-3f, "each open surfel bakes to skyColor");
    }

    if (g_fail == 0) {
        std::printf("gi: OK — sky-exact, facing beats away, occlusion darkens, energy bounded, bake per surfel.\n");
        return 0;
    }
    std::printf("gi: %d failure(s).\n", g_fail);
    return 1;
}
