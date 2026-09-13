// film3d-demo — the engine's own demo reel, staged and photographed in three dimensions.
//
// The same ten chapters the flat showcase runs (apps/filmshowcase/Montage.hpp), rendered through the
// 3D path instead: rooms with floors in them, bodies with proportions, a camera with a focal length.
// Same reel, same cuts, same captions — so the two can be put side by side and the only difference is
// the one being demonstrated.
#include "../filmshowcase/Montage.hpp"
#include "Frame3D.hpp"

#include "maz/render/ImageCodecGifAnim.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

std::string arg(int argc, char** argv, const char* name, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    const std::string out = arg(argc, argv, "--gif", "maz-demo-3d.gif");
    const int width = std::atoi(arg(argc, argv, "--width", "480").c_str());
    const int height = static_cast<int>(static_cast<double>(width) / maz::film::kAspect + 0.5);
    const int fps = std::atoi(arg(argc, argv, "--fps", "12").c_str());
    const int ss = std::atoi(arg(argc, argv, "--ss", "2").c_str());
    const double share = std::atof(arg(argc, argv, "--share", "1.0").c_str());

    const std::vector<filmshowcase::Chapter> chapters = filmshowcase::montage();
    std::vector<maz::render::Image> frames;
    const auto began = std::chrono::steady_clock::now();
    double filmSeconds = 0.0;

    std::printf("film3d-demo: %zu chapters at %dx%d, %d fps\n", chapters.size(), width, height, fps);
    for (const filmshowcase::Chapter& chapter : chapters) {
        const maz::film::Reel reel = filmshowcase::reelFor(chapter);
        const std::map<std::string, film3d::Cast> cast = film3d::castReel(reel);
        const double keep = reel.duration * (share < 0.05 ? 0.05 : (share > 1.0 ? 1.0 : share));
        const int count = static_cast<int>(keep * fps + 0.5);
        for (int i = 0; i < count; ++i) {
            frames.push_back(film3d::drawFrame3D(reel, cast, static_cast<double>(i) / fps, width, height,
                                                 ss));
        }
        filmSeconds += static_cast<double>(count) / fps;
        std::printf("\r  %-12s %d frames", chapter.genreLabel, static_cast<int>(frames.size()));
        std::fflush(stdout);
    }
    std::printf("\n");

    maz::render::GifAnimOptions opts;
    opts.delayCentiseconds = static_cast<int>(100.0 / fps + 0.5);
    opts.loopCount = 0;
    const std::vector<std::uint8_t> bytes = maz::render::encodeGifAnimation(frames, opts);
    const bool ok = !bytes.empty() && writeBytes(out, bytes);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::printf("film3d-demo: %s %s — %zu frames, %dx%d, %.1fs at %d fps, %.2f MB, in %.1fs\n",
                ok ? "wrote" : "FAILED to write", out.c_str(), frames.size(), width, height,
                filmSeconds, fps, static_cast<double>(bytes.size()) / (1024.0 * 1024.0), secs);
    return ok ? 0 : 1;
}
