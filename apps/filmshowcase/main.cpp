// filmshowcase — renders the demo reel and writes it as one animated GIF.
//
// Why a GIF, when the browser app already records a WebM: the WebM is VP9 in a WebM container, which
// is what Chromium's MediaRecorder can actually produce, and which an iPhone will not play. A GIF is
// silent and fat, but every device made in the last thirty years plays one. So this is the artifact
// that travels: the engine's own renderer, its own palette quantiser, its own LZW.
//
//   filmshowcase --out showcase.gif --width 480 --fps 12
//   filmshowcase --contact sheet.qoi            (one sheet of every shot, for looking at it)
//
// Montage.hpp is the reel; this file is the frames and the file.
#include "Montage.hpp"

#include "../filmreel/Frame.hpp"
#include "maz/render/ImageCodecGifAnim.hpp"
#include "maz/render/ImageCodecPnm.hpp"
#include "maz/render/ImageCodecQoi.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

using filmreel::drawFrame;
using maz::render::Color;
using maz::render::Image;

bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

std::string argAfter(int argc, char** argv, const std::string& flag, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (flag == argv[i]) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool hasFlag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i) {
        if (flag == argv[i]) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (hasFlag(argc, argv, "--help")) {
        std::printf(
            "filmshowcase — draw the engine's demo reel\n\n"
            "  --out <file.gif>    write the montage as one animated GIF (default showcase.gif)\n"
            "  --contact <file>    instead, one contact sheet of every shot (.qoi or .ppm)\n"
            "  --width <px>        frame width; height follows at 2.35:1 (default 480)\n"
            "  --fps <n>           frames per second (default 12)\n"
            "  --no-dither         skip the ordered dither (smaller file, visible banding)\n");
        return 0;
    }

    const int width = std::atoi(argAfter(argc, argv, "--width", "480").c_str());
    const int fps = std::atoi(argAfter(argc, argv, "--fps", "12").c_str());
    if (width < 64 || fps < 1 || fps > 50) {
        std::printf("filmshowcase: --width must be at least 64 and --fps between 1 and 50\n");
        return 2;
    }
    // Height at the film's own aspect, so the letterbox bars have no height and no bytes.
    const int height = static_cast<int>(std::lround(static_cast<double>(width) / maz::film::kAspect));

    const std::vector<filmshowcase::Chapter> chapters = filmshowcase::montage();
    const std::string contact = argAfter(argc, argv, "--contact", "");

    // --- one sheet of every shot in the montage ---
    if (!contact.empty()) {
        std::vector<std::pair<std::size_t, std::size_t>> cells; // chapter, shot
        for (std::size_t c = 0; c < chapters.size(); ++c) {
            for (std::size_t s = 0; s < chapters[c].beats.size(); ++s) {
                cells.push_back({c, s});
            }
        }
        const int cols = 4;
        const int cellW = width;
        const int cellH = height;
        const int rows = (static_cast<int>(cells.size()) + cols - 1) / cols;
        Image sheet(cellW * cols, cellH * rows, Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const maz::film::Reel reel = filmshowcase::reelFor(chapters[cells[i].first]);
            const maz::film::Shot& s = reel.shots[cells[i].second];
            const Image cell = drawFrame(reel, s.start + s.duration * 0.5, cellW, cellH);
            const int ox = static_cast<int>(i) % cols * cellW;
            const int oy = static_cast<int>(i) / cols * cellH;
            for (int y = 0; y < cellH; ++y) {
                for (int x = 0; x < cellW; ++x) {
                    sheet.setPixel(ox + x, oy + y, cell.getPixel(x, y));
                }
            }
        }
        const bool ppm = contact.size() > 4 && contact.substr(contact.size() - 4) == ".ppm";
        const bool ok = writeBytes(contact, ppm ? maz::render::encodePnmP6(sheet)
                                                : maz::render::encodeQoi(sheet));
        std::printf("filmshowcase: %s %s (%zu shots)\n", ok ? "wrote" : "FAILED to write",
                    contact.c_str(), cells.size());
        return ok ? 0 : 1;
    }

    // --- the montage, frame by frame ---
    const std::string out = argAfter(argc, argv, "--out", "showcase.gif");
    const auto began = std::chrono::steady_clock::now();

    std::vector<Image> frames;
    double filmSeconds = 0.0;
    for (const filmshowcase::Chapter& chapter : chapters) {
        const maz::film::Reel reel = filmshowcase::reelFor(chapter);
        const int count = static_cast<int>(std::lround(reel.duration * fps));
        for (int i = 0; i < count; ++i) {
            frames.push_back(drawFrame(reel, static_cast<double>(i) / static_cast<double>(fps), width,
                                       height));
        }
        filmSeconds += reel.duration;
        std::printf("\r  %s — %d frames", chapter.genreLabel, static_cast<int>(frames.size()));
        std::fflush(stdout);
    }

    maz::render::GifAnimOptions opts;
    opts.delayCentiseconds = static_cast<int>(std::lround(100.0 / static_cast<double>(fps)));
    opts.dither = !hasFlag(argc, argv, "--no-dither");
    const std::vector<std::uint8_t> gif = maz::render::encodeGifAnimation(frames, opts);
    const bool ok = writeBytes(out, gif);

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::printf("\nfilmshowcase: %s %s — %zu frames, %dx%d, %.1fs of reel at %d fps, %.2f MB, in %.1fs\n",
                ok ? "wrote" : "FAILED to write", out.c_str(), frames.size(), width, height,
                filmSeconds, fps, static_cast<double>(gif.size()) / (1024.0 * 1024.0), seconds);
    return ok ? 0 : 1;
}
