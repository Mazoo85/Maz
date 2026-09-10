// ZOMBOID: ANCHORAGE — headless audio renderer.
//
// Renders the ported chiptune SFX / music to WAV files with no audio device, so
// the sound layer exists and is verifiable in CI.
//
//   zomboid-audio --sfx gun out.wav        # one sound effect
//   zomboid-audio --music 40 city.wav      # 40 steps of the loop
//   zomboid-audio --demo demo.wav          # every SFX over a music bed
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "zomboid/audio/Audio.hpp"

namespace {

bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t wrote = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return wrote == data.size();
}

int usage() {
    std::printf("usage: zomboid-audio [--seed S] --sfx NAME out.wav\n"
                "                     [--seed S] --music STEPS out.wav\n"
                "                     [--seed S] --demo out.wav\n"
                "  SFX names: hit swing gun shotgun pickup open hurt death eat drink"
                " select start zgroan sega\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    uint64_t seed = 1;
    std::string mode, sfxName, outPath;
    int steps = 40;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--sfx") == 0 && i + 2 < argc) {
            mode = "sfx";
            sfxName = argv[++i];
            outPath = argv[++i];
        } else if (std::strcmp(argv[i], "--music") == 0 && i + 2 < argc) {
            mode = "music";
            steps = std::atoi(argv[++i]);
            outPath = argv[++i];
        } else if (std::strcmp(argv[i], "--demo") == 0 && i + 1 < argc) {
            mode = "demo";
            outPath = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
    }
    if (mode.empty() || outPath.empty()) return usage();

    zb::Rng rng(seed);
    zb::audio::Clip clip;
    if (mode == "sfx") {
        zb::audio::Sfx sfx;
        if (!zb::audio::sfxFromName(sfxName, sfx)) {
            std::printf("ERROR: unknown sfx '%s'\n", sfxName.c_str());
            return usage();
        }
        clip = zb::audio::renderSfx(sfx, rng);
    } else if (mode == "music") {
        clip = zb::audio::renderMusic(steps, rng);
    } else {
        clip = zb::audio::renderDemo(rng);
    }

    const std::vector<uint8_t> wav = zb::audio::encodeWav(clip);
    if (!writeFile(outPath, wav)) {
        std::printf("ERROR: could not write %s\n", outPath.c_str());
        return 1;
    }
    std::printf("wrote %s  (%.2fs, %zu samples, %zu bytes)\n", outPath.c_str(),
                static_cast<double>(clip.durationSec()), clip.samples().size(), wav.size());
    return 0;
}
