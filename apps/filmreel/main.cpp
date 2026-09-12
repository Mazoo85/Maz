// filmreel — draws a SCRIPT FORGE film reel natively, with no browser.
//
// The browser records a film by playing it: canvas.captureStream(30) into a MediaRecorder. That is a
// REALTIME capture, so a three-minute film takes three minutes, a frame the browser misses is missing
// from the file, and the container has to be patched afterwards to carry a duration the recorder did
// not know when it wrote its header. This draws frame n at time n/fps and writes it. It cannot drop a
// frame, it knows the duration before it starts, and it runs as fast as the machine allows.
//
//   filmreel --reel film.reel.json --out frames/ --fps 24 --width 1280
//   filmreel --reel film.reel.json --contact sheet.qoi          (one sheet of the whole film)
//
// This file is the command line and the files. Frame.hpp is everything that decides what a frame
// looks like, and carries the honest note on what is ported faithfully and what is still plain.
#include "Frame.hpp"

#include "maz/core/Jobs.hpp"
#include "maz/render/ImageCodecPnm.hpp"
#include "maz/render/ImageCodecQoi.hpp"

#include <chrono>
#include <cmath>
#include <deque>
#include <future>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

} // namespace

int main(int argc, char** argv) {
    const std::string reelPath = argAfter(argc, argv, "--reel", "");
    if (reelPath.empty()) {
        std::printf(
            "filmreel — draw a SCRIPT FORGE reel natively\n\n"
            "  --reel <file.reel.json>   the film to draw (required)\n"
            "  --out <dir>               write a frame sequence here (default: filmreel-out)\n"
            "  --contact <file>          instead, write one contact sheet of the whole film\n"
            "  --fps <n>                 frames per second (default 24)\n"
            "  --width <px>              frame width; height follows at 16:9 (default 1280)\n"
            "  --from <s> --to <s>       render only this stretch of the film\n"
            "  --frames <n>              stop after this many frames\n"
            "  --format qoi|ppm          frame format (default qoi, lossless)\n");
        return 2;
    }

    const maz::film::Reel reel = maz::film::loadReel(reelPath);
    if (!reel.valid) {
        std::printf("filmreel: %s\n", reel.error.c_str());
        return 1;
    }

    const int fps = std::atoi(argAfter(argc, argv, "--fps", "24").c_str());
    const int width = std::atoi(argAfter(argc, argv, "--width", "1280").c_str());
    const int height = width * 9 / 16;
    const std::string format = argAfter(argc, argv, "--format", "qoi");
    const std::string contact = argAfter(argc, argv, "--contact", "");
    if (fps < 1 || width < 64) {
        std::printf("filmreel: --fps must be at least 1 and --width at least 64\n");
        return 2;
    }

    std::printf("filmreel: \"%s\" — %s, %zu shots, %s at %dx%d\n", reel.title.c_str(),
                reel.genreLabel.c_str(), reel.shots.size(), maz::film::clock(reel.duration).c_str(),
                width, height);

    const auto began = std::chrono::steady_clock::now();

    // --- one sheet of the whole film, for looking at it ---
    if (!contact.empty()) {
        const int cols = 5;
        const int cellW = width / cols;
        const int cellH = cellW * 9 / 16;
        const int rows = (static_cast<int>(reel.shots.size()) + cols - 1) / cols;
        Image sheet(cellW * cols, cellH * rows, Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (std::size_t i = 0; i < reel.shots.size(); ++i) {
            const maz::film::Shot& s = reel.shots[i];
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
        std::printf("filmreel: %s %s (%d shots)\n", ok ? "wrote" : "FAILED to write", contact.c_str(),
                    static_cast<int>(reel.shots.size()));
        return ok ? 0 : 1;
    }

    // --- the frame sequence ---
    const std::string outDir = argAfter(argc, argv, "--out", "filmreel-out");
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        std::printf("filmreel: could not make the output directory \"%s\": %s\n", outDir.c_str(),
                    ec.message().c_str());
        return 1;
    }

    const double from = std::atof(argAfter(argc, argv, "--from", "0").c_str());
    const double to = std::atof(argAfter(argc, argv, "--to", "0").c_str());
    const double endAt = to > from ? std::fmin(to, reel.duration) : reel.duration;
    const int cap = std::atoi(argAfter(argc, argv, "--frames", "0").c_str());

    const int firstFrame = static_cast<int>(from * fps);
    int total = static_cast<int>(std::ceil(endAt * fps)) - firstFrame;
    if (cap > 0 && cap < total) {
        total = cap;
    }
    if (total < 1) {
        std::printf("filmreel: nothing to render in that stretch\n");
        return 2;
    }

    // Encoding and writing a frame happen on worker threads while the next frame is being drawn.
    // Measured: drawing a 720p frame costs about 24ms and encoding it losslessly about 9ms, so doing
    // the two in sequence spent a quarter of the run on work that had nothing to wait for. A couple
    // of workers is plenty -- the encode is the shorter job, and the point is only to hide it.
    maz::core::JobSystem jobs(2);
    std::deque<std::future<std::string>> pending;
    const std::size_t maxInFlight = 4; // bounds memory: each holds one frame's pixels
    bool failed = false;
    std::string failure;

    auto reap = [&](std::size_t keep) {
        while (pending.size() > keep) {
            const std::string err = pending.front().get();
            pending.pop_front();
            if (!err.empty() && failure.empty()) {
                failure = err;
                failed = true;
            }
        }
    };

    for (int i = 0; i < total && !failed; ++i) {
        const double t = static_cast<double>(firstFrame + i) / static_cast<double>(fps);
        Image frame = drawFrame(reel, t, width, height);
        char name[64];
        std::snprintf(name, sizeof(name), "/frame-%06d.%s", firstFrame + i, format.c_str());
        const std::string path = outDir + name;
        const bool ppm = format == "ppm";
        pending.push_back(jobs.submit([img = std::move(frame), path, ppm]() -> std::string {
            const std::vector<std::uint8_t> bytes =
                ppm ? maz::render::encodePnmP6(img) : maz::render::encodeQoi(img);
            return writeBytes(path, bytes) ? std::string() : path;
        }));
        reap(maxInFlight);
        if ((i + 1) % 25 == 0 || i + 1 == total) {
            std::printf("\r  %d / %d frames", i + 1, total);
            std::fflush(stdout);
        }
    }
    reap(0);
    if (failed) {
        std::printf("\nfilmreel: could not write %s\n", failure.c_str());
        return 1;
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const double filmSeconds = static_cast<double>(total) / static_cast<double>(fps);
    std::printf("\nfilmreel: %d frames of %s film in %.1fs — %.1fx realtime, %.0f frames/second\n",
                total, maz::film::clock(filmSeconds).c_str(), seconds,
                seconds > 0.0 ? filmSeconds / seconds : 0.0,
                seconds > 0.0 ? static_cast<double>(total) / seconds : 0.0);
    return 0;
}
