#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game — 2D Smoothed-Particle Hydrodynamics (SPH), the particle-based way to simulate fluids: water,
// goo, lava, blood, or a splash of coloured liquid, as a cloud of little blobs that push apart when squeezed
// and drag along their neighbours. Each particle carries a "smoothing kernel" — a soft bump of influence of
// radius h — and the fluid's density at a particle is the overlap of its neighbours' bumps. Where the fluid
// is denser than its rest density it develops pressure that shoves particles apart; a viscosity term makes
// neighbours share velocity so the flow stays coherent. This is the runtime core behind liquid effects,
// destructible mud/slime, and toy fluid sandboxes. It uses the standard Müller/Monaghan kernels (poly6 for
// density, spiky-gradient for pressure, viscosity-laplacian for drag) with Monaghan's symmetric pressure
// form, so the internal pressure forces conserve momentum exactly. Godot has no fluid solver. Header-only,
// std-only, deterministic; brute-force neighbours (fine for the few-thousand-particle scale games use).
namespace maz::game {

struct SphParams {
    float h = 1.0f;           // smoothing radius
    float restDensity = 1.0f; // target density
    float gasK = 1.0f;        // stiffness (pressure = gasK * (density - restDensity))
    float viscosity = 0.0f;   // viscosity coefficient
    float mass = 1.0f;        // per-particle mass
    math::vec2 gravity{0.0f, 0.0f};
};

namespace detail {
constexpr float kPi = 3.14159265358979323846f;
} // namespace detail

// Density at each particle: rho_i = sum_j mass * W_poly6(|x_i - x_j|, h).
inline std::vector<float> sphDensities(const std::vector<math::vec2>& pos, const SphParams& p) {
    const std::size_t n = pos.size();
    std::vector<float> rho(n, 0.0f);
    const float h = p.h;
    const float h2 = h * h;
    const float c6 = 315.0f / (64.0f * detail::kPi * std::pow(h, 9.0f));
    for (std::size_t i = 0; i < n; ++i) {
        float d = 0.0f;
        for (std::size_t j = 0; j < n; ++j) {
            const float dx = pos[i].x - pos[j].x, dy = pos[i].y - pos[j].y;
            const float r2 = dx * dx + dy * dy;
            if (r2 < h2) {
                const float t = h2 - r2;
                d += p.mass * c6 * t * t * t;
            }
        }
        rho[i] = d;
    }
    return rho;
}

// Acceleration on each particle: symmetric pressure + viscosity + gravity.
inline std::vector<math::vec2> sphAccelerations(const std::vector<math::vec2>& pos,
                                                const std::vector<math::vec2>& vel,
                                                const std::vector<float>& rho, const SphParams& p) {
    const std::size_t n = pos.size();
    std::vector<math::vec2> acc(n, math::vec2{0.0f, 0.0f});
    if (vel.size() != n || rho.size() != n) {
        return acc;
    }
    const float h = p.h;
    const float cSpiky = -45.0f / (detail::kPi * std::pow(h, 6.0f));
    const float cVisc = 45.0f / (detail::kPi * std::pow(h, 6.0f));
    std::vector<float> pressure(n);
    for (std::size_t i = 0; i < n; ++i) {
        pressure[i] = p.gasK * (rho[i] - p.restDensity);
    }
    for (std::size_t i = 0; i < n; ++i) {
        math::vec2 fPress{0.0f, 0.0f}, fVisc{0.0f, 0.0f};
        const float ri2 = rho[i] > 1e-12f ? rho[i] * rho[i] : 1e-12f;
        for (std::size_t j = 0; j < n; ++j) {
            if (j == i) {
                continue;
            }
            const float dx = pos[i].x - pos[j].x, dy = pos[i].y - pos[j].y;
            const float r2 = dx * dx + dy * dy;
            if (r2 >= h * h || r2 < 1e-12f) {
                continue;
            }
            const float r = std::sqrt(r2);
            const float rj2 = rho[j] > 1e-12f ? rho[j] * rho[j] : 1e-12f;
            // Symmetric pressure (Monaghan): grad W points along (x_i - x_j).
            const float gradMag = cSpiky * (h - r) * (h - r);
            const float coeff = p.mass * (pressure[i] / ri2 + pressure[j] / rj2) * gradMag / r;
            fPress.x -= coeff * dx;
            fPress.y -= coeff * dy;
            // Viscosity: pulls velocity toward the neighbour's.
            if (p.viscosity != 0.0f && rho[j] > 1e-12f) {
                const float lap = cVisc * (h - r);
                const float vc = p.viscosity * p.mass * lap / rho[j];
                fVisc.x += vc * (vel[j].x - vel[i].x);
                fVisc.y += vc * (vel[j].y - vel[i].y);
            }
        }
        // Pressure term already carries the 1/rho^2 factors (Monaghan symmetric form), so it is the
        // acceleration directly and must NOT be divided by rho again — that is what keeps the internal
        // pressure forces exactly momentum-conserving. Viscosity uses the standard 1/rho_i.
        const float invRho = rho[i] > 1e-12f ? 1.0f / rho[i] : 0.0f;
        acc[i].x = fPress.x + fVisc.x * invRho + p.gravity.x;
        acc[i].y = fPress.y + fVisc.y * invRho + p.gravity.y;
    }
    return acc;
}

} // namespace maz::game
