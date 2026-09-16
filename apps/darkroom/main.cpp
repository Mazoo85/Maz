// Maz Engine — "DARKROOM" (render's image processing: ImageBlur, BilateralFilter, MedianFilter,
// SobelEdge, OtsuThreshold, HarrisCorners, SeamCarve, SeamlessClone, ImageNormalMap, ImageBlend,
// ImageGradientMap)
// Eleven filters, each shown against the thing it is supposed to beat, because "it looks smoother"
// is not a result. A blur is only interesting next to the 2D convolution it replaces; a bilateral
// filter next to the plain blur whose edge it keeps; a median next to the average that cannot
// remove an impulse; seam carving next to the resize that squashes the subject. LEFT: making an
// image smoother without making it worse. MIDDLE: finding what is in it — edges, a threshold,
// corners. RIGHT: changing its shape and its colours, and putting one image inside another.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 698 headers.
#include "maz/render/BilateralFilter.hpp"
#include "maz/render/HarrisCorners.hpp"
#include "maz/render/ImageBlend.hpp"
#include "maz/render/ImageBlur.hpp"
#include "maz/render/ImageGradientMap.hpp"
#include "maz/render/ImageNormalMap.hpp"
#include "maz/render/MedianFilter.hpp"
#include "maz/render/OtsuThreshold.hpp"
#include "maz/render/SeamCarve.hpp"
#include "maz/render/SeamlessClone.hpp"
#include "maz/render/SobelEdge.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1e", v);
    return buf;
}

// Deterministic, so every number on screen is the same on every machine and every run.
struct Lcg {
    std::uint64_t s = 0xBEEFu;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float unit() { return static_cast<float>(next() % 100000u) / 100000.0f; }
};

render::Color grey(float v) { return render::Color{v, v, v, 1.0f}; }

