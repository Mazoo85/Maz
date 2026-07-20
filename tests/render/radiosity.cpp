// tests/render/radiosity.cpp — verifies multi-bounce GI (render::bakeRadiosity). The signature of a correct
// radiosity solve: with reflective surfaces (albedo<1) the total scene light GROWS with each bounce but the
// increments SHRINK and it converges to a finite steady state; with zero albedo there is no bounced light;
// and energy is bounded. Built on the tested hemisphere gather, so it verifies headlessly.
#include "maz/render/Radiosity.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;
namespace math = maz::math;

// A large horizontal quad at height y, two triangles wound so the normal points +Y (up) or -Y (down).
static void addQuad(std::vector<RadiosityPatch>& out, float y, bool up, float s, math::vec3 emission,
                    math::vec3 albedo) {
    // Corners.
    const math::vec3 c00{-s, y, -s}, c10{s, y, -s}, c11{s, y, s}, c01{-s, y, s};
    RadiosityPatch t1, t2;
    if (up) { // normal +Y
        t1.a = c00; t1.b = c11; t1.c = c10;
        t2.a = c00; t2.b = c01; t2.c = c11;
    } else {  // normal -Y
        t1.a = c00; t1.b = c10; t1.c = c11;
        t2.a = c00; t2.b = c11; t2.c = c01;
    }
    for (RadiosityPatch* t : {&t1, &t2}) { t->emission = emission; t->albedo = albedo; out.push_back(*t); }
}

static double totalRadiance(const std::vector<RadiosityPatch>& p) {
    double s = 0.0;
    for (const RadiosityPatch& q : p) s += static_cast<double>(q.radiance.x);
    return s;
}

int main() {
    GiBakeOptions opt;
    opt.skyColor = {0.0f, 0.0f, 0.0f};
    opt.samples = 400;

    auto scene = [] {
        std::vector<RadiosityPatch> p;
        // Bright emitting ceiling that also reflects, and a purely-reflective floor facing it.
        addQuad(p, 2.0f, false, 5.0f, math::vec3{1.0f, 1.0f, 1.0f}, math::vec3{0.6f, 0.6f, 0.6f}); // ceiling
        addQuad(p, 0.0f, true, 5.0f, math::vec3{0.0f, 0.0f, 0.0f}, math::vec3{0.7f, 0.7f, 0.7f});   // floor
        return p;
    };

    // --- 1. Total light grows with more bounces but the growth decelerates (convergence). ---
    double t[5];
    for (int b = 1; b <= 4; ++b) {
        std::vector<RadiosityPatch> p = scene();
        bakeRadiosity(p, opt, b);
        t[b] = totalRadiance(p);
    }
    CHECK(t[2] > t[1] + 1e-4, "bounce 2 has more total light than bounce 1 (indirect accumulates)");
    CHECK(t[3] > t[2] + 1e-6, "bounce 3 still adds light");
    CHECK((t[2] - t[1]) > (t[3] - t[2]), "increments shrink — the solve converges");
    CHECK((t[3] - t[2]) > (t[4] - t[3]) - 1e-9, "convergence continues at bounce 4");

    // --- 2. The floor (no emission) is lit only by the bounce, and stays dimmer than the ceiling. ---
    {
        std::vector<RadiosityPatch> p = scene();
        bakeRadiosity(p, opt, 4);
        const float ceiling = p[0].radiance.x; // an emitting ceiling tri
        const float floorR = p[2].radiance.x;  // a reflective floor tri
        CHECK(floorR > 0.05f, "floor receives real bounced light");
        CHECK(floorR < ceiling, "floor stays dimmer than the emitter (energy lost per bounce)");
    }

    // --- 3. Zero albedo => no bounced light: every patch equals its own emission. ---
    {
        std::vector<RadiosityPatch> p;
        addQuad(p, 2.0f, false, 5.0f, math::vec3{1.0f, 1.0f, 1.0f}, math::vec3{0.0f, 0.0f, 0.0f});
        addQuad(p, 0.0f, true, 5.0f, math::vec3{0.0f, 0.0f, 0.0f}, math::vec3{0.0f, 0.0f, 0.0f});
        bakeRadiosity(p, opt, 4);
        CHECK(p[0].radiance.x == 1.0f, "emitter with 0 albedo keeps exactly its emission");
        CHECK(p[2].radiance.x == 0.0f, "0-albedo floor with no emission stays dark");
    }

    // --- 4. Energy is bounded: radiance never exceeds emission/(1-albedo) for the closed pair. ---
    {
        std::vector<RadiosityPatch> p = scene();
        bakeRadiosity(p, opt, 8);
        const double bound = 1.0 / (1.0 - 0.7); // generous geometric-series ceiling
        bool bounded = true;
        for (const RadiosityPatch& q : p)
            if (static_cast<double>(q.radiance.x) > bound + 1e-3) bounded = false;
        CHECK(bounded, "no patch exceeds the geometric-series energy bound");
    }

    if (g_fail == 0) {
        std::printf("radiosity: OK — multi-bounce accumulation, convergence, energy bounds.\n");
        return 0;
    }
    std::printf("radiosity: %d failure(s).\n", g_fail);
    return 1;
}
