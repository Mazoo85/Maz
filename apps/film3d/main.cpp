// film3d — render a SCRIPT FORGE reel as a film in three dimensions.
//
//   ./film3d <reel.json> --gif out.gif --fps 12 --width 640
//   ./film3d <reel.json> --out frames/            # a numbered frame sequence
//   ./film3d <reel.json> --contact sheet.qoi      # one still per shot, as a contact sheet
//
// The reel is the same document the flat renderer reads, and the cuts land at the same moments,
// because a film rendered two ways has to be one film. What differs is everything below the cut: the
// set is a room, the characters are bodies standing in it, and the camera is in there with them.
#include "Frame3D.hpp"

#include "maz/render/ImageCodecGifAnim.hpp"
#include "maz/render/ImageCodecPnm.hpp"
#include "maz/render/ImageCodecQoi.hpp"

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

bool flag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == name) {
            return true;
        }
    }
    return false;
}

void usage() {
    std::printf(
        "film3d — a SCRIPT FORGE reel, staged and photographed in three dimensions\n"
        "\n"
        "  film3d <reel.json> [options]\n"
        "\n"
        "  --gif <file>       write the whole film as an animated GIF\n"
        "  --out <dir>        write a numbered frame sequence instead\n"
        "  --contact <file>   write one still per shot as a single contact sheet\n"
        "  --width <px>       frame width (default 640)\n"
        "  --fps <n>          frames per second (default 12)\n"
        "  --ss <1..3>        supersampling; 2 is the useful one (default 2)\n"
        "  --from <s> --to <s>  render only this stretch of the film\n"
        "  --ppm              write .ppm rather than .qoi for frames and contact sheets\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || flag(argc, argv, "--help")) {
        usage();
        return argc < 2 ? 1 : 0;
    }
    const std::string reelPath = argv[1];
    std::ifstream in(reelPath, std::ios::binary);
    if (!in) {
        std::printf("film3d: cannot open %s\n", reelPath.c_str());
        return 1;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const maz::film::Reel reel = maz::film::parseReel(text);
    if (!reel.valid) {
        std::printf("film3d: %s is not a reel: %s\n", reelPath.c_str(), reel.error.c_str());
        return 1;
    }

    const int width = std::atoi(arg(argc, argv, "--width", "640").c_str());
    const int height = static_cast<int>(static_cast<double>(width) / maz::film::kAspect + 0.5);
    const double fps = std::atof(arg(argc, argv, "--fps", "12").c_str());
    const int ss = std::atoi(arg(argc, argv, "--ss", "2").c_str());
    const bool ppm = flag(argc, argv, "--ppm");
    const double from = std::atof(arg(argc, argv, "--from", "0").c_str());
    const double to = std::atof(arg(argc, argv, "--to", "0").c_str());
    const double last = to > from ? to : reel.duration;

    const std::map<std::string, film3d::Cast> cast = film3d::castReel(reel);

    std::printf("film3d: \"%s\" — %s, %zu shots, %.1fs, %d characters\n", reel.title.c_str(),
                reel.genreLabel.c_str(), reel.shots.size(), reel.duration, static_cast<int>(cast.size()));

    // ---- a contact sheet: the middle of every shot, once -----------------------------------------
    const std::string contact = arg(argc, argv, "--contact", "");
    if (!contact.empty()) {
        const int cols = 4;
        const int rows = static_cast<int>((reel.shots.size() + static_cast<std::size_t>(cols) - 1) /
                                          static_cast<std::size_t>(cols));
        const int cw = width / 2;
        const int ch = static_cast<int>(static_cast<double>(cw) / maz::film::kAspect + 0.5);
        maz::render::Image sheet(cw * cols, ch * (rows < 1 ? 1 : rows),
                                 maz::render::Color{0.02f, 0.02f, 0.03f, 1.0f});
        for (std::size_t i = 0; i < reel.shots.size(); ++i) {
            const maz::film::Shot& sh = reel.shots[i];
            const maz::render::Image f =
                film3d::drawFrame3D(reel, cast, sh.start + sh.duration * 0.5, cw, ch, ss);
            const int ox = static_cast<int>(i % static_cast<std::size_t>(cols)) * cw;
            const int oy = static_cast<int>(i / static_cast<std::size_t>(cols)) * ch;
            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) {
                    sheet.setPixel(ox + x, oy + y, f.getPixel(x, y));
                }
            }
        }
        const bool ok = writeBytes(contact, ppm ? maz::render::encodePnmP6(sheet)
                                                : maz::render::encodeQoi(sheet));
        std::printf("film3d: %s %s (%zu shots)\n", ok ? "wrote" : "FAILED to write", contact.c_str(),
                    reel.shots.size());
        return ok ? 0 : 1;
    }

    const int frames = static_cast<int>((last - from) * fps + 0.5);
    if (frames < 1) {
        std::printf("film3d: nothing to render\n");
        return 1;
    }

    const std::string gif = arg(argc, argv, "--gif", "");
    const std::string outDir = arg(argc, argv, "--out", gif.empty() ? "film3d-out" : "");
    const auto began = std::chrono::steady_clock::now();

    std::vector<maz::render::Image> keep;
    if (!gif.empty()) {
        keep.reserve(static_cast<std::size_t>(frames));
    }
    for (int i = 0; i < frames; ++i) {
        const double t = from + static_cast<double>(i) / fps;
        maz::render::Image img = film3d::drawFrame3D(reel, cast, t, width, height, ss);
        if (!gif.empty()) {
            keep.push_back(std::move(img));
        } else {
            char name[64];
            std::snprintf(name, sizeof name, "/frame-%05d.%s", i, ppm ? "ppm" : "qoi");
            if (!writeBytes(outDir + name, ppm ? maz::render::encodePnmP6(img)
                                               : maz::render::encodeQoi(img))) {
                std::printf("\nfilm3d: could not write into %s — does it exist?\n", outDir.c_str());
                return 1;
            }
        }
        if ((i % 12) == 0 || i + 1 == frames) {
            std::printf("\rfilm3d: %d/%d frames", i + 1, frames);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    if (!gif.empty()) {
        maz::render::GifAnimOptions opts;
        opts.delayCentiseconds = static_cast<int>(100.0 / fps + 0.5);
        opts.loopCount = 0;
        const std::vector<std::uint8_t> bytes = maz::render::encodeGifAnimation(keep, opts);
        if (bytes.empty() || !writeBytes(gif, bytes)) {
            std::printf("film3d: could not write %s\n", gif.c_str());
            return 1;
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        std::printf("film3d: wrote %s — %d frames, %dx%d, %.1fs of film at %g fps, %.2f MB, in %.1fs\n",
                    gif.c_str(), frames, width, height, static_cast<double>(frames) / fps, fps,
                    static_cast<double>(bytes.size()) / (1024.0 * 1024.0), secs);
    } else {
        std::printf("film3d: wrote %d frames into %s\n", frames, outDir.c_str());
    }
    return 0;
}