// The largest per-pixel difference between two images, in the 0..1 the Image reports.
double worstDiff(const render::Image& a, const render::Image& b) {
    double worst = 0.0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            worst = std::max(worst, std::fabs(static_cast<double>(a.getPixel(x, y).r) -
                                              b.getPixel(x, y).r));
        }
    }
    return worst;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("DARKROOM starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Darkroom";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- separable blur: the same answer, a fraction of the work -------------------------------------
    struct BlurRow {
        int radius = 0;
        std::size_t taps = 0;
        long taps2d = 0;
        long tapsSeparable = 0;
        double kernelSum = 0.0;
        double worst = 0.0;
    };
    std::vector<BlurRow> blurs;
    double flatDrift = 0.0;
    {
        const int w = 64, h = 64;
        std::vector<float> src(static_cast<std::size_t>(w * h));
        Lcg rng;
        for (float& v : src) {
            v = rng.unit();
        }
        for (int radius : {2, 4, 8, 16}) {
            const std::vector<float> k = render::gaussianKernel1D(radius);
            double sum = 0.0;
            for (float v : k) {
                sum += v;
            }
            // The full 2D convolution, done the expensive way, as the thing to match.
            std::vector<float> ref(static_cast<std::size_t>(w * h), 0.0f);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    double a = 0.0;
                    for (int j = -radius; j <= radius; ++j) {
                        for (int i = -radius; i <= radius; ++i) {
                            const int sx = std::clamp(x + i, 0, w - 1);
                            const int sy = std::clamp(y + j, 0, h - 1);
                            a += static_cast<double>(src[static_cast<std::size_t>(sy * w + sx)]) *
                                 static_cast<double>(k[static_cast<std::size_t>(i + radius)]) *
                                 static_cast<double>(k[static_cast<std::size_t>(j + radius)]);
                        }
                    }
                    ref[static_cast<std::size_t>(y * w + x)] = static_cast<float>(a);
                }
            }
            const std::vector<float> got = render::detail::separableBlur(src, w, h, radius, k);
            double worst = 0.0;
            for (std::size_t i = 0; i < ref.size(); ++i) {
                worst = std::max(worst, std::fabs(static_cast<double>(ref[i]) - got[i]));
            }
            const long side = 2L * radius + 1;
            blurs.push_back(BlurRow{radius, k.size(), side * side, 2L * side, sum, worst});
        }
        // A flat image must come back flat: the kernel sums to one or the whole image drifts.
        const std::vector<float> flat(static_cast<std::size_t>(w * h), 0.375f);
        const std::vector<float> blurred = render::gaussianBlurGray(flat, w, h, 6, 0.0f);
        for (float v : blurred) {
            flatDrift = std::max(flatDrift, std::fabs(static_cast<double>(v) - 0.375));
        }
    }

    // ---- bilateral: smoothing that can tell an edge from noise ---------------------------------------
    struct SmoothRow {
        std::string what;
        double noise = 0.0;
        double edgeStep = 0.0;
    };
    std::vector<SmoothRow> smooths;
    {
        const int w = 64, h = 64;
        render::Image src(w, h);
        Lcg rng;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float base = x < w / 2 ? 0.25f : 0.75f;
                src.setPixel(x, y, grey(std::clamp(base + (rng.unit() - 0.5f) * 0.10f, 0.0f, 1.0f)));
            }
        }
        // How rough each flat half still is, measured away from the edge.
        auto flatNoise = [&](const render::Image& im) {
            double total = 0.0;
            for (int half = 0; half < 2; ++half) {
                const int x0 = half ? w / 2 + 6 : 6;
                const int x1 = half ? w - 6 : w / 2 - 6;
                double mean = 0.0;
                int n = 0;
                for (int y = 6; y < h - 6; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        mean += im.getPixel(x, y).r;
                        ++n;
                    }
                }
                mean /= n;
                double var = 0.0;
                for (int y = 6; y < h - 6; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        const double d = im.getPixel(x, y).r - mean;
                        var += d * d;
                    }
                }
                total += std::sqrt(var / n);
            }
            return total * 0.5;
        };
        // How much brightness the edge still carries, across the one pixel it sits on.
        auto edgeStep = [&](const render::Image& im) {
            double left = 0.0, right = 0.0;
            int n = 0;
            for (int y = 6; y < h - 6; ++y) {
                left += im.getPixel(w / 2 - 1, y).r;
                right += im.getPixel(w / 2, y).r;
                ++n;
            }
            return (right - left) / n;
        };
        const render::Image bilateral = render::bilateralFilter(src, 4.0f, 0.10f);
        std::vector<float> g(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                g[static_cast<std::size_t>(y * w + x)] = src.getPixel(x, y).r;
            }
        }
        const std::vector<float> gb = render::gaussianBlurGray(g, w, h, 4, 2.0f);
        render::Image gaussian(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                gaussian.setPixel(x, y, grey(gb[static_cast<std::size_t>(y * w + x)]));
            }
        }
        smooths.push_back(SmoothRow{"untouched", flatNoise(src), edgeStep(src)});
        smooths.push_back(SmoothRow{"gaussian blur", flatNoise(gaussian), edgeStep(gaussian)});
        smooths.push_back(SmoothRow{"bilateral", flatNoise(bilateral), edgeStep(bilateral)});
    }

    // ---- median: the one impulse an average can never remove ------------------------------------------
    struct ImpulseRow {
        std::string what;
        int wrong = 0;
    };
    std::vector<ImpulseRow> impulses;
    int impulseCount = 0, impulseTotal = 0;
    {
        const int w = 64, h = 64;
        std::vector<std::uint8_t> clean(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                clean[static_cast<std::size_t>(y * w + x)] = static_cast<std::uint8_t>(60 + x * 2);
            }
        }
        std::vector<std::uint8_t> noisy = clean;
        Lcg rng;
        for (std::uint8_t& p : noisy) {
            if (rng.unit() < 0.06f) {
                p = rng.unit() < 0.5f ? std::uint8_t{0} : std::uint8_t{255};
                ++impulseCount;
            }
        }
        impulseTotal = static_cast<int>(noisy.size());
        auto stillWrong = [&](const std::vector<std::uint8_t>& v) {
            int n = 0;
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (std::abs(static_cast<int>(v[i]) - static_cast<int>(clean[i])) > 20) {
                    ++n;
                }
            }
            return n;
        };
        std::vector<float> f(noisy.size());
        for (std::size_t i = 0; i < noisy.size(); ++i) {
            f[i] = static_cast<float>(noisy[i]) / 255.0f;
        }
        const std::vector<float> blurred = render::gaussianBlurGray(f, w, h, 1, 0.0f);
        std::vector<std::uint8_t> asBytes(noisy.size());
        for (std::size_t i = 0; i < asBytes.size(); ++i) {
            asBytes[i] = static_cast<std::uint8_t>(std::clamp(blurred[i] * 255.0f, 0.0f, 255.0f));
        }
        impulses.push_back(ImpulseRow{"untouched", stillWrong(noisy)});
        impulses.push_back(ImpulseRow{"gaussian, radius 1", stillWrong(asBytes)});
        impulses.push_back(
            ImpulseRow{"median, radius 1", stillWrong(render::medianFilter(noisy, w, h, 1))});
    }

    // ---- sobel: the gradient knows which way the edge runs ---------------------------------------------
    struct EdgeRow {
        std::string what;
        double asked = 0.0;
        double measured = 0.0;
        double spread = 0.0;
        int strong = 0;
    };
    std::vector<EdgeRow> edges;
    double flatGradient = 0.0;
    std::vector<int> maskCounts;
    {
        const int w = 64, h = 64;
        // A soft edge — brightness ramping over a few pixels, which is what an anti-aliased image
        // looks like. A hard step laid down on a diagonal is a staircase, and a staircase's local
        // gradients point along its steps rather than along the edge.
        const struct {
            double deg;
            const char* what;
        } cases[] = {{0.0, "vertical"},  {90.0, "horizontal"}, {45.0, "diagonal"},
                     {135.0, "the other diagonal"}, {30.0, "thirty degrees"}, {200.0, "two hundred"}};
        for (const auto& c : cases) {
            std::vector<float> g(static_cast<std::size_t>(w * h));
            const double a = c.deg * kPi / 180.0, nx = std::cos(a), ny = std::sin(a);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const double d = ((x - w / 2.0) * nx + (y - h / 2.0) * ny) / 3.0;
                    g[static_cast<std::size_t>(y * w + x)] =
                        static_cast<float>(0.5 + 0.3 * std::tanh(d));
                }
            }
            const render::EdgeField e = render::sobel(g, w, h);
            double peak = 0.0;
            for (float m : e.mag) {
                peak = std::max(peak, static_cast<double>(m));
            }
            double sx = 0.0, sy = 0.0;
            int n = 0;
            for (int y = 2; y < h - 2; ++y) {
                for (int x = 2; x < w - 2; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y * w + x);
                    if (e.mag[i] > 0.5 * peak) {
                        sx += e.gx[i];
                        sy += e.gy[i];
                        ++n;
                    }
                }
            }
            double dir = std::atan2(sy, sx) * 180.0 / kPi;
            if (dir < 0.0) {
                dir += 360.0;
            }
            double spread = 0.0;
            for (int y = 2; y < h - 2; ++y) {
                for (int x = 2; x < w - 2; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y * w + x);
                    if (e.mag[i] > 0.5 * peak) {
                        double d = std::atan2(static_cast<double>(e.gy[i]),
                                              static_cast<double>(e.gx[i])) * 180.0 / kPi;
                        if (d < 0.0) {
                            d += 360.0;
                        }
                        double diff = std::fabs(d - dir);
                        if (diff > 180.0) {
                            diff = 360.0 - diff;
                        }
                        spread = std::max(spread, diff);
                    }
                }
            }
            edges.push_back(EdgeRow{c.what, c.deg, dir, spread, n});
        }
        const std::vector<float> flat(static_cast<std::size_t>(w * h), 0.5f);
        const render::EdgeField fe = render::sobel(flat, w, h);
        for (float v : fe.mag) {
            flatGradient = std::max(flatGradient, static_cast<double>(v));
        }
        std::vector<float> step(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                step[static_cast<std::size_t>(y * w + x)] = x < w / 2 ? 0.2f : 0.8f;
            }
        }
        const render::EdgeField se = render::sobel(step, w, h);
        double peak = 0.0;
        for (float v : se.mag) {
            peak = std::max(peak, static_cast<double>(v));
        }
        for (double frac : {0.9, 0.5, 0.1}) {
            const std::vector<unsigned char> m =
                render::edgeMask(se, static_cast<float>(frac * peak));
            maskCounts.push_back(static_cast<int>(std::count_if(
                m.begin(), m.end(), [](unsigned char v) { return v != 0; })));
        }
    }

    // ---- otsu: the threshold nobody had to pick ---------------------------------------------------------
    struct OtsuRow {
        int lo = 0, hi = 0, midpoint = 0, chosen = 0;
    };
    std::vector<OtsuRow> otsus;
    int discPixels = 0, discTotal = 0, discThreshold = 0, discMarked = 0;
    {
        for (int sep : {40, 80, 120}) {
            std::uint32_t hist[256] = {0};
            const int lo = 128 - sep / 2, hi = 128 + sep / 2;
            for (int v = 0; v < 256; ++v) {
                auto mode = [&](int centre) {
                    const double d = (v - centre) / 12.0;
                    return std::exp(-0.5 * d * d);
                };
                hist[v] = static_cast<std::uint32_t>(4000.0 * mode(lo) + 4000.0 * mode(hi));
            }
            otsus.push_back(OtsuRow{lo, hi, (lo + hi) / 2, render::otsuThresholdHist(hist)});
        }
        const int w = 80, h = 80;
        std::vector<std::uint8_t> img(static_cast<std::size_t>(w * h), 50);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if ((x - 40) * (x - 40) + (y - 40) * (y - 40) < 24 * 24) {
                    img[static_cast<std::size_t>(y * w + x)] = 200;
                    ++discPixels;
                }
            }
        }
        discTotal = w * h;
        discThreshold = render::otsuThreshold(img);
        const std::vector<std::uint8_t> b = render::binarize(img, discThreshold);
        discMarked = static_cast<int>(
            std::count_if(b.begin(), b.end(), [](std::uint8_t v) { return v != 0; }));
    }

    // ---- harris: four corners, and it knows where --------------------------------------------------------
    std::vector<render::Corner> corners;
    std::size_t blankCorners = 0, stripeCorners = 0;
    {
        const int w = 64, h = 64;
        render::Image img(w, h, grey(0.15f));
        img.fillRect(18, 14, 28, 34, grey(0.85f));
        corners = render::harrisCorners(img);
        std::sort(corners.begin(), corners.end(),
                  [](const render::Corner& a, const render::Corner& b) {
                      return a.y != b.y ? a.y < b.y : a.x < b.x;
                  });
        blankCorners = render::harrisCorners(render::Image(w, h, grey(0.5f))).size();
        render::Image stripe(w, h, grey(0.15f));
        stripe.fillRect(0, 20, w, 10, grey(0.85f));
        stripeCorners = render::harrisCorners(stripe).size();
    }

    // ---- seam carving: narrower without squashing the subject ---------------------------------------------
    double energyBefore = 0.0, energyCarved = 0.0, energySquashed = 0.0;
    int seamThroughSubject = 0, subjectRows = 20;
    int carvedW = 0, carvedH = 0;
    {
        const int w = 80, h = 40;
        render::Image img(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                img.setPixel(x, y, grey(0.3f + 0.2f * static_cast<float>(x) / static_cast<float>(w)));
            }
        }
        for (int y = 10; y < 30; ++y) {
            for (int x = 8; x < 28; ++x) {
                img.setPixel(x, y, grey(((x / 2 + y / 2) % 2) ? 0.95f : 0.05f));
            }
        }
        auto totalEnergy = [](const render::Image& im) {
            double e = 0.0;
            for (int y = 1; y < im.height() - 1; ++y) {
                for (int x = 1; x < im.width() - 1; ++x) {
                    e += render::seamEnergyAt(im, x, y);
                }
            }
            return e;
        };
        energyBefore = totalEnergy(img);
        energyCarved = totalEnergy(render::carveWidth(img, 56));
        render::Image squashed = img;
        squashed.resize(56, h);
        energySquashed = totalEnergy(squashed);
        const std::vector<int> seam = render::findVerticalSeam(img);
        for (int y = 10; y < 30; ++y) {
            const int sx = seam[static_cast<std::size_t>(y)];
            if (sx >= 8 && sx < 28) {
                ++seamThroughSubject;
            }
        }
        const render::Image shorter = render::carveHeight(img, 30);
        carvedW = shorter.width();
        carvedH = shorter.height();
    }

    // ---- seamless clone: a patch that stops having a border ------------------------------------------------
    double naiveSeam = 0.0, clonedSeam = 0.0, clonedCentre = 0.0;
    {
        const int w = 64, h = 64;
        const render::Image dest(w, h, grey(0.30f));
        const render::Image src(w, h, grey(0.70f));
        render::Image mask(w, h, grey(0.0f));
        mask.fillRect(20, 20, 24, 24, grey(1.0f));
        render::Image naive = dest;
        for (int y = 20; y < 44; ++y) {
            for (int x = 20; x < 44; ++x) {
                naive.setPixel(x, y, src.getPixel(x, y));
            }
        }
        const render::Image cloned = render::seamlessClone(dest, src, mask, 0, 0);
        auto biggestJump = [&](const render::Image& im) {
            double worst = 0.0;
            for (int y = 20; y < 44; ++y) {
                worst = std::max(worst, std::fabs(static_cast<double>(im.getPixel(20, y).r) -
                                                  im.getPixel(19, y).r));
                worst = std::max(worst, std::fabs(static_cast<double>(im.getPixel(43, y).r) -
                                                  im.getPixel(44, y).r));
            }
            for (int x = 20; x < 44; ++x) {
                worst = std::max(worst, std::fabs(static_cast<double>(im.getPixel(x, 20).r) -
                                                  im.getPixel(x, 19).r));
                worst = std::max(worst, std::fabs(static_cast<double>(im.getPixel(x, 43).r) -
                                                  im.getPixel(x, 44).r));
            }
            return worst;
        };
        naiveSeam = biggestJump(naive);
        clonedSeam = biggestJump(cloned);
        clonedCentre = cloned.getPixel(32, 32).r;
    }

    // ---- height into normals -------------------------------------------------------------------------------
    struct NormalRow {
        std::string what;
        render::Color encoded;
        double nx = 0.0, ny = 0.0, nz = 0.0, length = 0.0;
    };
    std::vector<NormalRow> normals;
    {
        const int w = 32, h = 32;
        auto record = [&](const char* what, const render::Image& heights) {
            const render::Image n = render::heightToNormalMap(heights);
            const render::Color c = n.getPixel(16, 16);
            const double nx = c.r * 2.0 - 1.0, ny = c.g * 2.0 - 1.0, nz = c.b * 2.0 - 1.0;
            normals.push_back(NormalRow{what, c, nx, ny, nz,
                                        std::sqrt(nx * nx + ny * ny + nz * nz)});
        };
        record("flat ground", render::Image(w, h, grey(0.5f)));
        for (float slope : {0.02f, 0.05f}) {
            render::Image ramp(w, h);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    ramp.setPixel(x, y, grey(0.5f + slope * static_cast<float>(x - 16)));
                }
            }
            record(slope < 0.03f ? "a gentle slope" : "a steeper slope", ramp);
        }
    }

    // ---- blend identities ---------------------------------------------------------------------------------
    struct BlendRow {
        std::string what;
        double worst = 0.0;
    };
    std::vector<BlendRow> blends;
    {
        const int w = 16, h = 16;
        auto solid = [&](float v) { return render::Image(w, h, grey(v)); };
        const render::Image base = solid(0.30f);
        using M = render::ImageBlendMode;
        const struct {
            const char* what;
            render::Image got;
            render::Image want;
        } cases[] = {
            {"multiply by white", render::blend(base, solid(1.0f), M::Multiply), base},
            {"multiply by black", render::blend(base, solid(0.0f), M::Multiply), solid(0.0f)},
            {"screen by black", render::blend(base, solid(0.0f), M::Screen), base},
            {"screen by white", render::blend(base, solid(1.0f), M::Screen), solid(1.0f)},
            {"add nothing", render::blend(base, solid(0.0f), M::Add), base},
            {"subtract nothing", render::blend(base, solid(0.0f), M::Subtract), base},
            {"darken by white", render::blend(base, solid(1.0f), M::Darken), base},
            {"lighten by black", render::blend(base, solid(0.0f), M::Lighten), base},
            {"difference with itself", render::blend(base, base, M::Difference), solid(0.0f)},
            {"normal at opacity 0", render::blend(base, solid(0.9f), M::Normal, 0.0f), base},
            {"normal at opacity 1", render::blend(base, solid(0.9f), M::Normal, 1.0f), solid(0.9f)},
            {"normal at opacity 0.5", render::blend(base, solid(0.9f), M::Normal, 0.5f), solid(0.6f)},
        };
        for (const auto& c : cases) {
            blends.push_back(BlendRow{c.what, worstDiff(c.got, c.want)});
        }
    }

    // ---- a grey image given colours --------------------------------------------------------------------------
    double rampDeviation = 0.0;
    std::vector<render::Color> threeStopSamples;
    {
        const int w = 32, h = 4;
        render::Image ramp(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                ramp.setPixel(x, y, grey(static_cast<float>(x) / static_cast<float>(w - 1)));
            }
        }
        const render::Image mapped =
            render::gradientMap(ramp, render::Color{0, 0, 1, 1}, render::Color{1, 0.5f, 0, 1});
        for (int x = 0; x < w; ++x) {
            const double t = static_cast<double>(x) / (w - 1);
            const render::Color c = mapped.getPixel(x, 0);
            rampDeviation = std::max({rampDeviation, std::fabs(static_cast<double>(c.r) - t),
                                      std::fabs(static_cast<double>(c.g) - 0.5 * t),
                                      std::fabs(static_cast<double>(c.b) - (1.0 - t))});
        }
        // Written the obvious way — a plain, non-const vector — which until recently did not compile.
        std::vector<render::ColorStop> stops{{0.0f, render::Color{0, 0, 0, 1}},
                                             {0.5f, render::Color{1, 0, 0, 1}},
                                             {1.0f, render::Color{1, 1, 1, 1}}};
        const render::Image three = render::gradientMap(ramp, stops);
        threeStopSamples = {three.getPixel(0, 0), three.getPixel(w / 2, 0), three.getPixel(w - 1, 0)};
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kBad{1.0f, 0.52f, 0.45f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.26f;
            auto cell = [&](float x, float y, const std::string& s, render::Color colour,
                            float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto swatch = [&](float x, float y, float w, float h, const render::Color& c) {
                const render::Point2 quad[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
                renderer->drawConvexPolygon(quad, 4, c);
            };

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  DARKROOM", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "eleven image filters, each next to the thing it is supposed to beat — "
                          "because \"it looks smoother\" is not a result",
                          kDim, 0.32f);

            // ---- column 1 ----------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "THE SAME BLUR, A FRACTION OF THE WORK", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "radius", kDim, 0.24f);
            cell(110.0f, y, "kernel", kDim, 0.24f);
            cell(190.0f, y, "2D taps", kDim, 0.24f);
            cell(280.0f, y, "separable", kDim, 0.24f);
            cell(380.0f, y, "saving", kDim, 0.24f);
            cell(460.0f, y, "difference", kDim, 0.24f);
            y += 20.0f;
            for (const BlurRow& b : blurs) {
                cell(24.0f, y, std::to_string(b.radius), kText, sz);
                cell(110.0f, y, std::to_string(b.taps), kText, sz);
                cell(190.0f, y, std::to_string(b.taps2d), kDim, sz);
                cell(280.0f, y, std::to_string(b.tapsSeparable), kVal, sz);
                cell(380.0f, y,
                     num(static_cast<double>(b.taps2d) / static_cast<double>(b.tapsSeparable), 1) +
                         "x",
                     kOk, sz);
                cell(460.0f, y, sci(b.worst), b.worst < 1e-5 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(24.0f, y, "a flat 0.375 image blurred: drifts by " + sci(flatDrift), kDim, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The right-hand column is the whole argument. A gaussian is separable, so "
                          "two one-dimensional passes give the SAME image as the full square of "
                          "taps — the difference is a few parts in ten million, which is float "
                          "rounding and nothing else. At radius 16 that is 66 multiplications a pixel "
                          "instead of 1089. The kernel summing to one is what keeps a flat image "
                          "flat rather than fading it.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 24.0f, y, "SMOOTHING THAT KNOWS AN EDGE FROM NOISE", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "", kDim, 0.24f);
            cell(230.0f, y, "roughness left", kDim, 0.24f);
            cell(400.0f, y, "the edge's step", kDim, 0.24f);
            y += 20.0f;
            for (const SmoothRow& s : smooths) {
                cell(24.0f, y, s.what, kText, sz);
                cell(230.0f, y, num(s.noise, 5), kVal, sz);
                cell(400.0f, y, num(s.edgeStep, 4), s.edgeStep > 0.4 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Both filters are told to smooth by the same amount. The gaussian takes "
                          "six sevenths of the roughness out and four fifths of the edge with "
                          "it — it cannot tell the difference, because it only knows how far away "
                          "a neighbour is. The bilateral also weighs how DIFFERENT that neighbour "
                          "is, so the pixels on the far side of the edge are not allowed to vote, "
                          "and the step survives intact.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 24.0f, y, "AND THE NOISE AN AVERAGE CANNOT TOUCH", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y,
                 std::to_string(impulseCount) + " of " + std::to_string(impulseTotal) +
                     " pixels forced to pure black or white; still more than 20 out:",
                 kDim, 0.25f);
            y += 22.0f;
            for (const ImpulseRow& r : impulses) {
                cell(24.0f, y, r.what, kText, sz);
                cell(280.0f, y, std::to_string(r.wrong) + " pixels", r.wrong == 0 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Blurring an impulse does not remove it, it spreads it: every one of the "
                          "240 is still wrong afterwards, and now its neighbours are too. A median "
                          "does not average anything — it asks the nine pixels to vote and takes "
                          "the middle one, and an extreme value can never be the middle of nine. "
                          "All 240 gone, with the gradient underneath untouched.",
                          kDim, 0.25f);

            // ---- column 2 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 700.0f, y, "WHICH WAY DOES THE EDGE RUN", kHead, 0.34f);
            y += 28.0f;
            cell(700.0f, y, "edge", kDim, 0.24f);
            cell(920.0f, y, "asked", kDim, 0.24f);
            cell(1000.0f, y, "sobel says", kDim, 0.24f);
            cell(1120.0f, y, "off by", kDim, 0.24f);
            cell(1200.0f, y, "spread", kDim, 0.24f);
            y += 20.0f;
            for (const EdgeRow& e : edges) {
                const double off = std::fabs(e.measured - e.asked);
                cell(700.0f, y, e.what, kText, sz);
                cell(920.0f, y, num(e.asked, 1), kDim, sz);
                cell(1000.0f, y, num(e.measured, 2), kVal, sz);
                cell(1120.0f, y, num(off, 2), off < 0.5 ? kOk : kBad, sz);
                cell(1200.0f, y, num(e.spread, 2), kDim, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(700.0f, y, "a flat image's strongest gradient: " + sci(flatGradient), kDim, 0.25f);
            y += 21.0f;
            {
                std::string counts;
                const char* labels[3] = {"90%", "50%", "10%"};
                for (std::size_t i = 0; i < maskCounts.size(); ++i) {
                    counts += (i ? ", " : "") + std::string(labels[i]) + " -> " +
                              std::to_string(maskCounts[i]);
                }
                cell(700.0f, y, "one vertical step, mask at " + counts + " pixels", kDim, 0.25f);
            }
            y += 24.0f;
            font.drawText(*renderer, 700.0f, y,
                          "Every angle recovered to within an eighth of a degree, and every "
                          "strong pixel agrees with every other to within a quarter of one. The edges here ramp "
                          "over a few pixels, which is what an anti-aliased image looks like; a "
                          "hard step laid down on a diagonal is a staircase, and a staircase's "
                          "gradients point along its steps rather than along the edge. That is a "
                          "fact about the picture, not about Sobel, and it is worth knowing before "
                          "trusting a direction read off one pixel.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 700.0f, y, "THE THRESHOLD NOBODY HAD TO PICK", kHead, 0.34f);
            y += 28.0f;
            cell(700.0f, y, "two modes at", kDim, 0.24f);
            cell(900.0f, y, "midpoint", kDim, 0.24f);
            cell(1010.0f, y, "otsu chooses", kDim, 0.24f);
            y += 20.0f;
            for (const OtsuRow& o : otsus) {
                cell(700.0f, y, std::to_string(o.lo) + " and " + std::to_string(o.hi), kText, sz);
                cell(900.0f, y, std::to_string(o.midpoint), kDim, sz);
                cell(1010.0f, y, std::to_string(o.chosen),
                     std::abs(o.chosen - o.midpoint) <= 1 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(700.0f, y,
                 "a disc of " + std::to_string(discPixels) + " pixels in " +
                     std::to_string(discTotal) + ": threshold " + std::to_string(discThreshold) +
                     ", binarize marks " + std::to_string(discMarked),
                 discMarked == discPixels ? kOk : kBad, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 700.0f, y,
                          "Otsu picks the cut that leaves the least variance inside the two groups "
                          "it creates. Two equal modes always meet in the middle whatever their "
                          "separation, which is why all three rows land on the same number — and "
                          "the disc is recovered to the pixel, not to within a few.",
                          kDim, 0.25f);

            y += 66.0f;
            font.drawText(*renderer, 700.0f, y, "CORNERS, AND ONLY CORNERS", kHead, 0.34f);
            y += 28.0f;
            cell(700.0f, y,
                 "a rectangle at (18,14)-(45,47): harris found " + std::to_string(corners.size()),
                 corners.size() == 4 ? kOk : kBad, 0.26f);
            y += 22.0f;
            for (const render::Corner& c : corners) {
                cell(720.0f, y,
                     "(" + std::to_string(c.x) + ", " + std::to_string(c.y) + ")   response " +
                         num(static_cast<double>(c.response), 4),
                     kVal, sz);
                y += 21.0f;
            }
            cell(700.0f, y, "a blank image: " + std::to_string(blankCorners) + " corners",
                 blankCorners == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(700.0f, y,
                 "a stripe — two long edges, no corners: " + std::to_string(stripeCorners),
                 stripeCorners == 0 ? kOk : kBad, sz);
            y += 24.0f;
            font.drawText(*renderer, 700.0f, y,
                          "The four it finds are the four that are there, to the pixel, and it "
                          "scores them within a ten-thousandth of each other because the rectangle "
                          "is symmetric. The stripe is the test that matters: an edge detector "
                          "would light up along both of its sides. A corner detector wants "
                          "brightness changing in TWO directions at once, so a stripe gives it "
                          "nothing.",
                          kDim, 0.25f);

            // ---- column 3 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 1450.0f, y, "NARROWER, WITHOUT SQUASHING ANYTHING", kHead, 0.34f);
            y += 28.0f;
            cell(1450.0f, y, "80x40 down to 56 wide; detail surviving:", kDim, 0.25f);
            y += 22.0f;
            cell(1450.0f, y, "the original", kText, sz);
            cell(1650.0f, y, num(energyBefore, 1), kDim, sz);
            y += 21.0f;
            cell(1450.0f, y, "seam carved", kText, sz);
            cell(1650.0f, y, num(energyCarved, 1), kVal, sz);
            cell(1760.0f, y, num(100.0 * energyCarved / energyBefore, 0) + "% kept", kOk, sz);
            y += 21.0f;
            cell(1450.0f, y, "plain resize", kText, sz);
            cell(1650.0f, y, num(energySquashed, 1), kVal, sz);
            cell(1760.0f, y, num(100.0 * energySquashed / energyBefore, 0) + "% kept", kBad, sz);
            y += 24.0f;
            cell(1450.0f, y,
                 "the first seam crosses the subject in " + std::to_string(seamThroughSubject) +
                     " of its " + std::to_string(subjectRows) + " rows",
                 seamThroughSubject == 0 ? kOk : kBad, 0.25f);
            y += 21.0f;
            cell(1450.0f, y,
                 "carving height instead: " + std::to_string(carvedW) + "x" +
                     std::to_string(carvedH),
                 kDim, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 1450.0f, y,
                          "A resize takes the same fraction out of every column, so it takes it out "
                          "of the subject too. Seam carving looks for the cheapest path of pixels "
                          "from top to bottom and deletes that instead, twenty-four times over — "
                          "and not one of those paths went through the busy part, because a path "
                          "through it would cost far more. Ninety-eight per cent of the detail "
                          "survives against fifty-five.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 1450.0f, y, "A PATCH THAT STOPS HAVING A BORDER", kHead, 0.34f);
            y += 28.0f;
            cell(1450.0f, y, "biggest brightness jump across the join:", kDim, 0.25f);
            y += 22.0f;
            cell(1450.0f, y, "straight paste", kText, sz);
            cell(1680.0f, y, num(naiveSeam, 4), kBad, sz);
            y += 21.0f;
            cell(1450.0f, y, "seamless clone", kText, sz);
            cell(1680.0f, y, num(clonedSeam, 4), clonedSeam < 0.01 ? kOk : kBad, sz);
            y += 24.0f;
            cell(1450.0f, y,
                 std::string(
                          "The patch is 0.70 and the background 0.30, so pasting it leaves a step "
                          "of exactly 0.40 all the way round. The clone leaves none at all. The "
                          "revealing number is what the patch's middle became: " +
                              num(clonedCentre, 4) +
                              ". A Poisson clone carries the patch's GRADIENTS, not its "
                              "brightness — and a flat patch has no gradients, so what comes back "
                              "is the background. It transplants texture, not colour, which is "
                              "exactly why a face moved between two photographs keeps its features "
                              "and takes the new light."),
                 kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 1450.0f, y, "HEIGHT INTO NORMALS", kHead, 0.34f);
            y += 28.0f;
            for (const NormalRow& n : normals) {
                cell(1450.0f, y, n.what, kText, sz);
                swatch(1620.0f, y + 2.0f, 22.0f, 14.0f, n.encoded);
                cell(1650.0f, y,
                     num(static_cast<double>(n.encoded.r), 3) + " " +
                         num(static_cast<double>(n.encoded.g), 3) + " " +
                         num(static_cast<double>(n.encoded.b), 3),
                     kVal, 0.24f);
                cell(1800.0f, y, "length " + num(n.length, 4),
                     std::fabs(n.length - 1.0) < 0.01 ? kOk : kBad, 0.24f);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 1450.0f, y,
                          "Straight up encodes as 0.5 0.5 1.0, and flat ground gives that back. "
                          "Tilting the ground tips the red channel and leaves green alone, because "
                          "the slope only runs one way. Every vector comes out a unit length to "
                          "within a hundredth — the rest is the image being eight bits a channel, "
                          "which is also why 0.5 reads as 0.502: 128 of 255.",
                          kDim, 0.25f);

            // ---- column 4 ----------------------------------------------------------------
            y = 720.0f;
            font.drawText(*renderer, 24.0f, y, "THE IDENTITIES EVERY BLEND MODE OWES YOU",
                          kHead, 0.34f);
            y += 28.0f;
            {
                float x = 24.0f;
                int row = 0;
                for (const BlendRow& b : blends) {
                    cell(x, y + static_cast<float>(row) * 21.0f, b.what, kText, sz);
                    cell(x + 220.0f, y + static_cast<float>(row) * 21.0f, sci(b.worst),
                         b.worst <= 1.0 / 255.0 + 1e-6 ? kOk : kBad, sz);
                    ++row;
                    if (row == 6) {
                        row = 0;
                        x += 330.0f;
                    }
                }
            }
            y += 140.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Multiplying by white must change nothing; screening by black must "
                          "change nothing; a layer at zero opacity must not exist; an image "
                          "differenced with itself must be black. Eleven of the twelve are exact "
                          "to the bit. The one that is not reads 3.9e-03, which is one part in "
                          "255 — the smallest difference an eight-bit image can hold, and the "
                          "correct answer rather than a near miss.",
                          kDim, 0.25f);

            y += 60.0f;
            font.drawText(*renderer, 24.0f, y, "AND A GREY IMAGE GIVEN COLOURS", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y,
                 "blue to orange across a ramp: worst deviation from the exact lerp " +
                     sci(rampDeviation),
                 rampDeviation <= 1.0 / 255.0 + 1e-6 ? kOk : kBad, 0.25f);
            y += 22.0f;
            {
                float x = 24.0f;
                const char* at[3] = {"at 0.0", "at 0.5", "at 1.0"};
                for (std::size_t i = 0; i < threeStopSamples.size(); ++i) {
                    cell(x, y, at[i], kDim, 0.24f);
                    swatch(x + 62.0f, y + 2.0f, 22.0f, 14.0f, threeStopSamples[i]);
                    cell(x + 90.0f, y,
                         num(static_cast<double>(threeStopSamples[i].r), 2) + " " +
                             num(static_cast<double>(threeStopSamples[i].g), 2) + " " +
                             num(static_cast<double>(threeStopSamples[i].b), 2),
                         kVal, 0.24f);
                    x += 230.0f;
                }
            }
            y += 28.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The three-stop ramp above is built from a plain std::vector, which until "
                          "this demo was written did not compile: the general callback overload "
                          "took a forwarding reference and so bound a non-const vector better than "
                          "the stop-list overload's const reference did. The documented form only "
                          "worked if you happened to write const. Writing the demo is how that was "
                          "found, which is most of the argument for writing them.",
                          kDim, 0.25f);

            font.drawText(*renderer, 700.0f, 1020.0f,
                          "Every filter here is pure arithmetic on an array — no GPU, no window, "
                          "nothing to look at. Which is why each one can be put next to its "
                          "alternative and made to produce a number instead of an impression.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("DARKROOM shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
