#pragma once

#include <cstdint>
#include <vector>

// maz::render Otsu automatic thresholding — pick the best black/white cutoff for a grayscale image.
//
// Turning a grayscale image into a clean two-tone (foreground vs background) mask needs a threshold, and
// choosing it by hand is fragile across lighting. Otsu's method (1979) finds the threshold AUTOMATICALLY:
// it treats the pixel histogram as two classes split at level t and picks the t that maximises the
// BETWEEN-CLASS variance — i.e. separates the two groups as cleanly as possible (equivalently, minimises
// the spread within each group). This is the standard block behind converting a coverage/height/mask
// texture to 1-bit, isolating a sprite silhouette, blob/marker detection, and "sunk into a valley of the
// histogram" segmentation. Pure CPU on the 256-bin histogram, header-only, deterministic — unit-tested
// on bimodal images where the correct cutoff is obvious.
namespace maz::render {

// Otsu threshold (0..255) from a 256-bin histogram. Returns the level t such that pixels with value > t
// are the "bright" class. If the image is single-valued (one populated bin) the result is that value.
inline int otsuThresholdHist(const std::uint32_t hist[256]) {
    std::uint64_t total = 0;
    double sum = 0.0;
    int populated = 0;
    int lastPopulated = 0;
    for (int i = 0; i < 256; ++i) {
        total += hist[i];
        sum += static_cast<double>(i) * static_cast<double>(hist[i]);
        if (hist[i] != 0) {
            ++populated;
            lastPopulated = i;
        }
    }
    if (total == 0) return 0;
    if (populated == 1) return lastPopulated; // a single-valued image thresholds at that value

    // Sweep t; the between-class variance is constant across an empty stretch of the histogram, so the
    // optimum is often a PLATEAU. Track the max and the first/last t reaching it, then return the
    // plateau midpoint (still an Otsu-optimal split, and the natural cut when a valley is flat).
    double sumB = 0.0;
    std::uint64_t wB = 0;
    double maxVar = -1.0;
    int firstT = 0;
    int lastT = 0;
    for (int t = 0; t < 256; ++t) {
        wB += hist[t];
        if (wB == 0) continue;
        const std::uint64_t wF = total - wB;
        if (wF == 0) break; // all remaining bins empty
        sumB += static_cast<double>(t) * static_cast<double>(hist[t]);
        const double mB = sumB / static_cast<double>(wB);
        const double mF = (sum - sumB) / static_cast<double>(wF);
        const double between =
            static_cast<double>(wB) * static_cast<double>(wF) * (mB - mF) * (mB - mF);
        const double tol = maxVar * 1e-9;
        if (between > maxVar + tol) {
            maxVar = between;
            firstT = t;
            lastT = t;
        } else if (between >= maxVar - tol) {
            lastT = t; // extend the optimal plateau
        }
    }
    return (firstT + lastT) / 2;
}

// Otsu threshold of a grayscale byte image.
inline int otsuThreshold(const std::uint8_t* gray, int n) {
    std::uint32_t hist[256] = {0};
    if (gray != nullptr) {
        for (int i = 0; i < n; ++i) ++hist[gray[i]];
    }
    return otsuThresholdHist(hist);
}
inline int otsuThreshold(const std::vector<std::uint8_t>& gray) {
    return otsuThreshold(gray.data(), static_cast<int>(gray.size()));
}

// Binarize: pixels with value > threshold become 255 (foreground), the rest 0.
inline std::vector<std::uint8_t> binarize(const std::uint8_t* gray, int n, int threshold) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(n < 0 ? 0 : n), 0);
    if (gray == nullptr) return out;
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::size_t>(i)] = gray[i] > threshold ? std::uint8_t{255} : std::uint8_t{0};
    }
    return out;
}
inline std::vector<std::uint8_t> binarize(const std::vector<std::uint8_t>& gray, int threshold) {
    return binarize(gray.data(), static_cast<int>(gray.size()), threshold);
}

} // namespace maz::render
