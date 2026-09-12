#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::math — Poisson / Laplace equation solver on a 2D grid by Gauss-Seidel relaxation. Solving
// (discrete) Laplacian(u) = rhs with fixed (Dirichlet) values on chosen cells is the workhorse behind a
// surprising range of game/graphics tasks: the pressure-projection step that makes fluids incompressible,
// gradient-domain / "Poisson" image editing (seamlessly cloning a patch so its interior matches the
// surrounding gradients), steady-state heat/temperature diffusion, and smooth scattered-data interpolation.
// The 5-point stencil sets each free cell to the average of its four neighbours minus the source term; the
// iteration converges to the unique solution consistent with the fixed cells. Any cell flagged `fixed`
// holds its value (the boundary / the region border / a hot spot); all other cells are relaxed. Godot ships
// no PDE solver. Header-only, std-only, deterministic. Iterate to a residual tolerance or an iteration cap.
namespace maz::math {

struct PoissonResult {
    int iterations = 0;    // sweeps performed
    float residual = 0.0f; // final max |Laplacian(u) - rhs| over free cells
};

// Relax the grid `u` (row-major, width*height) in place so that, at every non-fixed interior cell,
// u[left]+u[right]+u[up]+u[down] - 4*u == rhs. Border cells and any cell with fixed[idx] != 0 are held.
// `rhs` is the discrete source term per cell (use 0 for Laplace). Returns sweeps done + final residual.
inline PoissonResult poissonSolve(int width, int height, std::vector<float>& u, const std::vector<float>& rhs,
                                  const std::vector<std::uint8_t>& fixed, int maxIters, float tol) {
    PoissonResult r;
    if (width < 3 || height < 3) {
        return r;
    }
    const std::size_t w = static_cast<std::size_t>(width);
    for (int iter = 0; iter < maxIters; ++iter) {
        // One Gauss-Seidel sweep over interior cells (border is always held).
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
                if (fixed[idx]) {
                    continue;
                }
                const float s = u[idx - 1] + u[idx + 1] + u[idx - w] + u[idx + w];
                u[idx] = (s - rhs[idx]) * 0.25f;
            }
        }
        // Residual: how far the current field is from satisfying the equation.
        float res = 0.0f;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
                if (fixed[idx]) {
                    continue;
                }
                const float lap = u[idx - 1] + u[idx + 1] + u[idx - w] + u[idx + w] - 4.0f * u[idx];
                const float e = std::fabs(lap - rhs[idx]);
                if (e > res) {
                    res = e;
                }
            }
        }
        r.iterations = iter + 1;
        r.residual = res;
        if (res < tol) {
            break;
        }
    }
    return r;
}

} // namespace maz::math
