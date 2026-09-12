#pragma once

#include "maz/math/LinearSolve.hpp" // solveLinearSystem

#include <cstddef>
#include <vector>

// maz::math Savitzky–Golay smoothing — denoise a 1-D signal by fitting a low-degree polynomial to a sliding
// window of samples (least squares) and taking the fitted value at each point. Unlike a moving average,
// which flattens peaks and troughs, an S–G filter PRESERVES the shape of features (peaks, edges, slopes)
// because a polynomial can follow curvature the box filter cannot — the standard tool for cleaning noisy
// sensor/telemetry traces, analog-stick or gyro input, audio envelopes, and procedurally generated curves
// before further processing. Godot offers no such filter. This implementation does a genuine local
// least-squares fit at every point (including a proper asymmetric fit at the two ends), so any polynomial of
// degree <= `order` passes through completely unchanged. Header-only, std-only, deterministic.
namespace maz::math {

// Smooth `y` with a Savitzky–Golay filter: a symmetric window of `2*halfWindow+1` samples and a fitting
// polynomial of degree `order`. Requires order >= 0, halfWindow >= 1, and a window no larger than the
// signal (2*halfWindow+1 <= y.size()) with order < window size; otherwise the input is returned unchanged.
// Endpoints use the nearest in-range window (asymmetric fit) so no samples are dropped.
inline std::vector<float> savitzkyGolay(const std::vector<float>& y, int halfWindow, int order) {
    const int n = static_cast<int>(y.size());
    const int w = 2 * halfWindow + 1;
    const int m = order + 1; // number of polynomial coefficients
    if (halfWindow < 1 || order < 0 || w > n || order >= w) {
        return y;
    }

    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        // Place the window so it stays in range; at the ends this shifts to an asymmetric position.
        int start = i - halfWindow;
        if (start < 0) {
            start = 0;
        }
        if (start > n - w) {
            start = n - w;
        }

        // Local least squares: minimise || J c - ywin ||, where row j of J is [ s^0, s^1, ... s^order ] with
        // s = (sample index) - i, so the fitted value at this point is simply c[0]. Solve the normal
        // equations (J^T J) c = J^T ywin.
        std::vector<double> ata(static_cast<std::size_t>(m * m), 0.0);
        std::vector<double> atb(static_cast<std::size_t>(m), 0.0);
        for (int j = 0; j < w; ++j) {
            const double s = static_cast<double>(start + j - i);
            // Powers s^0..s^order.
            std::vector<double> pw(static_cast<std::size_t>(m));
            double acc = 1.0;
            for (int k = 0; k < m; ++k) {
                pw[static_cast<std::size_t>(k)] = acc;
                acc *= s;
            }
            const double yv = static_cast<double>(y[static_cast<std::size_t>(start + j)]);
            for (int r = 0; r < m; ++r) {
                atb[static_cast<std::size_t>(r)] += pw[static_cast<std::size_t>(r)] * yv;
                for (int c = 0; c < m; ++c) {
                    ata[static_cast<std::size_t>(r * m + c)] +=
                        pw[static_cast<std::size_t>(r)] * pw[static_cast<std::size_t>(c)];
                }
            }
        }

        std::vector<double> coeff;
        if (solveLinearSystem(ata, atb, m, coeff)) {
            out[static_cast<std::size_t>(i)] = static_cast<float>(coeff[0]);
        } else {
            out[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(i)]; // singular — leave as-is
        }
    }
    return out;
}

} // namespace maz::math
