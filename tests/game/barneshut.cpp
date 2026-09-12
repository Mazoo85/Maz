// tests/game/barneshut.cpp — verifies the Barnes-Hut n-body force approximation (game BarnesHut.hpp).
// Ground truths, deterministic (seeded LCG for bodies, no <random>, no clock):
//   * with theta = 0 the tree opens fully and reproduces the EXACT brute-force all-pairs acceleration;
//   * with theta = 0.5 the approximation stays within a small relative error of brute force;
//   * an analytic two-body case matches the closed-form inverse-square law;
//   * Newton's third law: the mass-weighted sum of accelerations (total internal force) is ~zero;
//   * degenerate inputs (0/1 body) give zero acceleration.
#include "maz/game/BarnesHut.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::GravBody;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

// Independent O(n^2) all-pairs reference.
static std::vector<vec2> bruteForce(const std::vector<GravBody>& b, float g, float soft) {
    const std::size_t n = b.size();
    std::vector<vec2> acc(n, vec2{0.0f, 0.0f});
    const float eps2 = soft * soft;
    for (std::size_t i = 0; i < n; ++i) {
        vec2 a{0.0f, 0.0f};
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            const vec2 d{b[j].pos.x - b[i].pos.x, b[j].pos.y - b[i].pos.y};
            const float r2 = d.x * d.x + d.y * d.y + eps2;
            const float inv = 1.0f / (r2 * std::sqrt(r2));
            a.x += d.x * g * b[j].mass * inv;
            a.y += d.y * g * b[j].mass * inv;
        }
        acc[i] = a;
    }
    return acc;
}

static float relErr(vec2 a, vec2 b) {
    const float num = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    const float den = std::sqrt(b.x * b.x + b.y * b.y) + 1e-6f;
    return num / den;
}

int main() {
    // --- 1. theta = 0 reproduces brute force exactly. ---
    {
        Lcg rng{0x5A17u};
        bool exactOk = true;
        int trials = 0;
        for (int t = 0; t < 200 && exactOk; ++t) {
            const int n = 2 + static_cast<int>(rng.next() % 40u);
            std::vector<GravBody> b;
            for (int i = 0; i < n; ++i)
                b.push_back(GravBody{vec2{rng.range(-10.0f, 10.0f), rng.range(-10.0f, 10.0f)}, rng.range(0.5f, 5.0f)});
            const auto bh = maz::game::barnesHutAccelerations(b, 0.0f, 1.0f, 0.1f);
            const auto bf = bruteForce(b, 1.0f, 0.1f);
            for (std::size_t i = 0; i < b.size(); ++i)
                if (relErr(bh[i], bf[i]) > 1e-4f) exactOk = false;
            ++trials;
        }
        CHECK(trials > 100, "theta=0 battery ran");
        CHECK(exactOk, "theta=0 reproduces the exact all-pairs acceleration");
    }

    // --- 2. theta = 0.5 stays accurate: per-body error small relative to the SYSTEM's largest force.
    // (Relative-to-self error is meaningless for a body sitting in a near-symmetric cloud whose true net
    //  force is ~0, so normalize by the global force scale instead.) ---
    {
        Lcg rng{0xBEEF1u};
        float worst = 0.0f;
        for (int t = 0; t < 100; ++t) {
            const int n = 50 + static_cast<int>(rng.next() % 150u);
            std::vector<GravBody> b;
            for (int i = 0; i < n; ++i)
                b.push_back(GravBody{vec2{rng.range(-20.0f, 20.0f), rng.range(-20.0f, 20.0f)}, rng.range(0.5f, 3.0f)});
            const auto bh = maz::game::barnesHutAccelerations(b, 0.5f, 1.0f, 0.2f);
            const auto bf = bruteForce(b, 1.0f, 0.2f);
            float maxMag = 1e-6f;
            for (const vec2& a : bf) maxMag = std::fmax(maxMag, std::sqrt(a.x * a.x + a.y * a.y));
            for (std::size_t i = 0; i < b.size(); ++i) {
                const float e = std::sqrt((bh[i].x - bf[i].x) * (bh[i].x - bf[i].x) +
                                          (bh[i].y - bf[i].y) * (bh[i].y - bf[i].y));
                worst = std::fmax(worst, e / maxMag);
            }
        }
        CHECK(worst < 0.05f, "theta=0.5 error is under 5% of the system's largest force");
    }

    // --- 3. Analytic two-body case. ---
    {
        std::vector<GravBody> b{{vec2{0.0f, 0.0f}, 2.0f}, {vec2{3.0f, 0.0f}, 5.0f}};
        const float g = 1.0f, soft = 0.0f;
        const auto a = maz::game::barnesHutAccelerations(b, 0.0f, g, soft);
        // Body 0 pulled toward +x by mass 5 at distance 3: a = g*5/3^2 = 0.5556 in +x.
        const float expect0 = g * 5.0f / 9.0f;
        CHECK(std::fabs(a[0].x - expect0) < 1e-3f && std::fabs(a[0].y) < 1e-4f, "two-body: body 0 acceleration");
        // Body 1 pulled toward -x by mass 2 at distance 3: a = g*2/9 in -x.
        const float expect1 = g * 2.0f / 9.0f;
        CHECK(std::fabs(a[1].x + expect1) < 1e-3f && std::fabs(a[1].y) < 1e-4f, "two-body: body 1 acceleration");
    }

    // --- 4. Newton's third law: mass-weighted total force ~ 0. ---
    {
        Lcg rng{0x9911u};
        std::vector<GravBody> b;
        for (int i = 0; i < 80; ++i)
            b.push_back(GravBody{vec2{rng.range(-5.0f, 5.0f), rng.range(-5.0f, 5.0f)}, rng.range(1.0f, 4.0f)});
        const auto a = maz::game::barnesHutAccelerations(b, 0.0f, 1.0f, 0.3f);
        vec2 total{0.0f, 0.0f};
        float scale = 0.0f;
        for (std::size_t i = 0; i < b.size(); ++i) {
            total.x += b[i].mass * a[i].x;
            total.y += b[i].mass * a[i].y;
            scale += b[i].mass * std::sqrt(a[i].x * a[i].x + a[i].y * a[i].y);
        }
        const float rel = std::sqrt(total.x * total.x + total.y * total.y) / (scale + 1e-6f);
        CHECK(rel < 1e-4f, "sum of mass-weighted accelerations is ~zero (Newton's third law)");
    }

    // --- 5. Degenerate. ---
    {
        CHECK(maz::game::barnesHutAccelerations({}, 0.5f, 1.0f, 0.1f).empty(), "no bodies -> empty");
        std::vector<GravBody> one{{vec2{1.0f, 1.0f}, 1.0f}};
        const auto a = maz::game::barnesHutAccelerations(one, 0.5f, 1.0f, 0.1f);
        CHECK(a.size() == 1 && a[0].x == 0.0f && a[0].y == 0.0f, "single body has zero acceleration");
    }

    if (g_fail == 0) {
        std::printf("barneshut: OK — theta=0 exact vs brute force, theta=0.5 bounded, two-body, momentum, degenerate.\n");
        return 0;
    }
    std::printf("barneshut: %d failure(s).\n", g_fail);
    return 1;
}
